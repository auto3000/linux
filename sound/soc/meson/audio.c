/*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (c) 2020 Rezzonics
* Author: Rezzonics <rezzonics@gmail.com>
*/
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <dt-bindings/sound/meson-audio.h>
#include "audio.h"
#include "aiu-fifo.h"
#include "audin-fifo.h"

#define AIU_FIFO_I2S_BLOCK		256
#define AUDIN_FIFO_I2S_BLOCK		16384

static struct snd_pcm_hardware aiu_fifo_i2s_pcm = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_PAUSE),
	.formats = AUDIO_FORMATS,
	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 8,
	.period_bytes_min = AIU_FIFO_I2S_BLOCK,
	.period_bytes_max = AIU_FIFO_I2S_BLOCK * 256,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 1024 * 1024,
};

static struct snd_pcm_hardware audin_fifo_i2s_pcm = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER |
		 SNDRV_PCM_INFO_PAUSE),
	.formats = AUDIO_FORMATS,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 8,
	.period_bytes_min = AUDIN_FIFO_I2S_BLOCK,
	.period_bytes_max = AUDIN_FIFO_I2S_BLOCK * 256,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 8192 * 8192,
	.fifo_size = 0,
};

static struct snd_soc_dai *audio_fifo_dai(struct snd_pcm_substream *ss)
{
	struct snd_soc_pcm_runtime *rtd = ss->private_data;

	return rtd->cpu_dai;
}

snd_pcm_uframes_t audio_fifo_pointer(struct snd_soc_component *component,
				     struct snd_pcm_substream *substream)
{
	struct snd_soc_dai *dai = audio_fifo_dai(substream);
	struct audio_fifo *fifo;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int addr, buffer_size;
	snd_pcm_uframes_t frames = 0;
	
	switch(substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		fifo = dai->playback_dma_data;
		regmap_read(audio->aiu_map, 
			    fifo->mem_offset + AIU_MEM_I2S_RD_OFF,
		    	    &addr);
		frames = bytes_to_frames(runtime, addr - (unsigned int)runtime->dma_addr);
//		printk("audio_fifo_pointer: playback frames=%ld, addr=%x\n", frames, addr);
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		fifo = dai->capture_dma_data;
		regmap_write(audio->audin_map, 
			     fifo->mem_offset + AUDIN_FIFO_PTR_OFF,
		    	     1);
		regmap_read(audio->audin_map, 
			    fifo->mem_offset + AUDIN_FIFO_PTR_OFF,
		    	    &addr);
		if (runtime->format == SNDRV_PCM_FORMAT_S16_LE) {
			buffer_size = addr - (unsigned int)runtime->dma_addr;
			if (buffer_size == runtime->dma_bytes) 
				buffer_size = runtime->dma_bytes - 8;
			frames = bytes_to_frames(runtime, buffer_size);
//			printk("16bits: frames=%ld, format=%d addr-runtime->dma_addr=%d\n",
//				frames, runtime->format, addr - (unsigned int)runtime->dma_addr);
		}
		else {
			buffer_size = addr - (unsigned int)runtime->dma_addr;
			/* For a period size=48000, buffer_size can be 384 or 376 */
			if (buffer_size <= 384) 
				buffer_size = runtime->dma_bytes - buffer_size - 8;
			frames = bytes_to_frames(runtime, buffer_size);
//			printk("32bits: frames=%ld, format=%d addr-runtime->dma_addr=%d\n",
//				frames, runtime->format, addr - (unsigned int)runtime->dma_addr);
		}
		break;
	}
	return frames;
}

int audio_fifo_pcm_new(struct snd_soc_pcm_runtime *rtd, unsigned int type)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm_substream *ss = rtd->pcm->streams[type].substream;
	size_t size; 

	switch(type) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		size = aiu_fifo_i2s_pcm.buffer_bytes_max;
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		size = audin_fifo_i2s_pcm.buffer_bytes_max;
		break;
	default:
		size = aiu_fifo_i2s_pcm.buffer_bytes_max;
		break;
	}
	if (ss) {
		snd_pcm_lib_preallocate_pages(ss, SNDRV_DMA_TYPE_DEV, card->dev,
					      size, size);
	}
	else {
		printk("snd_pcm_lib_preallocate_pages error\n");
		return -1;
	}
	return 0;
}

int aiu_fifo_pcm_new(struct snd_soc_pcm_runtime *rtd,
		     struct snd_soc_dai *dai)
{
	return audio_fifo_pcm_new(rtd, SNDRV_PCM_STREAM_PLAYBACK);
}

int audin_fifo_pcm_new(struct snd_soc_pcm_runtime *rtd,
		       struct snd_soc_dai *dai)
{
	return audio_fifo_pcm_new(rtd, SNDRV_PCM_STREAM_CAPTURE);
}

int audio_fifo_dai_probe(struct snd_soc_dai *dai)
{
	struct audio_fifo *fifo;


	if (dai->capture_widget) {
		fifo = kzalloc(sizeof(*fifo), GFP_KERNEL);
		if (!fifo)
			return -ENOMEM;
		dai->capture_dma_data = fifo;
	}
	else if (dai->playback_widget) {
		fifo = kzalloc(sizeof(*fifo), GFP_KERNEL);
		if (!fifo)
			return -ENOMEM;
		dai->playback_dma_data = fifo;
	}
	return 0;
}

int audio_fifo_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_dai_get_drvdata(dai);
	struct audio_fifo *fifo;
	int ret;

	ret = audio_fifo_dai_probe(dai);
	if (ret)
		return ret;

	if (dai->capture_widget) {
		fifo = dai->capture_dma_data;
		fifo->mem_offset = AUDIN_FIFO0_START;
		fifo->fifo_block = AUDIN_FIFO_I2S_BLOCK;
		fifo->pclk = audio->aiu.clks[PCLK].clk;
		fifo->irq = audio->audin.irq;
		fifo->irq_name = audio->audin.irq_name;
		fifo->pcm = &audin_fifo_i2s_pcm;
	}
	else if (dai->playback_widget) {
		fifo = dai->playback_dma_data;
		fifo->mem_offset = AIU_MEM_I2S_START;
		fifo->fifo_block = AIU_FIFO_I2S_BLOCK;
		fifo->pclk = audio->aiu.clks[PCLK].clk;
		fifo->irq = audio->aiu.irq;
		fifo->irq_name = audio->aiu.irq_name;
		fifo->pcm = &aiu_fifo_i2s_pcm;
	}
	return 0;
}

int audio_fifo_dai_remove(struct snd_soc_dai *dai)
{
	if (dai->capture_widget)
		kfree(dai->capture_dma_data);
	else if (dai->playback_widget)
		kfree(dai->playback_dma_data);
	return 0;
}

#if 0
/* TODO: Remove SPDIF -------------------------------------  */

static const char * const aiu_spdif_encode_sel_texts[] = {
	"SPDIF", "I2S",
};

static SOC_ENUM_SINGLE_DECL(aiu_spdif_encode_sel_enum, AIU_I2S_MISC,
			    AIU_I2S_MISC_958_SRC_SHIFT,
			    aiu_spdif_encode_sel_texts);

static const struct snd_kcontrol_new aiu_spdif_encode_mux =
	SOC_DAPM_ENUM("SPDIF Buffer Src", aiu_spdif_encode_sel_enum);

static const struct snd_soc_dapm_widget aiu_cpu_dapm_widgets[] = {
	SND_SOC_DAPM_MUX("SPDIF SRC", SND_SOC_NOPM, 0, 0,
			 &aiu_spdif_encode_mux),
};
#endif
/* ----------------------------------------------------------- */
static const struct snd_soc_dapm_route audio_cpu_dapm_routes[] = {
	{ "I2S Encoder Playback", NULL, "I2S FIFO Playback" },
#if 0
	{ "SPDIF SRC", "SPDIF", "SPDIF FIFO Playback" },
	{ "SPDIF SRC", "I2S", "I2S FIFO Playback" },
	{ "SPDIF Encoder Playback", NULL, "SPDIF SRC" },
	{ "SPDIF FIFO Capture", NULL,  "SPDIF Decoder Capture"},
#endif
	{ "I2S FIFO Capture", NULL,  "I2S Decoder Capture"},
};

int audio_of_xlate_dai_name(struct snd_soc_component *component,
			    struct of_phandle_args *args,
			    const char **dai_name,
			    unsigned int component_id)
{
	struct snd_soc_dai *dai;
	int id;

	if (args->args_count != 2)
		return -EINVAL;

	if (args->args[0] != component_id)
		return -EINVAL;

	id = args->args[1];

	if (id < 0 || id >= component->num_dai)
		return -EINVAL;

	for_each_component_dais(component, dai) {
		printk("audio_of_xlate_dai_name: id=%d name=%s\n", id, dai->driver->name);
		if (id == 0)
			break;
		id--;
	}

	*dai_name = dai->driver->name;

	return 0;
}

static int audio_cpu_of_xlate_dai_name(struct snd_soc_component *component,
				       struct of_phandle_args *args,
				       const char **dai_name)
{
	return audio_of_xlate_dai_name(component, args, dai_name, AUDIO_CPU);
}

static int audio_cpu_component_probe(struct snd_soc_component *component)
{
	struct audio *audio = snd_soc_component_get_drvdata(component);
	int ret;

	clk_prepare_enable(audio->audin.clks[AUDIN].clk);
	clk_prepare_enable(audio->audin.clks[AUDBUF].clk);
	/* Required for the SPDIF Source control operation */
	ret = clk_prepare_enable(audio->aiu.clks[PCLK].clk);
	return ret;
}

static void audio_cpu_component_remove(struct snd_soc_component *component)
{
	struct audio *audio = snd_soc_component_get_drvdata(component);

	clk_disable_unprepare(audio->audin.clks[AUDIN].clk);
	clk_disable_unprepare(audio->audin.clks[AUDBUF].clk);
	clk_disable_unprepare(audio->aiu.clks[PCLK].clk);
}

static const struct snd_soc_component_driver audio_cpu_component = {
	.name			= "AUDIO_CPU",
#if 0
	.dapm_widgets		= audio_cpu_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(audio_cpu_dapm_widgets),
#endif
	.dapm_routes		= audio_cpu_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(audio_cpu_dapm_routes),
	.of_xlate_dai_name	= audio_cpu_of_xlate_dai_name,
	.pointer		= audio_fifo_pointer,
	.probe			= audio_cpu_component_probe,
	.remove			= audio_cpu_component_remove,
};

static struct snd_soc_dai_driver audio_cpu_dai_drv[] = {

	[CPU_I2S_FIFO] = {
		.name = "I2S FIFO ENCODE",
		.playback = {
			.stream_name	= "I2S FIFO Playback",
			.channels_min	= 2,
			.channels_max	= 8,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min	= 5512,
			.rate_max	= 192000,
			.formats	= AUDIO_FORMATS,
		},
		.ops		= &aiu_fifo_i2s_dai_ops,
		.pcm_new	= aiu_fifo_pcm_new,
		.probe		= audio_fifo_i2s_dai_probe,
		.remove		= audio_fifo_dai_remove,
	},
#if 0
	[CPU_SPDIF_FIFO] = {
		.name = "SPDIF FIFO",
		.playback = {
			.stream_name	= "SPDIF FIFO Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min	= 5512,
			.rate_max	= 192000,
			.formats	= AUDIO_FORMATS,
		},
		.ops		= &aiu_fifo_spdif_dai_ops,
		.pcm_new	= aiu_fifo_pcm_new,
		.probe		= aiu_fifo_spdif_dai_probe,
		.remove		= aiu_fifo_dai_remove,
	},
#endif
	[CPU_I2S_ENCODER] = {
		.name = "I2S Encoder",
		.playback = {
			.stream_name = "I2S Encoder Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = AUDIO_FORMATS,
		},
		.ops = &aiu_encoder_i2s_dai_ops,
	},
#if 0
	[CPU_SPDIF_ENCODER] = {
		.name = "SPDIF Encoder",
		.playback = {
			.stream_name = "SPDIF Encoder Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = (SNDRV_PCM_RATE_32000  |
				  SNDRV_PCM_RATE_44100  |
				  SNDRV_PCM_RATE_48000  |
				  SNDRV_PCM_RATE_88200  |
				  SNDRV_PCM_RATE_96000  |
				  SNDRV_PCM_RATE_176400 |
				  SNDRV_PCM_RATE_192000),
			.formats = AUDIO_FORMATS,
		},
		.ops = &aiu_encoder_spdif_dai_ops,
	},
#endif
	[CPU_I2S_FIFO_DECODE] = {
		.name = "I2S FIFO DECODE",
		.capture = {
			.stream_name	= "I2S FIFO Capture",
			.channels_min	= 2,
			.channels_max	= 8,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min	= 5512,
			.rate_max	= 192000,
			.formats	= AUDIO_FORMATS,
		},
		.ops		= &audin_fifo_i2s_dai_ops,
		.pcm_new	= audin_fifo_pcm_new,
		.probe		= audio_fifo_i2s_dai_probe,
		.remove		= audio_fifo_dai_remove,
	},
#if 0
	[CPU_SPDIF_FIFO_DECODE] = {
		.name = "SPDIF FIFO DECODE",
		.capture = {
			.stream_name	= "SPDIF FIFO Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min	= 5512,
			.rate_max	= 192000,
			.formats	= AUDIO_FORMATS,
		},
		.ops		= &audin_fifo_spdif_dai_ops,
		.pcm_new	= audin_fifo_pcm_new,
		.probe		= audin_fifo_spdif_dai_probe,
		.remove		= audin_fifo_dai_remove,
	},
#endif
	[CPU_I2S_DECODER] = {
		.name = "I2S Decoder",
		.capture = {
			.stream_name = "I2S Decoder Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = AUDIO_FORMATS,
		},
		.ops = &audin_decoder_i2s_dai_ops,
	},
#if 0
	[CPU_SPDIF_DECODER] = {
		.name = "SPDIF Decoder",
		.capture = {
			.stream_name = "SPDIF Decoder Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = (SNDRV_PCM_RATE_32000  |
				  SNDRV_PCM_RATE_44100  |
				  SNDRV_PCM_RATE_48000  |
				  SNDRV_PCM_RATE_88200  |
				  SNDRV_PCM_RATE_96000  |
				  SNDRV_PCM_RATE_176400 |
				  SNDRV_PCM_RATE_192000),
			.formats = AUDIO_FORMATS,
		},
		.ops = &audin_decoder_spdif_dai_ops,
	}
#endif
};

static const struct regmap_config aiu_regmap_cfg = {
	.name 		= "aiu",
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= 0x2ac,
};

static const struct regmap_config audin_regmap_cfg = {
	.name 		= "audin",
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= 0x304,
};

static int audio_clk_bulk_get(struct device *dev,
			    const char * const *ids,
			    unsigned int num,
			    struct audio_interface *interface)
{
	struct clk_bulk_data *clks;
	int i, ret;

	clks = devm_kcalloc(dev, num, sizeof(*clks), GFP_KERNEL);
	if (!clks)
		return -ENOMEM;

	for (i = 0; i < num; i++)
		clks[i].id = ids[i];

	ret = devm_clk_bulk_get(dev, num, clks);
	if (ret < 0)
		return ret;

	interface->clks = clks;
	interface->clk_num = num;
	return 0;
}

static const char * const aiu_ids[] = {
	[PCLK]		= "i2s_pclk",
	[AOCLK]		= "i2s_aoclk",
	[MCLK]		= "i2s_mclk",
	[MIXER]		= "i2s_mixer",
};

static const char * const audin_ids[] = {
	[AUDBUF] 	= "audin_bufclk",
	[AUDIN]		= "audin_clk",
	[ADC] 		= "adc_clk",
};

#if 0
const char * const aiu_spdif_ids[] = {
	[PCLK]	= "spdif_pclk",
	[AOCLK]	= "spdif_aoclk",
	[MCLK]	= "spdif_mclk_sel"
};
#endif

static int audio_clk_get(struct device *dev)
{
	struct audio *audio = dev_get_drvdata(dev);
	int ret;

	audio->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(audio->pclk)) {
		if (PTR_ERR(audio->pclk) != -EPROBE_DEFER)
			dev_err(dev, "Can't get the audio pclk\n");
		return PTR_ERR(audio->pclk);
	}

	ret = audio_clk_bulk_get(dev, aiu_ids, ARRAY_SIZE(aiu_ids),
			       &audio->aiu);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Can't get the audio i2s clocks\n");
		return ret;
	}

	ret = audio_clk_bulk_get(dev, audin_ids, ARRAY_SIZE(audin_ids),
			       &audio->audin);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Can't get the audio i2s clocks\n");
		return ret;
	}

	ret = clk_prepare_enable(audio->pclk);
	if (ret) {
		dev_err(dev, "peripheral clock enable failed\n");
		return ret;
	}
	ret = devm_add_action_or_reset(dev,
				       (void(*)(void *))clk_disable_unprepare,
				       audio->pclk);
	if (ret)
		dev_err(dev, "failed to add reset action on pclk");


	ret = clk_prepare_enable(audio->audin.clks[AUDBUF].clk);
	if (ret) {
		dev_err(dev, "Audio Buffer clock enable failed\n");
		return ret;
	}
	ret = devm_add_action_or_reset(dev,
				       (void(*)(void *))clk_disable_unprepare,
				       audio->audin.clks[AUDBUF].clk);
	if (ret)
		dev_err(dev, "failed to add reset action on Audio Buffer clock");


	ret = clk_prepare_enable(audio->audin.clks[AUDIN].clk);
	if (ret) {
		dev_err(dev, "Audin clock enable failed\n");
		return ret;
	}
	ret = devm_add_action_or_reset(dev,
				       (void(*)(void *))clk_disable_unprepare,
				       audio->audin.clks[AUDIN].clk);
	if (ret)
		dev_err(dev, "failed to add reset action on Audin clock");


	ret = clk_prepare_enable(audio->audin.clks[ADC].clk);
	if (ret) {
		dev_err(dev, "ADC clock enable failed\n");
		return ret;
	}
	ret = devm_add_action_or_reset(dev,
				       (void(*)(void *))clk_disable_unprepare,
				       audio->audin.clks[ADC].clk);
	if (ret)
		dev_err(dev, "failed to add reset action on ADC clock");

	return ret;
}

static int audio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct audio *audio;
	struct reset_control *rstc;
	int ret;

	audio = devm_kzalloc(dev, sizeof(*audio), GFP_KERNEL);
	if (!audio)
		return -ENOMEM;

	rstc = reset_control_get_exclusive(dev, "aiu");
	if (IS_ERR(rstc))
		return PTR_ERR(rstc);
	ret = reset_control_reset(rstc);
	reset_control_put(rstc);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to reset AIU device\n");
		return ret;
	}
	rstc = reset_control_get_exclusive(dev, "audin");
	if (IS_ERR(rstc))
		return PTR_ERR(rstc);
	ret = reset_control_reset(rstc);
	reset_control_put(rstc);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to reset AUDIN device\n");
		return ret;
	}

	audio->aiu_regs = devm_platform_ioremap_resource_byname(pdev, "aiu");
	if (IS_ERR(audio->aiu_regs))
		return PTR_ERR(audio->aiu_regs);

	audio->aiu_map = devm_regmap_init_mmio(dev, audio->aiu_regs, &aiu_regmap_cfg);
	if (IS_ERR(audio->aiu_map)) {
		dev_err(dev, "failed to init aiu regmap: %ld\n",
			PTR_ERR(audio->aiu_map));
		return PTR_ERR(audio->aiu_map);
	}

	audio->audin_regs = devm_platform_ioremap_resource_byname(pdev, "audin");
	if (IS_ERR(audio->audin_regs))
		return PTR_ERR(audio->audin_regs);

	audio->audin_map = devm_regmap_init_mmio(dev, audio->audin_regs, &audin_regmap_cfg);
	if (IS_ERR(audio->audin_map)) {
		dev_err(dev, "failed to init audin regmap: %ld\n",
			PTR_ERR(audio->audin_map));
		return PTR_ERR(audio->audin_map);
	}

	audio->aiu.irq = platform_get_irq_byname(pdev, "i2s");
	if (audio->aiu.irq < 0) {
		return audio->aiu.irq;
	} 
	else {
		audio->aiu.irq_name = devm_kasprintf(dev, GFP_KERNEL, "%s_aiu", dev_name(dev));
		if (!audio->aiu.irq_name) {
			ret = -ENOMEM;
		}
	}
#if 0
	audio->spdif.irq = platform_get_irq_byname(pdev, "spdif");
	if (audio->spdif.irq < 0) {
		return audio->spdif.irq;
	}

	audio->audin.irq = of_irq_get(dev->of_node, 0);
	if (audio->audin.irq <= 0) {
		dev_err(dev, "failed to get irq: %d\n", audio->audin.irq);
		return audio->audin.irq;
	}
#endif

	audio->audin.irq = platform_get_irq_byname(pdev, "audin");
	if (audio->audin.irq < 0) {
		return audio->audin.irq;
	}
	else {
		audio->audin.irq_name = devm_kasprintf(dev, GFP_KERNEL, "%s_audin", dev_name(dev));
		if (!audio->audin.irq_name) {
			ret = -ENOMEM;
		}
	}

	platform_set_drvdata(pdev, audio);
	ret = audio_clk_get(dev);
	if (ret)
		return ret;

	/* Register the audio cpu component */
	ret = snd_soc_register_component(dev, &audio_cpu_component,
					 audio_cpu_dai_drv,
					 ARRAY_SIZE(audio_cpu_dai_drv));
	if (ret) {
 		dev_err(dev, "Failed to register cpu audio component\n");
		return ret;
	}
	/* Register the audio codec control component */
	ret = audio_codec_ctrl_register_component(dev);
	if (ret) {
		dev_err(dev, "Failed to register codec control component\n");
		goto err;
	}

	return 0;
err:
	snd_soc_unregister_component(dev);

	return ret;
}

static int audio_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static const struct of_device_id audio_of_match[] = {
	{ .compatible = "amlogic,audio-gxbb", },
	{ .compatible = "amlogic,audio-gxl", },
	{}
};
MODULE_DEVICE_TABLE(of, audio_of_match);

static struct platform_driver audio_pdrv = {
	.probe = audio_probe,
	.remove = audio_remove,
	.driver = {
		.name = "meson-audio",
		.of_match_table = audio_of_match,
	},
};
module_platform_driver(audio_pdrv);

MODULE_DESCRIPTION("Meson AUDIO Driver");
MODULE_AUTHOR("Rezzonics <rezzonics@gmail.com>");
MODULE_LICENSE("GPL v2");
