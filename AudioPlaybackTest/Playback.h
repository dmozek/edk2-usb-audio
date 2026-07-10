/** @file
  Helpers for playback through the Audio Output Protocol.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/UefiLib.h>

#include <Protocol/AudioOutput.h>
#include <Protocol/AudioCodec.h>

/** Buffers queued per playback "set". Two sets fill the 8-deep driver queue. **/
#define PLAYBACK_SET_SIZE  8U

/** Total buffers allocated: two sets queued back-to-back. **/
#define PLAYBACK_TOTAL_BUFFERS  (2U * PLAYBACK_SET_SIZE)

/** Approximate playback duration per buffer, in milliseconds. **/
#define PLAYBACK_DURATION_MS  1000U

/** Bytes per audio frame the pre-allocated buffers are sized for (stereo, 4-byte subslots). **/
#define PLAYBACK_NOMINAL_FRAME_SIZE  8U

/** Highest sample rate we may select; sizes the pre-allocated buffers. **/
#define PLAYBACK_MAX_RATE_HZ  192000U

/** Allocation size of each buffer (large enough for the highest rate). **/
#define PLAYBACK_MAX_BUFFER_SIZE \
  (PLAYBACK_MAX_RATE_HZ * PLAYBACK_NOMINAL_FRAME_SIZE * PLAYBACK_DURATION_MS / 1000U)

/**
  Per-frame increment of the 32-bit sawtooth phase accumulator:
  one full period every 128 frames (375 Hz at 48 kHz).
**/
#define PLAYBACK_SAW_PHASE_STEP  0x02000000U

/** Upper bound on how long to wait for a set of callbacks, in microseconds. **/
#define PLAYBACK_WAIT_TIMEOUT_US  (10U * 1000U * 1000U)

typedef struct {
  volatile BOOLEAN    Done;
  EFI_STATUS          Status;
} ISOCH_CALLBACK_CONTEXT;

typedef struct {
  UINTN      FormatIndex;
  UINT32     SampleRateHz;
  BOOLEAN    Valid;
} FORMAT_SELECTION;

/**
  Callback invoked when a QueueAudio transfer completes or is cancelled.

  @param[in] Buffer      The pointer originally passed to QueueAudio.
  @param[in] BufferSize  The size originally passed to QueueAudio, in bytes.
  @param[in] Context     The context pointer originally passed to QueueAudio.
  @param[in] Status      EFI_SUCCESS - all bytes were transmitted.
                         EFI_ABORTED - cancelled by StopAudio.
 **/
VOID
EFIAPI
TransferCallback (
  IN VOID        *Buffer,
  IN UINTN       BufferSize,
  IN VOID        *Context,
  IN EFI_STATUS  Status
  );

/**
  Compute a frame-aligned playback length of about PLAYBACK_DURATION_MS for
  the given sample rate and format.

  @param[in] SampleRateHz  Sample rate of the selected format.
  @param[in] Info          Format information of the selected format.

  @return  Length in bytes, a multiple of the format's frame size and no
           larger than PLAYBACK_MAX_BUFFER_SIZE.
**/
UINTN
ComputePlaybackSize (
  IN UINT32                                    SampleRateHz,
  IN CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info
  );

/**
  Generate a sawtooth signal matching the selected format.

  Every channel of a frame carries the same sample at the format's bit resolution,
  left-justified within the subslot.
  The sample is the top BitsPerSample bits of a 32-bit phase accumulator.

  @param[out]     Buffer      The buffer to fill.
  @param[in]      BufferSize  The size of the buffer in bytes.
  @param[in]      Info        Format information of the selected format.
  @param[in,out]  Phase       The 32-bit phase accumulator of the sawtooth.
**/
VOID
GenerateSawFrames (
  OUT UINT8                                     *Buffer,
  IN  UINTN                                     BufferSize,
  IN  CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info,
  IN OUT UINT32                                 *Phase
  );

/**
  Queue a set of buffers for playback.

  Each buffer is refilled with a saw signal matching the format before being
  submitted.

  @param[in]     AudioOutput    The Audio Output protocol instance.
  @param[in,out] Buffers        Array of buffer pointers.
  @param[in,out] Ctx            Per-buffer callback contexts.
  @param[in]     Count          Number of buffers to queue.
  @param[in]     BufferSize     Bytes to submit from each buffer.
  @param[in]     Info           Format information of the selected format.
  @param[in,out] Phase          Saw generator phase, carried across buffers.
**/
VOID
QueuePlaybackSet (
  IN     EFI_AUDIO_OUTPUT_PROTOCOL                 *AudioOutput,
  IN OUT UINT8                                     **Buffers,
  IN OUT ISOCH_CALLBACK_CONTEXT                    *Ctx,
  IN     UINTN                                     Count,
  IN     UINTN                                     BufferSize,
  IN     CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info,
  IN OUT UINT32                                    *Phase
  );

/**
  Wait (bounded) for a set of queued buffers to complete and report each result.

  @param[in] Ctx    Per-buffer callback contexts.
  @param[in] Count  Number of buffers to wait on.

  @return  Number of buffers whose callback fired before the timeout.
**/
UINTN
WaitPlaybackSet (
  IN ISOCH_CALLBACK_CONTEXT  *Ctx,
  IN UINTN                   Count
  );

/**
  Count how many completed buffers returned a particular status.

  @param[in] Ctx     Per-buffer callback contexts.
  @param[in] Count   Number of buffers to inspect.
  @param[in] Status  Status to match.

  @return  Number of buffers whose callback fired with Status.
**/
UINTN
CountWithStatus (
  IN ISOCH_CALLBACK_CONTEXT  *Ctx,
  IN UINTN                   Count,
  IN EFI_STATUS              Status
  );

/**
  Set the device volume to a fraction (Num/Den) of its supported range and
  report the result. Quietly does nothing if the device has no volume control.

  @param[in] AudioOutput    The Audio Output protocol instance.
  @param[in] Num            Numerator of the fraction within [Min, Max].
  @param[in] Den            Denominator of the fraction (must be non-zero).
  @param[in] Label          Human-readable label for logging.
**/
VOID
SetVolumeFraction (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput,
  IN UINTN                      Num,
  IN UINTN                      Den,
  IN CONST CHAR16               *Label
  );

/**
  Play a raw PCM file verbatim (no format conversion).

  The entire file is read
  into memory first, then submitted in one QueueAudio call, so that no disk I/O
  occurs while audio is playing.
  The bytes are assumed to already match the selected device format.

  @param[in] AudioOutput    The Audio Output protocol instance.
  @param[in] FileName       Path to the raw PCM file.
  @param[in] FrameSize      Bytes per audio frame of the selected format; a
                            trailing partial frame of the file is dropped.

  @retval EFI_SUCCESS           The file was played.
  @retval EFI_OUT_OF_RESOURCES  The file did not fit in memory.
  @retval other                 The shell or file could not be opened/read.
**/
EFI_STATUS
PlayRawPcmFile (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput,
  IN CONST CHAR16               *FileName,
  IN UINTN                      FrameSize
  );

/**
  Open the Audio Output and Audio Codec protocols on the first handle that supports them.

  @param[out] AudioOutput   The Audio Output protocol instance.
  @param[out] AudioCodec    The Audio Codec protocol instance.

  @retval EFI_SUCCESS           Both protocols were opened successfully.
  @retval other                 Failed to locate or open the protocols.
 **/
EFI_STATUS
OpenAudioProtocols (
  OUT EFI_AUDIO_OUTPUT_PROTOCOL  **AudioOutput,
  OUT EFI_AUDIO_CODEC_PROTOCOL   **AudioCodec
  );
