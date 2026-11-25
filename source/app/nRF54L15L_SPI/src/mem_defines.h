/**
 * @file mem_defines.h
 * @brief Defines cache and buffer-alignment attributes for the SPI example.
 * @date 2026-04-22
 * @author Sungsu Kim
 */

#pragma once

#ifdef CONFIG_NOCACHE_MEMORY
#define SPI_NOCACHE __attribute__((__section__(".nocache")))
#elif defined(CONFIG_DT_DEFINED_NOCACHE)
#define SPI_NOCACHE __attribute__((__section__(CONFIG_DT_DEFINED_NOCACHE_NAME)))
#else
#define SPI_NOCACHE
#if CONFIG_DCACHE_LINE_SIZE != 0
#define SPI_BUFFER_ALIGN __aligned(CONFIG_DCACHE_LINE_SIZE)
#endif
#endif

#ifndef SPI_BUFFER_ALIGN
#define SPI_BUFFER_ALIGN __aligned(32)
#endif
