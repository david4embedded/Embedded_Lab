#pragma once

/**
 * @file app_shell.hpp
 * @brief Shell command interface for the IMU and microphone.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include "imu.hpp"
#include "mic.hpp"

namespace AppShell
{
    void Initialize(Imu& imu, bool imuReady, Mic& mic, bool micReady);
}
