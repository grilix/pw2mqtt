#pragma once

#include <pipewire/node.h>

struct node_instance {
  char *api_alsa_path;
  char *api_alsa_pcm_stream;
  char *object_path;

  char *name;
  char *media_class;
};

struct node_instance *node_instance_create(struct pw_node_info info);
const void node_instance_destroy(struct node_instance *instance);
