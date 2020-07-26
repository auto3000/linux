/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __DT_MESON_AUDIO_H
#define __DT_MESON_AUDIO_H

#define AUDIO_CPU		0
#define AUDIO_CODEC		1
#define AUDIO_HDMI		2
#define AUDIO_ACODEC		3

#define CPU_I2S_FIFO		0
#define CPU_I2S_ENCODER		1
#define CPU_I2S_FIFO_DECODE	2
#define CPU_I2S_DECODER		3
//#define CPU_SPDIF_FIFO	4
//#define CPU_SPDIF_ENCODER	5
//#define CPU_SPDIF_FIFO_DECODE	6
//#define CPU_SPDIF_DECODER	7

#define CTRL_I2S		0
#define CTRL_OUT		1
#define CODEC_IN		2
#define CODEC_OUT		3
//#define CTRL_PCM		4
//#define CODEC_IN1		5
//#define CODEC_IN2		6
//#define CODEC_IN3		7

#endif /* __DT_MESON_AUDIO_H */
