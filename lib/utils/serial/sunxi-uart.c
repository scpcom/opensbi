/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 AllwinnerTech.
 *
 * Authors:
 *   liush <liush@allwinnertech.com>
 */

#include <sbi/sbi_console.h>
#include <sbi_utils/serial/sunxi-uart.h>

#define SUNXI_UART_THR     0
#define SUNXI_UART_RBR     0
#define SUNXI_UART_USR     0x1F //addr:0x7C
#define SUNXI_UART_USR_NF  0x02
#define SUNXI_UART_USR_RFNE  0x04

static volatile uint32_t* sunxi_uart;

static void sunxi_uart_putc(char ch)
{
	while ((sunxi_uart[SUNXI_UART_USR] & SUNXI_UART_USR_NF) == 0);
	sunxi_uart[SUNXI_UART_THR] = ch;
}

static int sunxi_uart_getc(void)
{
	if ((sunxi_uart[SUNXI_UART_USR] & SUNXI_UART_USR_RFNE) != 0)
		return sunxi_uart[SUNXI_UART_RBR];
	else
		return -1;
}

static struct sbi_console_device sunxi_console = {
		.name = "sunxi_uart",
		.console_putc = sunxi_uart_putc,
		.console_getc = sunxi_uart_getc
};

int sunxi_uart_init(unsigned long base)
{
	sunxi_uart = (volatile void *)base;

	sbi_console_set_device(&sunxi_console);

	return 0;
}
