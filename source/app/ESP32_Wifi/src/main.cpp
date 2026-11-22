/**
 * @file main.cpp
 * @brief ESP32 Wi-Fi TCP echo server and Arduino application entry points.
 * @date 2026-10-28
 * @author Sungsu Kim
 */

#include <Arduino.h>
#include <WiFi.h>

#include "mqtt_client.h"
#include "secure_info.h"

constexpr auto g_ledPin = 23;
WiFiServer g_server(7);

/**
 * @brief Initializes the serial port, Wi-Fi, TCP server, and MQTT client.
 * @retval None
 */
void setup()
{
    Serial.begin(115200);

    pinMode(g_ledPin, OUTPUT);
    digitalWrite(g_ledPin, LOW);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    g_server.begin();
    Serial.println("TCP echo server listening on port 7");

    beginMqttClient();
}

/**
 * @brief Toggles the LED and echoes data from an available TCP client.
 * @retval None
 */
void loop()
{
    static auto s_lastToggleMs = 0UL;
    constexpr auto toggleIntervalMs = 1000UL;

    if (millis() - s_lastToggleMs >= toggleIntervalMs)
    {
        s_lastToggleMs = millis();
        digitalWrite(g_ledPin, !digitalRead(g_ledPin));
    }

    auto client = g_server.available();
    if (!client)
    {
        return;
    }

    Serial.println("Client connected");
    Serial.print("MSG from client: ");
    while (client.connected())
    {
        while (client.available())
        {
            const auto data = client.read();
            if (data < 0)
            {
                break;
            }

            Serial.write(static_cast<uint8_t>(data));
            client.write(static_cast<uint8_t>(data));
        }
        delay(1);
    }

    client.stop();
    Serial.println("Client disconnected");
}
