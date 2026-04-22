
#include "MemDefines.h"
#include "Spi.hpp"
#include <stdio.h>

#define FRAME_SIZE COND_CODE_1(CONFIG_SPI_LOOPBACK_16BITS_FRAMES, (16), (8))
#define MODE_LOOP  COND_CODE_1(CONFIG_SPI_LOOPBACK_MODE_LOOP, (SPI_MODE_LOOP), (0))

#define SPI_OP(frame_size)                                                 \
	SPI_OP_MODE_MASTER | SPI_MODE_CPOL | MODE_LOOP | SPI_MODE_CPHA |       \
	SPI_WORD_SET(frame_size) | SPI_LINES_SINGLE

#define SPI_FAST_DEV DT_COMPAT_GET_ANY_STATUS_OKAY(test_spi_loopback_fast)
static struct spi_dt_spec s_spiFast = SPI_DT_SPEC_GET(SPI_FAST_DEV, SPI_OP(FRAME_SIZE), 0);

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cs_loopback_gpios)
static const struct gpio_dt_spec cs_loopback_gpio = GPIO_DT_SPEC_GET_OR(DT_PATH(zephyr_user), cs_loopback_gpios, {0});
static struct gpio_callback cs_cb_data;
atomic_t cs_count;
#endif

constexpr const size_t BUF_SIZE = 32;

static struct k_poll_signal s_asyncSignal = K_POLL_SIGNAL_INITIALIZER(s_asyncSignal);
static struct k_poll_event s_asyncEvent = 
{
	.poller = nullptr,
    .type = K_POLL_TYPE_SIGNAL,
	.state = K_POLL_STATE_NOT_READY,
    .mode = K_POLL_MODE_NOTIFY_ONLY,
	.unused = 0,
	.signal = &s_asyncSignal
};

int main(void)
{
	printf("Sungsu's SPI Test\n");

	// Create buffers
	__BUF_ALIGN uint8_t bufferTx[BUF_SIZE] = {0};
	__BUF_ALIGN uint8_t bufferRx[BUF_SIZE] = {0};	
	Spi::Buffer tx(bufferTx, sizeof(bufferTx));
	Spi::Buffer rx(bufferRx, sizeof(bufferRx));

	Spi spi{s_spiFast, &s_asyncEvent};
	spi.Initialize();

	// Test in blocking mode
	snprintf(reinterpret_cast<char*>(tx.data()), tx.size(), "Hello");	
	spi.Tranceive(tx, rx);

	// Test in async mode
	snprintf(reinterpret_cast<char*>(tx.data()), tx.size(), "Haloo");
	spi.TranceiveAsync(tx, rx);	

	int ret = spi.WaitComplete(10);
	if(!ret)
	{
		printf("Main, Rx: %s\n", rx.data());
	}

	return 0;
}