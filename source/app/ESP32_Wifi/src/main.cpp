#include <Arduino.h>
#include <WiFi.h>
#include "secure_info.h"

const int ledPin = 23;
WiFiServer server(7);

void setup()
{
    Serial.begin(115200);

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);

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

    server.begin();
    Serial.println("TCP echo server listening on port 7");
}

void loop()
{
    static unsigned long lastToggleMs = 0;
    const unsigned long toggleIntervalMs = 1000;

    if (millis() - lastToggleMs >= toggleIntervalMs)
    {
        lastToggleMs = millis();
        digitalWrite(ledPin, !digitalRead(ledPin));
    }

    WiFiClient client = server.available();
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
            auto data = client.read();
            Serial.write(static_cast<uint8_t>(data));
            client.write(data);
        }
        delay(1);
    }

    client.stop();
    Serial.println("Client disconnected");
}