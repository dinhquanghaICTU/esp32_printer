#ifndef __MQTT_H__
#define __MQTT_H__

#include "config.h"

typedef struct 
{
    char broker_url [20];
    char topic [20]; 
}mqtt_ctx;


void mqtt_app_init(const char *url_broker);
void mqtt_app_publish(const char *topic, const char *data);
void mqtt_app_subscribe(const char *topic);

#endif//__MQTT_H__