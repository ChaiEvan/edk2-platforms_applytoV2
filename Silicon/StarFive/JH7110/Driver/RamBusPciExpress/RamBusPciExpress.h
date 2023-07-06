/** @file
  RamBus Pcie3.0 Dxe driver general header.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/DtIo.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <IndustryStandard/JH7110.h>

#ifndef _RAMBUS_PCIE_DXE_H_
#define _RAMBUS_PCIE_DXE_H_

#define RegWrite(addr,data)            MmioWrite32((addr), (data))
#define RegRead(addr,data)             ((data) = MmioRead32 (addr))

#define STG_SYSCON_BASE 0x10240000

#define STG_SYSCON_K_RP_NEP_MASK               (1 << 8)
#define STG_SYSCON_CKREF_SRC_SHIFT             18
#define STG_SYSCON_CKREF_SRC_MASK              (0x3 << 18)
#define STG_SYSCON_CLKREQ_MASK			(1 << 22)
#define STG_SYSCON_BASE 0x10240000
#define SYS_CLK_BASE 0x13020000
#define STG_CLK_BASE 0x10230000
#define SYS_CLK_NOC_OFFSET 0x98
#define STG_PCIE_CLK_OFFSET 0x20
#define STG_PCIE_CLKS 0xc
#define STG_PCIE_RESET_OFFSET 0x74
#define SYS_GPIO_BASE 0x13040000

#define PREF_MEM_WIN_64_SUPPORT         (1 << 3)
#define PMSG_LTR_SUPPORT                (1 << 2)
#define PDLA_LINK_SPEED_GEN2            (1 << 12)
#define PLDA_FUNCTION_DIS               (1 << 15)
#define PLDA_FUNC_NUM                   4
#define PLDA_PHY_FUNC_SHIFT             9
#define PLDA_RP_ENABLE                  1

#define PCIE_BASIC_STATUS               0x018
#define PCIE_CFGNUM                     0x140
#define IMASK_LOCAL                     0x180
#define ISTATUS_LOCAL                   0x184
#define IMSI_ADDR                       0x190
#define ISTATUS_MSI                     0x194
#define CFG_SPACE                       0x1000
#define GEN_SETTINGS                    0x80
#define PCIE_PCI_IDS                    0x9C
#define PCIE_WINROM                     0xFC
#define PMSG_SUPPORT_RX                 0x3F0
#define PCI_MISC                        0xB4

#define STG_SYSCON_AXI4_SLVL_ARFUNC_MASK        0x7FFF00
#define STG_SYSCON_AXI4_SLVL_ARFUNC_SHIFT       0x8
#define STG_SYSCON_AXI4_SLVL_AWFUNC_MASK        0x7FFF
#define STG_SYSCON_AXI4_SLVL_AWFUNC_SHIFT       0x0


#define XR3PCI_ATR_AXI4_SLV0            0x800
#define XR3PCI_ATR_SRC_ADDR_LOW         0x0
#define XR3PCI_ATR_SRC_ADDR_HIGH        0x4
#define XR3PCI_ATR_TRSL_ADDR_LOW        0x8
#define XR3PCI_ATR_TRSL_ADDR_HIGH       0xc
#define XR3PCI_ATR_TRSL_PARAM           0x10
#define XR3PCI_ATR_TABLE_OFFSET         0x20
#define XR3PCI_ATR_MAX_TABLE_NUM        8

#define XR3PCI_ATR_SRC_ADDR_MASK        0xfffff000
#define XR3PCI_ATR_TRSL_ADDR_MASK       0xfffff000
#define XR3PCI_ATR_SRC_WIN_SIZE_SHIFT   1
#define XR3_PCI_ECAM_SIZE		28

#define IDS_PCI_TO_PCI_BRIDGE           0x060400
#define IDS_CLASS_CODE_SHIFT            8
#define SYS_GPIO_OUTPUT_OFF		0x40

UINT32 AtrTableNum;
UINT64 PCIE_CFG_BASE[2] = {0x940000000, 0x9c0000000};
UINT64 PCI_MEMREGION_32[2] = {0x30000000, 0x38000000};
UINT64 PCI_MEMREGION_64[2] = {0x900000000, 0x980000000};
UINT64 PCI_MEMREGION_SIZE[2] = {27, 30};
UINT32 STG_ARFUNC_OFFSET[2] = {0xc0, 0x270};
UINT32 STG_AWFUNC_OFFSET[2] = {0xc4, 0x274};
UINT32 STG_RP_REP_OFFSET[2] = {0x130, 0x2e0};
UINT32 PCIE_GPIO[2] = {26, 28};

#endif