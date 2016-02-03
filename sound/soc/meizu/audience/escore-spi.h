/*
 * escore-spi.h  --  Audience eS705 SPI interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Hemal Meghpara <hmeghpara@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef _ESCORE_SPI_H
#define _ESCORE_SPI_H

extern struct spi_driver escore_spi_driver;

#define ES_SPI_BOOT_CMD			0x0001
#define ES_SPI_BOOT_ACK			0x0001
#define ES_SPI_SBL_SYNC_CMD		0x80000000
#define ES_SPI_SBL_SYNC_ACK		0x8000FFFF

#if defined(CONFIG_ARCH_OMAP)
#define ES_SPI_FW_SPEED	12000000
#define ES_SPI_OPERATION_SPEED	3000000
#define ES_SPI_LOW_SPEED	1000000
#elif defined(CONFIG_ARCH_MSM)
#define ES_SPI_FW_SPEED	9600000
#define ES_SPI_OPERATION_SPEED	4800000
#define ES_SPI_LOW_SPEED	1000000
#elif defined(CONFIG_ARCH_EXYNOS)
#define ES_SPI_FW_SPEED	9600000
#define ES_SPI_OPERATION_SPEED	4000000
#define ES_SPI_LOW_SPEED	1000000
#endif
#define ESCORE_SPI_PACKET_LEN 64

#define ES_SPI_SYNC_CMD		0x0000
#define ES_SPI_SYNC_ACK		0x0000

extern struct es_stream_device es_spi_streamdev;
extern int escore_spi_init(void);
extern void escore_spi_exit(void);
extern int escore_spi_setup(u32 speed);

#endif
