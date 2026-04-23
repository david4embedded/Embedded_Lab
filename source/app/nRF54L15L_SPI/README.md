* NOTE
  * This project is based off the example that can be found in nRF Connect SDK v3.2.4, e.g., C:\ncs\v3.2.4\zephyr\tests\drivers\spi\spi_loopback, but a huge amount of changes' been made to have only necessary stuffs to understand the core functionality of Zephyr SPI driver usage.
  * Also, I've added Spi class a wrapper to the driver interface to utilize the spi as objects in C++.
  
* Steps to build through nRF Connect extension in VS Code
  1. Open this folder with VS Code.
  2. Clink on the nRF Connect extension.
  3. Add bulid configuration under `APPLICATIONS` menu on the left.
  4. Choose `nrf54l15dk/nrf54l10/cpuapp/ns` as the board target and the default settings for the others.
  5. Generate and build (It takes quite creating all the device tree stuffs).
  6. For, loopback test, connect P2-08 and P2-09 on the nrf54l15DK board. 
  7. When it worked proplerly, you will see messages from Tera Term:
		*** Booting nRF Connect SDK v3.2.4-4c3fc0d44534 ***
		*** Using Zephyr OS v4.2.99-9673eec75908 ***
		Sungsu's SPI Test
		Tranceive, Tx: Hello
		Tranceive, Rx: Hello
		TranceiveAsync, Tx: Haloo
		Main, Rx: Haloo