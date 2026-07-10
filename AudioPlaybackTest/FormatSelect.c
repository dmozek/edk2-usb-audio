/** @file
  Helpers for selecting an audio format from the Audio Codec Protocol.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "FormatSelect.h"
#include "Menu.h"

/**
  Print a single audio format description.

  @param[in] FormatNumber  The format index this information belongs to.
  @param[in] Info          The format information to print.
  @param[out] String       Allocated description string; caller frees it.
**/
STATIC
VOID
FormatInfoToString (
  IN UINTN                               FormatNumber,
  IN EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info,
  OUT CHAR16                             **String
  )
{
  if ((Info == NULL) || (String == NULL)) {
    return;
  }

  *String = AllocateZeroPool (256 * sizeof (CHAR16));
  if (*String == NULL) {
    return;
  }

  UnicodeSPrint (
    *String,
    sizeof (CHAR16) * 256,
    L"Format %2u: %u ch, %u-bit (subslot %u B), ",
    FormatNumber,
    (UINTN)Info->Channels,
    (UINTN)Info->BitsPerSample,
    (UINTN)Info->SubslotSize
    );

  if (Info->MinSampleRateHz == Info->MaxSampleRateHz) {
    UnicodeSPrint (
      *String + StrLen (*String),
      sizeof (CHAR16) * (256 - StrLen (*String)),
      L"%u Hz",
      Info->MinSampleRateHz
      );
  } else {
    UnicodeSPrint (
      *String + StrLen (*String),
      sizeof (CHAR16) * (256 - StrLen (*String)),
      L"%u..%u Hz (step %u Hz)",
      Info->MinSampleRateHz,
      Info->MaxSampleRateHz,
      Info->SampleRateStepHz
      );
  }
}

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
  )
{
  EFI_STATUS  Status;

  Print (L"\n=== playback on format %u (%u Hz) ===\n", Selection->FormatIndex, Selection->SampleRateHz);
  Status = AudioCodec->SetFormat (AudioCodec, Selection->FormatIndex, Selection->SampleRateHz);
  if (EFI_ERROR (Status)) {
    Print (L"SetFormat failed: %r\n", Status);
  }

  return Status;
}

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
  )
{
  EFI_STATUS                          Status;
  MENU_ITEM                           *Menu;
  UINTN                               MaxFormat;
  UINTN                               FormatNumber;
  UINTN                               SizeOfInfo;
  EFI_AUDIO_CODEC_FORMAT_INFORMATION  *Info;
  UINTN                               ParsedSampleRate;
  UINTN                               MenuSelection;
  UINTN                               MenuItems;
  CHAR16                              InputBuffer[32];
  BOOLEAN                             SampleRateValid;

  Status = EFI_SUCCESS;

  MenuSelection    = 0;
  ParsedSampleRate = 0;

  if ((AudioCodec == NULL) || (Selection == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  gST->ConOut->ClearScreen (gST->ConOut);

  MaxFormat = (AudioCodec->CurrentFormat != NULL) ? AudioCodec->CurrentFormat->MaxFormat : 0;
  Info      = NULL;

  MenuItems = MaxFormat + 1;
  Menu      = AllocateZeroPool (sizeof (MENU_ITEM) * (MenuItems));
  if (Menu == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (FormatNumber = 0; FormatNumber < MaxFormat; FormatNumber++) {
    Status = AudioCodec->QueryFormat (AudioCodec, FormatNumber, &SizeOfInfo, &Info);
    if (EFI_ERROR (Status) || (Info == NULL)) {
      Print (L"  Format %2u: QueryFormat failed: %r\n", FormatNumber, Status);
      PressKeyToContinue ();
      continue;
    }

    FormatInfoToString (FormatNumber, Info, &Menu[FormatNumber].Text);

    FreePool (Info);
    Info = NULL;
  }

  Menu[MaxFormat].Text = L"Cancel";

  while (ParsedSampleRate == 0) {
    SampleRateValid = FALSE;

    HandleMenuInput (Menu, MenuItems, L"Select Audio Format", &MenuSelection);

    if (MenuSelection >= MaxFormat) {
      Status = EFI_ABORTED;
      goto Exit;
    }

    gST->ConOut->ClearScreen (gST->ConOut);

    Info   = NULL;
    Status = AudioCodec->QueryFormat (AudioCodec, MenuSelection, &SizeOfInfo, &Info);
    if (EFI_ERROR (Status) || (Info == NULL)) {
      Print (L"  Format %2u: QueryFormat failed: %r\n", MenuSelection, Status);
      goto Exit;
    }

    while (!SampleRateValid) {
      Print (
        L"Please enter a sample rate in Hz between %u and %u (step %u), or 0 to cancel.\n>: ",
        Info->MinSampleRateHz,
        Info->MaxSampleRateHz,
        Info->SampleRateStepHz
        );

      ReadLine (InputBuffer, sizeof (InputBuffer) / sizeof (CHAR16));
      ParsedSampleRate = StrDecimalToUintn (InputBuffer);

      Print (L"Got %u Hz\n", ParsedSampleRate);

      if (ParsedSampleRate == 0) {
        SampleRateValid = TRUE;
      } else if ((ParsedSampleRate < Info->MinSampleRateHz) ||
                 (ParsedSampleRate > Info->MaxSampleRateHz) ||
                 ((Info->SampleRateStepHz > 0) &&
                  ((ParsedSampleRate - Info->MinSampleRateHz) % Info->SampleRateStepHz != 0)))
      {
        Print (L"Invalid sample rate: %u Hz\n", ParsedSampleRate);
        ParsedSampleRate = 0;
      } else {
        SampleRateValid = TRUE;
      }
    }

    FreePool (Info);
    Info = NULL;
  }

  Selection->FormatIndex  = MenuSelection;
  Selection->SampleRateHz = ParsedSampleRate;
  Selection->Valid        = TRUE;

Exit:
  if (Info != NULL) {
    FreePool (Info);
  }

  if (Menu != NULL) {
    for (FormatNumber = 0; FormatNumber < MaxFormat; FormatNumber++) {
      if (Menu[FormatNumber].Text != NULL) {
        FreePool (Menu[FormatNumber].Text);
      }
    }

    FreePool (Menu);
  }

  return Status;
}
