/**
 * @file mqtt_client.cpp
 * @brief Implements the ESP32 MQTT task, publishing, and message handling.
 * @date 2026-10-28
 * @author Sungsu Kim
 */

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include <cstdio>

#include "mqtt_client.h"

static constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000U;
static constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS = 1000U;
static constexpr uint16_t MQTT_TASK_STACK_SIZE = 4096U;
static constexpr UBaseType_t MQTT_TASK_PRIORITY = 1U;
static constexpr BaseType_t MQTT_TASK_CORE = 0;
static constexpr std::size_t MQTT_TOPIC_BUFFER_SIZE = 128U;
static constexpr std::size_t MQTT_CLIENT_ID_BUFFER_SIZE = 32U;
static constexpr std::size_t UPTIME_BUFFER_SIZE = 11U;
static constexpr uint32_t MILLISECONDS_PER_SECOND = 1000U;

static WiFiClient s_mqttWiFiClient;
static PubSubClient s_mqttClient(s_mqttWiFiClient);

static void runMqttTask(void *);
static void handleMqttMessage(char *topic, byte *payload, unsigned int length);
static bool connectToMqttBroker();
static bool formatTopic(const char *baseTopic, const char *suffix, char *buffer, std::size_t bufferSize);

/**
 * @brief Configures and starts the MQTT task.
 * @retval None
 */
void mqttClientBegin()
{
    s_mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    s_mqttClient.setCallback(handleMqttMessage);

    const auto result = xTaskCreatePinnedToCore(runMqttTask, "MqttTask", MQTT_TASK_STACK_SIZE, nullptr, MQTT_TASK_PRIORITY, nullptr, MQTT_TASK_CORE);

    if (result != pdPASS)
    {
        Serial.println("Failed to create MQTT task");
    }
}

/**
 * @brief Runs the MQTT connection, message handling, and heartbeat publishing loop.
 * @retval None
 */
static void runMqttTask(void *)
{
    auto lastReconnectMs = 0U;
    auto lastPublishMs = 0U;

    for (;;)
    {
        const auto now = millis();

        if (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!s_mqttClient.connected())
        {
            if (now - lastReconnectMs >= MQTT_RECONNECT_INTERVAL_MS)
            {
                lastReconnectMs = now;
                connectToMqttBroker();
            }

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        s_mqttClient.loop();

        if (now - lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS)
        {
            lastPublishMs = now;

            char uptimeSeconds[UPTIME_BUFFER_SIZE];
            const auto written = std::snprintf(uptimeSeconds, sizeof(uptimeSeconds), "%lu", static_cast<unsigned long>(now / MILLISECONDS_PER_SECOND));

            if (written < 0 || static_cast<std::size_t>(written) >= sizeof(uptimeSeconds))
            {
                Serial.println("Failed to format MQTT heartbeat");
            }
            else if (!s_mqttClient.publish(MQTT_TOPIC_PREFIX, uptimeSeconds))
            {
                Serial.println("Failed to publish MQTT heartbeat");
            }
            else
            {
                Serial.print("Published Topic: ");
                Serial.print(MQTT_TOPIC_PREFIX);
                Serial.print(" Msg: ");
                Serial.println(uptimeSeconds);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Prints an incoming MQTT message to the serial port.
 * @retval None
 */
static void handleMqttMessage(char *topic, byte *payload, unsigned int length)
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

/**
 * @brief Connects to the MQTT broker and subscribes to command messages.
 * @retval true if the connection and topic setup succeed; otherwise false.
 */
static bool connectToMqttBroker()
{
    char clientId[MQTT_CLIENT_ID_BUFFER_SIZE];
    const auto clientIdLength = std::snprintf(clientId, sizeof(clientId), "esp32-%08lX", static_cast<unsigned long>(ESP.getEfuseMac()));

    if (clientIdLength < 0 || static_cast<std::size_t>(clientIdLength) >= sizeof(clientId))
    {
        Serial.println("Failed to format MQTT client ID");
        return false;
    }

    char commandTopic[MQTT_TOPIC_BUFFER_SIZE];
    char statusTopic[MQTT_TOPIC_BUFFER_SIZE];
    if (!formatTopic(MQTT_TOPIC_PREFIX, "command", commandTopic, sizeof(commandTopic)) ||
        !formatTopic(MQTT_TOPIC_PREFIX, "status", statusTopic, sizeof(statusTopic)))
    {
        Serial.println("Failed to format MQTT topic");
        return false;
    }

    Serial.print("Connecting to MQTT broker...");
    if (!s_mqttClient.connect(clientId))
    {
        Serial.print(" failed, state=");
        Serial.println(s_mqttClient.state());
        return false;
    }

    if (!s_mqttClient.subscribe(commandTopic))
    {
        Serial.println("Failed to subscribe to MQTT command topic");
        s_mqttClient.disconnect();
        return false;
    }

    if (!s_mqttClient.publish(statusTopic, "online", true))
    {
        Serial.println("Failed to publish MQTT status");
        s_mqttClient.disconnect();
        return false;
    }

    Serial.println(" connected");
    return true;
}

/**
 * @brief Formats a topic by appending a suffix to its base.
 * @retval true if the topic fits in the buffer; otherwise false.
 */
static bool formatTopic(const char *baseTopic, const char *suffix, char *buffer, std::size_t bufferSize)
{
    const auto written = std::snprintf(buffer, bufferSize, "%s/%s", baseTopic, suffix);
    return written >= 0 && static_cast<std::size_t>(written) < bufferSize;
}
