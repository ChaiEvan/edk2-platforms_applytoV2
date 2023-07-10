/** @file
  RamBus Pcie3.0 Dxe driver.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include "RamBusPciExpress.h"

VOID              *gDeviceTreeBase;
UINTN             mPcieRegBase = 0x2B000000;
UINTN             mStgSysconBase = 0x10240000;
UINTN             mStgClkBase = 0x10230000;
UINTN             mSysGpioBase = 0x13040000;
UINTN             mSysClkBase = 0x13020000;
UINTN             mPciConfigRegBase = 0x9C0000000;

EFI_STATUS
EFIAPI
GetFdtDTBase (
  VOID
  )
{
  VOID        *Hob;
  VOID        *DeviceTreeBase;

  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64))) {
    DEBUG ((DEBUG_ERROR, "No FDT passed in to UEFI\n"));
    return EFI_NOT_FOUND;
  }

  DeviceTreeBase = (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);
  if (fdt_check_header (DeviceTreeBase) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: DTB @ %p seems corrupted?\n",
      __func__,
      DeviceTreeBase
      ));
    return EFI_NOT_FOUND;
  }

  gDeviceTreeBase = DeviceTreeBase;
  DEBUG ((DEBUG_ERROR, "gDeviceTreeBase == [%lx]. \n", gDeviceTreeBase));

  return EFI_SUCCESS;
}

#if 1
STATIC
BOOLEAN
IsNodeEnabled2 (
  INT32  Node
  )
{
  CONST CHAR8  *NodeStatus;
  INT32        Len;

  //
  // A missing status property implies 'ok' so ignore any errors that
  // may occur here. If the status property is present, check whether
  // it is set to 'ok' or 'okay', anything else is treated as 'disabled'.
  //
  NodeStatus = fdt_getprop (gDeviceTreeBase, Node, "status", &Len);
  if (NodeStatus == NULL) {
    return TRUE;
  }

  if ((Len >= 5) && (AsciiStrCmp (NodeStatus, "okay") == 0)) {
    return TRUE;
  }

  if ((Len >= 3) && (AsciiStrCmp (NodeStatus, "ok") == 0)) {
    return TRUE;
  }

  return FALSE;
}
#endif

/**
  Given an FdtNode, return the device_type property or the empty string.

  @param[in]    TreeBase         Device Tree blob base
  @param[in]    FdtNode          INTN

  @retval CHAR8 *                Model name or empty string.

**/
CONST CHAR8 *
FdtGetRegNames (
  IN VOID    *TreeBase,
  IN  UINTN  FdtNode,
  IN  CONST CHAR8  *CompatibleString,
  OUT UINTN  *IndexOfReg
  )
{
  CONST CHAR8  *Buf;
  INT32        Prev, Next;
  CONST CHAR8  *Type, *Compatible;
  INT32        Len;

  Buf = NULL;

  for (Prev = 0; ; Prev = Next) {
    Next = fdt_next_node (gDeviceTreeBase, Prev, NULL);
    if (Next < 0) {
      break;
    }

    if (!IsNodeEnabled2 (Next)) {
      continue;
    }

    // if FdtNode is known, next will be replaced by FdtNode the
	// for loop outside will be removed.
    Type = fdt_getprop (gDeviceTreeBase, Next, "reg-names", &Len);
    if (Type == NULL) {
      continue;
    }

    //
    // A 'compatible' node may contain a sequence of NUL terminated
    // compatible strings so check each one
    //
    for (Compatible = Type, *IndexOfReg = 0; Compatible < Type + Len && *Compatible;
         Compatible += 1 + AsciiStrLen (Compatible), (*IndexOfReg)++)
    {
	  DEBUG ((DEBUG_ERROR, "====[%a]=====\n", Compatible));
      if (AsciiStrCmp (CompatibleString, Compatible) == 0) {
        break;
      }

      #if 0
      if (AsciiStrCmp ("config", Compatible) == 0) {
        //*Node = Next;
		//Buf = fdt_getprop (TreeBase, Next, "reg-names", NULL);
        //return EFI_SUCCESS;
        name = fdt_get_name (gDeviceTreeBase, Next, &Label_len);
        DEBUG ((DEBUG_ERROR, "Located the label--> [%a], Len[%d] \n", name, Label_len));
		break;
      }
      #endif

    }
  }

  return Buf;
}

VOID
Evan_Units_NO1 (
  IN OUT	EFI_DT_IO_PROTOCOL   **DtIo
  )
{
  UINTN                Index;
  EFI_STATUS           Status;
  UINTN                HandleCount;
  EFI_HANDLE           *HandleBuffer;
  UINTN                IndexOfReg;
  EFI_DT_REG           Reg;

  IndexOfReg = 0;
  Status = GetFdtDTBase ();
  ASSERT_EFI_ERROR (Status);

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDtIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiDtIoProtocolGuid, (VOID **)DtIo);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: HandleProtocol: %r\n",
        __func__,
        Status
        ));
      continue;
    } else {
      #if 0
		  if (!(DtIo->IsCompatible (DtIo, "stg_syscon"))) {
		    // find the Pcie device
        //name = FdtGetRegNames (gDeviceTreeBase, 0, &IndexOfReg);
        DEBUG ((DEBUG_ERROR, "got the syscon... \n"));
        Status = DtIo->GetReg (DtIo, 0, &Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetReg is failed. \n",
            __func__
            ));
        } else {
		      mStgSysconBase = Reg.Base;
		      DEBUG ((DEBUG_ERROR, "mStgSysconBase == [%lx]. \n", mStgSysconBase));
		    }
		  }

		  if (!(DtIo->IsCompatible (DtIo, "starfive,jh7110-sys-pinctrl"))) {
        Status = DtIo->GetRegNames (DtIo, "control", &IndexOfReg);
		    if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetRegNames is failed. \n",
            __func__
            ));
        }
        Status = DtIo->GetReg (DtIo, IndexOfReg, &Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetReg is failed. \n",
            __func__
            ));
        } else {
		      mSysGpioBase = Reg.Base;
		      DEBUG ((DEBUG_ERROR, "mSysGpioBase == [%lx]. \n", mSysGpioBase));
		    }
		  }

		  if (!(DtIo->IsCompatible (DtIo, "starfive,jh7110-reset"))) {
        Status = DtIo->GetRegNames (DtIo, "syscrg", &IndexOfReg);
		    if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetRegNames is failed. \n",
            __func__
            ));
        }
        Status = DtIo->GetReg (DtIo, IndexOfReg, &Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetReg is failed. \n",
            __func__
            ));
        } else {
			    mSysClkBase = Reg.Base;
			    DEBUG ((DEBUG_ERROR, "mSysClkBase == [%lx]. \n", mSysClkBase));
		    }

		    Status = DtIo->GetRegNames (DtIo, "stgcrg", &IndexOfReg);
		    if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetRegNames is failed. \n",
            __func__
            ));
        }
        Status = DtIo->GetReg (DtIo, IndexOfReg, &Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetReg is failed. \n",
            __func__
            ));
        } else {
			    mStgClkBase = Reg.Base;
			    DEBUG ((DEBUG_ERROR, "mStgClkBase == [%lx]. \n", mStgClkBase));
		    }
		  }

      if (!(DtIo->IsCompatible (DtIo, "starfive,jh7110-pcie0"))) {
		    // find the Pcie device
        Status = DtIo->GetRegNames (DtIo, "reg", &IndexOfReg);
		    if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetRegNames is failed. \n",
            __func__
            ));
        }
        Status = DtIo->GetReg (DtIo, IndexOfReg, &Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetReg is failed. \n",
            __func__
            ));
        } else {
          mPcieRegBase = Reg.Base;
			    DEBUG ((DEBUG_ERROR, "mPcieRegBase == [%lx]. \n", mPcieRegBase));
		    }
		  }
      #endif

		  if (!((*DtIo)->IsCompatible (*DtIo, "starfive,jh7110-pcie1"))) {
		    Status = (*DtIo)->GetRegNames (*DtIo, "config", &IndexOfReg);
		    if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetRegNames is failed. \n",
            __func__
            ));
        }
        Status = (*DtIo)->GetReg (*DtIo, IndexOfReg, &Reg);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetReg is failed. \n",
            __func__
            ));
        } else {
			    mPciConfigRegBase = Reg.Base;
			    DEBUG ((DEBUG_ERROR, "mPciConfigRegBase == [%lx]. \n", mPciConfigRegBase));
          break;
		    }
		  }
	  }
  }

  return;
}

static inline UINT64 get_pcie_reg_base(UINT32 Port)
{
    return mPcieRegBase + Port * 0x1000000;
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

EFI_STATUS 
PcieUpdatebits (
  IN EFI_DT_IO_PROTOCOL   *DtIo,
  UINT64 base, UINTN Offset, UINT32 mask, UINT32 val)
{
	EFI_STATUS           Status;
    UINT32 Value;
	EFI_DT_REG           Reg;

	Value = 0;
	Reg.Base   = base;
	Reg.Length = 0x00010000;

    Status = DtIo->ReadReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
      return Status;
    }

	  Value &= ~mask;
    Value |= val;

    Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
      return Status;
    }

    return EFI_SUCCESS;
}

STATIC
VOID PcieFuncSet(
  IN EFI_DT_IO_PROTOCOL   *DtIo,
    IN UINT32 Port)
{
	int i;
	UINT32 Value;
	UINT64 base = get_pcie_reg_base(Port);

	/* Disable physical functions except #0 */
	for (i = 1; i < PLDA_FUNC_NUM; i++) {
		PcieUpdatebits(DtIo, STG_SYSCON_BASE, STG_ARFUNC_OFFSET[Port],
			STG_SYSCON_AXI4_SLVL_ARFUNC_MASK,
			(i << PLDA_PHY_FUNC_SHIFT) <<
			STG_SYSCON_AXI4_SLVL_ARFUNC_SHIFT);
		PcieUpdatebits(DtIo, STG_SYSCON_BASE, STG_AWFUNC_OFFSET[Port],
			STG_SYSCON_AXI4_SLVL_AWFUNC_MASK,
			(i << PLDA_PHY_FUNC_SHIFT) <<
			STG_SYSCON_AXI4_SLVL_AWFUNC_SHIFT);
		PcieUpdatebits(DtIo, base, PCI_MISC,
			PLDA_FUNCTION_DIS, PLDA_FUNCTION_DIS);
	}

	PcieUpdatebits(DtIo, STG_SYSCON_BASE, STG_ARFUNC_OFFSET[Port],
			STG_SYSCON_AXI4_SLVL_ARFUNC_MASK, 0);
	PcieUpdatebits(DtIo, STG_SYSCON_BASE, STG_AWFUNC_OFFSET[Port],
			STG_SYSCON_AXI4_SLVL_AWFUNC_MASK, 0);

	/* Enable root port*/
	PcieUpdatebits(DtIo, base, GEN_SETTINGS,
		PLDA_RP_ENABLE, PLDA_RP_ENABLE);

        Value = (IDS_PCI_TO_PCI_BRIDGE << IDS_CLASS_CODE_SHIFT);
	PcieRegWrite(Port, PCIE_PCI_IDS, Value);

	PcieUpdatebits(DtIo, base, PMSG_SUPPORT_RX,
		PMSG_LTR_SUPPORT, 0);

	/* Prefetchable memory window 64-bit addressing support */
	PcieUpdatebits(DtIo, base, PCIE_WINROM,
		PREF_MEM_WIN_64_SUPPORT, PREF_MEM_WIN_64_SUPPORT);
}

static VOID PcieUpdatebits_org(UINT64 base, UINTN Offset, UINT32 mask, UINT32 val)
{
    UINT32 Value = 0;

    Value = MmioRead32((UINT64)base + Offset);
    Value &= ~mask;
    Value |= val;
    MmioWrite32((UINT64)base + Offset, Value);
}

STATIC
VOID
PcieSTGInit(
	IN EFI_DT_IO_PROTOCOL   *DtIo,
    IN UINT32 Port)
{

    PcieUpdatebits_org (STG_SYSCON_BASE, STG_RP_REP_OFFSET[Port],
		STG_SYSCON_K_RP_NEP_MASK, 
		STG_SYSCON_K_RP_NEP_MASK);
    PcieUpdatebits(DtIo, STG_SYSCON_BASE, STG_AWFUNC_OFFSET[Port],
		STG_SYSCON_CKREF_SRC_MASK, 
		2 << STG_SYSCON_CKREF_SRC_SHIFT);
    PcieUpdatebits(DtIo, STG_SYSCON_BASE, STG_AWFUNC_OFFSET[Port],
		STG_SYSCON_CLKREQ_MASK, 
		STG_SYSCON_CLKREQ_MASK);
}

EFI_STATUS
PcieClockInit(
  IN EFI_DT_IO_PROTOCOL   *DtIo,
    IN UINT32 Port)
{
  EFI_STATUS           Status;
  UINT32 Value;
	EFI_DT_REG           Reg;
  UINTN                Offset;

  //STG_CLK_BASE 0x10230000
  Reg.Base   = mStgClkBase;
	Reg.Length = 0x00010000;
  Value      = 1 << 31;

  Offset     = STG_PCIE_CLK_OFFSET + Port * STG_PCIE_CLKS;
  Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
    return Status;
  }

  Offset     = STG_PCIE_CLK_OFFSET + Port * STG_PCIE_CLKS + 4;
  Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
    return Status;
  }

  Offset     = STG_PCIE_CLK_OFFSET + Port * STG_PCIE_CLKS + 8;
  Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
    return Status;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
PcieResetDeassert(
  IN EFI_DT_IO_PROTOCOL   *DtIo,
    IN UINT32 Port)
{
	UINT32 Portoffset = Port * 6 + 11;

	PcieUpdatebits(DtIo, mStgClkBase, STG_PCIE_RESET_OFFSET,
		0x3f << (Portoffset), 0); /*reset all*/
}

VOID
PcieResetAssert(
  IN EFI_DT_IO_PROTOCOL   *DtIo,
    IN UINT32 Port)
{
	UINT32 Portoffset = Port * 6 + 11;

	PcieUpdatebits(DtIo, mStgClkBase, STG_PCIE_RESET_OFFSET,
		0x3f << (Portoffset), 0x3f << (Portoffset)); /*axi mst0*/
}

STATIC
VOID PcieGpioResetSet(
  IN EFI_DT_IO_PROTOCOL   *DtIo,
    IN UINT32 Port,
    IN UINT32 Value)
{
	UINT32 remain, mask;

	remain = PCIE_GPIO[Port] & 0x3;
	mask = 0xff << (remain * 8);
	PcieUpdatebits(DtIo, mSysGpioBase, SYS_GPIO_OUTPUT_OFF + (PCIE_GPIO[Port] & 0xfffc),
		mask, Value << (remain * 8));
}

EFI_STATUS
PcieAtrInit(
  IN EFI_DT_IO_PROTOCOL   *DtIo,
    IN UINT32 Port,
    IN UINT64 src_addr,
    IN UINT64 trsl_addr,
    IN UINT32 win_size,
    IN UINT32 config)
{
	UINT64 base = get_pcie_reg_base(Port) + XR3PCI_ATR_AXI4_SLV0;
	UINT32 Value;
  EFI_STATUS           Status;
  EFI_DT_REG           Reg;
  UINTN                Offset;

        base +=  XR3PCI_ATR_TABLE_OFFSET * AtrTableNum;
	AtrTableNum++;

        /* X3PCI_ATR_SRC_ADDR_LOW:
         *   - bit 0: enable entry,
         *   - bits 1-6: ATR window size: total size in bytes: 2^(ATR_WSIZE + 1)
         *   - bits 7-11: reserved
         *   - bits 12-31: start of source address
         */
	Value = src_addr;

  Reg.Base   = base ;
	Reg.Length = 0x1000000;

  Offset = XR3PCI_ATR_SRC_ADDR_LOW;
  Value = (Value & XR3PCI_ATR_SRC_ADDR_MASK) | ((win_size - 1) << 1) | 0x1;
  Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
    return Status;
  }

  Offset = XR3PCI_ATR_SRC_ADDR_HIGH;
  Value = src_addr >> 32;
  Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
    return Status;
  }

  Offset = XR3PCI_ATR_TRSL_ADDR_LOW;
  Value = trsl_addr;
  Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
    return Status;
  }

  Offset = XR3PCI_ATR_TRSL_ADDR_HIGH;
  Value = trsl_addr >> 32;
  Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
    return Status;
  }

  Offset = XR3PCI_ATR_TRSL_PARAM;
  Status = DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg, Offset, 1, &config);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,"Warning: failed to ReadReg. \n"));
    return Status;
  }

  return EFI_SUCCESS;
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
  EFI_DT_IO_PROTOCOL   *DtIo;

  DEBUG ((DEBUG_ERROR, "PCIe RootBridge constructor\n"));
  DtIo = NULL;

  Evan_Units_NO1 (&DtIo);
  PcieSTGInit(DtIo, PortIndex);
  RegWrite(mSysClkBase + SYS_CLK_NOC_OFFSET, 1 << 31);
  PcieClockInit(DtIo, PortIndex);
  PcieResetDeassert(DtIo, PortIndex);
  PcieGpioResetSet(DtIo, PortIndex, 0);
  PcieFuncSet(DtIo, PortIndex);
  
  PcieAtrInit(DtIo, PortIndex, PCIE_CFG_BASE[PortIndex],
     0, XR3_PCI_ECAM_SIZE, 1);
  PcieAtrInit(DtIo, PortIndex, PCI_MEMREGION_32[PortIndex],
     PCI_MEMREGION_32[PortIndex], PCI_MEMREGION_SIZE[0], 0);
  PcieAtrInit(DtIo, PortIndex, PCI_MEMREGION_64[PortIndex],
     PCI_MEMREGION_64[PortIndex], PCI_MEMREGION_SIZE[1], 0);
  PcieGpioResetSet(DtIo, PortIndex, 1);
  MicroSecondDelay(300);

  DEBUG ((DEBUG_ERROR, "PCIe port %d init\n", PortIndex));

  return EFI_SUCCESS;
}