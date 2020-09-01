/*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (c) 2020 Rezzonics
* Author: Rezzonics <rezzonics@gmail.com>
*/
#include <linux/bitfield.h>
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include <dt-bindings/sound/meson-audio.h>

#include "audio.h"
#include "meson-codec-glue.h"

#ifdef DEBUG_AUDIN
static const char * const audin_endian_texts[] = {
	"END0", "END1", "END2", "END3", "END4", "END5", "END6", "END7"
};

static int audin_endian_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_FIFO_CTRL_ENDIAN;
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_endian_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AUDIN_FIFO_CTRL_ENDIAN, mux);
	mask = AUDIN_FIFO_CTRL_ENDIAN;
	changed = regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_FIFO0_CTRL,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(audin_endian_enum, audin_endian_texts);

static const char * const audin_ch_texts[] = {
	"CH_0", "CH_1", "CH_2", "CH_3", "CH4", "CH5", "CH6", "CH_7",
	"CH8", "CH9", "CH10", "CH11", "CH12", "CH13", "CH14", "CH15",
};

static int audin_ch_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_FIFO_CTRL_CHAN;
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_ch_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AUDIN_FIFO_CTRL_CHAN, mux);
	mask = AUDIN_FIFO_CTRL_CHAN;
	changed = regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_FIFO0_CTRL,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(audin_ch_enum, audin_ch_texts);

static const char * const audin_dinpos_texts[] = {
	"DP0", "DP1", "DP2", "DP3"
};

static int audin_dinpos_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_FIFO_CTRL1_DINPOS;
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL1, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_dinpos_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AUDIN_FIFO_CTRL1_DINPOS, mux);
	mask = AUDIN_FIFO_CTRL1_DINPOS;
	changed = regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL1, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_FIFO0_CTRL1,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(audin_dinpos_enum, audin_dinpos_texts);

static const char * const audin_bytenum_texts[] = {
	"BN0", "BN1", "BN2", "BN3"
};

static int audin_bytenum_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_FIFO_CTRL1_DINBYTENUM;
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL1, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_bytenum_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AUDIN_FIFO_CTRL1_DINBYTENUM, mux);
	mask = AUDIN_FIFO_CTRL1_DINBYTENUM;
	changed = regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL1, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_FIFO0_CTRL1,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(audin_bytenum_enum, audin_bytenum_texts);

static int audin_dinpos2_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_FIFO_CTRL1_DINPOS2;
	regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL1, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_dinpos2_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AUDIN_FIFO_CTRL1_DINPOS2, mux);
	mask = AUDIN_FIFO_CTRL1_DINPOS2;
	changed = regmap_read(audio->audin_map, AUDIN_FIFO0_CTRL1, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_FIFO0_CTRL1,
			   mask,
			   val);
	return 0;
}

static int audin_lrclkinvt_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT;
	regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_lrclkinvt_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT, mux);
	mask = AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT;
	changed = regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_I2SIN_CTRL,
			   mask,
			   val);
	return 0;
}

static int audin_lrclkskew_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_I2SIN_CTRL_I2SIN_LRCLK_INVT;
	regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_lrclkskew_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW, mux);
	mask = AUDIN_I2SIN_CTRL_I2SIN_LRCLK_SKEW;
	changed = regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_I2SIN_CTRL,
			   mask,
			   val);
	return 0;
}

static int audin_possync_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC;
	regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_possync_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC, mux);
	mask = AUDIN_I2SIN_CTRL_I2SIN_POS_SYNC;
	changed = regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_I2SIN_CTRL,
			   mask,
			   val);
	return 0;
}

static const char * const audin_i2ssize_texts[] = {
	"SZ0", "SZ1", "SZ2", "SZ3"
};

static int audin_i2ssize_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_I2SIN_CTRL_I2SIN_SIZE;
	regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_i2ssize_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_SIZE, mux);
	mask = AUDIN_I2SIN_CTRL_I2SIN_SIZE;
	changed = regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_I2SIN_CTRL,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(audin_i2ssize_enum, audin_i2ssize_texts);

static const char * const audin_chen_texts[] = {
	"CHEN0", "CHEN1", "CHEN2", "CHEN3", "CHEN4", "CHEN5", "CHEN6", "CHEN7",
	"CHEN8", "CHEN9", "CHEN10", "CHEN11", "CHEN12", "CHEN13", "CHEN14", "CHEN15",
};

static int audin_chen_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN;
	regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int audin_chen_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN, mux);
	mask = AUDIN_I2SIN_CTRL_I2SIN_CHAN_EN;
	changed = regmap_read(audio->audin_map, AUDIN_I2SIN_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->audin_map, AUDIN_I2SIN_CTRL,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(audin_chen_enum, audin_chen_texts);
#endif
#ifdef DEBUG_AIU
static int aiu_msb_inv_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_I2S_SOURCE_DESC_MSB_INV;
	regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_msb_inv_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AIU_I2S_SOURCE_DESC_MSB_INV, mux);
	mask = AIU_I2S_SOURCE_DESC_MSB_INV;
	changed = regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_I2S_SOURCE_DESC,
			   mask,
			   val);
	return 0;
}

static int aiu_msb_ext_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_I2S_SOURCE_DESC_MSB_EXTEND;
	regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_msb_ext_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AIU_I2S_SOURCE_DESC_MSB_EXTEND, mux);
	mask = AIU_I2S_SOURCE_DESC_MSB_EXTEND;
	changed = regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_I2S_SOURCE_DESC,
			   mask,
			   val);
	return 0;
}

static int aiu_24_bit_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_I2S_SOURCE_DESC_MODE_24BIT;
	regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_24_bit_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AIU_I2S_SOURCE_DESC_MODE_24BIT, mux);
	mask = AIU_I2S_SOURCE_DESC_MODE_24BIT;
	changed = regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_I2S_SOURCE_DESC,
			   mask,
			   val);
	return 0;
}

static int aiu_32_bit_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_I2S_SOURCE_DESC_MODE_32BIT;
	regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_32_bit_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AIU_I2S_SOURCE_DESC_MODE_32BIT, mux);
	mask = AIU_I2S_SOURCE_DESC_MODE_32BIT;
	changed = regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_I2S_SOURCE_DESC,
			   mask,
			   val);
	return 0;
}

static const char * const aiu_msb_pos_texts[] = {
	"MSBPOS0", "MSBPOS1", "MSBPOS2", "MSBPOS3"
};

static int aiu_msb_pos_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_I2S_SOURCE_DESC_MSB_POS;
	regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_msb_pos_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AIU_I2S_SOURCE_DESC_MSB_POS, mux);
	mask = AIU_I2S_SOURCE_DESC_MSB_POS;
	changed = regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_I2S_SOURCE_DESC,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(aiu_msb_pos_enum, aiu_msb_pos_texts);

static const char * const aiu_shft_bits_texts[] = {
	"SHFTBIT0", "SHFTBIT1", "SHFTBIT2", "SHFTBIT3",
	"SHFTBIT4", "SHFTBIT5", "SHFTBIT6", "SHFTBIT7"
};

static int aiu_shft_bits_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_I2S_SOURCE_DESC_SHIFT_BITS;
	regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_shft_bits_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AIU_I2S_SOURCE_DESC_SHIFT_BITS, mux);
	mask = AIU_I2S_SOURCE_DESC_SHIFT_BITS;
	changed = regmap_read(audio->aiu_map, AIU_I2S_SOURCE_DESC, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_I2S_SOURCE_DESC,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(aiu_shft_bits_enum, aiu_shft_bits_texts);

static int aiu_aoclkinvt_get_enum(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_CLK_CTRL_AOCLK_INVERT;
	regmap_read(audio->aiu_map, AIU_CLK_CTRL, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_aoclkinvt_put_enum(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AIU_CLK_CTRL_AOCLK_INVERT, mux);
	mask = AIU_CLK_CTRL_AOCLK_INVERT;
	changed = regmap_read(audio->aiu_map, AIU_CLK_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_CLK_CTRL,
			   mask,
			   val);
	return 0;
}

static int aiu_lrclkinvt_get_enum(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_CLK_CTRL_LRCLK_INVERT;
	regmap_read(audio->aiu_map, AIU_CLK_CTRL, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_lrclkinvt_put_enum(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AIU_CLK_CTRL_LRCLK_INVERT, mux);
	mask = AIU_CLK_CTRL_LRCLK_INVERT;
	changed = regmap_read(audio->aiu_map, AIU_CLK_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_CLK_CTRL,
			   mask,
			   val);
	return 0;
}

static const char * const aiu_lrclkskew_texts[] = {
	"SKEW0", "SKEW1", "SKEW2", "SKEW3"
};

static int aiu_lrclkskew_get_enum(struct snd_kcontrol *kcontrol,
			         struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_CLK_CTRL_LRCLK_SKEW;
	regmap_read(audio->aiu_map, AIU_CLK_CTRL, &val);
	ucontrol->value.enumerated.item[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_lrclkskew_put_enum(struct snd_kcontrol *kcontrol,
			          struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = snd_soc_enum_item_to_val(e, ucontrol->value.enumerated.item[0]);
	val = FIELD_PREP(AIU_CLK_CTRL_LRCLK_SKEW, mux);
	mask = AIU_CLK_CTRL_LRCLK_SKEW;
	changed = regmap_read(audio->aiu_map, AIU_CLK_CTRL, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_CLK_CTRL,
			   mask,
			   val);
	return 0;
}

static SOC_ENUM_SINGLE_EXT_DECL(aiu_lrclkskew_enum, aiu_lrclkskew_texts);
#endif

static int aiu_swap_get_enum(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int val, mask;

	mask = AIU_I2S_MUTE_SWAP_01;
	regmap_read(audio->aiu_map, AIU_I2S_MUTE_SWAP, &val);
	ucontrol->value.integer.value[0] = (val & mask) >> __ffs(mask);
	return 0;
}

static int aiu_swap_put_enum(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct audio *audio = snd_soc_component_get_drvdata(component);
	unsigned int mux, val, mask, old, new;
	int changed;

	mux = ucontrol->value.integer.value[0];
	val = FIELD_PREP(AIU_I2S_MUTE_SWAP_01, mux);
	mask = AIU_I2S_MUTE_SWAP_01;
	changed = regmap_read(audio->aiu_map, AIU_I2S_MUTE_SWAP, &old);
	new = (old & ~mask) | val;
	changed = old != new;

	if (!changed)
		return 0;
	regmap_update_bits(audio->aiu_map, AIU_I2S_MUTE_SWAP,
			   mask,
			   val);
	return 0;
}

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
static const struct snd_kcontrol_new audio_codec_ctrl_controls[] = {
#ifdef DEBUG_AUDIN
	SOC_ENUM_EXT("Endian", audin_endian_enum, 
		     audin_endian_get_enum, audin_endian_put_enum),
	SOC_ENUM_EXT("Channel", audin_ch_enum, 
		     audin_ch_get_enum, audin_ch_put_enum),
	SOC_ENUM_EXT("DinPos", audin_dinpos_enum, 
		     audin_dinpos_get_enum, audin_dinpos_put_enum),
	SOC_ENUM_EXT("ByteNum", audin_bytenum_enum, 
		     audin_bytenum_get_enum, audin_bytenum_put_enum),
	SOC_SINGLE_BOOL_EXT("DinPos2", 0, 
			    audin_dinpos2_get_enum, audin_dinpos2_put_enum),
	SOC_SINGLE_BOOL_EXT("LrclkInvt", 0, 
			    audin_lrclkinvt_get_enum, audin_lrclkinvt_put_enum),
	SOC_SINGLE_BOOL_EXT("LrclkSkew", 0, 
			    audin_lrclkskew_get_enum, audin_lrclkskew_put_enum),
	SOC_SINGLE_BOOL_EXT("PosSync", 0, 
			    audin_possync_get_enum, audin_possync_put_enum),
	SOC_ENUM_EXT("I2SSize", audin_i2ssize_enum, 
		     audin_i2ssize_get_enum, audin_i2ssize_put_enum),
	SOC_ENUM_EXT("ChEn", audin_chen_enum, 
		     audin_chen_get_enum, audin_chen_put_enum),
#endif
#ifdef DEBUG_AIU
	SOC_SINGLE_BOOL_EXT("MSB_Inv", 0, 
			    aiu_msb_inv_get_enum, aiu_msb_inv_put_enum),
	SOC_SINGLE_BOOL_EXT("MSB_Ext", 0, 
			    aiu_msb_ext_get_enum, aiu_msb_ext_put_enum),
	SOC_ENUM_EXT("MSB_Pos", aiu_msb_pos_enum, 
		     aiu_msb_pos_get_enum, aiu_msb_pos_put_enum),
	SOC_SINGLE_BOOL_EXT("24_bit", 0, 
			    aiu_24_bit_get_enum, aiu_24_bit_put_enum),
	SOC_ENUM_EXT("Shft_Bits", aiu_shft_bits_enum, 
		     aiu_shft_bits_get_enum, aiu_shft_bits_put_enum),
	SOC_SINGLE_BOOL_EXT("32_bit", 0, 
			    aiu_32_bit_get_enum, aiu_32_bit_put_enum),
	SOC_SINGLE_BOOL_EXT("AoClkInvt", 0, 
			    aiu_aoclkinvt_get_enum, aiu_aoclkinvt_put_enum),
	SOC_SINGLE_BOOL_EXT("LrClkInvt", 0, 
			    aiu_lrclkinvt_get_enum, aiu_lrclkinvt_put_enum),
	SOC_ENUM_EXT("LrClkSkew", aiu_lrclkskew_enum, 
		     aiu_lrclkskew_get_enum, aiu_lrclkskew_put_enum),
#endif
	SOC_SINGLE_BOOL_EXT("Swap", 0, 
			    aiu_swap_get_enum, aiu_swap_put_enum),
};
static const struct snd_soc_dapm_widget audio_codec_ctrl_widgets[] = {
	SND_SOC_DAPM_AIF_IN("I2S IN Playback", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUTPUT("CODEC OUT Playback"),

	SND_SOC_DAPM_INPUT("CODEC IN Capture"),
	SND_SOC_DAPM_AIF_OUT("I2S OUT Capture", "Capture",  0, SND_SOC_NOPM, 0, 0),

};

static struct snd_soc_dai_driver audio_codec_ctrl_dai_drv[] = {
	[CTRL_I2S]  = AIU_CODEC_CTRL_INPUT("I2S IN", 2),
	[CTRL_OUT]  = AIU_CODEC_CTRL_OUTPUT("CODEC OUT", 2),
	[CODEC_IN]  = AUDIN_CODEC_CTRL_INPUT("CODEC IN", 2),
	[CODEC_OUT] = AUDIN_CODEC_CTRL_OUTPUT("I2S OUT", 2),
};

static const struct snd_soc_dapm_route audio_codec_ctrl_routes[] = {
	{ "CODEC OUT Playback", NULL, "I2S IN Playback" },
	{ "I2S OUT Capture", NULL, "CODEC IN Capture" },
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
	.controls		= audio_codec_ctrl_controls,
	.num_controls		= ARRAY_SIZE(audio_codec_ctrl_controls),
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

