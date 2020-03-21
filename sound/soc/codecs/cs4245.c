// SPDX-License-Identifier: GPL-2.0-only
/*
 * cs4245.c -- CS4245 ALSA SoC audio driver
 *
 * Author: Rezzonics <rezzonics@gmail.com>
 *
 * cs4245.c -- CS4245 ALSA SoC audio driver
 *
 * Notes:
 *
 * Based on cs4265.c ALSA SoC audio driver
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "cs4245.h"

//----------------------------------------------------------------------
#ifdef _MULTI_FX_EXT
	#include <linux/interrupt.h>
#endif

// GPIO macros
#define CHANNEL_LEFT    0
#define CHANNEL_RIGHT   1

#ifdef _MULTI_FX_EXT
	#define EXP_PEDAL_SIGNAL_ON  false

	enum input_imp {
		LINE = 0,	//  1M
		INST,		// 75K
		MIC,		//  3K
		RSVD		// <3K
	};
#endif

static bool left_true_bypass = false;
static bool right_true_bypass = false;

#ifdef _MULTI_FX_EXT
	static int input_left_gain = 0;
	static int input_right_gain = 0;
	static int input_left_impedance = LINE;
	static int input_right_impedance = LINE;
	/* Headphone volume has a total of 16 steps, each corresponds to 3dB. Step 11 is 0dB*/
	static int headphone_volume = 0; 
	static bool enable_headphone = true;
	/* true means expression pedal mode, false is CV mode (CV input mode)*/
	static bool cv_exp_pedal_mode = false; 
	static bool exp_pedal_mode = EXP_PEDAL_SIGNAL_ON;
#endif

static struct _multi_fx_gpios {
	struct gpio_desc *true_bypass_left;
	struct gpio_desc *true_bypass_right;
#ifdef _MULTI_FX_EXT
	struct gpio_desc *gain_left_low;
	struct gpio_desc *gain_left_mid;
	struct gpio_desc *gain_right_low;
	struct gpio_desc *gain_right_mid;
	struct gpio_desc *zmic_left;
	struct gpio_desc *zinst_left;
	struct gpio_desc *zmic_right;
	struct gpio_desc *zinst_right;
	struct gpio_desc *headphone_enable;
	struct gpio_desc *headphone_clk;
	struct gpio_desc *headphone_dir;
	struct gpio_desc *exp_enable;
	struct gpio_desc *cv_in_bias;
	struct gpio_desc *exp_flag1;
	struct gpio_desc *exp_flag2;
//	int irqFlag1, irqFlag2;
#endif
} *multi_fx_gpios;

#ifdef _MULTI_FX_EXT
	static void set_cv_exp_pedal_mode(int mode);
#endif

/*
static irq_handler_t exp_flag_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
	printk(KERN_INFO "FX Devices: Expression Pedal flag IRQ %u triggered! (values are %d %d)\n",
	       irq,
	       gpiod_get_value(multi_fx_gpios->exp_flag1),
	       gpiod_get_value(multi_fx_gpios->exp_flag2));
	set_cv_exp_pedal_mode(0);
	return IRQ_HANDLED;
}
*/

static int multi_fx_init(struct i2c_client *i2c_client)
{

	multi_fx_gpios = devm_kzalloc(&i2c_client->dev, sizeof(struct _multi_fx_gpios), GFP_KERNEL);
	if (multi_fx_gpios == NULL)
		return -ENOMEM;

	/* True Bypass gpios */
	multi_fx_gpios->true_bypass_left  = devm_gpiod_get(&i2c_client->dev, 
					    "true_bypass_left", GPIOD_OUT_HIGH);
	multi_fx_gpios->true_bypass_right = devm_gpiod_get(&i2c_client->dev, 
					    "true_bypass_right", GPIOD_OUT_HIGH);
#ifdef _MULTI_FX_EXT
	int i;

	/* Gain GPIOs are active low*/
	multi_fx_gpios->gain_left_low  = devm_gpiod_get(&i2c_client->dev, 
					    "gain_left_low", GPIOD_OUT_LOW);
	multi_fx_gpios->gain_left_mid  = devm_gpiod_get(&i2c_client->dev, 
					    "gain_left_mid", GPIOD_OUT_LOW);
	multi_fx_gpios->gain_right_low = devm_gpiod_get(&i2c_client->dev, 
					    "gain_right_low", GPIOD_OUT_LOW);
	multi_fx_gpios->gain_right_mid = devm_gpiod_get(&i2c_client->dev, 
					    "gain_right_mid", GPIOD_OUT_LOW);
	/* Input impedance gpios */
	multi_fx_gpios->zmic_left   = devm_gpiod_get(&i2c_client->dev, 
					    "zmic_left", GPIOD_OUT_HIGH);
	multi_fx_gpios->zmic_right  = devm_gpiod_get(&i2c_client->dev, 
					    "zmic_left", GPIOD_OUT_HIGH);
	multi_fx_gpios->zinst_left  = devm_gpiod_get(&i2c_client->dev, 
					    "zinst_left", GPIOD_OUT_HIGH);
	multi_fx_gpios->zinst_right = devm_gpiod_get(&i2c_client->dev, 
					    "zinst_right", GPIOD_OUT_HIGH);
	/* Headphone gpios */
	multi_fx_gpios->headphone_enable  = devm_gpiod_get(&i2c_client->dev, 
					    "headphone_enable", GPIOD_OUT_HIGH);
	multi_fx_gpios->headphone_clk     = devm_gpiod_get(&i2c_client->dev, 
					    "headphone_clk", GPIOD_OUT_HIGH);
	multi_fx_gpios->headphone_dir     = devm_gpiod_get(&i2c_client->dev, 
					    "headphone_dir", GPIOD_OUT_HIGH);
	/* Expression pedal gpios */
	multi_fx_gpios->exp_enable        = devm_gpiod_get(&i2c_client->dev, 
					    "exp_enable", GPIOD_OUT_HIGH);
	multi_fx_gpios->cv_in_bias        = devm_gpiod_get(&i2c_client->dev,
					    "cv_in_bias", GPIOD_OUT_HIGH);
	multi_fx_gpios->exp_flag1         = devm_gpiod_get(&i2c_client->dev,
					    "exp_flag1", GPIOD_IN);
	multi_fx_gpios->exp_flag2         = devm_gpiod_get(&i2c_client->dev, 
					    "exp_flag2", GPIOD_IN);
	/* Gain bits are active low: 1 => assert = low level, 0 => deassert = high level
			      ML
		High gain   = 11 => 24.3 dB
		Mid gain    = 10 => 11.8 dB
		Low gain    = 01 => -0.7 dB
		Lowest gain = 00 => -1.0 dB
	*/
	gpiod_set_value(multi_fx_gpios->gain_left_low, 0);
	gpiod_set_value(multi_fx_gpios->gain_left_mid, 0);
	gpiod_set_value(multi_fx_gpios->gain_right_low, 0);
	gpiod_set_value(multi_fx_gpios->gain_right_mid, 0);

	/* Init Input impedance gpios */
	gpiod_set_value(multi_fx_gpios->zmic_left, 0);
	gpiod_set_value(multi_fx_gpios->zinst_left, 0);
	gpiod_set_value(multi_fx_gpios->zmic_right, 0);
	gpiod_set_value(multi_fx_gpios->zinst_right, 0);

	/* put headphone volume to lowest setting, so we know where we are */
	gpiod_set_value(multi_fx_gpios->headphone_dir, 0);

	for (i = 0; i < 16; i++) {
		/* toggle clock in order to sample the volume pin upon clock's rising edge */
		gpiod_set_value(multi_fx_gpios->headphone_clk, 1);
		gpiod_set_value(multi_fx_gpios->headphone_clk, 0);
	}

	/* initialize gpios*/
	gpiod_set_value(multi_fx_gpios->headphone_enable, enable_headphone);
	gpiod_set_value(multi_fx_gpios->cv_in_bias, 0); // 0 to 5v
	gpiod_set_value(multi_fx_gpios->exp_enable, 0);
#endif
	return 0;
}

/* state == 1 => bypass:
 * No audio processing.
 * Input is connected directly to output, bypassing the codec.
 * INPUT => OUTPUT
 *
 * state == 0 => process:
 * INPUT => CODEC => OUTPUT
 */
static void set_true_bypass(int channel, bool state)
{
	switch (channel) {
	case CHANNEL_LEFT:
		gpiod_set_value(multi_fx_gpios->true_bypass_left, state);
		left_true_bypass = state;
		break;
	case CHANNEL_RIGHT:
		gpiod_set_value(multi_fx_gpios->true_bypass_right, state);
		right_true_bypass = state;
		break;
	}
}

#ifdef _MULTI_FX_EXT
static const unsigned int gains_tlv[] = {
    TLV_DB_RANGE_HEAD(4),
    0, 0, TLV_DB_SCALE_ITEM(0,  0, 0),
    1, 1, TLV_DB_SCALE_ITEM(7, 0, 0),
    2, 2, TLV_DB_SCALE_ITEM(118, 0, 0),
    3, 3, TLV_DB_SCALE_ITEM(243, 0, 0),
};

static void set_gain(int channel, int state)
{
	struct gpio_desc *gpio1, *gpio2;

	switch (channel) {
	case CHANNEL_LEFT:
		gpio1 = multi_fx_gpios->gain_left_low;
		gpio2 = multi_fx_gpios->gain_left_mid;
		input_left_gain = state;
		break;
	case CHANNEL_RIGHT:
		gpio1 = multi_fx_gpios->gain_right_low;
		gpio2 = multi_fx_gpios->gain_right_mid;
		input_right_gain = state;
		break;
	default:
		return;
	}

	switch (state) {
	case 0:
		gpiod_set_value(gpio1, 0);
		gpiod_set_value(gpio2, 0);
		break;
	case 1:
		gpiod_set_value(gpio1, 0);
		gpiod_set_value(gpio2, 1);
		break;
	case 2:
		gpiod_set_value(gpio1, 1);
		gpiod_set_value(gpio2, 0);
		break;
	case 3:
		gpiod_set_value(gpio1, 1);
		gpiod_set_value(gpio2, 1);
		break;
	}
}

static void set_impedance(int channel, enum input_imp state)
{
	struct gpio_desc *gpio1, *gpio2;

	switch (channel) {
	case CHANNEL_LEFT:
		gpio1 = multi_fx_gpios->zmic_left;
		gpio2 = multi_fx_gpios->zinst_left;
		input_left_impedance = state;
		break;
	case CHANNEL_RIGHT:
		gpio1 = multi_fx_gpios->zmic_right;
		gpio2 = multi_fx_gpios->zinst_right;
		input_right_impedance = state;
		break;
	default:
		return;
	}

	switch (state) {
	case LINE:
		gpiod_set_value(gpio1, 0);
		gpiod_set_value(gpio2, 0);
		break;
	case INST:
		gpiod_set_value(gpio1, 0);
		gpiod_set_value(gpio2, 1);
		break;
	case MIC:
		gpiod_set_value(gpio1, 1);
		gpiod_set_value(gpio2, 0);
		break;
	case RSVD:
		gpiod_set_value(gpio1, 1);
		gpiod_set_value(gpio2, 1);
		break;
	}
}

/* This routine flips the GPIO pins to send the volume adjustment
   message to the actual headphone gain-control chip (LM4811) */
static void set_headphone_volume(int new_volume)
{
	int i;
	int steps = abs(new_volume - headphone_volume);

	// select volume adjustment direction
	gpiod_set_value(multi_fx_gpios->headphone_dir, new_volume > headphone_volume ? 1 : 0);

	for (i=0; i < steps; i++) {
		// toggle clock in order to sample the volume pin upon clock's rising edge
		gpiod_set_value(multi_fx_gpios->headphone_clk, 1);
		gpiod_set_value(multi_fx_gpios->headphone_clk, 0);
	}

	headphone_volume = new_volume;
}

static void set_headphone_enable(bool state)
{
	gpiod_set_value(multi_fx_gpios->headphone_enable, state);
}

static void set_exp_pedal_mode(int mode)
{
	switch (mode) {
	case 0:
	case 1:
		if (cv_exp_pedal_mode)
		{
			/*
			if (multi_fx_gpios->irqFlag1 <= 0 || multi_fx_gpios->irqFlag2 <= 0)
			{
				printk("Multi FX: set_exp_pedal_mode(%i) call ignored, as Expression Pedal flag IRQ failed before\n");
			}
			else */
			if (mode == (int)EXP_PEDAL_SIGNAL_ON)
			{
				gpiod_set_value(multi_fx_gpios->exp_enable, 1);
			}
		}
		exp_pedal_mode = mode != 0;
		break;
	default:
		break;
	}
}

static void set_cv_exp_pedal_mode(int mode)
{
	switch (mode) {
	case 0: // cv mode
		cv_exp_pedal_mode = false;
		gpiod_set_value(multi_fx_gpios->exp_enable, 0);
		break;

	case 1: // exp.pedal mode
		cv_exp_pedal_mode = true;
		set_exp_pedal_mode(exp_pedal_mode);
		break;

	default:
		break;
	}
}
#endif
//----------------------------------------------------------------------

static int true_bypass_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int left_true_bypass_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = left_true_bypass;
	return 0;
}

static int right_true_bypass_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = right_true_bypass;
	return 0;
}

static int left_true_bypass_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (left_true_bypass != ucontrol->value.integer.value[0]) {
		set_true_bypass(CHANNEL_LEFT, ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

static int right_true_bypass_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (right_true_bypass != ucontrol->value.integer.value[0]) {
		set_true_bypass(CHANNEL_RIGHT, ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

#ifdef _MULTI_FX_EXT
//----------------------------------------------------------------------

static int input_gain_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 3;
	return 0;
}

static int input_left_gain_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = input_left_gain;
	return 0;
}

static int input_right_gain_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = input_right_gain;
	return 0;
}

static int input_left_gain_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (input_left_gain != ucontrol->value.integer.value[0]) {
		set_gain(CHANNEL_LEFT, ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

static int input_right_gain_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (input_right_gain != ucontrol->value.integer.value[0]) {
		set_gain(CHANNEL_RIGHT, ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

//----------------------------------------------------------------------

static int input_impedance_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 3;
	return 0;
}

static int input_left_impedance_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = input_left_impedance;
	return 0;
}

static int input_right_impedance_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = input_right_impedance;
	return 0;
}

static int input_left_impedance_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (input_left_impedance != ucontrol->value.integer.value[0]) {
		set_impedance(CHANNEL_LEFT, ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

static int input_right_impedance_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (input_right_impedance != ucontrol->value.integer.value[0]) {
		set_impedance(CHANNEL_RIGHT, ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

//----------------------------------------------------------------------

static int headphone_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 15;
	return 0;
}

static int headphone_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = headphone_volume;
	return 0;
}

static int headphone_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (headphone_volume != ucontrol->value.integer.value[0]) {
		set_headphone_volume(ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

//----------------------------------------------------------------------

static int headphone_enable_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int headphone_enable_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = enable_headphone;
	return 0;
}

static int headphone_enable_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (enable_headphone != ucontrol->value.integer.value[0]) {
		set_headphone_enable(ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

//----------------------------------------------------------------------

static int cv_exp_pedal_mode_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int cv_exp_pedal_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = cv_exp_pedal_mode;
	return 0;
}

static int cv_exp_pedal_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (cv_exp_pedal_mode != ucontrol->value.integer.value[0]) {
		set_cv_exp_pedal_mode(ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}

//----------------------------------------------------------------------

static int exp_pedal_mode_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int exp_pedal_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = exp_pedal_mode;
	return 0;
}

static int exp_pedal_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	if (exp_pedal_mode != ucontrol->value.integer.value[0]) {
		set_exp_pedal_mode(ucontrol->value.integer.value[0]);
		changed = 1;
	}
	return changed;
}
#endif

//----------------------------------------------------------------------

struct cs4245_private {
	struct regmap *regmap;
	struct gpio_desc *reset_gpio;
	u8 format;
	u32 sysclk;
};

static const struct reg_default cs4245_reg_defaults[] = {
	{ CS4245_PWRCTL, 	0x0F },  // HW default is 0x01
	{ CS4245_DAC_CTL, 	0x08 },
	{ CS4245_ADC_CTL, 	0x00 },
	{ CS4245_MCLK_FREQ, 	0x00 },
	{ CS4245_SIG_SEL, 	0x40 },
	{ CS4245_CHB_PGA_CTL, 	0x00 },
	{ CS4245_CHA_PGA_CTL, 	0x00 },
	{ CS4245_ADC_CTL2, 	0x19 },
	{ CS4245_DAC_CHA_VOL, 	0x00 },
	{ CS4245_DAC_CHB_VOL, 	0x00 },
	{ CS4245_DAC_CTL2, 	0xC0 },
	{ CS4245_INT_MASK, 	0x00 },
	{ CS4245_STATUS_MODE_MSB, 0x00 },
	{ CS4245_STATUS_MODE_LSB, 0x00 },
};

static bool cs4245_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS4245_CHIP_ID ... CS4245_MAX_REGISTER:
		return true;
	default:
		return false;
	}
}

static bool cs4245_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS4245_INT_STATUS:
		return true;
	default:
		return false;
	}
}

/* PGA scale -12.0 to +12.0dB in 0.5dB steps => -24 to 24 */
/* Original cs4265 scale goes from -12 to 0dB
static DECLARE_TLV_DB_SCALE(pga_tlv, -1200, 50, 0); 
*/
static DECLARE_TLV_DB_SCALE(pga_tlv, -1200, 50, 0);

/* DAC VOL scale 0 to -127.5dB in 0.5dB steps => 0 to 255 */
static DECLARE_TLV_DB_SCALE(dac_tlv, -12750, 50, 0);

/* Added to CS4245: AUX Analog Output mux on Sig Sel[5] */ 
static const char * const aux_analog_output_mux_text[] = {
	"HI_IMP", "DAC", "ADC", "RSV"
};

static SOC_ENUM_SINGLE_DECL(aux_analog_output_mux_enum, CS4245_SIG_SEL, 5,
		aux_analog_output_mux_text);

static const struct snd_kcontrol_new aux_analog_output_mux =
	SOC_DAPM_ENUM("AUX Analog Output Mux", aux_analog_output_mux_enum);

static const char * const mic_linein_text[] = {
	"MIC", "LINEIN1", "LINEIN2", "LINEIN3", "LINEIN4", "LINEIN5", "LINEIN6", "RSV"
};

static SOC_ENUM_SINGLE_DECL(mic_linein_enum, CS4245_ADC_CTL2, 0,
		mic_linein_text);

static const struct snd_kcontrol_new mic_linein_mux =
	SOC_DAPM_ENUM("ADC Input Capture Mux", mic_linein_enum);

static const struct snd_kcontrol_new loopback_ctl =
	SOC_DAPM_SINGLE("Loopback Switch", CS4245_SIG_SEL, 1, 1, 0);

static const struct snd_kcontrol_new dac_switch =
	SOC_DAPM_SINGLE("DAC Switch", CS4245_PWRCTL, 1, 1, 0);

static const struct snd_kcontrol_new cs4245_snd_controls[] = {

//Replaced min, max vakues 0x28, 0x30 by -24, 24
	SOC_DOUBLE_R_SX_TLV("PGA Volume", CS4245_CHA_PGA_CTL,
			      CS4245_CHB_PGA_CTL, 0, 0x28, 0x30, pga_tlv), 
	SOC_DOUBLE_R_TLV("DAC Volume", CS4245_DAC_CHA_VOL,
		      CS4245_DAC_CHB_VOL, 0, 0xFF, 1, dac_tlv), // 0xFF, 1
	SOC_SINGLE("De-emp 44.1kHz Switch", CS4245_DAC_CTL, 1, 1, 0),
	SOC_SINGLE("DAC INV Switch", CS4245_DAC_CTL2, 5, 1, 0),
	SOC_SINGLE("DAC Zero Cross Switch", CS4245_DAC_CTL2, 6, 1, 0),
	SOC_SINGLE("DAC Soft Ramp Switch", CS4245_DAC_CTL2, 7, 1, 0),
	SOC_SINGLE("ADC HPF Switch", CS4245_ADC_CTL, 1, 1, 0),
	SOC_SINGLE("ADC Zero Cross Switch", CS4245_ADC_CTL2, 3, 1, 1),
	SOC_SINGLE("ADC Soft Ramp", CS4245_ADC_CTL2, 4, 1, 0),

	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Left True-Bypass",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = true_bypass_info,
		.get = left_true_bypass_get,
		.put = left_true_bypass_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Right True-Bypass",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = true_bypass_info,
		.get = right_true_bypass_get,
		.put = right_true_bypass_put
	},
#ifdef _MULTI_FX_EXT
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Headphone Playback Volume",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = headphone_info,
		.get = headphone_get,
		.put = headphone_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Left Gain Stage",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = input_gain_info,
		.get = input_left_gain_get,
		.put = input_left_gain_put,
		.tlv.p = gains_tlv
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Right Gain Stage",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = input_gain_info,
		.get = input_right_gain_get,
		.put = input_right_gain_put,
		.tlv.p = gains_tlv
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Left Impedance",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = input_impedance_info,
		.get = input_left_impedance_get,
		.put = input_left_impedance_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Right Impedance",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = input_impedance_info,
		.get = input_right_impedance_get,
		.put = input_right_impedance_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Headphone Enable",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = headphone_enable_info,
		.get = headphone_enable_get,
		.put = headphone_enable_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "CV/Exp.Pedal Mode",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = cv_exp_pedal_mode_info,
		.get = cv_exp_pedal_mode_get,
		.put = cv_exp_pedal_mode_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Exp.Pedal Mode",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = exp_pedal_mode_info,
		.get = exp_pedal_mode_get,
		.put = exp_pedal_mode_put
	},
#endif
};

static const struct snd_soc_dapm_widget cs4245_dapm_widgets[] = {

	SND_SOC_DAPM_INPUT("LINEIN1L"),
	SND_SOC_DAPM_INPUT("LINEIN1R"),
	SND_SOC_DAPM_INPUT("LINEIN2L"),
	SND_SOC_DAPM_INPUT("LINEIN2R"),
	SND_SOC_DAPM_INPUT("LINEIN3L"),
	SND_SOC_DAPM_INPUT("LINEIN3R"),
	SND_SOC_DAPM_INPUT("LINEIN4L"),
	SND_SOC_DAPM_INPUT("LINEIN4R"),
	SND_SOC_DAPM_INPUT("LINEIN5L"),
	SND_SOC_DAPM_INPUT("LINEIN5R"),
	SND_SOC_DAPM_INPUT("LINEIN6L"),
	SND_SOC_DAPM_INPUT("LINEIN6R"),
	SND_SOC_DAPM_INPUT("MICL"),
	SND_SOC_DAPM_INPUT("MICR"),

	SND_SOC_DAPM_AIF_OUT("DOUT", NULL,  0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX("ADC Mux", SND_SOC_NOPM, 0, 0, &mic_linein_mux),

	SND_SOC_DAPM_ADC("ADC", NULL, CS4245_PWRCTL, 2, 1),

	SND_SOC_DAPM_PGA("Pre-amp MIC", CS4245_PWRCTL, 3, 1, NULL, 0),

	SND_SOC_DAPM_SWITCH("Loopback", SND_SOC_NOPM, 0, 0, &loopback_ctl),

	SND_SOC_DAPM_SWITCH("DAC", CS4245_PWRCTL, 1, 1, &dac_switch),

	SND_SOC_DAPM_AIF_IN("DIN1", NULL,  0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX("Aux Output Mux", SND_SOC_NOPM, 0, 0, &aux_analog_output_mux),

	SND_SOC_DAPM_OUTPUT("LINEOUTL"),
	SND_SOC_DAPM_OUTPUT("LINEOUTR"),
	SND_SOC_DAPM_OUTPUT("AUXOUTL"),
	SND_SOC_DAPM_OUTPUT("AUXOUTR"),
};

static const struct snd_soc_dapm_route cs4245_audio_map[] = {

	{"DIN1", NULL, "DAI1 Playback"},
	{"DAC", "DAC Switch", "DIN1"},
	{"LINEOUTL", NULL, "DAC"},
	{"LINEOUTR", NULL, "DAC"},
	{"Aux Output Mux", "HI_IMP", "DAC"},
	{"Aux Output Mux", "DAC", "DAC"},
	{"Aux Output Mux", "ADC", "ADC"},
	{"Aux Output Mux", "RSV", "DAC"},
	{"AUXOUTL", NULL, "Aux Output Mux"},
	{"AUXOUTR", NULL, "Aux Output Mux"},

	{"Pre-amp MIC", NULL, "MICL"},
	{"Pre-amp MIC", NULL, "MICR"},
	{"ADC Mux", "MIC", "Pre-amp MIC"},
	{"ADC Mux", "LINEIN1", "LINEIN1L"},
	{"ADC Mux", "LINEIN1", "LINEIN1R"},
	{"ADC Mux", "LINEIN2", "LINEIN2L"},
	{"ADC Mux", "LINEIN2", "LINEIN2R"},
	{"ADC Mux", "LINEIN3", "LINEIN3L"},
	{"ADC Mux", "LINEIN3", "LINEIN3R"},
	{"ADC Mux", "LINEIN4", "LINEIN4L"},
	{"ADC Mux", "LINEIN4", "LINEIN4R"},
	{"ADC Mux", "LINEIN5", "LINEIN5L"},
	{"ADC Mux", "LINEIN6", "LINEIN5R"},
	{"ADC Mux", "LINEIN6", "LINEIN6L"},
	{"ADC Mux", "LINEIN6", "LINEIN6R"},
	{"ADC", NULL, "ADC Mux"},
	{"DOUT", NULL, "ADC"},
	{"DAI1 Capture", NULL, "DOUT"},

	/* Loopback */
	{"Loopback", "Loopback Switch", "ADC"},
	{"DAC", NULL, "Loopback"},
};

struct cs4245_clk_para {
	u32 mclk;
	u32 rate;
	u8 fm_mode; /* values 1, 2, or 4 */
	u8 mclkdiv;
};

static const struct cs4245_clk_para clk_map_table[] = {
	/*32k*/
	{8192000, 32000, 0, 0},
	{12288000, 32000, 0, 1},
	{16384000, 32000, 0, 2},
	{24576000, 32000, 0, 3},
	{32768000, 32000, 0, 4},

	/*44.1k*/
	{11289600, 44100, 0, 0},
	{16934400, 44100, 0, 1},
	{22579200, 44100, 0, 2},
	{33868800, 44100, 0, 3},
	{45158400, 44100, 0, 4},

	/*48k*/
	{12288000, 48000, 0, 0},
	{18432000, 48000, 0, 1},
	{24576000, 48000, 0, 2},
	{36864000, 48000, 0, 3},
	{49152000, 48000, 0, 4},

	/*64k*/
	{8192000, 64000, 1, 0},
	{12288000, 64000, 1, 1},
	{16934400, 64000, 1, 2},
	{24576000, 64000, 1, 3},
	{32768000, 64000, 1, 4},

	/* 88.2k */
	{11289600, 88200, 1, 0},
	{16934400, 88200, 1, 1},
	{22579200, 88200, 1, 2},
	{33868800, 88200, 1, 3},
	{45158400, 88200, 1, 4},

	/* 96k */
	{12288000, 96000, 1, 0},
	{18432000, 96000, 1, 1},
	{24576000, 96000, 1, 2},
	{36864000, 96000, 1, 3},
	{49152000, 96000, 1, 4},

	/* 128k */
	{8192000, 128000, 2, 0},
	{12288000, 128000, 2, 1},
	{16934400, 128000, 2, 2},
	{24576000, 128000, 2, 3},
	{32768000, 128000, 2, 4},

	/* 176.4k */
	{11289600, 176400, 2, 0},
	{16934400, 176400, 2, 1},
	{22579200, 176400, 2, 2},
	{33868800, 176400, 2, 3},
	{49152000, 176400, 2, 4},

	/* 192k */
	{12288000, 192000, 2, 0},
	{18432000, 192000, 2, 1},
	{24576000, 192000, 2, 2},
	{36864000, 192000, 2, 3},
	{49152000, 192000, 2, 4},
};

static int cs4245_get_clk_index(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(clk_map_table); i++) {
		if (clk_map_table[i].rate == rate &&
				clk_map_table[i].mclk == mclk)
			return i;
	}
	return -EINVAL;
}

static int cs4245_set_sysclk(struct snd_soc_dai *codec_dai, int clk_id,
			unsigned int freq, int dir)
{
	struct snd_soc_component *component = codec_dai->component;
	struct cs4245_private *cs4245 = snd_soc_component_get_drvdata(component);
	int i;

	if (clk_id != 0) {
		dev_err(component->dev, "Invalid clk_id %d\n", clk_id);
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(clk_map_table); i++) {
		if (clk_map_table[i].mclk == freq) {
			cs4245->sysclk = freq;
			return 0;
		}
	}
	cs4245->sysclk = 0;
	dev_err(component->dev, "Invalid freq parameter %d\n", freq);
	return -EINVAL;
}

static int cs4245_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct cs4245_private *cs4245 = snd_soc_component_get_drvdata(component);
	u8 iface = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		snd_soc_component_update_bits(component, CS4245_ADC_CTL,
				CS4245_ADC_MASTER,
				CS4245_ADC_MASTER);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		snd_soc_component_update_bits(component, CS4245_ADC_CTL,
				CS4245_ADC_MASTER,
				0);
		break;
	default:
		return -EINVAL;
	}

	 /* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= SND_SOC_DAIFMT_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= SND_SOC_DAIFMT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= SND_SOC_DAIFMT_LEFT_J;
		break;
	default:
		return -EINVAL;
	}

	cs4245->format = iface;
	return 0;
}

static int cs4245_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_component *component = dai->component;

	if (mute) {
		snd_soc_component_update_bits(component, CS4245_DAC_CTL,
			CS4245_DAC_CTL_MUTE,
			CS4245_DAC_CTL_MUTE);
	} else {
		snd_soc_component_update_bits(component, CS4245_DAC_CTL,
			CS4245_DAC_CTL_MUTE,
			0);
	}
	return 0;
}

static int cs4245_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct cs4245_private *cs4245 = snd_soc_component_get_drvdata(component);
	int index;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		((cs4245->format & SND_SOC_DAIFMT_FORMAT_MASK)
		== SND_SOC_DAIFMT_RIGHT_J))
		return -EINVAL;

	index = cs4245_get_clk_index(cs4245->sysclk, params_rate(params));
	if (index >= 0) {
		snd_soc_component_update_bits(component, CS4245_ADC_CTL,
			CS4245_ADC_FM, clk_map_table[index].fm_mode << 6);
		snd_soc_component_update_bits(component, CS4245_MCLK_FREQ,
			CS4245_MCLK_FREQ_MASK,
			clk_map_table[index].mclkdiv << 4);

	} else {
		dev_err(component->dev, "can't get correct mclk\n");
		return -EINVAL;
	}

	switch (cs4245->format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		snd_soc_component_update_bits(component, CS4245_DAC_CTL,
			CS4245_DAC_CTL_DIF, (1 << 4));
		snd_soc_component_update_bits(component, CS4245_ADC_CTL,
			CS4245_ADC_DIF, (1 << 4));
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		if (params_width(params) == 16) {
			snd_soc_component_update_bits(component, CS4245_DAC_CTL,
				CS4245_DAC_CTL_DIF, (2 << 4));
		} else {
			snd_soc_component_update_bits(component, CS4245_DAC_CTL,
				CS4245_DAC_CTL_DIF, (3 << 4));
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		snd_soc_component_update_bits(component, CS4245_DAC_CTL,
			CS4245_DAC_CTL_DIF, 0);
		snd_soc_component_update_bits(component, CS4245_ADC_CTL,
			CS4245_ADC_DIF, 0);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int cs4245_set_bias_level(struct snd_soc_component *component,
					enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		snd_soc_component_update_bits(component, CS4245_PWRCTL,
			CS4245_PWRCTL_PDN, 0);
		break;
	case SND_SOC_BIAS_STANDBY:
		snd_soc_component_update_bits(component, CS4245_PWRCTL,
			CS4245_PWRCTL_PDN,
			CS4245_PWRCTL_PDN);
		break;
	case SND_SOC_BIAS_OFF:
		snd_soc_component_update_bits(component, CS4245_PWRCTL,
			CS4245_PWRCTL_PDN,
			CS4245_PWRCTL_PDN);
		break;
	}
	return 0;
}

#define CS4245_RATES (SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_64000 | \
			SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_192000)

#define CS4245_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_U32_LE)

static const struct snd_soc_dai_ops cs4245_ops = {
	.hw_params	= cs4245_pcm_hw_params,
	.digital_mute	= cs4245_digital_mute,
	.set_fmt	= cs4245_set_fmt,
	.set_sysclk	= cs4245_set_sysclk,
};

static struct snd_soc_dai_driver cs4245_dai[] = {
	{
		.name = "cs4245-dai",
		.playback = {
			.stream_name = "DAI1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS4245_RATES,
			.formats = CS4245_FORMATS,
		},
		.capture = {
			.stream_name = "DAI1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS4245_RATES,
			.formats = CS4245_FORMATS,
		},
		.ops = &cs4245_ops,
	}
};

static const struct snd_soc_component_driver soc_component_cs4245 = {
	.name			= "cs4245 codec",
	.set_bias_level		= cs4245_set_bias_level,
	.controls		= cs4245_snd_controls,
	.num_controls		= ARRAY_SIZE(cs4245_snd_controls),
	.dapm_widgets		= cs4245_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(cs4245_dapm_widgets),
	.dapm_routes		= cs4245_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(cs4245_audio_map),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

static const struct regmap_config cs4245_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = CS4245_MAX_REGISTER,
	.reg_defaults = cs4245_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(cs4245_reg_defaults),
	.readable_reg = cs4245_readable_register,
	.volatile_reg = cs4245_volatile_register,
	.cache_type = REGCACHE_RBTREE,
};

static int cs4245_i2c_probe(struct i2c_client *i2c_client,
			     const struct i2c_device_id *id)
{
	struct cs4245_private *cs4245;
	int ret = 0;
	unsigned int devid = 0;
	unsigned int reg;

	cs4245 = devm_kzalloc(&i2c_client->dev, sizeof(struct cs4245_private),
			       GFP_KERNEL);
	if (cs4245 == NULL)
		return -ENOMEM;

	cs4245->regmap = devm_regmap_init_i2c(i2c_client, &cs4245_regmap);
	if (IS_ERR(cs4245->regmap)) {
		ret = PTR_ERR(cs4245->regmap);
		dev_err(&i2c_client->dev, "regmap_init() failed: %d\n", ret);
		return ret;
	}
	cs4245->reset_gpio = devm_gpiod_get_optional(&i2c_client->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(cs4245->reset_gpio))
		return PTR_ERR(cs4245->reset_gpio);
	if (cs4245->reset_gpio) {
		/* assert reset */
		gpiod_set_value_cansleep(cs4245->reset_gpio, 1);
		mdelay(10);
		/* de-assert reset */
		gpiod_set_value_cansleep(cs4245->reset_gpio, 0);
	}

	i2c_set_clientdata(i2c_client, cs4245);

	ret = regmap_read(cs4245->regmap, CS4245_CHIP_ID, &reg);
	dev_info(&i2c_client->dev, "dev_revid = %X", reg);
	devid = reg & CS4245_CHIP_ID_MASK;
	if (devid != CS4245_CHIP_ID_VAL) {
		ret = -ENODEV;
		dev_err(&i2c_client->dev, "CS4245 Device ID (%X). Expected %X\n",
			devid, CS4245_CHIP_ID_VAL);
		return ret;
	}
	dev_info(&i2c_client->dev, "CS4245 Version %x\n", reg & CS4245_REV_ID_MASK);

	regmap_write(cs4245->regmap, CS4245_PWRCTL, 0x0F);

	ret = devm_snd_soc_register_component(&i2c_client->dev,
			&soc_component_cs4245, cs4245_dai,
			ARRAY_SIZE(cs4245_dai));
	return ret;
}

static const struct of_device_id cs4245_of_match[] = {
	{ .compatible = "cirrus,cs4245", },
	{ }
};
MODULE_DEVICE_TABLE(of, cs4245_of_match);

static const struct i2c_device_id cs4245_id[] = {
	{ "cs4245", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cs4245_id);

static struct i2c_driver cs4245_i2c_driver = {
	.driver = {
		.name = "cs4245",
		.of_match_table = cs4245_of_match,
	},
	.id_table = cs4245_id,
	.probe =    cs4245_i2c_probe,
};

module_i2c_driver(cs4245_i2c_driver);

MODULE_DESCRIPTION("ASoC CS4245 driver");
MODULE_AUTHOR("Rezzonics, <rezzonics@gmail.com>");
MODULE_LICENSE("GPL");
