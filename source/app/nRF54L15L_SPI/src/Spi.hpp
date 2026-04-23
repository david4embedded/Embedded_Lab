
#pragma once

#include <zephyr/drivers/spi.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <span>

class Spi
{
public:
    using Buffer = std::span<uint8_t>;

    Spi(struct spi_dt_spec &spec, 
        struct k_poll_event* asyncEvt = nullptr, 
        struct gpio_dt_spec* csGpio = nullptr, 
        struct gpio_callback csCbData = {})
     : m_spec(spec), 
       m_csGpio(csGpio),
       m_csCbData(csCbData),
       m_asyncEvt(asyncEvt)
    {  }

    ~Spi();

    int Initialize();
    int Tranceive(Buffer tx, Buffer rx);
    int TranceiveAsync(Buffer tx, Buffer rx);
    int WaitComplete(uint32_t timeoutMs = 2000);

private:
    struct spi_buf_set PopulateSpiBufSingle(struct spi_buf& spi_buf);
    struct spi_buf ToSpiBuffer(Buffer buf);
    int ConfigureCsChangeCallback();

    struct spi_dt_spec& m_spec;
    struct gpio_dt_spec* m_csGpio{nullptr};
    struct gpio_callback m_csCbData{};
    struct k_poll_event* m_asyncEvt{nullptr};

    bool m_initialized = false;
};