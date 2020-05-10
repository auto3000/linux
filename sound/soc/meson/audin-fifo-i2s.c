/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2020 Rezzonics
 * Copyright (c) 2020 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 *	   Rezzonics <rezzonics@gmail.com>
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "audin.h"
#include "audin-fifo.h"

#define AUDIN_FIFO0_CTRL_EN		BIT(0)
#define AUDIN_FIFO0_CTRL_RST		BIT(1)
#define AUDIN_FIFO0_CTRL_LOAD		BIT(2)
#define AUDIN_FIFO0_CTRL_DIN_SEL	GENMASK(5, 3)
#define AUDIN_FIFO0_CTRL_ENDIAN		GENMASK(10, 8)
#define AUDIN_FIFO0_CTRL_CHAN		GENMASK(14, 11)
#define AUDIN_FIFO0_CTRL_UG		BIT(15)

#define AUDIN_FIFO0_CTRL1_DINPOS	GENMASK(1, 0)
#define AUDIN_FIFO0_CTRL1_DINBYTENUM	GENMASK(3, 2)
#define AUDIN_FIFO0_CTRL1_DESTSEL	GENMASK(5, 4)
#define AUDIN_FIFO0_CTRL1_DINPOS2	BIT(7)

#define AUDIN_FIFO_I2S_BLOCK		64

static struct snd_pcm_hardware fifo_i2s_pcm = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER |
		 SNDRV_PCM_INFO_PAUSE),
	.formats = AUDIN_FORMATS,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 8,
	.period_bytes_min = AUDIN_FIFO_I2S_BLOCK,
	.period_bytes_max = 32 * 1024,
	.periods_min = 2,
	.periods_max = 256,

	/* No real justification for this */
	.buffer_bytes_max = 64 * 1024,
	.fifo_size = 0,
};

static int audin_fifo_i2s_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	unsigned int channels = (substream->runtime->channels == 2) ? 2 : 1;
	unsigned int desc = AUDIN_FIFO0_CTRL_EN |
			    AUDIN_FIFO0_CTRL_LOAD |
			    AUDIN_FIFO0_CTRL_UG ;

	desc |= FIELD_PREP(AUDIN_FIFO0_CTRL_DIN_SEL, I2S);
	desc |= FIELD_PREP(AUDIN_FIFO0_CTRL_ENDIAN, 4);
	desc |= FIELD_PREP(AUDIN_FIFO0_CTRL_CHAN, channels);

	snd_soc_component_update_bits(component,
				      AUDIN_FIFO0_CTRL,
				      AUDIN_FIFO0_CTRL_EN |
			    	      AUDIN_FIFO0_CTRL_LOAD |
				      AUDIN_FIFO0_CTRL_DIN_SEL |
				      AUDIN_FIFO0_CTRL_ENDIAN |
				      AUDIN_FIFO0_CTRL_CHAN |
			    	      AUDIN_FIFO0_CTRL_UG,
				      desc);
	/* DIN_BYTE_NUM: 0:8bit, 1:16bit, 2:32bit (24bit)? */
	desc = FIELD_PREP(AUDIN_FIFO0_CTRL1_DINBYTENUM, 2);
	snd_soc_component_update_bits(component,
				      AUDIN_FIFO0_CTRL1,
				      AUDIN_FIFO0_CTRL1_DINPOS |
			    	      AUDIN_FIFO0_CTRL1_DINBYTENUM |
				      AUDIN_FIFO0_CTRL1_DESTSEL |
				      AUDIN_FIFO0_CTRL1_DINPOS2,
				      desc);
	return 0;
}

const struct snd_soc_dai_ops audin_fifo_i2s_dai_ops = {
	.trigger	= audin_fifo_trigger,
	.prepare	= audin_fifo_i2s_prepare,
	.hw_params	= audin_fifo_hw_params,
	.hw_free	= audin_fifo_hw_free,
	.startup	= audin_fifo_startup,
	.shutdown	= audin_fifo_shutdown,
};

int audin_fifo_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct audin *audin = snd_soc_component_get_drvdata(component);
	struct audin_fifo *fifo;
	int ret;

	ret = audin_fifo_dai_probe(dai);
	if (ret)
		return ret;

	fifo = dai->capture_dma_data;

	fifo->pcm = &fifo_i2s_pcm;
	fifo->mem_offset = AUDIN_FIFO0_START;
	fifo->fifo_block = AUDIN_FIFO_I2S_BLOCK;
	fifo->pclk = audin->i2s.clks[PCLK].clk;
	fifo->irq = audin->i2s.irq;

	return 0;
}

