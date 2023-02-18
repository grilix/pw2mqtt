#pragma once

#include "mqtt/mqtt.h"

struct mqtt {
  const char *client_id;
  const char *host;
  const char *port;
  const char *username;
  const char *password;

  const char *name;

  uint8_t *send_buffer;
  uint8_t *receive_buffer;

  int sockfd;
  pthread_t client_daemon;
  struct mqtt_client client;
};

bool mqtt_start_connection(struct mqtt *mqtt);
void mqtt_end(struct mqtt *mqtt);

bool publish_mqtt_message(struct mqtt *mqtt, const char *topic, const char *message);

void mqtt_publish_callback(void** unused, struct mqtt_response_publish *published);
// void* mqtt_client_refresher(void* client);
