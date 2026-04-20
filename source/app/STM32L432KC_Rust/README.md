# STM32L432KC_Rust

## 1. Overview
* This is a Embedded Rust project targeting NucleoL432KC board.
* The project supports on CLI interface
  * one bi-polar stepping motor 
  * one LED on the nucleo board
* The project utilizes embassy_stm32, embassy_executor, and so on, and it supports async tasks to execute multiple tasks at the same time using the async framework. 
* For example, while executing the stepper CLI command, it can also service the ADC CLI command at the same time.

## 2. Project Setup
* TBD

## 3. Module Structure
* TBD

## 4. Usage
* How To Build & Run
  - Build: **"cargo build"**
  - Run(flashing): **"cargo run"**
  - Debugging: **press F5**

* CLI commands
  * **help**: show available command lists
  * **led**: toggle the led at the ms interval given
    * ex) "led 100": toggle the led every 100ms
  * **stepper**: move the stepper motor with a degree input.
    * ex) "stepper 100": rotate the stepper motor for 100 degree in CW direction
  * **adc**: turn on the adc input reading at the ms interval given
    * ex) "adc 1000": read the adc every 1000ms

* Nucleo H/W Setup
  * Serial interface for CLI
    * Module - USART2 (VCOM)
	  * Pins (NucleoL432KC)
		* TX: A7 (PA2)
		* RX: A2 (PA15)
	* Baudrate : 115200
	  * Line feed setting in Teraterm: TX: **CR+LF** and RX: **CR**
	* When connecting a UART-to-USB module to the laptop, make sure the com port is not of Nucleo's.
  * LED
    * Pin: PB3

## 5. Implementation Details
* How to use macros
  * You can use these macros to see printouts in the command line.
    * info!(""Hello World!={}", 1);
    * debug!("debug info={}", 1);
    * warn!(" ... ")
    * error!(" ... ")

## 6. Tests
* TBD

## 7. Contact
* david4embedded@gmail.com
