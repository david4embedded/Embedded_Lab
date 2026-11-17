#pragma once

#include "Imu.hpp"
#include "Mic.hpp"

/**
 * @brief Shell commands for the IMU and the microphone
 * @details Commands
 *          - imu info               : WHO_AM_I and chip name
 *          - imu read               : one accel/gyro/temperature sample
 *          - imu stream <ms>        : stream samples every <ms> until ESC is pressed
 *          - mic monitor [ms]       : print the sound level every [ms] until ESC is pressed
 *          Streaming runs on a separate thread while the shell is in bypass mode,
 *          so every key input goes to the stream and only ESC stops it.
 */
namespace AppShell
{
	/**
	 * @brief Register the objects used by the shell commands
	 * @details The commands report an error until this is called
	 */
	void Initialize(Imu& imu, bool imuReady, Mic& mic, bool micReady);
}
