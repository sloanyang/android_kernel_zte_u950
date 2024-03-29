/*
 * arch/arm/mach-tegra/include/mach/sleep-t3.S
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/const.h>
#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/cache.h>
#include <asm/domain.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/glue-cache.h>
#include <asm/glue-proc.h>
#include <asm/system.h>

#include <mach/iomap.h>
#include <mach/io.h>

#include "asm_macros.h"
#include "sleep.h"
#include "clock.h"

#define EMC_CFG				0xc
#define EMC_ADR_CFG			0x10
#define EMC_TIMING_CONTROL		0x28
#define EMC_REFRESH			0x70
#define EMC_NOP				0xdc
#define EMC_SELF_REF			0xe0
#define EMC_MRW				0xe8
#define EMC_REQ_CTRL			0x2b0
#define EMC_EMC_STATUS			0x2b4
#define EMC_FBIO_CFG5			0x104
#define EMC_AUTO_CAL_CONFIG		0x2a4
#define EMC_AUTO_CAL_INTERVAL		0x2a8
#define EMC_AUTO_CAL_STATUS		0x2ac
#define EMC_CFG_DIG_DLL			0x2bc
#define EMC_ZCAL_INTERVAL		0x2e0
#define EMC_ZQ_CAL			0x2ec
#define EMC_XM2VTTGENPADCTRL		0x310
#define EMC_XM2VTTGENPADCTRL2		0x314

#define PMC_CTRL			0x0
#define PMC_CTRL_SIDE_EFFECT_LP0	(1 << 14)  /* enter LP0 when CPU pwr gated */

#define PMC_PWRGATE_TOGGLE		0x30
#define PMC_REMOVE_CLAMPING_CMD		0x34
#define PMC_PWRGATE_STATUS		0x38

#define PMC_PWRGATE_PARTID_L2C		(0x5)

#define PMC_IO_DPD_REQ			0x1b8
#define PMC_IO_DPD_STATUS		0x1bc

#define CLK_RESET_CCLK_BURST		0x20
#define CLK_RESET_CCLK_DIVIDER		0x24
#define CLK_RESET_SCLK_BURST		0x28
#define CLK_RESET_SCLK_DIVIDER		0x2c

#define CLK_RESET_PLLC_BASE		0x80
#define CLK_RESET_PLLM_BASE		0x90
#define CLK_RESET_PLLP_BASE		0xa0
#define CLK_RESET_PLLA_BASE		0xb0
#define CLK_RESET_PLLX_BASE		0xe0

#define CLK_RESET_PLLC_MISC		0x8c
#define CLK_RESET_PLLM_MISC		0x9c
#define CLK_RESET_PLLP_MISC		0xac
#define CLK_RESET_PLLA_MISC		0xbc
#define CLK_RESET_PLLX_MISC		0xe4

#define CLK_RESET_PLLP_OUTA		0xa4
#define CLK_RESET_PLLP_OUTB		0xa8

#define PMC_PLLP_WB0_OVERRIDE		0xf8
#define PMC_PLLM_WB0_OVERRIDE		0x1dc

#define CLK_RESET_CLK_SOURCE_MSELECT	0x3b4
#define CLK_RESET_CLK_ENB_H_SET		0x328
#define CLK_RESET_CLK_ENB_H_CLR		0x32c
#define CLK_RESET_CLK_RST_DEV_H_SET		0x308
#define CLK_RESET_CLK_RST_DEV_H_CLR		0x30c

#define I2C_CNFG	0x0
#define I2C_ADDR0	0x4
#define I2C_DATA1	0xc
#define I2C_DATA2	0x10
#define I2C_STATUS	0x1c

#define MSELECT_CLKM			(0x3 << 30)

#if USE_PLL_LOCK_BITS
#define LOCK_DELAY 		PLL_POST_LOCK_DELAY
#else
#define LOCK_DELAY 		0xff /* 255uS delay for PLL stabilization */
#endif

#define USE_PLLP_ON_SLEEP_ENTRY 0

.macro emc_device_mask, rd, base
	ldr	\rd, [\base, #EMC_ADR_CFG]
	tst     \rd, #0x1
	moveq	\rd, #(0x1<<8)		@ just 1 device
	movne	\rd, #(0x3<<8)		@ 2 devices
.endm

.macro emc_timing_update, rd, base
	mov	\rd, #1
	str	\rd, [\base, #EMC_TIMING_CONTROL]
1001:
	ldr	\rd, [\base, #EMC_EMC_STATUS]
	tst	\rd, #(0x1<<23)		@ wait until EMC_STATUS_TIMING_UPDATE_STALLED is clear
	bne	1001b
.endm

#ifdef CONFIG_HOTPLUG_CPU
/*
 * tegra3_hotplug_shutdown(void)
 *
 * Powergates the current CPU.
 * Should never return.
 */
ENTRY(tegra3_hotplug_shutdown)
	mov	r6, lr
	bl	tegra_cpu_exit_coherency

	/* Powergate this CPU. */
	mov	r0, #TEGRA_POWER_HOTPLUG_SHUTDOWN
	bl	tegra3_cpu_reset
	mov	pc, r6				@ should never get here
ENDPROC(tegra3_hotplug_shutdown)
#endif

#if defined(CONFIG_HOTPLUG_CPU) || defined(CONFIG_PM_SLEEP)
/*
 * tegra3_cpu_reset(unsigned long flags)
 *
 * Puts the current CPU in wait-for-event mode on the flow controller
 * and powergates it -- flags (in R0) indicate the request type.
 * Must never be called for CPU 0.
 *
 * corrupts r0-r4, r12
 */
ENTRY(tegra3_cpu_reset)
	cpu_id	r3
	cmp	r3, #0
	moveq	pc, lr		@ Must never be called for CPU 0

	mov32	r12, TEGRA_FLOW_CTRL_VIRT
	cpu_to_csr_reg r1, r3
	add	r1, r1, r12	@ virtual CSR address for this CPU
	cpu_to_halt_reg r2, r3
	add	r2, r2, r12	@ virtual HALT_EVENTS address for this CPU

	/* Clear this CPU's "event" and "interrupt" flags and power gate
	   it when halting but not before it is in the "WFE" state. */
	movw	r12, FLOW_CTRL_CSR_INTR_FLAG | FLOW_CTRL_CSR_EVENT_FLAG | FLOW_CTRL_CSR_ENABLE
	mov	r4, #(1 << 4)
	orr	r12, r12, r4, lsl r3
	str	r12, [r1]

	/* Halt this CPU. */
	mov	r3, #0x400
delay_1:
	subs	r3, r3, #1				@ delay as a part of wfe war.
	bge	delay_1;
	cpsid	a					@ disable imprecise aborts.
	ldr	r3, [r1]				@ read CSR
	str	r3, [r1]				@ clear CSR
	tst	r0, #TEGRA_POWER_HOTPLUG_SHUTDOWN
	moveq	r3, #FLOW_CTRL_WAIT_FOR_INTERRUPT	@ For LP2
	movne	r3, #FLOW_CTRL_WAITEVENT		@ For hotplug
	str	r3, [r2]
	ldr	r0, [r2]
	b	wfe_war

__cpu_reset_again:
	dsb
	.align 5
	wfe					@ CPU should be power gated here
wfe_war:
	b	__cpu_reset_again

	/* 38 nop's, which fills reset of wfe cache line and 4 more cachelines with nop*/
	.rept 38
	nop
	.endr
	b 	.				@ should never get here

ENDPROC(tegra3_cpu_reset)
#endif

#ifdef CONFIG_PM_SLEEP

/*
 * tegra3_sleep_core(unsigned long v2p)
 *
 * enters suspend in LP0 or LP1 by turning off the mmu and jumping to
 * tegra3_tear_down_core in IRAM
 */
ENTRY(tegra3_sleep_core)
	mov	r12, pc 		@ return here is via r12
	b	tegra_cpu_save

	/* preload all the address literals that are needed for the
	 * CPU power-gating process, to avoid loads from SDRAM (which are
	 * not supported once SDRAM is put into self-refresh.
	 * LP0 / LP1 use physical address, since the MMU needs to be
	 * disabled before putting SDRAM into self-refresh to avoid
	 * memory access due to page table walks */
	mov32	r4, TEGRA_PMC_BASE
	mov32	r5, TEGRA_CLK_RESET_BASE
	mov32	r6, TEGRA_FLOW_CTRL_BASE
	mov32	r7, TEGRA_TMRUS_BASE

	mov32	r1, tegra3_tear_down_core
	mov32	r2, tegra3_iram_start
	sub	r1, r1, r2
	mov32	r2, TEGRA_IRAM_CODE_AREA
	add	r1, r1, r2
	b	tegra_turn_off_mmu
ENDPROC(tegra3_sleep_core)

/*
 * tegra3_sleep_cpu_secondary(unsigned long v2p)
 *
 * Enters LP2 on secondary CPU by exiting coherency and powergating the CPU.
 */
ENTRY(tegra3_sleep_cpu_secondary)
	mov	r12, pc 			@ return here is via r12
	b	tegra_cpu_save

	/* Powergate this CPU. */
	mov	r0, #0				@ power mode flags (!hotplug)
	bl	tegra3_cpu_reset
	b	.				@ should never get here
ENDPROC(tegra3_sleep_cpu_secondary)

/*
 * tegra3_tear_down_cpu
 *
 * Switches the CPU cluster to PLL-P and enters sleep.
 */
ENTRY(tegra3_tear_down_cpu)
	mov32	r4, TEGRA_PMC_BASE
	mov32	r5, TEGRA_CLK_RESET_BASE
	mov32	r6, TEGRA_FLOW_CTRL_BASE
	mov32	r7, TEGRA_TMRUS_BASE
#if USE_PLLP_ON_SLEEP_ENTRY
	bl	tegra_cpu_pllp
#endif
	b	tegra3_enter_sleep
ENDPROC(tegra3_tear_down_cpu)

/* START OF ROUTINES COPIED TO IRAM */
	.align L1_CACHE_SHIFT
	.globl tegra3_iram_start
tegra3_iram_start:

/*
 * tegra3_lp1_reset
 *
 * reset vector for LP1 restore; copied into IRAM during suspend.
 * brings the system back up to a safe starting point (SDRAM out of
 * self-refresh, PLLC, PLLM and PLLP reenabled, CPU running on PLLP,
 * system clock running on the same PLL that it suspended at), and
 * jumps to tegra_lp2_startup to restore PLLX and virtual addressing.
 * physical address of tegra_lp2_startup expected to be stored in
 * PMC_SCRATCH41
 *
 * NOTE: THIS *MUST* BE RELOCATED TO TEGRA_IRAM_CODE_AREA AND MUST BE FIRST.
 */
.macro pll_enable, rd, car, base, misc
	ldr	\rd, [\car, #\base]
	tst	\rd, #(1<<30)
	orreq	\rd, \rd, #(1<<30)
	streq	\rd, [\car, #\base]
#if USE_PLL_LOCK_BITS
	ldr	\rd, [\car, #\misc]
	orr	\rd, \rd, #(1<<18)
	str	\rd, [\car, #\misc]
#endif
.endm

.macro pll_locked, rd, car, base
#if USE_PLL_LOCK_BITS
1:
	ldr	\rd, [\car, #\base]
	tst	\rd, #(1<<27)
	beq	1b
#endif
.endm

ENTRY(tegra3_lp1_reset)
	/* the CPU and system bus are running at 32KHz and executing from
	 * IRAM when this code is executed; immediately switch to CLKM and
	 * enable PLLP, PLLM, PLLC, PLLA and PLLX. */
	mov32	r0, TEGRA_CLK_RESET_BASE
#ifndef CONFIG_TRUSTED_FOUNDATIONS
	/* secure code handles 32KHz to CLKM/OSC clock switch */
	mov	r1, #(1<<28)
	str	r1, [r0, #CLK_RESET_SCLK_BURST]
	str	r1, [r0, #CLK_RESET_CCLK_BURST]
	mov	r1, #0
	str	r1, [r0, #CLK_RESET_SCLK_DIVIDER]
	str	r1, [r0, #CLK_RESET_CCLK_DIVIDER]
#endif
	/* enable PLLM via PMC */
	mov32	r2, TEGRA_PMC_BASE
	ldr	r1, [r2, #PMC_PLLP_WB0_OVERRIDE]
	orr	r1, r1, #(1<<12)
	str	r1, [r2, #PMC_PLLP_WB0_OVERRIDE]

	pll_enable r1, r0, CLK_RESET_PLLM_BASE, CLK_RESET_PLLM_MISC
#if 0
	pll_enable r1, r0, CLK_RESET_PLLP_BASE, CLK_RESET_PLLP_MISC
	pll_enable r1, r0, CLK_RESET_PLLA_BASE, CLK_RESET_PLLA_MISC
#endif
	pll_enable r1, r0, CLK_RESET_PLLC_BASE, CLK_RESET_PLLC_MISC
	pll_enable r1, r0, CLK_RESET_PLLX_BASE, CLK_RESET_PLLX_MISC

	pll_locked r1, r0, CLK_RESET_PLLM_BASE
	pll_locked r1, r0, CLK_RESET_PLLP_BASE
	pll_locked r1, r0, CLK_RESET_PLLA_BASE
	pll_locked r1, r0, CLK_RESET_PLLC_BASE
	pll_locked r1, r0, CLK_RESET_PLLX_BASE

	mov32	r7, TEGRA_TMRUS_BASE
	ldr	r1, [r7]
	add	r1, r1, #LOCK_DELAY
	wait_until r1, r7, r3

	add	r5, pc, #tegra3_sdram_pad_save-(.+8)	@ r5 reserved for pad base

	ldr	r4, [r5, #0x18]
	str	r4, [r0, #CLK_RESET_CLK_SOURCE_MSELECT]

	ldr	r4, [r5, #0x1C]
	str	r4, [r0, #CLK_RESET_SCLK_BURST]

	mov32	r4, ((1<<28) | (8))	@ burst policy is PLLX
	str	r4, [r0, #CLK_RESET_CCLK_BURST]

#ifdef CONFIG_TEGRA_LP1_950
	/* Restore the Core voltage to high on LP1 resume */
	/* Reset(Enable/Disable) the DVC-I2C Controller*/
lp1_voltset:
	mov r1, #(1 << 15)
	str r1, [r0, #CLK_RESET_CLK_RST_DEV_H_SET]

	/* Wait for 2us */
	mov32	r7, TEGRA_TMRUS_BASE
	wait_for_us r1, r7, r9
	add r1, r1, #2
	wait_until r1, r7, r9

	mov r1, #(1 << 15)
	str r1, [r0, #CLK_RESET_CLK_RST_DEV_H_CLR]

	/* Enable the DVC-I2C Controller */
	mov r1, #(1 << 15)
	str r1, [r0, #CLK_RESET_CLK_ENB_H_SET]


	/* Same I2C transaction protocol as suspend */
	ldr r1, lp1_register_pmuslave_addr
	cmp r1, #0
	beq lp1_voltskip_resume

	ldr r4, lp1_register_i2c_base_addr
	str r1, [r4, #I2C_ADDR0]

	mov32 r1, 0x2
	str r1, [r4, #I2C_CNFG]

	ldr r1, lp1_register_core_highvolt
	str r1, [r4, #I2C_DATA1]

	mov32 r1, 0
	str r1, [r4, #I2C_DATA2]

	mov32 r1, 0xA02
	str r1, [r4, #I2C_CNFG]

	wait_for_us r1, r7, r9
	mov32 r3, 0x7D0   /* Wait for 2ms and try transaction again */
	add r0, r1, r3
loop_i2c_status_resume:
	add r1, r1, #0xFA /* Check status every 250us */
	wait_until r1, r7, r9
	cmp r0, r1
	beq lp1_voltset

	ldr r3, [r4, #I2C_STATUS]
	cmp r3, #0
	bne loop_i2c_status_resume
lp1_voltskip_resume:
#endif

#if defined (CONFIG_CACHE_L2X0)
	/* power up L2 */
	ldr	r0, [r2, #PMC_PWRGATE_STATUS]
	tst	r0, #(1<<PMC_PWRGATE_PARTID_L2C)
	bne	powerup_l2_done
	movw	r0, #(1<<8) | PMC_PWRGATE_PARTID_L2C
	str	r0, [r2, #PMC_PWRGATE_TOGGLE]
powerup_l2_wait:
	ldr	r0, [r2, #PMC_PWRGATE_STATUS]
	tst	r0, #(1<<PMC_PWRGATE_PARTID_L2C)
	beq	powerup_l2_wait
powerup_l2_done:
	mov	r0, #PMC_PWRGATE_PARTID_L2C
	str	r0, [r2, #PMC_REMOVE_CLAMPING_CMD]
#endif

	mov32	r0, TEGRA_EMC_BASE			@ r0 reserved for emc base

	ldr	r1, [r5, #0x14]	@ PMC_IO_DPD_STATUS
	mvn	r1, r1
	bic	r1, r1, #(0x1<<31)
	orr	r1, r1, #(0x1<<30)
	str	r1, [r2, #PMC_IO_DPD_REQ]
	ldr	r1, [r5, #0xC]
	str	r1, [r0, #EMC_XM2VTTGENPADCTRL]
	ldr	r1, [r5, #0x10]
	str	r1, [r0, #EMC_XM2VTTGENPADCTRL2]
	ldr	r1, [r5, #0x8]
	str	r1, [r0, #EMC_AUTO_CAL_INTERVAL]

	ldr	r1, [r0, #EMC_CFG_DIG_DLL]
	orr	r1, r1, #(0x1<<30)		@ set DLL_RESET
	str	r1, [r0, #EMC_CFG_DIG_DLL]

	emc_timing_update r1, r0

	ldr	r1, [r0, #EMC_AUTO_CAL_CONFIG]
	orr	r1, r1, #(0x1<<31)		@ set AUTO_CAL_ACTIVE
	str	r1, [r0, #EMC_AUTO_CAL_CONFIG]

emc_wait_audo_cal_onetime:
	ldr	r1, [r0, #EMC_AUTO_CAL_STATUS]
	tst	r1, #(0x1<<31)		@ wait until AUTO_CAL_ACTIVE is clear
	bne	emc_wait_audo_cal_onetime

	ldr	r1, [r0, #EMC_CFG]
	bic	r1, r1, #(1<<31)	@ disable DRAM_CLK_STOP
	str	r1, [r0, #EMC_CFG]

	mov	r1, #0
	str	r1, [r0, #EMC_SELF_REF]	@ take DRAM out of self refresh
	mov	r1, #1
	str	r1, [r0, #EMC_NOP]
	str	r1, [r0, #EMC_NOP]
	str	r1, [r0, #EMC_REFRESH]

	emc_device_mask r1, r0

exit_selfrefresh_loop:
	ldr	r2, [r0, #EMC_EMC_STATUS]
	ands	r2, r2, r1
	bne	exit_selfrefresh_loop

	lsr	r1, r1, #8		@ devSel, bit0:dev0 bit1:dev1

	mov32	r7, TEGRA_TMRUS_BASE
	ldr	r2, [r0, #EMC_FBIO_CFG5]

	and	r2, r2, #3
	cmp	r2, #2
	beq	emc_lpddr2

	mov32	r2, 0x80000011
	str	r2, [r0, #EMC_ZQ_CAL]
	ldr	r2, [r7]
	add	r2, r2, #10
	wait_until r2, r7, r3

	tst	r1, #2
	beq zcal_done

	mov32	r2, 0x40000011
	str	r2, [r0, #EMC_ZQ_CAL]
	ldr	r2, [r7]
	add	r2, r2, #10
	wait_until r2, r7, r3
	b zcal_done

emc_lpddr2:

	mov32	r2, 0x800A00AB
	str	r2, [r0, #EMC_MRW]
	ldr	r2, [r7]
	add	r2, r2, #1
	wait_until r2, r7, r3

	tst	r1, #2
	beq zcal_done

	mov32	r2, 0x400A00AB
	str	r2, [r0, #EMC_MRW]
	ldr	r2, [r7]
	add	r2, r2, #1
	wait_until r2, r7, r3

zcal_done:

	mov	r1, #0
	str	r1, [r0, #EMC_REQ_CTRL]
	ldr	r1, [r5, #0x4]
	str	r1, [r0, #EMC_ZCAL_INTERVAL]
	ldr	r1, [r5, #0x0]
	str	r1, [r0, #EMC_CFG]

	mov32	r0, TEGRA_PMC_BASE
	ldr	r0, [r0, #PMC_SCRATCH41]
	mov	pc, r0
ENDPROC(tegra3_lp1_reset)

	.align	L1_CACHE_SHIFT
	.type	tegra3_sdram_pad_save, %object
tegra3_sdram_pad_save:
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0

tegra3_sdram_pad_address:
	.word	TEGRA_EMC_BASE + EMC_CFG				@0x0
	.word	TEGRA_EMC_BASE + EMC_ZCAL_INTERVAL			@0x4
	.word	TEGRA_EMC_BASE + EMC_AUTO_CAL_INTERVAL			@0x8
	.word	TEGRA_EMC_BASE + EMC_XM2VTTGENPADCTRL			@0xc
	.word	TEGRA_EMC_BASE + EMC_XM2VTTGENPADCTRL2			@0x10
	.word	TEGRA_PMC_BASE + PMC_IO_DPD_STATUS			@0x14
	.word	TEGRA_CLK_RESET_BASE + CLK_RESET_CLK_SOURCE_MSELECT	@0x18
	.word	TEGRA_CLK_RESET_BASE + CLK_RESET_SCLK_BURST		@0x1c

tegra3_sdram_pad_size:
	.word	tegra3_sdram_pad_address - tegra3_sdram_pad_save

#ifdef CONFIG_TEGRA_LP1_950
	.globl lp1_register_pmuslave_addr
	.globl lp1_register_i2c_base_addr
	.globl lp1_register_core_lowvolt
	.globl lp1_register_core_highvolt
lp1_register_pmuslave_addr:
	.word	0
lp1_register_i2c_base_addr:
	.word	0
lp1_register_core_lowvolt:
	.word	0
lp1_register_core_highvolt:
	.word	0
#endif

/*
 * tegra3_tear_down_core
 *
 * copied into and executed from IRAM
 * puts memory in self-refresh for LP0 and LP1
 */
tegra3_tear_down_core:
	bl	tegra3_sdram_self_refresh
	bl	tegra3_cpu_clk32k
	b	tegra3_enter_sleep

/*
 * tegra3_cpu_clk32k
 *
 * In LP0 and LP1 all plls will be turned off.  Switch the CPU and system clock
 * to the 32khz clock (clks)
 * r4 = TEGRA_PMC_BASE
 * r5 = TEGRA_CLK_RESET_BASE
 * r6 = TEGRA_FLOW_CTRL_BASE
 * r7 = TEGRA_TMRUS_BASE
 */
tegra3_cpu_clk32k:
	ldr	r0, [r4, #PMC_CTRL]
	tst	r0, #PMC_CTRL_SIDE_EFFECT_LP0
	beq	lp1_clocks_prepare

	/* enable PLLM auto-restart via PMC in LP0; restore override settings */
	ldr	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
	orr	r0, r0, #((1 << 12) | (1 << 11))
	str	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
	ldr	r0, [r4, #PMC_SCRATCH2]
	str	r0, [r4, #PMC_PLLM_WB0_OVERRIDE]
	mov	pc, lr

lp1_clocks_prepare:
	/* Prepare to set the Core to the lowest voltage if supported.
	 * Start by setting the I2C clocks to make the I2C transfer */
#ifdef CONFIG_TEGRA_LP1_950
	/* Set up the PWR I2C GPIOs with the right masks*/

	/* Reset(Set/Clr) the DVC-I2C Controller*/
	mov r0, #(1 << 15)
	str r0, [r5, #CLK_RESET_CLK_RST_DEV_H_SET]

	/* Wait for 2us */
	wait_for_us r1, r7, r9
	mov32 r0, 0x7D0
	add r1, r1, r0
	wait_until r1, r7, r9

	mov r0, #(1 << 15)
	str r0, [r5, #CLK_RESET_CLK_RST_DEV_H_CLR]

	/* Enable the DVC-I2C Controller */
	mov r0, #(1 << 15)
	str r0, [r5, #CLK_RESET_CLK_ENB_H_SET]

	/* I2C transfer protocol:
	 * 4 packets: Slaveaddr + WriteConfigure + Data1 + Data2 */
	ldr r0, lp1_register_pmuslave_addr
	cmp r0, #0
	beq lp1_volt_skip
	ldr r1, lp1_register_i2c_base_addr
	str r0, [r1, #I2C_ADDR0]

	mov32 r0, 0x2
	str r0, [r1, #I2C_CNFG]

	ldr r0, lp1_register_core_lowvolt
	str r0, [r1, #I2C_DATA1]

	mov32 r0, 0
	str r0, [r1, #I2C_DATA2]

	/* Send I2C transaction */
	mov32 r0, 0xA02
	str r0, [r1, #I2C_CNFG]

	/* Check the transaction status before proceeding */
	wait_for_us r2, r7, r9
	mov32 r3, 0x7D0 /* Wait for 2ms for I2C transaction */
	add r3, r2, r3
loop_i2c_status_suspend:
	add r2, r2, #0xFA /* Check status every 250us */
	cmp r3, r2
	beq lp1_volt_skip  /* Waited for 2ms, I2C transaction didn't take place */
	wait_until r2, r7, r9

	ldr r0, [r1, #I2C_STATUS]
	cmp r0, #0
	bne loop_i2c_status_suspend
lp1_volt_skip:

	/* Disable the DVC-I2C Controller */
	mov r0, #(1 << 15)
	str r0, [r5, #CLK_RESET_CLK_ENB_H_CLR]

#endif
	/* start by jumping to clkm to safely disable PLLs, then jump
	 * to clks */
	mov	r0, #(1 << 28)
	str	r0, [r5, #CLK_RESET_SCLK_BURST]
	str	r0, [r5, #CLK_RESET_CCLK_BURST]
	mov	r0, #0
	str	r0, [r5, #CLK_RESET_CCLK_DIVIDER]
	str	r0, [r5, #CLK_RESET_SCLK_DIVIDER]

	/* switch the clock source for mselect to be CLK_M */
	ldr	r0, [r5, #CLK_RESET_CLK_SOURCE_MSELECT]
	orr	r0, r0, #MSELECT_CLKM
	str	r0, [r5, #CLK_RESET_CLK_SOURCE_MSELECT]

	/* 2 us delay between changing sclk and disabling PLLs */
	wait_for_us r1, r7, r9
	add	r1, r1, #2
	wait_until r1, r7, r9

	/* switch to CLKS */
	mov	r0, #0	/* burst policy = 32KHz */
	str	r0, [r5, #CLK_RESET_SCLK_BURST]

	/* disable PLLM via PMC in LP1 */
	ldr	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
	bic	r0, r0, #(1 << 12)
	str	r0, [r4, #PMC_PLLP_WB0_OVERRIDE]
	b	powerdown_pll_pcx

powerdown_pll_pcx:
	ldr	r11, [r4, #PMC_SCRATCH37]	@ load the LP1 flags
	tst	r11, #TEGRA_POWER_LP1_AUDIO	@ check if voice call is going on
	bne	powerdown_pll_cx		@ if yes, do not turn off pll-p/pll-a

#if 0
	ldr	r0, [r5, #CLK_RESET_PLLP_BASE]
	bic	r0, r0, #(1<<30)
	str	r0, [r5, #CLK_RESET_PLLP_BASE]
	ldr	r0, [r5, #CLK_RESET_PLLA_BASE]
	bic	r0, r0, #(1<<30)
	str	r0, [r5, #CLK_RESET_PLLA_BASE]
#endif

powerdown_pll_cx:
	ldr	r0, [r5, #CLK_RESET_PLLC_BASE]
	bic	r0, r0, #(1<<30)
	str	r0, [r5, #CLK_RESET_PLLC_BASE]
	ldr	r0, [r5, #CLK_RESET_PLLX_BASE]
	bic	r0, r0, #(1<<30)
	str	r0, [r5, #CLK_RESET_PLLX_BASE]

	mov	pc, lr

/*
 * tegra3_enter_sleep
 *
 * uses flow controller to enter sleep state
 * executes from IRAM with SDRAM in selfrefresh when target state is LP0 or LP1
 * executes from SDRAM with target state is LP2
 * r4 = TEGRA_PMC_BASE
 * r5 = TEGRA_CLK_RESET_BASE
 * r6 = TEGRA_FLOW_CTRL_BASE
 * r7 = TEGRA_TMRUS_BASE
 */
tegra3_enter_sleep:
	ldr	r1, [r7]
	str	r1, [r4, #PMC_SCRATCH38]
	dsb
	cpu_id	r1

	cpu_to_csr_reg	r2, r1
	ldr	r0, [r6, r2]
	orr	r0, r0, #FLOW_CTRL_CSR_INTR_FLAG | FLOW_CTRL_CSR_EVENT_FLAG
	orr	r0, r0, #FLOW_CTRL_CSR_ENABLE
	str	r0, [r6, r2]

	mov	r0, #FLOW_CTRL_WAIT_FOR_INTERRUPT
	orr	r0, r0, #FLOW_CTRL_HALT_CPU_IRQ | FLOW_CTRL_HALT_CPU_FIQ
	cpu_to_halt_reg r2, r1
	str	r0, [r6, r2]
	dsb
	ldr	r0, [r6, r2] /* memory barrier */

halted:
	isb
	dsb
	wfi	/* CPU should be power gated here */

	/* !!!FIXME!!! Implement halt failure handler */
	b	halted

/*
 * tegra3_sdram_self_refresh
 *
 * called with MMU off and caches disabled
 /* puts sdram in self refresh
 * must execute from IRAM
 * r4 = TEGRA_PMC_BASE
 * r5 = TEGRA_CLK_RESET_BASE
 * r6 = TEGRA_FLOW_CTRL_BASE
 * r7 = TEGRA_TMRUS_BASE
 */

tegra3_sdram_self_refresh:

	adr	r2, tegra3_sdram_pad_address
	adr	r8, tegra3_sdram_pad_save
	mov	r9, #0

padsave:
	ldr	r0, [r2, r9]			@ r0 is emc register address

	ldr	r1, [r0]
	str	r1, [r8, r9]			@ save emc register

	add	r9, r9, #4
	ldr	r0, tegra3_sdram_pad_size
	cmp	r0, r9
	bne	padsave
padsave_done:

	dsb

	mov32	r0, TEGRA_EMC_BASE			@ r0 reserved for emc base

	mov	r1, #0
	str	r1, [r0, #EMC_ZCAL_INTERVAL]
	str	r1, [r0, #EMC_AUTO_CAL_INTERVAL]
	ldr	r1, [r0, #EMC_CFG]
	bic	r1, r1, #(1<<28)
	str	r1, [r0, #EMC_CFG]		@ disable DYN_SELF_REF

	emc_timing_update r1, r0

	ldr	r1, [r7]
	add	r1, r1, #5
	wait_until r1, r7, r2

emc_wait_audo_cal:
	ldr	r1, [r0, #EMC_AUTO_CAL_STATUS]
	tst	r1, #(0x1<<31)		@ wait until AUTO_CAL_ACTIVE is clear
	bne	emc_wait_audo_cal

	mov	r1, #3
	str	r1, [r0, #EMC_REQ_CTRL]		@ stall incoming DRAM requests

emcidle:
	ldr	r1, [r0, #EMC_EMC_STATUS]
	tst	r1, #4
	beq	emcidle

	mov	r1, #1
	str	r1, [r0, #EMC_SELF_REF]

	emc_device_mask r1, r0

emcself:
	ldr	r2, [r0, #EMC_EMC_STATUS]
	and	r2, r2, r1
	cmp	r2, r1
	bne	emcself				@ loop until DDR in self-refresh

	ldr	r1, [r0, #EMC_XM2VTTGENPADCTRL]
	mov32	r2, 0xF8F8FFFF		@ clear XM2VTTGEN_DRVUP and XM2VTTGEN_DRVDN
	and	r1, r1, r2
	str	r1, [r0, #EMC_XM2VTTGENPADCTRL]
	ldr	r1, [r0, #EMC_XM2VTTGENPADCTRL2]
	orr	r1, r1, #7			@ set E_NO_VTTGEN
	str	r1, [r0, #EMC_XM2VTTGENPADCTRL2]

	emc_timing_update r1, r0

	ldr	r1, [r4, #PMC_CTRL]
	tst	r1, #PMC_CTRL_SIDE_EFFECT_LP0
	bne	pmc_io_dpd_skip
	mov32	r1, 0x8EC00000
	str	r1, [r4, #PMC_IO_DPD_REQ]
pmc_io_dpd_skip:

	dsb

	mov	pc, lr

	.ltorg
/* dummy symbol for end of IRAM */
	.align L1_CACHE_SHIFT
	.globl tegra3_iram_end
tegra3_iram_end:
	b	.
#endif
