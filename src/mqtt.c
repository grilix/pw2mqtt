#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "mqtt/mqtt.h"
#include "mqtt/posix_sockets.h"

#include "mqtt.h"

bool publish_mqtt_message(struct mqtt *mqtt, const char *topic, const char *message)
{
  mqtt_publish(&mqtt->client, topic, message, strlen(message), MQTT_PUBLISH_QOS_0);
  if (mqtt->client.error != MQTT_OK) {
    fprintf(stderr, "Error: Can't publish mqtt message: %s\n", mqtt_error_str(mqtt->client.error));
    mqtt_end(mqtt);
    return false;
  }

  return true;
}

void mqtt_publish_callback(void** unused, struct mqtt_response_publish *published) { }

void* mqtt_client_refresher(void* client)
{
    while(1) {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}

bool mqtt_start_connection(struct mqtt *mqtt)
{
  mqtt->sockfd = open_nb_socket(mqtt->host, mqtt->port);
  if (mqtt->sockfd == -1) {
    perror("Failed to open socket: ");

    return false;
  }

  int send_size = 2048;
  int recv_size = 1024;
  mqtt->send_buffer = calloc(1, send_size);
  mqtt->receive_buffer = calloc(1, recv_size);

  mqtt_init(&mqtt->client, mqtt->sockfd, mqtt->send_buffer, send_size, mqtt->receive_buffer, recv_size, mqtt_publish_callback);

  uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;
  mqtt_connect(&mqtt->client, mqtt->client_id, NULL, NULL, 0, mqtt->username, mqtt->password, connect_flags, 400);
  if (mqtt->client.error != MQTT_OK) {
    fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt->client.error));
    mqtt_end(mqtt);
    return false;
  }

  if(pthread_create(&mqtt->client_daemon, NULL, mqtt_client_refresher, &mqtt->client)) {
    fprintf(stderr, "Failed to start client daemon.\n");
    mqtt_end(mqtt);
    return false;
  }

  return true;
}

void mqtt_end(struct mqtt *mqtt)
{
    if (mqtt->sockfd != -1) close(mqtt->sockfd);
    if (mqtt->client_daemon != 0) pthread_cancel(mqtt->client_daemon);
    if (mqtt->send_buffer != NULL) free(mqtt->send_buffer);
    if (mqtt->receive_buffer != NULL) free(mqtt->receive_buffer);
}
