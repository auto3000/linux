// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 BayLibre, SAS.
// Author: Jerome Brunet <jbrunet@baylibre.com>

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "audin.h"

#define AUDIN_I2SIN_CTRL_I2SIN_DIR		BIT(0)
#define AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL		BIT(1)
#define AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL	BIT(2)
#define AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC		BIT(3)
#define AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW	GENMASK(6, 4)
#define AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT	BIT(7)
#define AUDIN_I2SIN_CTRL_I2SIN_SIZE		GENMASK(9, 8)
#define AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN		GENMASK(13, 10)
#define AUDIN_I2SIN_CTRL_I2SIN_EN		BIT(15)

#define AUDIN_FIFO0_CTRL_HOLD0_EN		BIT(19)
#define AUDIN_FIFO0_CTRL_HOLD1_EN		BIT(20)
#define AUDIN_FIFO0_CTRL_HOLD2_EN		BIT(21)
#define AUDIN_FIFO0_CTRL_HOLD0_SEL		GENMASK(23, 22)
#define AUDIN_FIFO0_CTRL_HOLD1_SEL		GENMASK(25, 24)
#define AUDIN_FIFO0_CTRL_HOLD2_SEL		GENMASK(27, 26)

/*
static void audin_decoder_i2s_divider_enable(struct snd_soc_component *component,
					     bool enable)
{
	snd_soc_component_update_bits(component, AIU_CLK_CTRL,
				      AIU_CLK_CTRL_I2S_DIV_EN,
				      enable ? AIU_CLK_CTRL_I2S_DIV_EN : 0);
}
*/
static void audin_decoder_i2s_hold(struct snd_soc_component *component,
				   bool enable)
{
/*
	unsigned int desc = AUDIN_FIFO0_CTRL_HOLD0_EN |
			    AUDIN_FIFO0_CTRL_HOLD1_EN |
			    AUDIN_FIFO0_CTRL_HOLD2_EN ;

	snd_soc_component_update_bits(component, AUDIN_FIFO0_CTRL,
				      AUDIN_FIFO0_CTRL_HOLD0_EN |
				      AUDIN_FIFO0_CTRL_HOLD1_EN |
				      AUDIN_FIFO0_CTRL_HOLD2_EN |
				      AUDIN_FIFO0_CTRL_HOLD0_SEL |
				      AUDIN_FIFO0_CTRL_HOLD1_SEL |
				      AUDIN_FIFO0_CTRL_HOLD2_SEL ,
				      enable ? desc : 0);
*/
	unsigned int desc = AUDIN_I2SIN_CTRL_I2SIN_EN;

	snd_soc_component_update_bits(component, AUDIN_I2SIN_CTRL,
				      AUDIN_I2SIN_CTRL_I2SIN_EN,
				      enable ? desc : 0);
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
	/* Set as master by default */
	unsigned int desc = AUDIN_I2SIN_CTRL_I2SIN_SIZE |
			    AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT |
			    AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC |
			    AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL |
			    AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL |
			    AUDIN_I2SIN_CTRL_I2SIN_DIR ;
	unsigned int ch;

	desc |= FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW, 1);

	switch (params_channels(params)) {
	case 1:
	case 2:
		ch = 0;
		break;
	case 3:
	case 4:
		ch = 1;
		break;
	case 5:
	case 6:
		ch = 3;
		break;
	case 7:
	case 8:
		ch = 7;
		break;
	default:
		return -EINVAL;
	}

	desc |= FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN, ch);

	/* Set AUDIN_I2SIN_CTRL register with I2SIN_EN = 0 */
	snd_soc_component_update_bits(component, AUDIN_I2SIN_CTRL,
				      AUDIN_I2SIN_CTRL_I2SIN_EN |
				      AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN |
				      AUDIN_I2SIN_CTRL_I2SIN_SIZE |
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT |
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW |
				      AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC |
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL |
				      AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL |
				      AUDIN_I2SIN_CTRL_I2SIN_DIR ,
				      desc);
	desc |= AUDIN_I2SIN_CTRL_I2SIN_EN;
	/* Set AUDIN_I2SIN_CTRL register with I2SIN_EN = 1 */
	snd_soc_component_update_bits(component, AUDIN_I2SIN_CTRL,
				      AUDIN_I2SIN_CTRL_I2SIN_EN |
				      AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN |
				      AUDIN_I2SIN_CTRL_I2SIN_SIZE |
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT |
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW |
				      AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC |
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL |
				      AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL |
				      AUDIN_I2SIN_CTRL_I2SIN_DIR ,
				      desc);
	return 0;
}

static int audin_decoder_i2s_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	int ret;


	ret = audin_decoder_i2s_setup_desc(component, params);
	if (ret) {
		dev_err(dai->dev, "setting i2s desc failed\n");
		return ret;
	}

	return 0;
}

/*
static int audin_decoder_i2s_hw_free(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	audin_decoder_i2s_divider_enable(component, false);

	return 0;
}
*/
static int audin_decoder_i2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	unsigned int inv = fmt & SND_SOC_DAIFMT_INV_MASK;
	unsigned int val = 0;
	unsigned int skew;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	/* CPU Master / Codec Slave */
	case SND_SOC_DAIFMT_CBS_CFS:
		val |= 	AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL |
			AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL |
			AUDIN_I2SIN_CTRL_I2SIN_DIR;
		break;
	/* CPU Slave / Codec Master */
	case SND_SOC_DAIFMT_CBM_CFM:
		break;
	default:
		return -EINVAL;
	}

	
	if (inv == SND_SOC_DAIFMT_NB_IF ||
	    inv == SND_SOC_DAIFMT_IB_IF)
		val |= AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT;

	/*
	if (inv == SND_SOC_DAIFMT_IB_NF ||
	    inv == SND_SOC_DAIFMT_IB_IF)
		val |= AUDIN_I2SIN_CTRL_I2SIN_CLK_INVT;
	*/
	/* Signal skew */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		val ^= AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT;
		skew = 1;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		skew = 0;
		break;
	default:
		return -EINVAL;
	}

	val |= FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW, skew);

	snd_soc_component_update_bits(component, AUDIN_I2SIN_CTRL,
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT |
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW |
				      AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL |
				      AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL |
				      AUDIN_I2SIN_CTRL_I2SIN_DIR ,
				      val);
	return 0;
}

static int audin_decoder_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				      unsigned int freq, int dir)
{
	struct audin *audin = snd_soc_component_get_drvdata(dai->component);
	int ret;

	if (WARN_ON(clk_id != 0))
		return -EINVAL;

	if (dir == SND_SOC_CLOCK_IN)
		return 0;

	ret = clk_set_rate(audin->i2s.clks[MCLK].clk, freq);
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
	struct audin *audin = snd_soc_component_get_drvdata(dai->component);
	int ret;

	ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
					 SNDRV_PCM_HW_PARAM_CHANNELS,
					 &hw_channel_constraints);
	if (ret) {
		dev_err(dai->dev, "adding channels constraints failed\n");
		return ret;
	}

	return ret;
}

static void audin_decoder_i2s_shutdown(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct audin *audin = snd_soc_component_get_drvdata(dai->component);

	clk_bulk_disable_unprepare(audin->i2s.clk_num, audin->i2s.clks);
}


const struct snd_soc_dai_ops audin_decoder_i2s_dai_ops = {
	.trigger	= audin_decoder_i2s_trigger,
	.hw_params	= audin_decoder_i2s_hw_params,
//	.hw_free	= audin_decoder_i2s_hw_free,
	.set_fmt	= audin_decoder_i2s_set_fmt,
	.set_sysclk	= audin_decoder_i2s_set_sysclk,
	.startup	= audin_decoder_i2s_startup,
	.shutdown	= audin_decoder_i2s_shutdown,
};

