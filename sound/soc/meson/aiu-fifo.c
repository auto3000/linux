// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 BayLibre, SAS.
// Author: Jerome Brunet <jbrunet@baylibre.com>

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "audio.h"
#include "aiu-fifo.h"

int aiu_fifo_hw_params(struct snd_pcm_substream *substream,
		       struct snd_pcm_hw_params *params,
		       struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	struct audio_fifo *fifo = dai->playback_dma_data;
	dma_addr_t end;
	int ret;
	unsigned int debug_val[2];

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0)
		return ret;

	/* Setup the fifo boundaries */
	end = runtime->dma_addr + runtime->dma_bytes - fifo->fifo_block;
	regmap_write(audio->aiu_map, fifo->mem_offset + AIU_MEM_I2S_START_OFF,
		     runtime->dma_addr);
	regmap_write(audio->aiu_map, fifo->mem_offset + AIU_MEM_I2S_RD_OFF,
		     runtime->dma_addr);
	regmap_write(audio->aiu_map, fifo->mem_offset + AIU_MEM_I2S_END_OFF,
		     end);

	/* Setup the fifo to read all the memory - no skip */
/*
	regmap_update_bits(audio->aiu_map,
			   fifo->mem_offset + AIU_MEM_I2S_MASKS_OFF,
			   AIU_MEM_I2S_MASKS_CH_RD | AIU_MEM_I2S_MASKS_CH_MEM,
			   FIELD_PREP(AIU_MEM_I2S_MASKS_CH_RD, 0xff) |
			   FIELD_PREP(AIU_MEM_I2S_MASKS_CH_MEM, 0xff));
*/
	regmap_read(audio->aiu_map, fifo->mem_offset + AIU_MEM_I2S_START_OFF, &debug_val[0]);
	regmap_read(audio->aiu_map, fifo->mem_offset + AIU_MEM_I2S_RD_OFF, &debug_val[1]);
	printk("aiu_fifo_hw_params: AIU_MEM_I2S_START=%x, AIU_MEM_I2S_RD=%x\n", 
		debug_val[0], debug_val[1]);
	return 0;
}

static int aiu_fifo_i2s_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	struct audio_fifo *fifo = dai->playback_dma_data;
	unsigned int val;
	int ret;

	ret = aiu_fifo_hw_params(substream, params, dai);
	if (ret)
		return ret;

	switch (params_physical_width(params)) {
	case 16:
		val = AIU_MEM_I2S_CONTROL_MODE_16BIT;
		break;
	case 24:
	case 32:
		val = 0;
		break;
	default:
		dev_err(dai->dev, "Unsupported physical width %u\n",
			params_physical_width(params));
		return -EINVAL;
	}

	regmap_update_bits(audio->aiu_map, AIU_MEM_I2S_CONTROL,
			  AIU_MEM_I2S_CONTROL_MODE_16BIT,
			  val);

	/* Set Channel Mask to 0xffff for split mode */
	val = FIELD_PREP(AIU_MEM_I2S_MASKS_CH_MEM |
			 AIU_MEM_I2S_MASKS_CH_RD, 
			 0xffff);
	regmap_update_bits(audio->aiu_map, AIU_MEM_I2S_MASKS, 
			  AIU_MEM_I2S_MASKS_CH_MEM |
			  AIU_MEM_I2S_MASKS_CH_RD,
			  val);

	/* Setup the irq periodicity */
	val = params_period_bytes(params) / fifo->fifo_block;
	val = FIELD_PREP(AIU_MEM_I2S_MASKS_IRQ_BLOCK, val);
	regmap_update_bits(audio->aiu_map, AIU_MEM_I2S_MASKS,
			  AIU_MEM_I2S_MASKS_IRQ_BLOCK, val);
	return 0;
}

int aiu_fifo_prepare(struct snd_pcm_substream *substream,
		     struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	struct audio_fifo *fifo = dai->playback_dma_data;
	unsigned int debug_val;

	regmap_update_bits(audio->aiu_map,
			   fifo->mem_offset + AIU_MEM_I2S_CONTROL_OFF,
			   AIU_MEM_I2S_CONTROL_INIT,
			   AIU_MEM_I2S_CONTROL_INIT);
	regmap_update_bits(audio->aiu_map,
			   fifo->mem_offset + AIU_MEM_I2S_CONTROL_OFF,
			   AIU_MEM_I2S_CONTROL_INIT, 0);
	regmap_read(audio->aiu_map, fifo->mem_offset + AIU_MEM_I2S_CONTROL_OFF, &debug_val);
	printk("aiu_fifo_prepare: AIU_MEM_I2S_CONTROL=%x\n", debug_val);
	return 0;
}

static int aiu_fifo_i2s_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	int ret;

	ret = aiu_fifo_prepare(substream, dai);
	if (ret)
		return ret;

	regmap_update_bits(audio->aiu_map,
			  AIU_MEM_I2S_BUF_CNTL,
			  AIU_MEM_I2S_BUF_CNTL_INIT,
			  AIU_MEM_I2S_BUF_CNTL_INIT);
	regmap_update_bits(audio->aiu_map,
			  AIU_MEM_I2S_BUF_CNTL,
			  AIU_MEM_I2S_BUF_CNTL_INIT, 0);

	return 0;
}

static void aiu_fifo_enable(struct snd_soc_dai *dai, bool enable)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	struct audio_fifo *fifo = dai->playback_dma_data;
	unsigned int en_mask = (AIU_MEM_I2S_CONTROL_FILL_EN |
				AIU_MEM_I2S_CONTROL_EMPTY_EN);
	unsigned int debug_val;

	regmap_update_bits(audio->aiu_map,
			   fifo->mem_offset + AIU_MEM_I2S_CONTROL_OFF,
			   en_mask, enable ? en_mask : 0);
	regmap_read(audio->aiu_map, fifo->mem_offset + AIU_MEM_I2S_CONTROL_OFF, &debug_val);
	printk("aiu_fifo_enable: AIU_MEM_I2S_CONTROL=%x\n", debug_val);
}

int aiu_fifo_trigger(struct snd_pcm_substream *substream, int cmd,
		     struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		aiu_fifo_enable(dai, true);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		aiu_fifo_enable(dai, false);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aiu_fifo_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	unsigned int val;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		regmap_write(audio->aiu_map, AIU_RST_SOFT,
			     AIU_RST_SOFT_I2S_FAST);
		regmap_read(audio->aiu_map, AIU_I2S_SYNC, &val);
		break;
	}

	return aiu_fifo_trigger(substream, cmd, dai);
}

int aiu_fifo_hw_free(struct snd_pcm_substream *substream,
		     struct snd_soc_dai *dai)
{
	return snd_pcm_lib_free_pages(substream);
}

static irqreturn_t aiu_fifo_isr(int irq, void *dev_id)
{
	struct snd_pcm_substream *playback = dev_id;

	snd_pcm_period_elapsed(playback);
	printk("aiu_fifo_isr\n");
	return IRQ_HANDLED;
}

int aiu_fifo_startup(struct snd_pcm_substream *substream,
		     struct snd_soc_dai *dai)
{
	struct audio_fifo *fifo = dai->playback_dma_data;
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

	ret = request_irq(fifo->irq, aiu_fifo_isr, 0, fifo->irq_name,
			  substream);
	printk("aiu_fifo_startup: devname=%s, irq_name=%s\n", dev_name(dai->dev), 
		fifo->irq_name);
	if (ret)
		clk_disable_unprepare(fifo->pclk);

	return ret;
}

void aiu_fifo_shutdown(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct audio_fifo *fifo = dai->playback_dma_data;

	free_irq(fifo->irq, substream);
	clk_disable_unprepare(fifo->pclk);
}

int aiu_fifo_dai_probe(struct snd_soc_dai *dai)
{
	struct audio_fifo *fifo;

	fifo = kzalloc(sizeof(*fifo), GFP_KERNEL);
	if (!fifo)
		return -ENOMEM;

	dai->playback_dma_data = fifo;

	return 0;
}

int aiu_fifo_dai_remove(struct snd_soc_dai *dai)
{
	kfree(dai->playback_dma_data);

	return 0;
}

const struct snd_soc_dai_ops aiu_fifo_i2s_dai_ops = {
	.hw_params	= aiu_fifo_i2s_hw_params,
	.prepare	= aiu_fifo_i2s_prepare,
	.trigger	= aiu_fifo_i2s_trigger,
	.hw_free	= aiu_fifo_hw_free,
	.startup	= aiu_fifo_startup,
	.shutdown	= aiu_fifo_shutdown,
};

