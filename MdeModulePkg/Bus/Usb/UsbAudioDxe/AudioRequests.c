/** @file
  Generic USB Audio control-request implementations.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/AudioOutput.h>
#include <Library/DebugLib.h>
#include <IndustryStandard/UsbAudio.h>
#include "AudioRequests.h"

/**
  Issue a standard SET_INTERFACE request selecting an alternate setting.

  @param[in] UsbIo            UsbIo of any interface on the device.
  @param[in] InterfaceNumber  Interface whose alt setting is selected.
  @param[in] AltSetting       Alternate setting index.

  @retval EFI_SUCCESS       Alt selected.
  @retval EFI_DEVICE_ERROR  The control transfer failed.
**/
EFI_STATUS
UsbAudioSetInterface (
  IN EFI_USB_IO_PROTOCOL  *UsbIo,
  IN UINT8                InterfaceNumber,
  IN UINT8                AltSetting
  )
{
  EFI_USB_DEVICE_REQUEST  Request;
  EFI_STATUS              Status;
  UINT32                  UsbStatus;

  Request.RequestType = 0x01;   /* standard, host-to-device, interface recipient */
  Request.Request     = USB_REQ_SET_INTERFACE;
  Request.Value       = AltSetting;
  Request.Index       = InterfaceNumber;
  Request.Length      = 0;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbNoData,
                    USB_AUDIO_CONTROL_TIMEOUT_MS,
                    NULL,
                    0,
                    &UsbStatus
                    );
  if (EFI_ERROR (Status) || (UsbStatus != EFI_USB_NOERROR)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Issue a UAC 1.0/2.0 Feature Unit control request on a given channel.

  @param[in]     UsbIo            UsbIo for the AC interface.
  @param[in]     RequestType      bmRequestType.
  @param[in]     Request          bRequest.
  @param[in]     ControlSelector  Feature Unit control selector (MUTE / VOLUME).
  @param[in]     AcInterface      AudioControl interface number.
  @param[in]     FeatureUnitId    Feature Unit entity ID.
  @param[in]     Channel          Channel number (0 = master).
  @param[in]     Direction        Data stage direction.
  @param[in,out] Data             Payload buffer.
  @param[in]     Length           Payload length in bytes.

  @retval EFI_SUCCESS       The control transfer completed.
  @retval EFI_DEVICE_ERROR  The control transfer failed.
**/
EFI_STATUS
UsbAudioFeatureUnitRequest (
  IN     EFI_USB_IO_PROTOCOL     *UsbIo,
  IN     UINT8                   RequestType,
  IN     UINT8                   Request,
  IN     UINT8                   ControlSelector,
  IN     UINT8                   AcInterface,
  IN     UINT8                   FeatureUnitId,
  IN     UINT8                   Channel,
  IN     EFI_USB_DATA_DIRECTION  Direction,
  IN OUT VOID                    *Data,
  IN     UINTN                   Length
  )
{
  EFI_USB_DEVICE_REQUEST  Req;
  EFI_STATUS              Status;
  UINT32                  UsbStatus;

  Req.RequestType = RequestType;
  Req.Request     = Request;
  Req.Value       = (UINT16)((ControlSelector << 8) | Channel);
  Req.Index       = (UINT16)((FeatureUnitId << 8) | AcInterface);
  Req.Length      = (UINT16)Length;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Req,
                    Direction,
                    USB_AUDIO_CONTROL_TIMEOUT_MS,
                    Data,
                    Length,
                    &UsbStatus
                    );
  if (EFI_ERROR (Status) || (UsbStatus != EFI_USB_NOERROR)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}
