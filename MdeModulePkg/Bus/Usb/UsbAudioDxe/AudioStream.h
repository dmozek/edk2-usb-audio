/** @file
  USB Audio isochronous stream manager.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include "UsbAudioTypes.h"

/**
  Initialize per-slot states, allocate the per-slot packet buffers and reset
  queue state.

  @param[in,out] Stream  Stream context to initialize. MaxPacketSize must
                         already reflect the selected alternate setting.

  @retval EFI_SUCCESS            Slots initialized.
  @retval EFI_INVALID_PARAMETER  The stream has no usable max packet size.
  @retval EFI_OUT_OF_RESOURCES   A packet buffer allocation failed.
**/
EFI_STATUS
InitStreams (
  IN OUT USB_AUDIO_STREAM_CTX  *Stream
  );

/**
  Reset per-slot transfer state and release the per-slot packet buffers.

  @param[in] Stream  Stream context to tear down.
**/
VOID
ResetStreams (
  IN USB_AUDIO_STREAM_CTX  *Stream
  );

/**
  Enqueue a caller buffer for async playback.

  @param[in] Dev         Main device context.
  @param[in] Buffer      PCM audio data in the configured format (not copied).
  @param[in] BufferSize  Length in bytes. Must be a non-zero multiple of
                         channels times subslot size.
  @param[in] Callback    Called when Buffer may be reused.
  @param[in] Context     Forwarded to Callback.

  @retval EFI_SUCCESS            Buffer enqueued.
  @retval EFI_OUT_OF_RESOURCES   Pending queue is full.
  @retval EFI_INVALID_PARAMETER  BufferSize is zero or not frame-aligned.
**/
EFI_STATUS
EnqueueAudioStream (
  IN USB_AUDIO_DEV                     *Dev,
  IN VOID                              *Buffer,
  IN UINTN                             BufferSize,
  IN EFI_AUDIO_OUTPUT_BUFFER_COMPLETE  Callback,
  IN VOID                              *Context   OPTIONAL
  );

/**
  Cancel all pending transfers and drain the in-flight ones.

  @param[in] Dev  Main device context.

  @retval TRUE   All transfers drained.
  @retval FALSE  Timeout occurred while draining transfers.
**/
BOOLEAN
StopAudio (
  IN USB_AUDIO_DEV  *Dev
  );

/**
  Attempt to fill free transfer slots from the pending queue and submit them.
  Called periodically from a timer.

  @param[in] Event    Timer event that triggered this call (unused).
  @param[in] Context  Pointer to the USB_AUDIO_DEV structure.
**/
VOID
EFIAPI
ManageAudioStreams (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

/**
  Open the USB I/O protocol for the audio stream.

  @param[in,out] Dev         Main device context.
  @param[in]     AgentHandle Handle of the agent opening the protocol.

  @retval EFI_SUCCESS           Protocol opened successfully.
  @retval EFI_NOT_FOUND         No matching USB I/O protocol found.
**/
EFI_STATUS
OpenAudioStreamIo (
  IN OUT USB_AUDIO_DEV  *Dev,
  IN EFI_HANDLE         AgentHandle
  );

/**
  Drain all pending transfers from the audio stream.

  @param[in] Stream  Stream context to drain.

  @retval TRUE   All transfers completed.
  @retval FALSE  Timeout occurred.
**/
BOOLEAN
DrainStreams (
  IN USB_AUDIO_STREAM_CTX  *Stream
  );
