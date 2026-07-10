/** @file
  Non-interactive playback test for capturing DEBUG output on the console.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/UefiLib.h>

#include <Protocol/AudioOutput.h>
#include <Protocol/AudioCodec.h>

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
  );
