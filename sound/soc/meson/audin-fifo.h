/* SPDX-License-Identifier: (GPL-2.0 OR MIT)
 *
 * Copyright (c) 2020 Rezzonics
 * Copyright (c) 2018 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 * Author: Rezzonics <rezzonics@gmail.com>
 */

#ifndef _MESON_AUDIN_FIFO_H
#define _MESON_AUDIN_FIFO_H

struct snd_pcm_hardware;
struct snd_soc_component_driver;
struct snd_soc_dai_driver;
struct clk;
struct snd_pcm_ops;
struct snd_pcm_substream;
struct snd_soc_dai;
struct snd_pcm_hw_params;
struct platform_device;

enum audin_fifo0_ctrl_din_sel_enum {
	SPDIF = 0,
	I2S,
	PCM,
	HDMI,
	DEMOD
};

int audin_fifo_dai_probe(struct snd_soc_dai *dai);
int audin_fifo_dai_remove(struct snd_soc_dai *dai);
int audin_fifo_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai);
int audin_fifo_prepare(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai);
int audin_fifo_i2s_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai);
int audin_fifo_hw_free(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai);
int audin_fifo_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai);
void audin_fifo_shutdown(struct snd_pcm_substream *substream,
		         struct snd_soc_dai *dai);
int audin_fifo_pcm_new(struct snd_soc_pcm_runtime *rtd,
		       struct snd_soc_dai *dai);
#endif /* _MESON_AUDIN_FIFO_H */
