// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 BayLibre, SAS.
// Author: Jerome Brunet <jbrunet@baylibre.com>

#include <linux/bitfield.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <dt-bindings/sound/meson-aiu.h>
#include "aiu.h"
#include "meson-codec-glue.h"

#define CTRL_CLK_SEL		GENMASK(1, 0)
#define CTRL_DATA_SEL_SHIFT	4
#define CTRL_DATA_SEL		(0x3 << CTRL_DATA_SEL_SHIFT)

static const char * const aiu_extcodec_ctrl_mux_texts[] = {
	"DISABLED", "I2S", "PCM",
};

static int aiu_extcodec_ctrl_mux_put_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int mux, changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	changed = snd_soc_component_test_bits(component, e->reg,
					      CTRL_DATA_SEL,
					      FIELD_PREP(CTRL_DATA_SEL, mux));

	if (!changed)
		return 0;

	/* Force disconnect of the mux while updating */
	snd_soc_dapm_mux_update_power(dapm, kcontrol, 0, NULL, NULL);

	/* Reset the source first */
	snd_soc_component_update_bits(component, e->reg,
				      CTRL_CLK_SEL |
				      CTRL_DATA_SEL,
				      FIELD_PREP(CTRL_CLK_SEL, 0) |
				      FIELD_PREP(CTRL_DATA_SEL, 0));

	/* Set the appropriate source */
	snd_soc_component_update_bits(component, e->reg,
				      CTRL_CLK_SEL |
				      CTRL_DATA_SEL,
				      FIELD_PREP(CTRL_CLK_SEL, mux) |
				      FIELD_PREP(CTRL_DATA_SEL, mux));

	snd_soc_dapm_mux_update_power(dapm, kcontrol, mux, e, NULL);

	return 0;
}

static SOC_ENUM_SINGLE_DECL(aiu_extcodec_ctrl_mux_enum, AIU_CODEC_CLK_DATA_CTRL,
			    CTRL_DATA_SEL_SHIFT,
			    aiu_extcodec_ctrl_mux_texts);

static const struct snd_kcontrol_new aiu_extcodec_ctrl_mux =
	SOC_DAPM_ENUM_EXT("ExtCodec Source", aiu_extcodec_ctrl_mux_enum,
			  snd_soc_dapm_get_enum_double,
			  aiu_extcodec_ctrl_mux_put_enum);

static const struct snd_soc_dapm_widget aiu_extcodec_ctrl_widgets[] = {
	SND_SOC_DAPM_MUX("EXTCODEC CTRL SRC", SND_SOC_NOPM, 0, 0,
			 &aiu_extcodec_ctrl_mux),
};

static const struct snd_soc_dai_ops aiu_extcodec_ctrl_input_ops = {
	.hw_params	= meson_codec_glue_input_hw_params,
	.set_fmt	= meson_codec_glue_input_set_fmt,
};

static const struct snd_soc_dai_ops aiu_extcodec_ctrl_output_ops = {
	.startup	= meson_codec_glue_output_startup,
};

#define AIU_EXTCODEC_CTRL_FORMATS				\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |	\
	 SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S24_LE |	\
	 SNDRV_PCM_FMTBIT_S32_LE)	// not supported by CS4245

#define AIU_EXTCODEC_CTRL_STREAM(xname, xsuffix)			\
{								\
	.stream_name	= xname " " xsuffix,			\
	.channels_min	= 1,					\
	.channels_max	= 2,					\
	.rate_min       = 5512,					\
	.rate_max	= 192000,				\
	.formats	= AIU_EXTCODEC_CTRL_FORMATS,		\
}

#define AIU_EXTCODEC_CTRL_INPUT(xname) {				\
	.name = "EXTCODEC CTRL " xname,				\
	.playback = AIU_EXTCODEC_CTRL_STREAM(xname, "Playback"),	\
	.ops = &aiu_extcodec_ctrl_input_ops,			\
	.probe = meson_codec_glue_input_dai_probe,		\
	.remove = meson_codec_glue_input_dai_remove,		\
}

#define AIU_EXTCODEC_CTRL_OUTPUT(xname) {				\
	.name = "EXTCODEC CTRL " xname,				\
	.capture = AIU_EXTCODEC_CTRL_STREAM(xname, "Capture"), 	\
	.ops = &aiu_extcodec_ctrl_output_ops,			\
}

static struct snd_soc_dai_driver aiu_extcodec_ctrl_dai_drv[] = {
	[CTRL_I2S] = AIU_EXTCODEC_CTRL_INPUT("EXTCODEC I2S IN"),
	[CTRL_PCM] = AIU_EXTCODEC_CTRL_INPUT("EXTCODEC PCM IN"),
	[CTRL_OUT] = AIU_EXTCODEC_CTRL_OUTPUT("EXTCODEC OUT"),
};

static const struct snd_soc_dapm_route aiu_extcodec_ctrl_routes[] = {
	{ "EXTCODEC CTRL SRC", "I2S", "EXTCODEC I2S IN Playback" },
	{ "EXTCODEC CTRL SRC", "PCM", "EXTCODEC PCM IN Playback" },
	{ "EXTCODEC OUT Capture", NULL, "EXTCODEC CTRL SRC" },
};

static int aiu_extcodec_of_xlate_dai_name(struct snd_soc_component *component,
					struct of_phandle_args *args,
					const char **dai_name)
{
	return aiu_of_xlate_dai_name(component, args, dai_name, AIU_EXTCODEC);
}

static const struct snd_soc_component_driver aiu_extcodec_ctrl_component = {
	.name			= "AIU External Codec Control",
	.dapm_widgets		= aiu_extcodec_ctrl_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(aiu_extcodec_ctrl_widgets),
	.dapm_routes		= aiu_extcodec_ctrl_routes,
	.num_dapm_routes	= ARRAY_SIZE(aiu_extcodec_ctrl_routes),
	.of_xlate_dai_name	= aiu_extcodec_of_xlate_dai_name,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

int aiu_extcodec_ctrl_register_component(struct device *dev)
{
	return snd_soc_register_component(dev, &aiu_extcodec_ctrl_component,
					  aiu_extcodec_ctrl_dai_drv,
					  ARRAY_SIZE(aiu_extcodec_ctrl_dai_drv));
}

