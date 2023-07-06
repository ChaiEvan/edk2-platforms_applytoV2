/** @file
  RamBus Pcie3.0 Dxe driver.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "RamBusPciExpress.h"

static inline UINT64 get_pcie_reg_base(UINT32 Port)
{
    return PCIE_REG_BASE + Port * 0x1000000;
}

VOID PcieRegWrite(UINT32 Port, UINTN Offset, UINT32 Value)
{
    UINT64 base = get_pcie_reg_base(Port);

    RegWrite((UINT64)base + Offset, Value);
}

UINT32 PcieRegRead(UINT32 Port, UINTN Offset)
{
    UINT32 Value = 0;
    UINT64 base = get_pcie_reg_base(Port);

    RegRead((UINT64)base + Offset, Value);
    return Value;
}

static VOID PcieUpdatebits(UINT64 base, UINTN Offset, UINT32 mask, UINT32 val)
{
    UINT32 Value = 0;

    Value = MmioRead32((UINT64)base + Offset);
    Value &= ~mask;
    Value |= val;
    MmioWrite32((UINT64)base + Offset, Value);
}

STATIC
VOID PcieFuncSet(
    IN UINT32 Port)
{
	int i;
	UINT32 Value;
	UINT64 base = get_pcie_reg_base(Port);

	/* Disable physical functions except #0 */
	for (i = 1; i < PLDA_FUNC_NUM; i++) {
		PcieUpdatebits(STG_SYSCON_BASE, STG_ARFUNC_OFFSET[Port],
			STG_SYSCON_AXI4_SLVL_ARFUNC_MASK,
			(i << PLDA_PHY_FUNC_SHIFT) <<
			STG_SYSCON_AXI4_SLVL_ARFUNC_SHIFT);
		PcieUpdatebits(STG_SYSCON_BASE, STG_AWFUNC_OFFSET[Port],
			STG_SYSCON_AXI4_SLVL_AWFUNC_MASK,
			(i << PLDA_PHY_FUNC_SHIFT) <<
			STG_SYSCON_AXI4_SLVL_AWFUNC_SHIFT);
		PcieUpdatebits(base, PCI_MISC,
			PLDA_FUNCTION_DIS, PLDA_FUNCTION_DIS);
	}

	PcieUpdatebits(STG_SYSCON_BASE, STG_ARFUNC_OFFSET[Port],
			STG_SYSCON_AXI4_SLVL_ARFUNC_MASK, 0);
	PcieUpdatebits(STG_SYSCON_BASE, STG_AWFUNC_OFFSET[Port],
			STG_SYSCON_AXI4_SLVL_AWFUNC_MASK, 0);

	/* Enable root port*/
	PcieUpdatebits(base, GEN_SETTINGS,
		PLDA_RP_ENABLE, PLDA_RP_ENABLE);

        Value = (IDS_PCI_TO_PCI_BRIDGE << IDS_CLASS_CODE_SHIFT);
	PcieRegWrite(Port, PCIE_PCI_IDS, Value);

	PcieUpdatebits(base, PMSG_SUPPORT_RX,
		PMSG_LTR_SUPPORT, 0);

	/* Prefetchable memory window 64-bit addressing support */
	PcieUpdatebits(base, PCIE_WINROM,
		PREF_MEM_WIN_64_SUPPORT, PREF_MEM_WIN_64_SUPPORT);
}

STATIC
VOID
PcieSTGInit(
    IN UINT32 Port)
{

    PcieUpdatebits(STG_SYSCON_BASE, STG_RP_REP_OFFSET[Port],
		STG_SYSCON_K_RP_NEP_MASK, 
		STG_SYSCON_K_RP_NEP_MASK);
    PcieUpdatebits(STG_SYSCON_BASE, STG_AWFUNC_OFFSET[Port],
		STG_SYSCON_CKREF_SRC_MASK, 
		2 << STG_SYSCON_CKREF_SRC_SHIFT);
    PcieUpdatebits(STG_SYSCON_BASE, STG_AWFUNC_OFFSET[Port],
		STG_SYSCON_CLKREQ_MASK, 
		STG_SYSCON_CLKREQ_MASK);
}

STATIC
VOID
PcieClockInit(
    IN UINT32 Port)
{
	RegWrite(STG_CLK_BASE + STG_PCIE_CLK_OFFSET
		+ Port * STG_PCIE_CLKS, 1 << 31); /*axi mst0*/
	RegWrite(STG_CLK_BASE + STG_PCIE_CLK_OFFSET
		+ Port * STG_PCIE_CLKS + 4, 1 << 31); /* apb */
	RegWrite(STG_CLK_BASE + STG_PCIE_CLK_OFFSET
		+ Port * STG_PCIE_CLKS + 8, 1 << 31); /* tl0 */
}


STATIC
VOID
PcieResetDeassert(
    IN UINT32 Port)
{
	UINT32 Portoffset = Port * 6 + 11;

	PcieUpdatebits(STG_CLK_BASE, STG_PCIE_RESET_OFFSET,
		0x3f << (Portoffset), 0); /*reset all*/
}

VOID
PcieResetAssert(
    IN UINT32 Port)
{
	UINT32 Portoffset = Port * 6 + 11;

	PcieUpdatebits(STG_CLK_BASE, STG_PCIE_RESET_OFFSET,
		0x3f << (Portoffset), 0x3f << (Portoffset)); /*axi mst0*/
}

STATIC
VOID PcieGpioResetSet(
    IN UINT32 Port,
    IN UINT32 Value)
{
	UINT32 remain, mask;

	remain = PCIE_GPIO[Port] & 0x3;
	mask = 0xff << (remain * 8);
	PcieUpdatebits(SYS_GPIO_BASE, SYS_GPIO_OUTPUT_OFF + (PCIE_GPIO[Port] & 0xfffc),
		mask, Value << (remain * 8));
}

STATIC
VOID PcieAtrInit(
    IN UINT32 Port,
    IN UINT64 src_addr,
    IN UINT64 trsl_addr,
    IN UINT32 win_size,
    IN UINT32 config)
{
	UINT64 base = get_pcie_reg_base(Port) + XR3PCI_ATR_AXI4_SLV0;
	UINT32 Value;

        base +=  XR3PCI_ATR_TABLE_OFFSET * AtrTableNum;
	AtrTableNum++;

        /* X3PCI_ATR_SRC_ADDR_LOW:
         *   - bit 0: enable entry,
         *   - bits 1-6: ATR window size: total size in bytes: 2^(ATR_WSIZE + 1)
         *   - bits 7-11: reserved
         *   - bits 12-31: start of source address
         */
	Value = src_addr;
	//DEBUG ((DEBUG_ERROR, "addr low %x\n", Value));
	RegWrite(base + XR3PCI_ATR_SRC_ADDR_LOW,
		(Value & XR3PCI_ATR_SRC_ADDR_MASK) | ((win_size - 1) << 1) | 0x1);
	Value = src_addr >> 32;
	//DEBUG ((DEBUG_ERROR, "addr high %x\n", Value));
	RegWrite(base + XR3PCI_ATR_SRC_ADDR_HIGH, Value);
	Value = trsl_addr;
	RegWrite(base + XR3PCI_ATR_TRSL_ADDR_LOW, Value);
	Value = trsl_addr >> 32;
	RegWrite(base + XR3PCI_ATR_TRSL_ADDR_HIGH, Value);
	RegWrite(base + XR3PCI_ATR_TRSL_PARAM, config);
}

/**
  RamBus Pcie3.0 Dxe driver. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
RamBusPcieEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT32		PortIndex = 1;

  DEBUG ((DEBUG_ERROR, "PCIe RootBridge constructor\n"));
  PcieSTGInit(PortIndex);
  RegWrite(SYS_CLK_BASE + SYS_CLK_NOC_OFFSET, 1 << 31);
  PcieClockInit(PortIndex);
  PcieResetDeassert(PortIndex);
  PcieGpioResetSet(PortIndex, 0);
  PcieFuncSet(PortIndex);
  
  PcieAtrInit(PortIndex, PCIE_CFG_BASE[PortIndex],
     0, XR3_PCI_ECAM_SIZE, 1);
  PcieAtrInit(PortIndex, PCI_MEMREGION_32[PortIndex],
     PCI_MEMREGION_32[PortIndex], PCI_MEMREGION_SIZE[0], 0);
  PcieAtrInit(PortIndex, PCI_MEMREGION_64[PortIndex],
     PCI_MEMREGION_64[PortIndex], PCI_MEMREGION_SIZE[1], 0);
  PcieGpioResetSet(PortIndex, 1);
  MicroSecondDelay(300);

  DEBUG ((DEBUG_ERROR, "PCIe port %d init\n", PortIndex));

  return EFI_SUCCESS;
}