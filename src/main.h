#pragma once

#include <pipewire/main-loop.h>

#include "mqtt.h"
#include "pipewire.h"

struct data {
  struct pw_main_loop *loop;
  struct pw_context *context;

  struct pipewire_connector *pipewire;

  struct mqtt mqtt;
};
