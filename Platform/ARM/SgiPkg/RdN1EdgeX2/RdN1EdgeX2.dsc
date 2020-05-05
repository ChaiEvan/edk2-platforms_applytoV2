#
#  Copyright (c) 2020, ARM Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = RdN1EdgeX2
  PLATFORM_GUID                  = a8ec2997-83ff-4347-8a9d-796d8c0c8b4d
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x0001001B
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = AARCH64|ARM
  BUILD_TARGETS                  = NOOPT|DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/ARM/SgiPkg/SgiPlatform.fdf
  BUILD_NUMBER                   = 1

# include common definitions from SgiPlatform.dsc
!include Platform/ARM/SgiPkg/SgiPlatform.dsc.inc

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsFixedAtBuild.common]
  # ARM Cores and Clusters
  gArmPlatformTokenSpaceGuid.PcdCoreCount|4
  gArmPlatformTokenSpaceGuid.PcdClusterCount|2

  # GIC Base Addresses
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x30000000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x300C0000
  gArmSgiTokenSpaceGuid.PcdGicSize|0x100000
