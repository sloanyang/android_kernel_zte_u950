/*
 * Part of MAX98095 ALSA SoC Audio driver
 *
 * Copyright 2011 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * defines the optional driver to control the dsp on the max98095
 */
#define DEBUG
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "max98095.h"
#include <sound/max98095.h>
#include "max98095-dsp.h"

#include <linux/version.h>


#ifdef M98095_USE_DSP

#include "t.h"

#undef TILE_N
#define TILE_N(a) MAXTILE_##a

/*
 * generate an enum of all tiles, just to get the count
 */
enum number_of_tiles {
	FLEXSOUND_TILE_LIST,
	FLEXSOUND_TILE_COUNT,
};

/*
 * table indexed by tile number, of module ids
 */
#undef TILE_N
#define TILE_N(a) FLEXSOUND_CONTROL_pmid(a)
static int pmid[] = {
	FLEXSOUND_TILE_LIST
};

/* table of original filenames */
//#undef TILE_N
//#define TILE_N(a) FLEXSOUND_CONTROL_binary(a)
//static char *filenames[] = {
//	FLEXSOUND_TILE_LIST
//};
/* table of generic tile names */
//#undef TILE_N
//#define TILE_N(a) FLEXSOUND_CONTROL_pmname(a)
//static char *pmnames[] = {
//	FLEXSOUND_TILE_LIST
//};

/* table of user-given tile names */
//#undef TILE_N
//#define TILE_N(a) FLEXSOUND_CONTROL_user_name(a)
//static char *usernames[] = {
//	FLEXSOUND_TILE_LIST
//};

/* table indexed by tile number, giving number of controls */
#undef TILE_N
#define TILE_N(a) a##_CONTROL_COUNT
static int number_of_controls_per_tile[] = {
	FLEXSOUND_TILE_LIST
};

/*
 * this gives a table of control "parameter numbers" for all tiles,
 * use number_of_controls table to index for a particular tile
 */
#undef TILE_N
#define TILE_N(b) b##_CONTROL_LIST
#undef TILE_C
#define TILE_C(a) FLEXSOUND_CONTROL_paramid(a)
static uint8_t control_ids[] = {
	FLEXSOUND_TILE_LIST
};

/*
 * this gives a table of control initial values for all tiles,
 * use number_of_controls table to index for a particular tile
 */
#undef TILE_N
#define TILE_N(b) b##_CONTROL_LIST
#undef TILE_C
#define TILE_C(a) FLEXSOUND_CONTROL_value(a)
static int control_values[] = {
	FLEXSOUND_TILE_LIST
};

/* every msav has one config and one download hex file */
/* let C figure out how many */
#undef TILE_C
	#define TILE_C(a) a##_CONFIG_NUMBER
enum number_of_configs {
	FLEXSOUND_CONFIG_LIST,
	FLEXSOUND_NUMBER_OF_CONFIGS
};

/* emit all configs in one table */
/* indexed by config number, will be zero if no config (stand alone program) */
#undef TILE_C
	#define TILE_C(a) a
static uint8_t all_config[] = {
	FLEXSOUND_CONFIG_LIST
};
/* emit size of each config, so to access the 2nd config add the length */
/* of the first config to the all_config ptr to skip the first config */
/* Note if config_sizes <= 1 there is no config for this file, */
/* but we still need its name and placeholder */
#undef TILE_C
	#define TILE_C(a) a##_CONF_LENGTH
static int config_sizes[] = {
	FLEXSOUND_CONFIG_LIST
};
/* emit number of tiles for each config, indexed by config #, */
/* so to access the 2nd config tiles, in a table of all tiles, */
/* add the number of tiles to the first config tile count. */
/* This gets an index to the desired tile param ptr. */
#undef TILE_C
	#define TILE_C(a) a##_NUMBER_OF_CONTROLS
static int config_module_count[] = {
	FLEXSOUND_CONFIG_LIST
};
/* length of binary used for each config, indexed by file number */

/* table per config to enter index length of binary file */
#undef TILE_C
	#define TILE_C(a) a##_LENGTH
static int file_sizes[] = {
	FLEXSOUND_FILENAME_LIST
};
/* emit file number for each config, */
/* used to index the filename length list, used to index the binary */
/* so to access the 2nd config file, add the length */
/* of the first config file to the desired file param ptr. */
/* Each config has 1 to n files, and the files can be duplicates, */
/* so here is a table indexed by config and FILE_COUNT */
/* to point into the download_binary table by adding through file_sizes */
static int config_file_index[] = {
	FLEXSOUND_CF_LIST
};
/* emit number of files for each config, will range from 1 to n */
/* used to index the filename length list, used to index the binary */
/* so to access the 2nd config file, add the length */
/* of the first config file(s) to the desired file param ptr. */
#undef TILE_C
	#define TILE_C(a) a##_FILE_COUNT
static int config_file_count[] = {
	FLEXSOUND_CONFIG_LIST
};
/* table of user-given msav configuration file names */
#undef TILE_C
#define TILE_C(a) a##_FILENAME
static char *config_filenames[] = {
	FLEXSOUND_CONFIG_LIST
};
/* table of register value for all configs, index using */
/* config # times FLEXSOUND_REGISTER_LIST_LENGTH */
/* in my case I am only storing the value so the */
/* len = FLEXSOUND_REGISTER_LIST_LENGTH / 2 */
/* it is consecutively numbered, so don't save the reg # */
/* the consecutive numbers start at FLEXSOUND_REGISTER_LIST_FIRST */

#define MY_REGISTER_LENGTH (FLEXSOUND_REGISTER_LIST_LENGTH / 2)
#undef TILE_C
#define TILE_C(a, b) b
static uint8_t default_registers[] = {
	FLEXSOUND_REGISTER_LIST
};


/* table of binary for all files */
#undef TILE_C
#define TILE_C(a) a
static uint8_t download_binary[] = {
	FLEXSOUND_FILENAME_LIST
};

/* global within module fixme move to max98095 private data */
extern int verbosity;

#define DEBUG
#ifdef DEBUG
#define DEBUGF(...) if (verbosity) printk(KERN_DEBUG __VA_ARGS__)
#else
#define DEBUGF(...) do {} while (0)
#endif

#ifdef DEBUG
static void dump(uint8_t *ptr, int base, int bytelength, int elementsize)
{
	int i;
	int first_time = 1;
	if (bytelength && elementsize) {
		for (i = 0; i < bytelength; i += elementsize) {
			if ((base + i) % 8 == 0 || first_time) {
				DEBUGF("\n0x%04x:", base + i);
				first_time = 0;
			}
			switch (elementsize) {
			case 4:
				DEBUGF(" 0x%08x", BUILD_WORD(ptr, i));
			break;
			default:
				DEBUGF(" 0x%02x", ptr[i] & 0xff);
			}
		}
		DEBUGF("\n");
	}
}
static void hex_dump(uint8_t *ptr, int bytelength)
{
	if ((verbosity > 1) && bytelength)
		dump(ptr, 0, bytelength, 1);
}
#else
#define hex_dump(a, b) do {} while (0)
#endif

static int offset_to_config(int config_no)
{
	int i;
	int size = 0;
	for (i = 0; i < config_no; i++)
		size += config_sizes[i];

	return size;
}
/*
 * every config has a initial index in file_sizes
 * And a number of files after this initial config
 */
static int offset_to_file_base(int config_no)
{
	int i;
	int size = 0;

	for (i = 0; i < config_no; i++)
		size += config_file_count[i];

	return size;
}
static int offset_to_file_size(int config_no, int file_no)
{
	return config_file_index[offset_to_file_base(config_no) + file_no];
}
static int offset_to_file_binary(int config_no, int file_no)
{
	int i;
	int size = 0;

	for (i = 0; i < offset_to_file_size(config_no, file_no); i++)
		size += file_sizes[i];

	return size;
}
/*
 * Every module or tile has a unique control count
 */
static int offset_to_module(int config_no)
{
	int i;
	int size = 0;
	for (i = 0; i < config_no; i++)
		size += config_module_count[i];

	return size;
}
/*
 * Skip to requested control in requested config
 *	need to skip number of controls until hit my config.
 *	I have the config_module_count per config
 *	need to consume that many entries in number_of_controls_per_tile
 *	to point to the base control of the config,
 *	then skip to module_no in config
 *
 */
static int offset_to_control(int config_no, int module_no)
{
	int i;
	int size = 0;
	for (i = 0; i < offset_to_module(config_no) + module_no; i++)
		size +=	number_of_controls_per_tile[i];

	return size;
}

/*
 * calculate index into table using:
 * config # times FLEXSOUND_REGISTER_LIST_LENGTH
 * in my case I am only storing the value so the
 * table length = FLEXSOUND_REGISTER_LIST_LENGTH / 2
 */
static int offset_to_registers(int config_no)
{
	return config_no * MY_REGISTER_LENGTH;
}

#define PREAMBLE_LENGTH 12
#define POSTAMBLE_LENGTH 9
#define KEY_LENGTH 6
static int send_preamble(struct snd_soc_codec *codec, int config_no,
								uint8_t *data)
{
	int i;
	for (i = 0; i < KEY_LENGTH; i++) {
		snd_soc_write(codec, M98095_018_KEYCODE3 + i,
				default_registers[
					offset_to_registers(config_no) +
					M98095_018_KEYCODE3 + i]);
	}
	for (i = 0; i < PREAMBLE_LENGTH; i++)
		snd_soc_write(codec, M98095_015_DEC, data[i]);

	return 0;
}
static int send_postamble(struct snd_soc_codec *codec, uint8_t *data)
{
	int i;
	for (i = 0; i < POSTAMBLE_LENGTH; i++)
		snd_soc_write(codec, M98095_015_DEC, data[i]);

	return 0;
}

#define FLEXSOUND_ENCRYPTED	0x2118
#define FLEXSOUND_DOWNLOAD	0
#define FLEXSOUND_CONFIG	0x003
static int do_one_download(struct snd_soc_codec *codec, int config_no,
								int file_no)
{
	int ret;
	int myfile = offset_to_file_binary(config_no, file_no);
	int this_file_size;
	uint32_t download_type = download_binary[myfile] +
				 (download_binary[myfile + 1] << 8);
	this_file_size = file_sizes[offset_to_file_size(config_no, file_no)];

	DEBUGF("%s:download_type %d myfile=%d\n", __func__, download_type,
									myfile);

	switch (download_type) {
	case FLEXSOUND_ENCRYPTED:
		/* handle encrypted download */

		snd_soc_update_bits(codec, M98095_00F_HOST_CFG,
					M98095_DCRYPTEN,
					M98095_DCRYPTEN);

		ret = send_preamble(codec, config_no, &download_binary[myfile]);
		if (ret < 0)
			return ret;

		dump(&download_binary[myfile + PREAMBLE_LENGTH],
			0,
			this_file_size - POSTAMBLE_LENGTH - PREAMBLE_LENGTH,
			1);

		ret =  max98095_mem_download(codec,
			&download_binary[myfile + PREAMBLE_LENGTH],
			this_file_size - POSTAMBLE_LENGTH - PREAMBLE_LENGTH,
			0);
		if (ret < 0)
			return ret;

		ret = send_postamble(codec,
				&download_binary[myfile + this_file_size -
							PREAMBLE_LENGTH]);
		if (ret < 0)
			return ret;

		snd_soc_update_bits(codec, M98095_00F_HOST_CFG,
					M98095_DCRYPTEN,
					0);

		ret = max98095_read_command_status(codec);
	break;
	case FLEXSOUND_DOWNLOAD:
	case FLEXSOUND_CONFIG:
		ret =  max98095_mem_download(codec,
		&download_binary[myfile],
		this_file_size,
		1);
	break;
	default:
		return -44; /* unknown binary */
	}
	return ret;
}
static int do_download(struct snd_soc_codec *codec, int config_no)
{
	int ret = 0;
	int i;

	for (i = 0; i < config_file_count[config_no]; i++) {
		ret = do_one_download(codec, config_no, i);
		if (ret < 0)
			break;
	}
	return ret;
}
/*
 * iterate through all modules in this config
 */
static int send_all_params(struct snd_soc_codec *codec,
					enum number_of_configs config_no)
{
	int i, j;
	int ret = 0;
	for (j = 0; j < config_module_count[config_no]; j++) {
		/*
		 * iterate through all controls in this module
		 */
		 for (i = 0;
		 i < number_of_controls_per_tile[offset_to_module(config_no)
									   + j];
		 i++) {
			uint32_t param =
			   (control_values[offset_to_control(config_no, j) + i]
			   & 0xffffff) +
			   (control_ids[offset_to_control(config_no, j) + i]
			   << 24);
			DEBUGF("\n%s set param module=%x, param=%x i=%d j=%d\n",
				__func__, pmid[offset_to_module(config_no) + j],
				param, i, j);
			ret = max98095_set_param(codec,
					pmid[offset_to_module(config_no) + j],
					1,
					&param);
			if (ret < 0)
				return ret;
		}
	}
	return ret;
}

int max98095_mid_write[M98095_REG_CNT] = {
	0, /* 00 cmdrsp */
	0, /* 01 */
	0, /* 02 */
	0, /* 03 */
	0, /* 04 */
	0, /* 05 */
	0, /* 06 */
	0, /* 07 */
	0, /* 08 */
	0, /* 09 */
	0, /* 0A */
	0, /* 0B */
	0, /* 0C */
	0, /* 0D */
	0, /* 0E */
	1, /* 0F segment pointer, xt enable */
	1, /* 10 host interrupt */
	1, /* 11 host interrupt enable */
	0, /* 12 codec interrupt enable */
	0, /* 13 jack det enable1 */
	0, /* 14 jack det enable2 */
	0, /* 15 decrypt */
	0, /* 16 reserved */
	0, /* 17 reserved */
	0, /* 18 keycode 1 */
	0, /* 19 keycode 2 */
	0, /* 1A keycode 3 */
	0, /* 1B keycode 4 */
	0, /* 1C oemcode 1 */
	0, /* 1D oemcode 2 */
	0, /* 1E fifo cfg1 */
	0, /* 1F fifo cfg2 */
	0, /* 20 fifo cfg3 */
	0, /* 21 fifo cfg4 */
	0, /* 22 fifo cfg5 */
	0, /* 23 fifo cfg6 */
	0, /* 24 GPIO enable/dir */
	0, /* 25 xtensa clock config */
	0, /* 26 codec clock config. */
	0, /* 27 dai1 clock mode. */
	0, /* 28 dai1 anyclock. */
	0, /* 29 dai1 anyclock. */
	0, /* 2A dai1 format */
	0, /* 2B dai1 clock */
	0, /* 2C dai1 io cfg */
	0, /* 2D dai1 TDM */
	0, /* 2E dai1 SRC, DHF, DVFLT filters */
	0, /* 2F dai1 gain ADCG, DSP1G1 */
	0, /* 30 dai1 DSP2G1, DSP3G1 */
#if 0
	1, /* 31 dai2 clock mode */
	1, /* 32 dai2 anyclock */
	1, /* 33 dai2 anyclock */
	1, /* 34 dai2 format */
	1, /* 35 dai2 clock */
	0, /* 36 dai2 io cfg */
	1, /* 37 dai2 TDM 0xFF */
	1, /* 38 dai2 SRC, DHF, DVFLT filters */
	1, /* 39 dai2 level */
	1, /* 3A dai2 level */
	1, /* 3B dai3 clock mode */
	1, /* 3C dai3 anyclock */
	1, /* 3D dai3 anyclock */
	1, /* 3E dai3 format */
	1, /* 3F dai3 clock */
	0, /* 40 dai3 io config */
	1, /* 41 dai3 TDM */
	1, /* 42 dai3 filters */
	1, /* 43 dai3 level */
	1, /* 44 dai3 level */
#endif
	0, /* 31 dai2 clock mode */
	0, /* 32 dai2 anyclock */
	0, /* 33 dai2 anyclock */
	0, /* 34 dai2 format */
	0, /* 35 dai2 clock */
	0, /* 36 dai2 io cfg */
	0, /* 37 dai2 TDM 0xFF */
	0, /* 38 dai2 SRC, DHF, DVFLT filters */
	0, /* 39 dai2 level */
	0, /* 3A dai2 level */
	0, /* 3B dai3 clock mode */
	0, /* 3C dai3 anyclock */
	0, /* 3D dai3 anyclock */
	0, /* 3E dai3 format */
	0, /* 3F dai3 clock */
	0, /* 40 dai3 io config */
	0, /* 41 dai3 TDM */
	0, /* 42 dai3 filters */
	0, /* 43 dai3 level */
	0, /* 44 dai3 level */
	
	0, /* 45 ### ADC clock select - to switch DAIs */
	0, /* 46 ### DAC control*/
	0, /* 47 DAC control */
	0, /* 48 mix DAC stereo - DAI to DAC mapping*/
	0, /* 49 mix DAC mono - DAI to DAC mapping */
	0, /* 4A mix left ADC */
	0, /* 4B mix right ADC */
	0, /* 4C mix left headphone */
	0, /* 4D mix right headphone */
	0, /* 4E mix HP */
	0, /* 4F mix RCV */
	0, /* 50 mix spkl */
	0, /* 51 mix spkr */
	0, /* 52 mix spk */
	0, /* 53 mix lineout */
	0, /* 54 mix lineout */
	0, /* 55 mix lineout */
	0, /* 56 lvl sidetone dai1,2 */
	0, /* 57 lvl sidetone dai3 */
	0, /* 58 lvl dai1 playback */
	0, /* 59 lvl dai1 eq1 */
	0, /* 5A lvl dai2 playback */
	0, /* 5B lvl dai2 eq2 */
	0, /* 5C lvl dai3 playback */
	0, /* 5D lvl adcl */
	0, /* 5E lvl adcr */
	0, /* 5F lvl mic1 */
	0, /* 60 lvl mic2 */
	0, /* 61 lvl linein */
	0, /* 62 lvl lineout1 */
	0, /* 63 lvl lineout2 */
	1, /* 64 lvl hpl */
	1, /* 65 lvl hpr */
	1, /* 66 lvl rcv */
	1, /* 67 lvl spkl */
	1, /* 68 lvl spkr */
	0, /* 69 mic agc */
	0, /* 6A mic agc */
	0, /* 6B spk noise gate */
	0, /* 6C dai1 drc */
	0, /* 6D dai1 drc */
	0, /* 6E dai1 drc */
	0, /* 6F dai1 drc */
	0, /* 70 dai1 drc */
	0, /* 71 dai1 drc */
	0, /* 72 dai1 drc */
	0, /* 73 dai1 drc */
	0, /* 74 dai1 drc */
	0, /* 75 dai1 drc */
	0, /* 76 dai1 drc */
	0, /* 77 dai1 drc */
	0, /* 78 dai2 drc */
	0, /* 79 dai2 drc */
	0, /* 7A dai2 drc */
	0, /* 7B dai2 drc */
	0, /* 7C dai2 drc */
	0, /* 7D dai2 drc */
	0, /* 7E dai2 drc */
	0, /* 7F dai2 drc */
	0, /* 80 dai2 drc */
	0, /* 81 dai2 drc */
	0, /* 82 dai2 drc */
	0, /* 83 dai2 drc */
	0, /* 84 HP noise gate */
	0, /* 85 aux adc */
	0, /* 86 lineout config */
	0, /* 87 mic config */
	0, /* 88 EQ BQ enables */
	0, /* 89 auto jack det */
	0, /* 8A manual jack det */
	0, /* 8B keyscan debounce */
	0, /* 8C keyscan delay */
	0, /* 8D keythresh */
	0, /* 8E jack DC slew */
	0, /* 8F jack test config */
#if 0
       1, /* 90 pwr input enable */
	1, /* 91 pwr output enable */
#endif
       0, /* 90 pwr input enable */
	0, /* 91 pwr output enable */
	
       0, /* 92 pwr output enable */
	0, /* 93 pwr bias HW default 0xF0 is fine */
	0, /* 94 pwr dac */
	0, /* 95 reserved */
	0, /* 96 pwr dac HW default 0x3F is fine */
	0, /* 97 pwr shdn */
	0, /* 98 */
	0, /* 99 */
	0, /* 9A */
	0, /* 9B */
	0, /* 9C */
	0, /* 9D */
	0, /* 9E */
	0, /* 9F */
	0, /* A0 */
	0, /* A1 */
	0, /* A2 */
	0, /* A3 */
	0, /* A4 */
	0, /* A5 */
	0, /* A6 */
	0, /* A7 */
	0, /* A8 */
	0, /* A9 */
	0, /* AA */
	0, /* AB */
	0, /* AC */
	0, /* AD */
	0, /* AE */
	0, /* AF */
	0, /* B0 */
	0, /* B1 */
	0, /* B2 */
	0, /* B3 */
	0, /* B4 */
	0, /* B5 */
	0, /* B6 */
	0, /* B7 */
	0, /* B8 */
	0, /* B9 */
	0, /* BA */
	0, /* BB */
	0, /* BC */
	0, /* BD */
	0, /* BE */
	0, /* BF */
	0, /* C0 */
	0, /* C1 */
	0, /* C2 */
	0, /* C3 */
	0, /* C4 */
	0, /* C5 */
	0, /* C6 */
	0, /* C7 */
	0, /* C8 */
	0, /* C9 */
	0, /* CA */
	0, /* CB */
	0, /* CC */
	0, /* CD */
	0, /* CE */
	0, /* CF */
	0, /* D0 */
	0, /* D1 */
	0, /* D2 */
	0, /* D3 */
	0, /* D4 */
	0, /* D5 */
	0, /* D6 */
	0, /* D7 */
	0, /* D8 */
	0, /* D9 */
	0, /* DA */
	0, /* DB */
	0, /* DC */
	0, /* DD */
	0, /* DE */
	0, /* DF */
	0, /* E0 */
	0, /* E1 */
	0, /* E2 */
	0, /* E3 */
	0, /* E4 */
	0, /* E5 */
	0, /* E6 */
	0, /* E7 */
	0, /* E8 */
	0, /* E9 */
	0, /* EA */
	0, /* EB */
	0, /* EC */
	0, /* ED */
	0, /* EE */
	0, /* EF */
	0, /* F0 */
	0, /* F1 */
	0, /* F2 */
	0, /* F3 */
	0, /* F4 */
	0, /* F5 */
	0, /* F6 */
	0, /* F7 */
	0, /* F8 */
	0, /* F9 */
	0, /* FA */
	0, /* FB */
	0, /* FC */
	0, /* FD */
	0, /* FE */
	0, /* FF */
};

static int max98095_write1(struct snd_soc_codec *codec, int i2c_reg, uint8_t val)
{
	struct i2c_client *i2c = codec->control_data;
	int ret;
	uint8_t buf[4];

	buf[0] = i2c_reg;
	buf[1] = val;
	ret = i2c_master_send(i2c, buf, 2);
	if (ret != 2) {
		DEBUGF("i2c_transfer() returned %d\n", ret);
		return -8;
	}
	return 0;
}

/*
 * Starting at register i2c_offs, write len bytes from buffer
 * returns <0 on error
 */
int max98095_write_reg_bytes(struct snd_soc_codec *codec, int i2c_offs, 
						uint8_t *buffer, int len)
{
	int ret = 0;
	int i;

	if (i2c_offs + len > 0xFF) {
		DEBUGF("Error: register out of bound\n");
		return -1;
	}	

	for (i=0; i<len; i++)
	{
		if (max98095_mid_write[i2c_offs+i]) {
			ret = max98095_write1(codec, i2c_offs + i, buffer[i]);
			if(ret < 0) {
				DEBUGF("Error: Write failed, return code %d\n", ret);
				return ret;
			}
		}
	}
	return ret;
}

/*
 * Driver to install a new binary, send it a system configuration and
 * then send it all the required configs and params to get it running.
 *
 */
static enum number_of_configs lastconfig = -1; //fixme move into codec private data

int max98095_start_new_dsp(struct snd_soc_codec *codec,
					enum number_of_configs config_no)
{
	int i;
	int ret = 0;
	volatile int status;

	if (config_no >= FLEXSOUND_NUMBER_OF_CONFIGS)
		return -ENODEV;
//	if (config_no != lastconfig)
        {
		DEBUGF("\n%s starting new config=%d\n", __func__, config_no);
		lastconfig = config_no;
		
		snd_soc_update_bits(codec, M98095_097_PWR_SYS, M98095_SHDNRUN, 0);

		/* data FIFO to bypasses DSP while we download */
		for (i=0; i<6; i++)
		{
			ret = max98095_write1(codec, 0x1E + i, 0);
			if(ret < 0) {
				DEBUGF("Error: Write failed, return code %d\n", ret);
				return ret;
			}
		}

		snd_soc_update_bits(codec, M98095_097_PWR_SYS, M98095_SHDNRUN, M98095_SHDNRUN);

		/* read from status register to clear any xsts bits */
		status = snd_soc_read(codec, M98095_001_HOST_INT_STS);

		/* reset the dsp ready for download */
		snd_soc_update_bits(codec, M98095_00F_HOST_CFG,
			M98095_XTCLKEN | M98095_MDLLEN |
			M98095_XTEN | M98095_SEG,
			0);

		DEBUGF("\nSending regs, starting=%x\n",
			FLEXSOUND_REGISTER_LIST_FIRST + 1);
			hex_dump(&default_registers[offset_to_registers(config_no)],
			M98095_097_PWR_SYS + 1 - FLEXSOUND_REGISTER_LIST_FIRST);

		/* write all registers: max98095_write_n_bytes */
		ret = max98095_write_reg_bytes(codec, 
				FLEXSOUND_REGISTER_LIST_FIRST + 1,
				&default_registers[	offset_to_registers(config_no) + 1],
				M98095_097_PWR_SYS - FLEXSOUND_REGISTER_LIST_FIRST - 1);

		if (ret < 0)
			goto err;
                 //ZTE: added for volume setting, begin
		/* write regs (0x5D..0x60): lvl_adcl, lvl_adcr, lvl_mic1, lvl_mic2 */
		/* ret = max98095_write_n_bytes(codec, 			*/
		/*	0x5D, &default_registers[offset_to_registers(config_no) + (0x5D - 0x0F)], 4); */
		{
			uint8_t *buf = &default_registers[offset_to_registers(config_no) + (0x5D - 0x0F)];
			for (i=0; i<4; i++)
			{
				ret = snd_soc_write(codec, 0x5D + i, buf[i]);
				if (ret < 0) {
					DEBUGF("Error: Write failed, return code %d\n", ret);
					break;
				}
			}
		}
                //ZTE: added for volume setting, end
		/*
		 * now send all the msav defined registers except 0xf
		 * Send second bank
		 */
//		snd_soc_update_bits(codec, M98095_00F_HOST_CFG,
//			M98095_SEG, M98095_SEG);
//
//		DEBUGF("\n%s sending second bank of regs, starting=%x\n",
//				__func__, FLEXSOUND_REGISTER_LIST_FIRST + 1);
//		hex_dump(&default_registers[
//					offset_to_registers(config_no) +
//					M98095_097_PWR_SYS + 1],
//				MY_REGISTER_LENGTH - M98095_097_PWR_SYS);
//				
//		ret = max98095_write_n_bytes(codec,
//				FLEXSOUND_REGISTER_LIST_FIRST + 1,
//				&default_registers[offset_to_registers(config_no) +	M98095_097_PWR_SYS + 1],
//				MY_REGISTER_LENGTH - 1 - M98095_097_PWR_SYS);
//		if (ret < 0)
//			goto err;

		/* turn on the dsp one bit at a time*/
		snd_soc_update_bits(codec, M98095_00F_HOST_CFG,
				M98095_MDLLEN | M98095_SEG, M98095_MDLLEN);
				
		snd_soc_update_bits(codec, M98095_00F_HOST_CFG,
				M98095_XTCLKEN, M98095_XTCLKEN);
						
		/* XTEN enable bit should be turned on last, separate from clocking */
		snd_soc_update_bits(codec, M98095_00F_HOST_CFG,
				M98095_XTEN, M98095_XTEN);

		/* verify reg 1 xsts[7:6] bits are 1s */
		status = snd_soc_read(codec, M98095_001_HOST_INT_STS);
		if ((status & 0xC0) != 0xC0) {
			/* reset dsp core again - toggling xten is sufficient */
			snd_soc_update_bits(codec, M98095_00F_HOST_CFG, M98095_XTEN, 0);
			dev_err(codec->dev, "Toggle XTEN. INT_STS status: 0x%02X\n", status);
			/* delay 50us */
			snd_soc_update_bits(codec, M98095_00F_HOST_CFG, M98095_XTEN, M98095_XTEN);
		}

		/* get rom id , where id=module#, or 0 for romid
 		 * returns 16 bit id number or neg value if error */
		status = max98095_get_id(codec, 0);
		if (status < 0)
			dev_err(codec->dev, "get_id=%d\n", status); 		 

		ret = do_download(codec, config_no);
		 if (ret < 0) {
		 		 		 dev_err(codec->dev, "do_download=%d\n", ret);
			goto err;
		 		 }

		/*
		 * The right file is there, now send its optional config
		 */
		hex_dump((uint8_t *)&all_config[offset_to_config(config_no)],
						config_sizes[config_no]);
		if (config_sizes[config_no] > 1) {
			ret =  max98095_mem_download(codec,
				&all_config[offset_to_config(config_no)],
				config_sizes[config_no],
				1);
			if (ret < 0) {
		 		dev_err(codec->dev, "mem_download=%d\n", ret);
				goto err;
		}
		}

		/*
		 * now the proper file and configuration are downloaded,
		 * set params to further tweak it
		 */
		 ret = send_all_params(codec, config_no);
		if (ret < 0) {
		 	dev_err(codec->dev, "send_all_params=%d\n", ret);
		 	goto err;
		 }

		/* write regs (0x1E..0x23) needed to turn on FIFOs 	*/
		/* ret = max98095_write_reg_bytes(codec, 			*/
		/*	0x1E, &default_registers[offset_to_registers(config_no) + (0x1E - 0x0F)], 6); */
		{
			uint8_t *buf = &default_registers[offset_to_registers(config_no) + (0x1E - 0x0F)];
			for (i=0; i<6; i++)
			{
				ret = max98095_write1(codec, 0x1E + i, buf[i]);
				if (ret < 0) {
					DEBUGF("Error: Write failed, return code %d\n", ret);
					break;
				}
			}
		}
		if (ret < 0)
			goto err;

	}
	return ret;
err:
	/* smudge saved values for reentry later */
	lastconfig = -1;
	return ret;
}

char **max98095_get_cfg_names(void)
{
	return &config_filenames[0];
}

int max98095_get_cfg_count(void)
{
	return FLEXSOUND_NUMBER_OF_CONFIGS;
}


#endif /* M98095_USE_DSP */
