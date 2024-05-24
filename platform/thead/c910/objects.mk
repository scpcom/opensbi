#
# SPDX-License-Identifier: BSD-2-Clause
#

# Compiler flags
platform-cppflags-y =
platform-cflags-y = -g
platform-asflags-y = -g
platform-ldflags-y = -g

# Blobs to build
FW_DYNAMIC=y
FW_JUMP=y
ifeq ($(SUNXI_CHIP),sun20iw1p1)
FW_TEXT_START?=0x40000000
FW_JUMP_ADDR?=0x42000000
endif

ifeq ($(SUNXI_CHIP),sun20iw2p1)
FW_TEXT_START?=0x40000000
FW_JUMP_ADDR?=0x43000000
endif
#FW_TEXT_START?=0x20000
#FW_JUMP_ADDR?=0x400000

ifeq ($(PLATFORM_RISCV_ISA),rv64gcxthead)
platform-cflags-y += -DPLATFORM_XTHEAD
endif

platform-objs-y += sunxi_platform.o opensbi_head.o
ifeq ($(PLATFORM_RISCV_ISA),rv64gcxthead)
platform-objs-y += sunxi_cpuidle.o sunxi_idle.o
endif
platform-objs-y += sunxi_hsm.o thead_c9xx_plic.o thead_c9xx_pmu.o
