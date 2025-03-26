/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_error.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>

__asm__(".section .rodata\n"
	".global suspend_sram_entry\n"
	".global suspend_sram_end\n"
	".type suspend_sram_entry, @object\n"
	".type suspend_sram_end, @object\n"
	".align 4\n"
	"suspend_sram_entry:\n"
	".incbin \"" STRINGIFY(PM_SRAM_BIN_PATH) "\"\n"
	".align 4\n"
	"suspend_sram_end:\n"
	".text\n");

#define RTC_SRAM_FLAG_ADDR 0x05026ff8
#ifdef CONFIG_CV180X
#define SUSPEND_SRAM_ENTRY 0x3C000000
#endif
#ifdef CONFIG_CV181X
#define SUSPEND_SRAM_ENTRY 0xC030000
#endif
#define memcpy sbi_memcpy

static void rtc_latch_pinmux_settings(void)
{
	writel(0x2, (void *)0x50250ac);
	writel(0x0, (void *)0x5027084);
	//TODO
}

static void rtc_power_saving_settings_for_suspend(void)
{
	//TODO
}

static int cvitek_sbi_system_suspend_check(u32 sleep_type)
{
	return sleep_type == SBI_SUSP_SLEEP_TYPE_SUSPEND ? 0 : SBI_EINVAL;
}

static int cvitek_sbi_system_suspend(u32 sleep_type,
				unsigned long mmode_resume_addr)
{
	void (*suspend)(void) = (void *)SUSPEND_SRAM_ENTRY;

	if (sleep_type != SBI_SUSP_SLEEP_TYPE_SUSPEND)
		return SBI_EINVAL;

	rtc_latch_pinmux_settings();
	rtc_power_saving_settings_for_suspend();

	/* store warmboot entry for resume*/
	writel(mmode_resume_addr, (void *)RTC_SRAM_FLAG_ADDR);
	writel(readl((void *)0x03002000) | 0x10, (void *)0x03002000); //enable TPU clock
	memcpy((void *)SUSPEND_SRAM_ENTRY, suspend_sram_entry, suspend_sram_end - suspend_sram_entry);

	asm volatile("fence.i" ::: "memory");

	asm volatile("fence rw,rw\n\t");

	suspend();

	wfi();

	return SBI_OK;
}

static struct sbi_system_suspend_device cvitek_sbi_suspend_device = {
	.name = "cvi-suspend",
	.system_suspend_check = cvitek_sbi_system_suspend_check,
	.system_suspend = cvitek_sbi_system_suspend,
};

static int cvitek_riscv_early_init(bool cold_boot, const struct fdt_match *match)
{
	sbi_system_suspend_set_device(&cvitek_sbi_suspend_device);

	return 0;
}

#include <sbi/sbi_trap.h>
#define CSR_MCOUNTERWEN  0x7c9
static void sbi_thead_pmu_init(void)
{
	unsigned long interrupts;

	interrupts = csr_read(CSR_MIDELEG) | (1 << 17);
	csr_write(CSR_MIDELEG, interrupts);

	/* CSR_MCOUNTEREN has already been set in mstatus_init() */
	csr_write(CSR_MCOUNTERWEN, 0xffffffff);
	csr_write(CSR_MHPMEVENT3, 1);
	csr_write(CSR_MHPMEVENT4, 2);
	csr_write(CSR_MHPMEVENT5, 3);
	csr_write(CSR_MHPMEVENT6, 4);
	csr_write(CSR_MHPMEVENT7, 5);
	csr_write(CSR_MHPMEVENT8, 6);
	csr_write(CSR_MHPMEVENT9, 7);
	csr_write(CSR_MHPMEVENT10, 8);
	csr_write(CSR_MHPMEVENT11, 9);
	csr_write(CSR_MHPMEVENT12, 10);
	csr_write(CSR_MHPMEVENT13, 11);
	csr_write(CSR_MHPMEVENT14, 12);
	csr_write(CSR_MHPMEVENT15, 13);
	csr_write(CSR_MHPMEVENT16, 14);
	csr_write(CSR_MHPMEVENT17, 15);
	csr_write(CSR_MHPMEVENT18, 16);
	csr_write(CSR_MHPMEVENT19, 17);
	csr_write(CSR_MHPMEVENT20, 18);
	csr_write(CSR_MHPMEVENT21, 19);
	csr_write(CSR_MHPMEVENT22, 20);
	csr_write(CSR_MHPMEVENT23, 21);
	csr_write(CSR_MHPMEVENT24, 22);
	csr_write(CSR_MHPMEVENT25, 23);
	csr_write(CSR_MHPMEVENT26, 24);
	csr_write(CSR_MHPMEVENT27, 25);
	csr_write(CSR_MHPMEVENT28, 26);
}

static void sbi_thead_pmu_map(unsigned long idx, unsigned long event_id)
{
	switch (idx) {
	case 3:
		csr_write(CSR_MHPMEVENT3, event_id);
		break;
	case 4:
		csr_write(CSR_MHPMEVENT4, event_id);
		break;
	case 5:
		csr_write(CSR_MHPMEVENT5, event_id);
		break;
	case 6:
		csr_write(CSR_MHPMEVENT6, event_id);
		break;
	case 7:
		csr_write(CSR_MHPMEVENT7, event_id);
		break;
	case 8:
		csr_write(CSR_MHPMEVENT8, event_id);
		break;
	case 9:
		csr_write(CSR_MHPMEVENT9, event_id);
		break;
	case 10:
		csr_write(CSR_MHPMEVENT10, event_id);
		break;
	case 11:
		csr_write(CSR_MHPMEVENT11, event_id);
		break;
	case 12:
		csr_write(CSR_MHPMEVENT12, event_id);
		break;
	case 13:
		csr_write(CSR_MHPMEVENT13, event_id);
		break;
	case 14:
		csr_write(CSR_MHPMEVENT14, event_id);
		break;
	case 15:
		csr_write(CSR_MHPMEVENT15, event_id);
		break;
	case 16:
		csr_write(CSR_MHPMEVENT16, event_id);
		break;
	case 17:
		csr_write(CSR_MHPMEVENT17, event_id);
		break;
	case 18:
		csr_write(CSR_MHPMEVENT18, event_id);
		break;
	case 19:
		csr_write(CSR_MHPMEVENT19, event_id);
		break;
	case 20:
		csr_write(CSR_MHPMEVENT20, event_id);
		break;
	case 21:
		csr_write(CSR_MHPMEVENT21, event_id);
		break;
	case 22:
		csr_write(CSR_MHPMEVENT22, event_id);
		break;
	case 23:
		csr_write(CSR_MHPMEVENT23, event_id);
		break;
	case 24:
		csr_write(CSR_MHPMEVENT24, event_id);
		break;
	case 25:
		csr_write(CSR_MHPMEVENT25, event_id);
		break;
	case 26:
		csr_write(CSR_MHPMEVENT26, event_id);
		break;
	case 27:
		csr_write(CSR_MHPMEVENT27, event_id);
		break;
	case 28:
		csr_write(CSR_MHPMEVENT28, event_id);
		break;
	case 29:
		csr_write(CSR_MHPMEVENT29, event_id);
		break;
	case 30:
		csr_write(CSR_MHPMEVENT30, event_id);
		break;
	case 31:
		csr_write(CSR_MHPMEVENT31, event_id);
		break;
	}
}

static void sbi_thead_pmu_set(unsigned long type, unsigned long idx, unsigned long event_id)
{
	switch (type) {
	case 2:
		sbi_thead_pmu_map(idx, event_id);
		break;
	default:
		sbi_thead_pmu_init();
		break;
	}
}

static int thead_vendor_ext_check(long extid, const struct fdt_match *match)
{
	return 1;
}

static int thead_vendor_ext_provider(long extid, long funcid,
	const struct sbi_trap_regs *regs, unsigned long *out_value,
	struct sbi_trap_info *out_trap, const struct fdt_match *match)
{
	switch (extid) {
	case 0x09000001:
		sbi_thead_pmu_set(regs->a0, regs->a1, regs->a2);
		break;
	default:
		while(1);
	}
	return 0;
}

static const struct fdt_match cvitek_riscv_match[] = {
	{ .compatible = "cvitek,cv180x" },
	{ .compatible = "cvitek,cv181x" }
};

const struct platform_override cvitek_riscv = {
	.match_table = cvitek_riscv_match,
	.early_init = cvitek_riscv_early_init,
	.vendor_ext_check = thead_vendor_ext_check,
	.vendor_ext_provider = thead_vendor_ext_provider,
};
