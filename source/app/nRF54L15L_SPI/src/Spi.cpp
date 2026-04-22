
#include "Spi.hpp"

Spi::~Spi()
{
    spi_release_dt(&m_spec);
    m_spec.config.operation &= ~SPI_LOCK_ON;
}

int Spi::Initialize()
{
    if(m_csGpio)
    {
        auto ret = ConfigureCsChangeCallback();
        if (ret)
        {
            printf("Failed to configure CS change callback, ret=%d\n", ret);
            return ret;
        }
    }

    m_initialized = true;
    return 0;
}

int Spi::Tranceive(Buffer tx, Buffer rx)
{
    if(!m_initialized)
    {
        printf("SPI not initialized. Call Initialize() before tranceiving.\n");
        return -1;
    }

    int ret = pm_device_runtime_get(m_spec.bus);
    if(ret)
    {
        printf("Failed to get runtime PM for device %s\n", m_spec.bus->name);
        return ret;
    }

    struct spi_buf spiBuffTx = ToSpiBuffer(tx);
    struct spi_buf spiBuffRx = ToSpiBuffer(rx);

    struct spi_buf_set spiBuffsTx = PopulateSpiBufSingle(spiBuffTx);
    struct spi_buf_set spiBuffsRx = PopulateSpiBufSingle(spiBuffRx);

    printf("Tranceive, Tx: %s\n", tx.data());

    ret = spi_transceive_dt(&m_spec, &spiBuffsTx, &spiBuffsRx);
    if (ret == -EINVAL || ret == -ENOTSUP) 
    {
        printf("Spi config invalid for this controller\n");
    }

    printf("Tranceive, Rx: %s\n", rx.data());

    pm_device_runtime_put(m_spec.bus);

    return ret;
}

int Spi::TranceiveAsync(Buffer tx, Buffer rx)
{
    struct spi_buf spiBuffTx = ToSpiBuffer(tx);
    struct spi_buf spiBuffRx = ToSpiBuffer(rx);

    struct spi_buf_set spiBuffsTx = PopulateSpiBufSingle(spiBuffTx);
    struct spi_buf_set spiBuffsRx = PopulateSpiBufSingle(spiBuffRx);

    printf("TranceiveAsync, Tx: %s\n", tx.data());

    int ret = spi_transceive_signal(m_spec.bus, &m_spec.config, &spiBuffsTx, &spiBuffsRx, m_asyncEvt->signal);
    if (ret) 
    {
        printf("SPI transceive asyncfailed, code %d\n", ret);
    }

    return ret;
}

int Spi::WaitComplete(uint32_t timeoutMs /* = 2000 */)
{
    if(k_poll(m_asyncEvt, 1, K_MSEC(timeoutMs)))
    {
        printf("Failed to wait for async transfer completion\n");
        return -1;
    }
    return 0;
}

struct spi_buf_set Spi::PopulateSpiBufSingle(struct spi_buf& spi_buf)
{
    struct spi_buf_set buf_set;
    buf_set.buffers = &spi_buf;
    buf_set.count = 1;

    return buf_set;
}

struct spi_buf Spi::ToSpiBuffer(Buffer buf)
{
    struct spi_buf spi_buf;
    spi_buf.buf = buf.data();
    spi_buf.len = buf.size();
    return spi_buf;
}

int Spi::ConfigureCsChangeCallback()
{
    const struct gpio_dt_spec *gpio = m_csGpio;

    if (!gpio_is_ready_dt(gpio)) 
    {
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(gpio, GPIO_INPUT);
    if (ret) 
    {
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(gpio, GPIO_INT_EDGE_BOTH);
    if (ret) 
    {
        return ret;
    }

    gpio_init_callback(&m_csCbData, nullptr, BIT(gpio->pin));

    return gpio_add_callback(gpio->port, &m_csCbData);    
}
