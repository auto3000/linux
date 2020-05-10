// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 Rezzonics
// Author: Rezzonics <rezzonics@gmail.com>

#include <linux/bitfield.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <dt-bindings/sound/meson-audin.h>

#include "audin.h"
#include "meson-codec-glue.h"

#define I2SIN_CLK_SEL		BIT(1)

static const char * const audin_codec_clk_sel_mux_texts[] = {
	"EXT", "INT"
};

int audin_codec_ctrl_mux_put_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);

static const struct snd_soc_dai_ops audin_codec_ctrl_input_ops = {
	.startup	= meson_codec_glue_input_startup,
//	.hw_params	= meson_codec_glue_input_hw_params,
//	.set_fmt	= meson_codec_glue_input_set_fmt,
};

static const struct snd_soc_dai_ops audin_codec_ctrl_output_ops = {
//	.startup	= meson_codec_glue_output_startup,
	.hw_params	= meson_codec_glue_output_hw_params,
	.set_fmt	= meson_codec_glue_output_set_fmt,
};

#define AUDIN_CODEC_CTRL_FORMATS				\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |	\
	 SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S24_LE |	\
	 SNDRV_PCM_FMTBIT_S32_LE)

#define AUDIN_CODEC_CTRL_STREAM(xname, xsuffix, xchan)			\
{									\
	.stream_name	= xname " " xsuffix,				\
	.channels_min	= 1,						\
	.channels_max	= xchan,					\
	.rate_min       = 5512,						\
	.rate_max	= 192000,					\
	.formats	= AUDIN_CODEC_CTRL_FORMATS,			\
}

#define AUDIN_CODEC_CTRL_OUTPUT(xname, xchan) {				\
	.name = "CODEC REC " xname,					\
	.capture = AUDIN_CODEC_CTRL_STREAM(xname, "Capture", xchan),	\
	.ops = &audin_codec_ctrl_output_ops,				\
	.probe = meson_codec_glue_output_dai_probe,			\
	.remove = meson_codec_glue_output_dai_remove,			\
}

#define AUDIN_CODEC_CTRL_INPUT(xname, xchan) {				\
	.name = "CODEC REC " xname,					\
	.capture = AUDIN_CODEC_CTRL_STREAM(xname, "Capture", xchan),	\
	.ops = &audin_codec_ctrl_input_ops,				\
	.probe = meson_codec_glue_input_dai_probe,			\
	.remove = meson_codec_glue_input_dai_remove,			\
}

/*
int audin_codec_ctrl_mux_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int mux;
	int changed;

	snd_soc_component_read(component, e->reg, &mux);
	printk(KERN_INFO "audin_codec_ctrl_mux_put_enum: AUDIN_I2SIN_CTRL = %lx", mux);
	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	changed = snd_soc_component_test_bits(component, e->reg,
					      I2SIN_CLK_SEL,
					      FIELD_PREP(I2SIN_CLK_SEL, mux));
	printk(KERN_INFO "audin_codec_ctrl_mux_put_enum: mux = %d item0 = %d, changed = %d", 
			  mux, ucontrol->value.enumerated.item[0], changed);

	if (!changed) {
		return 0;
	}
	else {
		// Force disconnect of the mux while updating
		//snd_soc_dapm_mux_update_power(dapm, kcontrol, 0, NULL, NULL);

		snd_soc_component_update_bits(component, e->reg,
					      I2SIN_CLK_SEL,
					      FIELD_PREP(I2SIN_CLK_SEL, mux));

		//snd_soc_dapm_mux_update_power(dapm, kcontrol, mux, e, NULL);
		return 0;
	}
}
EXPORT_SYMBOL_GPL(audin_codec_ctrl_mux_put_enum);
*/
static SOC_ENUM_SINGLE_DECL(audin_codec_ctrl_mux_enum, AUDIN_I2SIN_CTRL,
			    I2SIN_CLK_SEL,
			    audin_codec_clk_sel_mux_texts);

static const struct snd_kcontrol_new audin_codec_ctrl_mux =
	SOC_DAPM_ENUM("I2S_Clk Mux", audin_codec_ctrl_mux_enum);
/*
	SOC_DAPM_SINGLE("I2S Clk Int", AUDIN_I2SIN_CTRL,
			  I2SIN_CLK_SEL, 1, 0);

	SOC_DAPM_ENUM_EXT("I2S_Clk Mux", audin_codec_ctrl_mux_enum,
			  snd_soc_dapm_get_enum_double,
			  audin_codec_ctrl_mux_put_enum);
*/
static const struct snd_soc_dapm_widget audin_codec_ctrl_widgets[] = {
	SND_SOC_DAPM_MUX("I2S_CLK Mux", AUDIN_I2SIN_CTRL, I2SIN_CLK_SEL, 0,
			 &audin_codec_ctrl_mux),
};

static struct snd_soc_dai_driver audin_codec_ctrl_dai_drv[] = {
	[CLK_EXT] = AUDIN_CODEC_CTRL_INPUT( "I2S CLK EXT", 2),
	[CLK_INT] = AUDIN_CODEC_CTRL_INPUT( "I2S CLK INT", 2),
	[CLK_OUT] = AUDIN_CODEC_CTRL_OUTPUT("I2S CLK OUT", 2),
};

static const struct snd_soc_dapm_route audin_codec_ctrl_routes[] = {
	{ "I2S_CLK Mux", "EXT", "I2S CLK EXT Capture" },
	{ "I2S_CLK Mux", "INT", "I2S CLK INT Capture" },
	{ "I2S CLK OUT Capture", NULL, "I2S_CLK Mux" },
};

static int audin_codec_of_xlate_dai_name(struct snd_soc_component *component,
				        struct of_phandle_args *args,
				        const char **dai_name)
{
	return audin_of_xlate_dai_name(component, args, dai_name, AUDIN_CODEC);
}

static const struct snd_soc_component_driver audin_codec_ctrl_component = {
	.name			= "AUDIN Codec Control",
	.dapm_widgets		= audin_codec_ctrl_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(audin_codec_ctrl_widgets),
	.dapm_routes		= audin_codec_ctrl_routes,
	.num_dapm_routes	= ARRAY_SIZE(audin_codec_ctrl_routes),
	.of_xlate_dai_name	= audin_codec_of_xlate_dai_name,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

int audin_codec_ctrl_register_component(struct device *dev)
{
	return snd_soc_register_component(dev, &audin_codec_ctrl_component,
					  audin_codec_ctrl_dai_drv,
					  ARRAY_SIZE(audin_codec_ctrl_dai_drv));
}

