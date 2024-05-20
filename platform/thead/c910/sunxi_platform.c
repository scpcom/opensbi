/*
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_trap.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/sys/clint.h>
#include <sbi_utils/serial/sunxi-uart.h>
#include "sunxi_platform.h"
#include "private_opensbi.h"
#include "sbi/sbi_ecall_interface.h"

#define SBI_SET_WAKEUP_TIMER          (SBI_EXT_VENDOR_START + 0x1000)
#define SBI_SET_DEBUG_LEVEL             (SBI_EXT_VENDOR_START + 0x1001)
#define SBI_SET_DEBUG_DRAM_CRC_PARAS    (SBI_EXT_VENDOR_START + 0x1002)
#define SBI_SET_UART_BAUDRATE           (SBI_EXT_VENDOR_START + 0x1003)

extern struct private_opensbi_head  opensbi_head;
struct c910_regs_struct c910_regs;

static struct clint_data clint = {
	.addr = 0, /* Updated at cold boot time */
	.first_hartid = 0,
	.hart_count = C910_HART_COUNT,
	.has_64bit_mmio = FALSE,
};

extern int sbi_set_wakeup_src_timer(uint32_t wakeup_irq);
extern int sbi_set_dram_crc_paras(long dram_crc_en, long dram_crc_srcaddr,
				  long dram_crc_len);

static void c910_delegate_traps()
{
	unsigned long exceptions = csr_read(CSR_MEDELEG);

	/* Delegate 0 ~ 7 exceptions to S-mode */
	exceptions |= ((1U << CAUSE_MISALIGNED_FETCH) | (1U << CAUSE_FETCH_ACCESS) |
		(1U << CAUSE_ILLEGAL_INSTRUCTION) | (1U << CAUSE_BREAKPOINT) |
		(1U << CAUSE_MISALIGNED_LOAD) | (1U << CAUSE_LOAD_ACCESS) |
		(1U << CAUSE_MISALIGNED_STORE) | (1U << CAUSE_STORE_ACCESS));
	csr_write(CSR_MEDELEG, exceptions);
}

static void c910_system_shutdown(u32 type, u32 reason)
{
	/*TODO:power down something*/
	while(1);
}

static void sunxi_system_reset(u32 type, u32 reason)
{
	if (!type) {
		c910_system_shutdown(type, reason);
		return;
	}

	sbi_printf("sbi reboot\n");
	unsigned int value = 0;
	void *reg = NULL;

	/* config reset whole system */
	value = (0x1 << SUNXI_WDOG_CFG_CONFIG_OFFSET) |
		       (SUNXI_WDOG_KEY1 << SUNXI_WDOG_CFG_KEY_OFFSET);
	reg = (void *)(SUNXI_WDOG_BASE + SUNXI_WDOG_CFG_REG);
	writel(value,reg);

	/* enable wdog */
	value = (0x1 << SUNXI_WDOG_MODE_EN_OFFSET) |
		       (SUNXI_WDOG_KEY1 << SUNXI_WDOG_CFG_KEY_OFFSET);
	reg = (void *)(SUNXI_WDOG_BASE + SUNXI_WDOG_MODE_REG);
	writel(value, reg);
}

static struct sbi_system_reset_device c910_reset = {
	.name = "thead_c910_reset",
	//.system_reset_check = c910_system_reset_check,
	.system_reset = sunxi_system_reset
};

static int c910_early_init(bool cold_boot)
{
	unsigned long addr = 0;
		addr = opensbi_head.dtb_base;
		sbi_printf("opensbi r: %ld\n", addr);
	if (cold_boot) {
		sbi_system_reset_set_device(&c910_reset);

		/* Load from boot core */
		c910_regs.pmpaddr0 = csr_read(CSR_PMPADDR0);
		c910_regs.pmpaddr1 = csr_read(CSR_PMPADDR1);
		c910_regs.pmpaddr2 = csr_read(CSR_PMPADDR2);
		c910_regs.pmpaddr3 = csr_read(CSR_PMPADDR3);
		c910_regs.pmpaddr4 = csr_read(CSR_PMPADDR4);
		c910_regs.pmpaddr5 = csr_read(CSR_PMPADDR5);
		c910_regs.pmpaddr6 = csr_read(CSR_PMPADDR6);
		c910_regs.pmpaddr7 = csr_read(CSR_PMPADDR7);
		c910_regs.pmpcfg0  = csr_read(CSR_PMPCFG0);

		c910_regs.mcor     = csr_read(CSR_MCOR);
		c910_regs.mhcr     = csr_read(CSR_MHCR);
		c910_regs.mccr2    = csr_read(CSR_MCCR2);
		c910_regs.mhint    = csr_read(CSR_MHINT);
		c910_regs.mxstatus = csr_read(CSR_MXSTATUS);

		c910_regs.plic_base_addr = csr_read(CSR_PLIC_BASE);
		c910_regs.clint_base_addr =
			c910_regs.plic_base_addr + C910_PLIC_CLINT_OFFSET;
	} else {
		/* Store to other core */
		csr_write(CSR_PMPADDR0, c910_regs.pmpaddr0);
		csr_write(CSR_PMPADDR1, c910_regs.pmpaddr1);
		csr_write(CSR_PMPADDR2, c910_regs.pmpaddr2);
		csr_write(CSR_PMPADDR3, c910_regs.pmpaddr3);
		csr_write(CSR_PMPADDR4, c910_regs.pmpaddr4);
		csr_write(CSR_PMPADDR5, c910_regs.pmpaddr5);
		csr_write(CSR_PMPADDR6, c910_regs.pmpaddr6);
		csr_write(CSR_PMPADDR7, c910_regs.pmpaddr7);
		csr_write(CSR_PMPCFG0, c910_regs.pmpcfg0);

		csr_write(CSR_MCOR, c910_regs.mcor);
		csr_write(CSR_MHCR, c910_regs.mhcr);
		csr_write(CSR_MHINT, c910_regs.mhint);
		csr_write(CSR_MXSTATUS, c910_regs.mxstatus);
	}

	return 0;
}

static int c910_final_init(bool cold_boot)
{
	c910_delegate_traps();

	return 0;
}

static int try_uart_port(void)
{
	unsigned int reg, port_num;

	reg = readl((unsigned int *)(SUNXI_CCU_BASE + SUNXI_UART_BGR_REG));

	for (port_num = 0; port_num <= SUNXI_UART_MAX; port_num++) {
		if (reg & (1 << port_num)) {
			return port_num;
			break;
		}
	}
	return -1;
}

static int sunxi_console_init(void)
{
	unsigned int port_num, uart_base;

	port_num = try_uart_port();
	uart_base = SUNXI_UART_BASE + port_num * SUNXI_UART_ADDR_OFFSET;

	return sunxi_uart_init(uart_base);
}

static int c910_irqchip_init(bool cold_boot)
{
	/* Delegate plic enable into S-mode */
	writel(C910_PLIC_DELEG_ENABLE,
		(void *)c910_regs.plic_base_addr + C910_PLIC_DELEG_OFFSET);

	return 0;
}

static int c910_ipi_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		clint.addr = c910_regs.clint_base_addr;
		rc = clint_cold_ipi_init(&clint);
		if (rc)
			return rc;
	}

	return clint_warm_ipi_init();
}

static int c910_timer_init(bool cold_boot)
{
	int ret;

	if (cold_boot) {
		clint.addr = c910_regs.clint_base_addr;
		ret = clint_cold_timer_init(&clint, NULL);
		if (ret)
			return ret;
	}

	return clint_warm_timer_init();
}

void sbi_set_pmu()
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

extern void sbi_system_suspend(int state);
extern void sbi_system_set_wakeup(unsigned long irq, unsigned long on);

void sbi_boot_other_core(int hartid)
{
	csr_write(CSR_MRVBR, FW_TEXT_START);
	csr_write(CSR_MRMR, csr_read(CSR_MRMR) | (1 << hartid));
}

static int c910_vendor_ext_provider(long extid, long funcid,
				const struct sbi_trap_regs *regs,
				unsigned long *out_value,
				struct sbi_trap_info *out_trap)
{
	switch (extid) {
	case SBI_EXT_VENDOR_C910_BOOT_OTHER_CORE:
		sbi_boot_other_core((int)regs->a0);
		break;
	case SBI_EXT_VENDOR_C910_SET_PMU:
		sbi_set_pmu();
		break;
	case SBI_EXT_VENDOR_C910_SYSPEND:
		sbi_system_suspend(regs->a0);
		break;
	case SBI_SET_WAKEUP_TIMER:
		sbi_set_wakeup_src_timer((unsigned int)regs->a0);
		break;
	case SBI_SET_DEBUG_LEVEL:
		break;
	case SBI_SET_DEBUG_DRAM_CRC_PARAS:
		sbi_set_dram_crc_paras(regs->a0, regs->a1, regs->a2);
		break;
	case SBI_SET_UART_BAUDRATE:
		break;
	case SBI_EXT_VENDOR_C910_WAKEUP:
		sbi_system_set_wakeup(regs->a0, regs->a1);
		break;

	default:
		sbi_printf("Unsupported private sbi call: %ld\n", extid);
		asm volatile ("ebreak");
	}
	return 0;
}

const struct sbi_platform_operations platform_ops = {
	.early_init          = c910_early_init,
	.final_init          = c910_final_init,

	.console_init        = sunxi_console_init,

	.irqchip_init        = c910_irqchip_init,

	.ipi_init            = c910_ipi_init,

	.timer_init          = c910_timer_init,

	.vendor_ext_provider = c910_vendor_ext_provider,
};

const struct sbi_platform platform = {
	.opensbi_version     = OPENSBI_VERSION,
	.platform_version    = SBI_PLATFORM_VERSION(0x0, 0x01),
	.name                = "T-HEAD Xuantie Platform",
	.features            = SBI_THEAD_FEATURES,
	.hart_count          = C910_HART_COUNT,
	.hart_stack_size     = C910_HART_STACK_SIZE,
	.platform_ops_addr   = (unsigned long)&platform_ops
};
