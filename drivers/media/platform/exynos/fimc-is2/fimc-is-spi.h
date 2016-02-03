/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SPI_H
#define FIMC_IS_SPI_H

#define FIMC_IS_SPI_OUTPUT	1
#define FIMC_IS_SPI_FUNC	2

struct fimc_is_spi_gpio {
	char *clk;
	char *ssn;
	char *miso;
	char *mosi;
	char *pinname;
};

struct fimc_is_spi {
	struct spi_device	*device;
	char			*node;
	u32			channel;
	struct fimc_is_spi_gpio	gpio;
};

void fimc_is_spi_s_port(struct fimc_is_spi_gpio *spi_gpio, int func, bool ssn);
int fimc_is_spi_reset(struct fimc_is_spi *spi);
int fimc_is_spi_read(struct fimc_is_spi *spi, void *buf, u32 addr, size_t size);
int fimc_is_spi_read_module_id(struct fimc_is_spi *spi, void *buf, u16 addr, size_t size);
#endif
