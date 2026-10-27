#pragma once

#include <zephyr/drivers/i2c.h>

#include <cstdint>

/**
 * @brief Minimal register-level driver for the on-board 6-axis IMU
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
		int32_t accel_mg[3];		// milli-g
		int32_t gyro_mdps[3];		// milli-degree per second
		int32_t temp_centi_c;		// 0.01 degree Celsius
	};

	explicit Imu(const struct i2c_dt_spec& i2c_spec)
	: m_i2c(i2c_spec)
	{
	}

	/**
	 * @brief Check WHO_AM_I and configure accel (104 Hz, +-2 g) and gyro (104 Hz, 250 dps)
	 * @return 0 on success, negative errno otherwise
	 */
	int Initialize();

	/**
	 * @brief Read accel, gyro and temperature in one burst
	 * @return 0 on success, negative errno otherwise
	 */
	int Read(Sample& sample);

	uint8_t WhoAmI() const
	{
		return m_whoAmI;
	}

	const char* ChipName() const;

private:
	const struct i2c_dt_spec& m_i2c;
	uint8_t m_whoAmI{0};
};
