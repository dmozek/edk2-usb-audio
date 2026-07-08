/** @file
  Common USB Audio request and helper declarations.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Protocol/UsbIo.h>
#include "UsbAudioTypes.h"

/** Timeout for all audio-class control transfers, in milliseconds. */
#define USB_AUDIO_CONTROL_TIMEOUT_MS  1000u

/** UAC 2.0 Clock Source bmControls: D1..D0 = Clock Frequency Control. */
#define USB_AUDIO_20_CS_FREQ_CONTROL_MASK  0x03u

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
  );

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
  );

/**
  Set the sample rate of a UAC 2.0 Clock Source entity (CUR request on
  CS_SAM_FREQ_CONTROL, 4-byte little-endian payload).

  @param[in] UsbIo          UsbIo of any interface on the device (control
                            transfers reach EP0; wIndex selects the target).
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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

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
  );

/**
  Set the sample rate of a UAC 1.0 isochronous endpoint.

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
  );

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
  );

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
  );

/**
  Read the volume of channel via a UAC 1.0 GET_CUR request.

  @param[in]  UsbIo          UsbIo for the AC interface.
  @param[in]  AcInterface    AudioControl interface number.
  @param[in]  FeatureUnitId  Feature Unit entity ID.
  @param[in]  Channel        Channel number.
  @param[out] Volume         Current volume.

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
  );

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
  );

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
  );
