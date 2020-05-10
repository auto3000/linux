// SPDX-License-Identifier: (GPL-2.0 OR MIT)
//
// Copyright (c) 2020 BayLibre, SAS.
// Author: Jerome Brunet <jbrunet@baylibre.com>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "meson-card.h"

#define AUDIN_FORMATS						\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |	\
	 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

struct gx_dai_link_i2s_data {
	unsigned int mclk_fs;
};

/*
 * Base params for the codec to codec links
 * Those will be over-written by the CPU side of the link
 */
static const struct snd_soc_pcm_stream codec_params = {
	// SNDRV_PCM_FMTBIT_S24_LE,
	.formats = AUDIN_FORMATS,
	.rate_min = 5525,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 8,
};

static int gx_card_i2s_be_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct meson_card *priv = snd_soc_card_get_drvdata(rtd->card);
	struct gx_dai_link_i2s_data *be =
		(struct gx_dai_link_i2s_data *)priv->link_data[rtd->num];

	return meson_card_i2s_set_sysclk(substream, params, be->mclk_fs);
}

static const struct snd_soc_ops gx_card_i2s_be_ops = {
	.hw_params = gx_card_i2s_be_hw_params,
};

static int gx_card_parse_i2s(struct snd_soc_card *card,
			     struct device_node *node,
			     int *index)
{
	struct meson_card *priv = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai_link *link = &card->dai_link[*index];
	struct gx_dai_link_i2s_data *be;

	/* Allocate i2s link parameters */
	be = devm_kzalloc(card->dev, sizeof(*be), GFP_KERNEL);
	if (!be)
		return -ENOMEM;
	priv->link_data[*index] = be;

	/* Setup i2s link */
	link->ops = &gx_card_i2s_be_ops;
	link->dai_fmt = meson_card_parse_daifmt(node, link->cpus->of_node);

	of_property_read_u32(node, "mclk-fs", &be->mclk_fs);

	return 0;
}

static int gx_card_cpu_is_playback_fe(struct snd_soc_dai_link_component *c,
				char *match)
{
	if (of_device_is_compatible(c->of_node, "amlogic,aiu-gxbb")) {
		if (strstr(c->dai_name, match))
			return 1;
	}

	printk("gx_card_cpu_is_playback_fe: dai_name \"%s\" does not match \"%s\"", c->dai_name, match);
	return 0;
}

static int gx_card_cpu_is_capture_fe(struct snd_soc_dai_link_component *c,
				char *match)
{
	if (of_device_is_compatible(c->of_node, "amlogic,audin-gxbb")) {
		if (strstr(c->dai_name, match))
			return 1;
	}

	printk("gx_card_cpu_is_capture_fe: dai_name \"%s\" does not match \"%s\"", 
		c->dai_name, match);
	return 0;
}

static int gx_card_add_link(struct snd_soc_card *card, struct device_node *np,
			    int *index)
{
	struct snd_soc_dai_link *dai_link = &card->dai_link[*index];
	struct snd_soc_dai_link_component *cpu;
	int ret;

	cpu = devm_kzalloc(card->dev, sizeof(*cpu), GFP_KERNEL);
	printk("gx_card_add_link: %pr", cpu);
	if (!cpu)
		return -ENOMEM;

	dai_link->cpus = cpu;
	dai_link->num_cpus = 1;

	ret = meson_card_parse_dai(card, np, &dai_link->cpus->of_node,
				   &dai_link->cpus->dai_name);
	if (ret)
		return ret;

	/* axg-frddr */
	if (gx_card_cpu_is_playback_fe(dai_link->cpus, "I2S FIFO"))
		ret = meson_card_set_fe_link(card, dai_link, np, true);
	/* axg-toddr */
	else if (gx_card_cpu_is_capture_fe(dai_link->cpus, "I2S FIFO DECODE"))
		ret = meson_card_set_fe_link(card, dai_link, np, false);
	else ret = meson_card_set_be_link(card, dai_link, np);

	if (ret)
		return ret;
	/* Check if the cpu is the i2s encoder and parse i2s data (tdm-iface) */
	if (gx_card_cpu_is_playback_fe(dai_link->cpus, "I2S Encoder"))
		ret = gx_card_parse_i2s(card, np, index);
	/* Check if the cpu is the i2s decoder and parse i2s data (tdm-iface) */
	else if (gx_card_cpu_is_capture_fe(dai_link->cpus, "I2S Decoder"))
		ret = gx_card_parse_i2s(card, np, index);
	/* Apply codec to codec playback params if necessary (g12a-toacodec) */
	else if (gx_card_cpu_is_playback_fe(dai_link->cpus, "CODEC CTRL")) {
		dai_link->params = &codec_params;
		dai_link->no_pcm = 0; /* link is not a DPCM BE */
	}
	/* Apply codec to codec record params if necessary (g12a-toacodec) */
	else if (gx_card_cpu_is_capture_fe(dai_link->cpus, "CODEC REC")) {
		dai_link->params = &codec_params;
		dai_link->no_pcm = 0; /* link is not a DPCM BE */
	}
	return ret;
}

static const struct meson_card_match_data gx_card_match_data = {
	.add_link = gx_card_add_link,
};

static const struct of_device_id gx_card_of_match[] = {
	{
		.compatible = "amlogic,gx-sound-card",
		.data = &gx_card_match_data,
	}, {}
};
MODULE_DEVICE_TABLE(of, gx_card_of_match);

static struct platform_driver gx_card_pdrv = {
	.probe = meson_card_probe,
	.remove = meson_card_remove,
	.driver = {
		.name = "meson-gx-sound-card",
		.of_match_table = gx_card_of_match,
	},
};
module_platform_driver(gx_card_pdrv);

MODULE_DESCRIPTION("Amlogic GX ALSA machine driver");
MODULE_AUTHOR("Jerome Brunet <jbrunet@baylibre.com>");
MODULE_LICENSE("GPL v2");
