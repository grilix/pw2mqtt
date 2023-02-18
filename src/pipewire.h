#pragma once

#include <spa/utils/hook.h>
#include <pipewire/impl.h>

#include "node_instance.h"

struct pipewire_connector {
  void *data;

  int prompt_pending;

  struct pw_core *core;
  struct spa_hook core_listener;
  struct spa_hook proxy_core_listener;
  struct pw_registry *registry;
  struct spa_hook registry_listener;

  struct pw_map globals;

  unsigned int quit:1;
};

struct global {
  struct pipewire_connector *pipewire;

  uint32_t id;
  struct pw_proxy *proxy;
  struct node_instance *instance;
};

struct proxy_data {
  struct global *global;

  struct spa_hook proxy_listener;
  struct spa_hook object_listener;
};
