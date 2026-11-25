#pragma once

/**
 * @file imu.hpp
 * @brief Register-level I2C driver for the onboard 6-axis IMU.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/i2c.h>

#include <cstdint>

/**
 * @brief Minimal register-level driver for the on-board 6-axis IMU.
 * @details The board DTS describes the IMU as "st,lsm6dso", but the XIAO nRF54L15 Sense
 *          carries an LSM6DS3TR-C (WHO_AM_I = 0x6A), which the Zephyr lsm6dso driver rejects.
 *          Both parts share the control/output register map used here, so the IMU is
 *          accessed over I2C directly instead of through the sensor driver.
 */
class Imu
{
public:
    struct Sample
    {
        int32_t m_accelMg[3]; //!< Milli-g acceleration for X, Y and Z.
        int32_t m_gyroMdps[3]; //!< Milli-degree-per-second rotation for X, Y and Z.
        int32_t m_tempCentiC; //!< Temperature in 0.01 degree Celsius.
    };

    /**
     * @brief Construct an IMU wrapper for an I2C device specification.
     * @retval None
     */
    explicit Imu(const struct i2c_dt_spec& i2cSpec)
    : m_i2c(i2cSpec)
    {
    }

    int initialize();
    int read(Sample& sample);

    /**
     * @brief Return the cached WHO_AM_I value.
     * @retval The WHO_AM_I register value
     */
    uint8_t getWhoAmI() const
    {
        return m_whoAmI;
    }

    const char* getChipName() const;

private:
    const struct i2c_dt_spec& m_i2c; //!< I2C device specification.
    uint8_t m_whoAmI{}; //!< Cached WHO_AM_I value.
};
