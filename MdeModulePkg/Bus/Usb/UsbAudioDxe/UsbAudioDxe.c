/** @file
  USB Audio Class DXE driver - Driver Binding implementation.

  Supported() checks for a USB AudioControl interface with at least one
  Type I (PCM-family) AudioStreaming alternate setting.

  Start() opens the Audio Streaming interface and parses all supported formats.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbAudio.h"
#include "AudioDescriptors.h"
#include "AudioStream.h"
#include "AudioRequests.h"
#include "AudioFormats.h"

extern EFI_COMPONENT_NAME_PROTOCOL   gUsbAudioComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gUsbAudioComponentName2;

EFI_DRIVER_BINDING_PROTOCOL  gUsbAudioDriverBinding = {
  UsbAudioDriverBindingSupported,
  UsbAudioDriverBindingStart,
  UsbAudioDriverBindingStop,
  0x10,
  NULL,
  NULL
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

/**
  Driver entry point - installs Driver Binding and Component Name protocols.

  @param[in] ImageHandle  Handle for this driver image.
  @param[in] SystemTable  Pointer to the EFI System Table.

  @retval EFI_SUCCESS  Protocols installed.
**/
EFI_STATUS
EFIAPI
UsbAudioDriverBindingEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gUsbAudioDriverBinding,
             ImageHandle,
             &gUsbAudioComponentName,
             &gUsbAudioComponentName2
             );
  return Status;
}

/**
  Check whether the given interface descriptor is a UAC 1.0 or 2.0 AudioControl interface.

  @param[in] IfDesc  Interface descriptor to check.

  @retval TRUE   IfDesc is an AudioControl interface.
  @retval FALSE  IfDesc is not an AudioControl interface.
 **/
STATIC
BOOLEAN
UsbAudioInterfaceIsAudioControl (
  IN EFI_USB_INTERFACE_DESCRIPTOR  *IfDesc
  )
{
  return ((IfDesc->InterfaceClass    == USB_AUDIO_CLASS)           &&
          (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_CONTROL) &&
          ((IfDesc->InterfaceProtocol == USB_AUDIO_PROTOCOL_NONE) ||
           (IfDesc->InterfaceProtocol == USB_AUDIO_PROTOCOL_VERSION_02_00)));
}

/**
  Check whether this driver supports the given controller.

  Returns EFI_SUCCESS iff the controller is a USB AudioControl interface
  (class 0x01, subclass 0x01) with protocol 0x00 (UAC 1.0) or 0x20 (UAC 2.0)
  that exposes at least one Type I OUT AudioStreaming alternate setting.

  @param[in] This                   Driver Binding instance.
  @param[in] Controller             Handle to test.
  @param[in] RemainingDevicePath    Ignored.

  @retval EFI_SUCCESS       Controller is supported.
  @retval EFI_UNSUPPORTED   Controller is not supported.
**/
EFI_STATUS
EFIAPI
UsbAudioDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                    Status;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_INTERFACE_DESCRIPTOR  IfDesc;
  UINT8                         *Config;
  UINT16                        ConfigLength;
  UINT8                         AsInterface;
  UINT16                        Offset;
  UINT8                         AltSetting;

  Config = NULL;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &IfDesc);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  DEBUG ((
    DEBUG_INFO,
    "UsbAudioDxe: Supported() checking controller %p: class 0x%02x subclass 0x%02x protocol 0x%02x\n",
    Controller,
    IfDesc.InterfaceClass,
    IfDesc.InterfaceSubClass,
    IfDesc.InterfaceProtocol
    ));

  if (!UsbAudioInterfaceIsAudioControl (&IfDesc)) {
    DEBUG ((DEBUG_INFO, "UsbAudioDxe: Supported() -> not an AudioControl interface, rejected\n"));
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  //
  // Require at least one Type I (PCM-family) OUT AudioStreaming alt setting.
  // This rejects Type II / Type III-only devices.
  //
  Status = UsbAudioGetConfigDescriptor (UsbIo, &Config, &ConfigLength);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (!UsbAudioFindFirstTypeIStream (Config, ConfigLength, &AsInterface, &Offset, &AltSetting)) {
    DEBUG ((DEBUG_INFO, "UsbAudioDxe: Supported() -> no Type I OUT AudioStreaming alt, rejected\n"));
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  DEBUG ((
    DEBUG_INFO,
    "UsbAudioDxe: Supported() -> UAC %a device with Type I OUT stream on AS if %u, accepted\n",
    (IfDesc.InterfaceProtocol == USB_AUDIO_PROTOCOL_VERSION_02_00) ? "2.0" : "1.0",
    AsInterface
    ));
  Status = EFI_SUCCESS;

Done:
  if (Config != NULL) {
    FreePool (Config);
  }

  gBS->CloseProtocol (
         Controller,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );
  return Status;
}

/**
  Allocate and initialize a USB_AUDIO_DEV structure for the given controller.

  @param[in] This                  Driver Binding instance.
  @param[in] Controller            Handle to start.
  @param[in] UsbIo                 Opened USB I/O protocol for the controller.
  @param[in] Spec                  UAC version (1.0 or 2.0).
  @param[in] AcInterfaceNumber     AudioControl interface number.
  @param[out] DevOut               Pointer to receive allocated USB_AUDIO_DEV.

  @retval EFI_SUCCESS              DevOut allocated and initialized.
  @retval EFI_OUT_OF_RESOURCES     Memory allocation failed.
  @retval EFI_UNSUPPORTED          No Type I OUT alt was found.
  @retval other                    Resource or USB failure.
 **/
STATIC
EFI_STATUS
AllocateAndInitUsbAudioDev (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  EFI_USB_IO_PROTOCOL          *UsbIo,
  IN  USB_AUDIO_SPEC_VERSION       Spec,
  IN  UINT8                        AcInterfaceNumber,
  OUT USB_AUDIO_DEV                **DevOut
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  DevicePath = NULL;

  *DevOut = AllocateZeroPool (sizeof (USB_AUDIO_DEV));
  if (*DevOut == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DiscoverFormats (
             UsbIo,
             Spec,
             &(*DevOut)->SupportedFormatCount,
             &(*DevOut)->SupportedFormats
             );

  if (EFI_ERROR (Status)) {
    return Status;
  } else if ((*DevOut)->SupportedFormatCount == 0) {
    return EFI_UNSUPPORTED;
  }

  (*DevOut)->CurrentFormatIndex  = 0;
  (*DevOut)->Signature           = USB_AUDIO_DEV_SIGNATURE;
  (*DevOut)->ControllerHandle    = Controller;
  (*DevOut)->AcUsbIo             = UsbIo;
  (*DevOut)->SpecVersion         = Spec;
  (*DevOut)->AcInterfaceNumber   = AcInterfaceNumber;
  (*DevOut)->DriverBindingHandle = This->DriverBindingHandle;
  (*DevOut)->FeatureUnitId       = 0;
  (*DevOut)->VolumeControlMask   = 0;
  (*DevOut)->MuteControlMask     = 0;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath,
                  (*DevOut)->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  (*DevOut)->DevicePath = DevicePath;

  return Status;
}

/**
  Allocate and initialize the EFI_AUDIO_OUTPUT_PROTOCOL and EFI_AUDIO_CODEC_PROTOCOL
  structures in the given USB_AUDIO_DEV.

  @param[in,out] Dev  USB_AUDIO_DEV to initialize.

  @retval EFI_SUCCESS           Protocols initialized.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
 **/
STATIC
EFI_STATUS
SetupAudioProtocols (
  IN OUT USB_AUDIO_DEV  *Dev
  )
{
  CopyMem (&Dev->AudioOutputProtocol, &gAudioOutputProtocolTemplate, sizeof (EFI_AUDIO_OUTPUT_PROTOCOL));

  CopyMem (&Dev->AudioCodecProtocol, &gAudioCodecProtocolTemplate, sizeof (EFI_AUDIO_CODEC_PROTOCOL));

  Dev->Format = AllocateZeroPool (sizeof (EFI_AUDIO_CODEC_PROTOCOL_FORMAT));
  if (Dev->Format == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Dev->AudioCodecProtocol.CurrentFormat = Dev->Format;

  Dev->Format->MaxFormat  = (UINT32)Dev->SupportedFormatCount;
  Dev->Format->SizeOfInfo = sizeof (EFI_AUDIO_CODEC_FORMAT_INFORMATION);

  return EFI_SUCCESS;
}

/**
  Install the controller name strings for the given USB_AUDIO_DEV.

  @param[in,out] Dev  USB_AUDIO_DEV to install name strings for.
 **/
STATIC
VOID
InstallControllerNameStrings (
  IN OUT USB_AUDIO_DEV  *Dev
  )
{
  AddUnicodeString2 (
    "eng",
    gUsbAudioComponentName.SupportedLanguages,
    &Dev->ControllerNameTable,
    L"USB Audio Device",
    TRUE
    );
  AddUnicodeString2 (
    "en",
    gUsbAudioComponentName2.SupportedLanguages,
    &Dev->ControllerNameTable,
    L"USB Audio Device",
    FALSE
    );
}

/**
  Start managing the given USB Audio controller (UAC 1.0 and UAC 2.0).

  Parses the AudioControl entities, discovering all supported Type I (PCM-family) AudioStreaming formats.
  Opens the USB I/O protocol for the AudioControl and AudioStreaming interfaces
  and installs the EFI_AUDIO_OUTPUT_PROTOCOL and EFI_AUDIO_CODEC_PROTOCOL.

  @param[in] This                  Driver Binding instance.
  @param[in] Controller            Handle to start.
  @param[in] RemainingDevicePath   Ignored.

  @retval EFI_SUCCESS      Protocol installed and stream prepared.
  @retval EFI_UNSUPPORTED  No Type I OUT alt was found.
  @retval other            Resource or USB failure.
**/
EFI_STATUS
EFIAPI
UsbAudioDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                    Status;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_INTERFACE_DESCRIPTOR  IfDesc;
  USB_AUDIO_DEV                 *Dev;
  USB_AUDIO_SPEC_VERSION        Spec;

  Dev = NULL;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &IfDesc);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Spec = (IfDesc.InterfaceProtocol == USB_AUDIO_PROTOCOL_VERSION_02_00) ?
         UsbAudioSpec20 : UsbAudioSpec10;

  DEBUG ((
    DEBUG_INFO,
    "UsbAudioDxe: Start() on controller %p, AC if %u, UAC %a\n",
    Controller,
    IfDesc.InterfaceNumber,
    (Spec == UsbAudioSpec20) ? "2.0" : "1.0"
    ));

  Status = AllocateAndInitUsbAudioDev (
             This,
             Controller,
             UsbIo,
             Spec,
             IfDesc.InterfaceNumber,
             &Dev
             );

  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = SetupAudioProtocols (Dev);

  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  Status = UsbAudioSelectFormat (
             UsbIo,
             Dev->DriverBindingHandle,
             Dev->SupportedFormats,
             Dev->CurrentFormatIndex,
             Dev->SupportedFormats[Dev->CurrentFormatIndex].Info.MinSampleRateHz,
             Dev
             );

  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  //
  // Create timer event, but do not start timer yet
  // Timer is started when the first audio buffer is queued, and stopped when the last buffer completes
  //
  Dev->StreamTimer = NULL;
  Status           = gBS->CreateEvent (
                            EVT_TIMER | EVT_NOTIFY_SIGNAL,
                            TPL_CALLBACK,
                            ManageAudioStreams,
                            Dev,
                            &Dev->StreamTimer
                            );

  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  InstallControllerNameStrings (Dev);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Controller,
                  &gEdkiiAudioOutputProtocolGuid,
                  &Dev->AudioOutputProtocol,
                  &gEdkiiAudioCodecProtocolGuid,
                  &Dev->AudioCodecProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  return EFI_SUCCESS;

ErrorExit:
  if (Dev != NULL) {
    if (Dev->Stream.UsbIo != NULL) {
      UsbAudioSetInterface (Dev->Stream.UsbIo, Dev->Stream.InterfaceNumber, 0);
    }

    ResetStreams (&Dev->Stream);

    if (Dev->Stream.AsHandle != NULL) {
      gBS->CloseProtocol (
             Dev->Stream.AsHandle,
             &gEfiUsbIoProtocolGuid,
             Dev->DriverBindingHandle,
             Dev->ControllerHandle
             );
    }

    if (Dev->ControllerNameTable != NULL) {
      FreeUnicodeStringTable (Dev->ControllerNameTable);
    }

    if (Dev->StreamTimer != NULL) {
      gBS->CloseEvent (Dev->StreamTimer);
    }

    if ((Dev->SupportedFormatCount > 0) &&
        (Dev->SupportedFormats != NULL))
    {
      FreeFormatInfo (Dev->SupportedFormatCount, Dev->SupportedFormats);
    }

    if (Dev->Format != NULL) {
      FreePool (Dev->Format);
    }

    FreePool (Dev);
  }

  gBS->CloseProtocol (
         Controller,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  return Status;
}

/**
  Stop managing the given USB Audio controller.

  @param[in] This               Driver Binding instance.
  @param[in] Controller         Handle to stop.
  @param[in] NumberOfChildren   Number of child handles (unused).
  @param[in] ChildHandleBuffer  Child handle array (unused).

  @retval EFI_SUCCESS  Driver detached and resources released.
  @retval other        The protocol could not be uninstalled.
**/
EFI_STATUS
EFIAPI
UsbAudioDriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS                 Status;
  EFI_AUDIO_OUTPUT_PROTOCOL  *Protocol;
  USB_AUDIO_DEV              *Dev;
  BOOLEAN                    Drained;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEdkiiAudioOutputProtocolGuid,
                  (VOID **)&Protocol,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (Protocol);

  DEBUG ((DEBUG_INFO, "UsbAudioDxe: Stop() on controller %p\n", Controller));

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Controller,
                  &gEdkiiAudioOutputProtocolGuid,
                  &Dev->AudioOutputProtocol,
                  &gEdkiiAudioCodecProtocolGuid,
                  &Dev->AudioCodecProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Cancel any in-flight playback, return the AS interface to its idle
  // (zero-bandwidth) alt
  //
  Drained = StopAudio (Dev);

  UsbAudioSetInterface (Dev->Stream.UsbIo, Dev->Stream.InterfaceNumber, 0);

  if (Dev->Stream.AsHandle != NULL) {
    gBS->CloseProtocol (
           Dev->Stream.AsHandle,
           &gEfiUsbIoProtocolGuid,
           This->DriverBindingHandle,
           Dev->ControllerHandle
           );
  }

  if (Drained) {
    ResetStreams (&Dev->Stream);
  }

  if (Dev->ControllerNameTable != NULL) {
    FreeUnicodeStringTable (Dev->ControllerNameTable);
  }

  if ((Dev->SupportedFormatCount > 0) &&
      (Dev->SupportedFormats != NULL) &&
      Drained)
  {
    FreeFormatInfo (Dev->SupportedFormatCount, Dev->SupportedFormats);
  }

  if (Dev->Format != NULL) {
    FreePool (Dev->Format);
  }

  gBS->CloseProtocol (
         Controller,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         Controller
         );

  gBS->CloseEvent (Dev->StreamTimer);

  if (Drained) {
    FreePool (Dev);
  }

  return EFI_SUCCESS;
}
