/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2020 Rezzonics
 * Copyright (c) 2018 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 * 	   Rezzonics <rezzonics@gmail.com>
 */
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "audin-fifo.h"
#include "audin.h"

/* Registers offset from AUDIN_FIFO0_START*/
#define AUDIN_FIFO0_START_OFF			0x00
#define AUDIN_FIFO0_END_OFF			0x04
#define AUDIN_FIFO0_PTR_OFF			0x08
#define AUDIN_FIFO0_CTRL_OFF			0x14
#define AUDIN_FIFO0_CTRL_OFF_EN 		BIT(0)

static struct snd_soc_dai *audin_fifo_dai(struct snd_pcm_substream *ss)
{
	struct snd_soc_pcm_runtime *rtd = ss->private_data;

	return rtd->cpu_dai;
}

snd_pcm_uframes_t audin_fifo_pointer(struct snd_soc_component *component,
				   struct snd_pcm_substream *substream)
{
	struct snd_soc_dai *dai = audin_fifo_dai(substream);
	struct audin_fifo *fifo = dai->capture_dma_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int addr;

	snd_soc_component_read(component, fifo->mem_offset + AUDIN_FIFO0_PTR_OFF,
			       &addr);

	return bytes_to_frames(runtime, addr - (unsigned int)runtime->dma_addr);
}

static void audin_fifo_enable(struct snd_soc_dai *dai, bool enable)
{
	struct snd_soc_component *component = dai->component;
	struct audin_fifo *fifo = dai->capture_dma_data;
	unsigned int fifo_en = AUDIN_FIFO0_CTRL_OFF_EN;

	snd_soc_component_update_bits(component,
				      fifo->mem_offset + AUDIN_FIFO0_CTRL_OFF,
				      fifo_en, enable ? fifo_en : 0);
}

int audin_fifo_trigger(struct snd_pcm_substream *substream, int cmd,
		     struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		audin_fifo_enable(dai, true);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		audin_fifo_enable(dai, false);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int audin_fifo_hw_params(struct snd_pcm_substream *substream,
		         struct snd_pcm_hw_params *params,
		         struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_component *component = dai->component;
	struct audin_fifo *fifo = dai->capture_dma_data;
	dma_addr_t end;
	int ret;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0)
		return ret;

	/* Setup the fifo boundaries */
	end = runtime->dma_addr + runtime->dma_bytes - fifo->fifo_block;
	snd_soc_component_write(component, fifo->mem_offset + AUDIN_FIFO0_START_OFF,
				runtime->dma_addr);
	snd_soc_component_write(component, fifo->mem_offset + AUDIN_FIFO0_PTR_OFF,
				runtime->dma_addr);
	snd_soc_component_write(component, fifo->mem_offset + AUDIN_FIFO0_END_OFF,
				end);

	return 0;
}

int audin_fifo_hw_free(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	return snd_pcm_lib_free_pages(substream);
}

static irqreturn_t audin_fifo_isr(int irq, void *dev_id)
{
	struct snd_pcm_substream *capture = dev_id;

	snd_pcm_period_elapsed(capture);

	return IRQ_HANDLED;
}

int audin_fifo_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct audin_fifo *fifo = dai->capture_dma_data;
	int ret;

	snd_soc_set_runtime_hwparams(substream, fifo->pcm);

	/*
	 * Make sure the buffer and period size are multiple of the fifo burst
	 * size
	 */
	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
					 fifo->fifo_block);
	if (ret)
		return ret;

	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
					 fifo->fifo_block);
	if (ret)
		return ret;

	ret = clk_prepare_enable(fifo->pclk);
	if (ret)
		return ret;

	ret = request_irq(fifo->irq, audin_fifo_isr, 0, dev_name(dai->dev),
			  substream);
	if (ret)
		clk_disable_unprepare(fifo->pclk);

	return ret;
}

void audin_fifo_shutdown(struct snd_pcm_substream *substream,
		         struct snd_soc_dai *dai)
{
	struct audin_fifo *fifo = dai->capture_dma_data;

	free_irq(fifo->irq, substream);
	clk_disable_unprepare(fifo->pclk);
}

int audin_fifo_pcm_new(struct snd_soc_pcm_runtime *rtd,
		       struct snd_soc_dai *dai)
{
	struct snd_pcm_substream *substream =
		rtd->pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	struct snd_card *card = rtd->card->snd_card;
	struct audin_fifo *fifo = dai->capture_dma_data;
	size_t size = fifo->pcm->buffer_bytes_max;

	snd_pcm_lib_preallocate_pages(substream,
				      SNDRV_DMA_TYPE_DEV,
				      card->dev, size, size);

	return 0;
}

int audin_fifo_dai_probe(struct snd_soc_dai *dai)
{
	struct audin_fifo *fifo;

	fifo = kzalloc(sizeof(*fifo), GFP_KERNEL);
	if (!fifo)
		return -ENOMEM;

	dai->capture_dma_data = fifo;

	return 0;
}

int audin_fifo_dai_remove(struct snd_soc_dai *dai)
{
	kfree(dai->capture_dma_data);

	return 0;
}

