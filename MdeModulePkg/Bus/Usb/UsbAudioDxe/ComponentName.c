/** @file
  UEFI Component Name and Component Name 2 protocol implementation for the
  USB Audio Class DXE driver.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbAudio.h"

GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL  gUsbAudioComponentName = {
  UsbAudioComponentNameGetDriverName,
  UsbAudioComponentNameGetControllerName,
  "eng"
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL  gUsbAudioComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME)UsbAudioComponentNameGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME)UsbAudioComponentNameGetControllerName,
  "en"
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE  mUsbAudioDriverNameTable[] = {
  { "eng;en", L"USB Audio Class DXE Driver" },
  { NULL,     NULL                          }
};

/**
  Return the user-readable driver name.

  @param[in]  This        Component Name (2) protocol instance.
  @param[in]  Language    RFC 4646 / ISO 639-2 language code.
  @param[out] DriverName  Pointer to the driver name string.

  @retval EFI_SUCCESS            DriverName returned.
  @retval EFI_INVALID_PARAMETER  Language or DriverName is NULL.
  @retval EFI_UNSUPPORTED        Language not supported.
**/
EFI_STATUS
EFIAPI
UsbAudioComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mUsbAudioDriverNameTable,
           DriverName,
           (BOOLEAN)(This == &gUsbAudioComponentName)
           );
}

/**
  Return the user-readable controller name.

  @param[in]  This             Component Name (2) protocol instance.
  @param[in]  ControllerHandle Handle of the managed controller.
  @param[in]  ChildHandle      Must be NULL (device driver).
  @param[in]  Language         RFC 4646 / ISO 639-2 language code.
  @param[out] ControllerName   Pointer to the controller name string.

  @retval EFI_SUCCESS            ControllerName returned.
  @retval EFI_INVALID_PARAMETER  A required parameter is NULL or invalid.
  @retval EFI_UNSUPPORTED        Not managing ControllerHandle, or language
                                 not supported.
**/
EFI_STATUS
EFIAPI
UsbAudioComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_HANDLE                   ChildHandle        OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **ControllerName
  )
{
  EFI_STATUS                 Status;
  EFI_USB_IO_PROTOCOL        *UsbIo;
  EFI_AUDIO_OUTPUT_PROTOCOL  *AudioProtocol;
  USB_AUDIO_DEV              *Dev;

  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  //
  // Verify that this driver is managing ControllerHandle.
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  gUsbAudioDriverBinding.DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (!EFI_ERROR (Status)) {
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiUsbIoProtocolGuid,
           gUsbAudioDriverBinding.DriverBindingHandle,
           ControllerHandle
           );
    return EFI_UNSUPPORTED;
  }

  if (Status != EFI_ALREADY_STARTED) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEdkiiAudioOutputProtocolGuid,
                  (VOID **)&AudioProtocol,
                  gUsbAudioDriverBinding.DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (AudioProtocol);

  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           Dev->ControllerNameTable,
           ControllerName,
           (BOOLEAN)(This == &gUsbAudioComponentName)
           );
}
