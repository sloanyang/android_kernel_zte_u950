/*
 * arch/arm/mach-tegra/include/mach/legacy_irq.h
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
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

#ifndef _ARCH_ARM_MACH_TEGRA_LEGARY_IRQ_H
#define _ARCH_ARM_MACH_TEGRA_LEGARY_IRQ_H

void tegra_init_legacy_irq_cop(void);
void tegra_legacy_irq_set_lp1_wake_mask(void);
void tegra_legacy_irq_restore_mask(void);

#endif
