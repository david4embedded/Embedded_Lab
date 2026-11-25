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

static constexpr uint8_t LED_PIN = 23U;
static constexpr uint16_t TCP_SERVER_PORT = 7U;
static constexpr uint32_t SERIAL_BAUD_RATE = 115200U;
static constexpr unsigned long LED_TOGGLE_INTERVAL_MS = 1000UL;

WiFiServer g_server(TCP_SERVER_PORT);

/**
 * @brief Initializes the serial port, Wi-Fi, TCP server, and MQTT client.
 * @retval None
 */
void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

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
    Serial.print("TCP echo server listening on port ");
    Serial.println(TCP_SERVER_PORT);

    MQTT_beginClient();
}

/**
 * @brief Toggles the LED and echoes data from an available TCP client.
 * @retval None
 */
void loop()
{
    static auto s_lastToggleMs = 0UL;
    if (millis() - s_lastToggleMs >= LED_TOGGLE_INTERVAL_MS)
    {
        s_lastToggleMs = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
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
