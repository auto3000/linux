// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 BayLibre, SAS.
// Author: Jerome Brunet <jbrunet@baylibre.com>

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "aiu.h"
#include "audin.h"
#include "audin-fifo.h"

#define AUDIN_FIFO1_CTRL_EN		BIT(0)
#define AUDIN_FIFO1_CTRL_RST		BIT(1)
#define AUDIN_FIFO1_CTRL_LOAD		BIT(2)
#define AUDIN_FIFO1_CTRL_DIN_SEL	GENMASK(6, 3)
#define AUDIN_FIFO1_CTRL_ENDIAN		GENMASK(10, 8)
#define AUDIN_FIFO1_CTRL_CHAN		GENMASK(14, 11)
#define AUDIN_FIFO1_CTRL_UG		BIT(15)

#define AUDIN_FIFO_SPDIF_BLOCK			8

static struct snd_pcm_hardware fifo_spdif_pcm = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_PAUSE),
	.formats = AUDIN_FORMATS,
	.rate_min = 5512,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 2,
	.period_bytes_min = AUDIN_FIFO_SPDIF_BLOCK,
	.period_bytes_max = AUDIN_FIFO_SPDIF_BLOCK * USHRT_MAX,
	.periods_min = 2,
	.periods_max = UINT_MAX,

	/* No real justification for this */
	.buffer_bytes_max = 1 * 1024 * 1024,
};

static int audin_fifo_spdif_prepare(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	unsigned int desc = AUDIN_FIFO1_CTRL_EN |
			    AUDIN_FIFO1_CTRL_LOAD |
			    AUDIN_FIFO1_CTRL_UG ;

	desc |= FIELD_PREP(AUDIN_FIFO1_CTRL_DIN_SEL, SPDIF);
	desc |= FIELD_PREP(AUDIN_FIFO1_CTRL_ENDIAN, 4);
	desc |= FIELD_PREP(AUDIN_FIFO1_CTRL_CHAN, 2);

	snd_soc_component_update_bits(component,
				      AUDIN_FIFO1_CTRL,
				      AUDIN_FIFO1_CTRL_EN |
			    	      AUDIN_FIFO1_CTRL_LOAD |
				      AUDIN_FIFO1_CTRL_DIN_SEL |
				      AUDIN_FIFO1_CTRL_ENDIAN |
				      AUDIN_FIFO1_CTRL_CHAN |
			    	      AUDIN_FIFO1_CTRL_UG,
				      desc);
	return 0;
}

const struct snd_soc_dai_ops audin_fifo_spdif_dai_ops = {
	.trigger	= audin_fifo_trigger,
	.prepare	= audin_fifo_spdif_prepare,
	.hw_params	= audin_fifo_hw_params,
	.hw_free	= audin_fifo_hw_free,
	.startup	= audin_fifo_startup,
	.shutdown	= audin_fifo_shutdown,
};

int audin_fifo_spdif_dai_probe(struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct audin *audin = snd_soc_component_get_drvdata(component);
	struct audin_fifo *fifo;
	int ret;

	ret = audin_fifo_dai_probe(dai);
	if (ret)
		return ret;

	fifo = dai->capture_dma_data;

	fifo->pcm = &fifo_spdif_pcm;
	fifo->mem_offset = AUDIN_FIFO1_START;
	fifo->fifo_block = 1;
	fifo->pclk = audin->spdif.clks[PCLK].clk;
	fifo->irq = audin->spdif.irq;

	return 0;
}

