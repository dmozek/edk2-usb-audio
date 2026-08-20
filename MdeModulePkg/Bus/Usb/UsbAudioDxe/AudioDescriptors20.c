/** @file
  USB Audio Class 2.0 descriptor parsing helpers.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AudioDescriptors.h"
#include <Protocol/AudioOutput.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiUsbLib.h>

/**
  Extract the per-channel Volume/Mute control masks of a given UAC 2.0 Feature
  Unit (bit c set if channel c, 0 = master, exposes the control).

  @param[in]  Buffer         Configuration descriptor blob.
  @param[in]  TotalLength    Length of the configuration descriptor in bytes.
  @param[in]  FeatureUnitId  Feature Unit entity ID (0 = none).
  @param[out] VolumeMask     Volume control mask, 0 if none.
  @param[out] MuteMask       Mute control mask, 0 if none.
**/
VOID
UsbAudio20ExtractFeatureUnitControls (
  IN  UINT8   *Buffer,
  IN  UINT16  TotalLength,
  IN  UINT8   FeatureUnitId,
  OUT UINT32  *VolumeMask,
  OUT UINT32  *MuteMask
  )
{
  UINT16         DescriptorStart;
  USB_DESC_HEAD  *Head;
  UINTN          NumEntries;
  UINTN          Channel;
  UINT32         Controls;

  *VolumeMask = 0;
  *MuteMask   = 0;

  if (FeatureUnitId == 0) {
    return;
  }

  Head = UsbAudioFindFeatureUnit (
           Buffer,
           TotalLength,
           USB_AUDIO_20_AC_SUBTYPE_FEATURE_UNIT,
           FeatureUnitId,
           sizeof (USB_AUDIO_20_FEATURE_UNIT_DESC_HEADER),
           &DescriptorStart
           );

  if (Head == NULL) {
    return;
  }

  //
  // bmaControls: (NrChannels+1) UINT32 entries starting at offset 5 (after
  // the 5-byte header), followed by a 1-byte iFeature. Entry 0 is the master
  // channel. Each control is a 2-bit field: bits D1:D0 = Mute, D3:D2 = Volume
  // (00 = none, 01 = read-only, 11 = host programmable). A non-zero field
  // means the control is present.
  //
  if (Head->Len >= 6) {
    NumEntries = ((UINTN)Head->Len - 6) / sizeof (UINT32);
    for (Channel = 0; (Channel < NumEntries) && (Channel < USB_AUDIO_MAX_CHANNELS); Channel++) {
      Controls = (UINT32)(Buffer[DescriptorStart + 5 + Channel * 4]) |
                 ((UINT32)Buffer[DescriptorStart + 5 + Channel * 4 + 1] << 8) |
                 ((UINT32)Buffer[DescriptorStart + 5 + Channel * 4 + 2] << 16) |
                 ((UINT32)Buffer[DescriptorStart + 5 + Channel * 4 + 3] << 24);

      if ((Controls & 0x03) != 0) {
        *MuteMask |= (UINT32)(1u << Channel);
      }

      if (((Controls >> 2) & 0x03) != 0) {
        *VolumeMask |= (UINT32)(1u << Channel);
      }
    }
  }
}

/**
  Find all Clock Source descriptors in the Audio Control interface and append
  them to a linked list of USB_AUDIO_20_CLOCK_SOURCE_ENTRY structures.

  @param[in]  Buffer       Configuration descriptor blob.
  @param[in]  TotalLength  Length of the configuration descriptor in bytes.
  @param[in]  AcInterface  The interface number of the Audio Control interface.
  @param[in,out] ClockSources Pointer to the head of the linked list the clock sources
                              will be appended to. The list must be initialized before calling this function.

  @retval EFI_SUCCESS           Clock sources enumerated successfully.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
**/
EFI_STATUS
UsbAudio20EnumerateClockSources (
  IN  UINT8                               *Buffer,
  IN  UINT16                              TotalLength,
  IN  UINT8                               AcInterface,
  IN OUT USB_AUDIO_20_CLOCK_SOURCE_ENTRY  *ClockSources
  )
{
  UINT16                           Offset;
  USB_DESC_HEAD                    *Head;
  USB_INTERFACE_DESCRIPTOR         *IfDesc;
  BOOLEAN                          InAc;
  UINT8                            SubType;
  USB_AUDIO_20_CLOCK_SOURCE_ENTRY  *NewListEntry;
  USB_AUDIO_20_CLOCK_SOURCE_DESC   *ClockSourceDesc;

  Offset = 0;
  InAc   = FALSE;

  while ((Head = UsbAudioNextDescriptor (Buffer, TotalLength, &Offset)) != NULL) {
    if ((Head->Type == USB_DESC_TYPE_INTERFACE) &&
        (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR)))
    {
      IfDesc = (USB_INTERFACE_DESCRIPTOR *)Head;
      InAc   = (BOOLEAN)((IfDesc->InterfaceClass == USB_AUDIO_CLASS) &&
                         (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_CONTROL) &&
                         (IfDesc->InterfaceNumber == AcInterface));
    } else if (InAc && (Head->Type == USB_AUDIO_CS_INTERFACE) && (Head->Len >= 4)) {
      SubType = USB_AUDIO_DESC_SUBTYPE (Head);
      if ((SubType == USB_AUDIO_20_AC_SUBTYPE_CLOCK_SOURCE) &&
          (Head->Len >= sizeof (USB_AUDIO_20_CLOCK_SOURCE_DESC)))
      {
        NewListEntry = AllocatePool (sizeof (USB_AUDIO_20_CLOCK_SOURCE_ENTRY));
        if (NewListEntry == NULL) {
          FreeClockSourceList (&ClockSources->ListEntry);
          return EFI_OUT_OF_RESOURCES;
        }

        ClockSourceDesc          = (USB_AUDIO_20_CLOCK_SOURCE_DESC *)Head;
        NewListEntry->Id         = ClockSourceDesc->ClockId;
        NewListEntry->Attributes = ClockSourceDesc->Attributes;
        NewListEntry->Controls   = ClockSourceDesc->Controls;
        NewListEntry->NumRanges  = 0;
        NewListEntry->Subranges  = NULL;

        InsertTailList (&ClockSources->ListEntry, &NewListEntry->ListEntry);
      }
    }
  }

  return EFI_SUCCESS;
}

//
// Maximum number of chained clock entities (Clock Selector / Clock Multiplier)
// to follow before giving up, guarding against reference cycles in a
// malformed descriptor blob.
//
#define USB_AUDIO_20_MAX_CLOCK_CHAIN_DEPTH  8

/**
  Find the clock entity descriptor (Clock Source, Clock Selector or Clock
  Multiplier) with the given entity ID in the Audio Control interface.

  @param[in] Buffer       Configuration descriptor blob.
  @param[in] TotalLength  Length of the configuration descriptor in bytes.
  @param[in] AcInterface  The interface number of the Audio Control interface.
  @param[in] ClockId      The clock entity ID to find.

  @return Pointer to the found clock entity descriptor, or NULL if not found.
**/
STATIC
USB_DESC_HEAD *
UsbAudio20FindClockEntity (
  IN UINT8   *Buffer,
  IN UINT16  TotalLength,
  IN UINT8   AcInterface,
  IN UINT8   ClockId
  )
{
  UINT16                    Offset;
  USB_DESC_HEAD             *Head;
  USB_INTERFACE_DESCRIPTOR  *IfDesc;
  BOOLEAN                   InAc;
  UINT8                     SubType;

  Offset = 0;
  InAc   = FALSE;

  while ((Head = UsbAudioNextDescriptor (Buffer, TotalLength, &Offset)) != NULL) {
    if ((Head->Type == USB_DESC_TYPE_INTERFACE) &&
        (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR)))
    {
      IfDesc = (USB_INTERFACE_DESCRIPTOR *)Head;
      InAc   = (BOOLEAN)((IfDesc->InterfaceClass == USB_AUDIO_CLASS) &&
                         (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_CONTROL) &&
                         (IfDesc->InterfaceNumber == AcInterface));
    } else if (InAc && (Head->Type == USB_AUDIO_CS_INTERFACE) && (Head->Len >= 4)) {
      SubType = USB_AUDIO_DESC_SUBTYPE (Head);
      if (((SubType == USB_AUDIO_20_AC_SUBTYPE_CLOCK_SOURCE) ||
           (SubType == USB_AUDIO_20_AC_SUBTYPE_CLOCK_SELECTOR) ||
           (SubType == USB_AUDIO_20_AC_SUBTYPE_CLOCK_MULTIPLIER)) &&
          (USB_AUDIO_AC_ENTITY_ID (Head) == ClockId))
      {
        return Head;
      }
    }
  }

  return NULL;
}

/**
  Recursive worker for UsbAudio20ResolveClockSourceId, bounded by Depth.

  @param[in] Buffer         Configuration descriptor blob.
  @param[in] TotalLength    Length of the configuration descriptor in bytes.
  @param[in] AcInterface    The interface number of the Audio Control interface.
  @param[in] ClockEntityId  The clock entity ID to resolve.
  @param[in] Depth          Remaining chain hops before giving up.

  @return The Clock Source entity ID the chain resolves to, or 0 if none.
**/
STATIC
UINT8
UsbAudio20ResolveClockChain (
  IN UINT8   *Buffer,
  IN UINT16  TotalLength,
  IN UINT8   AcInterface,
  IN UINT8   ClockEntityId,
  IN UINTN   Depth
  )
{
  USB_DESC_HEAD                            *Head;
  USB_AUDIO_20_CLOCK_SELECTOR_DESC_HEADER  *Selector;
  UINT8                                    Pin;
  UINT8                                    Resolved;

  if (Depth == 0) {
    return 0;
  }

  Head = UsbAudio20FindClockEntity (Buffer, TotalLength, AcInterface, ClockEntityId);
  if (Head == NULL) {
    return 0;
  }

  switch (USB_AUDIO_DESC_SUBTYPE (Head)) {
    case USB_AUDIO_20_AC_SUBTYPE_CLOCK_SOURCE:
      return ClockEntityId;

    case USB_AUDIO_20_AC_SUBTYPE_CLOCK_SELECTOR:
      Selector = (USB_AUDIO_20_CLOCK_SELECTOR_DESC_HEADER *)Head;
      if (Head->Len < sizeof (USB_AUDIO_20_CLOCK_SELECTOR_DESC_HEADER) - 1 + Selector->NumInputPins) {
        return 0;
      }

      for (Pin = 0; Pin < Selector->NumInputPins; Pin++) {
        Resolved = UsbAudio20ResolveClockChain (
                     Buffer,
                     TotalLength,
                     AcInterface,
                     Selector->ClockSourceIds[Pin],
                     Depth - 1
                     );
        if (Resolved != 0) {
          return Resolved;
        }
      }

      return 0;

    case USB_AUDIO_20_AC_SUBTYPE_CLOCK_MULTIPLIER:
      if (Head->Len < sizeof (USB_AUDIO_20_CLOCK_MULTIPLIER_DESC)) {
        return 0;
      }

      return UsbAudio20ResolveClockChain (
               Buffer,
               TotalLength,
               AcInterface,
               ((USB_AUDIO_20_CLOCK_MULTIPLIER_DESC *)Head)->ClockSourceId,
               Depth - 1
               );

    default:
      return 0;
  }
}

/**
  Resolve a clock entity ID to the Clock Source entity it is ultimately
  derived from, following Clock Selector and Clock Multiplier entities.

  A terminal's bCSourceID may reference a Clock Selector or Clock Multiplier
  rather than a Clock Source directly. For a Clock Selector each input pin is
  tried in descriptor order and the first pin that resolves wins.

  @param[in] Buffer         Configuration descriptor blob.
  @param[in] TotalLength    Length of the configuration descriptor in bytes.
  @param[in] AcInterface    The interface number of the Audio Control interface.
  @param[in] ClockEntityId  The clock entity ID to resolve (a terminal's bCSourceID).

  @return The Clock Source entity ID the chain resolves to, or 0 if the chain
          does not end at a Clock Source.
**/
UINT8
UsbAudio20ResolveClockSourceId (
  IN UINT8   *Buffer,
  IN UINT16  TotalLength,
  IN UINT8   AcInterface,
  IN UINT8   ClockEntityId
  )
{
  return UsbAudio20ResolveClockChain (
           Buffer,
           TotalLength,
           AcInterface,
           ClockEntityId,
           USB_AUDIO_20_MAX_CLOCK_CHAIN_DEPTH
           );
}

/**
  Find the Terminal descriptor with the given entity ID in the Audio Control interface.

  @param[in]  Buffer          Configuration descriptor blob.
  @param[in]  TotalLength     Length of the configuration descriptor in bytes.
  @param[in]  TerminalId      The entity ID of the terminal to find.
  @param[in]  AcInterface     The interface number of the Audio Control interface.
  @param[out] TerminalDesc    Pointer to the found terminal descriptor, or NULL if not found.
  @param[out] IsInputTerminal TRUE if the found terminal is an Input Terminal, FALSE if it is an Output Terminal.

  @retval EFI_SUCCESS           Terminal found.
  @retval EFI_NOT_FOUND         Terminal with the given ID not found.
**/
EFI_STATUS
UsbAudio20GetTerminalById (
  IN  UINT8    *Buffer,
  IN  UINT16   TotalLength,
  IN  UINT8    TerminalId,
  IN  UINT8    AcInterface,
  OUT VOID     **TerminalDesc,
  OUT BOOLEAN  *IsInputTerminal
  )
{
  UINT16                    Offset;
  USB_DESC_HEAD             *Head;
  USB_INTERFACE_DESCRIPTOR  *IfDesc;
  BOOLEAN                   InAc;

  Offset           = 0;
  *IsInputTerminal = FALSE;
  InAc             = FALSE;

  while ((Head = UsbAudioNextDescriptor (Buffer, TotalLength, &Offset)) != NULL) {
    if ((Head->Type == USB_DESC_TYPE_INTERFACE) &&
        (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR)))
    {
      IfDesc = (USB_INTERFACE_DESCRIPTOR *)Head;
      InAc   = (BOOLEAN)((IfDesc->InterfaceClass == USB_AUDIO_CLASS) &&
                         (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_CONTROL) &&
                         (IfDesc->InterfaceNumber == AcInterface));
    } else if (InAc && (Head->Type == USB_AUDIO_CS_INTERFACE)) {
      if ((Head->Len >= sizeof (USB_AUDIO_20_OUTPUT_TERMINAL_DESC)) &&
          (USB_AUDIO_DESC_SUBTYPE (Head) == USB_AUDIO_20_AC_SUBTYPE_OUTPUT_TERMINAL))
      {
        if (((USB_AUDIO_20_OUTPUT_TERMINAL_DESC *)Head)->TerminalId == TerminalId) {
          *TerminalDesc    = (USB_AUDIO_20_OUTPUT_TERMINAL_DESC *)Head;
          *IsInputTerminal = FALSE;
          return EFI_SUCCESS;
        }
      } else if ((Head->Len >= sizeof (USB_AUDIO_20_INPUT_TERMINAL_DESC)) &&
                 (USB_AUDIO_DESC_SUBTYPE (Head) == USB_AUDIO_20_AC_SUBTYPE_INPUT_TERMINAL))
      {
        if (((USB_AUDIO_20_INPUT_TERMINAL_DESC *)Head)->TerminalId == TerminalId) {
          *TerminalDesc    = (USB_AUDIO_20_INPUT_TERMINAL_DESC *)Head;
          *IsInputTerminal = TRUE;
          return EFI_SUCCESS;
        }
      }
    }
  }

  *TerminalDesc = NULL;

  return EFI_NOT_FOUND;
}
