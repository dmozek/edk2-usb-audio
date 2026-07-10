/** @file
  Test patterns to exercise the Audio Output and Audio Codec protocols.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "PlaybackTest.h"
#include "FormatSelect.h"

/**
  Get the format information of the currently selected format.

  @param[in] AudioCodec  The Audio Codec protocol instance.

  @return  The format information, or NULL if no format is selected.
**/
STATIC
CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION *
CurrentFormatInfo (
  IN EFI_AUDIO_CODEC_PROTOCOL  *AudioCodec
  )
{
  if (AudioCodec->CurrentFormat == NULL) {
    return NULL;
  }

  return AudioCodec->CurrentFormat->Info;
}

/**
  Queue two back-to-back sets and wait for the second to complete. The first set
  plays at medium volume and the second at low volume.

  @param[in]     AudioOutput    The Audio Output protocol instance.
  @param[in,out] Buffers        Array of PLAYBACK_TOTAL_BUFFERS buffer pointers.
  @param[in,out] Ctx            Array of PLAYBACK_TOTAL_BUFFERS contexts.
  @param[in]     BufferSize     Bytes to submit from each buffer.
  @param[in]     Info           Format information of the selected format.
  @param[in,out] Phase          Saw generator phase.
**/
STATIC
VOID
RunTwoSets (
  IN     EFI_AUDIO_OUTPUT_PROTOCOL                 *AudioOutput,
  IN OUT UINT8                                     **Buffers,
  IN OUT ISOCH_CALLBACK_CONTEXT                    *Ctx,
  IN     UINTN                                     BufferSize,
  IN     CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info,
  IN OUT UINT32                                    *Phase
  )
{
  UINTN  Matches;

  SetVolumeFraction (AudioOutput, 1, 2, L"medium");
  Print (L" Queueing set 1 (%u buffers)\n", PLAYBACK_SET_SIZE);
  QueuePlaybackSet (AudioOutput, &Buffers[0], &Ctx[0], PLAYBACK_SET_SIZE, BufferSize, Info, Phase);

  WaitPlaybackSet (Ctx, PLAYBACK_SET_SIZE);

  Matches = CountWithStatus (Ctx, PLAYBACK_SET_SIZE, EFI_SUCCESS);

  SetVolumeFraction (AudioOutput, 1, 8, L"low");
  Print (L" Queueing set 2 (%u buffers)\n", PLAYBACK_SET_SIZE);
  QueuePlaybackSet (
    AudioOutput,
    &Buffers[PLAYBACK_SET_SIZE],
    &Ctx[PLAYBACK_SET_SIZE],
    PLAYBACK_SET_SIZE,
    BufferSize,
    Info,
    Phase
    );

  Print (L" Waiting for both sets to complete\n");
  WaitPlaybackSet (&Ctx[PLAYBACK_SET_SIZE], PLAYBACK_SET_SIZE);

  Matches += CountWithStatus (&Ctx[PLAYBACK_SET_SIZE], PLAYBACK_SET_SIZE, EFI_SUCCESS);

  if (Matches != 2*PLAYBACK_SET_SIZE) {
    Print (
      L"  WARNING: only %u of %u buffers completed successfully\n",
      Matches,
      2 * PLAYBACK_SET_SIZE
      );
  }
}

/**
  Exercise the Mute control

  Perform a mute, play a set (expected silent), read back the
  mute state, then unmute and play a set (expected audible).
  Skips cleanly if the device has no Mute control.

  @param[in]     AudioOutput    The Audio Output protocol instance.
  @param[in,out] Buffers        Array of buffer pointers.
  @param[in,out] Ctx            Array of contexts.
  @param[in]     BufferSize     Bytes to submit from each buffer.
  @param[in]     Info           Format information of the selected format.
  @param[in,out] Phase          Saw generator phase.
**/
STATIC
VOID
RunMuteTest (
  IN     EFI_AUDIO_OUTPUT_PROTOCOL                 *AudioOutput,
  IN OUT UINT8                                     **Buffers,
  IN OUT ISOCH_CALLBACK_CONTEXT                    *Ctx,
  IN     UINTN                                     BufferSize,
  IN     CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info,
  IN OUT UINT32                                    *Phase
  )
{
  EFI_STATUS  Status;
  BOOLEAN     Mute;

  Print (L"\n=== mute test ===\n");

  //
  // Play at medium volume so the muted/unmuted difference is obvious.
  //
  SetVolumeFraction (AudioOutput, 1, 2, L"medium");

  Status = AudioOutput->SetMute (AudioOutput, TRUE);
  if (Status == EFI_UNSUPPORTED) {
    Print (L" Mute control not supported by device; skipping mute test\n");
    return;
  }

  Print (L" SetMute(TRUE): %r\n", Status);

  Mute = FALSE;
  if (!EFI_ERROR (AudioOutput->GetMute (AudioOutput, &Mute))) {
    Print (L" GetMute -> %a\n", Mute ? "muted" : "unmuted");
    if (!Mute) {
      Print (L" WARNING: device did not report muted state after SetMute(TRUE)\n");
    }
  }

  Print (L" Playing one set while muted (expect silence)\n");
  QueuePlaybackSet (AudioOutput, Buffers, Ctx, PLAYBACK_SET_SIZE, BufferSize, Info, Phase);
  WaitPlaybackSet (Ctx, PLAYBACK_SET_SIZE);

  Status = AudioOutput->SetMute (AudioOutput, FALSE);
  Print (L" SetMute(FALSE): %r\n", Status);

  Mute = TRUE;
  if (!EFI_ERROR (AudioOutput->GetMute (AudioOutput, &Mute))) {
    Print (L" GetMute -> %a\n", Mute ? "muted" : "unmuted");
    if (Mute) {
      Print (L" WARNING: device still reports muted after SetMute(FALSE)\n");
    }
  }

  Print (L" Playing one set unmuted (expect audio)\n");
  QueuePlaybackSet (AudioOutput, Buffers, Ctx, PLAYBACK_SET_SIZE, BufferSize, Info, Phase);
  WaitPlaybackSet (Ctx, PLAYBACK_SET_SIZE);
}

/**
  Run a single-format playback exercise, including a mute/unmute test,
  changing volume, and a StopAudio test.

  @param[in] AudioOutput    The Audio Output protocol instance.
  @param[in] AudioCodec     The Audio Codec protocol instance.
  @param[in] Format         The format to use for playback.

  @retval EFI_SUCCESS       The test completed successfully.
  @retval other             Some error occurred during the test.
 **/
EFI_STATUS
RunSingleFormatTest (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput,
  IN EFI_AUDIO_CODEC_PROTOCOL   *AudioCodec,
  IN FORMAT_SELECTION           *Format
  )
{
  EFI_STATUS                                Status;
  UINT8                                     *AudioBuffers[PLAYBACK_TOTAL_BUFFERS];
  ISOCH_CALLBACK_CONTEXT                    CallbackCtx[PLAYBACK_TOTAL_BUFFERS];
  CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info;
  UINTN                                     Index;
  UINTN                                     PlaybackSize;
  UINT32                                    Phase;
  UINTN                                     Aborted;

  Phase   = 0;
  Aborted = 0;

  for (Index = 0; Index < PLAYBACK_TOTAL_BUFFERS; Index++) {
    AudioBuffers[Index] = AllocateZeroPool (PLAYBACK_MAX_BUFFER_SIZE);
    if (AudioBuffers[Index] == NULL) {
      Print (L"Failed to allocate audio buffer %u\n", Index);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }
  }

  Status = SelectFormat (AudioCodec, Format);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Info = CurrentFormatInfo (AudioCodec);
  if (Info == NULL) {
    Print (L"No format information for the selected format\n");
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  PlaybackSize = ComputePlaybackSize (Format->SampleRateHz, Info);

  Print (L"\n=== single-format test: two sets ===\n");
  RunTwoSets (AudioOutput, AudioBuffers, CallbackCtx, PlaybackSize, Info, &Phase);

  Print (L"\n=== single-format test: abort-on-stop ===\n");
  Print (L" Queueing one set (%u buffers)\n", PLAYBACK_SET_SIZE);
  QueuePlaybackSet (AudioOutput, AudioBuffers, CallbackCtx, PLAYBACK_SET_SIZE, PlaybackSize, Info, &Phase);

  Print (L" Calling StopAudio while buffers are outstanding\n");
  AudioOutput->StopAudio (AudioOutput);

  WaitPlaybackSet (CallbackCtx, PLAYBACK_SET_SIZE);
  Aborted = CountWithStatus (CallbackCtx, PLAYBACK_SET_SIZE, EFI_ABORTED);
  Print (L" %u of %u buffers returned EFI_ABORTED\n", Aborted, PLAYBACK_SET_SIZE);

  if (Aborted > 0) {
    Print (L" PASS: StopAudio aborted outstanding buffers\n");
  } else {
    Print (L" FAIL: expected at least one EFI_ABORTED buffer\n");
  }

  RunMuteTest (AudioOutput, AudioBuffers, CallbackCtx, PlaybackSize, Info, &Phase);

Exit:
  for (Index = 0; Index < PLAYBACK_TOTAL_BUFFERS; Index++) {
    if (AudioBuffers[Index] != NULL) {
      FreePool (AudioBuffers[Index]);
    }
  }

  return Status;
}

/**
  Run a two-format playback exercise, including a mute/unmute test,
  changing volume, a StopAudio test and a format switch mid-playback.

  @param[in] AudioOutput  The Audio Output protocol instance.
  @param[in] AudioCodec   The Audio Codec protocol instance.
  @param[in] FormatA      The first format to use for playback.
  @param[in] FormatB      The format to switch to mid-playback.

  @retval EFI_SUCCESS       The test completed successfully.
  @retval other             Some error occurred during the test.
 **/
EFI_STATUS
RunTwoFormatTest (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput,
  IN EFI_AUDIO_CODEC_PROTOCOL   *AudioCodec,
  IN FORMAT_SELECTION           *FormatA,
  IN FORMAT_SELECTION           *FormatB
  )
{
  EFI_STATUS                                Status;
  UINT8                                     *AudioBuffers[PLAYBACK_TOTAL_BUFFERS];
  ISOCH_CALLBACK_CONTEXT                    CallbackCtx[PLAYBACK_TOTAL_BUFFERS];
  CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION  *InfoA;
  CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION  *InfoB;
  UINTN                                     Index;
  UINTN                                     PlaybackSizeA;
  UINTN                                     PlaybackSizeB;
  UINT32                                    Phase;
  UINTN                                     Aborted;

  Phase   = 0;
  Aborted = 0;

  for (Index = 0; Index < PLAYBACK_TOTAL_BUFFERS; Index++) {
    AudioBuffers[Index] = AllocateZeroPool (PLAYBACK_MAX_BUFFER_SIZE);
    if (AudioBuffers[Index] == NULL) {
      Print (L"Failed to allocate audio buffer %u\n", Index);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }
  }

  Status = SelectFormat (AudioCodec, FormatA);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  InfoA = CurrentFormatInfo (AudioCodec);
  if (InfoA == NULL) {
    Print (L"No format information for format A\n");
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  PlaybackSizeA = ComputePlaybackSize (FormatA->SampleRateHz, InfoA);

  RunTwoSets (AudioOutput, AudioBuffers, CallbackCtx, PlaybackSizeA, InfoA, &Phase);

  Status = SelectFormat (AudioCodec, FormatB);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  InfoB = CurrentFormatInfo (AudioCodec);
  if (InfoB == NULL) {
    Print (L"No format information for format B\n");
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  PlaybackSizeB = ComputePlaybackSize (FormatB->SampleRateHz, InfoB);

  RunTwoSets (AudioOutput, AudioBuffers, CallbackCtx, PlaybackSizeB, InfoB, &Phase);

  //
  // queue a set on format B, then immediately switch back to format A.
  // The format change drains/cancels the stream, so outstanding buffers must
  // come back with EFI_ABORTED.
  //
  Print (L"\n=== abort-on-format-change ===\n");
  Print (L" Queueing one set (%u buffers) on format B\n", PLAYBACK_SET_SIZE);
  QueuePlaybackSet (AudioOutput, AudioBuffers, CallbackCtx, PLAYBACK_SET_SIZE, PlaybackSizeB, InfoB, &Phase);

  Status = SelectFormat (AudioCodec, FormatA);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  WaitPlaybackSet (CallbackCtx, PLAYBACK_SET_SIZE);
  Aborted = CountWithStatus (CallbackCtx, PLAYBACK_SET_SIZE, EFI_ABORTED);
  Print (L" %u of %u buffers returned EFI_ABORTED\n", Aborted, PLAYBACK_SET_SIZE);

  if (Aborted > 0) {
    Print (L" PASS: format change aborted outstanding buffers\n");
  } else {
    Print (L" FAIL: expected at least one EFI_ABORTED buffer\n");
  }

  //
  // one more set of playback on the now-selected format A.
  //
  Print (L"\n=== playback on restored format ===\n");
  Print (L" Queueing one set (%u buffers)\n", PLAYBACK_SET_SIZE);
  QueuePlaybackSet (AudioOutput, AudioBuffers, CallbackCtx, PLAYBACK_SET_SIZE, PlaybackSizeA, InfoA, &Phase);
  WaitPlaybackSet (CallbackCtx, PLAYBACK_SET_SIZE);

  Aborted = CountWithStatus (CallbackCtx, PLAYBACK_SET_SIZE, EFI_SUCCESS);

  if (Aborted != PLAYBACK_SET_SIZE) {
    Print (L" only %u of %u buffers completed successfully\n", Aborted, PLAYBACK_SET_SIZE);
  }

  //
  // Exercise mute on the restored format A.
  //
  RunMuteTest (AudioOutput, AudioBuffers, CallbackCtx, PlaybackSizeA, InfoA, &Phase);

Exit:
  for (Index = 0; Index < PLAYBACK_TOTAL_BUFFERS; Index++) {
    if (AudioBuffers[Index] != NULL) {
      FreePool (AudioBuffers[Index]);
    }
  }

  return Status;
}
