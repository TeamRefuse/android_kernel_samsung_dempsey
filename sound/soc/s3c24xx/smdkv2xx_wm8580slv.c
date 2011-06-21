/*
 *  smdk64xx_wm8580.c
 *
 *  Copyright (c) 2009 Samsung Electronics Co. Ltd
 *  Author: Jaswinder Singh <jassi.brar@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "../codecs/wm8580.h"
#include "s3c-dma.h"
#include "s3c64xx-i2s.h"

#include <linux/io.h>

#define I2S_NUM 0

extern struct snd_soc_dai i2s_sec_fifo_dai;
extern struct snd_soc_platform s3c_dma_wrapper;
extern const struct snd_kcontrol_new s5p_idma_control;

static int set_epll_rate(unsigned long rate);

static int smdk64xx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int rclk, psr=1;
	int bfs, rfs, ret;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	/* The Fvco for WM8580 PLLs must fall within [90,100]MHz.
	 * This criterion can't be met if we request PLL output
	 * as {8000x256, 64000x256, 11025x256}Hz.
	 * As a wayout, we rather change rfs to a minimum value that
	 * results in (params_rate(params) * rfs), and itself, acceptable
	 * to both - the CODEC and the CPU.
	 */
	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	/* We use MUX for basic ops in SoC-Master mode */
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_MUX,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER, psr-1);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, rfs);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * SMDK64XX WM8580 DAI operations.
 */
static struct snd_soc_ops smdk64xx_ops = {
	.hw_params = smdk64xx_hw_params,
};

/* SMDK64xx Playback widgets */
static const struct snd_soc_dapm_widget wm8580_dapm_widgets_pbk[] = {
	SND_SOC_DAPM_HP("Front-L/R", NULL),
	SND_SOC_DAPM_HP("Center/Sub", NULL),
	SND_SOC_DAPM_HP("Rear-L/R", NULL),
};

/* SMDK64xx Capture widgets */
static const struct snd_soc_dapm_widget wm8580_dapm_widgets_cpt[] = {
	SND_SOC_DAPM_MIC("MicIn", NULL),
	SND_SOC_DAPM_LINE("LineIn", NULL),
};

/* SMDK-PAIFTX connections */
static const struct snd_soc_dapm_route audio_map_tx[] = {
	/* MicIn feeds AINL */
	{"AINL", NULL, "MicIn"},

	/* LineIn feeds AINL/R */
	{"AINL", NULL, "LineIn"},
	{"AINR", NULL, "LineIn"},
};

/* SMDK-PAIFRX connections */
static const struct snd_soc_dapm_route audio_map_rx[] = {
	/* Front Left/Right are fed VOUT1L/R */
	{"Front-L/R", NULL, "VOUT1L"},
	{"Front-L/R", NULL, "VOUT1R"},

	/* Center/Sub are fed VOUT2L/R */
	{"Center/Sub", NULL, "VOUT2L"},
	{"Center/Sub", NULL, "VOUT2R"},

	/* Rear Left/Right are fed VOUT3L/R */
	{"Rear-L/R", NULL, "VOUT3L"},
	{"Rear-L/R", NULL, "VOUT3R"},
};

static int smdk64xx_wm8580_init_paiftx(struct snd_soc_codec *codec)
{
	int ret;

	/* Add smdk64xx specific Capture widgets */
	snd_soc_dapm_new_controls(codec, wm8580_dapm_widgets_cpt,
				  ARRAY_SIZE(wm8580_dapm_widgets_cpt));

	/* Set up PAIFTX audio path */
	snd_soc_dapm_add_routes(codec, audio_map_tx, ARRAY_SIZE(audio_map_tx));

	/* Enabling the microphone requires the fitting of a 0R
	 * resistor to connect the line from the microphone jack.
	 */
	snd_soc_dapm_disable_pin(codec, "MicIn");

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(&wm8580_dai[WM8580_DAI_PAIFTX], SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;
	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(&s3c64xx_i2s_dai[I2S_NUM], SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set WM8580 to drive MCLK from its MCLK-pin */
	ret = snd_soc_dai_set_clkdiv(&wm8580_dai[WM8580_DAI_PAIFTX], WM8580_MCLK,
					WM8580_CLKSRC_MCLK);
	if (ret < 0)
		return ret;

	/* Explicitly set WM8580-ADC to source from MCLK */
	ret = snd_soc_dai_set_clkdiv(&wm8580_dai[WM8580_DAI_PAIFTX], WM8580_ADC_CLKSEL,
					WM8580_CLKSRC_MCLK);
	if (ret < 0)
		return ret;

	return 0;
}

static int smdk64xx_wm8580_init_paifrx(struct snd_soc_codec *codec)
{
	int ret;

	/* Add smdk64xx specific Playback widgets */
	snd_soc_dapm_new_controls(codec, wm8580_dapm_widgets_pbk,
				  ARRAY_SIZE(wm8580_dapm_widgets_pbk));

	/* add iDMA controls */
	ret = snd_soc_add_controls(codec, &s5p_idma_control, 1);
	if (ret < 0)
		return ret;

	/* Set up PAIFRX audio path */
	snd_soc_dapm_add_routes(codec, audio_map_rx, ARRAY_SIZE(audio_map_rx));

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(&wm8580_dai[WM8580_DAI_PAIFRX], SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(&s3c64xx_i2s_dai[I2S_NUM], SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set WM8580 to drive MCLK from its MCLK-pin */
	ret = snd_soc_dai_set_clkdiv(&wm8580_dai[WM8580_DAI_PAIFRX], WM8580_MCLK,
					WM8580_CLKSRC_MCLK);
	if (ret < 0)
		return ret;

	/* Explicitly set WM8580-DAC to source from MCLK */
	ret = snd_soc_dai_set_clkdiv(&wm8580_dai[WM8580_DAI_PAIFRX], WM8580_DAC_CLKSEL,
					WM8580_CLKSRC_MCLK);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_dai_link smdk64xx_dai[] = {
{ /* Primary Playback i/f */
	.name = "WM8580 PAIF RX",
	.stream_name = "Playback",
	.cpu_dai = &s3c64xx_i2s_dai[I2S_NUM],
	.codec_dai = &wm8580_dai[WM8580_DAI_PAIFRX],
	.init = smdk64xx_wm8580_init_paifrx,
	.ops = &smdk64xx_ops,
},
{ /* Primary Capture i/f */
	.name = "WM8580 PAIF TX",
	.stream_name = "Capture",
	.cpu_dai = &s3c64xx_i2s_dai[I2S_NUM],
	.codec_dai = &wm8580_dai[WM8580_DAI_PAIFTX],
	.init = smdk64xx_wm8580_init_paiftx,
	.ops = &smdk64xx_ops,
},
{
	.name = "WM8580 PAIF RX",
	.stream_name = "Playback-Sec",
	.cpu_dai = &i2s_sec_fifo_dai,
	.codec_dai = &wm8580_dai[WM8580_DAI_PAIFRX],
},
};

static struct snd_soc_card smdk64xx = {
	.name = "smdk",
	.platform = &s3c_dma_wrapper,
	.dai_link = smdk64xx_dai,
	.num_links = ARRAY_SIZE(smdk64xx_dai),
};

static struct snd_soc_device smdk64xx_snd_devdata = {
	.card = &smdk64xx,
	.codec_dev = &soc_codec_dev_wm8580,
};

static struct platform_device *smdk64xx_snd_device;

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_disable(fout_epll);
	clk_set_rate(fout_epll, rate);
	clk_enable(fout_epll);

out:
	clk_put(fout_epll);

	return 0;
}

static int __init smdk64xx_audio_init(void)
{
	int ret;
	u32 val;

#include <mach/map.h>
#define S3C_VA_AUDSS	S3C_ADDR(0x01600000)	/* Audio SubSystem */
#include <mach/regs-audss.h>
	/* We use I2SCLK for rate generation, so set EPLLout as
	 * the parent of I2SCLK.
	 */
	val = readl(S5P_CLKSRC_AUDSS);
	val &= ~(0x3<<2);
	val |= (1<<0);
	writel(val, S5P_CLKSRC_AUDSS);

	val = readl(S5P_CLKGATE_AUDSS);
	val |= (0x7f<<0);
	writel(val, S5P_CLKGATE_AUDSS);

	smdk64xx_snd_device = platform_device_alloc("soc-audio", 0);
	if (!smdk64xx_snd_device)
		return -ENOMEM;

	platform_set_drvdata(smdk64xx_snd_device, &smdk64xx_snd_devdata);
	smdk64xx_snd_devdata.dev = &smdk64xx_snd_device->dev;
	ret = platform_device_add(smdk64xx_snd_device);

	if (ret)
		platform_device_put(smdk64xx_snd_device);

	return ret;
}
module_init(smdk64xx_audio_init);

MODULE_AUTHOR("Jaswinder Singh, jassi.brar@samsung.com");
MODULE_DESCRIPTION("ALSA SoC SMDK64XX WM8580");
MODULE_LICENSE("GPL");
