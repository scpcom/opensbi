/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hartmask.h>
#include <sbi_utils/irqchip/plic.h>
#include <thead_c9xx_plic.h>

#define PLIC_MAX_NR			16

static unsigned long plic_count = 0;
static struct plic_data plic[PLIC_MAX_NR];

static struct plic_data *plic_hartid2data[SBI_HARTMASK_MAX_BITS];
static int plic_hartid2context[SBI_HARTMASK_MAX_BITS][2];

static void thead_plic_plat_init(struct plic_data *pd);

void thead_c9xx_plic_priority_save(u8 *priority, u32 num)
{
	struct plic_data *plic = plic_hartid2data[current_hartid()];

	plic_priority_save(plic, priority, num);
}

void thead_c9xx_plic_priority_restore(const u8 *priority, u32 num)
{
	struct plic_data *plic = plic_hartid2data[current_hartid()];

	plic_priority_restore(plic, priority, num);
}

void thead_c9xx_plic_context_save(bool smode, u32 *enable, u32 *threshold, u32 num)
{
	u32 hartid = current_hartid();

	plic_context_save(plic_hartid2data[hartid],
			  plic_hartid2context[hartid][smode],
			  enable, threshold, num);
}

void thead_c9xx_plic_context_restore(bool smode, const u32 *enable, u32 threshold,
			      u32 num)
{
	u32 hartid = current_hartid();

	plic_context_restore(plic_hartid2data[hartid],
			     plic_hartid2context[hartid][smode],
			     enable, threshold, num);
}

static int thead_c9xx_irqchip_plic_warm_init(void)
{
	u32 hartid = current_hartid();

	return plic_warm_irqchip_init(plic_hartid2data[hartid],
				      plic_hartid2context[hartid][0],
				      plic_hartid2context[hartid][1]);
}

static int thead_c9xx_irqchip_plic_update_hartid_table(struct plic_data *pd)
{
	const u32 val[4] = { 0, IRQ_M_EXT, 1, IRQ_S_EXT };
	u32 hwirq, hartid;
	int i, count;

	count = 4;
	hartid = 0;

	for (i = 0; i < count; i += 2) {
		hwirq = val[i + 1];

		if (SBI_HARTMASK_MAX_BITS <= hartid)
			continue;

		plic_hartid2data[hartid] = pd;
		switch (hwirq) {
		case IRQ_M_EXT:
			plic_hartid2context[hartid][0] = i / 2;
			break;
		case IRQ_S_EXT:
			plic_hartid2context[hartid][1] = i / 2;
			break;
		}
	}

	return 0;
}

static int thead_c9xx_irqchip_plic_cold_init(void)
{
	int i, rc;
	struct plic_data *pd;

	if (PLIC_MAX_NR <= plic_count)
		return SBI_ENOSPC;
	pd = &plic[plic_count++];

	plic->addr = 0x10000000;
	plic->num_src = 175;

	thead_plic_plat_init(pd);

	rc = plic_cold_irqchip_init(pd);
	if (rc)
		return rc;

	if (plic_count == 1) {
		for (i = 0; i < SBI_HARTMASK_MAX_BITS; i++) {
			plic_hartid2data[i] = NULL;
			plic_hartid2context[i][0] = -1;
			plic_hartid2context[i][1] = -1;
		}
	}

	return thead_c9xx_irqchip_plic_update_hartid_table(pd);
}

#define THEAD_PLIC_CTRL_REG 0x1ffffc

static void thead_plic_plat_init(struct plic_data *pd)
{
	writel_relaxed(BIT(0), (char *)pd->addr + THEAD_PLIC_CTRL_REG);
}

void thead_c9xx_plic_restore(void)
{
	struct plic_data *plic = plic_hartid2data[current_hartid()];

	thead_plic_plat_init(plic);
}

int c9xx_irqchip_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		rc = thead_c9xx_irqchip_plic_cold_init();
		if (rc)
			return rc;
	}

	return thead_c9xx_irqchip_plic_warm_init();
}
