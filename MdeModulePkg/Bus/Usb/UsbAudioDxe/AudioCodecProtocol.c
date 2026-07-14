/** @file
  EFI_AUDIO_CODEC_PROTOCOL member implementations.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbAudio.h"
#include "AudioFormats.h"

/**
  Returns information for an available audio format that the audio device supports.

  The returned buffer is allocated by the callee and must be freed by the caller.

  @param  This                  The EFI_AUDIO_CODEC_PROTOCOL instance.
  @param  FormatNumber          The format number to return information on.
  @param  SizeOfInfo            A pointer to the size, in bytes, of the Info buffer.
  @param  Info                  A pointer to callee allocated buffer that returns information about FormatNumber.

  @retval EFI_SUCCESS           Valid format information was returned.
  @retval EFI_INVALID_PARAMETER FormatNumber is not valid.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.

**/
EFI_STATUS
EFIAPI
QueryFormat (
  IN  EFI_AUDIO_CODEC_PROTOCOL            *This,
  IN  UINTN                               FormatNumber,
  OUT UINTN                               *SizeOfInfo,
  OUT EFI_AUDIO_CODEC_FORMAT_INFORMATION  **Info
  )
{
  USB_AUDIO_DEV  *Dev;

  Dev = USB_AUDIO_DEV_FROM_CODEC_PROTOCOL (This);
  ASSERT (Dev->SupportedFormats != NULL);

  if (FormatNumber >= Dev->SupportedFormatCount) {
    DEBUG ((
      DEBUG_ERROR,
      "UsbAudioDxe: QueryFormat: invalid FormatNumber %u (SupportedFormatCount %u)\n",
      FormatNumber,
      Dev->SupportedFormatCount
      ));
    return EFI_INVALID_PARAMETER;
  }

  *SizeOfInfo = sizeof (EFI_AUDIO_CODEC_FORMAT_INFORMATION);
  *Info       = AllocateCopyPool (
                  sizeof (EFI_AUDIO_CODEC_FORMAT_INFORMATION),
                  &Dev->SupportedFormats[FormatNumber].Info
                  );

  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}

/**
  Set the audio device into the specified format.

  In the process of setting the format, all pending transfers are stopped.
  On failure, the device attempts to restore the previous format selection.

  @param  This              The EFI_AUDIO_CODEC_PROTOCOL instance.
  @param  FormatNumber      The format number to set.
  @param  SampleRateHz      The sample rate to set, in Hz.

  @retval EFI_SUCCESS       The audio format specified by FormatNumber was selected.
  @retval EFI_DEVICE_ERROR  The device had an error and could not complete the request.
  @retval EFI_UNSUPPORTED   FormatNumber or SampleRateHz is not supported by this device.
  @retval EFI_OUT_OF_RESOURCES  Insufficient resources to complete the operation.

**/
EFI_STATUS
EFIAPI
SetFormat (
  IN EFI_AUDIO_CODEC_PROTOCOL  *This,
  IN UINTN                     FormatNumber,
  IN UINT32                    SampleRateHz
  )
{
  USB_AUDIO_DEV                       *Dev;
  EFI_AUDIO_CODEC_FORMAT_INFORMATION  *FormatInfo;
  EFI_STATUS                          Status;
  UINT32                              OldSampleRateHz;

  Dev = USB_AUDIO_DEV_FROM_CODEC_PROTOCOL (This);
  ASSERT (Dev->SupportedFormats != NULL);

  if (FormatNumber >= Dev->SupportedFormatCount) {
    DEBUG ((
      DEBUG_ERROR,
      "UsbAudioDxe: SetFormat: invalid FormatNumber %u (SupportedFormatCount %u)\n",
      FormatNumber,
      Dev->SupportedFormatCount
      ));
    return EFI_UNSUPPORTED;
  }

  FormatInfo = &Dev->SupportedFormats[FormatNumber].Info;
  if ((SampleRateHz < FormatInfo->MinSampleRateHz) ||
      (SampleRateHz > FormatInfo->MaxSampleRateHz) ||
      ((FormatInfo->SampleRateStepHz != 0) &&
       ((SampleRateHz - FormatInfo->MinSampleRateHz) % FormatInfo->SampleRateStepHz != 0)))
  {
    DEBUG ((
      DEBUG_ERROR,
      "UsbAudioDxe: SetFormat: invalid SampleRateHz %u (format supports %u to %u in step %u)\n",
      SampleRateHz,
      FormatInfo->MinSampleRateHz,
      FormatInfo->MaxSampleRateHz,
      FormatInfo->SampleRateStepHz
      ));
    return EFI_UNSUPPORTED;
  }

  OldSampleRateHz = Dev->Stream.DeviceSampleRateHz;

  Status = UsbAudioSelectFormat (
             Dev->AcUsbIo,
             Dev->DriverBindingHandle,
             Dev->SupportedFormats,
             FormatNumber,
             SampleRateHz,
             Dev
             );

  if (EFI_ERROR (Status)) {
    UsbAudioSelectFormat (
      Dev->AcUsbIo,
      Dev->DriverBindingHandle,
      Dev->SupportedFormats,
      Dev->CurrentFormatIndex,
      OldSampleRateHz,
      Dev
      );
  }

  if (Status == EFI_NOT_FOUND) {
    return EFI_DEVICE_ERROR;
  }

  return Status;
}

// ---------------------------------------------------------------------------
// Pre-filled protocol template used by Start()
// ---------------------------------------------------------------------------

EFI_AUDIO_CODEC_PROTOCOL  gAudioCodecProtocolTemplate = {
  QueryFormat,
  SetFormat,
  NULL
};
