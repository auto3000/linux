// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 Rezzonics
// Author: Rezzonics <rezzonics@gmail.com>

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <dt-bindings/sound/meson-audin.h>
#include "audin.h"
#include "audin-fifo.h"

/*
static const struct snd_soc_dapm_widget audin_cpu_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("I2S Decoder Capture",   "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SPDIF Decoder Capture", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S FIFO Capture",     "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SPDIF FIFO Capture",   "Capture", 0, SND_SOC_NOPM, 0, 0),
};
*/
static const struct snd_soc_dapm_route audin_cpu_dapm_routes[] = {
	{ "I2S FIFO Capture", NULL,  "I2S Decoder Capture"},
	{ "SPDIF FIFO Capture", NULL,  "SPDIF Decoder Capture"},
};

int audin_of_xlate_dai_name(struct snd_soc_component *component,
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
		if (id == 0)
			break;
		id--;
	}

	*dai_name = dai->driver->name;

	return 0;
}

static int audin_cpu_of_xlate_dai_name(struct snd_soc_component *component,
				       struct of_phandle_args *args,
				       const char **dai_name)
{
	return audin_of_xlate_dai_name(component, args, dai_name, AUDIN_CPU);
}

static int audin_cpu_component_probe(struct snd_soc_component *component)
{
	struct audin *audin = snd_soc_component_get_drvdata(component);

	return clk_prepare_enable(audin->pclk);
}

static void audin_cpu_component_remove(struct snd_soc_component *component)
{
	struct audin *audin = snd_soc_component_get_drvdata(component);

	clk_disable_unprepare(audin->pclk);
}

static const struct snd_soc_component_driver audin_cpu_component = {
	.name			= "AUDIN_CPU",
//	.dapm_widgets		= audin_cpu_dapm_widgets,
//	.num_dapm_widgets	= ARRAY_SIZE(audin_cpu_dapm_widgets),
	.dapm_routes		= audin_cpu_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(audin_cpu_dapm_routes),
	.of_xlate_dai_name	= audin_cpu_of_xlate_dai_name,
	.pointer		= audin_fifo_pointer,
	.probe			= audin_cpu_component_probe,
	.remove			= audin_cpu_component_remove,
};

static struct snd_soc_dai_driver audin_cpu_dai_drv[] = {
	[CPU_I2S_FIFO_DECODE] = {
		.name = "I2S FIFO DECODE",
		.capture = {
			.stream_name	= "I2S FIFO Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min	= 5512,
			.rate_max	= 192000,
			.formats	= AUDIN_FORMATS,
		},
		.ops		= &audin_fifo_i2s_dai_ops,
		.pcm_new	= audin_fifo_pcm_new,
		.probe		= audin_fifo_i2s_dai_probe,
		.remove		= audin_fifo_dai_remove,
	},
	[CPU_SPDIF_FIFO_DECODE] = {
		.name = "SPDIF FIFO DECODE",
		.capture = {
			.stream_name	= "SPDIF FIFO Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min	= 5512,
			.rate_max	= 192000,
			.formats	= AUDIN_FORMATS,
		},
		.ops		= &audin_fifo_spdif_dai_ops,
		.pcm_new	= audin_fifo_pcm_new,
		.probe		= audin_fifo_spdif_dai_probe,
		.remove		= audin_fifo_dai_remove,
	},
	[CPU_I2S_DECODER] = {
		.name = "I2S Decoder",
		.capture = {
			.stream_name = "I2S Decoder Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = AUDIN_FORMATS,
		},
		.ops = &audin_decoder_i2s_dai_ops,
	},
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
			.formats = AUDIN_FORMATS,
		},
		.ops = &audin_decoder_spdif_dai_ops,
	}
};

static const struct regmap_config audin_regmap_cfg = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= 0x304,
};

static int audin_clk_bulk_get(struct device *dev,
			    const char * const *ids,
			    unsigned int num,
			    struct audin_interface *interface)
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

static const char * const audin_i2s_ids[] = {
	[PCLK]	= "i2s_pclk",
	[AOCLK]	= "i2s_aoclk",
	[MCLK]	= "i2s_mclk",
	[MIXER]	= "i2s_mixer",
	[AUDIN] = "audin_clk",
};


static int audin_clk_get(struct device *dev)
{
	struct audin *audin = dev_get_drvdata(dev);
	int ret;

	audin->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(audin->pclk)) {
		if (PTR_ERR(audin->pclk) != -EPROBE_DEFER)
			dev_err(dev, "Can't get the audin pclk\n");
		return PTR_ERR(audin->pclk);
	}

	ret = audin_clk_bulk_get(dev, audin_i2s_ids, ARRAY_SIZE(audin_i2s_ids),
			       &audin->i2s);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Can't get the i2s clocks\n");
		return ret;
	}

	ret = clk_prepare_enable(audin->pclk);
	if (ret) {
		dev_err(dev, "peripheral clock enable failed\n");
		return ret;
	}

	ret = devm_add_action_or_reset(dev,
				       (void(*)(void *))clk_disable_unprepare,
				       audin->pclk);
	if (ret)
		dev_err(dev, "failed to add reset action on pclk");

	return ret;
}

static int audin_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	void __iomem *regs;
	struct regmap *map;
	struct audin *audin;
	int ret;

	audin = devm_kzalloc(dev, sizeof(*audin), GFP_KERNEL);
	if (!audin)
		return -ENOMEM;
	platform_set_drvdata(pdev, audin);

	ret = device_reset(dev);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to reset device\n");
		return ret;
	}

	regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	map = devm_regmap_init_mmio(dev, regs, &audin_regmap_cfg);
	if (IS_ERR(map)) {
		dev_err(dev, "failed to init regmap: %ld\n",
			PTR_ERR(map));
		return PTR_ERR(map);
	}

	audin->i2s.irq = platform_get_irq_byname(pdev, "audin");
	if (audin->i2s.irq < 0) {
		return audin->i2s.irq;
	}

	ret = audin_clk_get(dev);
	if (ret)
		return ret;

	/* Register the cpu component of the audin */
	ret = snd_soc_register_component(dev, &audin_cpu_component,
					 audin_cpu_dai_drv,
					 ARRAY_SIZE(audin_cpu_dai_drv));
	if (ret) {
 		dev_err(dev, "Failed to register cpu component\n");
		return ret;
	}
 
	/* Register the codec control component */
	ret = audin_codec_ctrl_register_component(dev);
	if (ret) {
		dev_err(dev, "Failed to register codec control component\n");
		goto err;
	}

	return 0;
err:
	snd_soc_unregister_component(dev);

	return ret;
}

static int audin_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static const struct of_device_id audin_of_match[] = {
	{ .compatible = "amlogic,audin-gxbb", },
	{ .compatible = "amlogic,audin-gxl", },
	{}
};
MODULE_DEVICE_TABLE(of, audin_of_match);

static struct platform_driver audin_pdrv = {
	.probe = audin_probe,
	.remove = audin_remove,
	.driver = {
		.name = "meson-audin",
		.of_match_table = audin_of_match,
	},
};
module_platform_driver(audin_pdrv);

MODULE_DESCRIPTION("Meson AUDIN Driver");
MODULE_AUTHOR("Rezzonics <rezzonics@gmail.com>");
MODULE_LICENSE("GPL v2");
