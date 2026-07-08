/** @file
  Declarations for helpers to parse UAC 1.0 and 2.0 descriptors
  and discover supported audio formats.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Library/BaseLib.h>
#include <Protocol/UsbIo.h>
#include <IndustryStandard/UsbAudio.h>

#include "UsbAudioTypes.h"

typedef struct {
  USB_AUDIO_20_FORMAT_TYPE_I_DESC       *FormatTypeDesc;
  USB_AUDIO_20_AS_GENERAL_DESC          *AsGeneralDesc;
  USB_AUDIO_20_RANGE_UINT32_SUBRANGE    SampleRates;
  BOOLEAN                               FrequencyControlSupported;
  LIST_ENTRY                            ListEntry;
} USB_AUDIO_20_FORMAT_DESC_ENTRY;

#define FORMAT_DESC_20_LIST_CONTAINER(Entry)  BASE_CR(Entry, USB_AUDIO_20_FORMAT_DESC_ENTRY, ListEntry)

typedef struct {
  USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER    *FormatTypeDesc;
  USB_AUDIO_10_AS_GENERAL_DESC              *AsGeneralDesc;
  USB_AUDIO_20_RANGE_UINT32_SUBRANGE        SampleRates;
  BOOLEAN                                   FrequencyControlSupported;
  LIST_ENTRY                                ListEntry;
} USB_AUDIO_10_FORMAT_DESC_ENTRY;

#define FORMAT_DESC_10_LIST_CONTAINER(Entry)  BASE_CR(Entry, USB_AUDIO_10_FORMAT_DESC_ENTRY, ListEntry)

typedef struct {
  USB_AUDIO_INTERNAL_FORMAT_INFO    InternalInfo;
  UINT16                            Offset;
  LIST_ENTRY                        ListEntry;
  LIST_ENTRY                        FormatTypeList;
} USB_AUDIO_STREAMING_INTERFACE;

#define INTERFACE_LIST_CONTAINER(Entry)  BASE_CR(Entry, USB_AUDIO_STREAMING_INTERFACE, ListEntry)

/**
  Discover the supported audio formats for a given USB audio device.

  @param[in]  UsbIo        The audio control USB I/O protocol instance.
  @param[in]  SpecVersion  The USB audio specification version.
  @param[out] NumFormats   The number of supported formats.
  @param[out] Formats      A pointer to the array of supported formats.
                           Must be freed by the caller using FreeFormatInfo().

  @retval EFI_SUCCESS             The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES    Insufficient memory to complete the operation.
  @retval EFI_DEVICE_ERROR        An error occurred while communicating with the USB audio device.
  @retval EFI_UNSUPPORTED         No supported formats were found for the specified specification version.
  @retval EFI_INVALID_PARAMETER   SpecVersion is not UAC 1.0 or UAC 2.0.
  @retval EFI_NOT_FOUND           The USB audio device does not have a valid configuration descriptor.
**/
EFI_STATUS
DiscoverFormats (
  IN  EFI_USB_IO_PROTOCOL     *UsbIo,
  IN  USB_AUDIO_SPEC_VERSION  SpecVersion,
  OUT UINTN                   *NumFormats,
  OUT USB_AUDIO_FORMAT_INFO   **Formats
  );

/**
  Free the memory allocated for the audio format information.

  @param[in] NumFormats   The number of supported formats.
  @param[in] Formats      A pointer to the array of supported formats.
**/
VOID
FreeFormatInfo (
  IN UINTN                  NumFormats,
  IN USB_AUDIO_FORMAT_INFO  *Formats
  );

/**
  Select a specific audio format for the USB audio device.

  @param[in]  ControlUsbIo          The audio control USB I/O protocol instance.
  @param[in]  AgentHandle           The handle of the agent requesting the format selection.
  @param[in]  Formats               The array of supported audio formats.
  @param[in]  SelectedFormatIndex   The index of the format to select.
  @param[in]  SampleRateHz          The desired sample rate in Hz.
  @param[in,out] Dev                The USB audio device context to update with the selected format.

  @retval EFI_SUCCESS           The format was successfully selected.
  @retval EFI_INVALID_PARAMETER The selected format index is out of range or the sample rate is not supported by the selected format.
  @retval EFI_DEVICE_ERROR      An error occurred while stopping the audio stream or configuring the device for the selected format.
  @retval EFI_OUT_OF_RESOURCES  Insufficient resources to complete the operation.
**/
EFI_STATUS
UsbAudioSelectFormat (
  IN EFI_USB_IO_PROTOCOL    *ControlUsbIo,
  IN EFI_HANDLE             AgentHandle,
  IN USB_AUDIO_FORMAT_INFO  *Formats,
  IN UINTN                  SelectedFormatIndex,
  IN UINT32                 SampleRateHz,
  IN OUT USB_AUDIO_DEV      *Dev
  );

/**
  Get the size of the current audio frame in bytes.

  @param[in]  Dev  The USB audio device context.

  @return The size of the current audio frame in bytes, but not less than 1.
**/
UINTN
GetCurrentFrameSize (
  IN USB_AUDIO_DEV  *Dev
  );
