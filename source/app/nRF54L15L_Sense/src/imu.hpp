/**
 * @file imu.hpp
 * @brief Declares a register-level driver for the on-board six-axis IMU.
 * @date 2026-04-19
 * @author Sungsu Kim
 */

#pragma once

#include <zephyr/drivers/i2c.h>

#include <cstdint>

/**
 * @brief Provides register-level access to the on-board six-axis IMU.
 * @details The board DTS describes the IMU as "st,lsm6dso", but this board carries an
 *          LSM6DS3TR-C (WHO_AM_I = 0x6A), which the Zephyr lsm6dso driver rejects.
 *          Both parts share the control/output register map used here.
 */
class Imu
{
public:
	struct Sample
	{
		int32_t m_accelMg[3]{};		//!< Acceleration in milli-g.
		int32_t m_gyroMdps[3]{};	//!< Angular rate in milli-degrees per second.
		int32_t m_tempCentiC{};		//!< Temperature in 0.01 degrees Celsius.
	};

	/**
	 * @brief Stores the I2C specification used by the IMU.
	 * @retval None
	 */
	explicit Imu(const struct i2c_dt_spec& i2cSpec)
	: m_i2c(i2cSpec)
	{
	}

	/**
	 * @brief Checks WHO_AM_I and configures the accelerometer and gyroscope.
	 * @retval 0 on success; a negative errno value otherwise.
	 */
	int initialize();

	/**
	 * @brief Reads accel, gyro and temperature in one burst.
	 * @retval 0 on success; a negative errno value otherwise.
	 */
	int read(Sample& sample);

	/**
	 * @brief Returns the value of the WHO_AM_I register.
	 * @retval The most recently read device identifier.
	 */
	uint8_t getWhoAmI() const
	{
		return m_whoAmI;
	}

	/**
	 * @brief Returns the detected IMU chip name.
	 * @retval A null-terminated chip name.
	 */
	const char* getChipName() const;

private:
	const struct i2c_dt_spec& m_i2c;	//!< I2C device specification.
	uint8_t m_whoAmI{};				//!< Most recently read device identifier.
};
