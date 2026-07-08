/** @file
  USB Audio Class 1.0 control-request implementations.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/AudioOutput.h>
#include <IndustryStandard/UsbAudio.h>
#include "AudioRequests.h"

/**
  Set the sample rate of a UAC 1.0 isochronous endpoint (SET_CUR on the
  endpoint's SAMPLING_FREQ control, 3-byte little-endian payload).

  @param[in] UsbIo         UsbIo of any interface on the device.
  @param[in] EndpointAddr  Isochronous OUT endpoint address.
  @param[in] RateHz        Desired sample rate in Hz.

  @retval EFI_SUCCESS       Rate accepted by the device.
  @retval EFI_DEVICE_ERROR  The control transfer failed.
**/
EFI_STATUS
UsbAudio10SetSampleRate (
  IN EFI_USB_IO_PROTOCOL  *UsbIo,
  IN UINT8                EndpointAddr,
  IN UINT32               RateHz
  )
{
  EFI_USB_DEVICE_REQUEST  Request;
  EFI_STATUS              Status;
  UINT32                  UsbStatus;
  UINT8                   RateData[3];

  RateData[0] = (UINT8)(RateHz & 0xFF);
  RateData[1] = (UINT8)((RateHz >> 8) & 0xFF);
  RateData[2] = (UINT8)((RateHz >> 16) & 0xFF);

  Request.RequestType = USB_AUDIO_10_REQTYPE_SET_ENDPOINT;   /* 0x22: class, OUT, endpoint */
  Request.Request     = USB_AUDIO_10_REQ_SET_CUR;
  Request.Value       = (UINT16)(USB_AUDIO_10_EP_CS_SAMPLING_FREQ << 8);
  Request.Index       = EndpointAddr;
  Request.Length      = (UINT16)sizeof (RateData);

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataOut,
                    USB_AUDIO_CONTROL_TIMEOUT_MS,
                    RateData,
                    sizeof (RateData),
                    &UsbStatus
                    );
  if (EFI_ERROR (Status) || (UsbStatus != EFI_USB_NOERROR)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Read the mute state of channel via a UAC 1.0 GET_CUR request.

  @param[in]  UsbIo          UsbIo for the AC interface.
  @param[in]  AcInterface    AudioControl interface number.
  @param[in]  FeatureUnitId  Feature Unit entity ID.
  @param[in]  Channel        Channel number.
  @param[out] Mute           Current mute state.

  @retval EFI_SUCCESS       Mute state returned.
  @retval EFI_DEVICE_ERROR  The control transfer failed.
**/
EFI_STATUS
EFIAPI
UsbAudio10GetMute (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT8                AcInterface,
  IN  UINT8                FeatureUnitId,
  IN  UINT8                Channel,
  OUT BOOLEAN              *Mute
  )
{
  EFI_STATUS  Status;
  UINT8       Value;

  Value  = 0;
  Status = UsbAudioFeatureUnitRequest (
             UsbIo,
             USB_AUDIO_10_REQTYPE_GET_INTERFACE,
             USB_AUDIO_10_REQ_GET_CUR,
             USB_AUDIO_10_FU_CS_MUTE,
             AcInterface,
             FeatureUnitId,
             Channel,
             EfiUsbDataIn,
             &Value,
             sizeof (Value)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Mute = (BOOLEAN)(Value != 0);
  return EFI_SUCCESS;
}

/**
  Write the mute state of channel via a UAC 1.0 SET_CUR request.

  @param[in] UsbIo          UsbIo for the AC interface.
  @param[in] AcInterface    AudioControl interface number.
  @param[in] FeatureUnitId  Feature Unit entity ID.
  @param[in] Channel        Channel number.
  @param[in] Mute           Desired mute state.

  @retval EFI_SUCCESS       Mute state applied.
  @retval EFI_DEVICE_ERROR  The control transfer failed.
**/
EFI_STATUS
EFIAPI
UsbAudio10SetMute (
  IN EFI_USB_IO_PROTOCOL  *UsbIo,
  IN UINT8                AcInterface,
  IN UINT8                FeatureUnitId,
  IN UINT8                Channel,
  IN BOOLEAN              Mute
  )
{
  UINT8  Value;

  Value = (UINT8)(Mute ? 1 : 0);
  return UsbAudioFeatureUnitRequest (
           UsbIo,
           USB_AUDIO_10_REQTYPE_SET_INTERFACE,
           USB_AUDIO_10_REQ_SET_CUR,
           USB_AUDIO_10_FU_CS_MUTE,
           AcInterface,
           FeatureUnitId,
           Channel,
           EfiUsbDataOut,
           &Value,
           sizeof (Value)
           );
}

/**
  Read the volume of channel via a UAC 1.0 GET_CUR request.

  @param[in]  UsbIo          UsbIo for the AC interface.
  @param[in]  AcInterface    AudioControl interface number.
  @param[in]  FeatureUnitId  Feature Unit entity ID.
  @param[in]  Channel        Channel number.
  @param[out] Volume         Current volume in Q8.8 signed dB.

  @retval EFI_SUCCESS       Volume returned.
  @retval EFI_DEVICE_ERROR  The control transfer failed.
**/
EFI_STATUS
EFIAPI
UsbAudio10GetVolume (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT8                AcInterface,
  IN  UINT8                FeatureUnitId,
  IN  UINT8                Channel,
  OUT INT16                *Volume
  )
{
  EFI_STATUS  Status;
  INT16       Value;

  Value  = 0;
  Status = UsbAudioFeatureUnitRequest (
             UsbIo,
             USB_AUDIO_10_REQTYPE_GET_INTERFACE,
             USB_AUDIO_10_REQ_GET_CUR,
             USB_AUDIO_10_FU_CS_VOLUME,
             AcInterface,
             FeatureUnitId,
             Channel,
             EfiUsbDataIn,
             &Value,
             sizeof (Value)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Volume = Value;
  return EFI_SUCCESS;
}

/**
  Write the volume of channel via a UAC 1.0 SET_CUR request.

  @param[in] UsbIo          UsbIo for the AC interface.
  @param[in] AcInterface    AudioControl interface number.
  @param[in] FeatureUnitId  Feature Unit entity ID.
  @param[in] Channel        Channel number.
  @param[in] Volume         Desired volume in Q8.8 signed dB.

  @retval EFI_SUCCESS       Volume applied.
  @retval EFI_DEVICE_ERROR  The control transfer failed.
**/
EFI_STATUS
EFIAPI
UsbAudio10SetVolume (
  IN EFI_USB_IO_PROTOCOL  *UsbIo,
  IN UINT8                AcInterface,
  IN UINT8                FeatureUnitId,
  IN UINT8                Channel,
  IN INT16                Volume
  )
{
  return UsbAudioFeatureUnitRequest (
           UsbIo,
           USB_AUDIO_10_REQTYPE_SET_INTERFACE,
           USB_AUDIO_10_REQ_SET_CUR,
           USB_AUDIO_10_FU_CS_VOLUME,
           AcInterface,
           FeatureUnitId,
           Channel,
           EfiUsbDataOut,
           &Volume,
           sizeof (Volume)
           );
}

/**
  Read the volume range via UAC 1.0 GET_MIN / GET_MAX / GET_RES requests.

  @param[in]  UsbIo          UsbIo for the AC interface.
  @param[in]  AcInterface    AudioControl interface number.
  @param[in]  FeatureUnitId  Feature Unit entity ID.
  @param[in]  Channel        Channel number.
  @param[out] MinVolume      Minimum volume in Q8.8 signed dB.
  @param[out] MaxVolume      Maximum volume in Q8.8 signed dB.
  @param[out] Resolution     Step size in Q8.8 signed dB.

  @retval EFI_SUCCESS       Range returned.
  @retval EFI_DEVICE_ERROR  A control transfer failed.
**/
EFI_STATUS
EFIAPI
UsbAudio10GetVolumeRange (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT8                AcInterface,
  IN  UINT8                FeatureUnitId,
  IN  UINT8                Channel,
  OUT INT16                *MinVolume,
  OUT INT16                *MaxVolume,
  OUT INT16                *Resolution
  )
{
  EFI_STATUS  Status;

  Status = UsbAudioFeatureUnitRequest (
             UsbIo,
             USB_AUDIO_10_REQTYPE_GET_INTERFACE,
             USB_AUDIO_10_REQ_GET_MIN,
             USB_AUDIO_10_FU_CS_VOLUME,
             AcInterface,
             FeatureUnitId,
             Channel,
             EfiUsbDataIn,
             MinVolume,
             sizeof (*MinVolume)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UsbAudioFeatureUnitRequest (
             UsbIo,
             USB_AUDIO_10_REQTYPE_GET_INTERFACE,
             USB_AUDIO_10_REQ_GET_MAX,
             USB_AUDIO_10_FU_CS_VOLUME,
             AcInterface,
             FeatureUnitId,
             Channel,
             EfiUsbDataIn,
             MaxVolume,
             sizeof (*MaxVolume)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UsbAudioFeatureUnitRequest (
             UsbIo,
             USB_AUDIO_10_REQTYPE_GET_INTERFACE,
             USB_AUDIO_10_REQ_GET_RES,
             USB_AUDIO_10_FU_CS_VOLUME,
             AcInterface,
             FeatureUnitId,
             Channel,
             EfiUsbDataIn,
             Resolution,
             sizeof (*Resolution)
             );
  return Status;
}
