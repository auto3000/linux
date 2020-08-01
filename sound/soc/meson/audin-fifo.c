/* 
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (c) 2018 BayLibre, SAS.
*		2020 Rezzonics
* Author: Jerome Brunet <jbrunet@baylibre.com>
*	  Rezzonics <rezzonics@gmail.com>
*/
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "audin-fifo.h"
#include "audio.h"

int audin_fifo_i2s_hw_params(struct snd_pcm_substream *substream,
		    	     struct snd_pcm_hw_params *params,
		  	     struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	struct audio_fifo *fifo = dai->capture_dma_data;
	dma_addr_t end;
	dma_addr_t end_block;
	unsigned int desc = 0;
	unsigned int desc1 = 0;
	unsigned int val = 0;
	int ret;
//	unsigned int debug_val[2];

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0)
		return ret;

	/* Setup the fifo boundaries */
	end = runtime->dma_addr + runtime->dma_bytes; // - fifo->fifo_block;
	end_block = runtime->dma_addr + fifo->fifo_block;
	regmap_write(audio->audin_map, fifo->mem_offset + AUDIN_FIFO_START_OFF,
		     runtime->dma_addr);
	regmap_write(audio->audin_map, fifo->mem_offset + AUDIN_FIFO_PTR_OFF,
		     runtime->dma_addr);
	regmap_write(audio->audin_map, fifo->mem_offset + AUDIN_FIFO_END_OFF,
		     end);
	regmap_write(audio->audin_map, fifo->mem_offset + AUDIN_FIFO_INTR_OFF,
		     end_block);
/*
	regmap_read(audio->audin_map, fifo->mem_offset + AUDIN_FIFO_START_OFF, &debug_val[0]);
	regmap_write(audio->audin_map, fifo->mem_offset + AUDIN_FIFO_PTR_OFF, 1);
	regmap_read(audio->audin_map, fifo->mem_offset + AUDIN_FIFO_PTR_OFF, &debug_val[1]);
	printk("audin_fifo_i2s_hw_params: AUDIN_FIFO_START=%x, AUDIN_FIFO_PTR=%x\n", 
		debug_val[0], debug_val[1]);
*/
	/* DIN_POS: 0: 1:1-byte, 2:2-bytes 3:3-bytes 4:4-bytes? */
	/* DIN_BYTE_NUM: 0:8bit, 1:16bit, 2:32bit (24bit) */
	switch (params_physical_width(params)) {
	case 16:
		desc |= FIELD_PREP(AUDIN_FIFO_CTRL_ENDIAN, 4);
		desc1 |= FIELD_PREP(AUDIN_FIFO_CTRL1_DINPOS, 1) |
			 FIELD_PREP(AUDIN_FIFO_CTRL1_DINBYTENUM, 1);;
		val = AIU_MEM_I2S_CONTROL_MODE_16BIT;
		break;
	case 24:
	case 32:
		desc |= FIELD_PREP(AUDIN_FIFO_CTRL_ENDIAN, 4);
		desc1 |= FIELD_PREP(AUDIN_FIFO_CTRL1_DINPOS, 1) |
			 FIELD_PREP(AUDIN_FIFO_CTRL1_DINBYTENUM, 2);;
		break;
	default:
		dev_err(dai->dev, "Unsupported physical width %u\n",
			params_physical_width(params));
		return -EINVAL;
	}

	desc |= FIELD_PREP(AUDIN_FIFO_CTRL_DIN_SEL, I2S) |
		FIELD_PREP(AUDIN_FIFO_CTRL_CHAN, 1);

	regmap_update_bits(audio->aiu_map, AIU_MEM_I2S_CONTROL,
			  AIU_MEM_I2S_CONTROL_MODE_16BIT,
			  val);

	regmap_update_bits(audio->audin_map,
			   AUDIN_FIFO0_CTRL,
			   AUDIN_FIFO_CTRL_EN |
			   AUDIN_FIFO_CTRL_LOAD |
			   AUDIN_FIFO_CTRL_DIN_SEL |
			   AUDIN_FIFO_CTRL_ENDIAN |
			   AUDIN_FIFO_CTRL_CHAN |
			   AUDIN_FIFO_CTRL_UG,
			   desc);

	regmap_update_bits(audio->audin_map,
			   AUDIN_FIFO0_CTRL1,
			   AUDIN_FIFO_CTRL1_DINPOS |
			   AUDIN_FIFO_CTRL1_DINBYTENUM,
			   desc1);

	/* Setup the fifo to read all the memory - no skip */
	/* Set Channel Mask to 0xffff for split mode */
	val = FIELD_PREP(AIU_MEM_I2S_MASKS_CH_MEM |
			 AIU_MEM_I2S_MASKS_CH_RD, 
			 0xffff);
	regmap_update_bits(audio->aiu_map, AIU_MEM_I2S_MASKS, 
			  AIU_MEM_I2S_MASKS_CH_MEM |
			  AIU_MEM_I2S_MASKS_CH_RD,
			  val);

	/* Setup the irq periodicity */
#if 0
	val = params_period_bytes(params) / fifo->fifo_block;
	val = FIELD_PREP(AIU_MEM_I2S_MASKS_IRQ_BLOCK, val);
	regmap_update_bits(audio->aiu_map, AIU_MEM_I2S_MASKS,
			  AIU_MEM_I2S_MASKS_IRQ_BLOCK, val);
#endif
	/* Enable FIFO0 address trigger interrupt */
	regmap_update_bits(audio->audin_map, AUDIN_INT_CTRL,
			   AUDIN_INT_CTRL_FIFO0_ADDR, 1);

/*
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL, &debug_val[0]);
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL1, &debug_val[1]);
	printk("audin_fifo_i2s_hw_params: AUDIN_FIFO0_CTRL=%x, AUDIN_FIFO0_CTRL1=%x\n", 
		debug_val[0], debug_val[1]);
*/
	return 0;
}

static int audin_fifo_i2s_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
//	unsigned int debug_val[2];

	regmap_update_bits(audio->aiu_map,
			   AIU_MEM_I2S_CONTROL,
			   AIU_MEM_I2S_CONTROL_INIT,
			   AIU_MEM_I2S_CONTROL_INIT);
	regmap_update_bits(audio->aiu_map,
			   AIU_MEM_I2S_CONTROL,
			   AIU_MEM_I2S_CONTROL_INIT,
			   0);

	regmap_update_bits(audio->aiu_map,
			  AIU_MEM_I2S_BUF_CNTL,
			  AIU_MEM_I2S_BUF_CNTL_INIT,
			  AIU_MEM_I2S_BUF_CNTL_INIT);
	regmap_update_bits(audio->aiu_map,
			  AIU_MEM_I2S_BUF_CNTL,
			  AIU_MEM_I2S_BUF_CNTL_INIT, 
			  0);
/*
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL, &debug_val[0]);
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL1, &debug_val[1]);
	printk("audin_fifo_i2s_prepare: AUDIN_FIFO0_CTRL=%x, AUDIN_FIFO0_CTRL1=%x\n", 
		debug_val[0], debug_val[1]);
*/
	return 0;
}

static void audin_fifo_enable(struct snd_soc_dai *dai, bool enable)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	struct audio_fifo *fifo = dai->capture_dma_data;
	unsigned int aiu_mask = (AIU_MEM_I2S_CONTROL_FILL_EN |
				AIU_MEM_I2S_CONTROL_EMPTY_EN);
	unsigned int en_mask = AUDIN_FIFO_CTRL_EN |
			       AUDIN_FIFO_CTRL_LOAD |
			       AUDIN_FIFO_CTRL_UG;
//	unsigned int debug_val;

	regmap_update_bits(audio->aiu_map,
			   AIU_MEM_I2S_CONTROL,
			   aiu_mask, enable ? aiu_mask : 0);
	regmap_update_bits(audio->audin_map,
			   fifo->mem_offset + AUDIN_FIFO_CTRL_OFF,
			   en_mask, enable ? en_mask : 0);
/*
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL, &debug_val);
	printk("audin_fifo_enable: AUDIN_FIFO0_CTRL=%x\n", debug_val);
*/
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

static int audin_fifo_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
				  struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	unsigned int rd_ptr, start;
	unsigned int val = 0;
//	unsigned in debug_val;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		do {
			regmap_update_bits(audio->audin_map, AUDIN_FIFO0_CTRL,
					   AUDIN_FIFO_CTRL_RST,
					   AUDIN_FIFO_CTRL_RST);
			regmap_update_bits(audio->audin_map, AUDIN_FIFO0_CTRL,
					   AUDIN_FIFO_CTRL_RST,
					   0);
//			regmap_read(audio->audin_map, AUDIN_FIFO_CTRL, &debug_val);
//			printk("audin_fifo_i2s_trigger: AUDIN_FIFO_CTRL=%x\n", debug_val);
			regmap_write(audio->audin_map, AUDIN_FIFO0_PTR, 0);
			regmap_read(audio->audin_map, AUDIN_FIFO0_PTR, &rd_ptr);
			regmap_read(audio->audin_map, AUDIN_FIFO0_START, &start);
		} while(rd_ptr != start);

		regmap_write(audio->aiu_map, AIU_RST_SOFT,
			     AIU_RST_SOFT_I2S_FAST);
		regmap_read(audio->aiu_map, AIU_I2S_SYNC, &val);
/*
		regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL, &debug_val);
	 	printk("audin_fifo_i2s_trigger: ");
		printk("AUDIN_FIFO0_START=%x, AUDIN_FIFO0_PTR=%x, AUDIN_FIFO0_CTRL=%x\n",
			start, rd_ptr, debug_val );
*/
		break;
	}
	return audin_fifo_trigger(substream, cmd, dai);
}

int audin_fifo_hw_free(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);

	/* Disable FIFO0 address trigger interrupt */
	regmap_update_bits(audio->audin_map, AUDIN_INT_CTRL,
			   AUDIN_INT_CTRL_FIFO0_ADDR, 0);

	return snd_pcm_lib_free_pages(substream);
}

static irqreturn_t audin_fifo_isr(int irq, void *dev_id)
{
	struct snd_pcm_substream *capture = dev_id;
	struct snd_soc_pcm_runtime *rtd = capture->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct audio *audio = snd_soc_dai_get_drvdata(dai);

	regmap_update_bits(audio->audin_map, AUDIN_FIFO_INT,
			   AUDIN_FIFO_INT_FIFO0_ADDR,
			   1);

	/* Clear must also be cleared */
	regmap_update_bits(audio->audin_map, AUDIN_FIFO_INT,
			   AUDIN_FIFO_INT_FIFO0_ADDR,
			   0);
	snd_pcm_period_elapsed(capture);
	return IRQ_HANDLED;
}
#if 0
static irqreturn_t audin_fifo_isr(int irq, void *dev_id)
{
	struct snd_pcm_substream *capture = dev_id;

	snd_pcm_period_elapsed(capture);
//	printk("audin_fifo_isr\n");
	return IRQ_HANDLED;
}
#endif
int audin_fifo_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct audio_fifo *fifo = dai->capture_dma_data;
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
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

	ret = request_irq(fifo->irq, audin_fifo_isr, 0, fifo->irq_name,
			  substream);
//	printk("audin_fifo_startup: devname=%s, irq_name=%s\n", dev_name(dai->dev), 
//		fifo->irq_name);

	/* Disable FIFO0 address trigger interrupt until params ready */
	regmap_update_bits(audio->audin_map, AUDIN_INT_CTRL,
			   AUDIN_INT_CTRL_FIFO0_ADDR, 0);

	/* Clear any pending interrupt */
	regmap_update_bits(audio->audin_map, AUDIN_FIFO_INT,
			   AUDIN_FIFO_INT_FIFO0_ADDR,
			   1);

	/* Clear must also be cleared */
	regmap_update_bits(audio->audin_map, AUDIN_FIFO_INT,
			   AUDIN_FIFO_INT_FIFO0_ADDR,
			   0);

	if (ret)
		clk_disable_unprepare(fifo->pclk);

	return ret;
}

void audin_fifo_shutdown(struct snd_pcm_substream *substream,
		         struct snd_soc_dai *dai)
{
	struct audio_fifo *fifo = dai->capture_dma_data;

	free_irq(fifo->irq, substream);
	clk_disable_unprepare(fifo->pclk);
}
int audin_fifo_dai_probe(struct snd_soc_dai *dai)
{
	struct audio_fifo *fifo;

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

const struct snd_soc_dai_ops audin_fifo_i2s_dai_ops = {
	.hw_params	= audin_fifo_i2s_hw_params,
	.prepare	= audin_fifo_i2s_prepare,
	.trigger	= audin_fifo_i2s_trigger,
	.hw_free	= audin_fifo_hw_free,
	.startup	= audin_fifo_startup,
	.shutdown	= audin_fifo_shutdown,
};

