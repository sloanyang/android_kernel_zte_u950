/*
 * drivers/video/tegra/dc/dsi.c
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <mach/clk.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/csi.h>
#include <mach/iomap.h>
#include <linux/nvhost.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "dsi_regs.h"
#include "dsi.h"

#define APB_MISC_GP_MIPI_PAD_CTRL_0 	(TEGRA_APB_MISC_BASE + 0x820)
#define DSIB_MODE_ENABLE		0x2

#define DSI_USE_SYNC_POINTS		0
#define S_TO_MS(x)			(1000 * (x))

#define DSI_MODULE_NOT_INIT		0x0
#define DSI_MODULE_INIT			0x1

#define DSI_LPHS_NOT_INIT		0x0
#define DSI_LPHS_IN_LP_MODE		0x1
#define DSI_LPHS_IN_HS_MODE		0x2

#define DSI_VIDEO_TYPE_NOT_INIT		0x0
#define DSI_VIDEO_TYPE_VIDEO_MODE	0x1
#define DSI_VIDEO_TYPE_CMD_MODE		0x2

#define DSI_DRIVEN_MODE_NOT_INIT	0x0
#define DSI_DRIVEN_MODE_DC		0x1
#define DSI_DRIVEN_MODE_HOST		0x2

#define DSI_PHYCLK_OUT_DIS		0x0
#define DSI_PHYCLK_OUT_EN		0x1

#define DSI_PHYCLK_NOT_INIT		0x0
#define DSI_PHYCLK_CONTINUOUS		0x1
#define DSI_PHYCLK_TX_ONLY		0x2

#define DSI_CLK_BURST_NOT_INIT		0x0
#define DSI_CLK_BURST_NONE_BURST	0x1
#define DSI_CLK_BURST_BURST_MODE	0x2

#define DSI_DC_STREAM_DISABLE		0x0
#define DSI_DC_STREAM_ENABLE		0x1

#define DSI_LP_OP_NOT_INIT		0x0
#define DSI_LP_OP_WRITE			0x1
#define DSI_LP_OP_READ			0x2

static bool enable_read_debug;
module_param(enable_read_debug, bool, 0644);
MODULE_PARM_DESC(enable_read_debug,
		"Enable to print read fifo and return packet type");

struct dsi_status {
	unsigned init:2;

	unsigned lphs:2;

	unsigned vtype:2;
	unsigned driven:2;

	unsigned clk_out:2;
	unsigned clk_mode:2;
	unsigned clk_burst:2;

	unsigned lp_op:2;

	unsigned dc_stream:1;
};

/* source of video data */
enum {
	TEGRA_DSI_DRIVEN_BY_DC,
	TEGRA_DSI_DRIVEN_BY_HOST,
};

struct tegra_dc_dsi_data {
	struct tegra_dc *dc;
	void __iomem *base;
	struct resource *base_res;

	struct clk *dc_clk;
	struct clk *dsi_clk;
	struct clk *dsi_fixed_clk;
	bool clk_ref;

	struct mutex lock;

	/* data from board info */
	struct tegra_dsi_out info;

	struct dsi_status status;

	struct dsi_phy_timing_inclk phy_timing;

	u8 driven_mode;
	u8 controller_index;

	u8 pixel_scaler_mul;
	u8 pixel_scaler_div;

	u32 default_shift_clk_div;
	u32 default_pixel_clk_khz;
	u32 default_hs_clk_khz;

	u32 shift_clk_div;
	u32 target_hs_clk_khz;
	u32 target_lp_clk_khz;

	u32 syncpt_id;
	u32 syncpt_val;

	u16 current_bit_clk_ns;
	u32 current_dsi_clk_khz;

	u32 dsi_control_val;

	bool ulpm;
	bool enabled;
};

const u32 dsi_pkt_seq_reg[NUMOF_PKT_SEQ] = {
	DSI_PKT_SEQ_0_LO,
	DSI_PKT_SEQ_0_HI,
	DSI_PKT_SEQ_1_LO,
	DSI_PKT_SEQ_1_HI,
	DSI_PKT_SEQ_2_LO,
	DSI_PKT_SEQ_2_HI,
	DSI_PKT_SEQ_3_LO,
	DSI_PKT_SEQ_3_HI,
	DSI_PKT_SEQ_4_LO,
	DSI_PKT_SEQ_4_HI,
	DSI_PKT_SEQ_5_LO,
	DSI_PKT_SEQ_5_HI,
};

const u32 dsi_pkt_seq_video_non_burst_syne[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_VE) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(1) |
	PKT_ID2(CMD_HE) | PKT_LEN2(0),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(2) | PKT_ID4(CMD_RGB) | PKT_LEN4(3) |
	PKT_ID5(CMD_BLNK) | PKT_LEN5(4),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(1) |
	PKT_ID2(CMD_HE) | PKT_LEN2(0),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(2) | PKT_ID4(CMD_RGB) | PKT_LEN4(3) |
	PKT_ID5(CMD_BLNK) | PKT_LEN5(4),
};

const u32 dsi_pkt_seq_video_non_burst[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2) |
	PKT_ID2(CMD_RGB) | PKT_LEN2(3),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(4),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2) |
	PKT_ID2(CMD_RGB) | PKT_LEN2(3),
	PKT_ID3(CMD_BLNK) | PKT_LEN3(4),
};

static const u32 dsi_pkt_seq_video_burst[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(7),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(7) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(7),
};

static const u32 dsi_pkt_seq_video_burst_no_eot[NUMOF_PKT_SEQ] = {
	PKT_ID0(CMD_VS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(0),
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_EOT) | PKT_LEN1(0) | PKT_LP,
	0,
	PKT_ID0(CMD_HS) | PKT_LEN0(0) | PKT_ID1(CMD_BLNK) | PKT_LEN1(2)|
	PKT_ID2(CMD_RGB) | PKT_LEN2(3) | PKT_LP,
	PKT_ID0(CMD_EOT) | PKT_LEN0(0),
};

/* TODO: verify with hw about this format */
const u32 dsi_pkt_seq_cmd_mode[NUMOF_PKT_SEQ] = {
	0,
	0,
	0,
	0,
	0,
	0,
	PKT_ID0(CMD_LONGW) | PKT_LEN0(3) | PKT_ID1(CMD_EOT) | PKT_LEN1(7),
	0,
	0,
	0,
	PKT_ID0(CMD_LONGW) | PKT_LEN0(3) | PKT_ID1(CMD_EOT) | PKT_LEN1(7),
	0,
};

const u32 init_reg[] = {
	DSI_INT_ENABLE,
	DSI_INT_STATUS,
	DSI_INT_MASK,
	DSI_INIT_SEQ_DATA_0,
	DSI_INIT_SEQ_DATA_1,
	DSI_INIT_SEQ_DATA_2,
	DSI_INIT_SEQ_DATA_3,
	DSI_INIT_SEQ_DATA_4,
	DSI_INIT_SEQ_DATA_5,
	DSI_INIT_SEQ_DATA_6,
	DSI_INIT_SEQ_DATA_7,
	DSI_DCS_CMDS,
	DSI_PKT_SEQ_0_LO,
	DSI_PKT_SEQ_1_LO,
	DSI_PKT_SEQ_2_LO,
	DSI_PKT_SEQ_3_LO,
	DSI_PKT_SEQ_4_LO,
	DSI_PKT_SEQ_5_LO,
	DSI_PKT_SEQ_0_HI,
	DSI_PKT_SEQ_1_HI,
	DSI_PKT_SEQ_2_HI,
	DSI_PKT_SEQ_3_HI,
	DSI_PKT_SEQ_4_HI,
	DSI_PKT_SEQ_5_HI,
	DSI_CONTROL,
	DSI_HOST_DSI_CONTROL,
	DSI_PAD_CONTROL,
	DSI_PAD_CONTROL_CD,
	DSI_SOL_DELAY,
	DSI_MAX_THRESHOLD,
	DSI_TRIGGER,
	DSI_TX_CRC,
	DSI_INIT_SEQ_CONTROL,
	DSI_PKT_LEN_0_1,
	DSI_PKT_LEN_2_3,
	DSI_PKT_LEN_4_5,
	DSI_PKT_LEN_6_7,
};
inline unsigned long tegra_dsi_readl(struct tegra_dc_dsi_data *dsi, u32 reg)
{
	unsigned long ret;

	BUG_ON(!nvhost_module_powered(nvhost_get_host(dsi->dc->ndev)->dev));
	ret = readl(dsi->base + reg * 4);
	trace_printk("readl %p=%#08lx\n", dsi->base + reg * 4, ret);
	return ret;
}
EXPORT_SYMBOL(tegra_dsi_readl);

inline void tegra_dsi_writel(struct tegra_dc_dsi_data *dsi, u32 val, u32 reg)
{
	BUG_ON(!nvhost_module_powered(nvhost_get_host(dsi->dc->ndev)->dev));
	trace_printk("writel %p=%#08x\n", dsi->base + reg * 4, val);
	writel(val, dsi->base + reg * 4);
}
EXPORT_SYMBOL(tegra_dsi_writel);

#ifdef CONFIG_DEBUG_FS
static int dbg_dsi_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;

#define DUMP_REG(a) do {						\
		seq_printf(s, "%-32s\t%03x\t%08lx\n",			\
		       #a, a, tegra_dsi_readl(dsi, a));		\
	} while (0)

	tegra_dc_io_start(dsi->dc);
	clk_enable(dsi->dsi_clk);

	DUMP_REG(DSI_INCR_SYNCPT_CNTRL);
	DUMP_REG(DSI_INCR_SYNCPT_ERROR);
	DUMP_REG(DSI_CTXSW);
	DUMP_REG(DSI_POWER_CONTROL);
	DUMP_REG(DSI_INT_ENABLE);
	DUMP_REG(DSI_CONTROL);
	DUMP_REG(DSI_SOL_DELAY);
	DUMP_REG(DSI_MAX_THRESHOLD);
	DUMP_REG(DSI_TRIGGER);
	DUMP_REG(DSI_TX_CRC);
	DUMP_REG(DSI_STATUS);
	DUMP_REG(DSI_INIT_SEQ_CONTROL);
	DUMP_REG(DSI_INIT_SEQ_DATA_0);
	DUMP_REG(DSI_INIT_SEQ_DATA_1);
	DUMP_REG(DSI_INIT_SEQ_DATA_2);
	DUMP_REG(DSI_INIT_SEQ_DATA_3);
	DUMP_REG(DSI_INIT_SEQ_DATA_4);
	DUMP_REG(DSI_INIT_SEQ_DATA_5);
	DUMP_REG(DSI_INIT_SEQ_DATA_6);
	DUMP_REG(DSI_INIT_SEQ_DATA_7);
	DUMP_REG(DSI_PKT_SEQ_0_LO);
	DUMP_REG(DSI_PKT_SEQ_0_HI);
	DUMP_REG(DSI_PKT_SEQ_1_LO);
	DUMP_REG(DSI_PKT_SEQ_1_HI);
	DUMP_REG(DSI_PKT_SEQ_2_LO);
	DUMP_REG(DSI_PKT_SEQ_2_HI);
	DUMP_REG(DSI_PKT_SEQ_3_LO);
	DUMP_REG(DSI_PKT_SEQ_3_HI);
	DUMP_REG(DSI_PKT_SEQ_4_LO);
	DUMP_REG(DSI_PKT_SEQ_4_HI);
	DUMP_REG(DSI_PKT_SEQ_5_LO);
	DUMP_REG(DSI_PKT_SEQ_5_HI);
	DUMP_REG(DSI_DCS_CMDS);
	DUMP_REG(DSI_PKT_LEN_0_1);
	DUMP_REG(DSI_PKT_LEN_2_3);
	DUMP_REG(DSI_PKT_LEN_4_5);
	DUMP_REG(DSI_PKT_LEN_6_7);
	DUMP_REG(DSI_PHY_TIMING_0);
	DUMP_REG(DSI_PHY_TIMING_1);
	DUMP_REG(DSI_PHY_TIMING_2);
	DUMP_REG(DSI_BTA_TIMING);
	DUMP_REG(DSI_TIMEOUT_0);
	DUMP_REG(DSI_TIMEOUT_1);
	DUMP_REG(DSI_TO_TALLY);
	DUMP_REG(DSI_PAD_CONTROL);
	DUMP_REG(DSI_PAD_CONTROL_CD);
	DUMP_REG(DSI_PAD_CD_STATUS);
	DUMP_REG(DSI_VID_MODE_CONTROL);
#undef DUMP_REG

	clk_disable(dsi->dsi_clk);
	tegra_dc_io_end(dsi->dc);

	return 0;
}

static int dbg_dsi_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dsi_show, inode->i_private);
}

static const struct file_operations dbg_fops = {
	.open		= dbg_dsi_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *dsidir;

static void tegra_dc_dsi_debug_create(struct tegra_dc_dsi_data *dsi)
{
	struct dentry *retval;

	dsidir = debugfs_create_dir("tegra_dsi", NULL);
	if (!dsidir)
		return;
	retval = debugfs_create_file("regs", S_IRUGO, dsidir, dsi,
		&dbg_fops);
	if (!retval)
		goto free_out;
	return;
free_out:
	debugfs_remove_recursive(dsidir);
	dsidir = NULL;
	return;
}
#else
static inline void tegra_dc_dsi_debug_create(struct tegra_dc_dsi_data *dsi)
{ }
#endif

static int tegra_dsi_syncpt(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int ret;

	ret = 0;

	dsi->syncpt_val = nvhost_syncpt_read(
			&nvhost_get_host(dsi->dc->ndev)->syncpt,
			dsi->syncpt_id);

	val = DSI_INCR_SYNCPT_COND(OP_DONE) |
		DSI_INCR_SYNCPT_INDX(dsi->syncpt_id);
	tegra_dsi_writel(dsi, val, DSI_INCR_SYNCPT);

	/* TODO: Use interrupt rather than polling */
	ret = nvhost_syncpt_wait(&nvhost_get_host(dsi->dc->ndev)->syncpt,
		dsi->syncpt_id, dsi->syncpt_val + 1);
	if (ret < 0) {
		dev_err(&dsi->dc->ndev->dev, "DSI sync point failure\n");
		goto fail;
	}

	(dsi->syncpt_val)++;
	return 0;
fail:
	return ret;
}

static u32 tegra_dsi_get_hs_clk_rate(struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_clock_rate_khz;

	switch (dsi->info.video_burst_mode) {
	case TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED:
	case TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED:
		/* Calculate DSI HS clock rate for DSI burst mode */
		dsi_clock_rate_khz = dsi->default_pixel_clk_khz *
							dsi->shift_clk_div;
		break;
	case TEGRA_DSI_VIDEO_NONE_BURST_MODE:
	case TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END:
	case TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED:
	default:
		/* Clock rate is default DSI clock rate for non-burst mode */
		dsi_clock_rate_khz = dsi->default_hs_clk_khz;
		break;
	}

	return dsi_clock_rate_khz;
}

static u32 tegra_dsi_get_lp_clk_rate(struct tegra_dc_dsi_data *dsi, u8 lp_op)
{
	u32 dsi_clock_rate_khz;

	if (dsi->info.enable_hs_clock_on_lp_cmd_mode)
		if (dsi->info.hs_clk_in_lp_cmd_mode_freq_khz)
			dsi_clock_rate_khz =
				dsi->info.hs_clk_in_lp_cmd_mode_freq_khz;
		else
			dsi_clock_rate_khz = tegra_dsi_get_hs_clk_rate(dsi);
	else
		if (lp_op == DSI_LP_OP_READ)
			dsi_clock_rate_khz =
				dsi->info.lp_read_cmd_mode_freq_khz;
		else
			dsi_clock_rate_khz =
				dsi->info.lp_cmd_mode_freq_khz;

	return dsi_clock_rate_khz;
}

static u32 tegra_dsi_get_shift_clk_div(struct tegra_dc_dsi_data *dsi)
{
	u32 shift_clk_div;
	u32 max_shift_clk_div;
	u32 burst_width;
	u32 burst_width_max;

	/* Get the real value of default shift_clk_div. default_shift_clk_div
	 * holds the real value of shift_clk_div.
	 */
	shift_clk_div = dsi->default_shift_clk_div;

	/* Calculate shift_clk_div which can matche the video_burst_mode. */
	if (dsi->info.video_burst_mode >=
			TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED) {
		/* The max_shift_clk_div is multiplied by 10 to save the
		 * fraction
		 */
		if (dsi->info.max_panel_freq_khz >= dsi->default_hs_clk_khz)
			max_shift_clk_div = dsi->info.max_panel_freq_khz
				* shift_clk_div * 10 / dsi->default_hs_clk_khz;
		else
			max_shift_clk_div = shift_clk_div * 10;

		burst_width = dsi->info.video_burst_mode
				- TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;
		burst_width_max = TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED
				- TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;

		shift_clk_div = (max_shift_clk_div - shift_clk_div * 10) *
			burst_width / (burst_width_max * 10) + shift_clk_div;
	}

	return shift_clk_div;
}

static void tegra_dsi_init_sw(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi)
{
	u32 h_width_pixels;
	u32 v_width_lines;
	u32 pixel_clk_hz;
	u32 byte_clk_hz;
	u32 plld_clk_mhz;

	switch (dsi->info.pixel_format) {
	case TEGRA_DSI_PIXEL_FORMAT_16BIT_P:
		/* 2 bytes per pixel */
		dsi->pixel_scaler_mul = 2;
		dsi->pixel_scaler_div = 1;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_P:
		/* 2.25 bytes per pixel */
		dsi->pixel_scaler_mul = 9;
		dsi->pixel_scaler_div = 4;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_NP:
	case TEGRA_DSI_PIXEL_FORMAT_24BIT_P:
		/* 3 bytes per pixel */
		dsi->pixel_scaler_mul = 3;
		dsi->pixel_scaler_div = 1;
		break;
	default:
		break;
	}

	dsi->controller_index = dc->ndev->id;
	dsi->ulpm = false;
	dsi->enabled = false;
	dsi->clk_ref = false;

	dsi->dsi_control_val =
			DSI_CONTROL_VIRTUAL_CHANNEL(dsi->info.virtual_channel) |
			DSI_CONTROL_NUM_DATA_LANES(dsi->info.n_data_lanes - 1) |
			DSI_CONTROL_VID_SOURCE(dsi->controller_index) |
			DSI_CONTROL_DATA_FORMAT(dsi->info.pixel_format);

	/* Below we are going to calculate dsi and dc clock rate.
	 * Calcuate the horizontal and vertical width.
	 */
	h_width_pixels = dc->mode.h_back_porch + dc->mode.h_front_porch +
			dc->mode.h_sync_width + dc->mode.h_active;
	v_width_lines = dc->mode.v_back_porch + dc->mode.v_front_porch +
			dc->mode.v_sync_width + dc->mode.v_active;

	/* Calculate minimum required pixel rate. */
	pixel_clk_hz = h_width_pixels * v_width_lines * dsi->info.refresh_rate;
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		if (dsi->info.rated_refresh_rate >= dsi->info.refresh_rate)
			dev_info(&dc->ndev->dev, "DSI: measured refresh rate "
				"should be larger than rated refresh rate.\n");
		dc->mode.rated_pclk = h_width_pixels * v_width_lines *
						dsi->info.rated_refresh_rate;
	}

	/* Calculate minimum byte rate on DSI interface. */
	byte_clk_hz = (pixel_clk_hz * dsi->pixel_scaler_mul) /
			(dsi->pixel_scaler_div * dsi->info.n_data_lanes);

	/* Round up to multiple of mega hz. */
	plld_clk_mhz = DIV_ROUND_UP((byte_clk_hz * NUMOF_BIT_PER_BYTE),
								1000000);

	/* Calculate default real shift_clk_div. */
	dsi->default_shift_clk_div = (NUMOF_BIT_PER_BYTE / 2) *
		dsi->pixel_scaler_mul / (dsi->pixel_scaler_div *
		dsi->info.n_data_lanes);
	/* Calculate default DSI hs clock. DSI interface is double data rate.
	 * Data is transferred on both rising and falling edge of clk, div by 2
	 * to get the actual clock rate.
	 */
	dsi->default_hs_clk_khz = plld_clk_mhz * 1000 / 2;
	dsi->default_pixel_clk_khz = plld_clk_mhz * 1000 / 2
						/ dsi->default_shift_clk_div;

	/* Get the actual shift_clk_div and clock rates. */
	dsi->shift_clk_div = tegra_dsi_get_shift_clk_div(dsi);
	dsi->target_lp_clk_khz =
			tegra_dsi_get_lp_clk_rate(dsi, DSI_LP_OP_WRITE);
	dsi->target_hs_clk_khz = tegra_dsi_get_hs_clk_rate(dsi);

	dev_info(&dc->ndev->dev, "DSI: HS clock rate is %d\n",
							dsi->target_hs_clk_khz);

	dsi->controller_index = dc->ndev->id;

#if DSI_USE_SYNC_POINTS
	dsi->syncpt_id = NVSYNCPT_DSI;
#endif

	/*
	 * Force video clock to be continuous mode if
	 * enable_hs_clock_on_lp_cmd_mode is set
	 */
	if (dsi->info.enable_hs_clock_on_lp_cmd_mode) {
		if (dsi->info.video_clock_mode !=
					TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS)
			dev_warn(&dc->ndev->dev,
				"Force clock continuous mode\n");

		dsi->info.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS;
	}

}

#define SELECT_T_PHY(platform_t_phy_ns, default_phy, clk_ns, hw_inc) ( \
(platform_t_phy_ns) ? ( \
((DSI_CONVERT_T_PHY_NS_TO_T_PHY(platform_t_phy_ns, clk_ns, hw_inc)) < 0 ? 0 : \
(DSI_CONVERT_T_PHY_NS_TO_T_PHY(platform_t_phy_ns, clk_ns, hw_inc)))) : \
((default_phy) < 0 ? 0 : (default_phy)))

static void tegra_dsi_get_clk_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ns)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns,
		T_TLPX_DEFAULT(clk_ns), clk_ns, T_TLPX_HW_INC);

	phy_timing_clk->t_clktrail = SELECT_T_PHY(
		dsi->info.phy_timing.t_clktrail_ns,
		T_CLKTRAIL_DEFAULT(clk_ns), clk_ns, T_CLKTRAIL_HW_INC);

	phy_timing_clk->t_clkpost = SELECT_T_PHY(
		dsi->info.phy_timing.t_clkpost_ns,
		T_CLKPOST_DEFAULT(clk_ns), clk_ns, T_CLKPOST_HW_INC);

	phy_timing_clk->t_clkzero = SELECT_T_PHY(
		dsi->info.phy_timing.t_clkzero_ns,
		T_CLKZERO_DEFAULT(clk_ns), clk_ns, T_CLKZERO_HW_INC);

	phy_timing_clk->t_clkprepare = SELECT_T_PHY(
		dsi->info.phy_timing.t_clkprepare_ns,
		T_CLKPREPARE_DEFAULT(clk_ns), clk_ns, T_CLKPREPARE_HW_INC);

	phy_timing_clk->t_clkpre = SELECT_T_PHY(
		dsi->info.phy_timing.t_clkpre_ns,
		T_CLKPRE_DEFAULT, clk_ns, T_CLKPRE_HW_INC);
}

static void tegra_dsi_get_hs_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ns)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns,
		T_TLPX_DEFAULT(clk_ns), clk_ns, T_TLPX_HW_INC);

	phy_timing_clk->t_hsdexit = SELECT_T_PHY(
		dsi->info.phy_timing.t_hsdexit_ns,
		T_HSEXIT_DEFAULT(clk_ns), clk_ns, T_HSEXIT_HW_INC);

	phy_timing_clk->t_hstrail = SELECT_T_PHY(
		dsi->info.phy_timing.t_hstrail_ns,
		T_HSTRAIL_DEFAULT(clk_ns), clk_ns, T_HSTRAIL_HW_INC);

	phy_timing_clk->t_datzero = SELECT_T_PHY(
		dsi->info.phy_timing.t_datzero_ns,
		T_DATZERO_DEFAULT(clk_ns), clk_ns, T_DATZERO_HW_INC);

	phy_timing_clk->t_hsprepare = SELECT_T_PHY(
		dsi->info.phy_timing.t_hsprepare_ns,
		T_HSPREPARE_DEFAULT(clk_ns), clk_ns, T_HSPREPARE_HW_INC);
}

static void tegra_dsi_get_escape_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ns)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns,
		T_TLPX_DEFAULT(clk_ns), clk_ns, T_TLPX_HW_INC);
}

static void tegra_dsi_get_bta_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ns)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns,
		T_TLPX_DEFAULT(clk_ns), clk_ns, T_TLPX_HW_INC);

	phy_timing_clk->t_taget = SELECT_T_PHY(
		dsi->info.phy_timing.t_taget_ns,
		T_TAGET_DEFAULT(clk_ns), clk_ns, T_TAGET_HW_INC);

	phy_timing_clk->t_tasure = SELECT_T_PHY(
		dsi->info.phy_timing.t_tasure_ns,
		T_TASURE_DEFAULT(clk_ns), clk_ns, T_TASURE_HW_INC);

	phy_timing_clk->t_tago = SELECT_T_PHY(
		dsi->info.phy_timing.t_tago_ns,
		T_TAGO_DEFAULT(clk_ns), clk_ns, T_TAGO_HW_INC);
}

static void tegra_dsi_get_ulps_phy_timing(struct tegra_dc_dsi_data *dsi,
		struct dsi_phy_timing_inclk *phy_timing_clk, u32 clk_ns)
{
	phy_timing_clk->t_tlpx = SELECT_T_PHY(
		dsi->info.phy_timing.t_tlpx_ns,
		T_TLPX_DEFAULT(clk_ns), clk_ns, T_TLPX_HW_INC);

	phy_timing_clk->t_wakeup = SELECT_T_PHY(
		dsi->info.phy_timing.t_wakeup_ns,
		T_WAKEUP_DEFAULT, clk_ns, T_WAKEUP_HW_INC);
}

#undef SELECT_T_PHY

static void tegra_dsi_get_phy_timing(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing_clk,
				u32 clk_ns, u8 lphs)
{
	if (lphs == DSI_LPHS_IN_HS_MODE) {
		tegra_dsi_get_clk_phy_timing(dsi, phy_timing_clk, clk_ns);
		tegra_dsi_get_hs_phy_timing(dsi, phy_timing_clk, clk_ns);
	} else {
		/* default is LP mode */
		tegra_dsi_get_escape_phy_timing(dsi, phy_timing_clk, clk_ns);
		tegra_dsi_get_bta_phy_timing(dsi, phy_timing_clk, clk_ns);
		tegra_dsi_get_ulps_phy_timing(dsi, phy_timing_clk, clk_ns);
		if (dsi->info.enable_hs_clock_on_lp_cmd_mode)
			tegra_dsi_get_clk_phy_timing
				(dsi, phy_timing_clk, clk_ns);
	}
}

static int tegra_dsi_mipi_phy_timing_range(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing,
				u32 clk_ns, u8 lphs)
{
#define CHECK_RANGE(val, min, max) ( \
		((min) == NOT_DEFINED ? 0 : (val) < (min)) || \
		((max) == NOT_DEFINED ? 0 : (val) > (max)) ? -EINVAL : 0)

	int err = 0;

	err = CHECK_RANGE(
	DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_tlpx, clk_ns, T_TLPX_HW_INC),
			MIPI_T_TLPX_NS_MIN, MIPI_T_TLPX_NS_MAX);
	if (err < 0) {
		dev_warn(&dsi->dc->ndev->dev,
			"dsi: Tlpx mipi range violated\n");
		goto fail;
	}

	if (lphs == DSI_LPHS_IN_HS_MODE) {
		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_hsdexit, clk_ns, T_HSEXIT_HW_INC),
			MIPI_T_HSEXIT_NS_MIN, MIPI_T_HSEXIT_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: HsExit mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_hstrail, clk_ns, T_HSTRAIL_HW_INC),
			MIPI_T_HSTRAIL_NS_MIN(clk_ns), MIPI_T_HSTRAIL_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: HsTrail mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_datzero, clk_ns, T_DATZERO_HW_INC),
			MIPI_T_HSZERO_NS_MIN, MIPI_T_HSZERO_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: HsZero mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_hsprepare, clk_ns, T_HSPREPARE_HW_INC),
			MIPI_T_HSPREPARE_NS_MIN(clk_ns),
			MIPI_T_HSPREPARE_NS_MAX(clk_ns));
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: HsPrepare mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_hsprepare, clk_ns, T_HSPREPARE_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_datzero, clk_ns, T_DATZERO_HW_INC),
			MIPI_T_HSPREPARE_ADD_HSZERO_NS_MIN(clk_ns),
			MIPI_T_HSPREPARE_ADD_HSZERO_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
			"dsi: HsPrepare + HsZero mipi range violated\n");
			goto fail;
		}
	} else {
		/* default is LP mode */
		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_wakeup, clk_ns, T_WAKEUP_HW_INC),
			MIPI_T_WAKEUP_NS_MIN, MIPI_T_WAKEUP_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: WakeUp mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_tasure, clk_ns, T_TASURE_HW_INC),
			MIPI_T_TASURE_NS_MIN(DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_tlpx, clk_ns, T_TLPX_HW_INC)),
			MIPI_T_TASURE_NS_MAX(DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_tlpx, clk_ns, T_TLPX_HW_INC)));
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: TaSure mipi range violated\n");
			goto fail;
		}
	}

	if (lphs == DSI_LPHS_IN_HS_MODE ||
		dsi->info.enable_hs_clock_on_lp_cmd_mode) {
		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_clktrail, clk_ns, T_CLKTRAIL_HW_INC),
			MIPI_T_CLKTRAIL_NS_MIN, MIPI_T_CLKTRAIL_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkTrail mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_clkpost, clk_ns, T_CLKPOST_HW_INC),
			MIPI_T_CLKPOST_NS_MIN(clk_ns), MIPI_T_CLKPOST_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkPost mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_clkzero, clk_ns, T_CLKZERO_HW_INC),
			MIPI_T_CLKZERO_NS_MIN, MIPI_T_CLKZERO_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkZero mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_clkprepare, clk_ns, T_CLKPREPARE_HW_INC),
			MIPI_T_CLKPREPARE_NS_MIN, MIPI_T_CLKPREPARE_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkPrepare mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_clkpre, clk_ns, T_CLKPRE_HW_INC),
			MIPI_T_CLKPRE_NS_MIN, MIPI_T_CLKPRE_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: ClkPre mipi range violated\n");
			goto fail;
		}

		err = CHECK_RANGE(
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_clkprepare, clk_ns, T_CLKPREPARE_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
			phy_timing->t_clkzero, clk_ns, T_CLKZERO_HW_INC),
			MIPI_T_CLKPREPARE_ADD_CLKZERO_NS_MIN,
			MIPI_T_CLKPREPARE_ADD_CLKZERO_NS_MAX);
		if (err < 0) {
			dev_warn(&dsi->dc->ndev->dev,
			"dsi: ClkPrepare + ClkZero mipi range violated\n");
			goto fail;
		}
	}
fail:
#undef CHECK_RANGE
	return err;
}

static int tegra_dsi_hs_phy_len(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing,
				u32 clk_ns, u8 lphs)
{
	u32 hs_t_phy_ns;
	u32 clk_t_phy_ns;
	u32 t_phy_ns;
	u32 h_blank_ns;
	struct tegra_dc_mode *modes;
	u32 t_pix_ns;
	int err = 0;

	if (!(lphs == DSI_LPHS_IN_HS_MODE))
		goto fail;

	modes = dsi->dc->out->modes;
	t_pix_ns = clk_ns * BITS_PER_BYTE *
		dsi->pixel_scaler_mul / dsi->pixel_scaler_div;

	hs_t_phy_ns =
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_tlpx, clk_ns, T_TLPX_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_hsprepare, clk_ns, T_HSPREPARE_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_datzero, clk_ns, T_DATZERO_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_hstrail, clk_ns, T_HSTRAIL_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_hsdexit, clk_ns, T_HSEXIT_HW_INC);

	clk_t_phy_ns =
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_clkpost, clk_ns, T_CLKPOST_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_clktrail, clk_ns, T_CLKTRAIL_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_hsdexit, clk_ns, T_HSEXIT_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_tlpx, clk_ns, T_TLPX_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_clkprepare, clk_ns, T_CLKPREPARE_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_clkzero, clk_ns, T_CLKZERO_HW_INC) +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_clkpre, clk_ns, T_CLKPRE_HW_INC);

	h_blank_ns = t_pix_ns * (modes->h_sync_width + modes->h_back_porch +
						modes->h_front_porch);

	/* Extra tlpx and byte cycle required by dsi HW */
	t_phy_ns = dsi->info.n_data_lanes * (hs_t_phy_ns + clk_t_phy_ns +
		DSI_CONVERT_T_PHY_TO_T_PHY_NS(
		phy_timing->t_tlpx, clk_ns, T_TLPX_HW_INC) +
		clk_ns * BITS_PER_BYTE);

	if (h_blank_ns < t_phy_ns) {
		err = -EINVAL;
	//	dev_err(&dsi->dc->ndev->dev,
	//		"dsi: Hblank is smaller than HS trans phy timing\n");
		goto fail;
	}

	return 0;
fail:
	return err;
}

static int tegra_dsi_constraint_phy_timing(struct tegra_dc_dsi_data *dsi,
				struct dsi_phy_timing_inclk *phy_timing,
				u32 clk_ns, u8 lphs)
{
	int err = 0;

	err = tegra_dsi_mipi_phy_timing_range(dsi, phy_timing, clk_ns, lphs);
	if (err < 0) {
		dev_warn(&dsi->dc->ndev->dev, "dsi: mipi range violated\n");
		goto fail;
	}

	err = tegra_dsi_hs_phy_len(dsi, phy_timing, clk_ns, lphs);
	if (err < 0) {
//		dev_err(&dsi->dc->ndev->dev, "dsi: Hblank too short\n");
		goto fail;
	}

	/* TODO: add more contraints */
fail:
	return err;
}

static void tegra_dsi_set_phy_timing(struct tegra_dc_dsi_data *dsi, u8 lphs)
{
	u32 val;
	struct dsi_phy_timing_inclk phy_timing = dsi->phy_timing;

	tegra_dsi_get_phy_timing
		(dsi, &phy_timing, dsi->current_bit_clk_ns, lphs);

	tegra_dsi_constraint_phy_timing(dsi, &phy_timing,
					dsi->current_bit_clk_ns, lphs);

	val = DSI_PHY_TIMING_0_THSDEXIT(phy_timing.t_hsdexit) |
			DSI_PHY_TIMING_0_THSTRAIL(phy_timing.t_hstrail) |
			DSI_PHY_TIMING_0_TDATZERO(phy_timing.t_datzero) |
			DSI_PHY_TIMING_0_THSPREPR(phy_timing.t_hsprepare);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_0);

	val = DSI_PHY_TIMING_1_TCLKTRAIL(phy_timing.t_clktrail) |
			DSI_PHY_TIMING_1_TCLKPOST(phy_timing.t_clkpost) |
			DSI_PHY_TIMING_1_TCLKZERO(phy_timing.t_clkzero) |
			DSI_PHY_TIMING_1_TTLPX(phy_timing.t_tlpx);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_1);

	val = DSI_PHY_TIMING_2_TCLKPREPARE(phy_timing.t_clkprepare) |
		DSI_PHY_TIMING_2_TCLKPRE(phy_timing.t_clkpre) |
			DSI_PHY_TIMING_2_TWAKEUP(phy_timing.t_wakeup);
	tegra_dsi_writel(dsi, val, DSI_PHY_TIMING_2);

	val = DSI_BTA_TIMING_TTAGET(phy_timing.t_taget) |
			DSI_BTA_TIMING_TTASURE(phy_timing.t_tasure) |
			DSI_BTA_TIMING_TTAGO(phy_timing.t_tago);
	tegra_dsi_writel(dsi, val, DSI_BTA_TIMING);

	dsi->phy_timing = phy_timing;
}

static u32 tegra_dsi_sol_delay_burst(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_to_pixel_clk_ratio;
	u32 temp;
	u32 temp1;
	u32 mipi_clk_adj_kHz;
	u32 sol_delay;
	struct tegra_dc_mode *dc_modes = &dc->mode;

	/* Get Fdsi/Fpixel ration (note: Fdsi is in bit format) */
	dsi_to_pixel_clk_ratio = (dsi->current_dsi_clk_khz * 2 +
		dsi->default_pixel_clk_khz - 1) / dsi->default_pixel_clk_khz;

	/* Convert Fdsi to byte format */
	dsi_to_pixel_clk_ratio *= 1000/8;

	/* Multiplying by 1000 so that we don't loose the fraction part */
	temp = dc_modes->h_active * 1000;
	temp1 = dc_modes->h_active + dc_modes->h_back_porch +
			dc_modes->h_sync_width;

	sol_delay = temp1 * dsi_to_pixel_clk_ratio -
			temp * dsi->pixel_scaler_mul /
			(dsi->pixel_scaler_div * dsi->info.n_data_lanes);

	/* Do rounding on sol delay */
	sol_delay = (sol_delay + 1000 - 1)/1000;

	/* TODO:
	 * 1. find out the correct sol fifo depth to use
	 * 2. verify with hw about the clamping function
	 */
	if (sol_delay > (480 * 4)) {
		sol_delay = (480 * 4);
		mipi_clk_adj_kHz = sol_delay +
			(dc_modes->h_active * dsi->pixel_scaler_mul) /
			(dsi->info.n_data_lanes * dsi->pixel_scaler_div);

		mipi_clk_adj_kHz *= (dsi->default_pixel_clk_khz / temp1);

		mipi_clk_adj_kHz *= 4;
	}

	dsi->target_hs_clk_khz = mipi_clk_adj_kHz;

	return sol_delay;
}

static void tegra_dsi_set_sol_delay(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	u32 sol_delay;

	if (dsi->info.video_burst_mode == TEGRA_DSI_VIDEO_NONE_BURST_MODE ||
		dsi->info.video_burst_mode ==
				TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END) {
#define VIDEO_FIFO_LATENCY_PIXEL_CLK 8
		sol_delay = VIDEO_FIFO_LATENCY_PIXEL_CLK *
			dsi->pixel_scaler_mul / dsi->pixel_scaler_div;
#undef VIDEO_FIFO_LATENCY_PIXEL_CLK
		dsi->status.clk_burst = DSI_CLK_BURST_NONE_BURST;
	} else {
		sol_delay = tegra_dsi_sol_delay_burst(dc, dsi);
		dsi->status.clk_burst = DSI_CLK_BURST_BURST_MODE;
	}

	tegra_dsi_writel(dsi, DSI_SOL_DELAY_SOL_DELAY(sol_delay),
								DSI_SOL_DELAY);
}

static void tegra_dsi_set_timeout(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 bytes_per_frame;
	u32 timeout = 0;

	/* TODO: verify the following equation */
	bytes_per_frame = dsi->current_dsi_clk_khz * 1000 * 2 /
						(dsi->info.refresh_rate * 8);
	timeout = bytes_per_frame / DSI_CYCLE_COUNTER_VALUE;
	timeout = (timeout + DSI_HTX_TO_MARGIN) & 0xffff;

	val = DSI_TIMEOUT_0_LRXH_TO(DSI_LRXH_TO_VALUE) |
			DSI_TIMEOUT_0_HTX_TO(timeout);
	tegra_dsi_writel(dsi, val, DSI_TIMEOUT_0);

	if (dsi->info.panel_reset_timeout_msec)
		timeout = (dsi->info.panel_reset_timeout_msec * 1000*1000)
					/ dsi->current_bit_clk_ns;
	else
		timeout = DSI_PR_TO_VALUE;

	val = DSI_TIMEOUT_1_PR_TO(timeout) |
		DSI_TIMEOUT_1_TA_TO(DSI_TA_TO_VALUE);
	tegra_dsi_writel(dsi, val, DSI_TIMEOUT_1);

	val = DSI_TO_TALLY_P_RESET_STATUS(IN_RESET) |
		DSI_TO_TALLY_TA_TALLY(DSI_TA_TALLY_VALUE)|
		DSI_TO_TALLY_LRXH_TALLY(DSI_LRXH_TALLY_VALUE)|
		DSI_TO_TALLY_HTX_TALLY(DSI_HTX_TALLY_VALUE);
	tegra_dsi_writel(dsi, val, DSI_TO_TALLY);
}

static void tegra_dsi_setup_video_mode_pkt_length(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 hact_pkt_len;
	u32 hsa_pkt_len;
	u32 hbp_pkt_len;
	u32 hfp_pkt_len;

	hact_pkt_len = dc->mode.h_active * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hsa_pkt_len = dc->mode.h_sync_width * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hbp_pkt_len = dc->mode.h_back_porch * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;
	hfp_pkt_len = dc->mode.h_front_porch * dsi->pixel_scaler_mul /
							dsi->pixel_scaler_div;

	if (dsi->info.video_burst_mode !=
				TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END)
		hbp_pkt_len += hsa_pkt_len;

	hsa_pkt_len -= DSI_HSYNC_BLNK_PKT_OVERHEAD;
	hbp_pkt_len -= DSI_HBACK_PORCH_PKT_OVERHEAD;
	hfp_pkt_len -= DSI_HFRONT_PORCH_PKT_OVERHEAD;

	val = DSI_PKT_LEN_0_1_LENGTH_0(0) |
			DSI_PKT_LEN_0_1_LENGTH_1(hsa_pkt_len);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_0_1);

	val = DSI_PKT_LEN_2_3_LENGTH_2(hbp_pkt_len) |
			DSI_PKT_LEN_2_3_LENGTH_3(hact_pkt_len);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_2_3);

	val = DSI_PKT_LEN_4_5_LENGTH_4(hfp_pkt_len) |
			DSI_PKT_LEN_4_5_LENGTH_5(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_4_5);

	val = DSI_PKT_LEN_6_7_LENGTH_6(0) | DSI_PKT_LEN_6_7_LENGTH_7(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_6_7);
}

static void tegra_dsi_setup_cmd_mode_pkt_length(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	unsigned long	val;
	unsigned long	act_bytes;

	act_bytes = dc->mode.h_active * dsi->pixel_scaler_mul /
			dsi->pixel_scaler_div + 1;

	val = DSI_PKT_LEN_0_1_LENGTH_0(0) | DSI_PKT_LEN_0_1_LENGTH_1(0);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_0_1);

	val = DSI_PKT_LEN_2_3_LENGTH_2(0) | DSI_PKT_LEN_2_3_LENGTH_3(act_bytes);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_2_3);

	val = DSI_PKT_LEN_4_5_LENGTH_4(0) | DSI_PKT_LEN_4_5_LENGTH_5(act_bytes);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_4_5);

	val = DSI_PKT_LEN_6_7_LENGTH_6(0) | DSI_PKT_LEN_6_7_LENGTH_7(0x0f0f);
	tegra_dsi_writel(dsi, val, DSI_PKT_LEN_6_7);
}

static void tegra_dsi_set_pkt_length(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_HOST)
		return;

	if (dsi->info.video_data_type == TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE)
		tegra_dsi_setup_video_mode_pkt_length(dc, dsi);
	else
		tegra_dsi_setup_cmd_mode_pkt_length(dc, dsi);
}

static void tegra_dsi_set_pkt_seq(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	const u32 *pkt_seq;
	u32 rgb_info;
	u32 pkt_seq_3_5_rgb_lo;
	u32 pkt_seq_3_5_rgb_hi;
	u32	val;
	u32 reg;
	u8  i;

	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_HOST)
		return;

	switch (dsi->info.pixel_format) {
	case TEGRA_DSI_PIXEL_FORMAT_16BIT_P:
		rgb_info = CMD_RGB_16BPP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_P:
		rgb_info = CMD_RGB_18BPP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_18BIT_NP:
		rgb_info = CMD_RGB_18BPPNP;
		break;
	case TEGRA_DSI_PIXEL_FORMAT_24BIT_P:
	default:
		rgb_info = CMD_RGB_24BPP;
		break;
	}

	pkt_seq_3_5_rgb_lo = 0;
	pkt_seq_3_5_rgb_hi = 0;
	if (dsi->info.video_data_type == TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE)
		pkt_seq = dsi_pkt_seq_cmd_mode;
	else {
		switch (dsi->info.video_burst_mode) {
		case TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED:
		case TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED:
			pkt_seq_3_5_rgb_lo =
					DSI_PKT_SEQ_3_LO_PKT_32_ID(rgb_info);
			if (!dsi->info.no_pkt_seq_eot)
				pkt_seq = dsi_pkt_seq_video_burst;
			else
				pkt_seq = dsi_pkt_seq_video_burst_no_eot;
			break;
		case TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END:
			pkt_seq_3_5_rgb_hi =
					DSI_PKT_SEQ_3_HI_PKT_34_ID(rgb_info);
			pkt_seq = dsi_pkt_seq_video_non_burst_syne;
			break;
		case TEGRA_DSI_VIDEO_NONE_BURST_MODE:
		default:
			pkt_seq_3_5_rgb_lo =
					DSI_PKT_SEQ_3_LO_PKT_32_ID(rgb_info);
			pkt_seq = dsi_pkt_seq_video_non_burst;
			break;
		}
	}

	for (i = 0; i < NUMOF_PKT_SEQ; i++) {
		val = pkt_seq[i];
		reg = dsi_pkt_seq_reg[i];
		if ((reg == DSI_PKT_SEQ_3_LO) || (reg == DSI_PKT_SEQ_5_LO))
			val |= pkt_seq_3_5_rgb_lo;
		if ((reg == DSI_PKT_SEQ_3_HI) || (reg == DSI_PKT_SEQ_5_HI))
			val |= pkt_seq_3_5_rgb_hi;
		tegra_dsi_writel(dsi, val, reg);
	}
}

static void tegra_dsi_reset_underflow_overflow
				(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= (DSI_STATUS_LB_OVERFLOW(0x1) | DSI_STATUS_LB_UNDERFLOW(0x1));
	if (val) {
		if (val & DSI_STATUS_LB_OVERFLOW(0x1))
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: video fifo overflow. Resetting flag\n");
		if (val & DSI_STATUS_LB_UNDERFLOW(0x1))
			dev_warn(&dsi->dc->ndev->dev,
				"dsi: video fifo underflow. Resetting flag\n");
		val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
		val |= DSI_HOST_CONTROL_FIFO_STAT_RESET(0x1);
		tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
		udelay(5);
	}
}

static void tegra_dsi_stop_dc_stream(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_writel(dc, 0, DC_DISP_DISP_WIN_OPTIONS);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ , DC_CMD_STATE_CONTROL);

	dsi->status.dc_stream = DSI_DC_STREAM_DISABLE;
}

static void tegra_dsi_stop_dc_stream_at_frame_end(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	int val;
	long timeout;
	u32 frame_period = DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);

	/* stop dc */
	tegra_dsi_stop_dc_stream(dc, dsi);

	/* enable frame end interrupt */
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	val |= FRAME_END_INT;
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);

	/* wait for frame_end completion.
	 * timeout is 2 frame duration to accomodate for
	 * internal delay.
	 */
	timeout = wait_for_completion_interruptible_timeout(
			&dc->frame_end_complete,
			msecs_to_jiffies(2 * frame_period));

	/* disable frame end interrupt */
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	val &= ~FRAME_END_INT;
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);

	if (timeout == 0)
		dev_warn(&dc->ndev->dev,
			"DC doesn't stop at end of frame.\n");

	tegra_dsi_reset_underflow_overflow(dsi);
}

static void tegra_dsi_start_dc_stream(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	tegra_dc_writel(dc, DSI_ENABLE, DC_DISP_DISP_WIN_OPTIONS);

	/* TODO: clean up */
	tegra_dc_writel(dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE | PW3_ENABLE |
			PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
			DC_CMD_DISPLAY_POWER_CONTROL);

	/* Configure one-shot mode or continuous mode */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		/* disable LSPI/LCD_DE output */
		val = PIN_OUTPUT_LSPI_OUTPUT_DIS;
		tegra_dc_writel(dc, val, DC_COM_PIN_OUTPUT_ENABLE3);

		/* enable MSF & set MSF polarity */
		val = MSF_ENABLE | MSF_LSPI;
		if (!dsi->info.te_polarity_low)
			val |= MSF_POLARITY_HIGH;
		else
			val |= MSF_POLARITY_LOW;
		tegra_dc_writel(dc, val, DC_CMD_DISPLAY_COMMAND_OPTION0);

		/* set non-continuous mode */
		tegra_dc_writel(dc, DISP_CTRL_MODE_NC_DISPLAY,
						DC_CMD_DISPLAY_COMMAND);
		tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ | NC_HOST_TRIG,
						DC_CMD_STATE_CONTROL);
	} else {
		/* set continuous mode */
		tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY,
						DC_CMD_DISPLAY_COMMAND);
		tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	}

	dsi->status.dc_stream = DSI_DC_STREAM_ENABLE;
}

static void tegra_dsi_set_dc_clk(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	u32 shift_clk_div_register;
	u32 val;

	/* Get the corresponding register value of shift_clk_div. */
	shift_clk_div_register = dsi->shift_clk_div * 2 - 2;

#ifndef CONFIG_TEGRA_SILICON_PLATFORM
	shift_clk_div_register = 1;
#endif

	/* TODO: find out if PCD3 option is required */
	val = PIXEL_CLK_DIVIDER_PCD1 |
		SHIFT_CLK_DIVIDER(shift_clk_div_register);
	tegra_dc_writel(dc, val, DC_DISP_DISP_CLOCK_CONTROL);
}

static void tegra_dsi_set_dsi_clk(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi, u32 clk)
{
	u32 rm;

	/* Round up to MHz */
	rm = clk % 1000;
	if (rm != 0)
		clk -= rm;

	/* Set up pixel clock */
	dc->shift_clk_div = dsi->shift_clk_div;
	dc->mode.pclk = (clk * 1000) / dsi->shift_clk_div;
	/* TODO: Define one shot work delay in board file. */
	/* Since for one-shot mode, refresh rate is usually set larger than
	 * expected refresh rate, it needs at least 3 frame period. Less
	 * delay one shot work is, more powering saving we have. */
	dc->one_shot_delay_ms = 4 *
			DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);

	/* Enable DSI clock */
	tegra_dc_setup_clk(dc, dsi->dsi_clk);
	if (!dsi->clk_ref) {
		dsi->clk_ref = true;
		clk_enable(dsi->dsi_clk);
		clk_enable(dsi->dsi_fixed_clk);
		tegra_periph_reset_deassert(dsi->dsi_clk);
	}
	dsi->current_dsi_clk_khz = clk_get_rate(dsi->dsi_clk) / 1000;
	dsi->current_bit_clk_ns =  1000*1000 / (dsi->current_dsi_clk_khz * 2);
}

static void tegra_dsi_hs_clk_out_enable(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val = tegra_dsi_readl(dsi, DSI_CONTROL);
	val &= ~DSI_CONTROL_HS_CLK_CTRL(1);

	if (dsi->info.video_clock_mode == TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS) {
		val |= DSI_CONTROL_HS_CLK_CTRL(CONTINUOUS);
		dsi->status.clk_mode = DSI_PHYCLK_CONTINUOUS;
	} else {
		val |= DSI_CONTROL_HS_CLK_CTRL(TX_ONLY);
		dsi->status.clk_mode = DSI_PHYCLK_TX_ONLY;
	}
	tegra_dsi_writel(dsi, val, DSI_CONTROL);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_HIGH);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	dsi->status.clk_out = DSI_PHYCLK_OUT_EN;
}

static void tegra_dsi_hs_clk_out_enable_in_lp(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	tegra_dsi_hs_clk_out_enable(dsi);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
}

static void tegra_dsi_hs_clk_out_disable(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);

	tegra_dsi_writel(dsi, TEGRA_DSI_DISABLE, DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(1);
	val |= DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

	tegra_dsi_writel(dsi, TEGRA_DSI_ENABLE, DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	dsi->status.clk_mode = DSI_PHYCLK_NOT_INIT;
	dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;
}

static void tegra_dsi_set_control_reg_lp(struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_control;
	u32 host_dsi_control;
	u32 max_threshold;

	dsi_control = dsi->dsi_control_val | DSI_CTRL_HOST_DRIVEN;
	host_dsi_control = HOST_DSI_CTRL_COMMON |
			HOST_DSI_CTRL_HOST_DRIVEN |
			DSI_HOST_DSI_CONTROL_HIGH_SPEED_TRANS(TEGRA_DSI_LOW);
	max_threshold = DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_HOST_FIFO_DEPTH);

	tegra_dsi_writel(dsi, max_threshold, DSI_MAX_THRESHOLD);
	tegra_dsi_writel(dsi, dsi_control, DSI_CONTROL);
	tegra_dsi_writel(dsi, host_dsi_control, DSI_HOST_DSI_CONTROL);

	dsi->status.driven = DSI_DRIVEN_MODE_HOST;
	dsi->status.clk_burst = DSI_CLK_BURST_NOT_INIT;
	dsi->status.vtype = DSI_VIDEO_TYPE_NOT_INIT;
}

static void tegra_dsi_set_control_reg_hs(struct tegra_dc_dsi_data *dsi)
{
	u32 dsi_control;
	u32 host_dsi_control;
	u32 max_threshold;
	u32 dcs_cmd;

	dsi_control = dsi->dsi_control_val;
	host_dsi_control = HOST_DSI_CTRL_COMMON;
	max_threshold = 0;
	dcs_cmd = 0;

	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_HOST) {
		dsi_control |= DSI_CTRL_HOST_DRIVEN;
		host_dsi_control |= HOST_DSI_CTRL_HOST_DRIVEN;
		max_threshold =
			DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_HOST_FIFO_DEPTH);
		dsi->status.driven = DSI_DRIVEN_MODE_HOST;
	} else {
		dsi_control |= DSI_CTRL_DC_DRIVEN;
		host_dsi_control |= HOST_DSI_CTRL_DC_DRIVEN;
		max_threshold =
			DSI_MAX_THRESHOLD_MAX_THRESHOLD(DSI_VIDEO_FIFO_DEPTH);
		dsi->status.driven = DSI_DRIVEN_MODE_DC;
	}

	if (dsi->info.video_data_type == TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE) {
		dsi_control |= DSI_CTRL_CMD_MODE;
		dcs_cmd = DSI_DCS_CMDS_LT5_DCS_CMD(DSI_WRITE_MEMORY_START)|
			DSI_DCS_CMDS_LT3_DCS_CMD(DSI_WRITE_MEMORY_CONTINUE);
		dsi->status.vtype = DSI_VIDEO_TYPE_CMD_MODE;

	} else {
		dsi_control |= DSI_CTRL_VIDEO_MODE;
		dsi->status.vtype = DSI_VIDEO_TYPE_VIDEO_MODE;
	}

	tegra_dsi_writel(dsi, max_threshold, DSI_MAX_THRESHOLD);
	tegra_dsi_writel(dsi, dcs_cmd, DSI_DCS_CMDS);
	tegra_dsi_writel(dsi, dsi_control, DSI_CONTROL);
	tegra_dsi_writel(dsi, host_dsi_control, DSI_HOST_DSI_CONTROL);
}

static void tegra_dsi_pad_calibration(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val =	DSI_PAD_CONTROL_PAD_LPUPADJ(0x1) |
		DSI_PAD_CONTROL_PAD_LPDNADJ(0x1) |
		DSI_PAD_CONTROL_PAD_PREEMP_EN(0x1) |
		DSI_PAD_CONTROL_PAD_SLEWDNADJ(0x6) |
		DSI_PAD_CONTROL_PAD_SLEWUPADJ(0x6);
	if (!dsi->ulpm) {
		val |=	DSI_PAD_CONTROL_PAD_PDIO(0) |
			DSI_PAD_CONTROL_PAD_PDIO_CLK(0) |
			DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_DISABLE);
	} else {
		val |=	DSI_PAD_CONTROL_PAD_PDIO(0x3) |
			DSI_PAD_CONTROL_PAD_PDIO_CLK(0x1) |
			DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_ENABLE);
	}
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);

	val = MIPI_CAL_TERMOSA(0x4);
	tegra_vi_csi_writel(val, CSI_CILA_MIPI_CAL_CONFIG_0);

	val = MIPI_CAL_TERMOSB(0x4);
	tegra_vi_csi_writel(val, CSI_CILB_MIPI_CAL_CONFIG_0);

	val = MIPI_CAL_HSPUOSD(0x3) | MIPI_CAL_HSPDOSD(0x4);
	tegra_vi_csi_writel(val, CSI_DSI_MIPI_CAL_CONFIG);

	val = PAD_DRIV_DN_REF(0x5) | PAD_DRIV_UP_REF(0x7);
	tegra_vi_csi_writel(val, CSI_MIPIBIAS_PAD_CONFIG);

	val = PAD_CIL_PDVREG(0x0);
	tegra_vi_csi_writel(val, CSI_CIL_PAD_CONFIG);
}

static void tegra_dsi_panelB_enable()
{
	unsigned int val;

	val = readl(IO_ADDRESS(APB_MISC_GP_MIPI_PAD_CTRL_0));
	val |= DSIB_MODE_ENABLE;
	writel(val, (IO_ADDRESS(APB_MISC_GP_MIPI_PAD_CTRL_0)));
}

static int tegra_dsi_init_hw(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	u32 i;

	tegra_dsi_writel(dsi,
		DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE),
		DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_lp_clk_khz);
	if (dsi->info.dsi_instance) {
		tegra_dsi_panelB_enable();
	}

	/* TODO: only need to change the timing for bta */
	tegra_dsi_set_phy_timing(dsi, DSI_LPHS_IN_LP_MODE);

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);
 
	/* Initializing DSI registers */
	for (i = 0; i < ARRAY_SIZE(init_reg); i++)
		tegra_dsi_writel(dsi, 0, init_reg[i]);

	tegra_dsi_writel(dsi, dsi->dsi_control_val, DSI_CONTROL);

	tegra_dsi_pad_calibration(dsi);

	tegra_dsi_writel(dsi,
		DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_ENABLE),
		DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	dsi->status.init = DSI_MODULE_INIT;
	dsi->status.lphs = DSI_LPHS_NOT_INIT;
	dsi->status.vtype = DSI_VIDEO_TYPE_NOT_INIT;
	dsi->status.driven = DSI_DRIVEN_MODE_NOT_INIT;
	dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;
	dsi->status.clk_mode = DSI_PHYCLK_NOT_INIT;
	dsi->status.clk_burst = DSI_CLK_BURST_NOT_INIT;
	dsi->status.dc_stream = DSI_DC_STREAM_DISABLE;
	dsi->status.lp_op = DSI_LP_OP_NOT_INIT;

	return 0;
}

static int tegra_dsi_set_to_lp_mode(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi, u8 lp_op)
{
	int err;
//        printk("tegra_dsi_set_to_lp_mode dsi->status.init %d dsi->status.lphs %d lp_op %d\n", dsi->status.init, dsi->status.lphs, lp_op);

	if (dsi->status.init != DSI_MODULE_INIT) {
		err = -EPERM;
		goto fail;
	}

	if (dsi->status.lphs == DSI_LPHS_IN_LP_MODE &&
			dsi->status.lp_op == lp_op)
		goto success;

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);
	/* disable/enable hs clk according to enable_hs_clock_on_lp_cmd_mode */
	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_EN) &&
		(!dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	dsi->target_lp_clk_khz = tegra_dsi_get_lp_clk_rate(dsi, lp_op);
	if (dsi->current_dsi_clk_khz != dsi->target_lp_clk_khz) {
		tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_lp_clk_khz);
		tegra_dsi_set_timeout(dsi);
	}
//        printk("tegra_dsi_set_to_lp_mode target_lp_clk_khz %d, dsi->current_dsi_clk_khz %d\n", dsi->target_lp_clk_khz, dsi->current_dsi_clk_khz);

	tegra_dsi_set_phy_timing(dsi, DSI_LPHS_IN_LP_MODE);

	tegra_dsi_set_control_reg_lp(dsi);

	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_DIS) &&
		(dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_enable_in_lp(dsi);

	dsi->status.lphs = DSI_LPHS_IN_LP_MODE;
	dsi->status.lp_op = lp_op;
success:
	err = 0;
fail:
	return err;
}

static int tegra_dsi_set_to_hs_mode(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi)
{
	int err;

	if (dsi->status.init != DSI_MODULE_INIT) {
		err = -EPERM;
		goto fail;
	}

	if (dsi->status.lphs == DSI_LPHS_IN_HS_MODE)
		goto success;

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);
	if ((dsi->status.clk_out == DSI_PHYCLK_OUT_EN) &&
		(!dsi->info.enable_hs_clock_on_lp_cmd_mode))
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	if (dsi->current_dsi_clk_khz != dsi->target_hs_clk_khz) {
		tegra_dsi_set_dsi_clk(dc, dsi, dsi->target_hs_clk_khz);
		tegra_dsi_set_timeout(dsi);
	}
//        printk("tegra_dsi_set_to_hs_mode target_hs_clk_khz %d, dsi->current_dsi_clk_khz %d\n", dsi->target_hs_clk_khz, dsi->current_dsi_clk_khz);

	tegra_dsi_set_phy_timing(dsi, DSI_LPHS_IN_HS_MODE);

	if (dsi->driven_mode == TEGRA_DSI_DRIVEN_BY_DC) {
		tegra_dsi_set_pkt_seq(dc, dsi);
		tegra_dsi_set_pkt_length(dc, dsi);
		tegra_dsi_set_sol_delay(dc, dsi);
		tegra_dsi_set_dc_clk(dc, dsi);
	}

	tegra_dsi_set_control_reg_hs(dsi);

	if (dsi->status.clk_out == DSI_PHYCLK_OUT_DIS ||
		dsi->info.enable_hs_clock_on_lp_cmd_mode)
		tegra_dsi_hs_clk_out_enable(dsi);

	dsi->status.lphs = DSI_LPHS_IN_HS_MODE;
success:
	dsi->status.lp_op = DSI_LP_OP_NOT_INIT;
	err = 0;
fail:
	return err;
}

static bool tegra_dsi_write_busy(struct tegra_dc_dsi_data *dsi)
{
	u32 timeout = 0;
	bool retVal = true;

	while (timeout <= DSI_MAX_COMMAND_DELAY_USEC) {
		if (!(DSI_TRIGGER_HOST_TRIGGER(0x1) &
			tegra_dsi_readl(dsi, DSI_TRIGGER))) {
			retVal = false;
			break;
		}
		udelay(DSI_COMMAND_DELAY_STEPS_USEC);
		timeout += DSI_COMMAND_DELAY_STEPS_USEC;
	}

	return retVal;
}

static bool tegra_dsi_read_busy(struct tegra_dc_dsi_data *dsi)
{
	u32 timeout = 0;
	bool retVal = true;

	while (timeout <  DSI_STATUS_POLLING_DURATION_USEC) {
		if (!(DSI_HOST_DSI_CONTROL_IMM_BTA(0x1) &
			tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL))) {
			retVal = false;
			break;
		}
		udelay(DSI_STATUS_POLLING_DELAY_USEC);
		timeout += DSI_STATUS_POLLING_DELAY_USEC;
	}

	return retVal;
}

static bool tegra_dsi_host_busy(struct tegra_dc_dsi_data *dsi)
{
	int err = 0;

	if (tegra_dsi_write_busy(dsi)) {
		err = -EBUSY;
		dev_err(&dsi->dc->ndev->dev,
			"DSI trigger bit already set\n");
		goto fail;
	}

	if (tegra_dsi_read_busy(dsi)) {
		err = -EBUSY;
		dev_err(&dsi->dc->ndev->dev,
			"DSI immediate bta bit already set\n");
		goto fail;
	}
fail:
	return (err < 0 ? true : false);
}

static void tegra_dsi_soft_reset(struct tegra_dc_dsi_data *dsi)
{
	u32 trigger;
	u32 status;

	tegra_dsi_writel(dsi,
		DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE),
		DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	tegra_dsi_writel(dsi,
		DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_ENABLE),
		DSI_POWER_CONTROL);
	/* stabilization delay */
	udelay(300);

	/* dsi HW does not clear host trigger bit automatically
	 * on dsi interface disable if host fifo is empty
	 */
	trigger = tegra_dsi_readl(dsi, DSI_TRIGGER);
	status = tegra_dsi_readl(dsi, DSI_STATUS);
	if (trigger & DSI_TRIGGER_HOST_TRIGGER(0x1) &&
		status & DSI_STATUS_IDLE(0x1)) {
		trigger &= ~(DSI_TRIGGER_HOST_TRIGGER(0x1));
		tegra_dsi_writel(dsi, trigger, DSI_TRIGGER);
	}
}

static void tegra_dsi_reset_read_count(struct tegra_dc_dsi_data *dsi)
{
	u32 val;

	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= DSI_STATUS_RD_FIFO_COUNT(0x1f);
	if (val) {
		dev_warn(&dsi->dc->ndev->dev,
			"DSI read count not zero, resetting\n");
		tegra_dsi_soft_reset(dsi);
	}
}

static struct dsi_status *tegra_dsi_save_state_switch_to_host_cmd_mode(
						struct tegra_dc_dsi_data *dsi,
						struct tegra_dc *dc,
						u8 lp_op)
{
	struct dsi_status *init_status;
	int err;

	init_status = kzalloc(sizeof(*init_status), GFP_KERNEL);
	if (!init_status)
		return ERR_PTR(-ENOMEM);

	*init_status = dsi->status;

	if (dsi->status.lphs == DSI_LPHS_IN_HS_MODE) {
		if (dsi->status.driven == DSI_DRIVEN_MODE_DC) {
			if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
				tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);
			dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_HOST;
			if (dsi->info.hs_cmd_mode_supported) {
				err = tegra_dsi_set_to_hs_mode(dc, dsi);
				if (err < 0) {
					dev_err(&dc->ndev->dev,
					"Switch to HS host mode failed\n");
					goto fail;
				}
			}
		}
		if (!dsi->info.hs_cmd_mode_supported) {
			err =
			tegra_dsi_set_to_lp_mode(dc, dsi, lp_op);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"DSI failed to go to LP mode\n");
				goto fail;
			}
		}
	} else if (dsi->status.lphs == DSI_LPHS_IN_LP_MODE) {
		if (dsi->status.lp_op != lp_op) {
			printk("tegra_dsi_set_to_hs_mode in tegra_dsi_save_state_switch_to_host_cmd_mode 2\n");
			err = tegra_dsi_set_to_hs_mode(dc, dsi);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"DSI failed to go to LP mode\n");
				goto fail;
			}
		}
	}

	return init_status;
fail:
	kfree(init_status);
	return ERR_PTR(err);
}

static struct dsi_status *tegra_dsi_prepare_host_transmission(
				struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				u8 lp_op)
{
	int err = 0;
	struct dsi_status *init_status;

	if (dsi->status.init != DSI_MODULE_INIT ||
		dsi->ulpm) {
		err = -EPERM;
		goto fail;
	}

	if (tegra_dsi_host_busy(dsi)) {
		tegra_dsi_soft_reset(dsi);

		/* WAR to stop host write in middle */
		tegra_dsi_writel(dsi, TEGRA_DSI_DISABLE, DSI_TRIGGER);

		if (tegra_dsi_host_busy(dsi)) {
			err = -EBUSY;
			dev_err(&dc->ndev->dev, "DSI host busy\n");
			goto fail;
		}
	}

	if (lp_op == DSI_LP_OP_READ)
		tegra_dsi_reset_read_count(dsi);

	if (dsi->status.lphs == DSI_LPHS_NOT_INIT) {
		printk("tegra_dsi_set_to_hs_mode in tegra_dsi_prepare_host_transmission\n"); 
		err = tegra_dsi_set_to_hs_mode(dc, dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev, "Failed to config LP write\n");
			goto fail;
		}
	}

	init_status = tegra_dsi_save_state_switch_to_host_cmd_mode
					(dsi, dc, lp_op);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "DSI state saving failed\n");
		goto fail;
	}

	return init_status;
fail:
	return ERR_PTR(err);
}

static int tegra_dsi_restore_state(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				struct dsi_status *init_status)
{
	bool switch_back_to_dc_mode = false;
	bool switch_back_to_hs_mode = false;
	bool restart_dc_stream;
	int err = 0;

	switch_back_to_dc_mode = (dsi->status.driven ==
				DSI_DRIVEN_MODE_HOST &&
				init_status->driven ==
				DSI_DRIVEN_MODE_DC);
	switch_back_to_hs_mode = (dsi->status.lphs ==
				DSI_LPHS_IN_LP_MODE &&
				init_status->lphs ==
				DSI_LPHS_IN_HS_MODE);
	restart_dc_stream = (dsi->status.dc_stream ==
				DSI_DC_STREAM_DISABLE &&
				init_status->dc_stream ==
				DSI_DC_STREAM_ENABLE);

	if (dsi->status.lphs == DSI_LPHS_IN_LP_MODE &&
		init_status->lphs == DSI_LPHS_IN_LP_MODE) {
		if (dsi->status.lp_op != init_status->lp_op) {
			err =
			tegra_dsi_set_to_lp_mode(dc, dsi, init_status->lp_op);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
					"Failed to config LP mode\n");
				goto fail;
			}
		}
		goto success;
	}

	if (switch_back_to_dc_mode)
		dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_DC;
	if (switch_back_to_dc_mode || switch_back_to_hs_mode) {
		err = tegra_dsi_set_to_hs_mode(dc, dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev, "Failed to config HS mode\n");
			goto fail;
		}
	}
	if (restart_dc_stream)
		tegra_dsi_start_dc_stream(dc, dsi);

success:
fail:
	kfree(init_status);
	return err;
}

static int tegra_dsi_host_trigger(struct tegra_dc_dsi_data *dsi)
{
	int status = 0;

	if (tegra_dsi_readl(dsi, DSI_TRIGGER)) {
		status = -EBUSY;
		goto fail;
	}

	tegra_dsi_writel(dsi,
		DSI_TRIGGER_HOST_TRIGGER(TEGRA_DSI_ENABLE), DSI_TRIGGER);

#if DSI_USE_SYNC_POINTS
	status = tegra_dsi_syncpt(dsi);
	if (status < 0) {
		dev_err(&dsi->dc->ndev->dev,
			"DSI syncpt for host trigger failed\n");
		goto fail;
	}
#else
	if (tegra_dsi_write_busy(dsi)) {
		status = -EBUSY;
		dev_err(&dsi->dc->ndev->dev,
			"Timeout waiting on write completion\n");
	}
#endif

fail:
	return status;
}

static int _tegra_dsi_write_data(struct tegra_dc_dsi_data *dsi,
					u8 *pdata, u8 data_id, u16 data_len)
{
	u8 virtual_channel;
	u8 *pval;
	u32 val;
	int err;

	err = 0;

	virtual_channel = dsi->info.virtual_channel <<
						DSI_VIR_CHANNEL_BIT_POSITION;

	/* always use hw for ecc */
	val = (virtual_channel | data_id) << 0 |
			data_len << 8;
	tegra_dsi_writel(dsi, val, DSI_WR_DATA);

	/* if pdata != NULL, pkt type is long pkt */
	if (pdata != NULL) {
		while (data_len) {
			if (data_len >= 4) {
				val = ((u32 *) pdata)[0];
				data_len -= 4;
				pdata += 4;
			} else {
				val = 0;
				pval = (u8 *) &val;
				do
					*pval++ = *pdata++;
				while (--data_len);
			}
			tegra_dsi_writel(dsi, val, DSI_WR_DATA);
		}
	}

	err = tegra_dsi_host_trigger(dsi);
	if (err < 0)
		dev_err(&dsi->dc->ndev->dev, "DSI host trigger failed\n");

	return err;
}

int tegra_dsi_write_data(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi,
			u8 *pdata, u8 data_id, u16 data_len)
{
	int err = 0;
	struct dsi_status *init_status;

	tegra_dc_io_start(dc);

	init_status = tegra_dsi_prepare_host_transmission(
				dc, dsi, DSI_LP_OP_WRITE);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "DSI host config failed\n");
		goto fail;
	}

	err = _tegra_dsi_write_data(dsi, pdata, data_id, data_len);
fail:
	err = tegra_dsi_restore_state(dc, dsi, init_status);
	if (err < 0)
		dev_err(&dc->ndev->dev, "Failed to restore prev state\n");
	tegra_dc_io_end(dc);
	return err;
}
EXPORT_SYMBOL(tegra_dsi_write_data);

int tegra_dsi_read_data(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				u32 max_ret_payload_size,
				u32 panel_reg_addr, u8 *read_data);


extern int enterprise_panel_get_lcd_id(void);
int  tegra_dc_dsi_read_lcd_sattus(struct tegra_dc *dc, int watchdogflag);
static int tegra_dsi_send_panel_cmd(struct tegra_dc *dc,
					struct tegra_dc_dsi_data *dsi,
					struct tegra_dsi_cmd *cmd,
					u32 n_cmd)
{
	u32 i;
	int err;
       static int flag = 0;
       u8 buffer[10];
       int lcdid = enterprise_panel_get_lcd_id();
      // if(lcdid != 0)
  /*      {
            if(flag == 0)
            {
                flag = 1;
                printk("[tegra_dsi_send_panel_cmd ] flag == 0\n");
                return 0;
            }
        }*/
    //  if(n_cmd > 5)
        {
    	gpio_set_value(176, 1);
        mdelay(20);
	gpio_set_value(176, 0);
	mdelay(20);
	gpio_set_value(176, 1);
    #ifdef CONFIG_PROJECT_U985
	    mdelay(10);
    #else
        mdelay(120);
    #endif
        printk("tegra_dsi_send_panel_cmd reset success\n");
        }
	err = 0;
	for (i = 0; i < n_cmd; i++) {
		struct tegra_dsi_cmd *cur_cmd;
		cur_cmd = &cmd[i];
		if (cur_cmd->cmd_type == TEGRA_DSI_DELAY_MS)
			mdelay(cur_cmd->sp_len_dly.delay_ms);
		else {
			err = tegra_dsi_write_data(dc, dsi,
						cur_cmd->pdata,
						cur_cmd->data_id,
						cur_cmd->sp_len_dly.data_len);
			if (err < 0)
				break;
		}
	}

	return err;
}


static u8 get_8bit_ecc(u32 header)
{
	char ecc_parity[24] = {
		0x07, 0x0b, 0x0d, 0x0e, 0x13, 0x15, 0x16, 0x19,
		0x1a, 0x1c, 0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c,
		0x31, 0x32, 0x34, 0x38, 0x1f, 0x2f, 0x37, 0x3b
	};
	u8 ecc_byte;
	int i;

	ecc_byte = 0;
	for (i = 0; i < 24; i++)
		ecc_byte ^= ((header >> i) & 1) ? ecc_parity[i] : 0x00;

	return ecc_byte;
}

/* This function is written to send DCS short write (1 parameter) only.
 * This means the cmd will contain only 1 byte of index and 1 byte of value.
 * The data type ID is fixed at 0x15 and the ECC is calculated based on the
 * data in pdata.
 * The command will be sent by hardware every frame.
 * pdata should contain both the index + value for each cmd.
 * data_len will be the total number of bytes in pdata.
 */
int tegra_dsi_send_panel_short_cmd(struct tegra_dc *dc, u8 *pdata, u8 data_len)
{
	u8 ecc8bits = 0, data_len_orig = 0;
	u32 val = 0, pkthdr = 0;
	int err = 0, count = 0;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	data_len_orig = data_len;
	if (pdata != NULL) {
		while (data_len) {
			if (data_len >= 2) {
				pkthdr = (CMD_SHORTW |
					(((u16 *)pdata)[0]) << 8 | 0x00 << 24);
				ecc8bits = get_8bit_ecc(pkthdr);
				val = (pkthdr | (ecc8bits << 24));
				data_len -= 2;
				pdata += 2;
				count++;
			}
			switch (count) {
			case 1:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_0);
				break;
			case 2:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_1);
				break;
			case 3:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_2);
				break;
			case 4:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_3);
				break;
			case 5:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_4);
				break;
			case 6:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_5);
				break;
			case 7:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_6);
				break;
			case 8:
				tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_DATA_7);
				break;
			default:
				err = 1;
				break;
			}
		}
	}

	val = DSI_INIT_SEQ_CONTROL_DSI_FRAME_INIT_BYTE_COUNT(data_len_orig * 2)
		| DSI_INIT_SEQ_CONTROL_DSI_SEND_INIT_SEQUENCE(1);
	tegra_dsi_writel(dsi, val, DSI_INIT_SEQ_CONTROL);

	return err;
}
EXPORT_SYMBOL(tegra_dsi_send_panel_short_cmd);

#if 0//CONFIG_READ_LCD_STATUS
static int tegra_dsi_bta(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 poll_time;
	int err;

	poll_time = 0;
	err = 0;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val |= DSI_HOST_DSI_CONTROL_IMM_BTA(TEGRA_DSI_ENABLE);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

       if (tegra_dsi_read_busy(dsi)) {
		err = -EBUSY;
		dev_err(&dsi->dc->ndev->dev,
			"Timeout wating on read completion\n");
	}

	return err;
}
#else
static int tegra_dsi_bta(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	u32 poll_time;
	int err;

	poll_time = 0;
	err = 0;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val |= DSI_HOST_DSI_CONTROL_IMM_BTA(TEGRA_DSI_ENABLE);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
#if DSI_USE_SYNC_POINTS
	/* FIXME: Workaround for nvhost_syncpt_read */
	dsi->syncpt_val = nvhost_syncpt_update_min(
			&nvhost_get_host(dsi->dc->ndev)->syncpt,
			dsi->syncpt_id);

	val = DSI_INCR_SYNCPT_COND(OP_DONE) |
		DSI_INCR_SYNCPT_INDX(dsi->syncpt_id);
	tegra_dsi_writel(dsi, val, DSI_INCR_SYNCPT);

	/* TODO: Use interrupt rather than polling */
	err = nvhost_syncpt_wait(&nvhost_get_host(dsi->dc->ndev)->syncpt,
		dsi->syncpt_id, dsi->syncpt_val + 1);
	if (err < 0)
		dev_err(&dsi->dc->ndev->dev,
			"DSI sync point failure\n");
	else
		(dsi->syncpt_val)++;
#else
	if (tegra_dsi_read_busy(dsi)) {
		err = -EBUSY;
		dev_err(&dsi->dc->ndev->dev,
			"Timeout wating on read completion\n");
	}
#endif

	return err;
}
#endif
static int tegra_dsi_parse_read_response(struct tegra_dc *dc,
					u32 rd_fifo_cnt, u8 *read_fifo)
{
	int err;
	u32 payload_size;

	payload_size = 0;
	err = 0;

	switch (read_fifo[0]) {
	case DSI_ESCAPE_CMD:
		dev_info(&dc->ndev->dev, "escape cmd[0x%x]\n", read_fifo[0]);
		break;
	case DSI_ACK_NO_ERR:
		dev_info(&dc->ndev->dev,
			"Panel ack, no err[0x%x]\n", read_fifo[0]);
		return err;
	default:
		dev_info(&dc->ndev->dev, "Invalid read response\n");
		break;
	}

	switch (read_fifo[4] & 0xff) {
	case GEN_LONG_RD_RES:
		/* Fall through */
	case DCS_LONG_RD_RES:
		payload_size = (read_fifo[5] |
				(read_fifo[6] << 8)) & 0xFFFF;
		dev_info(&dc->ndev->dev, "Long read response Packet\n"
				"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_1_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_1_BYTE_SHORT_RD_RES:
		payload_size = 1;
		dev_info(&dc->ndev->dev, "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_2_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_2_BYTE_SHORT_RD_RES:
		payload_size = 2;
		dev_info(&dc->ndev->dev, "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case ACK_ERR_RES:
		payload_size = 2;
		dev_info(&dc->ndev->dev, "Acknowledge error report response\n"
			"Packet payload_size[0x%x]\n", payload_size);
		break;
	default:
		dev_info(&dc->ndev->dev, "Invalid response packet\n");
		err = -EINVAL;
		break;
	}
	return err;
}

static int tegra_dsi_read_fifo(struct tegra_dc *dc,
			struct tegra_dc_dsi_data *dsi,
			u8 *read_fifo)
{
	u32 val;
	u32 i;
	u32 poll_time = 0;
	u32 rd_fifo_cnt;
	int err = 0;
	u8 *read_fifo_cp = read_fifo;

	while (poll_time <  DSI_DELAY_FOR_READ_FIFO) {
		mdelay(1);
		val = tegra_dsi_readl(dsi, DSI_STATUS);
		rd_fifo_cnt = val & DSI_STATUS_RD_FIFO_COUNT(0x1f);
		if (rd_fifo_cnt << 2 > DSI_READ_FIFO_DEPTH)
			dev_err(&dc->ndev->dev,
			"DSI RD_FIFO_CNT is greater than RD_FIFO_DEPTH\n");
			break;
		poll_time++;
	}

	if (rd_fifo_cnt == 0) {
		dev_info(&dc->ndev->dev,
			"DSI RD_FIFO_CNT is zero\n");
		err = -EINVAL;
		goto fail;
	}

	if (val & (DSI_STATUS_LB_UNDERFLOW(0x1) |
		DSI_STATUS_LB_OVERFLOW(0x1))) {
		dev_warn(&dc->ndev->dev,
			"DSI overflow/underflow error\n");
	}

	/* Read data from FIFO */
	for (i = 0; i < rd_fifo_cnt; i++) {
		val = tegra_dsi_readl(dsi, DSI_RD_DATA);
		if (enable_read_debug)
			dev_info(&dc->ndev->dev,
			"Read data[%d]: 0x%x\n", i, val);
		memcpy(read_fifo, &val, 4);
		read_fifo += 4;
	}

	/* Make sure all the data is read from the FIFO */
	val = tegra_dsi_readl(dsi, DSI_STATUS);
	val &= DSI_STATUS_RD_FIFO_COUNT(0x1f);
	if (val)
		dev_err(&dc->ndev->dev, "DSI FIFO_RD_CNT not zero"
		" even after reading FIFO_RD_CNT words from read fifo\n");

	if (enable_read_debug) {
		err =
		tegra_dsi_parse_read_response(dc, rd_fifo_cnt, read_fifo_cp);
		if (err < 0)
			dev_warn(&dc->ndev->dev, "Unexpected read data\n");
	}
fail:
	return err;
}

int tegra_dsi_read_data(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi,
				u32 max_ret_payload_size,
				u32 panel_reg_addr, u8 *read_data)
{
	int err = 0;
	struct dsi_status *init_status;

	tegra_dc_io_start(dc);

	init_status = tegra_dsi_prepare_host_transmission(
				dc, dsi, DSI_LP_OP_WRITE);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "DSI host config failed\n");
		goto fail;
	}

	/* Set max return payload size in words */
	err = _tegra_dsi_write_data(dsi, NULL,
		dsi_command_max_return_pkt_size,
		max_ret_payload_size);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
				"DSI write failed\n");
		goto fail;
	}

	/* DCS to read given panel register */
	err = _tegra_dsi_write_data(dsi, NULL,
		dsi_command_dcs_read_with_no_params,
		panel_reg_addr);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
				"DSI write failed\n");
		goto fail;
	}

	tegra_dsi_reset_read_count(dsi);

	if (dsi->status.lp_op == DSI_LP_OP_WRITE) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_READ);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
			"DSI failed to go to LP read mode\n");
			goto fail;
		}
	}

	err = tegra_dsi_bta(dsi);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"DSI IMM BTA timeout\n");
		goto fail;
	}

	err = tegra_dsi_read_fifo(dc, dsi, read_data);
	if (err < 0) {
		dev_err(&dc->ndev->dev, "DSI read fifo failure\n");
		goto fail;
	}
fail:
	err = tegra_dsi_restore_state(dc, dsi, init_status);
	if (err < 0)
		dev_err(&dc->ndev->dev, "Failed to restore prev state\n");
	tegra_dc_io_end(dc);
	return err;
}
EXPORT_SYMBOL(tegra_dsi_read_data);

int tegra_dsi_panel_sanity_check(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	int err = 0;
	u8 read_fifo[DSI_READ_FIFO_DEPTH];
	struct dsi_status *init_status;
	static struct tegra_dsi_cmd dsi_nop_cmd =
			DSI_CMD_SHORT(0x05, 0x0, 0x0);

	tegra_dc_io_start(dc);

	init_status = tegra_dsi_prepare_host_transmission(
					dc, dsi, DSI_LP_OP_WRITE);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "DSI host config failed\n");
		goto fail;
	}

	err = _tegra_dsi_write_data(dsi, NULL, dsi_nop_cmd.data_id, 0x0);
	if (err < 0) {
		dev_err(&dc->ndev->dev, "DSI nop write failed\n");
		goto fail;
	}

	tegra_dsi_reset_read_count(dsi);

	if (dsi->status.lp_op == DSI_LP_OP_WRITE) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_READ);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
			"DSI failed to go to LP read mode\n");
			goto fail;
		}
	}

	err = tegra_dsi_bta(dsi);
	if (err < 0) {
		dev_err(&dc->ndev->dev, "DSI BTA failed\n");
		goto fail;
	}

	err = tegra_dsi_read_fifo(dc, dsi, read_fifo);
	if (err < 0) {
		dev_err(&dc->ndev->dev, "DSI read fifo failure\n");
		goto fail;
	}

	if (read_fifo[0] != DSI_ACK_NO_ERR) {
		dev_warn(&dc->ndev->dev,
			"Ack no error trigger message not received\n");
		err = -EAGAIN;
	}
fail:
	err = tegra_dsi_restore_state(dc, dsi, init_status);
	if (err < 0)
		dev_err(&dc->ndev->dev, "Failed to restore prev state\n");
	tegra_dc_io_end(dc);
	return err;
}
EXPORT_SYMBOL(tegra_dsi_panel_sanity_check);

static int tegra_dsi_enter_ulpm(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int ret;

	ret = 0;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(ENTER_ULPM);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

#if DSI_USE_SYNC_POINTS
	ret = tegra_dsi_syncpt(dsi);
	if (ret < 0) {
		dev_err(&dsi->dc->ndev->dev,
			"DSI syncpt for ulpm enter failed\n");
		goto fail;
	}
#else
	/* TODO: Find exact delay required */
	mdelay(10);
#endif
	dsi->ulpm = true;
fail:
	return ret;
}

static int tegra_dsi_exit_ulpm(struct tegra_dc_dsi_data *dsi)
{
	u32 val;
	int ret;

	ret = 0;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(EXIT_ULPM);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);

#if DSI_USE_SYNC_POINTS
	ret = tegra_dsi_syncpt(dsi);
	if (ret < 0) {
		dev_err(&dsi->dc->ndev->dev,
			"DSI syncpt for ulpm exit failed\n");
		goto fail;
	}
#else
	/* TODO: Find exact delay required */
	mdelay(10);
#endif
	dsi->ulpm = false;

	val = tegra_dsi_readl(dsi, DSI_HOST_DSI_CONTROL);
	val &= ~DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(0x3);
	val |= DSI_HOST_DSI_CONTROL_ULTRA_LOW_POWER(NORMAL);
	tegra_dsi_writel(dsi, val, DSI_HOST_DSI_CONTROL);
fail:
	return ret;

}
static int tegra_dc_dsi_cp_p_cmd(struct tegra_dsi_cmd *src,
					struct tegra_dsi_cmd *dst, u16 n_cmd)
{
	u16 i;
	u16 len;

	memcpy(dst, src, sizeof(*dst) * n_cmd);

	for (i = 0; i < n_cmd; i++)
		if (src[i].pdata) {
			len = sizeof(*src[i].pdata) *
					src[i].sp_len_dly.data_len;
			dst[i].pdata = kzalloc(len, GFP_KERNEL);
			if (!dst[i].pdata)
				goto free_cmd_pdata;
			memcpy(dst[i].pdata, src[i].pdata, len);
		}

	return 0;

free_cmd_pdata:
	for (--i; i >= 0; i--)
		if (dst[i].pdata)
			kfree(dst[i].pdata);
	return -ENOMEM;
}
extern int g_SuspendFlag;
static void tegra_dsi_stop_dc_stream_at_frame_end1(struct tegra_dc *dc,
						struct tegra_dc_dsi_data *dsi)
{
	int val;
	long timeout;
	u32 frame_period = DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);

	/* stop dc */
//	tegra_dsi_stop_dc_stream(dc, dsi);

	/* enable frame end interrupt */
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	val |= FRAME_END_INT;
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);

	/* wait for frame_end completion.
	 * timeout is 2 frame duration to accomodate for
	 * internal delay.
	 */
	timeout = wait_for_completion_interruptible_timeout(
			&dc->frame_end_complete,
			msecs_to_jiffies(1000));

	/* disable frame end interrupt */
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	val &= ~FRAME_END_INT;
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);

	if (timeout == 0)
		dev_warn(&dc->ndev->dev,
			"DC doesn't stop at end of frame.\n");

//	tegra_dsi_reset_underflow_overflow(dsi);
}
extern int tegra_dc_program_mode(struct tegra_dc *dc, struct tegra_dc_mode *mode);
extern int enterprise_panel_set_lcd_type(int panelid);
int  tegra_dc_dsi_read_lcd_sattus(struct tegra_dc *dc, int watchdogflag)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
        int panel_id, i,j, err;
        u8 buffer[20];
        static int count = 0;
  	struct tegra_dsi_cmd *p_cmd;
        struct tegra_dsi_out *dsi_pdata;
       static uint64_t	timestamp = 0;
         struct timex j1, j2, j3, j4, j5, j6,j7;
         int lcdid = enterprise_panel_get_lcd_id();
         if(lcdid == 0 || lcdid == 2){
            return 1;
         }
        #ifdef CONFIG_PROJECT_U950
         static int initflag = 0;
         if(initflag == 0)
         {
             tegra_dc_io_start(dc);
	         mutex_lock(&dsi->lock);
             tegra_dsi_stop_dc_stream(dc, dsi);
             tegra_dsi_stop_dc_stream_at_frame_end1(dc, dsi);
             tegra_dc_program_mode(dc, &dc->mode);
             tegra_dsi_start_dc_stream(dc, dsi);
             mutex_unlock(&dsi->lock);
	         tegra_dc_io_end(dc);
             initflag = 1;
         }
        #endif
                                 

        if(g_SuspendFlag==1)
            return 0;
        count ++;
        if(timestamp == 0)
        {
            timestamp = jiffies;
        }
 //       printk("*********************tegra_dc_dsi_read_lcd_sattus watchdogflag %d count %d jiffies_to_msecs(jiffies - timestamp) %d\n", watchdogflag, count, jiffies_to_msecs(jiffies - timestamp));

        if(watchdogflag == 0)
        {
            tegra_dc_io_start(dc);
	     mutex_lock(&dsi->lock);
            if(jiffies_to_msecs(jiffies - timestamp) >= 2000)
            {
                    if(count <  3)
                    {
                        mutex_unlock(&dsi->lock);
                        tegra_dc_io_end(dc);
                        return 0;
                     }
            else
                {
                    count = 0;
                    timestamp = jiffies;
                }
            }
            else
            {
                mutex_unlock(&dsi->lock);
                tegra_dc_io_end(dc);
                return 0;
            }
          wait_for_completion_interruptible_timeout(
            &dc->frame_end_complete,
            msecs_to_jiffies(1000));    
          }

        if(watchdogflag == 1)
        {
        long timeout;
        int val;
     //      do_gettimeofday(&(j1.time));
                 tegra_dc_io_start(dc);
	 mutex_lock(&dsi->lock); 
    //    do_gettimeofday(&(j2.time));
     //           printk("*********************1 %d  %d  %d %d\n", j2.time.tv_sec , j1.time.tv_sec , j2.time.tv_usec ,j1.time.tv_usec);

	/* enable frame end interrupt */
/*	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	val |= FRAME_END_INT;
	tegra_dc_writel(dc, val, DC_CMD_INT_MASK);
     */    
  //   do_gettimeofday(&(j1.time));

            tegra_dsi_stop_dc_stream_at_frame_end1(dc, dsi);
   //    do_gettimeofday(&(j2.time));
   //             printk("*********************2 %d  %d  %d %d\n", j2.time.tv_sec , j1.time.tv_sec , j2.time.tv_usec ,j1.time.tv_usec);

	 /*         do_gettimeofday(&(j1.time));


	timeout = wait_for_completion_interruptible_timeout(
			&dc->frame_end_complete,
			msecs_to_jiffies(1000));


	if (timeout == 0)
		dev_warn(&dc->ndev->dev,
			"DC doesn't stop at end of frame.\n");
       do_gettimeofday(&(j2.time));
                printk("*********************3 %d  %d  %d %d timeout %d\n", j2.time.tv_sec , j1.time.tv_sec , j2.time.tv_usec ,j1.time.tv_usec, timeout);
*/
        }  

//	 do_gettimeofday(&(j1.time));


       // dsi_pdata = dc->pdata->default_out->dsi;
        if(tegra_dsi_read_data(dc, dsi, 10, 0x0A, buffer) < 0){
             printk("[shangzhi]read data fail!!");
        }
   //    do_gettimeofday(&(j2.time));
   //             printk("*********************4 %d  %d  %d %d\n", j2.time.tv_sec , j1.time.tv_sec , j2.time.tv_usec ,j1.time.tv_usec);
        if(watchdogflag)
        {
       //     tegra_dsi_set_to_hs_mode(dc, dsi);
     //       tegra_dsi_start_dc_stream(dc, dsi);
         }
	//          do_gettimeofday(&(j1.time));

        	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
  //    do_gettimeofday(&(j2.time));
  //      printk("*********************5 %d  %d  %d %d\n", j2.time.tv_sec , j1.time.tv_sec , j2.time.tv_usec ,j1.time.tv_usec);
        if(buffer[5] != 0x9c && buffer[5] != 0x8)
	{
                printk("*********************0x0A:%x\n",buffer[5]);

                return -1;
            }
       else
	 {
            return 1;
            }
}
EXPORT_SYMBOL(tegra_dc_dsi_read_lcd_sattus);
struct tegra_dc_dsi_data *g_dsi = NULL;
struct tegra_dc *g_dc = NULL;
extern void enterprise_dsi_lcd_power_restart();
void tegra_dc_dsi_reset_lcd(struct tegra_dc *dc)
{
	int err = 0;
       struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	struct dsi_status *init_status;

	tegra_dc_io_start(dc);

	init_status = tegra_dsi_prepare_host_transmission(
				dc, dsi, DSI_LP_OP_WRITE);
	if (IS_ERR_OR_NULL(init_status)) {
		err = PTR_ERR(init_status);
		dev_err(&dc->ndev->dev, "tegra_dc_dsi_reset_lcd DSI host config failed\n");
	}
        msleep(10);
        tegra_dsi_send_panel_cmd(dc, dsi,
                                dsi->info.dsi_suspend_cmd,
                                dsi->info.n_suspend_cmd);
          
        tegra_dsi_send_panel_cmd(dc, dsi,
                                dsi->info.dsi_init_cmd,
                                dsi->info.n_init_cmd);
        msleep(10);
        err = tegra_dsi_restore_state(dc, dsi, init_status);
	if (err < 0)
		dev_err(&dc->ndev->dev, "tegra_dc_dsi_reset_lcd Failed to restore prev state\n");
	tegra_dc_io_end(dc);
	return;
}
EXPORT_SYMBOL(tegra_dc_dsi_reset_lcd);
static void tegra_dsi_send_dc_frames(struct tegra_dc *dc,
				     struct tegra_dc_dsi_data *dsi,
				     int no_of_frames)
{
	int err;
	u32 frame_period = DIV_ROUND_UP(S_TO_MS(1), dsi->info.refresh_rate);
	u8 lp_op = dsi->status.lp_op;
	bool switch_to_lp = (dsi->status.lphs == DSI_LPHS_IN_LP_MODE);

	if (dsi->status.lphs != DSI_LPHS_IN_HS_MODE) {
		err = tegra_dsi_set_to_hs_mode(dc, dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"Switch to HS host mode failed\n");
			return;
		}
	}

	/*
	 * Some panels need DC frames be sent under certain
	 * conditions. We are working on the right fix for this
	 * requirement, while using this current fix.
	 */
	tegra_dsi_start_dc_stream(dc, dsi);

	/*
	 * Send frames in Continuous or One-shot mode.
	 */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		while (no_of_frames--) {
			tegra_dc_writel(dc, GENERAL_ACT_REQ | NC_HOST_TRIG,
					DC_CMD_STATE_CONTROL);
			mdelay(frame_period);
		}
	} else
		mdelay(no_of_frames * frame_period);

	tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);

	if (switch_to_lp) {
		err = tegra_dsi_set_to_lp_mode(dc, dsi, lp_op);
		if (err < 0)
			dev_err(&dc->ndev->dev,
				"DSI failed to go to LP mode\n");
	}
}

extern int enterprise_panel_get_lcd_id(void);

static void tegra_dc_dsi_enable(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	int err;
	u32 val;
       static int initflag = 0;
       static int lcdid = 0;
	tegra_dc_io_start(dc);
	mutex_lock(&dsi->lock);

	 //zte lipeng10094834 modify for LCD can not show after first start
//#ifdef CONFIG_PROJECT_U985
/*	if(initflag == 0)
       {
            lcdid = enterprise_panel_get_lcd_id();
            if(lcdid==0x81 || lcdid==0x80 || lcdid==0x0)
            {
                printk("[dsi]: tegra_dc_dsi_enable tegra_dsi_hs_clk_out_enable_in_lp lcdid %d\n",lcdid );
                tegra_dsi_hs_clk_out_enable_in_lp(dsi);
            }
            initflag = 1;
       }*/
//#endif
	/* Stop DC stream before configuring DSI registers
	 * to avoid visible glitches on panel during transition
	 * from bootloader to kernel driver
	 */
	tegra_dsi_stop_dc_stream(dc, dsi);

	if (dsi->enabled) {
		if (dsi->ulpm) {
			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to exit ulpm\n");
				goto fail;
			}
		}

		if (dsi->info.panel_reset) {
            		/*
			 * Certain panels need dc frames be sent before
			 * waking panel.
			 */
			if (dsi->info.panel_send_dc_frames)
				tegra_dsi_send_dc_frames(dc, dsi, 2);

			err = tegra_dsi_send_panel_cmd(dc, dsi,
							dsi->info.dsi_init_cmd,
							dsi->info.n_init_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: error sending dsi init cmd\n");
				goto fail;
			}
		} else if (dsi->info.dsi_late_resume_cmd) {
			err = tegra_dsi_send_panel_cmd(dc, dsi,
						dsi->info.dsi_late_resume_cmd,
						dsi->info.n_late_resume_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: error sending late resume cmd\n");
				goto fail;
			}
		}
	} else {
		err = tegra_dsi_init_hw(dc, dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to init dsi hardware\n");
			goto fail;
		}
		
		if (dsi->ulpm) {
			if (tegra_dsi_enter_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to enter ulpm\n");
				goto fail;
			}

			val = tegra_dsi_readl(dsi, DSI_PAD_CONTROL);

			/* erase bits we're about to set */
			val &= ~(DSI_PAD_CONTROL_PAD_PDIO(0x3) |
				DSI_PAD_CONTROL_PAD_PDIO_CLK(0x1) |
				DSI_PAD_CONTROL_PAD_PULLDN_ENAB(0x1));

			val |= (DSI_PAD_CONTROL_PAD_PDIO(0) |
				DSI_PAD_CONTROL_PAD_PDIO_CLK(0) |
				DSI_PAD_CONTROL_PAD_PULLDN_ENAB
						(TEGRA_DSI_DISABLE));

			tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);
			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to exit ulpm\n");
				goto fail;
			}
		}
		/*
		 * Certain panels need dc frames be sent before
		 * waking panel.
		 */
		if (dsi->info.panel_send_dc_frames)
			tegra_dsi_send_dc_frames(dc, dsi, 2);

		err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to set to lp mode\n");
			goto fail;
		}
 /*          if(initflag == 0)
              {
                    tegra_dc_dsi_set_lcd_type(dc);
                    initflag = 1;
              }
*/
		err = tegra_dsi_send_panel_cmd(dc, dsi, dsi->info.dsi_init_cmd,
						dsi->info.n_init_cmd);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: error while sending dsi init cmd\n");
			goto fail;
		}

		err = tegra_dsi_set_to_hs_mode(dc, dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"dsi: not able to set to hs mode\n");
			goto fail;
		}

		dsi->enabled = true;
	}

	if (dsi->status.driven == DSI_DRIVEN_MODE_DC)
		tegra_dsi_start_dc_stream(dc, dsi);
       g_SuspendFlag = 0;
fail:
	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
}
void tegra_dsi_set_status_and_pclk(struct tegra_dc *dc)
{
        struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
        u32 rm;
        int lcdid = enterprise_panel_get_lcd_id();
        g_dsi = dsi;
         g_dc = dc;

         if(lcdid == 0 || lcdid == 2){
            dsi->enabled = true;
            dsi->status.init = DSI_MODULE_INIT;
            dsi->status.lphs = DSI_LPHS_NOT_INIT;
            dsi->status.vtype = DSI_VIDEO_TYPE_NOT_INIT;
            dsi->status.driven = DSI_DRIVEN_MODE_NOT_INIT;
            dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;
            dsi->status.clk_mode = DSI_PHYCLK_NOT_INIT;
            dsi->status.clk_burst = DSI_CLK_BURST_NOT_INIT;
            dsi->status.dc_stream = DSI_DC_STREAM_DISABLE;
            dsi->status.lp_op = DSI_LP_OP_NOT_INIT;
         }else{
         #if CONFIG_READ_LCD_STATUS
            dsi->enabled = true;
            dsi->status.init = DSI_MODULE_INIT;
            dsi->status.lphs = DSI_LPHS_IN_HS_MODE;
            dsi->status.vtype = DSI_VIDEO_TYPE_VIDEO_MODE;//DSI_VIDEO_TYPE_NOT_INIT;
            dsi->status.driven = DSI_DRIVEN_MODE_DC;//DSI_DRIVEN_MODE_NOT_INIT;
            dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;//DSI_PHYCLK_OUT_EN;
            dsi->status.clk_mode = DSI_PHYCLK_TX_ONLY;//DSI_PHYCLK_NOT_INIT;
            dsi->status.clk_burst = DSI_CLK_BURST_NONE_BURST;//DSI_CLK_BURST_NOT_INIT;
            dsi->status.lp_op = DSI_LP_OP_NOT_INIT;
            dsi->status.dc_stream = DSI_DC_STREAM_ENABLE;
            dsi->driven_mode = DSI_DRIVEN_MODE_DC;
          #else
            dsi->enabled = true;
            dsi->status.init = DSI_MODULE_INIT;
            dsi->status.lphs = DSI_LPHS_NOT_INIT;
            dsi->status.vtype = DSI_VIDEO_TYPE_NOT_INIT;
            dsi->status.driven = DSI_DRIVEN_MODE_NOT_INIT;
            dsi->status.clk_out = DSI_PHYCLK_OUT_DIS;
            dsi->status.clk_mode = DSI_PHYCLK_NOT_INIT;
            dsi->status.clk_burst = DSI_CLK_BURST_NOT_INIT;
            dsi->status.dc_stream = DSI_DC_STREAM_DISABLE;
            dsi->status.lp_op = DSI_LP_OP_NOT_INIT;
          #endif
         }


        rm = dsi->target_hs_clk_khz % 1000;
        if (rm != 0)
        	dsi->target_hs_clk_khz -= rm;

	dc->shift_clk_div = dsi->shift_clk_div;
	dc->mode.pclk = (dsi->target_hs_clk_khz * 1000) / dsi->shift_clk_div;

}

static void _tegra_dc_dsi_init(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	tegra_dc_dsi_debug_create(dsi);
	tegra_dsi_init_sw(dc, dsi);
	/* TODO: Configure the CSI pad configuration */
}



static int tegra_dc_dsi_cp_info(struct tegra_dc_dsi_data *dsi,
					struct tegra_dsi_out *p_dsi)
{
	struct tegra_dsi_cmd *p_init_cmd;
	struct tegra_dsi_cmd *p_early_suspend_cmd;
	struct tegra_dsi_cmd *p_late_resume_cmd;
	struct tegra_dsi_cmd *p_suspend_cmd;
	int err;

	if (p_dsi->n_data_lanes > MAX_DSI_DATA_LANES)
		return -EINVAL;

	p_init_cmd = kzalloc(sizeof(*p_init_cmd) *
				p_dsi->n_init_cmd, GFP_KERNEL);
	if (!p_init_cmd)
		return -ENOMEM;

	if (p_dsi->dsi_early_suspend_cmd) {
		p_early_suspend_cmd = kzalloc(sizeof(*p_early_suspend_cmd) *
					p_dsi->n_early_suspend_cmd,
					GFP_KERNEL);
		if (!p_early_suspend_cmd) {
			err = -ENOMEM;
			goto err_free_init_cmd;
		}
	}

	if (p_dsi->dsi_late_resume_cmd) {
		p_late_resume_cmd = kzalloc(sizeof(*p_late_resume_cmd) *
					p_dsi->n_late_resume_cmd,
					GFP_KERNEL);
		if (!p_late_resume_cmd) {
			err = -ENOMEM;
			goto err_free_p_early_suspend_cmd;
		}
	}

	p_suspend_cmd = kzalloc(sizeof(*p_suspend_cmd) * p_dsi->n_suspend_cmd,
				GFP_KERNEL);
	if (!p_suspend_cmd) {
		err = -ENOMEM;
		goto err_free_p_late_resume_cmd;
	}

	memcpy(&dsi->info, p_dsi, sizeof(dsi->info));

	/* Copy panel init cmd */
	err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_init_cmd,
						p_init_cmd, p_dsi->n_init_cmd);
	if (err < 0)
		goto err_free;
	dsi->info.dsi_init_cmd = p_init_cmd;

	/* Copy panel early suspend cmd */
	if (p_dsi->dsi_early_suspend_cmd) {
		err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_early_suspend_cmd,
					p_early_suspend_cmd,
					p_dsi->n_early_suspend_cmd);
		if (err < 0)
			goto err_free;
		dsi->info.dsi_early_suspend_cmd = p_early_suspend_cmd;
	}

	/* Copy panel late resume cmd */
	if (p_dsi->dsi_late_resume_cmd) {
		err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_late_resume_cmd,
						p_late_resume_cmd,
						p_dsi->n_late_resume_cmd);
		if (err < 0)
			goto err_free;
		dsi->info.dsi_late_resume_cmd = p_late_resume_cmd;
	}

	/* Copy panel suspend cmd */
	err = tegra_dc_dsi_cp_p_cmd(p_dsi->dsi_suspend_cmd, p_suspend_cmd,
					p_dsi->n_suspend_cmd);
	if (err < 0)
		goto err_free;
	dsi->info.dsi_suspend_cmd = p_suspend_cmd;

	if (!dsi->info.panel_reset_timeout_msec)
		dsi->info.panel_reset_timeout_msec =
						DEFAULT_PANEL_RESET_TIMEOUT;

	if (!dsi->info.panel_buffer_size_byte)
		dsi->info.panel_buffer_size_byte = DEFAULT_PANEL_BUFFER_BYTE;

	if (!dsi->info.max_panel_freq_khz) {
		dsi->info.max_panel_freq_khz = DEFAULT_MAX_DSI_PHY_CLK_KHZ;

		if (dsi->info.video_burst_mode >
				TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END){
			dev_err(&dsi->dc->ndev->dev, "DSI: max_panel_freq_khz"
					"is not set for DSI burst mode.\n");
			dsi->info.video_burst_mode =
				TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED;
		}
	}

	if (!dsi->info.lp_cmd_mode_freq_khz)
		dsi->info.lp_cmd_mode_freq_khz = DEFAULT_LP_CMD_MODE_CLK_KHZ;

	if (!dsi->info.chip_id || !dsi->info.chip_rev)
		dev_warn(&dsi->dc->ndev->dev,
			"DSI: Failed to get chip info\n");

	if (!dsi->info.lp_read_cmd_mode_freq_khz)
		dsi->info.lp_read_cmd_mode_freq_khz =
			dsi->info.lp_cmd_mode_freq_khz;

	/* host mode is for testing only */
	dsi->driven_mode = TEGRA_DSI_DRIVEN_BY_DC;
	return 0;

err_free:
	kfree(p_suspend_cmd);
err_free_p_late_resume_cmd:
	kfree(p_late_resume_cmd);
err_free_p_early_suspend_cmd:
	kfree(p_early_suspend_cmd);
err_free_init_cmd:
	kfree(p_init_cmd);
	return err;
}

static int tegra_dc_dsi_init(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;
	struct resource *res;
	struct resource *base_res;
	void __iomem *base;
	struct clk *dc_clk = NULL;
	struct clk *dsi_clk = NULL;
	struct clk *dsi_fixed_clk = NULL;
	struct tegra_dsi_out *dsi_pdata;
	int err;

	err = 0;

	dsi = kzalloc(sizeof(*dsi), GFP_KERNEL);
	if (!dsi)
		return -ENOMEM;

	res = nvhost_get_resource_byname(dc->ndev, IORESOURCE_MEM,
					"dsi_regs");
	if (!res) {
		dev_err(&dc->ndev->dev, "dsi: no mem resource\n");
		err = -ENOENT;
		goto err_free_dsi;
	}

	base_res = request_mem_region(res->start, resource_size(res),
				dc->ndev->name);
	if (!base_res) {
		dev_err(&dc->ndev->dev, "dsi: request_mem_region failed\n");
		err = -EBUSY;
		goto err_free_dsi;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&dc->ndev->dev, "dsi: registers can't be mapped\n");
		err = -EBUSY;
		goto err_release_regs;
	}

	dsi_pdata = dc->pdata->default_out->dsi;
	if (!dsi_pdata) {
		dev_err(&dc->ndev->dev, "dsi: dsi data not available\n");
		goto err_release_regs;
	}

	if (dsi_pdata->dsi_instance)
		dsi_clk = clk_get(&dc->ndev->dev, "dsib");
	else
		dsi_clk = clk_get(&dc->ndev->dev, "dsia");
	dsi_fixed_clk = clk_get(&dc->ndev->dev, "dsi-fixed");

	if (IS_ERR_OR_NULL(dsi_clk) || IS_ERR_OR_NULL(dsi_fixed_clk)) {
		dev_err(&dc->ndev->dev, "dsi: can't get clock\n");
		err = -EBUSY;
		goto err_release_regs;
	}

	dc_clk = clk_get_sys(dev_name(&dc->ndev->dev), NULL);
	if (IS_ERR_OR_NULL(dc_clk)) {
		dev_err(&dc->ndev->dev, "dsi: dc clock %s unavailable\n",
			dev_name(&dc->ndev->dev));
		err = -EBUSY;
		goto err_clk_put;
	}

	mutex_init(&dsi->lock);
	dsi->dc = dc;
	dsi->base = base;
	dsi->base_res = base_res;
	dsi->dc_clk = dc_clk;
	dsi->dsi_clk = dsi_clk;
	dsi->dsi_fixed_clk = dsi_fixed_clk;

	err = tegra_dc_dsi_cp_info(dsi, dsi_pdata);
	if (err < 0)
		goto err_dsi_data;

	tegra_dc_set_outdata(dc, dsi);
	_tegra_dc_dsi_init(dc);

	return 0;

err_dsi_data:
err_clk_put:
	clk_put(dsi_clk);
	clk_put(dsi_fixed_clk);
err_release_regs:
	release_resource(base_res);
err_free_dsi:
	kfree(dsi);

	return err;
}

static void tegra_dc_dsi_destroy(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);
	u16 i;
	u32 val;

	mutex_lock(&dsi->lock);

	/* free up the pdata */
	for (i = 0; i < dsi->info.n_init_cmd; i++) {
		if (dsi->info.dsi_init_cmd[i].pdata)
			kfree(dsi->info.dsi_init_cmd[i].pdata);
	}
	kfree(dsi->info.dsi_init_cmd);

	/* Disable dc stream */
	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);
	/* Disable dsi phy clock */
	if (dsi->status.clk_out == DSI_PHYCLK_OUT_EN)
		tegra_dsi_hs_clk_out_disable(dc, dsi);

	val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE);
	tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	iounmap(dsi->base);
	release_resource(dsi->base_res);

	clk_put(dsi->dc_clk);
	clk_put(dsi->dsi_clk);

	mutex_unlock(&dsi->lock);

	mutex_destroy(&dsi->lock);
	kfree(dsi);
}

static int tegra_dsi_deep_sleep(struct tegra_dc *dc,
				struct tegra_dc_dsi_data *dsi)
{
	int err = 0;
	int val;
	struct clk *parent_clk = NULL;
	struct clk *base_clk = NULL;

	if (!dsi->enabled) {
		err = -EPERM;
		goto fail;
	}

	err = tegra_dsi_set_to_lp_mode(dc, dsi, DSI_LP_OP_WRITE);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
		"DSI failed to go to LP mode\n");
		goto fail;
	}

	/* Suspend panel */
	err = tegra_dsi_send_panel_cmd(dc, dsi,
			dsi->info.dsi_suspend_cmd,
			dsi->info.n_suspend_cmd);
	if (err < 0) {
		dev_err(&dc->ndev->dev,
			"dsi: Error sending suspend cmd\n");
		goto fail;
	}
    
	/*
	 * Certain panels need dc frames be sent after
	 * putting panel to sleep.
	 */
	if (dsi->info.panel_send_dc_frames)
		tegra_dsi_send_dc_frames(dc, dsi, 2);

	if (!dsi->ulpm) {
		err = tegra_dsi_enter_ulpm(dsi);
		if (err < 0) {
			dev_err(&dc->ndev->dev,
				"DSI failed to enter ulpm\n");
			goto fail;
		}
	}

	/*
	 * Suspend pad
	 * It is ok to overwrite previous value of DSI_PAD_CONTROL reg
	 * because it will be restored properly in resume sequence
	 */
	val = DSI_PAD_CONTROL_PAD_PDIO(0x3) |
		DSI_PAD_CONTROL_PAD_PDIO_CLK(0x1) |
		DSI_PAD_CONTROL_PAD_PULLDN_ENAB(TEGRA_DSI_ENABLE);
	tegra_dsi_writel(dsi, val, DSI_PAD_CONTROL);

	/* Suspend core-logic */
	val = DSI_POWER_CONTROL_LEG_DSI_ENABLE(TEGRA_DSI_DISABLE);
	tegra_dsi_writel(dsi, val, DSI_POWER_CONTROL);

	/* Disable dsi fast and slow clock */
	parent_clk = clk_get_parent(dsi->dsi_clk);
	base_clk = clk_get_parent(parent_clk);
	if (dsi->info.dsi_instance)
		tegra_clk_cfg_ex(base_clk,
				TEGRA_CLK_PLLD_CSI_OUT_ENB,
				0);
	else
		tegra_clk_cfg_ex(base_clk,
				TEGRA_CLK_PLLD_DSI_OUT_ENB,
				0);

	/* Disable dsi source clock */
	clk_disable(dsi->dsi_clk);
	clk_disable(dsi->dsi_fixed_clk);

	dsi->clk_ref = false;
	dsi->enabled = false;

	return 0;
fail:
	return err;
}

static void tegra_dc_dsi_disable(struct tegra_dc *dc)
{
	int err;
	struct tegra_dc_dsi_data *dsi = tegra_dc_get_outdata(dc);

	tegra_dc_io_start(dc);
	mutex_lock(&dsi->lock);

	if (dsi->status.dc_stream == DSI_DC_STREAM_ENABLE)
		tegra_dsi_stop_dc_stream_at_frame_end(dc, dsi);
	if (dsi->info.power_saving_suspend) {
		if (tegra_dsi_deep_sleep(dc, dsi) < 0) {
			dev_err(&dc->ndev->dev,
				"DSI failed to enter deep sleep\n");
			goto fail;
		}
	} else {
		if (dsi->info.dsi_early_suspend_cmd) {
			err = tegra_dsi_send_panel_cmd(dc, dsi,
				dsi->info.dsi_early_suspend_cmd,
				dsi->info.n_early_suspend_cmd);
			if (err < 0) {
				dev_err(&dc->ndev->dev,
				"dsi: Error sending early suspend cmd\n");
				goto fail;
			}
		}

		if (!dsi->ulpm) {
			if (tegra_dsi_enter_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to enter ulpm\n");
				goto fail;
			}
		}
	}

fail:
	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
}
void tegra_dsi_reset_lcd()
{
       if(g_dsi==NULL || g_dc==NULL)
        {
            return;
        }
       tegra_dc_dsi_disable(g_dc);
       tegra_dc_dsi_enable(g_dc);
}
EXPORT_SYMBOL(tegra_dsi_reset_lcd);
#ifdef CONFIG_PM
static void tegra_dc_dsi_suspend(struct tegra_dc *dc)
{
	struct tegra_dc_dsi_data *dsi;

	dsi = tegra_dc_get_outdata(dc);

	if (!dsi->enabled)
		return;

	tegra_dc_io_start(dc);
	mutex_lock(&dsi->lock);

	if (!dsi->info.power_saving_suspend) {
		if (dsi->ulpm) {
			if (tegra_dsi_exit_ulpm(dsi) < 0) {
				dev_err(&dc->ndev->dev,
					"DSI failed to exit ulpm");
				goto fail;
			}
		}

		if (tegra_dsi_deep_sleep(dc, dsi) < 0) {
			dev_err(&dc->ndev->dev,
				"DSI failed to enter deep sleep\n");
			goto fail;
		}
	}
fail:
	mutex_unlock(&dsi->lock);
	tegra_dc_io_end(dc);
}

static void tegra_dc_dsi_resume(struct tegra_dc *dc)
{
	/* Not required since tegra_dc_dsi_enable
	 * will reconfigure the controller from scratch
	 */
}
#endif

struct tegra_dc_out_ops tegra_dc_dsi_ops = {
	.init = tegra_dc_dsi_init,
	.destroy = tegra_dc_dsi_destroy,
	.enable = tegra_dc_dsi_enable,
	.disable = tegra_dc_dsi_disable,
#ifdef CONFIG_PM
	.suspend = tegra_dc_dsi_suspend,
	.resume = tegra_dc_dsi_resume,
#endif
};
