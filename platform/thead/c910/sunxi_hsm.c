/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Samuel Holland <samuel@sholland.org>
 */

#include <thead_c9xx.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_pmu.h>
#include <sbi/sbi_scratch.h>
#include <thead_c9xx_plic.h>

#define SUN20I_D1_CCU_BASE		((void *)0x02001000)
#define SUN20I_D1_RISCV_CFG_BASE	((void *)0x06010000)
#define SUN20I_D1_PPU_BASE		((void *)0x07001000)
#define SUN20I_D1_PRCM_BASE		((void *)0x07010000)

/*
 * CCU
 */

#define CCU_BGR_ENABLE			(BIT(16) | BIT(0))

#define RISCV_CFG_BGR_REG		0xd0c
#define PPU_BGR_REG			0x1ac

/*
 * CSRs
 */

#define CSR_MXSTATUS			0x7c0
#define CSR_MHCR			0x7c1
#define CSR_MCOR			0x7c2
#define CSR_MHINT			0x7c5

static unsigned long csr_mxstatus;
static unsigned long csr_mhcr;
static unsigned long csr_mhint;

static void sunxi_csr_save(void)
{
	/* Save custom CSRs. */
	csr_mxstatus	= csr_read(CSR_MXSTATUS);
	csr_mhcr	= csr_read(CSR_MHCR);
	csr_mhint	= csr_read(CSR_MHINT);

	/* Flush and disable caches. */
	csr_write(CSR_MCOR, 0x22);
	csr_write(CSR_MHCR, 0x0);
}

static void sunxi_csr_restore(void)
{
	/* Invalidate caches and the branch predictor. */
	csr_write(CSR_MCOR, 0x70013);

	/* Restore custom CSRs, including the cache state. */
	csr_write(CSR_MXSTATUS,	csr_mxstatus);
	csr_write(CSR_MHCR,	csr_mhcr);
	csr_write(CSR_MHINT,	csr_mhint);
}

/*
 * PLIC
 */

#define PLIC_SOURCES			176
#define PLIC_IE_WORDS			((PLIC_SOURCES + 31) / 32)

static u8 plic_priority[PLIC_SOURCES];
static u32 plic_sie[PLIC_IE_WORDS];
static u32 plic_threshold;

static void sunxi_plic_save(void)
{
	thead_c9xx_plic_context_save(true, plic_sie, &plic_threshold, PLIC_IE_WORDS);
	thead_c9xx_plic_priority_save(plic_priority, PLIC_SOURCES);
}

static void sunxi_plic_restore(void)
{
	thead_c9xx_plic_restore();
	thead_c9xx_plic_priority_restore(plic_priority, PLIC_SOURCES);
	thead_c9xx_plic_context_restore(true, plic_sie, plic_threshold,
				 PLIC_IE_WORDS);
}

/*
 * PPU
 */

#define PPU_PD_ACTIVE_CTRL		0x2c

static void sunxi_ppu_save(void)
{
	/* Enable MMIO access. Do not assume S-mode leaves the clock enabled. */
	writel_relaxed(CCU_BGR_ENABLE, SUN20I_D1_PRCM_BASE + PPU_BGR_REG);

	/* Activate automatic power-down during the next WFI. */
	writel_relaxed(1, SUN20I_D1_PPU_BASE + PPU_PD_ACTIVE_CTRL);
}

static void sunxi_ppu_restore(void)
{
	/* Disable automatic power-down. */
	writel_relaxed(0, SUN20I_D1_PPU_BASE + PPU_PD_ACTIVE_CTRL);
}

/*
 * RISCV_CFG
 */

#define RESET_ENTRY_LO_REG		0x0004
#define RESET_ENTRY_HI_REG		0x0008
#define WAKEUP_EN_REG			0x0020
#define WAKEUP_MASK_REG(i)		(0x0024 + 4 * (i))

static void sunxi_riscv_cfg_save(void)
{
	/* Enable MMIO access. Do not assume S-mode leaves the clock enabled. */
	writel_relaxed(CCU_BGR_ENABLE, SUN20I_D1_CCU_BASE + RISCV_CFG_BGR_REG);

	/*
	 * Copy the SIE bits to the wakeup registers. D1 has 160 "real"
	 * interrupt sources, numbered 16-175. These are the ones that map to
	 * the wakeup mask registers (the offset is for GIC compatibility). So
	 * copying SIE to the wakeup mask needs some bit manipulation.
	 */
	for (int i = 0; i < PLIC_IE_WORDS - 1; i++)
		writel_relaxed(plic_sie[i] >> 16 | plic_sie[i + 1] << 16,
			       SUN20I_D1_RISCV_CFG_BASE + WAKEUP_MASK_REG(i));

	/* Enable PPU wakeup for interrupts. */
	writel_relaxed(1, SUN20I_D1_RISCV_CFG_BASE + WAKEUP_EN_REG);
}

static void sunxi_riscv_cfg_restore(void)
{
	/* Disable PPU wakeup for interrupts. */
	writel_relaxed(0, SUN20I_D1_RISCV_CFG_BASE + WAKEUP_EN_REG);
}

static void sunxi_riscv_cfg_init(void)
{
	u64 entry = sbi_hartid_to_scratch(0)->warmboot_addr;

	/* Enable MMIO access. */
	writel_relaxed(CCU_BGR_ENABLE, SUN20I_D1_CCU_BASE + RISCV_CFG_BGR_REG);

	/* Program the reset entry address. */
	writel_relaxed(entry, SUN20I_D1_RISCV_CFG_BASE + RESET_ENTRY_LO_REG);
	writel_relaxed(entry >> 32, SUN20I_D1_RISCV_CFG_BASE + RESET_ENTRY_HI_REG);
}

static int sunxi_hart_suspend(u32 suspend_type)
{
	/* Use the generic code for retentive suspend. */
	if (!(suspend_type & SBI_HSM_SUSP_NON_RET_BIT))
		return SBI_ENOTSUPP;

	sunxi_plic_save();
	sunxi_ppu_save();
	sunxi_riscv_cfg_save();
	sunxi_csr_save();

	/*
	 * If no interrupt is pending, this will power down the CPU power
	 * domain. Otherwise, this will fall through, and the generic HSM
	 * code will jump to the resume address.
	 */
	wfi();

	return 0;
}

static void sunxi_hart_resume(void)
{
	sunxi_csr_restore();
	sunxi_riscv_cfg_restore();
	sunxi_ppu_restore();
	sunxi_plic_restore();
}

static const struct sbi_hsm_device sunxi_ppu = {
	.name		= "sunxi_ppu",
	.hart_suspend	= sunxi_hart_suspend,
	.hart_resume	= sunxi_hart_resume,
};

int sunxi_final_init(bool cold_boot)
{
	if (cold_boot) {
		sunxi_riscv_cfg_init();
		sbi_hsm_set_device(&sunxi_ppu);
	}

	return 0;
}
