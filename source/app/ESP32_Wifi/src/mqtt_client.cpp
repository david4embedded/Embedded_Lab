#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "mqtt_client.h"
#include "secure_info.h"

static constexpr uint32_t MqttReconnectIntervalMs = 5000;
static constexpr uint32_t MqttPublishIntervalMs = 1000;
static constexpr uint16_t MqttTaskStackSize = 4096;
static constexpr UBaseType_t MqttTaskPriority = 1;

static WiFiClient mqttWiFiClient;
static PubSubClient mqttClient(mqttWiFiClient);

static void mqttTask(void *parameter);
static void mqttMessageCallback(char *topic, byte *payload, unsigned int length);
static bool connectMqtt();

void mqttClientBegin()
{
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttMessageCallback);

    const BaseType_t result = xTaskCreatePinnedToCore(
        mqttTask,
        "MqttTask",
        MqttTaskStackSize,
        nullptr,
        MqttTaskPriority,
        nullptr,
        0);

    if (result != pdPASS)
    {
        Serial.println("Failed to create MQTT task");
    }
}

static void mqttTask(void *parameter)
{
    uint32_t lastReconnectMs = 0;
    uint32_t lastPublishMs = 0;

    for (;;)
    {
        const uint32_t now = millis();

        if (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!mqttClient.connected())
        {
            if (now - lastReconnectMs >= MqttReconnectIntervalMs)
            {
                lastReconnectMs = now;
                connectMqtt();
            }

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        mqttClient.loop();

        if (now - lastPublishMs >= MqttPublishIntervalMs)
        {
            lastPublishMs = now;

            const String heartbeatTopic = String(MQTT_TOPIC_PREFIX);
            const String uptimeSeconds = String(now / 1000);
            mqttClient.publish(heartbeatTopic.c_str(), uptimeSeconds.c_str());
            Serial.println("Published Topic: " + heartbeatTopic + " Msg: " + uptimeSeconds);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void mqttMessageCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("MQTT message on ");
    Serial.print(topic);
    Serial.print(": ");

    for (unsigned int index = 0; index < length; ++index)
    {
        Serial.write(payload[index]);
    }

    Serial.println();
}

static bool connectMqtt()
{
    char clientId[32];
    snprintf(clientId, sizeof(clientId), "esp32-%08lX", static_cast<unsigned long>(ESP.getEfuseMac()));

    Serial.print("Connecting to MQTT broker...");
    if (!mqttClient.connect(clientId))
    {
        Serial.print(" failed, state=");
        Serial.println(mqttClient.state());
        return false;
    }

    const String commandTopic = String(MQTT_TOPIC_PREFIX) + "/command";
    const String statusTopic = String(MQTT_TOPIC_PREFIX) + "/status";

    mqttClient.subscribe(commandTopic.c_str());
    mqttClient.publish(statusTopic.c_str(), "online", true);
    Serial.println(" connected");
    return true;
}