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

static constexpr uint32_t s_mqttReconnectIntervalMs = 5000U;
static constexpr uint32_t s_mqttPublishIntervalMs = 1000U;
static constexpr uint16_t s_mqttTaskStackSize = 4096U;
static constexpr UBaseType_t s_mqttTaskPriority = 1U;
static constexpr std::size_t s_mqttTopicBufferSize = 128U;

static WiFiClient s_mqttWiFiClient;
static PubSubClient s_mqttClient(s_mqttWiFiClient);

static void runMqttTask(void *parameter);
static void handleMqttMessage(char *topic, byte *payload, unsigned int length);
static bool connectToMqttBroker();
static bool formatTopic(const char *baseTopic, const char *suffix, char *buffer, std::size_t bufferSize);

/**
 * @brief Configures and starts the MQTT task.
 * @retval None
 */
void beginMqttClient()
{
    (void)s_mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    (void)s_mqttClient.setCallback(handleMqttMessage);

    const auto result = xTaskCreatePinnedToCore(
        runMqttTask,
        "MqttTask",
        s_mqttTaskStackSize,
        nullptr,
        s_mqttTaskPriority,
        nullptr,
        0);

    if (result != pdPASS)
    {
        (void)Serial.println("Failed to create MQTT task");
    }
}

/**
 * @brief Runs the MQTT connection, message handling, and heartbeat publishing loop.
 * @retval None
 */
static void runMqttTask(void *parameter)
{
    (void)parameter;

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
            if (now - lastReconnectMs >= s_mqttReconnectIntervalMs)
            {
                lastReconnectMs = now;
                (void)connectToMqttBroker();
            }

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        (void)s_mqttClient.loop();

        if (now - lastPublishMs >= s_mqttPublishIntervalMs)
        {
            lastPublishMs = now;

            char uptimeSeconds[11];
            const auto written = std::snprintf(
                uptimeSeconds,
                sizeof(uptimeSeconds),
                "%lu",
                static_cast<unsigned long>(now / 1000U));

            if (written < 0 || static_cast<std::size_t>(written) >= sizeof(uptimeSeconds))
            {
                (void)Serial.println("Failed to format MQTT heartbeat");
            }
            else if (!s_mqttClient.publish(MQTT_TOPIC_PREFIX, uptimeSeconds))
            {
                (void)Serial.println("Failed to publish MQTT heartbeat");
            }
            else
            {
                (void)Serial.print("Published Topic: ");
                (void)Serial.print(MQTT_TOPIC_PREFIX);
                (void)Serial.print(" Msg: ");
                (void)Serial.println(uptimeSeconds);
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
    (void)Serial.print("MQTT message on ");
    (void)Serial.print(topic);
    (void)Serial.print(": ");

    for (unsigned int index = 0; index < length; ++index)
    {
        (void)Serial.write(payload[index]);
    }

    (void)Serial.println();
}

/**
 * @brief Connects to the MQTT broker and subscribes to command messages.
 * @retval true if the connection and topic setup succeed; otherwise false.
 */
static bool connectToMqttBroker()
{
    char clientId[32];
    const auto clientIdLength = std::snprintf(
        clientId,
        sizeof(clientId),
        "esp32-%08lX",
        static_cast<unsigned long>(ESP.getEfuseMac()));

    if (clientIdLength < 0 || static_cast<std::size_t>(clientIdLength) >= sizeof(clientId))
    {
        (void)Serial.println("Failed to format MQTT client ID");
        return false;
    }

    char commandTopic[s_mqttTopicBufferSize];
    char statusTopic[s_mqttTopicBufferSize];
    if (!formatTopic(MQTT_TOPIC_PREFIX, "command", commandTopic, sizeof(commandTopic)) ||
        !formatTopic(MQTT_TOPIC_PREFIX, "status", statusTopic, sizeof(statusTopic)))
    {
        (void)Serial.println("Failed to format MQTT topic");
        return false;
    }

    (void)Serial.print("Connecting to MQTT broker...");
    if (!s_mqttClient.connect(clientId))
    {
        (void)Serial.print(" failed, state=");
        (void)Serial.println(s_mqttClient.state());
        return false;
    }

    if (!s_mqttClient.subscribe(commandTopic))
    {
        (void)Serial.println("Failed to subscribe to MQTT command topic");
        (void)s_mqttClient.disconnect();
        return false;
    }

    if (!s_mqttClient.publish(statusTopic, "online", true))
    {
        (void)Serial.println("Failed to publish MQTT status");
        (void)s_mqttClient.disconnect();
        return false;
    }

    (void)Serial.println(" connected");
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
