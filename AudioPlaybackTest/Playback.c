/** @file
  Helpers for playback through the Audio Output Protocol.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Protocol/Shell.h>

#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "Playback.h"

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
  )
{
  ISOCH_CALLBACK_CONTEXT  *Ctx;

  Ctx         = (ISOCH_CALLBACK_CONTEXT *)Context;
  Ctx->Status = Status;
  Ctx->Done   = TRUE;
}

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
  )
{
  UINTN  FrameSize;
  UINTN  Size;

  FrameSize = (UINTN)Info->Channels * Info->SubslotSize;
  if (FrameSize == 0) {
    FrameSize = 1;
  }

  Size = (UINTN)SampleRateHz * FrameSize * PLAYBACK_DURATION_MS / 1000U;

  if (Size > PLAYBACK_MAX_BUFFER_SIZE) {
    Size = PLAYBACK_MAX_BUFFER_SIZE;
  }

  Size -= (Size % FrameSize);

  if (Size == 0) {
    Size = FrameSize;
  }

  return Size;
}

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
  )
{
  UINTN   Offset;
  UINTN   FrameSize;
  UINTN   Channel;
  UINTN   ByteIndex;
  UINTN   BitsPerSample;
  UINT32  Slot;

  if ((Buffer == NULL) || (Info == NULL) || (Phase == NULL)) {
    return;
  }

  FrameSize = (UINTN)Info->Channels * Info->SubslotSize;

  BitsPerSample = Info->BitsPerSample;
  if ((BitsPerSample == 0) || (BitsPerSample > 8 * (UINTN)Info->SubslotSize)) {
    BitsPerSample = 8 * (UINTN)Info->SubslotSize;
  }

  for (Offset = 0; (Offset + FrameSize) <= BufferSize; Offset += FrameSize) {
    Slot = (*Phase >> (32 - BitsPerSample)) << (8 * Info->SubslotSize - BitsPerSample);

    for (Channel = 0; Channel < Info->Channels; Channel++) {
      for (ByteIndex = 0; ByteIndex < Info->SubslotSize; ByteIndex++) {
        Buffer[Offset + Channel * Info->SubslotSize + ByteIndex] = (UINT8)(Slot >> (8 * ByteIndex));
      }
    }

    *Phase += PLAYBACK_SAW_PHASE_STEP;
  }
}

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
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  for (Index = 0; Index < Count; Index++) {
    Ctx[Index].Done   = FALSE;
    Ctx[Index].Status = EFI_SUCCESS;

    GenerateSawFrames (Buffers[Index], BufferSize, Info, Phase);

    Status = AudioOutput->QueueAudio (
                              AudioOutput,
                              Buffers[Index],
                              BufferSize,
                              TransferCallback,
                              &Ctx[Index]
                              );
    if (EFI_ERROR (Status)) {
      Print (L"  QueueAudio failed for buffer %u: %r\n", Index, Status);
      Ctx[Index].Status = Status;
      Ctx[Index].Done   = TRUE;
    }
  }
}

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
  )
{
  UINTN    Index;
  UINTN    Waited;
  UINTN    Completed;
  BOOLEAN  AllDone;

  for (Waited = 0; Waited < PLAYBACK_WAIT_TIMEOUT_US; Waited += 20000) {
    AllDone = TRUE;
    for (Index = 0; Index < Count; Index++) {
      if (!Ctx[Index].Done) {
        AllDone = FALSE;
        break;
      }
    }

    if (AllDone) {
      break;
    }

    gBS->Stall (20000);
  }

  Completed = 0;
  for (Index = 0; Index < Count; Index++) {
    if (Ctx[Index].Done) {
      Completed++;
      Print (L"  Buffer %u: %r\n", Index, Ctx[Index].Status);
    } else {
      Print (L"  Buffer %u: TIMED OUT (callback never fired)\n", Index);
    }
  }

  return Completed;
}

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
  )
{
  UINTN  Index;
  UINTN  Matches;

  Matches = 0;
  for (Index = 0; Index < Count; Index++) {
    if (Ctx[Index].Done && (Ctx[Index].Status == Status)) {
      Matches++;
    }
  }

  return Matches;
}

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
  )
{
  EFI_STATUS  Status;
  INT16       Min;
  INT16       Max;
  INT16       Res;
  INT16       Target;

  Status = AudioOutput->GetVolumeRange (AudioOutput, &Min, &Max, &Res);
  if (EFI_ERROR (Status)) {
    Print (L" Volume control unavailable (%r); leaving %s volume unchanged\n", Status, Label);
    return;
  }

  Target = (INT16)((INT32)Min + (((INT32)Max - (INT32)Min) * (INT32)Num) / (INT32)Den);

  Status = AudioOutput->SetVolume (AudioOutput, Target);
  Print (
    L" Volume -> %s: target %d (range %d..%d, step %d): %r\n",
    Label,
    Target,
    Min,
    Max,
    Res,
    Status
    );
}

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
  )
{
  EFI_STATUS              Status;
  EFI_SHELL_PROTOCOL      *Shell;
  SHELL_FILE_HANDLE       File;
  UINT64                  FileSize;
  UINT64                  Remaining;
  UINT8                   *FileBuffer;
  UINT8                   *Ptr;
  UINTN                   ReadSize;
  UINTN                   PlaySize;
  ISOCH_CALLBACK_CONTEXT  Ctx;

  if (FrameSize == 0) {
    FrameSize = 1;
  }

  Status = gBS->LocateProtocol (&gEfiShellProtocolGuid, NULL, (VOID **)&Shell);
  if (EFI_ERROR (Status)) {
    Print (L"Shell protocol unavailable (%r); cannot read file\n", Status);
    return Status;
  }

  Status = Shell->OpenFileByName (FileName, &File, EFI_FILE_MODE_READ);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to open '%s': %r\n", FileName, Status);
    return Status;
  }

  FileSize = 0;
  Shell->GetFileSize (File, &FileSize);

  FileBuffer = AllocatePool ((UINTN)FileSize);
  if (FileBuffer == NULL) {
    Print (L"File too large to load into memory (%lu bytes)\n", FileSize);
    Shell->CloseFile (File);
    return EFI_OUT_OF_RESOURCES;
  }

  Print (L"Loading raw PCM file '%s' (%lu bytes) into memory\n", FileName, FileSize);

  //
  // Read the whole file up front so playback never blocks on disk I/O.
  //
  Ptr       = FileBuffer;
  Remaining = FileSize;
  while (Remaining > 0) {
    ReadSize = (Remaining > SIZE_1MB) ? SIZE_1MB : (UINTN)Remaining;
    Status   = Shell->ReadFile (File, &ReadSize, Ptr);
    if (EFI_ERROR (Status) || (ReadSize == 0)) {
      break;
    }

    Ptr       += ReadSize;
    Remaining -= ReadSize;
  }

  Shell->CloseFile (File);

  //
  // Submit only whole frames; a trailing partial frame is dropped.
  //
  PlaySize  = (UINTN)(FileSize - Remaining);
  PlaySize -= (PlaySize % FrameSize);
  if (PlaySize == 0) {
    Print (L"No playable audio in file\n");
    FreePool (FileBuffer);
    return EFI_SUCCESS;
  }

  Print (L"Playing %u bytes...\n", PlaySize);

  Ctx.Done   = FALSE;
  Ctx.Status = EFI_SUCCESS;

  Status = AudioOutput->QueueAudio (AudioOutput, FileBuffer, PlaySize, TransferCallback, &Ctx);
  if (EFI_ERROR (Status)) {
    Print (L"QueueAudio failed: %r\n", Status);
  } else {
    while (!Ctx.Done) {
      gBS->Stall (1000);
    }

    Print (L"Finished (%r)\n", Ctx.Status);
  }

  FreePool (FileBuffer);
  return Status;
}

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
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       HandleCount;

  Handles = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEdkiiAudioOutputProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    if (Handles != NULL) {
      FreePool (Handles);
    }

    Print (L"No USB Audio device found: %r\n", Status);
    return EFI_NOT_FOUND;
  }

  Status = gBS->HandleProtocol (Handles[0], &gEdkiiAudioOutputProtocolGuid, (VOID **)AudioOutput);
  if (!EFI_ERROR (Status)) {
    Status = gBS->HandleProtocol (Handles[0], &gEdkiiAudioCodecProtocolGuid, (VOID **)AudioCodec);
  }

  if (Handles != NULL) {
    FreePool (Handles);
  }

  if (EFI_ERROR (Status)) {
    Print (L"Failed to obtain audio protocols: %r\n", Status);
  }

  return Status;
}
