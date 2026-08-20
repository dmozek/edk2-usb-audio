/** @file
  An application to exercise the Audio Output and Audio Codec protocols.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/AudioOutput.h>
#include <Protocol/AudioCodec.h>
#include <Protocol/ShellParameters.h>

#include "Menu.h"
#include "PlaybackTest.h"
#include "FormatSelect.h"
#include "NoUiTest.h"

MENU_ITEM  Menu[] = {
  { L"Run single format test" },
  { L"Run two format test"    },
  { L"Play audio file"        },
  { L"Exit"                   }
};

/**
  Execute the two-format test, prompting the user to select two PCM16 stereo formats.

  @param[in] AudioOutput   The Audio Output protocol instance.
  @param[in] AudioCodec    The Audio Codec protocol instance.
 **/
STATIC
VOID
ExecuteTwoFormatTest (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput,
  IN EFI_AUDIO_CODEC_PROTOCOL   *AudioCodec
  )
{
  EFI_STATUS        Status;
  FORMAT_SELECTION  FormatA;
  FORMAT_SELECTION  FormatB;

  Status = ShowFormatSelectMenu (AudioCodec, &FormatA);
  if (EFI_ERROR (Status) || !FormatA.Valid) {
    Print (L"Format selection failed: %r\n", Status);
    PressKeyToContinue ();
    return;
  }

  Status = ShowFormatSelectMenu (AudioCodec, &FormatB);
  if (EFI_ERROR (Status) || !FormatB.Valid) {
    Print (L"Format selection failed: %r\n", Status);
    PressKeyToContinue ();
    return;
  }

  Status = RunTwoFormatTest (AudioOutput, AudioCodec, &FormatA, &FormatB);
  if (EFI_ERROR (Status)) {
    Print (L"RunTwoFormatTest failed: %r\n", Status);
  }

  PressKeyToContinue ();
}

/**
  Execute the single-format test, prompting the user to select a PCM16 stereo format.

  @param[in] AudioOutput   The Audio Output protocol instance.
  @param[in] AudioCodec    The Audio Codec protocol instance.
 **/
STATIC
VOID
ExecuteSingleFormatTest (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput,
  IN EFI_AUDIO_CODEC_PROTOCOL   *AudioCodec
  )
{
  EFI_STATUS        Status;
  FORMAT_SELECTION  Format;

  Status = ShowFormatSelectMenu (AudioCodec, &Format);
  if (EFI_ERROR (Status) || !Format.Valid) {
    Print (L"Format selection failed: %r\n", Status);
    PressKeyToContinue ();
    return;
  }

  Status = RunSingleFormatTest (AudioOutput, AudioCodec, &Format);
  if (EFI_ERROR (Status)) {
    Print (L"RunSingleFormatTest failed: %r\n", Status);
  }

  PressKeyToContinue ();
}

/**
  Play a raw PCM file, prompting the user to select a format and file first.

  @param[in] AudioOutput   The Audio Output protocol instance.
  @param[in] AudioCodec    The Audio Codec protocol instance.
 **/
STATIC
VOID
PlayAudioFile (
  IN EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput,
  IN EFI_AUDIO_CODEC_PROTOCOL   *AudioCodec
  )
{
  EFI_STATUS        Status;
  CHAR16            FileName[128];
  FORMAT_SELECTION  Format;
  UINTN             FrameSize;

  gST->ConOut->ClearScreen (gST->ConOut);

  Print (L"Enter the path to a raw PCM file (max %u characters).\n>: ", sizeof (FileName) / sizeof (CHAR16) - 1);
  ReadLine (FileName, sizeof (FileName) / sizeof (CHAR16));

  Status = ShowFormatSelectMenu (AudioCodec, &Format);
  if (EFI_ERROR (Status) || !Format.Valid) {
    Print (L"Format selection failed: %r\n", Status);
    PressKeyToContinue ();
    return;
  }

  Status = SelectFormat (AudioCodec, &Format);
  if (EFI_ERROR (Status)) {
    Print (L"SelectFormat failed: %r\n", Status);
    PressKeyToContinue ();
    return;
  }

  AudioOutput->SetMute (AudioOutput, FALSE);
  SetVolumeFraction (AudioOutput, 3, 4, L"playback");

  FrameSize = 1;
  if ((AudioCodec->CurrentFormat != NULL) && (AudioCodec->CurrentFormat->Info != NULL)) {
    FrameSize = (UINTN)AudioCodec->CurrentFormat->Info->Channels * AudioCodec->CurrentFormat->Info->SubslotSize;
  }

  Status = PlayRawPcmFile (AudioOutput, FileName, FrameSize);
  if (EFI_ERROR (Status)) {
    Print (L"PlayRawPcmFile failed: %r\n", Status);
  }

  PressKeyToContinue ();
}

/**
  Check whether the application was invoked with the --no-ui flag.

  @param[in] ImageHandle  The image handle, used to obtain the shell
                          parameters protocol.

  @retval TRUE   --no-ui was passed on the command line.
  @retval FALSE  No such flag, or not launched from the shell.
**/
STATIC
BOOLEAN
HasNoUiFlag (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                     Status;
  EFI_SHELL_PARAMETERS_PROTOCOL  *ShellParameters;
  UINTN                          Index;

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiShellParametersProtocolGuid,
                  (VOID **)&ShellParameters
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  for (Index = 1; Index < ShellParameters->Argc; Index++) {
    if (StrCmp (ShellParameters->Argv[Index], L"--no-ui") == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Display a menu to select a test to run, and execute the selected test.

  With --no-ui on the command line, instead run a non-interactive playback
  test that never reads the keyboard or clears the screen, so DEBUG output
  written to the console can be captured alongside the test's own output.

  @param   ImageHandle     The image handle.
  @param   SystemTable     The system table.

  @retval  EFI_SUCCESS              The application exited successfully.
  @retval  EFI_INVALID_PARAMETER    The user selected an invalid menu option.
  @retval  other                    Some error occurred while opening the audio protocols.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                 Status;
  BOOLEAN                    Exit;
  EFI_AUDIO_OUTPUT_PROTOCOL  *AudioOutput;
  EFI_AUDIO_CODEC_PROTOCOL   *AudioCodec;

  Exit = FALSE;

  Status = OpenAudioProtocols (&AudioOutput, &AudioCodec);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to open audio protocols: %r\n", Status);
    return Status;
  }

  if (HasNoUiFlag (ImageHandle)) {
    return RunNoUiTest (AudioOutput, AudioCodec);
  }

  while (!Exit) {
    UINTN  SelectedIndex;
    HandleMenuInput (Menu, ARRAY_SIZE (Menu), L"USB Audio Isochronous Test", &SelectedIndex);
    if (SelectedIndex == ARRAY_SIZE (Menu)) {
      break;
    }

    switch (SelectedIndex) {
      case 0:
        ExecuteSingleFormatTest (AudioOutput, AudioCodec);
        break;
      case 1:
        ExecuteTwoFormatTest (AudioOutput, AudioCodec);
        break;
      case 2:
        PlayAudioFile (AudioOutput, AudioCodec);
        break;
      case 3:
        Exit = TRUE;
        break;
      default:
        Print (L"Invalid selection: %u\n", SelectedIndex);
        Status = EFI_INVALID_PARAMETER;
        break;
    }

    if (EFI_ERROR (Status)) {
      Print (L"Error: %r\n", Status);
    }
  }

  return Status;
}
