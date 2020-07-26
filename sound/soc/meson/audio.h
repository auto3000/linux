/* SPDX-License-Identifier: (GPL-2.0 OR MIT) */
/*
 * Copyright (c) 2020 Rezzonics
 * Author: Rezzonics <rezzonics@gmail.com>
 */

#ifndef _MESON_AUDIO_H
#define _MESON_AUDIO_H
#define REGMAP_ALLOW_WRITE_DEBUGFS

struct clk;
struct clk_bulk_data;
struct device;
struct of_phandle_args;
struct snd_soc_dai;
struct snd_soc_dai_ops;

struct snd_pcm_hardware;
struct snd_pcm_substream;
struct snd_pcm_hw_params;

enum aiu_clk_ids {
	PCLK = 0,
	AOCLK,
	MCLK,
	MIXER,
};

enum audin_clk_ids {
	AUDBUF = 0,
	AUDIN,
	ADC,
};

struct audio_interface {
	struct clk_bulk_data *clks;
	unsigned int clk_num;
	int irq;
	char *irq_name;
};

struct audio {
	struct clk *pclk;
	struct clk *spdif_mclk;
	struct audio_interface aiu;
//	struct audio_interface spdif;
	struct audio_interface audin;
	void __iomem *aiu_regs;
	void __iomem *audin_regs;
	struct regmap *aiu_map;
	struct regmap *audin_map;
};

struct audio_fifo {
	struct snd_pcm_hardware *pcm;
	unsigned int mem_offset;
	unsigned int fifo_block;
	struct clk *pclk;
	int irq;
	char *irq_name;
};

#define AUDIO_FORMATS						\
	(SNDRV_PCM_FMTBIT_S16_LE  | SNDRV_PCM_FMTBIT_S24_3LE |	\
	 SNDRV_PCM_FMTBIT_S24_3BE | SNDRV_PCM_FMTBIT_S24_LE  |	\
	 SNDRV_PCM_FMTBIT_S32_LE  | SNDRV_PCM_FMTBIT_S32_BE)

int audio_of_xlate_dai_name(struct snd_soc_component *component,
			  struct of_phandle_args *args,
			  const char **dai_name,
			  unsigned int component_id);
//int audio_hdmi_ctrl_register_component(struct device *dev);
//int audio_acodec_ctrl_register_component(struct device *dev);
int audio_codec_ctrl_register_component(struct device *dev);

int aiu_fifo_i2s_dai_probe(struct snd_soc_dai *dai);
//int aiu_fifo_spdif_dai_probe(struct snd_soc_dai *dai);

int audin_fifo_i2s_dai_probe(struct snd_soc_dai *dai);
//int audin_fifo_spdif_dai_probe(struct snd_soc_dai *dai);

extern const struct snd_soc_dai_ops aiu_fifo_i2s_dai_ops;
//extern const struct snd_soc_dai_ops aiu_fifo_spdif_dai_ops;
extern const struct snd_soc_dai_ops aiu_encoder_i2s_dai_ops;
//extern const struct snd_soc_dai_ops aiu_decoder_spdif_dai_ops;

extern const struct snd_soc_dai_ops audin_fifo_i2s_dai_ops;
//extern const struct snd_soc_dai_ops audin_fifo_spdif_dai_ops;
extern const struct snd_soc_dai_ops audin_decoder_i2s_dai_ops;
//extern const struct snd_soc_dai_ops audin_decoder_spdif_dai_ops;

//struct snd_pcm_hardware audio_fifo_i2s_pcm;

snd_pcm_uframes_t audio_fifo_pointer(struct snd_soc_component *component,
				     struct snd_pcm_substream *substream);

/* AIU registers */
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
#define AIU_MEM_I2S_RD			0x184
#define AIU_MEM_I2S_END			0x188
#define AIU_MEM_I2S_MASKS		0x18c
#define AIU_MEM_I2S_CONTROL		0x190
#define AIU_MEM_IEC958_START		0x194
#define AIU_MEM_IEC958_CONTROL		0x1a4
#define AIU_MEM_I2S_BUF_CNTL		0x1d8
#define AIU_MEM_IEC958_BUF_CNTL		0x1fc

/* AIU register masks */
#define AIU_I2S_SOURCE_DESC_MODE_8CH	BIT(0)
#define AIU_I2S_SOURCE_DESC_MSB_INV	BIT(1)
#define AIU_I2S_SOURCE_DESC_MSB_EXTEND	BIT(2)
#define AIU_I2S_SOURCE_DESC_MSB_POS	GENMASK(4, 3)
#define AIU_I2S_SOURCE_DESC_MODE_24BIT	BIT(5)
#define AIU_I2S_SOURCE_DESC_SHIFT_BITS	GENMASK(8, 6)
#define AIU_I2S_SOURCE_DESC_MODE_32BIT	BIT(9)
#define AIU_I2S_SOURCE_DESC_MODE_SPLIT	BIT(11)

#define AIU_I2S_DAC_CFG_SIZE		GENMASK(1, 0)
#define AIU_I2S_DAC_CFG_MSB_FIRST	BIT(2)
#define AIU_I2S_MISC_HOLD_EN		BIT(2)
#define AIU_I2S_MISC_958_SRC_SHIFT 	BIT(3)

#define AIU_CLK_CTRL_I2S_DIV_EN		BIT(0)
#define AIU_CLK_CTRL_I2S_DIV		GENMASK(3, 2)
#define AIU_CLK_CTRL_AOCLK_INVERT	BIT(6)
#define AIU_CLK_CTRL_LRCLK_INVERT	BIT(7)
#define AIU_CLK_CTRL_LRCLK_SKEW		GENMASK(9, 8)
#define AIU_CLK_CTRL_CLK_SRC		BIT(10)

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

#define AIU_HDMI_CLK_DATA_CTRL_CLK_SEL	GENMASK(1, 0)
#define AIU_HDMI_CLK_DATA_CTRL_DATA_SEL GENMASK(5, 4)

#define AIU_MEM_I2S_MASKS_CH_RD		GENMASK(7, 0)
#define AIU_MEM_I2S_MASKS_CH_MEM	GENMASK(15, 8)
#define AIU_MEM_I2S_MASKS_IRQ_BLOCK	GENMASK(31, 16)

#define AIU_MEM_I2S_CONTROL_INIT 	BIT(0)
#define AIU_MEM_I2S_CONTROL_FILL_EN 	BIT(1)
#define AIU_MEM_I2S_CONTROL_EMPTY_EN 	BIT(2)
#define AIU_MEM_I2S_CONTROL_ENDIAN	GENMASK(5, 3)
#define AIU_MEM_I2S_CONTROL_MODE_16BIT	BIT(6)
#define AIU_MEM_I2S_CONTROL_LVL		BIT(9)
#define AIU_MEM_I2S_CONTROL_PTR		GENMASK(11, 10)

#define AIU_MEM_I2S_BUF_CNTL_INIT	BIT(0)

#define AIU_RST_SOFT_I2S_FAST		BIT(0)

/* AIU register offsets */
/* Registers offset from AIU_MEM_I2S_START */
#define AIU_MEM_I2S_START_OFF		0x00
#define AIU_MEM_I2S_RD_OFF		(AIU_MEM_I2S_RD - AIU_MEM_I2S_START)
#define AIU_MEM_I2S_END_OFF		(AIU_MEM_I2S_END - AIU_MEM_I2S_START)
#define AIU_MEM_I2S_MASKS_OFF		(AIU_MEM_I2S_MASKS - AIU_MEM_I2S_START)
#define AIU_MEM_I2S_CONTROL_OFF		(AIU_MEM_I2S_CONTROL - AIU_MEM_I2S_START)

/* AUDIN registers */
#define AUDIN_SPDIF_MODE		0x000
#define AUDIN_SPDIF_FS_CLK_RLTN		0x004
#define AUDIN_SPDIF_CHNL_STS_A		0x008
#define AUDIN_SPDIF_CHNL_STS_B		0x00C
#define AUDIN_SPDIF_MISC		0x010
#define AUDIN_SPDIF_NPCM_PCPD		0x014
#define AUDIN_SPDIF_END			0x03C	/* Unknown */
#define AUDIN_I2SIN_CTRL		0x040
#define AUDIN_SOURCE_SEL		0x044
#define AUDIN_DECODE_FORMAT		0x048
#define AUDIN_DECODE_CONTROL_STATUS	0x04C
#define AUDIN_DECODE_CHANNEL_STATUS_A_0	0x050
#define AUDIN_DECODE_CHANNEL_STATUS_A_1	0x054
#define AUDIN_DECODE_CHANNEL_STATUS_A_2	0x058
#define AUDIN_DECODE_CHANNEL_STATUS_A_3	0x05C
#define AUDIN_DECODE_CHANNEL_STATUS_A_4	0x060
#define AUDIN_DECODE_CHANNEL_STATUS_A_5	0x064
#define AUDIN_FIFO0_START		0x080
#define AUDIN_FIFO0_END			0x084
#define AUDIN_FIFO0_PTR			0x088
#define AUDIN_FIFO0_INTR		0x08C
#define AUDIN_FIFO0_RDPTR		0x090
#define AUDIN_FIFO0_CTRL		0x094
#define AUDIN_FIFO0_CTRL1		0x098
#define AUDIN_FIFO0_LVL0		0x09C
#define AUDIN_FIFO0_LVL1		0x0A0
#define AUDIN_FIFO0_LVL2		0x0A4
#define AUDIN_FIFO0_REQID		0x0C0
#define AUDIN_FIFO0_WRAP		0x0C4
#define AUDIN_FIFO1_START		0x0CC
#define AUDIN_FIFO1_END			0x0D0
#define AUDIN_FIFO1_PTR			0x0D4
#define AUDIN_FIFO1_INTR		0x0D8
#define AUDIN_FIFO1_RDPTR		0x0DC
#define AUDIN_FIFO1_CTRL		0x0E0
#define AUDIN_FIFO1_CTRL1		0x0E4
#define AUDIN_FIFO1_LVL0		0x100
#define AUDIN_FIFO1_LVL1		0x104
#define AUDIN_FIFO1_LVL2		0x108
#define AUDIN_FIFO1_REQID		0x10C
#define AUDIN_FIFO1_WRAP		0x110
#define AUDIN_FIFO2_START		0x114
#define AUDIN_FIFO2_END			0x118
#define AUDIN_FIFO2_PTR			0x11C
#define AUDIN_FIFO2_INTR		0x120
#define AUDIN_FIFO2_RDPTR		0x124
#define AUDIN_FIFO2_CTRL		0x128
#define AUDIN_FIFO2_CTRL1		0x12C
#define AUDIN_FIFO2_LVL0		0x130
#define AUDIN_FIFO2_LVL1		0x134
#define AUDIN_FIFO2_LVL2		0x138
#define AUDIN_FIFO2_REQID		0x13C
#define AUDIN_FIFO2_WRAP		0x140
#define AUDIN_INT_CTRL			0x144
#define AUDIN_FIFO_INT			0x148
#define PCMIN_CTRL0			0x180
#define PCMIN_CTRL1			0x184
#define PCMIN1_CTRL0			0x188
#define PCMIN1_CTRL1			0x18C
#define PCMOUT_CTRL0			0x1C0
#define PCMOUT_CTRL1			0x1C4
#define PCMOUT_CTRL2			0x1C8
#define PCMOUT_CTRL3			0x1CC
#define PCMOUT1_CTRL0			0x1D0
#define PCMOUT1_CTRL1			0x1D4
#define PCMOUT1_CTRL2			0x1D8
#define PCMOUT1_CTRL3			0x1DC
#define AUDOUT_CTRL			0x200
#define AUDOUT_CTRL1			0x204
#define AUDOUT_BUF0_STA			0x208
#define AUDOUT_BUF0_EDA			0x20C
#define AUDOUT_BUF0_WPTR		0x210
#define AUDOUT_BUF1_STA			0x214
#define AUDOUT_BUF1_EDA			0x218
#define AUDOUT_BUF1_WPTR		0x21C
#define AUDOUT_FIFO_RPTR		0x220
#define AUDOUT_INTR_PTR			0x224
#define AUDOUT_FIFO_STS			0x228
#define AUDOUT1_CTRL			0x240
#define AUDOUT1_CTRL1			0x244
#define AUDOUT1_BUF0_STA		0x248
#define AUDOUT1_BUF0_EDA		0x24C
#define AUDOUT1_BUF0_WPTR		0x250
#define AUDOUT1_BUF1_STA		0x254
#define AUDOUT1_BUF1_EDA		0x258
#define AUDOUT1_BUF1_WPTR		0x25C
#define AUDOUT1_FIFO_RPTR		0x260
#define AUDOUT1_INTR_PTR		0x264
#define AUDOUT1_FIFO_STS		0x268
#define AUDIN_HDMI_MEAS_CTRL		0x280
#define AUDIN_HDMI_MEAS_CYCLES_M1	0x284
#define AUDIN_HDMI_MEAS_INTR_MASKN	0x288
#define AUDIN_HDMI_MEAS_INTR_STAT	0x28C
#define AUDIN_HDMI_REF_CYCLES_STAT_0	0x290
#define AUDIN_HDMI_REF_CYCLES_STAT_1	0x294
#define AUDIN_HDMIRX_AFIFO_STAT		0x298
#define AUDIN_FIFO0_PIO_STS		0x2C0
#define AUDIN_FIFO0_PIO_RDL		0x2C4
#define AUDIN_FIFO0_PIO_RDH		0x2C8
#define AUDIN_FIFO1_PIO_STS		0x2CC
#define AUDIN_FIFO1_PIO_RDL		0x2D0
#define AUDIN_FIFO1_PIO_RDH		0x2D4
#define AUDIN_FIFO2_PIO_STS		0x2D8
#define AUDIN_FIFO2_PIO_RDL		0x2DC
#define AUDIN_FIFO2_PIO_RDH		0x2E0
#define AUDOUT_FIFO_PIO_STS		0x2E4
#define AUDOUT_FIFO_PIO_WRL		0x2E8
#define AUDOUT_FIFO_PIO_WRH		0x2EC
#define AUDOUT1_FIFO_PIO_STS		0x2F0	/* Unknown */
#define AUDOUT1_FIFO_PIO_WRL		0x2F4	/* Unknown */
#define AUDOUT1_FIFO_PIO_WRH		0x2F8	/* Unknown */
#define AUD_RESAMPLE_CTRL0		0x2FC
#define AUD_RESAMPLE_CTRL1		0x300
#define AUD_RESAMPLE_STATUS		0x304

/* AUDIN register masks */
#define AUDIN_I2SIN_CTRL_I2SIN_DIR		BIT(0)
#define AUDIN_I2SIN_CTRL_I2SIN_CLK_SEL		BIT(1)
#define AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SEL	BIT(2)
#define AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC		BIT(3)
#define AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW	GENMASK(6, 4)
#define AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT	BIT(7)
#define AUDIN_I2SIN_CTRL_I2SIN_SIZE		GENMASK(9, 8)
#define AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN		GENMASK(13, 10)
#define AUDIN_I2SIN_CTRL_I2SIN_EN		BIT(15)

#define AUDIN_FIFO_CTRL_EN			BIT(0)
#define AUDIN_FIFO_CTRL_RST			BIT(1)
#define AUDIN_FIFO_CTRL_LOAD			BIT(2)
#define AUDIN_FIFO_CTRL_DIN_SEL			GENMASK(5, 3)
#define AUDIN_FIFO_CTRL_ENDIAN			GENMASK(10, 8)
#define AUDIN_FIFO_CTRL_CHAN			GENMASK(14, 11)
#define AUDIN_FIFO_CTRL_UG			BIT(15)
#define AUDIN_FIFO_CTRL_HOLD0_EN		BIT(19)
#define AUDIN_FIFO_CTRL_HOLD1_EN		BIT(20)
#define AUDIN_FIFO_CTRL_HOLD2_EN		BIT(21)
#define AUDIN_FIFO_CTRL_HOLD0_SEL		GENMASK(23, 22)
#define AUDIN_FIFO_CTRL_HOLD1_SEL		GENMASK(25, 24)
#define AUDIN_FIFO_CTRL_HOLD2_SEL		GENMASK(27, 26)
#define AUDIN_FIFO_CTRL_HOLD_LVL		BIT(28)

#define AUDIN_FIFO_CTRL1_DINPOS			GENMASK(1, 0)
#define AUDIN_FIFO_CTRL1_DINBYTENUM		GENMASK(3, 2)
#define AUDIN_FIFO_CTRL1_DESTSEL		GENMASK(5, 4)
#define AUDIN_FIFO_CTRL1_DINPOS2		BIT(7)

#define AUDIN_DECODE_FMT_NOSPDIF		BIT(0)  	//0x0000_0001 0001
#define AUDIN_DECODE_FMT_BIT_WIDTH		GENMASK(3, 2)	//0x0000_000C 1100 
#define AUDIN_DECODE_FMT_FMT_SELECT		GENMASK(5, 4)	//0x0000_0030 0011
#define AUDIN_DECODE_FMT_CHAN_CFG		BIT(6)  	//0x0000_0040 0100
#define AUDIN_DECODE_FMT_HDMI_TX		BIT(7)  	//0x0000_0080 1000
#define AUDIN_DECODE_FMT_CH_ALLOC		GENMASK(15, 8) 	//0x0000_FF00 1111_1111
#define AUDIN_DECODE_FMT_I2S_ENA		BIT(16) 	//0x0001_0000 0001
#define AUDIN_DECODE_FMT_I2S_BLK		GENMASK(21, 20)	//0x0030_0000 0011
#define AUDIN_DECODE_FMT_SPDIF_ENA		BIT(24)		//0x0100_0000 0001

#define AUDIN_SOURCE_SEL_I2S			GENMASK(1, 0)	//0x0000_0003 0011 
#define AUDIN_SOURCE_SEL_CLK_SEL		GENMASK(3, 2)	//0x0000_000C 1100 
#define AUDIN_SOURCE_SEL_SPDIF			GENMASK(5, 4)	//0x0000_0030 0011 
#define AUDIN_SOURCE_SEL_HDMI_EN		GENMASK(11, 8)	//0x0000_0F00 1111 
#define AUDIN_SOURCE_SEL_HDMI_SEL		GENMASK(14, 12)	//0x0000_7000 0111 

/* AUDIN registers offsets */
/* Registers offset from AUDIN_FIFO0_START*/
#define AUDIN_FIFO_START_OFF			0x00
#define AUDIN_FIFO_END_OFF			(AUDIN_FIFO0_END  - AUDIN_FIFO0_START)
#define AUDIN_FIFO_PTR_OFF			(AUDIN_FIFO0_PTR  - AUDIN_FIFO0_START)
#define AUDIN_FIFO_INTR_OFF			(AUDIN_FIFO0_INTR - AUDIN_FIFO0_START)
#define AUDIN_FIFO_CTRL_OFF			(AUDIN_FIFO0_CTRL - AUDIN_FIFO0_START)

#endif /* _MESON_AUDIO_H */

