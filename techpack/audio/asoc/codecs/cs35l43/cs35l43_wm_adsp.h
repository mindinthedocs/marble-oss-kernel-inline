/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * cs35l43_wm_adsp.h  --  Wolfson ADSP support
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 */

#ifndef __cs35l43_wm_adsp_H
#define __cs35l43_wm_adsp_H

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/compress_driver.h>

#include "wmfw.h"

/* Return values for cs35l43_wm_adsp_compr_handle_irq */
#define cs35l43_wm_adsp_COMPR_OK                 0
#define cs35l43_wm_adsp_COMPR_VOICE_TRIGGER      1

#define cs35l43_wm_adsp2_REGION_0 BIT(0)
#define cs35l43_wm_adsp2_REGION_1 BIT(1)
#define cs35l43_wm_adsp2_REGION_2 BIT(2)
#define cs35l43_wm_adsp2_REGION_3 BIT(3)
#define cs35l43_wm_adsp2_REGION_4 BIT(4)
#define cs35l43_wm_adsp2_REGION_5 BIT(5)
#define cs35l43_wm_adsp2_REGION_6 BIT(6)
#define cs35l43_wm_adsp2_REGION_7 BIT(7)
#define cs35l43_wm_adsp2_REGION_8 BIT(8)
#define cs35l43_wm_adsp2_REGION_9 BIT(9)
#define cs35l43_wm_adsp2_REGION_1_9 (cs35l43_wm_adsp2_REGION_1 | \
		cs35l43_wm_adsp2_REGION_2 | cs35l43_wm_adsp2_REGION_3 | \
		cs35l43_wm_adsp2_REGION_4 | cs35l43_wm_adsp2_REGION_5 | \
		cs35l43_wm_adsp2_REGION_6 | cs35l43_wm_adsp2_REGION_7 | \
		cs35l43_wm_adsp2_REGION_8 | cs35l43_wm_adsp2_REGION_9)
#define cs35l43_wm_adsp2_REGION_ALL (cs35l43_wm_adsp2_REGION_0 | cs35l43_wm_adsp2_REGION_1_9)

struct cs35l43_wm_adsp_region {
	int type;
	unsigned int base;
};

struct cs35l43_wm_adsp_alg_region {
	struct list_head list;
	unsigned int alg;
	int type;
	unsigned int base;
};

struct cs35l43_wm_adsp_compr;
struct cs35l43_wm_adsp_compr_buf;
struct cs35l43_wm_adsp_ops;

struct cs35l43_wm_adsp {
	const char *part;
	const char *name;
	const char *fwf_name;
	int rev;
	int num;
	int type;
	struct device *dev;
	struct regmap *regmap;
	struct snd_soc_component *component;

	const struct cs35l43_wm_adsp_ops *ops;

	unsigned int base;
	unsigned int base_sysinfo;
	unsigned int sysclk_reg;
	unsigned int sysclk_mask;
	unsigned int sysclk_shift;

	struct list_head alg_regions;

	unsigned int fw_id;
	unsigned int fw_id_version;
	unsigned int fw_vendor_id;

	const struct cs35l43_wm_adsp_region *mem;
	int num_mems;

	int fw;
	int fw_ver;

	bool preloaded;
	bool booted;
	bool running;
	bool fatal_error;
	bool tuning_has_prefix;

	struct list_head ctl_list;

	struct work_struct boot_work;

	struct list_head compr_list;
	struct list_head buffer_list;

	struct mutex pwr_lock;

	unsigned int lock_regions;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
	char *wmfw_file_name;
	char *bin_file_name;
#endif
	/*
	 * Flag indicating the preloader widget only needs power toggled
	 * on state change rather than held on for the duration of the
	 * preload, useful for devices that can retain firmware memory
	 * across power down.
	 */
	bool toggle_preload;
};

struct cs35l43_wm_adsp_ops {
	unsigned int sys_config_size;

	bool (*validate_version)(struct cs35l43_wm_adsp *dsp, unsigned int version);
	unsigned int (*parse_sizes)(struct cs35l43_wm_adsp *dsp,
				    const char * const file,
				    unsigned int pos,
				    const struct firmware *firmware);
	int (*setup_algs)(struct cs35l43_wm_adsp *dsp);
	unsigned int (*region_to_reg)(struct cs35l43_wm_adsp_region const *mem,
				      unsigned int offset);

	void (*show_fw_status)(struct cs35l43_wm_adsp *dsp);
	void (*stop_watchdog)(struct cs35l43_wm_adsp *dsp);

	int (*enable_memory)(struct cs35l43_wm_adsp *dsp);
	void (*disable_memory)(struct cs35l43_wm_adsp *dsp);
	int (*lock_memory)(struct cs35l43_wm_adsp *dsp, unsigned int lock_regions);

	int (*enable_core)(struct cs35l43_wm_adsp *dsp);
	void (*disable_core)(struct cs35l43_wm_adsp *dsp);

	int (*start_core)(struct cs35l43_wm_adsp *dsp);
	void (*stop_core)(struct cs35l43_wm_adsp *dsp);
};

#define cs35l43_wm_adsp1(wname, num) \
	SND_SOC_DAPM_PGA_E(wname, SND_SOC_NOPM, num, 0, NULL, 0, \
		cs35l43_wm_adsp1_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD)

#define cs35l43_wm_adsp2_PRELOAD_SWITCH(wname, num) \
	SOC_SINGLE_EXT(wname " Preload Switch", SND_SOC_NOPM, num, 1, 0, \
		cs35l43_wm_adsp2_preloader_get, cs35l43_wm_adsp2_preloader_put)

#define cs35l43_wm_adsp2(wname, num, event_fn) \
	SND_SOC_DAPM_SPK(wname " Preload", NULL), \
{	.id = snd_soc_dapm_supply, .name = wname " Preloader", \
	.reg = SND_SOC_NOPM, .shift = num, .event = event_fn, \
	.event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD, \
	.subseq = 100, /* Ensure we run after SYSCLK supply widget */ }, \
{	.id = snd_soc_dapm_out_drv, .name = wname, \
	.reg = SND_SOC_NOPM, .shift = num, .event = cs35l43_wm_adsp_event, \
	.event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD }

#define cs35l43_wm_adsp_FW_CONTROL(dspname, num) \
	SOC_ENUM_EXT(dspname " Firmware", cs35l43_wm_adsp_fw_enum[num], \
		     cs35l43_wm_adsp_fw_get, cs35l43_wm_adsp_fw_put)

// get more logs when i2c retry happen, JIRA: XIAOM-339
#undef regmap_update_bits
#define regmap_update_bits(map, reg, mask, val) \
({ \
	int __ret = 0, __i; \
	for (__i = 0; __i < 3; __i ++) { \
		__ret = regmap_update_bits_base(map, reg, mask, val, NULL, false, false); \
		if (__ret) { \
		    pr_info("I2C retry : regmap_update_bits: %d, (0x%x, %x, %x), ret = %d\n", __i, reg, mask, val, __ret); \
			usleep_range(1000, 1050); \
			continue; \
		} \
        break; \
	} \
	__ret; \
})

#undef regmap_write
#define regmap_write(map, reg, val) \
({ \
	int __ret = 0, __i; \
	for (__i = 0; __i < 3; __i ++) { \
		__ret = regmap_write(map, reg, val); \
		if (__ret) { \
		    pr_info("I2C retry : regmap_write: %d, (0x%x, %x), ret = %d\n", __i, reg, val, __ret); \
			usleep_range(1000, 1050); \
			continue; \
		} \
        break; \
	} \
	__ret; \
})

#undef regmap_read
#define regmap_read(map, reg, val) \
({ \
	int __ret = 0, __i; \
	for (__i = 0; __i < 3; __i ++) { \
		__ret = regmap_read(map, reg, val); \
		if (__ret) { \
		    pr_info("I2C retry : regmap_read: %d, (0x%x, %x), ret = %d\n", __i, reg, val, __ret); \
			usleep_range(1000, 1050); \
			continue; \
		} \
        break; \
	} \
	__ret; \
})

#undef regmap_raw_write
#define regmap_raw_write(map, reg, val, len) \
({ \
	int __ret = 0, __i; \
	for (__i = 0; __i < 3; __i ++) { \
		__ret = regmap_raw_write(map, reg, val, len); \
		if (__ret) { \
		    pr_info("I2C retry : regmap_raw_write: %d, (0x%x, %p, %d), ret = %d\n", __i, reg, val, len, __ret); \
			usleep_range(1000, 1050); \
			continue; \
		} \
        break; \
	} \
	__ret; \
})

#undef regmap_raw_write_async
#define regmap_raw_write_async(map, reg, val, len) \
({ \
	int __ret = 0, __i; \
	for (__i = 0; __i < 3; __i ++) { \
		__ret = regmap_raw_write_async(map, reg, val, len); \
		if (__ret) { \
		    pr_info("I2C retry : regmap_raw_write_async: %d, (0x%x, %p, %d), ret = %d\n", __i, reg, val, len, __ret); \
			usleep_range(1000, 1050); \
			continue; \
		} \
        break; \
	} \
	__ret; \
})
#undef regmap_raw_read
#define regmap_raw_read(map, reg, val, len) \
({ \
	int __ret = 0, __i; \
	for (__i = 0; __i < 3; __i ++) { \
		__ret = regmap_raw_read(map, reg, val, len); \
		if (__ret) { \
		    pr_info("I2C retry : regmap_raw_read: %d, (0x%x, %p, %d), ret = %d\n", __i, reg, val, len, __ret); \
			usleep_range(1000, 1050); \
			continue; \
		} \
	} \
	__ret; \
})
extern const struct soc_enum cs35l43_wm_adsp_fw_enum[];

int cs35l43_wm_adsp1_init(struct cs35l43_wm_adsp *dsp);
int cs35l43_wm_adsp2_init(struct cs35l43_wm_adsp *dsp);
void cs35l43_wm_adsp2_remove(struct cs35l43_wm_adsp *dsp);
int cs35l43_wm_adsp2_component_probe(struct cs35l43_wm_adsp *dsp, struct snd_soc_component *component);
int cs35l43_wm_adsp2_component_remove(struct cs35l43_wm_adsp *dsp, struct snd_soc_component *component);
int cs35l43_wm_halo_init(struct cs35l43_wm_adsp *dsp);

int cs35l43_wm_adsp1_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event);

int cs35l43_wm_adsp_early_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event);

irqreturn_t cs35l43_wm_adsp2_bus_error(int irq, void *data);
irqreturn_t cs35l43_wm_halo_bus_error(int irq, void *data);
irqreturn_t cs35l43_wm_halo_wdt_expire(int irq, void *data);

int cs35l43_wm_adsp_event(struct snd_soc_dapm_widget *w,
		  struct snd_kcontrol *kcontrol, int event);

int cs35l43_wm_adsp2_set_dspclk(struct snd_soc_dapm_widget *w, unsigned int freq);

int cs35l43_wm_adsp2_preloader_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
int cs35l43_wm_adsp2_preloader_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
int cs35l43_wm_adsp_fw_get(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol);
int cs35l43_wm_adsp_fw_put(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol);

int cs35l43_wm_adsp_compr_open(struct cs35l43_wm_adsp *dsp, struct snd_compr_stream *stream);
int cs35l43_wm_adsp_compr_free(struct snd_soc_component *component,
		       struct snd_compr_stream *stream);
int cs35l43_wm_adsp_compr_set_params(struct snd_soc_component *component,
			     struct snd_compr_stream *stream,
			     struct snd_compr_params *params);
int cs35l43_wm_adsp_compr_get_caps(struct snd_soc_component *component,
			   struct snd_compr_stream *stream,
			   struct snd_compr_caps *caps);
int cs35l43_wm_adsp_compr_trigger(struct snd_soc_component *component,
			  struct snd_compr_stream *stream, int cmd);
int cs35l43_wm_adsp_compr_handle_irq(struct cs35l43_wm_adsp *dsp);
int cs35l43_wm_adsp_compr_pointer(struct snd_soc_component *component,
			  struct snd_compr_stream *stream,
			  struct snd_compr_tstamp *tstamp);
int cs35l43_wm_adsp_compr_copy(struct snd_soc_component *component,
		       struct snd_compr_stream *stream,
		       char __user *buf, size_t count);
int cs35l43_wm_adsp_write_ctl(struct cs35l43_wm_adsp *dsp, const char *name,  int type,
		      unsigned int alg, void *buf, size_t len);
int cs35l43_wm_adsp_read_ctl(struct cs35l43_wm_adsp *dsp, const char *name,  int type,
		      unsigned int alg, void *buf, size_t len);
int cs35l43_wm_adsp_load_coeff(struct cs35l43_wm_adsp *dsp);
#endif
