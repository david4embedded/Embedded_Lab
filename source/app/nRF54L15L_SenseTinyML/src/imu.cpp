#include "imu.hpp"

/**
 * @file imu.cpp
 * @brief Register-level implementation of the onboard 6-axis IMU driver.
 * @date 2026-11-16
 * @author Sungsu Kim
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <cerrno>
#include <cstdint>

constexpr uint8_t REG_WHO_AM_I = 0x0F;
constexpr uint8_t REG_CTRL1_XL = 0x10;
constexpr uint8_t REG_CTRL2_G = 0x11;
constexpr uint8_t REG_CTRL3_C = 0x12;
constexpr uint8_t REG_OUT_TEMP_L = 0x20;

constexpr uint8_t WHO_AM_I_LSM6DS3TRC = 0x6A;
constexpr uint8_t WHO_AM_I_LSM6DSO = 0x6C;

constexpr uint8_t ODR_104HZ_FS_DEFAULT = 0x40;
constexpr uint8_t CTRL3_BDU_IF_INC = 0x44;

constexpr int32_t ACCEL_UG_PER_LSB = 61;
constexpr int32_t GYRO_CENTI_MDPS_PER_LSB = 875;
constexpr int32_t TEMP_LSB_PER_C = 256;
constexpr int32_t TEMP_OFFSET_CENTI_C = 2500;

static int16_t IMU_toInt16(const uint8_t* buf);

/**
 * @brief Convert two little-endian bytes to a signed 16-bit value.
 * @retval The converted signed value
 */
static int16_t IMU_toInt16(const uint8_t* buf)
{
    return static_cast<int16_t>(static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8));
}

/**
 * @brief Check WHO_AM_I and configure accel and gyro.
 * @retval 0 on success, negative errno otherwise
 */
int Imu::initialize()
{
    if (!i2c_is_ready_dt(&m_i2c))
    {
        return -ENODEV;
    }

    k_msleep(20);

    auto ret = i2c_reg_read_byte_dt(&m_i2c, REG_WHO_AM_I, &m_whoAmI);
    if (ret != 0)
    {
        return ret;
    }

    if ((m_whoAmI != WHO_AM_I_LSM6DS3TRC) && (m_whoAmI != WHO_AM_I_LSM6DSO))
    {
        return -ENOTSUP;
    }

    ret = i2c_reg_write_byte_dt(&m_i2c, REG_CTRL3_C, CTRL3_BDU_IF_INC);
    if (ret == 0)
    {
        ret = i2c_reg_write_byte_dt(&m_i2c, REG_CTRL1_XL, ODR_104HZ_FS_DEFAULT);
    }
    if (ret == 0)
    {
        ret = i2c_reg_write_byte_dt(&m_i2c, REG_CTRL2_G, ODR_104HZ_FS_DEFAULT);
    }

    return ret;
}

/**
 * @brief Read accel, gyro and temperature in one burst.
 * @retval 0 on success, negative errno otherwise
 */
int Imu::read(Sample& sample)
{
    uint8_t buf[14] = {0};

    auto ret = i2c_burst_read_dt(&m_i2c, REG_OUT_TEMP_L, buf, sizeof(buf));
    if (ret != 0)
    {
        return ret;
    }

    sample.m_tempCentiC = TEMP_OFFSET_CENTI_C + (IMU_toInt16(&buf[0]) * 100) / TEMP_LSB_PER_C;

    for (int axis = 0; axis < 3; ++axis)
    {
        sample.m_gyroMdps[axis] = (IMU_toInt16(&buf[2 + (axis * 2)]) * GYRO_CENTI_MDPS_PER_LSB) / 100;
        sample.m_accelMg[axis] = (IMU_toInt16(&buf[8 + (axis * 2)]) * ACCEL_UG_PER_LSB) / 1000;
    }

    return 0;
}

/**
 * @brief Return the human-readable name for the detected IMU.
 * @retval Pointer to the chip name
 */
const char* Imu::getChipName() const
{
    switch (m_whoAmI)
    {
    case WHO_AM_I_LSM6DS3TRC:
        return "LSM6DS3TR-C";
    case WHO_AM_I_LSM6DSO:
        return "LSM6DSO";
    default:
        return "unknown";
    }
}
