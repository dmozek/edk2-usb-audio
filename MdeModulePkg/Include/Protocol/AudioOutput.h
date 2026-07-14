/** @file
  EDK II Audio Output Protocol. A minimal playback and volume control interface.

  The protocol exposes two capabilities:
  1. Asynchronous PCM audio playback via QueueAudio / StopAudio.
  2. Volume and mute control.

  The input audio format is determined by the Audio Codec Protocol.

  The driver internally handles all USB-level details: device detection,
  alternate-setting selection and isochronous transfer scheduling.
  Callers interact with this protocol for playback and volume control only.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>

#define EDKII_AUDIO_OUTPUT_PROTOCOL_GUID \
  { 0x8832e8f7, 0x5fb4, 0x4ce9, \
    { 0xac, 0xb4, 0x0e, 0x08, 0x55, 0xa0, 0x79, 0x52 } }

extern EFI_GUID  gEdkiiAudioOutputProtocolGuid;

/**
  Maximum number of concurrently outstanding QueueAudio calls.
  If this limit is reached, QueueAudio returns EFI_OUT_OF_RESOURCES.
**/
#define AUDIO_OUTPUT_MAX_PENDING_BUFFERS  8u

typedef struct _EFI_AUDIO_OUTPUT_PROTOCOL EFI_AUDIO_OUTPUT_PROTOCOL;

/**
  Invoked once per QueueAudio call when all bytes in the submitted buffer have
  been transmitted over USB, or when the transfer was cancelled by StopAudio.

  After this callback returns, Buffer may be freely modified or freed by the
  caller.

  This callback may be invoked at TPL_CALLBACK or TPL_NOTIFY; callers must
  not acquire locks incompatible with those TPLs inside the callback.

  @param[in] Buffer      The pointer originally passed to QueueAudio.
  @param[in] BufferSize  The size originally passed to QueueAudio, in bytes.
  @param[in] Context     The context pointer originally passed to QueueAudio.
  @param[in] Status      EFI_SUCCESS - all bytes were transmitted.
                         EFI_ABORTED - cancelled by StopAudio.
**/
typedef
VOID
(EFIAPI *EFI_AUDIO_OUTPUT_BUFFER_COMPLETE)(
  IN VOID        *Buffer,
  IN UINTN        BufferSize,
  IN VOID        *Context,
  IN EFI_STATUS   Status
  );

/**
  Submit a buffer of PCM audio for asynchronous playback.

  The buffer must contain audio in the format determined by the Audio Codec Protocol.
  This function returns immediately without copying Buffer. The Callback is invoked exactly once,
  after all bytes have been transmitted or the transfer is cancelled.

  Buffer must remain valid and unmodified from when this function returns until
  Callback fires.

  @param[in] This        Protocol instance.
  @param[in] Buffer      PCM audio data in the configured input format.
  @param[in] BufferSize  Length of Buffer in bytes; must be a non-zero multiple
                         of number of channels times subslot size.
  @param[in] Callback    Function to call when Buffer may be reused.
  @param[in] Context     Opaque value forwarded to Callback.

  @retval EFI_SUCCESS           Buffer accepted; Callback will fire.
  @retval EFI_OUT_OF_RESOURCES  Pending queue is full; retry after a Callback.
  @retval EFI_INVALID_PARAMETER Callback or Buffer is NULL, BufferSize is 0 or not a
                                multiple of the frame size.
  @retval EFI_DEVICE_ERROR      No valid format selected or previous format selection failed.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_OUTPUT_QUEUE_AUDIO)(
  IN EFI_AUDIO_OUTPUT_PROTOCOL         *This,
  IN VOID                           *Buffer,
  IN UINTN                           BufferSize,
  IN EFI_AUDIO_OUTPUT_BUFFER_COMPLETE   Callback,
  IN VOID                           *Context   OPTIONAL
  );

/**
  Cancel all pending and in-flight audio buffers.

  Pending-queue entries (not yet submitted to USB) have their Callbacks
  invoked synchronously, within this call, with Status = EFI_ABORTED.
  In-flight entries (USB transfer already submitted) are waited to
  complete.

  After this function returns successfully, QueueAudio may be called again immediately.
  The isochronous pipe and alternate setting remain active.
  On failure, all pending and some in-flight entries may remain and no callback is fired for them.

  Must be called below TPL_NOTIFY.

  @param[in] This  Protocol instance.

  @retval EFI_SUCCESS       Pending queue drained synchronously; in-flight
                            cancellations issued.
  @retval EFI_DEVICE_ERROR  Some transfers remain in-flight. Pending entries still queued.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_OUTPUT_STOP_AUDIO)(
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *This
  );

/**
  Read the mute state of the device.

  @param[in]  This  Protocol instance.
  @param[out] Mute  TRUE if muted, FALSE if unmuted.

  @retval EFI_SUCCESS           Mute state returned.
  @retval EFI_UNSUPPORTED       Device has no mute control.
  @retval EFI_DEVICE_ERROR      USB request failed.
  @retval EFI_INVALID_PARAMETER Mute is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_OUTPUT_GET_MUTE)(
  IN  EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  OUT BOOLEAN                 *Mute
  );

/**
  Write the mute state of the device.

  @param[in] This  Protocol instance.
  @param[in] Mute  TRUE to mute, FALSE to unmute.

  @retval EFI_SUCCESS        Mute state applied.
  @retval EFI_UNSUPPORTED    Device has no mute control.
  @retval EFI_DEVICE_ERROR   USB request failed.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_OUTPUT_SET_MUTE)(
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  IN BOOLEAN                  Mute
  );

/**
  Read the current master volume.

  @param[in]  This    Protocol instance.
  @param[out] Volume  Current volume in Q8.8 signed dB.

  @retval EFI_SUCCESS           Volume returned.
  @retval EFI_UNSUPPORTED       Device has no volume control.
  @retval EFI_DEVICE_ERROR      USB request failed.
  @retval EFI_INVALID_PARAMETER Volume is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_OUTPUT_GET_VOLUME)(
  IN  EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  OUT INT16                   *Volume
  );

/**
  Write the master volume.

  @param[in] This    Protocol instance.
  @param[in] Volume  Desired volume in Q8.8 signed dB.

  @retval EFI_SUCCESS        Volume applied.
  @retval EFI_UNSUPPORTED    Device has no volume control.
  @retval EFI_DEVICE_ERROR   USB request failed or an invalid volume was specified.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_OUTPUT_SET_VOLUME)(
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  IN INT16                    Volume
  );

/**
  Return the hardware volume limits and step size.

  @param[in]  This        Protocol instance.
  @param[out] MinVolume   Minimum volume in Q8.8 signed dB.
  @param[out] MaxVolume   Maximum volume in Q8.8 signed dB.
  @param[out] Resolution  Smallest step in Q8.8 signed dB.

  @retval EFI_SUCCESS           Range returned.
  @retval EFI_UNSUPPORTED       Device has no volume control.
  @retval EFI_DEVICE_ERROR      USB request failed.
  @retval EFI_INVALID_PARAMETER An output pointer is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_OUTPUT_GET_VOLUME_RANGE)(
  IN  EFI_AUDIO_OUTPUT_PROTOCOL  *This,
  OUT INT16                   *MinVolume,
  OUT INT16                   *MaxVolume,
  OUT INT16                   *Resolution
  );

/**
  EDK II Audio Output Protocol.

  Produced by UsbAudioDxe on the AudioControl interface handle of a USB Audio
  Class device that exposes at least one Type I (PCM-family) AudioStreaming
  alternate setting.
**/
struct _EFI_AUDIO_OUTPUT_PROTOCOL {
  EFI_AUDIO_OUTPUT_QUEUE_AUDIO         QueueAudio;
  EFI_AUDIO_OUTPUT_STOP_AUDIO          StopAudio;
  EFI_AUDIO_OUTPUT_GET_MUTE            GetMute;
  EFI_AUDIO_OUTPUT_SET_MUTE            SetMute;
  EFI_AUDIO_OUTPUT_GET_VOLUME          GetVolume;
  EFI_AUDIO_OUTPUT_SET_VOLUME          SetVolume;
  EFI_AUDIO_OUTPUT_GET_VOLUME_RANGE    GetVolumeRange;
};
