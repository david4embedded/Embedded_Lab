# STM32F439ZI_LwIP_TCPIP (Echo Server)

## 1. Overview
* This is a base firmware that runs FreeRTOS and lwIP on TCP/IP, created through the ST's CubeMX configurator.
* The firmware package utilized for drivers and the middleware libraries inclduing lwIP is STM32Cube FW_F4 V1.28.2.

## 2. Project Setup
* The firmware project requires these tools or higher versions on each to be installed in advance:
  - STM32CubeIDE_1.5.0 or later
  - STM32CubeCLT_1.15.1 or later (for debugging through VS Code)
  - STM32CubeMX6.11 or later (not mandatory unless having to change the driver configuration)
  - VS Code with extensions:
   . STM32
   . CMake

## 3. Module Structure
* File Directory Structure:

## 4. Usage
* Open the root directory where the .ioc file is in VS Code.
* Configure and build the project using CMake by opening and select them in the palette.
* Start debugging by pressing F5; the debugging environment is setup through the CubeCLT and the configuration in the launch.json in .vscode folder.
* The firmware supports TCP/IP echo server.
   - The server's address is fixed to 192.168.1.3 with no gateway option, i.e., direct ethernet connection to a client.
   - when a client sneds a message to the server, the server echoes the message to the client back.

## 5. Implementation Details
* 

## 6. Tests
* 

## 7. Contact
* 
 