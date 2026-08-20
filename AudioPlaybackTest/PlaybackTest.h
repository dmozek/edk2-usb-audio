/** @file
  Test patterns to exercise the Audio Output and Audio Codec protocols.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/UefiLib.h>

#include <Protocol/AudioOutput.h>
#include <Protocol/AudioCodec.h>

#include "Playback.h"

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
  );

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
  );
