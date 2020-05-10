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

#define AUDIN_SPDIF_MODE_SPDIF_CLKNUM_54U	GENMASK(13, 0)
#define AUDIN_SPDIF_MODE_SPDIF_EN		BIT(31)

#define AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_192K	GENMASK(29, 24)
#define AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_96K	GENMASK(23, 18)
#define AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_48K	GENMASK(17, 12)
#define AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_44K	GENMASK(10, 6)
#define AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_32K	GENMASK(5, 0)

#define AUDIN_FIFO1_CTRL_HOLD0_EN		BIT(19)
#define AUDIN_FIFO1_CTRL_HOLD1_EN		BIT(20)
#define AUDIN_FIFO1_CTRL_HOLD2_EN		BIT(21)
#define AUDIN_FIFO1_CTRL_HOLD0_SEL		GENMASK(23, 22)
#define AUDIN_FIFO1_CTRL_HOLD1_SEL		GENMASK(25, 24)
#define AUDIN_FIFO1_CTRL_HOLD2_SEL		GENMASK(27, 26)

/*
static void audin_decoder_i2s_divider_enable(struct snd_soc_component *component,
					     bool enable)
{
	snd_soc_component_update_bits(component, AIU_CLK_CTRL,
				      AIU_CLK_CTRL_I2S_DIV_EN,
				      enable ? AIU_CLK_CTRL_I2S_DIV_EN : 0);
}
*/
static void audin_decoder_spdif_hold(struct snd_soc_component *component,
				     bool enable)
{
	unsigned int desc = AUDIN_FIFO1_CTRL_HOLD0_EN |
			    AUDIN_FIFO1_CTRL_HOLD1_EN |
			    AUDIN_FIFO1_CTRL_HOLD2_EN |
			    AUDIN_FIFO1_CTRL_HOLD0_SEL |
			    AUDIN_FIFO1_CTRL_HOLD1_SEL |
			    AUDIN_FIFO1_CTRL_HOLD2_SEL ;

	snd_soc_component_update_bits(component, AUDIN_FIFO1_CTRL,
				      AUDIN_FIFO1_CTRL_HOLD0_EN |
				      AUDIN_FIFO1_CTRL_HOLD1_EN |
				      AUDIN_FIFO1_CTRL_HOLD2_EN |
				      AUDIN_FIFO1_CTRL_HOLD0_SEL |
				      AUDIN_FIFO1_CTRL_HOLD1_SEL |
				      AUDIN_FIFO1_CTRL_HOLD2_SEL ,
				      enable ? desc : 0);
}

static int audin_decoder_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		audin_decoder_spdif_hold(component, false);
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		audin_decoder_spdif_hold(component, true);
		return 0;

	default:
		return -EINVAL;
	}
}

static int audin_decoder_spdif_setup_desc(struct snd_soc_component *component,
				        struct snd_pcm_hw_params *params)
{
	struct audin *audin = snd_soc_component_get_drvdata(component);
	unsigned int desc;
	unsigned int rate, period;
	unsigned int clknum_192k, clknum_96k, clknum_48k, clknum_44k, clknum_32k;

	rate = clk_get_rate(audin->pclk);
	period = (rate / 64000 + 1) >> 1;
	clknum_192k = (period / 192 + 1) >> 1;
	clknum_96k  = (period / 96 + 1) >> 1;
	clknum_48k  = (period / 48 + 1) >> 1;
	clknum_44k  = (period / 44 + 1) >> 1;
	clknum_32k  = (period  + (1 << 5)) >> 6;
	desc =  FIELD_PREP(AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_192K, clknum_192k) |
		FIELD_PREP(AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_96K,  clknum_96k)  |
		FIELD_PREP(AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_48K,  clknum_48k)  |
		FIELD_PREP(AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_44K,  clknum_44k)  |
		FIELD_PREP(AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_32K,  clknum_32k);

	snd_soc_component_update_bits(component, AUDIN_SPDIF_FS_CLK_RLTN,
				      AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_192K |
				      AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_96K  |
				      AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_48K  |
				      AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_44K  |
				      AUDIN_SPDIF_FS_CLK_RLTN_CLKNUM_32K,
				      desc);
	/* clocknum 54us */
	desc = (rate / 1000000 + 1) * 27;

	/* Set AUDIN_SPDIF_MODE register with CLKNUM_54U and SPDIF_EN = 0 */
	snd_soc_component_update_bits(component, AUDIN_SPDIF_MODE,
				      AUDIN_SPDIF_MODE_SPDIF_EN |
				      AUDIN_SPDIF_MODE_SPDIF_CLKNUM_54U,
				      desc);
	desc |= AUDIN_SPDIF_MODE_SPDIF_EN;
	/* Set AUDIN_SPDIF_MODE register with CLKNUM_54U and SPDIF_EN = 1 */
	snd_soc_component_update_bits(component, AUDIN_SPDIF_MODE,
				      AUDIN_SPDIF_MODE_SPDIF_EN |
				      AUDIN_SPDIF_MODE_SPDIF_CLKNUM_54U,
				      desc);
	return 0;
}

static int audin_decoder_spdif_hw_params(struct snd_pcm_substream *substream,
				         struct snd_pcm_hw_params *params,
				         struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	int ret;


	ret = audin_decoder_spdif_setup_desc(component, params);
	if (ret) {
		dev_err(dai->dev, "setting spdif desc failed\n");
		return ret;
	}

	return 0;
}

/*
static int audin_decoder_spdif_hw_free(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	audin_decoder_spdif_divider_enable(component, false);

	return 0;
}
*/
/*
static const unsigned int hw_channels[] = {2, 8};
static const struct snd_pcm_hw_constraint_list hw_channel_constraints = {
	.list = hw_channels,
	.count = ARRAY_SIZE(hw_channels),
	.mask = 0,
};
*/
/*
static int audin_decoder_spdif_startup(struct snd_pcm_substream *substream,
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

static void audin_decoder_spdif_shutdown(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct audin *audin = snd_soc_component_get_drvdata(dai->component);

	clk_bulk_disable_unprepare(aiu->spdif.clk_num, aiu->spdif.clks);
}
*/

const struct snd_soc_dai_ops audin_decoder_spdif_dai_ops = {
	.trigger	= audin_decoder_spdif_trigger,
	.hw_params	= audin_decoder_spdif_hw_params,
//	.hw_free	= audin_decoder_spdif_hw_free,
//	.set_fmt	= audin_decoder_spdif_set_fmt,
//	.set_sysclk	= audin_decoder_spdif_set_sysclk,
//	.startup	= audin_decoder_spdif_startup,
//	.shutdown	= audin_decoder_spdif_shutdown,
};

