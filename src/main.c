#include "main.h"

#include <stdio.h>
/* #include <unistd.h> */

#include <spa/utils/result.h>
#include <spa/utils/string.h>
#include <spa/debug/pod.h>
#include <pipewire/impl.h>

#include <json-c/json.h>

#include "mqtt/mqtt.h"
/* #include "mqtt/posix_sockets.h" */
#include "pipewire.h"
#include "node_instance.h"

static void node_event_info(void *data, const struct pw_node_info *info);

static void event_param(void *_data, int seq, uint32_t id,
		uint32_t index, uint32_t next, const struct spa_pod *param)
{
  spa_debug_pod(2, NULL, param);
}

static const struct pw_node_events node_events = {
  PW_VERSION_NODE_EVENTS,
  .info = node_event_info,
  .param = event_param
};

static void on_core_done(void *_data, uint32_t id, int seq)
{
  struct data *d = _data;
  struct pipewire_connector *pw = d->pipewire;

  if (seq == pw->prompt_pending) {
    pw_main_loop_quit(d->loop);
  }
}

static void removed_proxy(void *data)
{
  struct proxy_data *pd = data;
  pw_proxy_destroy(pd->global->proxy);
}

static void destroy_proxy(void *data)
{
  struct proxy_data *pd = data;

  spa_hook_remove(&pd->proxy_listener);
  spa_hook_remove(&pd->object_listener);

  if (pd->global) {
    pd->global->proxy = NULL;
  }
}

static const struct pw_proxy_events proxy_events = {
  PW_VERSION_PROXY_EVENTS,
  .removed = removed_proxy,
  .destroy = destroy_proxy,
};

static void registry_event_global(void *_data, uint32_t id,
		uint32_t permissions, const char *type, uint32_t version,
		const struct spa_dict *props)
{
  struct data *data = _data;
  struct pipewire_connector *pw = data->pipewire;
  struct global *global;
  size_t size;
  struct proxy_data *pd;
  struct pw_proxy *proxy;

  global = calloc(1, sizeof(struct global));
  global->pipewire = pw;
  /* global->data = data; */
  global->id = id;

  size = pw_map_get_size(&pw->globals);
  while (id > size) {
    pw_map_insert_at(&pw->globals, size++, NULL);
  }
  pw_map_insert_at(&pw->globals, id, global);

  if (spa_streq(PW_TYPE_INTERFACE_Node, type) && PW_VERSION_NODE <= version) {
    proxy = pw_registry_bind(pw->registry,
        global->id,
        PW_TYPE_INTERFACE_Node,
        PW_VERSION_NODE,
        sizeof(struct proxy_data));
    global->proxy = proxy;

    pd = pw_proxy_get_user_data(proxy);
    pd->global = global;
    pw_proxy_add_object_listener(proxy, &pd->object_listener, &node_events, pd);
    pw_proxy_add_listener(proxy, &pd->proxy_listener, &proxy_events, pd);

    pw->prompt_pending = pw_core_sync(pw->core, 0, 0);
  }
}

static int global_destroy(void *obj, void *data)
{
  struct global *global = obj;

  if (global == NULL) {
    return 0;
  }

  if (global->proxy) {
    pw_proxy_destroy(global->proxy);
  }
  if (global->instance) {
    node_instance_destroy(global->instance);
  }

  pw_map_insert_at(&global->pipewire->globals, global->id, NULL);
  free(global);
  return 0;
}

static void registry_event_global_remove(void *_data, uint32_t id)
{
  struct data *data = _data;
  struct pipewire_connector *pw = data->pipewire;
  struct global *global;

  global = pw_map_lookup(&pw->globals, id);
  if (global == NULL) {
    return;
  }

  global_destroy(global, pw);
}

static const struct pw_registry_events registry_events = {
  PW_VERSION_REGISTRY_EVENTS,
  .global = registry_event_global,
  .global_remove = registry_event_global_remove,
};

static void on_core_error(void *_data, uint32_t id, int seq, int res, const char *message)
{
  struct data *data = _data;

  pw_log_error("error id:%u seq:%d res:%d (%s): %s", id, seq, res, spa_strerror(res), message);

  if (id == PW_ID_CORE && res == -EPIPE) {
    pw_main_loop_quit(data->loop);
  }
}

static const struct pw_core_events remote_core_events = {
  PW_VERSION_CORE_EVENTS,
  .done = on_core_done,
  .error = on_core_error,
};

static void on_core_destroy(void *_data)
{
  struct data *data = _data;
  struct pipewire_connector *pw = data->pipewire;

  spa_hook_remove(&pw->core_listener);
  spa_hook_remove(&pw->proxy_core_listener);

  pw_map_for_each(&pw->globals, global_destroy, pw);
  pw_map_clear(&pw->globals);

  if (data->pipewire == pw) {
    data->pipewire = NULL;
  }
}

static const struct pw_proxy_events proxy_core_events = {
  PW_VERSION_PROXY_EVENTS,
  .destroy = on_core_destroy,
};

static void remote_data_free(struct pipewire_connector *pw)
{
  spa_hook_remove(&pw->registry_listener);
  pw_proxy_destroy((struct pw_proxy*)pw->registry);
  pw_core_disconnect(pw->core);
}

static bool connect_pipewire(struct data *data, char **error)
{
  struct pw_properties *props = NULL;
  struct pw_core *core;
  struct pipewire_connector *pw;

  core = pw_context_connect(data->context, props, sizeof(struct pipewire_connector));
  if (core == NULL) {
    *error = spa_aprintf("Can't connecto to pipewire: %m");
    return false;
  }

  pw = pw_proxy_get_user_data((struct pw_proxy*)core);
  pw->core = core;
  pw->data = data;
  pw_map_init(&pw->globals, 64, 16);

  data->pipewire = pw;

  pw_core_add_listener(pw->core, &pw->core_listener, &remote_core_events, data);
  pw_proxy_add_listener((struct pw_proxy*)pw->core,
      &pw->proxy_core_listener,
      &proxy_core_events, data);

  pw->registry = pw_core_get_registry(pw->core, PW_VERSION_REGISTRY, 0);
  pw_registry_add_listener(pw->registry, &pw->registry_listener, &registry_events, data);
  pw->prompt_pending = pw_core_sync(pw->core, 0, 0);

  return true;
}

static bool has_changed_state(const struct pw_node_info *info)
{
  return info->change_mask & PW_NODE_CHANGE_MASK_STATE;
}

bool report_node_change(struct proxy_data pd, struct pw_node_info info)
{
  bool result;

  struct data *m_data = pd.global->pipewire->data;
  const char *new_state = pw_node_state_as_string(info.state);
  struct node_instance *instance = (struct node_instance *)pd.global->instance;

  json_object *root = json_object_new_object();
  if (!root) {
    fprintf(stderr, "Error: Can't create root json object: %m.\n");

    json_object_put(root);
    return false;
  }

  json_object *node = json_object_new_object();
  if (!node) {
    fprintf(stderr, "Error: Can't create node json object: %m.\n");
    return false;
  }

  json_object_object_add(node, "state", json_object_new_string(new_state));
  json_object_object_add(node, "media.class", json_object_new_string(instance->media_class));
  json_object_object_add(node, "name", json_object_new_string(instance->name));

  json_object_object_add(root, instance->object_path, node);

  const char *update = json_object_to_json_string(root);

  char *topic = spa_aprintf("stat/%s/wire", m_data->mqtt.name);

  result = publish_mqtt_message(&m_data->mqtt, topic, update);

  free(topic);
  json_object_put(root);

  return result;
}

static void node_event_info(void *_data, const struct pw_node_info *info)
{
  struct proxy_data *pd = _data;
  struct data *data;

  if (pd->global->instance == NULL) {
    pd->global->instance = node_instance_create(*info);
  }

  struct node_instance *instance = pd->global->instance;

  if (instance->media_class != NULL) {
    if (has_changed_state(info)) {
      if (!report_node_change(*pd, *info)) {
        pd->global->pipewire->quit = true;
        data = pd->global->pipewire->data;
        pw_main_loop_quit(data->loop);
      }
    }
  }
}

static void do_quit_on_signal(void *data, int signal_number)
{
  struct data *d = data;

  d->pipewire->quit = true;
  pw_main_loop_quit(d->loop);
}

int main(int argc, char *argv[])
{
  struct data data = {
    0,
    .mqtt = {
      .client_id = "pipewire2mqtt",
      .host = getenv("MQTT_HOST"),
      .port = getenv("MQTT_PORT"),
      .username = getenv("MQTT_USERNAME"),
      .password = getenv("MQTT_PASSWORD"),
      /* .client = { 0 }, //(void*)NULL, */
    },
  };
  struct pw_loop *l;
  char *error;
  char *name = getenv("REPORT_NAME");
  char *topic;

  if (name == NULL) {
    fprintf(stderr, "Error: Missing environment variable: REPORT_NAME\n");
    return 1;
  }

  data.mqtt.name = name; // TODO: strdup(name); ?

  if (data.mqtt.host == NULL) {
    data.mqtt.host = "localhost";
  }
  if (data.mqtt.port == NULL) {
    data.mqtt.port = "1883";
  }

  if (!mqtt_start_connection(&data.mqtt)) {
    return 1;
  }

  topic = spa_aprintf("tele/%s/status", name);
  if (!publish_mqtt_message(&data.mqtt, topic, "online")) {
    free(topic);
    return 1;
  }
  free(topic);

  setlinebuf(stdout);

  pw_init(NULL, NULL);

  data.loop = pw_main_loop_new(NULL);
  if (data.loop == NULL) {
    mqtt_end(&data.mqtt);
    fprintf(stderr, "Error: Can't create pipewire loop: %m\n");
    return -1;
  }
  l = pw_main_loop_get_loop(data.loop);

  pw_loop_add_signal(l, SIGINT, do_quit_on_signal, &data);
  pw_loop_add_signal(l, SIGTERM, do_quit_on_signal, &data);

  data.context = pw_context_new(l, NULL, 0);
  if (data.context == NULL) {
    mqtt_end(&data.mqtt);
    fprintf(stderr, "Can't create context: %m\n");
    return -1;
  }

  if (!connect_pipewire(&data, &error)) {
    mqtt_end(&data.mqtt);
    fprintf(stderr, "Error: \"%s\"\n", error);
    return -1;
  }

  pw_main_loop_run(data.loop);

  data.pipewire->prompt_pending = pw_core_sync(data.pipewire->core, 0, 0);
  while (!data.pipewire->quit && data.pipewire) {
    pw_main_loop_run(data.loop);
  }

  remote_data_free(data.pipewire);
  pw_context_destroy(data.context);
  pw_main_loop_destroy(data.loop);
  pw_deinit();

  mqtt_end(&data.mqtt);

  return 0;
}
