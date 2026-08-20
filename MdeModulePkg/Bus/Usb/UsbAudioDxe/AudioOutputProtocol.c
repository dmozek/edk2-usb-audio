/** @file
  EFI_AUDIO_OUTPUT_PROTOCOL member implementations.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbAudio.h"
#include "UsbAudioTypes.h"
#include "AudioStream.h"
#include "AudioFormats.h"
#include "AudioRequests.h"

typedef enum {
  UsbAudioControlMute,
  UsbAudioControlVolume
} USB_AUDIO_CONTROL_KIND;

/**
  Return the set of channels a write should target. If the master channel (0)
  has the control, writing it covers the whole unit, so only channel 0 is
  returned; otherwise every per-channel control is targeted.

  @param[in] Mask  Control mask (bit c set if channel c has the control).

  @return  The channel mask to write.
**/
STATIC
UINT32
WriteChannelMask (
  IN UINT32  Mask
  )
{
  if ((Mask & 1u) != 0) {
    return 1u;
  }

  return Mask;
}

/**
  Apply a Feature Unit control (Mute or Volume) to every channel in the control
  mask, dispatching on the device's spec version.

  @param[in] Dev          Audio device context.
  @param[in] ControlMask  Control mask (bit c set if channel c has the control).
  @param[in] Kind         Which control to write (Mute or Volume).
  @param[in] Value        Mute state (0/1) or volume in Q8.8 signed dB.

  @retval EFI_SUCCESS       All channels written.
  @retval EFI_DEVICE_ERROR  A write failed.
 **/
STATIC
EFI_STATUS
ForEachWriteChannel (
  IN USB_AUDIO_DEV           *Dev,
  IN UINT32                  ControlMask,
  IN USB_AUDIO_CONTROL_KIND  Kind,
  IN INT16                   Value
  )
{
  UINT8       Channel;
  UINT32      WriteMask;
  EFI_STATUS  Status;

  WriteMask = WriteChannelMask (ControlMask);
  for (Channel = 0; Channel < USB_AUDIO_MAX_CHANNELS; Channel++) {
    if ((WriteMask & (1u << Channel)) == 0) {
      continue;
    }

    if (Kind == UsbAudioControlMute) {
      Status = (Dev->SpecVersion == UsbAudioSpec20)
               ? UsbAudio20SetMute (Dev->AcUsbIo, Dev->AcInterfaceNumber, Dev->FeatureUnitId, Channel, (BOOLEAN)Value)
               : UsbAudio10SetMute (Dev->AcUsbIo, Dev->AcInterfaceNumber, Dev->FeatureUnitId, Channel, (BOOLEAN)Value);
    } else {
      Status = (Dev->SpecVersion == UsbAudioSpec20)
               ? UsbAudio20SetVolume (Dev->AcUsbIo, Dev->AcInterfaceNumber, Dev->FeatureUnitId, Channel, Value)
               : UsbAudio10SetVolume (Dev->AcUsbIo, Dev->AcInterfaceNumber, Dev->FeatureUnitId, Channel, Value);
    }

    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Submit a buffer of PCM audio for asynchronous zero-copy playback.

  @param[in] This        Protocol instance.
  @param[in] Buffer      PCM audio data in the configured format (not copied).
  @param[in] BufferSize  Length in bytes. Must be a non-zero multiple of
                         channels times subslot size.
  @param[in] Callback    Invoked when Buffer may be reused.
  @param[in] Context     Forwarded to Callback.

  @retval EFI_SUCCESS            Buffer enqueued.
  @retval EFI_OUT_OF_RESOURCES   Pending queue is full.
  @retval EFI_INVALID_PARAMETER  BufferSize is zero or not frame-aligned or callback is null.
  @retval EFI_DEVICE_ERROR       No valid format selected or previous format selection failed.
**/
EFI_STATUS
EFIAPI
UsbAudioQueueAudio (
  IN EFI_AUDIO_OUTPUT_PROTOCOL         *This,
  IN VOID                              *Buffer,
  IN UINTN                             BufferSize,
  IN EFI_AUDIO_OUTPUT_BUFFER_COMPLETE  Callback,
  IN VOID                              *Context   OPTIONAL
  )
{
  EFI_STATUS     Status;
  USB_AUDIO_DEV  *Dev;

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (This);
  ASSERT (Dev->SupportedFormats != NULL);
  ASSERT (Dev->CurrentFormatIndex < Dev->SupportedFormatCount);

  if (!Dev->FormatValid) {
    DEBUG ((DEBUG_ERROR, "UsbAudioDxe: QueueAudio: no valid format selected\n"));
    return EFI_DEVICE_ERROR;
  }

  if ((Buffer == NULL) ||
      (BufferSize == 0) ||
      ((BufferSize % GetCurrentFrameSize (Dev)) != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "UsbAudioDxe: QueueAudio: invalid parameters (Buffer %p, BufferSize %u)\n",
      Buffer,
      BufferSize
      ));
    return EFI_INVALID_PARAMETER;
  }

  if (Callback == NULL) {
    DEBUG ((DEBUG_ERROR, "UsbAudioDxe: QueueAudio: callback is NULL\n"));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((
    DEBUG_INFO,
    "UsbAudioDxe: QueueAudio: %u bytes\n",
    BufferSize
    ));

  Status = EnqueueAudioStream (Dev, Buffer, BufferSize, Callback, Context);

  return Status;
}

/**
  Cancel all pending and in-flight audio buffers.

  @param[in] This  Protocol instance.

  @retval EFI_SUCCESS       All pending buffers cancelled.
  @retval EFI_DEVICE_ERROR  Timeout occurred while draining in-flight transfers.
**/
EFI_STATUS
EFIAPI
UsbAudioStopAudio (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *This
  )
{
  USB_AUDIO_DEV  *Dev;

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (This);

  DEBUG ((DEBUG_INFO, "UsbAudioDxe: StopAudio: protocol stop requested\n"));

  if (!StopAudio (Dev)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Read the mute state of the device.

  Mute is read from the master channel if it has a Mute control, otherwise from
  the lowest logical channel that does.

  @param[in]  This  Protocol instance.
  @param[out] Mute  TRUE if muted, FALSE if unmuted.

  @retval EFI_SUCCESS            Mute state returned.
  @retval EFI_INVALID_PARAMETER  Mute is NULL.
  @retval EFI_UNSUPPORTED        Device has no Mute control.
  @retval EFI_DEVICE_ERROR       USB request failed.
**/
EFI_STATUS
EFIAPI
UsbAudioGetMute (
  IN  EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  OUT BOOLEAN                    *Mute
  )
{
  USB_AUDIO_DEV  *Dev;
  UINT8          Channel;

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (This);

  if (Mute == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Dev->FeatureUnitId == 0) || (Dev->MuteControlMask == 0)) {
    return EFI_UNSUPPORTED;
  }

  Channel = (UINT8)LowBitSet32 (Dev->MuteControlMask);

  if (Dev->SpecVersion == UsbAudioSpec20) {
    return UsbAudio20GetMute (Dev->AcUsbIo, Dev->AcInterfaceNumber, Dev->FeatureUnitId, Channel, Mute);
  }

  return UsbAudio10GetMute (Dev->AcUsbIo, Dev->AcInterfaceNumber, Dev->FeatureUnitId, Channel, Mute);
}

/**
  Write the mute state of the device. The state is applied to every channel that
  exposes a Mute control (master, or all per-channel controls).

  @param[in] This  Protocol instance.
  @param[in] Mute  TRUE to mute, FALSE to unmute.

  @retval EFI_SUCCESS       Mute state applied.
  @retval EFI_UNSUPPORTED   Device has no Mute control.
  @retval EFI_DEVICE_ERROR  USB request failed.
**/
EFI_STATUS
EFIAPI
UsbAudioSetMute (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  IN BOOLEAN                    Mute
  )
{
  USB_AUDIO_DEV  *Dev;

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (This);

  if ((Dev->FeatureUnitId == 0) || (Dev->MuteControlMask == 0)) {
    return EFI_UNSUPPORTED;
  }

  return ForEachWriteChannel (Dev, Dev->MuteControlMask, UsbAudioControlMute, (INT16)Mute);
}

/**
  Read the current master volume in Q8.8 signed dB.

  Volume is read from the master channel if it has a Volume control, otherwise
  from the lowest logical channel that does.

  @param[in]  This    Protocol instance.
  @param[out] Volume  Current volume.

  @retval EFI_SUCCESS            Volume returned.
  @retval EFI_INVALID_PARAMETER  Volume is NULL.
  @retval EFI_UNSUPPORTED        Device has no Volume control.
  @retval EFI_DEVICE_ERROR       USB request failed.
**/
EFI_STATUS
EFIAPI
UsbAudioGetVolume (
  IN  EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  OUT INT16                      *Volume
  )
{
  USB_AUDIO_DEV  *Dev;
  UINT8          Channel;

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (This);

  if (Volume == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Dev->FeatureUnitId == 0) || (Dev->VolumeControlMask == 0)) {
    return EFI_UNSUPPORTED;
  }

  Channel = (UINT8)LowBitSet32 (Dev->VolumeControlMask);

  if (Dev->SpecVersion == UsbAudioSpec20) {
    return UsbAudio20GetVolume (Dev->AcUsbIo, Dev->AcInterfaceNumber, Dev->FeatureUnitId, Channel, Volume);
  }

  return UsbAudio10GetVolume (Dev->AcUsbIo, Dev->AcInterfaceNumber, Dev->FeatureUnitId, Channel, Volume);
}

/**
  Write the master volume in Q8.8 signed dB. The value is applied to every
  channel that exposes a Volume control (master, or all per-channel controls).

  @param[in] This    Protocol instance.
  @param[in] Volume  Desired volume.

  @retval EFI_SUCCESS       Volume applied.
  @retval EFI_UNSUPPORTED   Device has no Volume control.
  @retval EFI_DEVICE_ERROR  USB request failed.
**/
EFI_STATUS
EFIAPI
UsbAudioSetVolume (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  IN INT16                      Volume
  )
{
  USB_AUDIO_DEV  *Dev;

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (This);

  if ((Dev->FeatureUnitId == 0) || (Dev->VolumeControlMask == 0)) {
    return EFI_UNSUPPORTED;
  }

  return ForEachWriteChannel (Dev, Dev->VolumeControlMask, UsbAudioControlVolume, Volume);
}

/**
  Return the hardware volume limits and step size, read from the channel used by
  GetVolume.

  @param[in]  This        Protocol instance.
  @param[out] MinVolume   Minimum volume in Q8.8 signed dB.
  @param[out] MaxVolume   Maximum volume in Q8.8 signed dB.
  @param[out] Resolution  Smallest step in Q8.8 signed dB.

  @retval EFI_SUCCESS            Range returned.
  @retval EFI_INVALID_PARAMETER  An output pointer is NULL.
  @retval EFI_UNSUPPORTED        Device has no Volume control.
  @retval EFI_DEVICE_ERROR       USB request failed.
**/
EFI_STATUS
EFIAPI
UsbAudioGetVolumeRange (
  IN  EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  OUT INT16                      *MinVolume,
  OUT INT16                      *MaxVolume,
  OUT INT16                      *Resolution
  )
{
  USB_AUDIO_DEV  *Dev;
  UINT8          Channel;

  Dev = USB_AUDIO_DEV_FROM_OUTPUT_PROTOCOL (This);

  if ((MinVolume == NULL) || (MaxVolume == NULL) || (Resolution == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Dev->FeatureUnitId == 0) || (Dev->VolumeControlMask == 0)) {
    return EFI_UNSUPPORTED;
  }

  Channel = (UINT8)LowBitSet32 (Dev->VolumeControlMask);

  if (Dev->SpecVersion == UsbAudioSpec20) {
    return UsbAudio20GetVolumeRange (
             Dev->AcUsbIo,
             Dev->AcInterfaceNumber,
             Dev->FeatureUnitId,
             Channel,
             MinVolume,
             MaxVolume,
             Resolution
             );
  }

  return UsbAudio10GetVolumeRange (
           Dev->AcUsbIo,
           Dev->AcInterfaceNumber,
           Dev->FeatureUnitId,
           Channel,
           MinVolume,
           MaxVolume,
           Resolution
           );
}

// ---------------------------------------------------------------------------
// Pre-filled protocol template used by Start()
// ---------------------------------------------------------------------------

EFI_AUDIO_OUTPUT_PROTOCOL  gAudioOutputProtocolTemplate = {
  UsbAudioQueueAudio,
  UsbAudioStopAudio,
  UsbAudioGetMute,
  UsbAudioSetMute,
  UsbAudioGetVolume,
  UsbAudioSetVolume,
  UsbAudioGetVolumeRange,
};
