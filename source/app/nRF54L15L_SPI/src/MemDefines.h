#pragma once

#ifdef CONFIG_NOCACHE_MEMORY
#define __NOCACHE	__attribute__((__section__(".nocache")))
#elif defined(CONFIG_DT_DEFINED_NOCACHE)
#define __NOCACHE	__attribute__((__section__(CONFIG_DT_DEFINED_NOCACHE_NAME)))
#else /* CONFIG_NOCACHE_MEMORY */
#define __NOCACHE
#if CONFIG_DCACHE_LINE_SIZE != 0
#define __BUF_ALIGN	__aligned(CONFIG_DCACHE_LINE_SIZE)
#endif
#endif /* CONFIG_NOCACHE_MEMORY */

#ifndef __BUF_ALIGN
#define __BUF_ALIGN	__aligned(32)
#endif