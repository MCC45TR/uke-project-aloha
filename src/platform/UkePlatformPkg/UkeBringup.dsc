# SPDX-License-Identifier: MIT
# Component integration only; raw SEC/PrePi/DXE firmware is a separate target.
[Defines]
  PLATFORM_NAME = UkeBringup
  PLATFORM_GUID = 8f409f54-dc88-420d-a74d-8766432b90b1
  PLATFORM_VERSION = 0.1
  DSC_SPECIFICATION = 0x0001001B
  OUTPUT_DIRECTORY = build/mu-integration/Build
  SUPPORTED_ARCHITECTURES = AARCH64
  BUILD_TARGETS = DEBUG|RELEASE
  SKUID_IDENTIFIER = DEFAULT

[LibraryClasses]
  NULL|MdePkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf

[Components]
  src/platform/UkePlatformPkg/BootDispatcher/BootDispatcher.inf
  src/platform/UkePlatformPkg/Sec/UkeSecEntryLib.inf
  src/platform/UkePlatformPkg/UfsReadOnly/UfsReadOnly.inf
  src/platform/UkePlatformPkg/AndroidReturn/UkeAndroidReturnLib.inf
  tests/MuDispatchSmoke.inf

[BuildOptions]
  *_CLANGPDB_AARCH64_CC_FLAGS = -mgeneral-regs-only -mstrict-align
