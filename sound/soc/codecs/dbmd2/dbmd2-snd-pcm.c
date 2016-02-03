/*
 * snd-dbmd2-pcm.c -- DVF99 DBMD2 ASoC platform driver
 *
 *  Copyright (C) 2014 DSPG Technologies GmbH
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>

#define DRV_NAME "dbmd2-snd-soc-platform"

/* defaults */
/* must be a multiple of 4 */
#define MAX_BUFFER_SIZE		96000 /* 3 seconds */
#define MIN_PERIOD_SIZE		64
#define MAX_PERIOD_SIZE		(MAX_BUFFER_SIZE / 4)
#define USE_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE | \
				 SNDRV_PCM_FMTBIT_MU_LAW)
#define USE_RATE		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)
#define USE_RATE_MIN		8000
#define USE_RATE_MAX		16000
#define USE_CHANNELS_MIN	1
#define USE_CHANNELS_MAX	1
#define USE_PERIODS_MIN		1
#define USE_PERIODS_MAX		1024
/* 3 seconds + 4 bytes for position */
#define REAL_BUFFER_SIZE	(MAX_BUFFER_SIZE + 4)

struct snd_dbmd2 {
	struct snd_soc_card *card;
	struct snd_pcm_hardware pcm_hw;
	struct timer_list timer;
};

static struct snd_pcm_hardware dbmd2_pcm_hardware = {
	.info =			(SNDRV_PCM_INFO_MMAP |
				 SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_RESUME |
				 SNDRV_PCM_INFO_MMAP_VALID |
				 SNDRV_PCM_INFO_BATCH),
	.formats =		USE_FORMATS,
	.rates =		USE_RATE,
	.rate_min =		USE_RATE_MIN,
	.rate_max =		USE_RATE_MAX,
	.channels_min =		USE_CHANNELS_MIN,
	.channels_max =		USE_CHANNELS_MAX,
	.buffer_bytes_max =	MAX_BUFFER_SIZE,
	.period_bytes_min =	MIN_PERIOD_SIZE,
	.period_bytes_max =	MAX_PERIOD_SIZE,
	.periods_min =		USE_PERIODS_MIN,
	.periods_max =		USE_PERIODS_MAX,
	.fifo_size =		0,
};

extern int dbmd2_get_samples(char *buffer, unsigned int samples);
extern int dbmd2_codec_lock(void);
extern int dbmd2_codec_unlock(void);
extern void dbmd2_start_buffering(void);
extern void dbmd2_stop_buffering(void);

static u32 stream_get_position(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return *(volatile u32 *)&(runtime->dma_area[MAX_BUFFER_SIZE]);
}

static void stream_set_position(struct snd_pcm_substream *substream,
				u32 position)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	*(volatile u32 *)&(runtime->dma_area[MAX_BUFFER_SIZE]) = position;
}

static void dbmd2_pcm_timer(unsigned long _substream)
{
	struct snd_pcm_substream *substream =
				(struct snd_pcm_substream *)_substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct timer_list *timer = substream->runtime->private_data;
	unsigned int size = snd_pcm_lib_buffer_bytes(substream);
	u32 pos;
	unsigned long msecs;
	unsigned long to_copy;

	pos = stream_get_position(substream);
	to_copy = frames_to_bytes(runtime, runtime->period_size);
	pos += to_copy;
	if (pos >= size)
		pos = 0;
	stream_set_position(substream, pos);

	if (dbmd2_get_samples(runtime->dma_area + pos, runtime->period_size))
		memset(runtime->dma_area + pos, 0, to_copy);

	snd_pcm_period_elapsed(substream);

	msecs = (runtime->period_size * 1000) / runtime->rate;
	mod_timer(timer, jiffies + msecs_to_jiffies(msecs));
}

static int dbmd2_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct timer_list *timer;

	if (dbmd2_codec_lock())
		return -EBUSY;

	dbmd2_start_buffering();

	timer = kzalloc(sizeof(*timer), GFP_KERNEL);
	if (!timer) {
		dbmd2_stop_buffering();
		dbmd2_codec_unlock();
		return -ENOMEM;
	}

	init_timer(timer);
	timer->function = dbmd2_pcm_timer;
	timer->data = (unsigned long)substream;

	runtime->private_data = timer;

	snd_soc_set_runtime_hwparams(substream, &dbmd2_pcm_hardware);

	return 0;
}

static int dbmd2_stop_period_timer(struct snd_pcm_substream *substream)
{
	struct timer_list *timer = substream->runtime->private_data;

	del_timer_sync(timer);

	return 0;
}

static int dbmd2_pcm_close(struct snd_pcm_substream *substream)
{
	struct timer_list *timer = substream->runtime->private_data;

	dbmd2_stop_buffering();
	dbmd2_stop_period_timer(substream);
	kfree(timer);

	dbmd2_codec_unlock();

	return 0;
}

static int dbmd2_pcm_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes   = params_buffer_bytes(hw_params);
	runtime->buffer_size = params_buffer_size(hw_params);

	return 0;
}

static int dbmd2_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	memset(runtime->dma_area, 0, REAL_BUFFER_SIZE);
	return 0;
}

static int dbmd2_start_period_timer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct timer_list *timer = runtime->private_data;
	unsigned long msecs;

	*(volatile u32 *)&(runtime->dma_area[MAX_BUFFER_SIZE]) = 0;
	msecs = (runtime->period_size * 1000) / runtime->rate;
	mod_timer(timer, jiffies + msecs_to_jiffies(msecs));

	return 0;
}

static int dbmd2_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		return dbmd2_start_period_timer(substream);
	case SNDRV_PCM_TRIGGER_RESUME:
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
		return dbmd2_stop_period_timer(substream);
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return 0;
	}

	return -EINVAL;
}

static snd_pcm_uframes_t dbmd2_pcm_pointer(struct snd_pcm_substream *substream)
{
	u32 pos;

	pos = stream_get_position(substream);
	return bytes_to_frames(substream->runtime, pos);
}

static struct snd_pcm_ops dbmd2_pcm_ops = {
	.open		= dbmd2_pcm_open,
	.close		= dbmd2_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= dbmd2_pcm_hw_params,
	.prepare	= dbmd2_pcm_prepare,
	.trigger	= dbmd2_pcm_trigger,
	.pointer	= dbmd2_pcm_pointer,
};

static int dbmd2_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = MAX_BUFFER_SIZE;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_coherent(pcm->card->dev,
				       REAL_BUFFER_SIZE,
				       &buf->addr,
				       GFP_KERNEL);
	if (!buf->area) {
		pr_err("Failed to allocate dma memory.\n");
	    pr_err("Please increase uncached DMA memory region\n");
		return -ENOMEM;
	}
	buf->bytes = size;

	return 0;
}

static int dbmd2_pcm_probe(struct snd_soc_platform *pt)
{
	struct snd_dbmd2 *dbmd2;

	dbmd2 = kzalloc(sizeof(*dbmd2), GFP_KERNEL);
	if (!dbmd2)
		return -ENOMEM;

	dbmd2->card = pt->card;
	dbmd2->pcm_hw = dbmd2_pcm_hardware;
	snd_soc_platform_set_drvdata(pt, dbmd2);

	return 0;
}

static int dbmd2_pcm_remove(struct snd_soc_platform *pt)
{
	struct snd_dbmd2 *dbmd2;

	dbmd2 = snd_soc_platform_get_drvdata(pt);
	kfree(dbmd2);

	return 0;
}

static int dbmd2_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_pcm *pcm;
	int ret = 0;

	pcm = runtime->pcm;
	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = dbmd2_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = dbmd2_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
out:
	return ret;
}

static void dbmd2_pcm_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;
		dma_free_coherent(pcm->card->dev,
				  REAL_BUFFER_SIZE,
				  (void *)buf->area,
				  buf->addr);
		buf->area = NULL;
	}
}

static struct snd_soc_platform_driver dbmd2_soc_platform = {
	.probe		= &dbmd2_pcm_probe,
	.remove		= &dbmd2_pcm_remove,
	.ops		= &dbmd2_pcm_ops,
	.pcm_new	= dbmd2_pcm_new,
	.pcm_free	= dbmd2_pcm_free,
};

static int dbmd2_pcm_platform_probe(struct platform_device *pdev)
{
	int err;

	err = snd_soc_register_platform(&pdev->dev, &dbmd2_soc_platform);
	if (err)
		dev_err(&pdev->dev, "snd_soc_register_platform() failed");

	return err;
}

static int dbmd2_pcm_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static struct of_device_id snd_soc_platform_of_ids[] = {
	{ .compatible = "dspg,dbmd2-snd-soc-platform" },
	{ },
};

static struct platform_driver dbmd2_pcm_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = snd_soc_platform_of_ids,
	},
	.probe = dbmd2_pcm_platform_probe,
	.remove = dbmd2_pcm_platform_remove,
};

static int __init snd_dbmd2_pcm_init(void)
{
	return platform_driver_register(&dbmd2_pcm_driver);
}
module_init(snd_dbmd2_pcm_init);

static void __exit snd_dbmd2_pcm_exit(void)
{
	platform_driver_unregister(&dbmd2_pcm_driver);
}
module_exit(snd_dbmd2_pcm_exit);

MODULE_DESCRIPTION("DBMD2 ASoC platform driver");
MODULE_AUTHOR("DSP Group");
MODULE_LICENSE("GPL");
