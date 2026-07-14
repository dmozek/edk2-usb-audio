/** @file
  EDK II Audio Codec Protocol

  The protocol enables the querying and changing of used and available formats
  for an audio device.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>

#define EDKII_AUDIO_CODEC_PROTOCOL_GUID \
  { 0x2b449a3e, 0xfca9, 0x47ed, \
    { 0x8e, 0x71, 0x6f, 0xc2, 0x64, 0x0e, 0x4b, 0xfe } }

extern EFI_GUID  gEdkiiAudioCodecProtocolGuid;

typedef struct _EFI_AUDIO_CODEC_PROTOCOL EFI_AUDIO_CODEC_PROTOCOL;

typedef struct {
  ///
  /// The version of this data structure. A value of zero represents the
  /// EFI_AUDIO_CODEC_FORMAT_INFORMATION structure as defined in this header.
  ///
  UINT32    Version;
  ///
  /// Minimum sample rate supported by the current alternate setting, in Hz.
  ///
  UINT32    MinSampleRateHz;
  ///
  /// Maximum sample rate supported by the current alternate setting, in Hz.
  ///
  UINT32    MaxSampleRateHz;
  ///
  /// Sample rate step size for the current alternate setting, in Hz.  The
  /// supported sample rates are all values in the range [MinSampleRateHz,
  /// MaxSampleRateHz] that are equal to MinSampleRateHz modulo SampleRateStepHz.
  /// A step of zero indicates that only the minimum sample rate is supported.
  ///
  UINT32    SampleRateStepHz;
  ///
  /// The number of audio channels supported by the current alternate setting.
  ///
  UINT8     Channels;
  ///
  /// The number of bits per sample supported by the current alternate setting.
  ///
  UINT8     BitsPerSample;
  ///
  /// The number of bytes per sample subslot.
  ///
  UINT8     SubslotSize;
  ///
  /// Reserved for future use. Must be zero.
  ///
  UINT8     Reserved;
} EFI_AUDIO_CODEC_FORMAT_INFORMATION;

/**
  Returns information for an available audio format that the audio device supports.

  The returned buffer is allocated by the callee and must be freed by the caller.

  @param  This                  The EFI_AUDIO_CODEC_PROTOCOL instance.
  @param  FormatNumber          The format number to return information on.
  @param  SizeOfInfo            A pointer to the size, in bytes, of the Info buffer.
  @param  Info                  A pointer to callee allocated buffer that returns information about FormatNumber.
                                Must be freed by the caller.

  @retval EFI_SUCCESS           Valid format information was returned.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to retrieve the audio format.
  @retval EFI_INVALID_PARAMETER FormatNumber is not valid.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_CODEC_PROTOCOL_QUERY_FORMAT)(
  IN  EFI_AUDIO_CODEC_PROTOCOL           *This,
  IN  UINTN                               FormatNumber,
  OUT UINTN                               *SizeOfInfo,
  OUT EFI_AUDIO_CODEC_FORMAT_INFORMATION **Info
  );

/**
  Set the audio device into the specified format.

  Audio playback is cancelled.

  @param  This              The EFI_AUDIO_CODEC_PROTOCOL instance.
  @param  FormatNumber      The format number to set.
  @param  SampleRateHz      The sample rate to set, in Hz.

  @retval EFI_SUCCESS             The audio format specified by FormatNumber was selected.
  @retval EFI_DEVICE_ERROR        The device had an error and could not complete the request.
  @retval EFI_UNSUPPORTED         FormatNumber or SampleRateHz is not supported by this device.
  @retval EFI_OUT_OF_RESOURCES    Insufficient resources to complete the operation.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_AUDIO_CODEC_PROTOCOL_SET_FORMAT)(
  IN EFI_AUDIO_CODEC_PROTOCOL    *This,
  IN UINTN                        FormatNumber,
  IN UINT32                       SampleRateHz
  );

typedef struct {
  ///
  /// The number of formats supported by QueryFormat() and SetFormat().
  ///
  UINT32                                      MaxFormat;
  ///
  /// Current Format of the audio device. Valid format numbers are 0 to MaxFormat - 1.
  ///
  UINT32                                      Format;
  ///
  /// Pointer to read-only EFI_AUDIO_CODEC_FORMAT_INFORMATION data.
  ///
  CONST EFI_AUDIO_CODEC_FORMAT_INFORMATION    *Info;
  ///
  /// Size of Info structure in bytes.
  ///
  UINTN                                       SizeOfInfo;
} EFI_AUDIO_CODEC_PROTOCOL_FORMAT;

///
/// Provides a basic abstraction to set audio formats.
///
struct _EFI_AUDIO_CODEC_PROTOCOL {
  EFI_AUDIO_CODEC_PROTOCOL_QUERY_FORMAT    QueryFormat;
  EFI_AUDIO_CODEC_PROTOCOL_SET_FORMAT      SetFormat;
  ///
  /// Pointer to EFI_AUDIO_CODEC_PROTOCOL_FORMAT data.
  ///
  CONST EFI_AUDIO_CODEC_PROTOCOL_FORMAT    *CurrentFormat;
};
