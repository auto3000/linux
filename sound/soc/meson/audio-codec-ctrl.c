// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 Rezzonics
// Author: Rezzonics <rezzonics@gmail.com>

#include <linux/bitfield.h>
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <dt-bindings/sound/meson-audio.h>

#include "audio.h"
#include "meson-codec-glue.h"

#define CTRL_DATA_SEL_SHIFT	4

static const char * const aiu_codec_ctrl_mux_texts[] = {
	"DISABLED", "PCM", "I2S",
};

static const char * const audin_codec_src_sel_mux_texts[] = {
	"SRC0", "SRC1", "SRC2", "SRC3",
};

int audin_codec_ctrl_mux_put_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);

int aiu_codec_ctrl_mux_put_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

int audin_codec_ctrl_mux_get_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);

int aiu_codec_ctrl_mux_get_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

int aiu_codec_ctrl_mux_put_enum(struct snd_kcontrol *kcontrol,
			        struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AIU_HDMI_CLK_DATA_CTRL_DATA_SEL, mux);
	mask = AIU_HDMI_CLK_DATA_CTRL_DATA_SEL;
	snd_soc_component_test_bits(component, e->reg,
				    AIU_HDMI_CLK_DATA_CTRL_DATA_SEL,
				    FIELD_PREP(AIU_HDMI_CLK_DATA_CTRL_DATA_SEL, mux));
	changed = regmap_read(audio->aiu_map, AIU_HDMI_CLK_DATA_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;
	if (!changed)
		return 0;
	/* Force disconnect of the mux while updating */
	snd_soc_dapm_mux_update_power(dapm, kcontrol, 0, NULL, NULL);

	/* Reset the source first */
	regmap_update_bits(audio->aiu_map, AIU_HDMI_CLK_DATA_CTRL,
			  AIU_HDMI_CLK_DATA_CTRL_CLK_SEL |
			  AIU_HDMI_CLK_DATA_CTRL_DATA_SEL,
			  FIELD_PREP(AIU_HDMI_CLK_DATA_CTRL_CLK_SEL, 0) |
			  FIELD_PREP(AIU_HDMI_CLK_DATA_CTRL_DATA_SEL, 0));

	/* Set the appropriate source */
	regmap_update_bits(audio->aiu_map, AIU_HDMI_CLK_DATA_CTRL,
			  AIU_HDMI_CLK_DATA_CTRL_CLK_SEL |
			  AIU_HDMI_CLK_DATA_CTRL_DATA_SEL,
			  FIELD_PREP(AIU_HDMI_CLK_DATA_CTRL_CLK_SEL, mux) |
			  FIELD_PREP(AIU_HDMI_CLK_DATA_CTRL_DATA_SEL, mux));

	snd_soc_dapm_mux_update_power(dapm, kcontrol, mux, e, NULL);
//	printk("aiu_codec_ctrl_mux_put_enum: old=%x, new=%x\n", old, new); 
	return 0;
}
EXPORT_SYMBOL_GPL(aiu_codec_ctrl_mux_put_enum);

int aiu_codec_ctrl_mux_get_enum(struct snd_kcontrol *kcontrol,
			        struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_dapm_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_HDMI_CLK_DATA_CTRL_DATA_SEL;
	regmap_read(audio->aiu_map, AIU_HDMI_CLK_DATA_CTRL, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}
EXPORT_SYMBOL_GPL(aiu_codec_ctrl_mux_get_enum);

int audin_codec_ctrl_mux_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AUDIN_SOURCE_SEL_I2S, mux);
	mask = AUDIN_SOURCE_SEL_I2S;
	changed = regmap_read(audio->audin_map, AUDIN_SOURCE_SEL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	// Force disconnect of the mux while updating
	snd_soc_dapm_mux_update_power(dapm, kcontrol, 0, NULL, NULL);
	regmap_update_bits(audio->audin_map, AUDIN_SOURCE_SEL,
			   AUDIN_SOURCE_SEL_I2S,
			   FIELD_PREP(AUDIN_SOURCE_SEL_I2S, mux));

	snd_soc_dapm_mux_update_power(dapm, kcontrol, mux, e, NULL);
	return 0;
}
EXPORT_SYMBOL_GPL(audin_codec_ctrl_mux_put_enum);

int audin_codec_ctrl_mux_get_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_dapm_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_SOURCE_SEL_I2S;
	regmap_read(audio->audin_map, AUDIN_SOURCE_SEL, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}
EXPORT_SYMBOL_GPL(audin_codec_ctrl_mux_get_enum);

static const struct snd_soc_dai_ops aiu_codec_ctrl_input_ops = {
	.hw_params	= meson_codec_glue_input_hw_params,
	.set_fmt	= meson_codec_glue_input_set_fmt,
//	.startup	= meson_codec_glue_input_startup,
};

static const struct snd_soc_dai_ops aiu_codec_ctrl_output_ops = {
//	.hw_params	= meson_codec_glue_output_hw_params,
//	.set_fmt	= meson_codec_glue_output_set_fmt,
	.startup	= meson_codec_glue_output_startup,
};

static const struct snd_soc_dai_ops audin_codec_ctrl_output_ops = {
	.hw_params	= meson_codec_glue_output_hw_params,
	.set_fmt	= meson_codec_glue_output_set_fmt,
//	.startup	= meson_codec_glue_output_startup,
};

static const struct snd_soc_dai_ops audin_codec_ctrl_input_ops = {
//	.hw_params	= meson_codec_glue_input_hw_params,
//	.set_fmt	= meson_codec_glue_input_set_fmt,
	.startup	= meson_codec_glue_input_startup,
};

#define AUDIO_CODEC_CTRL_FORMATS	\
	(SNDRV_PCM_FMTBIT_S16_LE |	\
	 SNDRV_PCM_FMTBIT_S24_3LE | 	\
	 SNDRV_PCM_FMTBIT_S24_LE |	\
	 SNDRV_PCM_FMTBIT_S32_LE)

#define AUDIO_CODEC_CTRL_STREAM(xname, xsuffix, xchan)			\
{									\
	.stream_name	= xname " " xsuffix,				\
	.channels_min	= 1,						\
	.channels_max	= xchan,					\
	.rate_min       = 5512,						\
	.rate_max	= 192000,					\
	.formats	= AUDIO_CODEC_CTRL_FORMATS,			\
}

#define AIU_CODEC_CTRL_INPUT(xname, xchan) {				\
	.name = "CODEC CTRL " xname,					\
	.playback = AUDIO_CODEC_CTRL_STREAM(xname, "Playback", xchan),	\
	.ops = &aiu_codec_ctrl_input_ops,				\
	.probe = meson_codec_glue_input_dai_probe,			\
	.remove = meson_codec_glue_input_dai_remove,			\
}

#define AIU_CODEC_CTRL_OUTPUT(xname, xchan) {				\
	.name = "CODEC CTRL " xname,					\
	.playback = AUDIO_CODEC_CTRL_STREAM(xname, "Playback", xchan),	\
	.ops = &aiu_codec_ctrl_output_ops,				\
}
/*
	.probe = meson_codec_glue_input_dai_probe,			\
	.remove = meson_codec_glue_input_dai_remove,			\
*/
#define AUDIN_CODEC_CTRL_OUTPUT(xname, xchan) {				\
	.name = "CODEC REC " xname,					\
	.capture = AUDIO_CODEC_CTRL_STREAM(xname, "Capture", xchan),	\
	.ops = &audin_codec_ctrl_output_ops,				\
	.probe = meson_codec_glue_output_dai_probe,			\
	.remove = meson_codec_glue_output_dai_remove,			\
}

#define AUDIN_CODEC_CTRL_INPUT(xname, xchan) {				\
	.name = "CODEC REC " xname,					\
	.capture = AUDIO_CODEC_CTRL_STREAM(xname, "Capture", xchan),	\
	.ops = &audin_codec_ctrl_input_ops,				\
}
/*
	.probe = meson_codec_glue_output_dai_probe,			\
	.remove = meson_codec_glue_output_dai_remove,			\
*/
static SOC_ENUM_SINGLE_DECL(aiu_codec_ctrl_mux_enum, AIU_HDMI_CLK_DATA_CTRL,
			    0, aiu_codec_ctrl_mux_texts);

static const struct snd_kcontrol_new aiu_codec_ctrl_mux =
	SOC_DAPM_ENUM_EXT("Codec Source", aiu_codec_ctrl_mux_enum,
			  aiu_codec_ctrl_mux_get_enum,
			  aiu_codec_ctrl_mux_put_enum);

static SOC_ENUM_SINGLE_DECL(audin_codec_src_sel_mux_enum, AUDIN_SOURCE_SEL,
			    0, audin_codec_src_sel_mux_texts);
/*
static SOC_ENUM_SINGLE_DECL(audin_codec_src_sel_mux_enum, SND_SOC_NOPM,
			    0, audin_codec_src_sel_mux_texts);
*/
static const struct snd_kcontrol_new audin_source_sel_mux =
	SOC_DAPM_ENUM_EXT("Audin Src Sel", audin_codec_src_sel_mux_enum,
			  audin_codec_ctrl_mux_get_enum,
			  audin_codec_ctrl_mux_put_enum);

static const struct snd_soc_dapm_widget audio_codec_ctrl_widgets[] = {
	SND_SOC_DAPM_MUX("CODEC SRC", SND_SOC_NOPM, 0, 0,
			 &aiu_codec_ctrl_mux),
	SND_SOC_DAPM_MUX("AUDIN SRC", SND_SOC_NOPM, 0, 0,	
			 &audin_source_sel_mux),

	SND_SOC_DAPM_AIF_IN("I2S IN Playback", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("PCM IN Playback", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUTPUT("CODEC OUT Playback"),

	SND_SOC_DAPM_INPUT("CODEC IN0"),
	SND_SOC_DAPM_INPUT("CODEC IN1"),
	SND_SOC_DAPM_INPUT("CODEC IN2"),
	SND_SOC_DAPM_INPUT("CODEC IN3"),
	SND_SOC_DAPM_AIF_OUT("CODEC OUT Capture", "Capture",  0, SND_SOC_NOPM, 0, 0),

};

static struct snd_soc_dai_driver audio_codec_ctrl_dai_drv[] = {
	[CTRL_I2S] = AIU_CODEC_CTRL_INPUT("I2S IN", 2),
	[CTRL_PCM] = AIU_CODEC_CTRL_INPUT("PCM IN", 2),
	[CTRL_OUT] = AIU_CODEC_CTRL_OUTPUT("CODEC OUT", 2),
	[CODEC_IN0] = AUDIN_CODEC_CTRL_INPUT( "CODEC IN0", 8),
	[CODEC_IN1] = AUDIN_CODEC_CTRL_INPUT( "CODEC IN1", 8),
	[CODEC_IN2] = AUDIN_CODEC_CTRL_INPUT( "CODEC IN2", 8),
	[CODEC_IN3] = AUDIN_CODEC_CTRL_INPUT( "CODEC IN3", 8),
	[CODEC_OUT] = AUDIN_CODEC_CTRL_OUTPUT("CODEC OUT", 8),
};

static const struct snd_soc_dapm_route audio_codec_ctrl_routes[] = {
	{ "CODEC SRC", "I2S", "I2S IN Playback" },
	{ "CODEC SRC", "PCM", "PCM IN Playback" },
	{ "CODEC OUT Playback", NULL, "CODEC SRC" },
	{ "AUDIN SRC", "SRC0", "CODEC IN0 Capture" },
	{ "AUDIN SRC", "SRC1", "CODEC IN1 Capture" },
	{ "AUDIN SRC", "SRC2", "CODEC IN2 Capture" },
	{ "AUDIN SRC", "SRC3", "CODEC IN3 Capture" },
	{ "CODEC OUT Capture", NULL, "AUDIN SRC" }
};

static int audio_codec_of_xlate_dai_name(struct snd_soc_component *component,
				         struct of_phandle_args *args,
				         const char **dai_name)
{
	return audio_of_xlate_dai_name(component, args, dai_name, AUDIO_CODEC);
}

static const struct snd_soc_component_driver audio_codec_ctrl_component = {
	.name			= "AUDIO Codec Control",
	.dapm_widgets		= audio_codec_ctrl_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(audio_codec_ctrl_widgets),
	.dapm_routes		= audio_codec_ctrl_routes,
	.num_dapm_routes	= ARRAY_SIZE(audio_codec_ctrl_routes),
	.of_xlate_dai_name	= audio_codec_of_xlate_dai_name,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

int audio_codec_ctrl_register_component(struct device *dev)
{
	return snd_soc_register_component(dev, &audio_codec_ctrl_component,
					  audio_codec_ctrl_dai_drv,
					  ARRAY_SIZE(audio_codec_ctrl_dai_drv));
}

