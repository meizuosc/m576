/* sound/soc/samsung/eax.h
 *
 * Exynos Audio Mixer driver
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_AMIXER_H
#define _EXYNOS_AMIXER_H

extern int eax_dev_register(struct device *dev, const char *name,
			struct s3c_dma_params *dma, int ch);
extern int eax_dai_register(struct snd_soc_dai *dai,
			const struct snd_soc_dai_ops *dai_ops,
			int (*dai_suspend)(struct snd_soc_dai *dai),
			int (*dai_resume)(struct snd_soc_dai *dai));
extern int eax_dai_unregister(void);
extern int eax_dma_dai_register(struct snd_soc_dai *dai);
extern int eax_dma_dai_unregister(void);
extern int eax_dma_params_register(struct s3c_dma_params *dma);
extern int eax_asoc_platform_register(struct device *dev);

#endif
