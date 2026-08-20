/** @file
  Helpers for selecting an audio format from the Audio Codec Protocol.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Protocol/AudioCodec.h>

#include "Playback.h"

/**
  Select an audio format by calling SetFormat on the given Audio Codec protocol.

  @param[in]  AudioCodec   The Audio Codec protocol instance.
  @param[out] Selection    The format selection to apply.

  @retval EFI_SUCCESS      The format was successfully selected.
  @retval other            Some error occurred while selecting the format.
 **/
EFI_STATUS
SelectFormat (
  IN  EFI_AUDIO_CODEC_PROTOCOL  *AudioCodec,
  OUT FORMAT_SELECTION          *Selection
  );

/**
 Show a menu for selecting an audio format.

  @param[in]  AudioCodec   The Audio Codec protocol instance.
  @param[out] Selection    The selected format.

  @retval EFI_SUCCESS      A format was successfully selected.
  @retval EFI_ABORTED      The user cancelled the selection.
  @retval other            Some error occurred while selecting the format.
 **/
EFI_STATUS
ShowFormatSelectMenu (
  IN EFI_AUDIO_CODEC_PROTOCOL  *AudioCodec,
  OUT FORMAT_SELECTION         *Selection
  );
