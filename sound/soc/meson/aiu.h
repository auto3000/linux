/* SPDX-License-Identifier: (GPL-2.0 OR MIT) */
/*
 * Copyright (c) 2018 BayLibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 */

#ifndef _MESON_AIU_H
#define _MESON_AIU_H

struct clk;
struct clk_bulk_data;
struct device;
struct of_phandle_args;
struct snd_soc_dai;
struct snd_soc_dai_ops;

enum aiu_clk_ids {
	PCLK = 0,
	AOCLK,
	MCLK,
	MIXER,
};

struct aiu_interface {
	struct clk_bulk_data *clks;
	unsigned int clk_num;
	int irq;
};

struct aiu {
	struct clk *pclk;
	struct clk *spdif_mclk;
	struct aiu_interface i2s;
	struct aiu_interface spdif;
};

#define AIU_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |	\
		     SNDRV_PCM_FMTBIT_S24_LE | \
		     SNDRV_PCM_FMTBIT_S32_LE)

int aiu_of_xlate_dai_name(struct snd_soc_component *component,
			  struct of_phandle_args *args,
			  const char **dai_name,
			  unsigned int component_id);
int aiu_hdmi_ctrl_register_component(struct device *dev);
int aiu_acodec_ctrl_register_component(struct device *dev);
int aiu_codec_ctrl_register_component(struct device *dev);

int aiu_fifo_i2s_dai_probe(struct snd_soc_dai *dai);
int aiu_fifo_spdif_dai_probe(struct snd_soc_dai *dai);
int aiu_clk_get(struct device *dev);
int aiu_clk_bulk_get(struct device *dev,
		     const char * const *ids,
		     unsigned int num,
		     struct aiu_interface *interface);


extern const struct snd_soc_dai_ops aiu_fifo_i2s_dai_ops;
extern const struct snd_soc_dai_ops aiu_fifo_spdif_dai_ops;
extern const struct snd_soc_dai_ops aiu_encoder_i2s_dai_ops;
extern const struct snd_soc_dai_ops aiu_encoder_spdif_dai_ops;

#define AIU_IEC958_BPF			0x000
#define AIU_958_MISC			0x010
#define AIU_IEC958_DCU_FF_CTRL		0x01c
#define AIU_958_CHSTAT_L0		0x020
#define AIU_958_CHSTAT_L1		0x024
#define AIU_958_CTRL			0x028
#define AIU_I2S_SOURCE_DESC		0x034
#define AIU_I2S_DAC_CFG			0x040
#define AIU_I2S_SYNC			0x044
#define AIU_I2S_MISC			0x048
#define AIU_RST_SOFT			0x054
#define AIU_CLK_CTRL			0x058
#define AIU_MIX_ADCCFG			0x05c
#define AIU_CLK_CTRL_MORE		0x064
#define AIU_CODEC_DAC_LRCLK_CTRL	0x0a0
#define AIU_CODEC_ADC_LRCLK_CTRL	0x0a4
#define AIU_HDMI_CLK_DATA_CTRL		0x0a8
#define AIU_CODEC_CLK_DATA_CTRL		0x0ac
#define AIU_ACODEC_CTRL			0x0b0
#define AIU_958_CHSTAT_R0		0x0c0
#define AIU_958_CHSTAT_R1		0x0c4
#define AIU_MEM_I2S_START		0x180
#define AIU_MEM_I2S_MASKS		0x18c
#define AIU_MEM_I2S_CONTROL		0x190
#define AIU_MEM_IEC958_START		0x194
#define AIU_MEM_IEC958_CONTROL		0x1a4
#define AIU_MEM_I2S_BUF_CNTL		0x1d8
#define AIU_MEM_IEC958_BUF_CNTL		0x1fc

#define AIU_I2S_SOURCE_DESC_MODE_8CH	BIT(0)
#define AIU_I2S_SOURCE_DESC_MSB_INV	BIT(1)
#define AIU_I2S_SOURCE_DESC_MSB_EXTEND	BIT(2)
#define AIU_I2S_SOURCE_DESC_MSB_POS	BIT(4)
#define AIU_I2S_SOURCE_DESC_MODE_24BIT	BIT(5)
#define AIU_I2S_SOURCE_DESC_SHIFT_BITS	GENMASK(8, 6)
#define AIU_I2S_SOURCE_DESC_MODE_32BIT	BIT(9)
#define AIU_I2S_SOURCE_DESC_MODE_SPLIT	BIT(11)
#define AIU_RST_SOFT_I2S_FAST		BIT(0)

#define AIU_I2S_DAC_CFG_SIZE		GENMASK(1, 0)
#define AIU_I2S_DAC_CFG_MSB_FIRST	BIT(2)
#define AIU_I2S_MISC_HOLD_EN		BIT(2)
#define AIU_CLK_CTRL_I2S_DIV_EN		BIT(0)
#define AIU_CLK_CTRL_I2S_DIV		GENMASK(3, 2)
#define AIU_CLK_CTRL_AOCLK_INVERT	BIT(6)
#define AIU_CLK_CTRL_LRCLK_INVERT	BIT(7)
#define AIU_CLK_CTRL_LRCLK_SKEW		GENMASK(9, 8)
#define AIU_CLK_CTRL_MORE_I2S_DIV	GENMASK(5, 0)
#define AIU_CLK_CTRL_MORE_HDMI_AMCLK	BIT(6)
#define AIU_CLK_CTRL_MORE_ADC_DIV	GENMASK(13, 8)
#define AIU_CLK_CTRL_MORE_ADC_EN	BIT(14)
//#define AIU_CLK_CTRL_MORE_SCLK_INVERT	BIT(15)
#define AIU_MIX_ADCCFG_AOCLK_INVERT	BIT(3)
#define AIU_MIX_ADCCFG_LRCLK_INVERT	BIT(4)
#define AIU_MIX_ADCCFG_LRCLK_SKEW	GENMASK(7, 5)
#define AIU_MIX_ADCCFG_ADC_SIZE		GENMASK(11, 10)
#define AIU_MIX_ADCCFG_ADC_SEL		BIT(12)
#define AIU_CODEC_DAC_LRCLK_CTRL_DIV	GENMASK(11, 0)
#define AIU_CODEC_ADC_LRCLK_CTRL_DIV	GENMASK(11, 0)

#endif /* _MESON_AIU_H */


