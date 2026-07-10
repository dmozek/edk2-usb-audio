/** @file
  Non-interactive playback test for capturing DEBUG output on the console.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "NoUiTest.h"
#include "Playback.h"
#include "FormatSelect.h"

/** Sample rate to prefer when the device supports it. **/
#define NOUI_PREFERRED_RATE_HZ  48000U

/** How many times a full set of buffers is queued and drained. **/
#define NOUI_PLAYBACK_PASSES  3U

/**
  Check whether a format supports the given sample rate.

  @param[in] Info    Format information.
  @param[in] RateHz  Sample rate to check.

  @retval TRUE   RateHz is supported by the format.
  @retval FALSE  RateHz is not supported.
**/
STATIC
BOOLEAN
FormatSupportsRate (
  IN EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info,
  IN UINT32                              RateHz
  )
{
  if ((RateHz < Info->MinSampleRateHz) || (RateHz > Info->MaxSampleRateHz)) {
    return FALSE;
  }

  if (Info->SampleRateStepHz == 0) {
    return RateHz == Info->MinSampleRateHz;
  }

  return ((RateHz - Info->MinSampleRateHz) % Info->SampleRateStepHz) == 0;
}

/**
  List all formats the device reports and pick one: the first format that
  supports NOUI_PREFERRED_RATE_HZ, or format 0 at its minimum rate.

  @param[in]  AudioCodec   The Audio Codec protocol instance.
  @param[out] Selection    The chosen format index and sample rate.
  @param[out] Info         Format information of the chosen format; the
                           caller frees it with FreePool.

  @retval EFI_SUCCESS    A format was chosen.
  @retval EFI_NOT_FOUND  The device reports no usable formats.
  @retval other          QueryFormat failed for the chosen format.
**/
STATIC
EFI_STATUS
ChooseFormat (
  IN  EFI_AUDIO_CODEC_PROTOCOL            *AudioCodec,
  OUT FORMAT_SELECTION                    *Selection,
  OUT EFI_AUDIO_CODEC_FORMAT_INFORMATION  **Info
  )
{
  EFI_STATUS                          Status;
  UINTN                               MaxFormat;
  UINTN                               FormatNumber;
  UINTN                               SizeOfInfo;
  EFI_AUDIO_CODEC_FORMAT_INFORMATION  *FormatInfo;
  BOOLEAN                             Chosen;

  MaxFormat = (AudioCodec->CurrentFormat != NULL) ? AudioCodec->CurrentFormat->MaxFormat : 0;
  if (MaxFormat == 0) {
    Print (L"NoUi: device reports no formats\n");
    return EFI_NOT_FOUND;
  }

  *Info  = NULL;
  Chosen = FALSE;

  Print (L"NoUi: device reports %u format(s):\n", MaxFormat);

  for (FormatNumber = 0; FormatNumber < MaxFormat; FormatNumber++) {
    FormatInfo = NULL;
    Status     = AudioCodec->QueryFormat (AudioCodec, FormatNumber, &SizeOfInfo, &FormatInfo);
    if (EFI_ERROR (Status) || (FormatInfo == NULL)) {
      Print (L"NoUi:   format %2u: QueryFormat failed: %r\n", FormatNumber, Status);
      continue;
    }

    Print (
      L"NoUi:   format %2u: %u ch, %u-bit (subslot %u B), %u..%u Hz (step %u)\n",
      FormatNumber,
      (UINTN)FormatInfo->Channels,
      (UINTN)FormatInfo->BitsPerSample,
      (UINTN)FormatInfo->SubslotSize,
      FormatInfo->MinSampleRateHz,
      FormatInfo->MaxSampleRateHz,
      FormatInfo->SampleRateStepHz
      );

    if (!Chosen && FormatSupportsRate (FormatInfo, NOUI_PREFERRED_RATE_HZ)) {
      Selection->FormatIndex  = FormatNumber;
      Selection->SampleRateHz = NOUI_PREFERRED_RATE_HZ;
      Selection->Valid        = TRUE;
      *Info                   = FormatInfo;
      Chosen                  = TRUE;
      continue;
    }

    FreePool (FormatInfo);
  }

  if (!Chosen) {
    //
    // Fall back to format 0 at its minimum rate.
    //
    Status = AudioCodec->QueryFormat (AudioCodec, 0, &SizeOfInfo, Info);
    if (EFI_ERROR (Status) || (*Info == NULL)) {
      Print (L"NoUi: QueryFormat for fallback format 0 failed: %r\n", Status);
      return EFI_ERROR (Status) ? Status : EFI_NOT_FOUND;
    }

    Selection->FormatIndex  = 0;
    Selection->SampleRateHz = (*Info)->MinSampleRateHz;
    Selection->Valid        = TRUE;
  }

  Print (
    L"NoUi: selected format %u at %u Hz\n",
    Selection->FormatIndex,
    Selection->SampleRateHz
    );

  return EFI_SUCCESS;
}

/**
  Run the non-interactive playback test: select a format and play a few
  buffer sets, without reading the keyboard or drawing any UI, so that
  interleaved DEBUG output on the console is left undisturbed.

  Prefers a format supporting 48000 Hz; falls back to format 0 at its
  minimum rate. The signal buffers are generated to match the selected
  format's channel count and subslot size.

  @param[in] AudioOutput   The Audio Output protocol instance.
  @param[in] AudioCodec    The Audio Codec protocol instance.

  @retval EFI_SUCCESS       All queued buffers completed successfully.
  @retval EFI_DEVICE_ERROR  One or more buffers failed or timed out.
  @retval other             Format selection or setup failed.
**/
EFI_STATUS
RunNoUiTest (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput,
  IN EFI_AUDIO_CODEC_PROTOCOL   *AudioCodec
  )
{
  EFI_STATUS                          Status;
  FORMAT_SELECTION                    Selection;
  EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info;
  UINT8                               *AudioBuffers[PLAYBACK_SET_SIZE];
  ISOCH_CALLBACK_CONTEXT              CallbackCtx[PLAYBACK_SET_SIZE];
  UINTN                               Index;
  UINTN                               PlaybackSize;
  UINTN                               Pass;
  UINTN                               Succeeded;
  UINTN                               TotalSucceeded;
  UINT32                              Phase;

  Print (L"NoUi: starting non-interactive playback test\n");

  Info  = NULL;
  Phase = 0;

  for (Index = 0; Index < PLAYBACK_SET_SIZE; Index++) {
    AudioBuffers[Index] = NULL;
  }

  Status = ChooseFormat (AudioCodec, &Selection, &Info);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SelectFormat (AudioCodec, &Selection);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = AudioOutput->SetMute (AudioOutput, FALSE);
  Print (L"NoUi: SetMute(FALSE): %r\n", Status);

  SetVolumeFraction (AudioOutput, 1, 2, L"no-ui");

  //
  // One second of audio per buffer, aligned to the frame size of the
  // selected format (QueueAudio requires a multiple of the frame size).
  //
  PlaybackSize = ComputePlaybackSize (Selection.SampleRateHz, Info);

  Print (
    L"NoUi: frame size %u B, %u B per buffer, %u buffers per pass, %u passes\n",
    (UINTN)Info->Channels * Info->SubslotSize,
    PlaybackSize,
    PLAYBACK_SET_SIZE,
    NOUI_PLAYBACK_PASSES
    );

  for (Index = 0; Index < PLAYBACK_SET_SIZE; Index++) {
    AudioBuffers[Index] = AllocateZeroPool (PLAYBACK_MAX_BUFFER_SIZE);
    if (AudioBuffers[Index] == NULL) {
      Print (L"NoUi: failed to allocate audio buffer %u\n", Index);
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }
  }

  TotalSucceeded = 0;
  for (Pass = 0; Pass < NOUI_PLAYBACK_PASSES; Pass++) {
    Print (L"NoUi: === pass %u of %u ===\n", Pass + 1, NOUI_PLAYBACK_PASSES);
    QueuePlaybackSet (
      AudioOutput,
      AudioBuffers,
      CallbackCtx,
      PLAYBACK_SET_SIZE,
      PlaybackSize,
      Info,
      &Phase
      );
    WaitPlaybackSet (CallbackCtx, PLAYBACK_SET_SIZE);
    Succeeded = CountWithStatus (CallbackCtx, PLAYBACK_SET_SIZE, EFI_SUCCESS);
    Print (L"NoUi: pass %u: %u of %u buffers succeeded\n", Pass + 1, Succeeded, PLAYBACK_SET_SIZE);
    TotalSucceeded += Succeeded;
  }

  AudioOutput->StopAudio (AudioOutput);

  if (TotalSucceeded == NOUI_PLAYBACK_PASSES * PLAYBACK_SET_SIZE) {
    Print (L"NoUi: PASS: all %u buffers completed successfully\n", TotalSucceeded);
    Status = EFI_SUCCESS;
  } else {
    Print (
      L"NoUi: FAIL: only %u of %u buffers completed successfully\n",
      TotalSucceeded,
      (UINTN)(NOUI_PLAYBACK_PASSES * PLAYBACK_SET_SIZE)
      );
    Status = EFI_DEVICE_ERROR;
  }

Exit:
  for (Index = 0; Index < PLAYBACK_SET_SIZE; Index++) {
    if (AudioBuffers[Index] != NULL) {
      FreePool (AudioBuffers[Index]);
    }
  }

  if (Info != NULL) {
    FreePool (Info);
  }

  return Status;
}
