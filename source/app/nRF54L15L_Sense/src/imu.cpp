/**
 * @file imu.cpp
 * @brief Implements the register-level driver for the on-board six-axis IMU.
 * @date 2026-10-28
 * @author Sungsu Kim
 */

#include "imu.hpp"

#include <zephyr/kernel.h>

#include <cerrno>

static constexpr uint8_t REG_WHO_AM_I = 0x0F;
static constexpr uint8_t REG_CTRL1_XL = 0x10;
static constexpr uint8_t REG_CTRL2_G = 0x11;
static constexpr uint8_t REG_CTRL3_C = 0x12;
static constexpr uint8_t REG_OUT_TEMP_L = 0x20;
static constexpr uint8_t WHO_AM_I_LSM6DS3TRC = 0x6A;
static constexpr uint8_t WHO_AM_I_LSM6DSO = 0x6C;
static constexpr uint8_t ODR_104HZ_FS_DEFAULT = 0x40;
static constexpr uint8_t CTRL3_BDU_IF_INC = 0x44;
static constexpr uint32_t IMU_POWER_UP_DELAY_MS = 20;
static constexpr int32_t SENSOR_AXIS_COUNT = 3;
static constexpr int32_t CENTI_UNITS_PER_UNIT = 100;
static constexpr int32_t MICRO_UNITS_PER_MILLI_UNIT = 1000;
static constexpr int32_t ACCEL_UG_PER_LSB = 61;
static constexpr int32_t GYRO_CENTI_MDPS_PER_LSB = 875;
static constexpr int32_t TEMP_LSB_PER_C = 256;
static constexpr int32_t TEMP_OFFSET_CENTI_C = 2500;

static int16_t toInt16(const uint8_t* buffer);

/**
 * @brief Checks the IMU identity and configures its sensors.
 * @retval 0 on success; a negative errno value otherwise.
 */
int Imu::initialize()
{
	if (!i2c_is_ready_dt(&m_i2c))
	{
		return -ENODEV;
	}

	k_msleep(IMU_POWER_UP_DELAY_MS);

	auto result = i2c_reg_read_byte_dt(&m_i2c, REG_WHO_AM_I, &m_whoAmI);
	if (result != 0)
	{
		return result;
	}

	if ((m_whoAmI != WHO_AM_I_LSM6DS3TRC) && (m_whoAmI != WHO_AM_I_LSM6DSO))
	{
		return -ENOTSUP;
	}

	result = i2c_reg_write_byte_dt(&m_i2c, REG_CTRL3_C, CTRL3_BDU_IF_INC);
	if (result == 0)
	{
		result = i2c_reg_write_byte_dt(&m_i2c, REG_CTRL1_XL, ODR_104HZ_FS_DEFAULT);
	}
	if (result == 0)
	{
		result = i2c_reg_write_byte_dt(&m_i2c, REG_CTRL2_G, ODR_104HZ_FS_DEFAULT);
	}

	return result;
}

/**
 * @brief Reads all sensor values in one burst.
 * @retval 0 on success; a negative errno value otherwise.
 */
int Imu::read(Sample& sample)
{
	uint8_t buffer[14];

	auto result = i2c_burst_read_dt(&m_i2c, REG_OUT_TEMP_L, buffer, sizeof(buffer));
	if (result != 0)
	{
		return result;
	}

	sample.m_tempCentiC = TEMP_OFFSET_CENTI_C +
		(toInt16(&buffer[0]) * CENTI_UNITS_PER_UNIT) / TEMP_LSB_PER_C;

	for (int32_t axis = 0; axis < SENSOR_AXIS_COUNT; ++axis)
	{
		sample.m_gyroMdps[axis] =
			(toInt16(&buffer[2 + (axis * 2)]) * GYRO_CENTI_MDPS_PER_LSB) / CENTI_UNITS_PER_UNIT;
		sample.m_accelMg[axis] =
			(toInt16(&buffer[8 + (axis * 2)]) * ACCEL_UG_PER_LSB) / MICRO_UNITS_PER_MILLI_UNIT;
	}

	return 0;
}

/**
 * @brief Returns a readable name for the detected chip identifier.
 * @retval The chip name, or "unknown" for an unsupported identifier.
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

/**
 * @brief Converts two little-endian bytes to a signed 16-bit value.
 * @retval The decoded signed value.
 */
static int16_t toInt16(const uint8_t* buffer)
{
	return static_cast<int16_t>(static_cast<uint16_t>(buffer[0]) | (static_cast<uint16_t>(buffer[1]) << 8));
}
