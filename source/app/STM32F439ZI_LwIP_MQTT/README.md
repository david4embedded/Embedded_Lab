# STM32F439ZI_LwIP_MQTT

## 1. Overview
* This is a base firmware for MQTT applications running on Nucleo F439ZI board.
* The firmware utilizes Paho MQTT which runs on lwIP.
* The lwIP is configured through ST's CubeMX configurator, together with FreeRTOS.
* The firmware package utilized for drivers and the middleware libraries inclduing lwIP is STM32Cube FW_F4 V1.28.2.

## 2. Project Setup
* The firmware project requires these tools or higher versions on each to be installed in advance:
  - STM32CubeIDE_1.19.0 or later
  - STM32CubeCLT_1.15.1 or later (for debugging through VS Code)
  - STM32CubeMX6.15 or later (not mandatory unless having to change the driver configuration)
  - VS Code with extensions:
   . STM32
   . CMake

## 3. Module Structure
* File Directory Structure:

## 4. Usage
* Open the root directory where the .ioc file is in VS Code.
* Configure and build the project using CMake by opening and select them in the palette.
* Start debugging by pressing F5; the debugging environment is setup through the CubeCLT and the configuration in the launch.json in .vscode folder.
* MQTT examples (freertos.cpp)
   - Users can connect to a MQTT broker, publish, and subsribe topics through MqttManagerPaho class object.
   - Note that lwIP must properly be initialized beforehand.
   - Refer to freertos.cpp for more details.
* The firmware also supports the serial debugging using ST-LINK's virtual COM port; simply open a proper COM port while a USB cable is connected between the Nucleo and the laptop.
* Currently LEDs on the nucleo board are used:
   - LD1 (Green): indicates the ethernet link status (Green when on)
   - LD2 (Blue): toggled whenever there's a new message arrived that the firmware subscribed.

## 5. Implementation Details
* The ip address on the Nucleo side is fixed to be 192.168.1.2 with no gateway option.

## 6. Tests
* Tested with Mosquitto broker installed in my Windows laptop.
   - Mosquitto broker is fixed to 192.168.1.2 for the ip and 1883 for the port and it should be running as a Windows service.
   - When the firmware boots up, it tries to connect to the broker and publish 'test' for the topic with messages.
   - On the Windows' side, you can watch the topic is updated, through the mosquitto_sub a CLI command.

## 7. Contact
* david4embedded@gmail.com
 