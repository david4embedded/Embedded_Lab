#include "Imu.hpp"

#include <zephyr/kernel.h>

#include <cerrno>

namespace
{
	// Registers common to LSM6DS3TR-C and LSM6DSO
	constexpr uint8_t REG_WHO_AM_I	= 0x0F;
	constexpr uint8_t REG_CTRL1_XL	= 0x10;
	constexpr uint8_t REG_CTRL2_G	= 0x11;
	constexpr uint8_t REG_CTRL3_C	= 0x12;
	constexpr uint8_t REG_OUT_TEMP_L	= 0x20;	// TEMP(2) + GYRO(6) + ACCEL(6) follow in order

	constexpr uint8_t WHO_AM_I_LSM6DS3TRC	= 0x6A;
	constexpr uint8_t WHO_AM_I_LSM6DSO	= 0x6C;

	constexpr uint8_t ODR_104HZ_FS_DEFAULT	= 0x40;	// ODR = 104 Hz, accel +-2 g / gyro 250 dps
	constexpr uint8_t CTRL3_BDU_IF_INC	= 0x44;	// Block data update + register auto increment

	// Sensitivity at the full scale configured above
	constexpr int32_t ACCEL_UG_PER_LSB	= 61;	// 0.061 mg/LSB
	constexpr int32_t GYRO_CENTI_MDPS_PER_LSB	= 875;	// 8.75 mdps/LSB
	constexpr int32_t TEMP_LSB_PER_C	= 256;
	constexpr int32_t TEMP_OFFSET_CENTI_C	= 2500;	// 0 LSB = 25 degree Celsius

	int16_t ToInt16(const uint8_t* buf)
	{
		return static_cast<int16_t>(static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8));
	}
}

int Imu::Initialize()
{
	if (!i2c_is_ready_dt(&m_i2c))
	{
		return -ENODEV;
	}

	// The IMU power rail is enabled at boot; give the sensor time to finish its own boot sequence
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

int Imu::Read(Sample& sample)
{
	uint8_t buf[14] = {0};

	auto ret = i2c_burst_read_dt(&m_i2c, REG_OUT_TEMP_L, buf, sizeof(buf));
	if (ret != 0)
	{
		return ret;
	}

	sample.temp_centi_c = TEMP_OFFSET_CENTI_C + (ToInt16(&buf[0]) * 100) / TEMP_LSB_PER_C;

	for (int axis = 0; axis < 3; ++axis)
	{
		sample.gyro_mdps[axis] = (ToInt16(&buf[2 + (axis * 2)]) * GYRO_CENTI_MDPS_PER_LSB) / 100;
		sample.accel_mg[axis] = (ToInt16(&buf[8 + (axis * 2)]) * ACCEL_UG_PER_LSB) / 1000;
	}

	return 0;
}

const char* Imu::ChipName() const
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
