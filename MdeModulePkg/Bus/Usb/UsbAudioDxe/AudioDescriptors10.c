/** @file
  USB Audio Class 1.0 descriptor parsing helpers.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AudioDescriptors.h"
#include <Protocol/AudioOutput.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiUsbLib.h>

/**
  Extract the per-channel Volume/Mute control masks of a given UAC 1.0 Feature
  Unit (bit c set if channel c, 0 = master, exposes the control).

  @param[in]  Buffer         Configuration descriptor blob.
  @param[in]  TotalLength    Length of the configuration descriptor in bytes.
  @param[in]  FeatureUnitId  Feature Unit entity ID (0 = none).
  @param[out] VolumeMask     Volume control mask, 0 if none.
  @param[out] MuteMask       Mute control mask, 0 if none.
**/
VOID
UsbAudio10ExtractFeatureUnitControls (
  IN  UINT8   *Buffer,
  IN  UINT16  TotalLength,
  IN  UINT8   FeatureUnitId,
  OUT UINT32  *VolumeMask,
  OUT UINT32  *MuteMask
  )
{
  UINT16         DescriptorStart;
  USB_DESC_HEAD  *Head;
  UINTN          ControlSize;
  UINTN          NumEntries;
  UINTN          Channel;
  UINT8          Controls;

  *VolumeMask = 0;
  *MuteMask   = 0;

  if (FeatureUnitId == 0) {
    return;
  }

  Head = UsbAudioFindFeatureUnit (
           Buffer,
           TotalLength,
           USB_AUDIO_10_AC_SUBTYPE_FEATURE_UNIT,
           FeatureUnitId,
           sizeof (USB_AUDIO_10_FEATURE_UNIT_DESC_HEADER),
           &DescriptorStart
           );

  if (Head == NULL) {
    return;
  }

  //
  // bmaControls: (NrChannels+1) entries of bControlSize bytes each, starting
  // at offset 6. Entry 0 is the master channel. Bit D0 = Mute, D1 = Volume.
  //
  ControlSize = Buffer[DescriptorStart + 5];
  if ((ControlSize > 0) && (Head->Len >= 7)) {
    NumEntries = ((UINTN)Head->Len - 7) / ControlSize;
    for (Channel = 0; (Channel < NumEntries) && (Channel < USB_AUDIO_MAX_CHANNELS); Channel++) {
      Controls = Buffer[DescriptorStart + 6 + Channel * ControlSize];
      if ((Controls & 0x01) != 0) {
        *MuteMask |= (UINT32)(1u << Channel);
      }

      if ((Controls & 0x02) != 0) {
        *VolumeMask |= (UINT32)(1u << Channel);
      }
    }
  }
}
