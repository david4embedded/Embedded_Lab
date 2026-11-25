/**
 * @file app_shell.hpp
 * @brief Declares the IMU and microphone shell-command initialization interface.
 * @date 2026-10-28
 * @author Sungsu Kim
 */

#pragma once

#include "imu.hpp"
#include "mic.hpp"

/**
 * @brief Provides IMU and microphone shell commands.
 */
namespace AppShell
{
	void Initialize(Imu& imu, const bool imuReady, Mic& mic, const bool micReady);
}
