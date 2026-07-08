/** @file
  Common USB Audio descriptor iterator helpers.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AudioDescriptors.h"
#include <Protocol/AudioOutput.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiUsbLib.h>

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
  )
{
  USB_DESC_HEAD  *Head;

  if ((UINTN)*Offset + sizeof (USB_DESC_HEAD) > TotalLength) {
    return NULL;
  }

  Head = (USB_DESC_HEAD *)(Buffer + *Offset);
  if ((Head->Len < sizeof (USB_DESC_HEAD)) ||
      ((UINTN)*Offset + Head->Len > TotalLength))
  {
    return NULL;
  }

  *Offset = (UINT16)(*Offset + Head->Len);
  return Head;
}

/**
  Return TRUE if the given AudioStreaming alternate setting carries a
  FORMAT_TYPE descriptor with bFormatType == USB_AUDIO_FORMAT_TYPE_I.

  The check is version-agnostic: the FORMAT_TYPE subtype (0x02) and the
  bFormatType byte (offset 3) are identical in UAC 1.0 and UAC 2.0.

  @param[in] DescriptorBuffer  Configuration descriptor blob.
  @param[in] TotalLength       Length in bytes.
  @param[in] InterfaceNumber   AS interface to inspect.
  @param[in] AltSetting        Alternate setting to inspect.

  @retval TRUE   The alt has a Type I FORMAT_TYPE descriptor.
  @retval FALSE  It does not, or the alt was not found.
**/
BOOLEAN
UsbAudioIsTypeIAlt (
  IN UINT8   *DescriptorBuffer,
  IN UINT16  TotalLength,
  IN UINT8   InterfaceNumber,
  IN UINT8   AltSetting
  )
{
  UINT16                    Offset;
  USB_DESC_HEAD             *Head;
  USB_INTERFACE_DESCRIPTOR  *IfDesc;
  BOOLEAN                   InTarget;

  Offset   = 0;
  InTarget = FALSE;

  while ((Head = UsbAudioNextDescriptor (DescriptorBuffer, TotalLength, &Offset)) != NULL) {
    if ((Head->Type == USB_DESC_TYPE_INTERFACE) &&
        (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR)))
    {
      IfDesc   = (USB_INTERFACE_DESCRIPTOR *)Head;
      InTarget = (BOOLEAN)((IfDesc->InterfaceClass == USB_AUDIO_CLASS) &&
                           (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_STREAMING) &&
                           (IfDesc->InterfaceNumber == InterfaceNumber) &&
                           (IfDesc->AlternateSetting == AltSetting));
    } else if (InTarget &&
               (Head->Type == USB_AUDIO_CS_INTERFACE) &&
               ((Head->Len >= sizeof (USB_AUDIO_20_FORMAT_TYPE_I_DESC)) ||
                (Head->Len >= sizeof (USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER))) &&
               (USB_AUDIO_DESC_SUBTYPE (Head) == USB_AUDIO_20_AS_SUBTYPE_FORMAT_TYPE) &&
               (USB_AUDIO_FORMAT_TYPE (Head) == USB_AUDIO_FORMAT_TYPE_I))
    {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Fetch the full configuration descriptor blob into an allocated buffer.

  @param[in]  UsbIo        UsbIo of an interface on the target device.
  @param[out] Buffer       Allocated descriptor blob (caller frees).
  @param[out] TotalLength  Length of the blob in bytes.

  @retval EFI_SUCCESS           Blob fetched.
  @retval EFI_OUT_OF_RESOURCES  Allocation failed.
  @retval other                 USB request failed.
**/
EFI_STATUS
UsbAudioGetConfigDescriptor (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  OUT UINT8                **Buffer,
  OUT UINT16               *TotalLength
  )
{
  EFI_STATUS                 Status;
  EFI_USB_CONFIG_DESCRIPTOR  ConfigDesc;
  UINT8                      *Buf;
  UINT32                     TransferResult;
  EFI_USB_DEVICE_DESCRIPTOR  DevDesc;
  EFI_USB_CONFIG_DESCRIPTOR  Header;
  UINT8                      ConfigIndex;

  Status = UsbIo->UsbGetConfigDescriptor (UsbIo, &ConfigDesc);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = UsbIo->UsbGetDeviceDescriptor (UsbIo, &DevDesc);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ConfigIndex = 0;
  if (DevDesc.NumConfigurations > 1) {
    for (ConfigIndex = 0; ConfigIndex < DevDesc.NumConfigurations; ConfigIndex++) {
      Status = UsbGetDescriptor (
                 UsbIo,
                 (UINT16)((USB_DESC_TYPE_CONFIG << 8) | ConfigIndex),
                 0,
                 sizeof (Header),
                 &Header,
                 &TransferResult
                 );
      if (!EFI_ERROR (Status) && (Header.ConfigurationValue == ConfigDesc.ConfigurationValue)) {
        break;
      }
    }

    if (ConfigIndex == DevDesc.NumConfigurations) {
      return EFI_DEVICE_ERROR;
    }
  }

  Buf = AllocateZeroPool (ConfigDesc.TotalLength);
  if (Buf == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = UsbGetDescriptor (
             UsbIo,
             (UINT16)((USB_DESC_TYPE_CONFIG << 8) | ConfigIndex),
             0,
             ConfigDesc.TotalLength,
             Buf,
             &TransferResult
             );
  if (EFI_ERROR (Status)) {
    FreePool (Buf);
    return Status;
  }

  *Buffer      = Buf;
  *TotalLength = ConfigDesc.TotalLength;
  return EFI_SUCCESS;
}

/**
  Find the first AudioStreaming interface that exposes a Type I OUT alternate
  setting.

  @param[in]  Buffer           Configuration descriptor blob.
  @param[in]  TotalLength      Length in bytes.
  @param[out] InterfaceNumber  Interface number of the matching AS interface.
  @param[out] InterfaceOffset  Offset of the matching AS interface descriptor in the blob.
  @param[out] AltSetting       Alternate setting index of the matching AS interface.

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
  )
{
  UINT16                    Offset;
  USB_DESC_HEAD             *Head;
  USB_INTERFACE_DESCRIPTOR  *IfDesc;
  UINT8                     EpAddr;
  UINT16                    Mps;
  UINT8                     Interval;

  Offset = 0;
  while ((Head = UsbAudioNextDescriptor (Buffer, TotalLength, &Offset)) != NULL) {
    if ((Head->Type == USB_DESC_TYPE_INTERFACE) &&
        (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR)))
    {
      IfDesc = (USB_INTERFACE_DESCRIPTOR *)Head;
      if ((IfDesc->InterfaceClass == USB_AUDIO_CLASS) &&
          (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_STREAMING) &&
          (IfDesc->AlternateSetting != 0) &&
          UsbAudioIsTypeIAlt (Buffer, TotalLength, IfDesc->InterfaceNumber, IfDesc->AlternateSetting))
      {
        // This interface has a Type I AS alt setting; check for an OUT endpoint.
        if (!EFI_ERROR (
               UsbAudioFindOutEndpoint (
                 Buffer,
                 TotalLength,
                 IfDesc->InterfaceNumber,
                 IfDesc->AlternateSetting,
                 &EpAddr,
                 &Mps,
                 &Interval
                 )
               ))
        {
          *InterfaceNumber = IfDesc->InterfaceNumber;
          *InterfaceOffset = (UINT16)(Offset - Head->Len);
          *AltSetting      = IfDesc->AlternateSetting;
          return TRUE;
        }
      }
    }
  }

  *InterfaceOffset = TotalLength;
  return FALSE;
}

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
  )
{
  UINT16                    Offset;
  USB_DESC_HEAD             *Head;
  USB_INTERFACE_DESCRIPTOR  *IfDesc;
  USB_ENDPOINT_DESCRIPTOR   *EpDesc;
  BOOLEAN                   InTarget;

  Offset   = 0;
  InTarget = FALSE;

  while ((Head = UsbAudioNextDescriptor (Buffer, TotalLength, &Offset)) != NULL) {
    if ((Head->Type == USB_DESC_TYPE_INTERFACE) &&
        (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR)))
    {
      IfDesc   = (USB_INTERFACE_DESCRIPTOR *)Head;
      InTarget = (BOOLEAN)((IfDesc->InterfaceNumber == InterfaceNumber) &&
                           (IfDesc->AlternateSetting == AltSetting));
    } else if (InTarget &&
               (Head->Type == USB_DESC_TYPE_ENDPOINT) &&
               (Head->Len >= sizeof (USB_ENDPOINT_DESCRIPTOR)))
    {
      EpDesc = (USB_ENDPOINT_DESCRIPTOR *)Head;
      if (((EpDesc->Attributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_ISO) &&
          ((EpDesc->EndpointAddress & USB_ENDPOINT_DIR_IN) == 0))
      {
        *EndpointAddr  = EpDesc->EndpointAddress;
        *MaxPacketSize = EpDesc->MaxPacketSize;
        *Interval      = EpDesc->Interval;
        return EFI_SUCCESS;
      }
    }
  }

  return EFI_NOT_FOUND;
}

#define USB_AUDIO_MAX_TOPOLOGY_HOPS  16u

/**
  Find the AudioControl entity with the given entity ID in the configuration descriptor.

  @param[in]  Buffer       Configuration descriptor blob.
  @param[in]  TotalLength  Length of the configuration descriptor in bytes.
  @param[in]  EntityId     The entity ID of the AudioControl unit/terminal to find.

  @return  The offset of the found entity in the configuration descriptor, or TotalLength if not found.
**/
STATIC
UINT16
FindAcEntityOffset (
  IN UINT8   *Buffer,
  IN UINT16  TotalLength,
  IN UINT8   EntityId
  )
{
  UINT16                    Offset;
  USB_DESC_HEAD             *Head;
  USB_INTERFACE_DESCRIPTOR  *IfDesc;
  BOOLEAN                   InAc;

  Offset = 0;
  InAc   = FALSE;

  while ((Head = UsbAudioNextDescriptor (Buffer, TotalLength, &Offset)) != NULL) {
    if ((Head->Type == USB_DESC_TYPE_INTERFACE) && (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR))) {
      IfDesc = (USB_INTERFACE_DESCRIPTOR *)Head;
      InAc   = (BOOLEAN)((IfDesc->InterfaceClass == USB_AUDIO_CLASS) &&
                         (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_CONTROL));
    } else if (InAc &&
               (Head->Type == USB_AUDIO_CS_INTERFACE) &&
               (Head->Len >= 4) &&
               (USB_AUDIO_DESC_SUBTYPE (Head) != USB_AUDIO_20_AC_SUBTYPE_HEADER) &&
               (USB_AUDIO_AC_ENTITY_ID (Head) == EntityId))
    {
      return (UINT16)(Offset - Head->Len);
    }
  }

  return TotalLength;
}

/**
  Get the upstream source entity ID of the unit/terminal at EntityOffset.

  @param[in]  Buffer         Configuration descriptor blob.
  @param[in]  EntityOffset   Offset of the entity in the descriptor.
  @param[out] SourceId       Pointer to the source entity ID.

  @retval TRUE    The entity has an upstream source; SourceId is valid.
  @retval FALSE   The entity is an Input Terminal with no upstream source or
                  passes through an unsupported entity; SourceId is not valid.
**/
STATIC
BOOLEAN
GetEntitySource (
  IN  UINT8   *Buffer,
  IN  UINT16  EntityOffset,
  OUT UINT8   *SourceId
  )
{
  USB_DESC_HEAD  *Head;
  UINT8          SubType;

  Head    = (USB_DESC_HEAD *)(Buffer + EntityOffset);
  SubType = USB_AUDIO_DESC_SUBTYPE (Head);

  if (SubType == USB_AUDIO_20_AC_SUBTYPE_INPUT_TERMINAL) {
    return FALSE;
  }

  if (SubType == USB_AUDIO_20_AC_SUBTYPE_OUTPUT_TERMINAL) {
    if (Head->Len > 7) {
      *SourceId = Buffer[EntityOffset + 7];
      return TRUE;
    }

    return FALSE;
  }

  if (SubType == USB_AUDIO_20_AC_SUBTYPE_FEATURE_UNIT) {
    if (Head->Len > 4) {
      *SourceId = USB_AUDIO_FEATURE_UNIT_SOURCE_ID (Head);
      return TRUE;
    }
  }

  if ((SubType == USB_AUDIO_20_AC_SUBTYPE_MIXER_UNIT) ||
      (SubType == USB_AUDIO_20_AC_SUBTYPE_SELECTOR_UNIT))
  {
    if (Head->Len > 5) {
      *SourceId = USB_AUDIO_MIXER_UNIT_SOURCE_ID (Head);
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Find the Feature Unit on the signal path of a streaming interface, given the
  Terminal the interface's endpoint links to (bTerminalLink).

  For playback that Terminal is an Input Terminal feeding forward into the unit graph;
  the returned Feature Unit is the one whose source chain reaches it.
  As a fallback, if the link refers to an Output Terminal,
  the unit feeding that terminal is traced backward instead.

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
  )
{
  UINT16                    Offset;
  USB_DESC_HEAD             *Head;
  USB_INTERFACE_DESCRIPTOR  *IfDesc;
  BOOLEAN                   InAc;
  UINT8                     SourceId;
  UINTN                     Hops;
  UINT16                    Eo;

  //
  // TerminalId is the Input Terminal the stream feeds.
  // Find a Feature Unit whose upstream source chain reaches it.
  //
  Offset = 0;
  InAc   = FALSE;
  while ((Head = UsbAudioNextDescriptor (Buffer, TotalLength, &Offset)) != NULL) {
    if ((Head->Type == USB_DESC_TYPE_INTERFACE) && (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR))) {
      IfDesc = (USB_INTERFACE_DESCRIPTOR *)Head;
      InAc   = (BOOLEAN)((IfDesc->InterfaceClass == USB_AUDIO_CLASS) &&
                         (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_CONTROL));
    } else if (InAc &&
               (Head->Type == USB_AUDIO_CS_INTERFACE) &&
               (Head->Len >= 5) &&
               (USB_AUDIO_DESC_SUBTYPE (Head) == USB_AUDIO_20_AC_SUBTYPE_FEATURE_UNIT))
    {
      SourceId = USB_AUDIO_FEATURE_UNIT_SOURCE_ID (Head);
      for (Hops = 0; Hops < USB_AUDIO_MAX_TOPOLOGY_HOPS; Hops++) {
        if (SourceId == TerminalId) {
          return USB_AUDIO_AC_ENTITY_ID (Head);
        }

        Eo = FindAcEntityOffset (Buffer, TotalLength, SourceId);
        if (Eo == TotalLength) {
          break;
        }

        if (!GetEntitySource (Buffer, Eo, &SourceId)) {
          break;
        }
      }
    }
  }

  //
  // TerminalId is an Output Terminal. Trace back
  // from the unit feeding it to the first Feature Unit.
  //
  Eo = FindAcEntityOffset (Buffer, TotalLength, TerminalId);
  if ((Eo != TotalLength) && (Buffer[Eo + 2] == USB_AUDIO_20_AC_SUBTYPE_OUTPUT_TERMINAL)) {
    if (GetEntitySource (Buffer, Eo, &SourceId)) {
      for (Hops = 0; Hops < USB_AUDIO_MAX_TOPOLOGY_HOPS; Hops++) {
        Eo = FindAcEntityOffset (Buffer, TotalLength, SourceId);
        if (Eo == TotalLength) {
          break;
        }

        if (Buffer[Eo + 2] == USB_AUDIO_20_AC_SUBTYPE_FEATURE_UNIT) {
          return SourceId;
        }

        if (!GetEntitySource (Buffer, Eo, &SourceId)) {
          break;
        }
      }
    }
  }

  return 0;
}

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
  )
{
  UINT16                    Offset;
  USB_DESC_HEAD             *Head;
  USB_INTERFACE_DESCRIPTOR  *IfDesc;
  BOOLEAN                   InAc;

  if (FeatureUnitId == 0) {
    *DescriptorOffset = TotalLength;
    return NULL;
  }

  Offset = 0;
  InAc   = FALSE;

  while ((Head = UsbAudioNextDescriptor (Buffer, TotalLength, &Offset)) != NULL) {
    *DescriptorOffset = (UINT16)(Offset - Head->Len);

    if ((Head->Type == USB_DESC_TYPE_INTERFACE) &&
        (Head->Len >= sizeof (USB_INTERFACE_DESCRIPTOR)))
    {
      IfDesc = (USB_INTERFACE_DESCRIPTOR *)Head;
      InAc   = (BOOLEAN)((IfDesc->InterfaceClass == USB_AUDIO_CLASS) &&
                         (IfDesc->InterfaceSubClass == USB_AUDIO_SUBCLASS_CONTROL));
    } else if (InAc &&
               (Head->Type == USB_AUDIO_CS_INTERFACE) &&
               (Head->Len >= MinLen) &&
               (USB_AUDIO_DESC_SUBTYPE (Head) == FeatureUnitSubtype) &&
               (USB_AUDIO_AC_ENTITY_ID (Head) == FeatureUnitId))
    {
      return Head;
    }
  }

  *DescriptorOffset = TotalLength;
  return NULL;
}
