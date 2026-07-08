/** @file
  USB Audio Class 2.0 control-request implementations.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/AudioOutput.h>
#include <IndustryStandard/UsbAudio.h>
#include "AudioRequests.h"

//
// Maximum CS_SAM_FREQ_CONTROL sub-ranges we will parse from a RANGE reply.
//
#define USB_AUDIO_CLOCK_RANGE_MAX  16u

/**
  Set the sample rate of a UAC 2.0 Clock Source entity (CUR request on
  CS_SAM_FREQ_CONTROL, 4-byte little-endian payload).

  @param[in] UsbIo          UsbIo of any interface on the device.
  @param[in] AcInterface    AudioControl interface number.
  @param[in] ClockSourceId  Clock Source entity ID.
  @param[in] RateHz         Desired sample rate in Hz.

  @retval EFI_SUCCESS       Rate accepted by the device.
  @retval EFI_DEVICE_ERROR  The control transfer failed.
**/
EFI_STATUS
UsbAudio20SetSampleRate (
  IN EFI_USB_IO_PROTOCOL  *UsbIo,
  IN UINT8                AcInterface,
  IN UINT8                ClockSourceId,
  IN UINT32               RateHz
  )
{
  EFI_USB_DEVICE_REQUEST  Request;
  EFI_STATUS              Status;
  UINT32                  UsbStatus;

  Request.RequestType = USB_AUDIO_20_REQTYPE_SET_INTERFACE;   /* 0x21: class, OUT, interface */
  Request.Request     = USB_AUDIO_20_REQ_CUR;
  Request.Value       = (UINT16)(USB_AUDIO_20_CS_SAM_FREQ_CONTROL << 8);
  Request.Index       = (UINT16)((ClockSourceId << 8) | AcInterface);
  Request.Length      = (UINT16)sizeof (RateHz);

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataOut,
                    USB_AUDIO_CONTROL_TIMEOUT_MS,
                    &RateHz,
                    sizeof (RateHz),
                    &UsbStatus
                    );
  if (EFI_ERROR (Status) || (UsbStatus != EFI_USB_NOERROR)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Parse a UAC 2.0 Clock Source CS_SAM_FREQ_CONTROL RANGE reply.

  @param[in]  Reply      Raw reply from the device.
  @param[out] Subranges  Newly allocated array of sub-ranges.
  @param[out] NumRanges  Number of sub-ranges in the array.

  @retval EFI_SUCCESS           Subranges and NumRanges are valid.
  @retval EFI_DEVICE_ERROR      The reply contained no ranges.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
 **/
STATIC
EFI_STATUS
ParseClockRangeReply (
  IN  UINT8                               *Reply,
  OUT USB_AUDIO_20_RANGE_UINT32_SUBRANGE  **Subranges,
  OUT UINT16                              *NumRanges
  )
{
  UINT16  Count;

  Count = (UINT16)(Reply[0] | (Reply[1] << 8));
  if (Count == 0) {
    return EFI_DEVICE_ERROR;
  }

  if (Count > USB_AUDIO_CLOCK_RANGE_MAX) {
    Count = USB_AUDIO_CLOCK_RANGE_MAX;
  }

  *Subranges = AllocateCopyPool (Count * sizeof (**Subranges), Reply + 2);
  if (*Subranges == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *NumRanges = Count;
  return EFI_SUCCESS;
}

/**
  Query the UAC 2.0 Clock Source specified in ClockSourceEntry for its supported sample rates.

  @param[in]      UsbIo             UsbIo of any interface on the device.
  @param[in]      AcInterface       AudioControl interface number.
  @param[in,out]  ClockSourceEntry  Clock Source entry, must be freed by the caller.

  @retval EFI_SUCCESS           ClockSourceEntry was updated.
  @retval EFI_DEVICE_ERROR      The control transfer failed or returned no ranges.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
**/
EFI_STATUS
UsbAudio20GetClockRange (
  IN  EFI_USB_IO_PROTOCOL                 *UsbIo,
  IN  UINT8                               AcInterface,
  IN OUT USB_AUDIO_20_CLOCK_SOURCE_ENTRY  *ClockSourceEntry
  )
{
  EFI_USB_DEVICE_REQUEST  Request;
  EFI_STATUS              Status;
  UINT32                  UsbStatus;
  UINT8                   Reply[2 + USB_AUDIO_CLOCK_RANGE_MAX * sizeof (USB_AUDIO_20_RANGE_UINT32_SUBRANGE)];
  UINT32                  CurRate;
  UINT16                  Count;

  Request.RequestType = USB_AUDIO_20_REQTYPE_GET_INTERFACE; /* 0xA1: class, IN, interface */
  Request.Value       = (UINT16)(USB_AUDIO_20_CS_SAM_FREQ_CONTROL << 8);
  Request.Index       = (UINT16)((ClockSourceEntry->Id << 8) | AcInterface);

  if ((ClockSourceEntry->Controls & USB_AUDIO_20_CS_FREQ_CONTROL_MASK) != 0) {
    ZeroMem (Reply, sizeof (Reply));

    Request.Request = USB_AUDIO_20_REQ_RANGE;
    Request.Length  = (UINT16)sizeof (Count);

    Status = UsbIo->UsbControlTransfer (
                      UsbIo,
                      &Request,
                      EfiUsbDataIn,
                      USB_AUDIO_CONTROL_TIMEOUT_MS,
                      Reply,
                      sizeof (Count),
                      &UsbStatus
                      );
    if (EFI_ERROR (Status) || (UsbStatus != EFI_USB_NOERROR)) {
      return EFI_DEVICE_ERROR;
    }

    Count = (UINT16)(Reply[0] | (Reply[1] << 8));
    if (Count == 0) {
      return EFI_DEVICE_ERROR;
    }

    if (Count > USB_AUDIO_CLOCK_RANGE_MAX) {
      Count = USB_AUDIO_CLOCK_RANGE_MAX;
    }

    Request.Length = (UINT16)(2 + Count * sizeof (USB_AUDIO_20_RANGE_UINT32_SUBRANGE));

    Status = UsbIo->UsbControlTransfer (
                      UsbIo,
                      &Request,
                      EfiUsbDataIn,
                      USB_AUDIO_CONTROL_TIMEOUT_MS,
                      Reply,
                      Request.Length,
                      &UsbStatus
                      );
    if (EFI_ERROR (Status) || (UsbStatus != EFI_USB_NOERROR)) {
      return EFI_DEVICE_ERROR;
    }

    Status = ParseClockRangeReply (Reply, &ClockSourceEntry->Subranges, &ClockSourceEntry->NumRanges);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else {
    Request.Request = USB_AUDIO_20_REQ_CUR;
    Request.Length  = (UINT16)sizeof (CurRate);

    Status = UsbIo->UsbControlTransfer (
                      UsbIo,
                      &Request,
                      EfiUsbDataIn,
                      USB_AUDIO_CONTROL_TIMEOUT_MS,
                      &CurRate,
                      sizeof (CurRate),
                      &UsbStatus
                      );
    if (EFI_ERROR (Status) || (UsbStatus != EFI_USB_NOERROR)) {
      return EFI_DEVICE_ERROR;
    }

    ClockSourceEntry->Subranges = AllocatePool (sizeof (USB_AUDIO_20_RANGE_UINT32_SUBRANGE));
    if (ClockSourceEntry->Subranges == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    ClockSourceEntry->Subranges[0].Min = CurRate;
    ClockSourceEntry->Subranges[0].Max = CurRate;
    ClockSourceEntry->Subranges[0].Res = 0;
    ClockSourceEntry->NumRanges        = 1;
  }

  return EFI_SUCCESS;
}

/**
  Read the mute state of channel via a UAC 2.0 CUR request.

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
UsbAudio20GetMute (
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
             USB_AUDIO_20_REQTYPE_GET_INTERFACE,
             USB_AUDIO_20_REQ_CUR,
             USB_AUDIO_20_FU_CS_MUTE,
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
  Write the mute state of channel via a UAC 2.0 CUR request.

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
UsbAudio20SetMute (
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
           USB_AUDIO_20_REQTYPE_SET_INTERFACE,
           USB_AUDIO_20_REQ_CUR,
           USB_AUDIO_20_FU_CS_MUTE,
           AcInterface,
           FeatureUnitId,
           Channel,
           EfiUsbDataOut,
           &Value,
           sizeof (Value)
           );
}

/**
  Read the volume of channel via a UAC 2.0 CUR request.

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
UsbAudio20GetVolume (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN  UINT8                AcInterface,
  IN  UINT8                FeatureUnitId,
  IN  UINT8                Channel,
  OUT INT16                *Volume
  )
{
  EFI_STATUS  Status;
  UINT8       Reply[2];

  Reply[0] = 0;
  Reply[1] = 0;
  Status   = UsbAudioFeatureUnitRequest (
               UsbIo,
               USB_AUDIO_20_REQTYPE_GET_INTERFACE,
               USB_AUDIO_20_REQ_CUR,
               USB_AUDIO_20_FU_CS_VOLUME,
               AcInterface,
               FeatureUnitId,
               Channel,
               EfiUsbDataIn,
               Reply,
               sizeof (Reply)
               );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *Volume = (INT16)(Reply[0] | (Reply[1] << 8));
  return EFI_SUCCESS;
}

/**
  Write the volume of channel via a UAC 2.0 CUR request.

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
UsbAudio20SetVolume (
  IN EFI_USB_IO_PROTOCOL  *UsbIo,
  IN UINT8                AcInterface,
  IN UINT8                FeatureUnitId,
  IN UINT8                Channel,
  IN INT16                Volume
  )
{
  return UsbAudioFeatureUnitRequest (
           UsbIo,
           USB_AUDIO_20_REQTYPE_SET_INTERFACE,
           USB_AUDIO_20_REQ_CUR,
           USB_AUDIO_20_FU_CS_VOLUME,
           AcInterface,
           FeatureUnitId,
           Channel,
           EfiUsbDataOut,
           &Volume,
           sizeof (Volume)
           );
}

/**
  Read the volume range via a UAC 2.0 RANGE request.

  The RANGE reply is wNumSubRanges (UINT16) followed by sub-ranges of
  {wMIN, wMAX, wRES} (INT16 each). The first sub-range is returned.

  @param[in]  UsbIo          UsbIo for the AC interface.
  @param[in]  AcInterface    AudioControl interface number.
  @param[in]  FeatureUnitId  Feature Unit entity ID.
  @param[in]  Channel        Channel number.
  @param[out] MinVolume      Minimum volume in Q8.8 signed dB.
  @param[out] MaxVolume      Maximum volume in Q8.8 signed dB.
  @param[out] Resolution     Step size in Q8.8 signed dB.

  @retval EFI_SUCCESS       Range returned.
  @retval EFI_DEVICE_ERROR  The control transfer failed or returned no ranges.
**/
EFI_STATUS
EFIAPI
UsbAudio20GetVolumeRange (
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
  UINT8       Reply[2 + 3 * sizeof (INT16)];   /* wNumSubRanges + first sub-range */
  UINT16      NumRanges;

  ZeroMem (Reply, sizeof (Reply));

  Status = UsbAudioFeatureUnitRequest (
             UsbIo,
             USB_AUDIO_20_REQTYPE_GET_INTERFACE,
             USB_AUDIO_20_REQ_RANGE,
             USB_AUDIO_20_FU_CS_VOLUME,
             AcInterface,
             FeatureUnitId,
             Channel,
             EfiUsbDataIn,
             Reply,
             sizeof (Reply)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumRanges = (UINT16)(Reply[0] | (Reply[1] << 8));
  if (NumRanges == 0) {
    return EFI_DEVICE_ERROR;
  }

  *MinVolume  = (INT16)(Reply[2] | (Reply[3] << 8));
  *MaxVolume  = (INT16)(Reply[4] | (Reply[5] << 8));
  *Resolution = (INT16)(Reply[6] | (Reply[7] << 8));

  return EFI_SUCCESS;
}
