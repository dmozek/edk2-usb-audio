/** @file
  Private definitions for the USB Audio Class DXE driver.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiUsbLib.h>
#include <Protocol/DevicePath.h>

#include "UsbAudioTypes.h"

#define USB_AUDIO_DEV_SIGNATURE  SIGNATURE_32 ('U', 'A', 'u', 'd')

#define USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL(a) \
  CR (a, USB_AUDIO_DEV, AudioOutputProtocol, USB_AUDIO_DEV_SIGNATURE)

#define USB_AUDIO_DEV_FROM_CODEC_PROTOCOL(a) \
  CR (a, USB_AUDIO_DEV, AudioCodecProtocol, USB_AUDIO_DEV_SIGNATURE)

extern EFI_DRIVER_BINDING_PROTOCOL  gUsbAudioDriverBinding;

EFI_STATUS
EFIAPI
UsbAudioDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
UsbAudioDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
UsbAudioDriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  );

EFI_STATUS
EFIAPI
UsbAudioComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );

EFI_STATUS
EFIAPI
UsbAudioComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_HANDLE                   ChildHandle        OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **ControllerName
  );

//
// Protocol templates filled into USB_AUDIO_DEV.AudioOutputProtocol and
// USB_AUDIO_DEV.AudioCodecProtocol at Start time
// (defined in AudioOutputProtocol.c and AudioCodecProtocol.c).
//
extern EFI_AUDIO_OUTPUT_PROTOCOL  gAudioOutputProtocolTemplate;
extern EFI_AUDIO_CODEC_PROTOCOL   gAudioCodecProtocolTemplate;
