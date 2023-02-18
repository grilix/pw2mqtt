#include <stdio.h>

#include <pipewire/impl.h>
#include <pipewire/node.h>

#include "node_instance.h"

struct node_instance *node_instance_create(struct pw_node_info info) {
  struct node_instance *instance = calloc(1, sizeof(struct node_instance));

  const struct spa_dict_item *item;

  spa_dict_for_each(item, info.props) {
    if (spa_streq(item->key, "node.name")) {
      instance->name = strdup(item->value);
    }
    if (spa_streq(item->key, "media.class")) {
      instance->media_class = strdup(item->value);
    }
    if (spa_streq(item->key, "api.alsa.path")) {
      instance->api_alsa_path = strdup(item->value);
    }
    if (spa_streq(item->key, "object.path")) {
      instance->object_path = strdup(item->value);
    }
    if (spa_streq(item->key, "api.alsa.pcm.stream")) {
      instance->api_alsa_pcm_stream = strdup(item->value);
    }
    if (spa_streq(item->key, "media.class")) {
      instance->media_class = strdup(item->value);
    }
  }
  if (instance->object_path == NULL) {
    instance->object_path = strdup("unknown");
  }

  return instance;
}

const void _optional_free(void *object)
{
  if (object != NULL) {
    free(object);
  }
}

const void node_instance_destroy(struct node_instance *instance)
{
  _optional_free(instance->name);
  _optional_free(instance->media_class);
  _optional_free(instance->api_alsa_path);
  _optional_free(instance->object_path);
  _optional_free(instance->api_alsa_pcm_stream);

  free(instance);
}
