/*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (c) 2020 BayLibre, SAS.
*		2020 Rezzonics
* Author: Jerome Brunet <jbrunet@baylibre.com>
*	  Rezzonics <rezzonics@gmail.com>
*/
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "audio.h"

static void audin_decoder_i2s_hold(struct snd_soc_component *component,
				   bool hold)
{
	struct audio *audio = snd_soc_component_get_drvdata(component);
//	unsigned int debug_val;

	regmap_update_bits(audio->audin_map, AUDIN_I2SIN_CTRL,
			   AUDIN_I2SIN_CTRL_I2SIN_EN,
			   hold ? 0 :AUDIN_I2SIN_CTRL_I2SIN_EN);
	regmap_update_bits(audio->aiu_map, AIU_I2S_MISC,
			   AIU_I2S_MISC_HOLD_EN,
			   hold ? AIU_I2S_MISC_HOLD_EN : 0);
/*
	regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &debug_val);
	printk("audin_decoder_i2s_hold: AUDIN_I2SIN_CTRL=%x\n",
		debug_val);
*/
}

static int audin_decoder_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		audin_decoder_i2s_hold(component, false);
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		audin_decoder_i2s_hold(component, true);
		return 0;

	default:
		return -EINVAL;
	}
}

static int audin_decoder_i2s_setup_desc(struct snd_soc_component *component,
				        struct snd_pcm_hw_params *params)
{
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int desc_aiu = AIU_I2S_SOURCE_DESC_MODE_SPLIT;
	unsigned int desc_adc = AIU_MIX_ADCCFG_LRCLK_INVERT | AIU_MIX_ADCCFG_ADC_SEL;
	unsigned int desc = 0;
	unsigned int val = 0;
	unsigned int ch; 

	/* Reset required to update the pipeline */
	regmap_write(audio->aiu_map, AIU_RST_SOFT, AIU_RST_SOFT_I2S_FAST);
	regmap_read(audio->aiu_map, AIU_I2S_SYNC, &val);

	regmap_update_bits(audio->aiu_map, AIU_I2S_SOURCE_DESC,
			   AIU_I2S_SOURCE_DESC_MODE_SPLIT ,
			   desc_aiu);

	desc_adc |= FIELD_PREP(AIU_MIX_ADCCFG_LRCLK_SKEW, 1);

	/* AUDIN_I2SIN_CTRL_I2SIN_SIZE: 0=16-bit, 1=18-bits, 2=20-bits, 3=24-bits */
	switch (params_physical_width(params)) {
	case 16: 
#ifndef DEBUG_AUDIN
		desc |= FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_SIZE, 0);
#endif
		desc_aiu |= FIELD_PREP(AIU_I2S_SOURCE_DESC_MSB_POS, 2) |
			    FIELD_PREP(AIU_I2S_SOURCE_DESC_SHIFT_BITS, 7);
		desc_adc |= FIELD_PREP(AIU_MIX_ADCCFG_ADC_SIZE, 0);
		break;
	case 24:
	case 32:
#ifndef DEBUG_AUDIN
		desc |= FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_SIZE, 3);
#endif
		desc_aiu |= FIELD_PREP(AIU_I2S_SOURCE_DESC_SHIFT_BITS, 7) |
			    AIU_I2S_SOURCE_DESC_MODE_24BIT |
			    AIU_I2S_SOURCE_DESC_MODE_32BIT;
		desc_adc |= FIELD_PREP(AIU_MIX_ADCCFG_ADC_SIZE, 3);
		break;
	default:
		return -EINVAL;
	}
	
	switch (params_channels(params)) {
	case 2:
		ch = 1;
		break;
	case 8:
		ch = 0xf;
		desc_aiu |= AIU_I2S_SOURCE_DESC_MODE_8CH;
		break;
	default:
		return -EINVAL;
	}
#ifndef DEBUG_AUDIN
	desc |= FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN, ch);
#endif
	regmap_update_bits(audio->aiu_map, AIU_MIX_ADCCFG,
			   AIU_MIX_ADCCFG_LRCLK_INVERT |
			   AIU_MIX_ADCCFG_LRCLK_SKEW |
			   AIU_MIX_ADCCFG_ADC_SIZE |
			   AIU_MIX_ADCCFG_ADC_SEL ,
			   desc_adc);

	regmap_update_bits(audio->aiu_map, AIU_I2S_SOURCE_DESC,
			   AIU_I2S_SOURCE_DESC_MODE_8CH |
			   AIU_I2S_SOURCE_DESC_MSB_INV |
			   AIU_I2S_SOURCE_DESC_MSB_POS |
			   AIU_I2S_SOURCE_DESC_MODE_24BIT |
			   AIU_I2S_SOURCE_DESC_SHIFT_BITS |
			   AIU_I2S_SOURCE_DESC_MODE_32BIT |
			   AIU_I2S_SOURCE_DESC_MODE_SPLIT ,
			   desc_aiu);

	regmap_update_bits(audio->aiu_map, AIU_HDMI_CLK_DATA_CTRL,
			  AIU_HDMI_CLK_DATA_CTRL_CLK_SEL |
			  AIU_HDMI_CLK_DATA_CTRL_DATA_SEL,
			  FIELD_PREP(AIU_HDMI_CLK_DATA_CTRL_CLK_SEL, 2) |
			  FIELD_PREP(AIU_HDMI_CLK_DATA_CTRL_DATA_SEL, 2));

	/* Set AUDIN_I2SIN_CTRL register with I2SIN_EN = 0 */

	regmap_update_bits(audio->audin_map, AUDIN_I2SIN_CTRL,
#ifndef DEBUG_AUDIN
			   AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN |
			   AUDIN_I2SIN_CTRL_I2SIN_SIZE |
#endif
			   AUDIN_I2SIN_CTRL_I2SIN_EN, 
			   desc);

/*
	regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &val);
	regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &desc_aiu);
	printk("audin_decoder_i2s_setup_desc: AUDIN_I2SIN_CTRL=%x, AIU_I2S_SOURCE_DESC=%x\n", 
		val, desc_aiu);
*/
	return 0;
}

static int audin_decoder_i2s_set_clocks(struct snd_soc_component *component,
				        struct snd_pcm_hw_params *params)
{
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int srate = params_rate(params);
	unsigned int fs, bs;
	unsigned int val = 0;
//	unsigned int debug_val[2];

	/* Get the oversampling factor */
	fs = DIV_ROUND_CLOSEST(clk_get_rate(audio->aiu.clks[MCLK].clk), srate);

	if (fs % 64)
		return -EINVAL;

	/* 
	 * Send data MSB first, 
	 * payload size 00 => 16 bits, alrclk = aoclk/32
	 * payload size 11 => 24 bits, alrclk = aoclk/64 
	*/
	val |= AIU_I2S_DAC_CFG_MSB_FIRST;
	regmap_update_bits(audio->aiu_map, AIU_I2S_DAC_CFG,
			   AIU_I2S_DAC_CFG_MSB_FIRST,
			   val);

	/* Set bclk to lrlck ratio */
	regmap_update_bits(audio->aiu_map, AIU_CODEC_DAC_LRCLK_CTRL,
			   AIU_CODEC_DAC_LRCLK_CTRL_DIV,
			   FIELD_PREP(AIU_CODEC_DAC_LRCLK_CTRL_DIV,
			   64 - 1));
	regmap_update_bits(audio->aiu_map, AIU_CODEC_ADC_LRCLK_CTRL,
			   AIU_CODEC_ADC_LRCLK_CTRL_DIV,
			   FIELD_PREP(AIU_CODEC_ADC_LRCLK_CTRL_DIV,
			   64 - 1));

	/* Use CLK_MORE for mclk to bclk divider */
	regmap_update_bits(audio->aiu_map, AIU_CLK_CTRL,
			   AIU_CLK_CTRL_I2S_DIV, 0);

	/*
	 * NOTE: this HW is odd.
	 * In most configuration, the i2s divider is 'mclk / blck'.
	 * However, in 16 bits - 8ch mode, this factor needs to be
	 * increased by 50% to get the correct output rate.
	 * No idea why !
	 */
	bs = fs / 64;
	if (params_width(params) == 16 && params_channels(params) == 8) {
		if (bs % 2) {
			dev_err(component->dev,
				"Cannot increase i2s divider by 50%%\n");
			return -EINVAL;
		}
		bs += bs / 2;
	}

	/* Make sure amclk is used for HDMI i2s as well */
	regmap_update_bits(audio->aiu_map, AIU_CLK_CTRL_MORE,
			   AIU_CLK_CTRL_MORE_I2S_DIV |
			   AIU_CLK_CTRL_MORE_ADC_DIV |
			   AIU_CLK_CTRL_MORE_HDMI_AMCLK,
			   FIELD_PREP(AIU_CLK_CTRL_MORE_I2S_DIV, bs-1) |
			   FIELD_PREP(AIU_CLK_CTRL_MORE_ADC_DIV, bs-1) |
			   AIU_CLK_CTRL_MORE_HDMI_AMCLK);
/*
	regmap_read(audio->aiu_map, AIU_CLK_CTRL, &debug_val[0]);
	regmap_read(audio->aiu_map, AIU_CLK_CTRL_MORE, &debug_val[1]);
	printk("audin_decoder_i2s_set_clocks: AIU_CLK_CTRL=%x, AIU_CLK_CTRL_MORE=%x\n", 
		debug_val[0], debug_val[1]);
*/
	return 0;
}

static void audin_decoder_i2s_divider_enable(struct snd_soc_component *component,
					     bool enable)
{
	struct audio *audio = snd_soc_component_get_drvdata(component);
//	unsigned int debug_val;

	regmap_update_bits(audio->aiu_map, AIU_CLK_CTRL,
			   AIU_CLK_CTRL_I2S_DIV_EN,
			   enable ? AIU_CLK_CTRL_I2S_DIV_EN : 0);

	regmap_update_bits(audio->aiu_map, AIU_CLK_CTRL_MORE,
			   AIU_CLK_CTRL_MORE_ADC_EN,
			   enable ? AIU_CLK_CTRL_MORE_ADC_EN : 0);
/*
	regmap_read(audio->aiu_map, AIU_CLK_CTRL, &debug_val);
	printk("audin_decoder_i2s_divider_enable: AIU_CLK_CTRL=%x\n", debug_val);
*/
}

static int audin_decoder_i2s_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	int ret;

	/* Disable the clock while changing the settings */
	audin_decoder_i2s_divider_enable(component, false);

	ret = audin_decoder_i2s_setup_desc(component, params);
	if (ret) {
		dev_err(dai->dev, "setting i2s desc failed\n");
		return ret;
	}

	ret = audin_decoder_i2s_set_clocks(component, params);
	if (ret) {
		dev_err(dai->dev, "setting i2s clocks failed\n");
		return ret;
	}

	audin_decoder_i2s_divider_enable(component, true);

	return 0;
}


static int audin_decoder_i2s_hw_free(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	audin_decoder_i2s_divider_enable(component, false);

	return 0;
}

static int audin_decoder_i2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct audio *audio = dev_get_drvdata(dai->dev);
	unsigned int inv = fmt & SND_SOC_DAIFMT_INV_MASK;
	unsigned int audin_val = 0;
	unsigned int aiu_val = 0;
	unsigned int adc_val = 0;
	unsigned int skew;
//	unsigned int debug_val = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	/* CPU Master / Codec Slave */
	case SND_SOC_DAIFMT_CBS_CFS:
		audin_val |= AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL |
			     AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL |
			     AUDIN_I2SIN_CTRL_I2SIN_DIR;
		adc_val |= AIU_MIX_ADCCFG_ADC_SEL;
		aiu_val |= AIU_CLK_CTRL_CLK_SRC;
//		printk("audin_decoder_i2s_set_fmt: CPU Master / Codec Slave\n");
		break;
	/* CPU Slave / Codec Master */
	case SND_SOC_DAIFMT_CBM_CFM:
//		printk("audin_decoder_i2s_set_fmt: CPU Slave / Codec Master (!!)\n");
		break;
	default:
		return -EINVAL;
	}
#ifndef DEBUG_AUDIN
/*	
	if (inv == SND_SOC_DAIFMT_NB_IF ||
	    inv == SND_SOC_DAIFMT_IB_IF) {
		aiu_val |= AIU_CLK_CTRL_LRCLK_INVERT;
		adc_val |= AIU_MIX_ADCCFG_LRCLK_INVERT;
	}

	if (inv == SND_SOC_DAIFMT_IB_NF ||
	    inv == SND_SOC_DAIFMT_IB_IF) {
		aiu_val |= AIU_CLK_CTRL_AOCLK_INVERT;
		adc_val |= AIU_MIX_ADCCFG_AOCLK_INVERT;
	}
*/
	audin_val |= AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT |
		     AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC;
#endif
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		/* Invert sample clock for i2s */
#ifndef DEBUG_AUDIN
		audin_val ^= AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT;
//		aiu_val |= AIU_CLK_CTRL_LRCLK_INVERT;
#endif
		adc_val ^= AIU_MIX_ADCCFG_LRCLK_INVERT;
		// 0:Left justify, 1:right justified, 2:I2S, 3: DSP
//		desc |= FIELD_PREP(AUDIN_DECODE_FMT_FMT_SELECT, 2);  
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return -EINVAL;
	}
	aiu_val |= AIU_CLK_CTRL_LRCLK_INVERT;
	aiu_val |= AIU_CLK_CTRL_AOCLK_INVERT;
	skew = 1;

#ifndef DEBUG_AUDIN
	audin_val |= FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW, skew);
#endif
	aiu_val |= FIELD_PREP(AIU_CLK_CTRL_LRCLK_SKEW, 0);
	adc_val |= FIELD_PREP(AIU_MIX_ADCCFG_LRCLK_SKEW, skew);
	regmap_update_bits(audio->audin_map, AUDIN_I2SIN_CTRL,
#ifndef DEBUG_AUDIN
			   AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT |
			   AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW |
			   AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC |
#endif
			   AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL |
			   AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL |
			   AUDIN_I2SIN_CTRL_I2SIN_DIR ,
			   audin_val);
	regmap_update_bits(audio->aiu_map, AIU_CLK_CTRL,
			   AIU_CLK_CTRL_LRCLK_INVERT |
			   AIU_CLK_CTRL_AOCLK_INVERT |
			   AIU_CLK_CTRL_LRCLK_SKEW,
			   aiu_val);

	regmap_update_bits(audio->aiu_map, AIU_MIX_ADCCFG,
			   AIU_MIX_ADCCFG_ADC_SEL |
			   AIU_MIX_ADCCFG_LRCLK_INVERT |
			   AIU_MIX_ADCCFG_AOCLK_INVERT |
			   AIU_MIX_ADCCFG_LRCLK_SKEW,
			   adc_val);
/*
	regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &debug_val);
	printk("audin_decoder_i2s_set_fmt: AUDIN_I2SIN_CTRL=%x\n", debug_val);
*/
	return 0;
}

static int audin_decoder_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				        unsigned int freq, int dir)
{
	struct audio *audio = snd_soc_component_get_drvdata(dai->component);
	int ret;

	if (WARN_ON(clk_id != 0))
		return -EINVAL;

	if (dir == SND_SOC_CLOCK_IN)
		return 0;

	ret = clk_set_rate(audio->aiu.clks[MCLK].clk, freq);
	if (ret)
		dev_err(dai->dev, "Failed to set sysclk to %uHz", freq);

	return ret;
}

static const unsigned int hw_channels[] = {2, 8};
static const struct snd_pcm_hw_constraint_list hw_channel_constraints = {
	.list = hw_channels,
	.count = ARRAY_SIZE(hw_channels),
	.mask = 0,
};


static int audin_decoder_i2s_startup(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_component_get_drvdata(dai->component);
	int ret;

	/* Make sure the encoder gets either 1, 2 or 8 channels? */
	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_CHANNELS,
					 &hw_channel_constraints);
	if (ret) {
		dev_err(dai->dev, "adding channels constraints failed\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(audio->audin.clk_num, audio->audin.clks);
	if (ret)
		dev_err(dai->dev, "failed to enable audin i2s clocks\n");

	ret = clk_bulk_prepare_enable(audio->aiu.clk_num, audio->aiu.clks);
	if (ret)
		dev_err(dai->dev, "failed to enable aiu i2s clocks\n");

	return ret;
}

static void audin_decoder_i2s_shutdown(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct audio *audio = snd_soc_component_get_drvdata(dai->component);

	clk_bulk_disable_unprepare(audio->audin.clk_num, audio->audin.clks);
	clk_bulk_disable_unprepare(audio->aiu.clk_num, audio->aiu.clks);
}


const struct snd_soc_dai_ops audin_decoder_i2s_dai_ops = {
	.set_fmt	= audin_decoder_i2s_set_fmt,
	.set_sysclk	= audin_decoder_i2s_set_sysclk,
	.hw_params	= audin_decoder_i2s_hw_params,
	.trigger	= audin_decoder_i2s_trigger,
	.hw_free	= audin_decoder_i2s_hw_free,
	.startup	= audin_decoder_i2s_startup,
	.shutdown	= audin_decoder_i2s_shutdown,
};

