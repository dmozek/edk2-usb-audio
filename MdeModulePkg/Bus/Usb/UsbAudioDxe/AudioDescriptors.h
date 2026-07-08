/** @file
  Common USB Audio descriptor iterator and helper declarations.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Protocol/UsbIo.h>
#include <IndustryStandard/UsbAudio.h>
#include "UsbAudioTypes.h"

/**
  Accessors for the bDescriptorSubType and bEntityId fields of Audio Control descriptors.
 **/
#define USB_AUDIO_DESC_SUBTYPE(Head)            (((UINT8 *)(Head))[2])
#define USB_AUDIO_AC_ENTITY_ID(Head)            (((UINT8 *)(Head))[3])
#define USB_AUDIO_FEATURE_UNIT_SOURCE_ID(Head)  (((UINT8 *)(Head))[4])
#define USB_AUDIO_MIXER_UNIT_SOURCE_ID(Head)    (((UINT8 *)(Head))[5])
#define USB_AUDIO_FORMAT_TYPE(Head)             (((UINT8 *)(Head))[3])

/**
  Return the next fully-bounded descriptor in a config blob and advance *Offset
  past it.

  @param[in]      Buffer       Config descriptor blob.
  @param[in]      TotalLength  Config descriptor length in bytes.
  @param[in,out]  Offset       Current offset in; advanced past the descriptor out.

  @retval non-NULL  The descriptor at the entry offset.
  @retval NULL      No further complete descriptor.
**/
USB_DESC_HEAD *
UsbAudioNextDescriptor (
  IN     UINT8   *Buffer,
  IN     UINT16  TotalLength,
  IN OUT UINT16  *Offset
  );

/**
  Return TRUE if the FORMAT_TYPE descriptor for this alternate setting has
  bFormatType == USB_AUDIO_FORMAT_TYPE_I (0x01).

  @param[in] DescriptorBuffer  Full configuration descriptor blob.
  @param[in] TotalLength       Length of DescriptorBuffer in bytes.
  @param[in] InterfaceNumber   AS interface number to inspect.
  @param[in] AltSetting        Alternate setting index to inspect.

  @retval TRUE   At least one FORMAT_TYPE descriptor for the given alt has
                 USB_AUDIO_FORMAT_TYPE_I.
  @retval FALSE  No TYPE_I FORMAT_TYPE descriptor found.
**/
BOOLEAN
UsbAudioIsTypeIAlt (
  IN UINT8   *DescriptorBuffer,
  IN UINT16  TotalLength,
  IN UINT8   InterfaceNumber,
  IN UINT8   AltSetting
  );

/**
  Find the Feature Unit descriptor with the given entity ID in the Audio Control interface.

  @param[in]  Buffer              Configuration descriptor blob.
  @param[in]  TotalLength         Length of the configuration descriptor in bytes.
  @param[in]  FeatureUnitSubtype  USB_AUDIO_{10,20}_AC_SUBTYPE_FEATURE_UNIT.
  @param[in]  FeatureUnitId       Feature Unit entity ID.
  @param[in]  MinLen              Minimum length of the descriptor to match.
  @param[out] DescriptorOffset    Offset of the found descriptor in Buffer.

  @return Pointer to the found Feature Unit descriptor, or NULL if not found.
          The pointer is valid as long as Buffer remains valid.
 **/
USB_DESC_HEAD *
UsbAudioFindFeatureUnit (
  IN  UINT8   *Buffer,
  IN  UINT16  TotalLength,
  IN  UINT8   FeatureUnitSubtype,   /* USB_AUDIO_{10,20}_AC_SUBTYPE_FEATURE_UNIT */
  IN  UINT8   FeatureUnitId,
  IN  UINTN   MinLen,               /* header size gate */
  OUT UINT16  *DescriptorOffset
  );

/**
  Fetch the full configuration descriptor blob into a freshly allocated buffer.
  The caller frees *Buffer with FreePool.

  @param[in]  UsbIo         UsbIo of an interface on the target device.
  @param[out] Buffer        Allocated buffer containing the descriptor blob.
  @param[out] TotalLength   Length of the blob in bytes.

  @retval EFI_SUCCESS           Blob fetched.
  @retval EFI_OUT_OF_RESOURCES  Allocation failed.
  @retval other                 USB request failed.
**/
EFI_STATUS
UsbAudioGetConfigDescriptor (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  OUT UINT8                **Buffer,
  OUT UINT16               *TotalLength
  );

/**
  Find the first AudioStreaming interface that exposes a Type I OUT alternate
  setting.

  @param[in]  Buffer          Configuration descriptor blob.
  @param[in]  TotalLength     Length in bytes.
  @param[out] InterfaceNumber Interface number of the matching AS interface.
  @param[out] InterfaceOffset Offset of the matching interface descriptor in Buffer.
  @param[out] AltSetting      Alternate setting index of the Type I OUT alt.

  @retval TRUE   A Type I OUT AS interface was found.
  @retval FALSE  None found.
**/
BOOLEAN
UsbAudioFindFirstTypeIStream (
  IN  UINT8   *Buffer,
  IN  UINT16  TotalLength,
  OUT UINT8   *InterfaceNumber,
  OUT UINT16  *InterfaceOffset,
  OUT UINT8   *AltSetting
  );

/**
  Find the isochronous OUT endpoint of a given AS interface / alternate setting.

  @param[in]  Buffer           Configuration descriptor blob.
  @param[in]  TotalLength      Length in bytes.
  @param[in]  InterfaceNumber  AS interface number.
  @param[in]  AltSetting       Alternate setting index.
  @param[out] EndpointAddr     Endpoint address (bit 7 clear = OUT).
  @param[out] MaxPacketSize    Endpoint max packet size.
  @param[out] Interval         Endpoint polling interval.

  @retval EFI_SUCCESS    Endpoint found.
  @retval EFI_NOT_FOUND  No isochronous OUT endpoint for that alt.
**/
EFI_STATUS
UsbAudioFindOutEndpoint (
  IN  UINT8   *Buffer,
  IN  UINT16  TotalLength,
  IN  UINT8   InterfaceNumber,
  IN  UINT8   AltSetting,
  OUT UINT8   *EndpointAddr,
  OUT UINT16  *MaxPacketSize,
  OUT UINT8   *Interval
  );

/**
  Find the Feature Unit on the signal path of a streaming interface, given the
  Terminal the interface's endpoint links to.

  @param[in]  Buffer       Configuration descriptor blob.
  @param[in]  TotalLength  Length of the configuration descriptor in bytes.
  @param[in]  TerminalId   bTerminalLink of the streaming interface.

  @return  The Feature Unit entity ID on the stream's path, or 0 if none.
**/
UINT8
UsbAudioFindFeatureUnitForTerminal (
  IN  UINT8   *Buffer,
  IN  UINT16  TotalLength,
  IN  UINT8   TerminalId
  );

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
  );

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
  );

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
  );

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
  );

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
  );
