/** @file
 *
 * PCI Host Bridge Library instance for StarFive JH7110 SOC
 *
 * Copyright (c) 2023, Minda Chen <minda.chen@starfivetech.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <IndustryStandard/JH7110.h>
#include <IndustryStandard/Pci22.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <PiDxe.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/DtIo.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DxeServicesTableLib.h>

#pragma pack(1)

typedef PACKED struct {
  ACPI_HID_DEVICE_PATH     AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL EndDevicePath;
} EFI_PCI_ROOT_BRIDGE_DEVICE_PATH;

#pragma pack ()

STATIC CONST EFI_PCI_ROOT_BRIDGE_DEVICE_PATH mEfiPciRootBridgeDevicePath[] = {
  {
    {
      {
        ACPI_DEVICE_PATH,
        ACPI_DP,
        {
          (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
          (UINT8)(sizeof (ACPI_HID_DEVICE_PATH) >> 8)
        }
      },
      EISA_PNP_ID (0x0A08), // PCI Express
      0
    },

    {
      END_DEVICE_PATH_TYPE,
      END_ENTIRE_DEVICE_PATH_SUBTYPE,
      {
        END_DEVICE_PATH_LENGTH,
        0
      }
    }
  },
};

GLOBAL_REMOVE_IF_UNREFERENCED
CHAR16 *mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

// These should come from the PCD...
#define JH7110_PCI_SEG0_BUSNUM_MIN     0x00
#define JH7110_PCI_SEG0_BUSNUM_MAX     0xFF
#define JH7110_PCI_SEG0_PORTIO_MIN     0x01
#define JH7110_PCI_SEG0_PORTIO_MAX     0x00 // MIN>MAX disables PIO
#define JH7110_PCI_SEG0_PORTIO_OFFSET  0x00
// The bridge thinks its MMIO is here (which means it can't access this area in phy ram)
#define JH7110_PCI_SEG0_MMIO32_MIN     (0x38000000)
#define JH7110_PCI_SEG0_MMIO32_MAX     (JH7110_PCI_SEG0_MMIO32_MIN + 0x8000000)
// The CPU views it via a window here..
//#define BCM2711_PCI_SEG0_MMIO32_XLATE   (PCIE_CPU_MMIO_WINDOW - PCIE_TOP_OF_MEM_WIN)

// We might be able to size another region?
#define JH7110_PCI_SEG0_MMIO64_MIN     (0x980000000)
#define JH7110_PCI_SEG0_MMIO64_MAX     (0x9c0000000)

//
// See description in MdeModulePkg/Include/Library/PciHostBridgeLib.h
//
PCI_ROOT_BRIDGE mPciRootBridges[] = {
  {
    0,                                      // Segment
    0,                                      // Supports
    0,                                      // Attributes
    FALSE,                                  // DmaAbove4G
    FALSE,                                  // NoExtendedConfigSpace (true=256 byte config, false=4k)
    FALSE,                                  // ResourceAssigned
    EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM |
    EFI_PCI_HOST_BRIDGE_MEM64_DECODE,   // AllocationAttributes
    { JH7110_PCI_SEG0_BUSNUM_MIN,
      JH7110_PCI_SEG0_BUSNUM_MAX },        // Bus
    { JH7110_PCI_SEG0_PORTIO_MIN,
      JH7110_PCI_SEG0_PORTIO_MAX,
      MAX_UINT64 - JH7110_PCI_SEG0_PORTIO_OFFSET + 1 },   // Io
    { JH7110_PCI_SEG0_MMIO32_MIN,
      JH7110_PCI_SEG0_MMIO32_MAX, 0},// Mem
    {JH7110_PCI_SEG0_MMIO64_MIN, JH7110_PCI_SEG0_MMIO64_MAX, 0},     // MemAbove4G            
    { MAX_UINT64, 0x0 },                    // Pefetchable Mem
    { MAX_UINT64, 0x0 },                    // Pefetchable MemAbove4G
    (EFI_DEVICE_PATH_PROTOCOL *)&mEfiPciRootBridgeDevicePath[0]
  }
};

#define FDT_APIS_HOSTBRIDGE_ENABLE

#ifdef FDT_APIS_HOSTBRIDGE_ENABLE
UINTN                mHBCount = 0;

#define EFI_DT_PCI_HOST_RANGE_RELOCATABLE   BIT31
#define EFI_DT_PCI_HOST_RANGE_PREFETCHABLE  BIT30
#define EFI_DT_PCI_HOST_RANGE_ALIASED       BIT29
#define EFI_DT_PCI_HOST_RANGE_MMIO32        BIT25
#define EFI_DT_PCI_HOST_RANGE_IO            BIT24
#if 0
#define EFI_DT_PCI_HOST_RANGE_MMIO64        (EFI_DT_PCI_HOST_RANGE_MMIO32 \
  | EFI_DT_PCI_HOST_RANGE_IO)
#define EFI_DT_PCI_HOST_RANGE_TYPEMASK      (EFI_DT_PCI_HOST_RANGE_RELOCATABLE \
  | EFI_DT_PCI_HOST_RANGE_PREFETCHABLE | EFI_DT_PCI_HOST_RANGE_ALIASED \
  | EFI_DT_PCI_HOST_RANGE_MMIO32 | EFI_DT_PCI_HOST_RANGE_IO)
#else
#define EFI_DT_PCI_HOST_RANGE_MMIO64        (EFI_DT_PCI_HOST_RANGE_PREFETCHABLE \
  |EFI_DT_PCI_HOST_RANGE_MMIO32 | EFI_DT_PCI_HOST_RANGE_IO)
#define EFI_DT_PCI_HOST_RANGE_TYPEMASK      (EFI_DT_PCI_HOST_RANGE_PREFETCHABLE \
| EFI_DT_PCI_HOST_RANGE_ALIASED \
| EFI_DT_PCI_HOST_RANGE_MMIO32 | EFI_DT_PCI_HOST_RANGE_IO)
#endif

ACPI_HID_DEVICE_PATH mRootBridgeDeviceNodeTemplate = {
  {
    ACPI_DEVICE_PATH,
    ACPI_DP,
    {
      (UINT8) (sizeof (ACPI_HID_DEVICE_PATH)),
      (UINT8) ((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
    }
  },
  EISA_PNP_ID (0x0A03),
  0
};

PCI_ROOT_BRIDGE mRootBridgeTemplate = {
  0,
  0,                                    // Supports;
  0,                                    // Attributes;
  FALSE,                                // DmaAbove4G;
  FALSE,                                // NoExtendedConfigSpace;
  FALSE,                                // ResourceAssigned;
  EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM, // AllocationAttributes
  { 0, 255 },                           // Bus
  { 0, 0 },                             // Io - to be fixed later
  { 0, 0 },                             // Mem - to be fixed later
  { 0, 0 },                             // MemAbove4G - to be fixed later
  { 0, 0 },                             // PMem - COMBINE_MEM_PMEM indicating no PMem and PMemAbove4GB
  { 0, 0 },                             // PMemAbove4G
  NULL                                  // DevicePath;
};

/**
  Looks up a range property value by name for a EFI_DT_IO_PROTOCOL instance.
  Caller has the responsibility to free Range buffer after calling.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Property to look up.
  @param  Index                 Index of the reg value to return.
  @param  Range                 Pointer to the EFI_DT_RANGE array.
  @param  Len                   The size of EFI_DT_RANGE array.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Device Tree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_UNSUPPORTED       Not supported DtIo Typpe.

**/
EFI_STATUS
EFIAPI
DtIoGetRange (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  IN OUT VOID             **Range,
  OUT UINTN               *Len
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  UINTN            Dtvaltype;

  Dtvaltype = 0;
  *Len      = 0;

  if (This == NULL || Name == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: invalid parameter!! \n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Status = This->GetProp (This, Name, &Property);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetProp:: Status=[%r]. \n", __func__, Status));
    return Status;
  }

  *Len = Property.End - Property.Iter; //ccydbg
  DEBUG ((DEBUG_ERROR, "%a: ParseProp:: Len=[%x]. \n", __func__, *Len)); //ccydbg

  // generally:address-cells=<3>, size-cells = <2>;
  // (*Len) == Dt data occupied. 4+4*2+4*2+4*2=28
  //(*Len * 2) >= EFI_DT_RANGE2 takes: 4+4*4+4*4+4*4=52
  if (*Len > 0) {
    *Range = AllocateZeroPool (*Len * 4);  // here will *4 in case address-cells=<2>, size-cells = <1>;
                                          // (*Len * 4) >= EFI_DT_RANGE2 takes: 4+4*4+4*4+4*4=52 
    if (*Range == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: failed to allocate memory!! \n", __func__));
      return EFI_OUT_OF_RESOURCES;
    }
  } else {
    return EFI_NOT_FOUND;
  }

  if (AsciiStrCmp (Name, "reg") == 0) {
    Dtvaltype = EFI_DT_VALUE_REG;
  } else if (AsciiStrCmp (Name, "ranges") == 0) {
    Dtvaltype = EFI_DT_VALUE_RANGE;
  } else if (AsciiStrCmp (Name, "dma-ranges") == 0) {
    Dtvaltype = EFI_DT_VALUE_RANGE;
  } else {
    return EFI_UNSUPPORTED;
  }

  Status = This->ParseProp (
             This,
             &Property,
             Dtvaltype,
             Index,
             *Range
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: warning - ParseProp:: Status=[%r]. \n", __func__, Status));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Looks up a bus-range property value by name for a EFI_DT_IO_PROTOCOL instance.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Index                 Index of the reg value to return.
  @param  BusRange              Pointer to the 'bus-range' property

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Device Tree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetBusRange (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               Index,
  OUT EFI_DT_BUS_RANGE    *BusRange
  )
{
  EFI_STATUS        Status;
  EFI_DT_PROPERTY   Prop;

  if (This == NULL || BusRange == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: invalid parameter!! \n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Status = This->GetProp (This, "bus-range", &Prop);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: GetProp:: Status=[%r]. \n", __func__, Status));
    return Status;
  }

  CopyMem (BusRange, (VOID *) Prop.Iter, sizeof (EFI_DT_BUS_RANGE));

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MapGcdMmioSpace (
  IN    UINT64  Base,
  IN    UINT64  Size
  )
{
  EFI_STATUS  Status;

  Status = gDS->AddMemorySpace (
                  EfiGcdMemoryTypeMemoryMappedIo,
                  Base,
                  Size,
                  EFI_MEMORY_UC
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to add GCD memory space for region [0x%Lx+0x%Lx)\n",
      __FUNCTION__,
      Base,
      Size
      ));
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (Base, Size, EFI_MEMORY_UC);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set memory space attributes for region [0x%Lx+0x%Lx)\n",
      __FUNCTION__,
      Base,
      Size
      ));
  }

  return Status;
}

STATIC
EFI_STATUS
ProcessPciHost (
  OUT  UINT64  *IoBase,
  OUT  UINT64  *IoSize,
  OUT  UINT64  *Mmio32Base,
  OUT  UINT64  *Mmio32Size,
  OUT  UINT64  *Mmio64Base,
  OUT  UINT64  *Mmio64Size,
  OUT  UINT32  *BusMin,
  OUT  UINT32  *BusMax
  )
{
  UINT64               ConfigBase, ConfigSize;
  UINTN                Len;
  UINTN                Index;
  EFI_STATUS           Status;
  EFI_DT_IO_PROTOCOL   *DtIo;
  UINTN                HandleCount;
  EFI_HANDLE           *HandleBuffer;
  UINT64               IoTranslation;
  UINT64               Mmio32Translation;
  UINT64               Mmio64Translation;
  EFI_DT_RANGE         *Range;
  UINTN                Index2;
  EFI_DT_BUS_RANGE     BusRange;
  EFI_DT_REG           Reg;

  Range   = NULL;
  Len     = 0;

  //
  // The following output arguments are initialized only in
  // order to suppress '-Werror=maybe-uninitialized' warnings
  // *incorrectly* emitted by some gcc versions.
  //
  *IoBase     = 0;
  *Mmio32Base = 0;
  *Mmio64Base = MAX_UINT64;
  *BusMin     = 0;
  *BusMax     = 0;

  //
  // *IoSize, *Mmio##Size and IoTranslation are initialized to zero because the
  // logic below requires it. However, since they are also affected by the issue
  // reported above, they are initialized early.
  //
  *IoSize       = 0;
  *Mmio32Size   = 0;
  *Mmio64Size   = 0;
  IoTranslation = 0;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiDtIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiDtIoProtocolGuid, (VOID **)&DtIo);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: HandleProtocol: %r\n",
        __func__,
        Status
        ));
      continue;
    } else {
        if (!(DtIo->IsCompatible (DtIo, "starfive,jh7110-pcie1"))) {
        //
        // found the node or nodes
        //
        Status = DtIoGetRange (DtIo, "ranges", 0, (VOID **)&Range, &Len);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: DtIoGetRange is failed. \n",
            __func__
            ));
        }

        //for (Index2 = 0; Index2 < Len / sizeof (EFI_DT_RANGE2); Index2++) {
        for (Index2 = 0; Index2 < 2; Index2++) {
          switch (Range[Index2].Type & EFI_DT_PCI_HOST_RANGE_TYPEMASK) {
            case EFI_DT_PCI_HOST_RANGE_IO:
              *IoBase       = Range[Index2].ChildBase;
              *IoSize       = Range[Index2].Size;
              IoTranslation = Range[Index2].ParentBase - *IoBase;
              break;

            case EFI_DT_PCI_HOST_RANGE_MMIO32:
              *Mmio32Base       = Range[Index2].ChildBase;
              *Mmio32Size       = Range[Index2].Size;
              Mmio32Translation = Range[Index2].ParentBase - *Mmio32Base;

              if ((*Mmio32Base > MAX_UINT32) || (*Mmio32Size > MAX_UINT32) ||
                  (*Mmio32Base + *Mmio32Size > SIZE_4GB)) {
                DEBUG ((DEBUG_ERROR, "%a: MMIO32 space invalid\n", __FUNCTION__));
                break;
              }

              if (Mmio32Translation != 0) {
                DEBUG ((
                  DEBUG_ERROR,
                  "%a: unsupported nonzero MMIO32 translation "
                  "0x%Lx\n",
                  __FUNCTION__,
                  Mmio32Translation
                  ));
              }
              break;

            case EFI_DT_PCI_HOST_RANGE_MMIO64:
              *Mmio64Base       = Range[Index2].ChildBase;
              *Mmio64Size       = Range[Index2].Size;
              Mmio64Translation = Range[Index2].ParentBase - *Mmio64Base;

              if (Mmio64Translation != 0) {
                DEBUG ((
                  DEBUG_ERROR,
                  "%a: unsupported nonzero MMIO64 translation "
                  "0x%Lx\n",
                  __FUNCTION__,
                  Mmio64Translation
                  ));
              }
              break;

            default:
              DEBUG ((
                DEBUG_ERROR,
                "%a: Unknow type is detected. \n",
                __func__
                ));
              break;
          } // The end of switch

        } // then end of for-loop

        if (*Mmio32Size == 0) {
          DEBUG ((DEBUG_ERROR, "%a: MMIO32 space empty\n", __FUNCTION__));
          continue;
        }

        //
        // Locate 'Reg'
        //
        Status = DtIo->GetReg (DtIo, 1, &Reg);  // offset 0-->1
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: GetReg is failed. \n",
            __func__
            ));
        }

        //
        // Fetch the ECAM window.
        //
        ConfigBase = Reg.Base;
        ConfigSize = Reg.Length;
        DEBUG ((DEBUG_ERROR, "%a: GetReg:: ConfigBase=[%lx], ConfigSize=[%lx]. \n", __func__, ConfigBase, ConfigSize));

        //
        // The dynamic PCD PcdPciExpressBaseAddress should have already been set,
        // and should match the value we found in the DT node.
        //
        //ASSERT (PcdGet64 (PcdPciExpressBaseAddress) == ConfigBase);

        //
        // Locate 'bus-range'
        //
        Status = DtIoGetBusRange (DtIo, 0, &BusRange);
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: DtIoGetRange is failed. \n",
            __func__
            ));
        }

        *BusMin = SwapBytes32(BusRange.BusMin);
        *BusMax = SwapBytes32(BusRange.BusMax);

        DEBUG ((
          DEBUG_INFO,
          "%a: Config[0x%Lx+0x%Lx) Bus[0x%x..0x%x] "
          "Io[0x%Lx+0x%Lx)@0x%Lx Mem32[0x%Lx+0x%Lx)@0x0 Mem64[0x%Lx+0x%Lx)@0x0\n",
          __FUNCTION__,
          ConfigBase,
          ConfigSize,
          *BusMin,
          *BusMax,
          *IoBase,
          *IoSize,
          IoTranslation,
          *Mmio32Base,
          *Mmio32Size,
          *Mmio64Base,
          *Mmio64Size
          ));

        //
        // Map the ECAM space in the GCD memory map
        //
        Status = MapGcdMmioSpace (ConfigBase, ConfigSize);
        ASSERT_EFI_ERROR (Status);

        if (*IoSize != 0) {
          //
          // Map the MMIO window that provides I/O access - the PCI host bridge code
          // is not aware of this translation and so it will only map the I/O view
          // in the GCD I/O map.
          //
          Status = MapGcdMmioSpace (*IoBase + IoTranslation, *IoSize);
          ASSERT_EFI_ERROR (Status);
        }

        mHBCount++;
      } //if (!(DtIo->IsCompatible (DtIo, "pci-host-ecam-generic")))
    }
  }

  if (Range != NULL) {
    FreePool (Range);
  }

  return Status;
}
#endif  //FDT_APIS_HOSTBRIDGE_ENABLE

/**
  Return all the root bridge instances in an array.

  @param Count  Return the count of root bridge instances.

  @return All the root bridge instances in an array.
          The array should be passed into PciHostBridgeFreeRootBridges()
          when it's not used.
**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  OUT UINTN     *Count
  )
{
#ifdef FDT_APIS_HOSTBRIDGE_ENABLE
  EFI_STATUS                Status;
  UINT64                    IoBase;
  UINT64                    IoSize;
  UINT64                    Mmio32Base;
  UINT64                    Mmio32Size;
  UINT64                    Mmio64Base;
  UINT64                    Mmio64Size;
  UINT32                    BusMin;
  UINT32                    BusMax;
  UINT64                    AllocationAttributes;
  PCI_ROOT_BRIDGE_APERTURE  Io;
  PCI_ROOT_BRIDGE_APERTURE  Mem;
  PCI_ROOT_BRIDGE_APERTURE  MemAbove4G;
  PCI_ROOT_BRIDGE_APERTURE  PMem;
  PCI_ROOT_BRIDGE_APERTURE  PMemAbove4G;
  PCI_ROOT_BRIDGE           *Bridges;
  UINTN                     Index;

  *Count = 0;

  Status = ProcessPciHost (
             &IoBase,
             &IoSize,
             &Mmio32Base,
             &Mmio32Size,
             &Mmio64Base,
             &Mmio64Size,
             &BusMin,
             &BusMax
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to discover PCI host bridge: %r\n",
      __FUNCTION__,
      Status
      ));
    return NULL;
  }

  if (mHBCount == 0) {
    DEBUG ((DEBUG_INFO, "%a: PCI host bridge not present\n", __FUNCTION__));
    return NULL;
  }

  ZeroMem (&Io, sizeof (Io));
  ZeroMem (&Mem, sizeof (Mem));
  ZeroMem (&MemAbove4G, sizeof (MemAbove4G));
  ZeroMem (&PMem, sizeof (PMem));
  ZeroMem (&PMemAbove4G, sizeof (PMemAbove4G));

  AllocationAttributes = EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;

  if (IoSize != 0) {
    Io.Base  = IoBase;
    Io.Limit = IoBase + IoSize - 1;
  } else {
    Io.Base  = MAX_UINT64;
    Io.Limit = 0;
  }

  Mem.Base  = Mmio32Base;
  Mem.Limit = Mmio32Base + Mmio32Size - 1;

  if ((sizeof (UINTN) == sizeof (UINT64)) && (Mmio64Size != 0)) {
    MemAbove4G.Base       = Mmio64Base;
    MemAbove4G.Limit      = Mmio64Base + Mmio64Size - 1;
    AllocationAttributes |= EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
  } else {
    //
    // UEFI mandates a 1:1 virtual-to-physical mapping, so on a 32-bit
    // architecture such as ARM, we will not be able to access 64-bit MMIO
    // BARs unless they are allocated below 4 GB. So ignore the range above
    // 4 GB in this case.
    //
    MemAbove4G.Base  = MAX_UINT64;
    MemAbove4G.Limit = 0;
  }

  //
  // No separate ranges for prefetchable and non-prefetchable BARs
  //
  PMem.Base         = MAX_UINT64;
  PMem.Limit        = 0;
  PMemAbove4G.Base  = MAX_UINT64;
  PMemAbove4G.Limit = 0;

  Bridges = AllocatePool (sizeof (PCI_ROOT_BRIDGE) * mHBCount);
  if (Bridges == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: %r\n", __FUNCTION__, EFI_OUT_OF_RESOURCES));
    return NULL;
  }

  mRootBridgeTemplate.Supports              = PcdGet64 (PcdSupportedAttributes);
  mRootBridgeTemplate.Attributes            = PcdGet64 (PcdInitialAttributes);
  mRootBridgeTemplate.DmaAbove4G            = PcdGetBool (PcdDmaAbove4G);
  mRootBridgeTemplate.AllocationAttributes  = PcdGet64 (PcdAllocationAttributes);
  mRootBridgeTemplate.Bus.Base              = BusMin;
  mRootBridgeTemplate.Bus.Limit             = BusMax;
  mRootBridgeTemplate.NoExtendedConfigSpace = PcdGetBool (PcdNoExtendedConfigSpace);
  mRootBridgeTemplate.ResourceAssigned      = PcdGetBool (PcdResourceAssigned);

  CopyMem (&mRootBridgeTemplate.Io, &Io, sizeof (Io));
  CopyMem (&mRootBridgeTemplate.Mem, &Mem, sizeof (Mem));
  CopyMem (&mRootBridgeTemplate.MemAbove4G, &MemAbove4G, sizeof (MemAbove4G));
  CopyMem (&mRootBridgeTemplate.PMem, &PMem, sizeof (PMem));
  CopyMem (&mRootBridgeTemplate.PMemAbove4G, &PMemAbove4G, sizeof (PMemAbove4G));

  for (Index = 0; Index < mHBCount; Index ++) {
    mRootBridgeDeviceNodeTemplate.UID = Index;
    mRootBridgeTemplate.Segment = Index;
    mRootBridgeTemplate.DevicePath = AppendDevicePathNode (NULL, &mEfiPciRootBridgeDevicePath[Index].AcpiDevicePath.Header);

    CopyMem (Bridges + Index, &mRootBridgeTemplate, sizeof (PCI_ROOT_BRIDGE));
  }

  *Count = mHBCount;

  return Bridges;
#else
  *Count = ARRAY_SIZE (mPciRootBridges);
  return mPciRootBridges;
#endif
}

/**
  Free the root bridge instances array returned from PciHostBridgeGetRootBridges().

  @param Bridges The root bridge instances array.
  @param Count   The count of the array.
**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  PCI_ROOT_BRIDGE *Bridges,
  UINTN           Count
  )
{
}

/**
  Inform the platform that the resource conflict happens.

  @param HostBridgeHandle Handle of the Host Bridge.
  @param Configuration    Pointer to PCI I/O and PCI memory resource
                          descriptors. The Configuration contains the resources
                          for all the root bridges. The resource for each root
                          bridge is terminated with END descriptor and an
                          additional END is appended indicating the end of the
                          entire resources. The resource descriptor field
                          values follow the description in
                          EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
                          .SubmitResources().
**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
  EFI_HANDLE                        HostBridgeHandle,
  VOID                              *Configuration
  )
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Descriptor;
  UINTN                             RootBridgeIndex;
  DEBUG ((DEBUG_ERROR, "PciHostBridge: Resource conflict happens!\n"));

  RootBridgeIndex = 0;
  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Configuration;
  while (Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    DEBUG ((DEBUG_ERROR, "RootBridge[%d]:\n", RootBridgeIndex++));
    for (; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
      ASSERT (Descriptor->ResType <
              ARRAY_SIZE (mPciHostBridgeLibAcpiAddressSpaceTypeStr));
      DEBUG ((DEBUG_ERROR, " %s: Length/Alignment = 0x%lx / 0x%lx\n",
              mPciHostBridgeLibAcpiAddressSpaceTypeStr[Descriptor->ResType],
              Descriptor->AddrLen, Descriptor->AddrRangeMax
              ));
      if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
        DEBUG ((DEBUG_ERROR, "     Granularity/SpecificFlag = %ld / %02x%s\n",
                Descriptor->AddrSpaceGranularity, Descriptor->SpecificFlag,
                ((Descriptor->SpecificFlag &
                  EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE
                  ) != 0) ? L" (Prefetchable)" : L""
                ));
      }
    }
    //
    // Skip the END descriptor for root bridge
    //
    ASSERT (Descriptor->Desc == ACPI_END_TAG_DESCRIPTOR);
    Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(
                   (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor + 1
                   );
  }
}
