// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * meson-gx-cs4245 - meson-gx ASoC driver for boards with CS4245 codec.
 *
 * Copyright (C) 2012 Atmel
 *
 * Author: Bo Shen <voice.shen@atmel.com>
 * Author: rezzonics <rezzonics@gmail.com>
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <sound/soc.h>

#include "../codecs/cs4245.h"
//#include "atmel_ssc_dai.h"

static const struct snd_soc_dapm_widget meson-gx_cs4245_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Mic", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
};

static int meson-gx_cs4245_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_dai_set_pll(codec_dai, CS4245_FLL_MCLK, CS4245_FLL_MCLK,
		32768, params_rate(params) * 256);
	if (ret < 0) {
		pr_err("%s - failed to set cs4245 codec PLL.", __func__);
		return ret;
	}

	/*
	 * As here cs4245 use FLL output as its system clock
	 * so calling set_sysclk won't care freq parameter
	 * then we pass 0
	 */
	ret = snd_soc_dai_set_sysclk(codec_dai, CS4245_CLK_FLL,
			0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		pr_err("%s -failed to set cs4245 SYSCLK\n", __func__);
		return ret;
	}

	return 0;
}

static const struct snd_soc_ops meson-gx_cs4245_ops = {
	.hw_params = meson-gx_cs4245_hw_params,
};

static struct snd_soc_dai_link meson-gx_cs4245_dailink = {
	.name = "CS4245",
	.stream_name = "CS8904 PCM",
	.codec_dai_name = "cs4245-hifi",
	.dai_fmt = SND_SOC_DAIFMT_I2S
		| SND_SOC_DAIFMT_NB_NF
		| SND_SOC_DAIFMT_CBM_CFM,
	.ops = &meson-gx_cs4245_ops,
};

static struct snd_soc_card meson-gx_cs4245_card = {
	.name = "meson-gx_cs4245",
	.owner = THIS_MODULE,
	.dai_link = &meson-gx_cs4245_dailink,
	.num_links = 1,
	.dapm_widgets = meson-gx_cs4245_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(meson-gx_cs4245_dapm_widgets),
	.fully_routed = true,
};

static int meson-gx_cs4245_dt_init(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *codec_np, *cpu_np;
	struct snd_soc_card *card = &meson-gx_cs4245_card;
	struct snd_soc_dai_link *dailink = &meson-gx_cs4245_dailink;
	int ret;

	if (!np) {
		dev_err(&pdev->dev, "only device tree supported\n");
		return -EINVAL;
	}

	ret = snd_soc_of_parse_card_name(card, "model");
	if (ret) {
		dev_err(&pdev->dev, "failed to parse card name\n");
		return ret;
	}

	ret = snd_soc_of_parse_audio_routing(card, "audio-routing");
	if (ret) {
		dev_err(&pdev->dev, "failed to parse audio routing\n");
		return ret;
	}

	cpu_np = of_parse_phandle(np, "cpu", 0);
	if (!cpu_np) {
		dev_err(&pdev->dev, "failed to get dai and pcm info\n");
		ret = -EINVAL;
		return ret;
	}
	dailink->cpu_of_node = cpu_np;
	dailink->platform_of_node = cpu_np;
	of_node_put(cpu_np);

	codec_np = of_parse_phandle(np, "codec", 0);
	if (!codec_np) {
		dev_err(&pdev->dev, "failed to get codec info\n");
		ret = -EINVAL;
		return ret;
	}
	dailink->codec_of_node = codec_np;
	of_node_put(codec_np);

	return 0;
}

static int meson-gx_cs4245_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &meson-gx_cs4245_card;
	struct snd_soc_dai_link *dailink = &meson-gx_cs4245_dailink;
	int id, ret;

	card->dev = &pdev->dev;
	ret = meson-gx_cs4245_dt_init(pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to init dt info\n");
		return ret;
	}

	id = of_alias_get_id((struct device_node *)dailink->cpu_of_node, "aiu_i2s");
//	ret = atmel_ssc_set_audio(id);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to set I2S %d for audio\n", id);
		return ret;
	}

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed\n");
		goto err_set_audio;
	}

	return 0;

err_set_audio:
//	atmel_ssc_put_audio(id);
	return ret;
}

static int meson-gx_cs4245_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct snd_soc_dai_link *dailink = &meson-gx_cs4245_dailink;
	int id;

	id = of_alias_get_id((struct device_node *)dailink->cpu_of_node, "aiu_i2s");

	snd_soc_unregister_card(card);
//	atmel_ssc_put_audio(id);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id meson-gx_cs4245_dt_ids[] = {
	{ .compatible = "amlogic,asoc-cs4245", },
	{ }
};
MODULE_DEVICE_TABLE(of, meson-gx_cs4245_dt_ids);
#endif

static struct platform_driver meson-gx_cs4245_driver = {
	.driver = {
		.name = "meson-gx-cs4245-audio",
		.of_match_table = of_match_ptr(meson-gx_cs4245_dt_ids),
		.pm		= &snd_soc_pm_ops,
	},
	.probe = meson-gx_cs4245_probe,
	.remove = meson-gx_cs4245_remove,
};

module_platform_driver(meson-gx_cs4245_driver);

/* Module information */
MODULE_AUTHOR("Rezzonics <rezzonics@gmail.com>");
MODULE_DESCRIPTION("ALSA SoC machine driver for Meson-GX with CS4245 codec");
MODULE_LICENSE("GPL");
