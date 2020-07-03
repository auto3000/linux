// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 BayLibre, SAS.
// Author: Jerome Brunet <jbrunet@baylibre.com>

#ifndef _MESON_AIU_CODEC_CTRL_H
#define _MESON_AIU_CODEC_CTRL_H

#include <linux/bitfield.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <dt-bindings/sound/meson-aiu.h>

#include "meson-codec-glue.h"

#define CTRL_CLK_SEL		GENMASK(1, 0)
#define CTRL_DATA_SEL_SHIFT	4
#define CTRL_DATA_SEL		(0x3 << CTRL_DATA_SEL_SHIFT)

static const char * const aiu_codec_ctrl_mux_texts[] = {
	"DISABLED", "PCM", "I2S",
};

int aiu_codec_ctrl_mux_put_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

static const struct snd_soc_dai_ops aiu_codec_ctrl_input_ops = {
	.hw_params	= meson_codec_glue_input_hw_params,
	.set_fmt	= meson_codec_glue_input_set_fmt,
};

static const struct snd_soc_dai_ops aiu_codec_ctrl_output_ops = {
	.startup	= meson_codec_glue_output_startup,
};

#define AIU_CODEC_CTRL_FORMATS					\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |	\
	 SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S24_LE |	\
	 SNDRV_PCM_FMTBIT_S32_LE)

#define AIU_CODEC_CTRL_STREAM(xname, xsuffix, xchan)			\
{									\
	.stream_name	= xname " " xsuffix,				\
	.channels_min	= 1,						\
	.channels_max	= xchan,					\
	.rate_min       = 5512,						\
	.rate_max	= 192000,					\
	.formats	= AIU_CODEC_CTRL_FORMATS,			\
}

#define AIU_CODEC_CTRL_INPUT(xname, xchan) {				\
	.name = "CODEC CTRL " xname,					\
	.playback = AIU_CODEC_CTRL_STREAM(xname, "Playback", xchan),	\
	.ops = &aiu_codec_ctrl_input_ops,				\
	.probe = meson_codec_glue_input_dai_probe,			\
	.remove = meson_codec_glue_input_dai_remove,			\
}

#define AIU_CODEC_CTRL_OUTPUT(xname, xchan) {				\
	.name = "CODEC CTRL " xname,					\
	.capture = AIU_CODEC_CTRL_STREAM(xname, "Capture", xchan),	\
	.ops = &aiu_codec_ctrl_output_ops,				\
}

#endif /* _MESON_AIU_CODEC_CTRL_H */


