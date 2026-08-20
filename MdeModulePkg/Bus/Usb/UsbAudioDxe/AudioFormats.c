/** @file
  Implementation of helpers to parse UAC 1.0 and 2.0 descriptors
  and discover supported audio formats.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AudioFormats.h"
#include "AudioDescriptors.h"
#include "AudioRequests.h"
#include "UsbAudio.h"
#include "AudioStream.h"

/**
  Free the memory allocated for a list with USB_AUDIO_20_CLOCK_SOURCE_ENTRY entries.

  @param[in, out] ClockSourceList  Pointer to the head of the list to free.
**/
VOID
FreeClockSourceList (
  IN OUT LIST_ENTRY  *ClockSourceList
  )
{
  LIST_ENTRY                       *Entry;
  LIST_ENTRY                       *NextEntry;
  USB_AUDIO_20_CLOCK_SOURCE_ENTRY  *ClockSourceEntry;

  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, ClockSourceList) {
    ClockSourceEntry = CLOCK_SOURCE_LIST_CONTAINER (Entry);
    if (ClockSourceEntry->Subranges != NULL) {
      FreePool (ClockSourceEntry->Subranges);
      ClockSourceEntry->Subranges = NULL;
    }

    FreePool (ClockSourceEntry);
  }
}

/**
  Free the memory allocated for a list with USB_AUDIO_20_FORMAT_DESC_ENTRY entries.

  @param[in, out] FormatDescList  Pointer to the head of the list to free.
**/
STATIC
VOID
FreeFormatDesc20List (
  IN OUT LIST_ENTRY  *FormatDescList
  )
{
  LIST_ENTRY                      *Entry;
  LIST_ENTRY                      *NextEntry;
  USB_AUDIO_20_FORMAT_DESC_ENTRY  *FormatDescEntry;

  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, FormatDescList) {
    FormatDescEntry = FORMAT_DESC_20_LIST_CONTAINER (Entry);
    FreePool (FormatDescEntry);
  }
}

/**
  Free the memory allocated for a list with USB_AUDIO_10_FORMAT_DESC_ENTRY entries.

  @param[in, out] FormatDescList  Pointer to the head of the list to free.
**/
STATIC
VOID
FreeFormatDesc10List (
  IN OUT LIST_ENTRY  *FormatDescList
  )
{
  LIST_ENTRY                      *Entry;
  LIST_ENTRY                      *NextEntry;
  USB_AUDIO_10_FORMAT_DESC_ENTRY  *FormatDescEntry;

  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, FormatDescList) {
    FormatDescEntry = FORMAT_DESC_10_LIST_CONTAINER (Entry);
    FreePool (FormatDescEntry);
  }
}

/**
  Free the memory allocated for a list with USB_AUDIO_STREAMING_INTERFACE entries.

  @param[in] SpecVersion   The USB audio specification version.
  @param[in, out] InterfaceList Pointer to the head of the list to free.
**/
STATIC
VOID
FreeInterfaceList (
  IN USB_AUDIO_SPEC_VERSION  SpecVersion,
  IN OUT LIST_ENTRY          *InterfaceList
  )
{
  LIST_ENTRY                     *Entry;
  LIST_ENTRY                     *NextEntry;
  USB_AUDIO_STREAMING_INTERFACE  *Interface;

  BASE_LIST_FOR_EACH_SAFE (Entry, NextEntry, InterfaceList) {
    Interface = INTERFACE_LIST_CONTAINER (Entry);
    switch (SpecVersion) {
      case UsbAudioSpec10:
        FreeFormatDesc10List (&Interface->FormatTypeList);
        break;
      case UsbAudioSpec20:
        FreeFormatDesc20List (&Interface->FormatTypeList);
        break;
      default:
        DEBUG ((DEBUG_ERROR, "FreeInterfaceList: unknown spec version %d\n", SpecVersion));
        break;
    }

    FreePool (Interface);
  }
}

/**
  Get a list of clock sources and their supported sample rates
  for a given audio control interface.

  @param[in]  UsbIo        The audio control USB I/O protocol instance.
  @param[in]  Buffer       The configuration descriptor buffer.
  @param[in]  TotalLength  The total length of the configuration descriptor buffer.
  @param[in]  AcInterface  The audio control interface number.
  @param[out] ClockSources Pointer to the list of clock sources to populate.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory to complete the operation.
**/
STATIC
EFI_STATUS
UsbAudio20GetClockRanges (
  IN  EFI_USB_IO_PROTOCOL              *UsbIo,
  IN UINT8                             *Buffer,
  IN UINT16                            TotalLength,
  IN UINT8                             AcInterface,
  OUT USB_AUDIO_20_CLOCK_SOURCE_ENTRY  *ClockSources
  )
{
  EFI_STATUS                       Status;
  LIST_ENTRY                       *Entry;
  USB_AUDIO_20_CLOCK_SOURCE_ENTRY  *ClockSourceEntry;

  Status = UsbAudio20EnumerateClockSources (Buffer, TotalLength, AcInterface, ClockSources);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BASE_LIST_FOR_EACH (Entry, &ClockSources->ListEntry) {
    ClockSourceEntry = CLOCK_SOURCE_LIST_CONTAINER (Entry);

    Status = UsbAudio20GetClockRange (UsbIo, AcInterface, ClockSourceEntry);
    if (Status == EFI_DEVICE_ERROR) {
      //
      // Device may return an error if the clock source doesn't support frequency control.
      //
      DEBUG ((
        DEBUG_WARN,
        "UsbAudioDxe:   sample rate range query for clock source %u failed, ignoring it\n",
        ClockSourceEntry->Id
        ));
      continue;
    } else if (EFI_ERROR (Status)) {
      FreeClockSourceList (&ClockSources->ListEntry);
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  Find a USB_AUDIO_20_CLOCK_SOURCE_ENTRY in a list by its ID.

  @param[in] ClockSourceList  Pointer to the head of the list to search.
  @param[in] Id               The ID of the clock source to find.

  @return A pointer to the USB_AUDIO_20_CLOCK_SOURCE_ENTRY if found, or NULL if not found.
**/
STATIC
USB_AUDIO_20_CLOCK_SOURCE_ENTRY *
FindClockSourceById (
  IN LIST_ENTRY  *ClockSourceList,
  IN UINT8       Id
  )
{
  LIST_ENTRY                       *Entry;
  USB_AUDIO_20_CLOCK_SOURCE_ENTRY  *ClockSourceEntry;

  BASE_LIST_FOR_EACH (Entry, ClockSourceList) {
    ClockSourceEntry = CLOCK_SOURCE_LIST_CONTAINER (Entry);
    if (ClockSourceEntry->Id == Id) {
      return ClockSourceEntry;
    }
  }

  return NULL;
}

/**
 Handle a UAC 2.0 AS_GENERAL descriptor, extracting the clock source and feature unit information for the given interface.

  @param[in]  Config            The configuration descriptor buffer.
  @param[in]  ConfigLength      The total length of the configuration descriptor buffer.
  @param[in]  AcInterface       The audio control interface number.
  @param[in]  AsGeneral         Pointer to the UAC 2.0 AS_GENERAL descriptor.
  @param[in,out] Interface      Pointer to the USB_AUDIO_STREAMING_INTERFACE to populate.
  @param[in]  ClockSources      Pointer to the list of clock sources.
  @param[out] ClockSourceEntry  Pointer to receive the found clock source entry.
  @param[out] HaveGeneral       Pointer to receive whether a valid AS_GENERAL was found.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_NOT_FOUND         The clock source for the terminal link was not found in the list.
  @retval other                 Resource or USB failure.
 **/
STATIC
EFI_STATUS
UsbAudio20HandleAsGeneral (
  IN     UINT8                            *Config,
  IN     UINT16                           ConfigLength,
  IN     UINT8                            AcInterface,
  IN     USB_AUDIO_20_AS_GENERAL_DESC     *AsGeneral,
  IN OUT USB_AUDIO_STREAMING_INTERFACE    *Interface,
  IN     LIST_ENTRY                       *ClockSources,
  OUT    USB_AUDIO_20_CLOCK_SOURCE_ENTRY  **ClockSourceEntry,
  OUT    BOOLEAN                          *HaveGeneral
  )
{
  EFI_STATUS                        Status;
  USB_AUDIO_20_INPUT_TERMINAL_DESC  *TerminalDesc;
  BOOLEAN                           IsInputTerminal;

  *HaveGeneral      = FALSE;
  *ClockSourceEntry = NULL;

  if ((AsGeneral->FormatType != USB_AUDIO_FORMAT_TYPE_I) ||
      ((AsGeneral->Formats & USB_AUDIO_20_FORMAT_PCM) == 0))
  {
    return EFI_SUCCESS;
  }

  Status = UsbAudio20GetTerminalById (
             Config,
             ConfigLength,
             AsGeneral->TerminalLink,
             AcInterface,
             (VOID **)&TerminalDesc,
             &IsInputTerminal
             );
  if (EFI_ERROR (Status)) {
    return Status;
  } else if (!IsInputTerminal) {
    return EFI_NOT_FOUND;
  }

  //
  // The terminal's bCSourceID may reference a Clock Selector or Clock
  // Multiplier instead of a Clock Source directly; resolve the chain down
  // to the underlying Clock Source.
  //
  Interface->InternalInfo.ClockSourceId = UsbAudio20ResolveClockSourceId (
                                            Config,
                                            ConfigLength,
                                            AcInterface,
                                            TerminalDesc->ClockSourceId
                                            );
  if (Interface->InternalInfo.ClockSourceId != TerminalDesc->ClockSourceId) {
    DEBUG ((
      DEBUG_INFO,
      "UsbAudioDxe:   clock entity %u resolved to clock source %u\n",
      TerminalDesc->ClockSourceId,
      Interface->InternalInfo.ClockSourceId
      ));
  }

  *ClockSourceEntry = FindClockSourceById (
                        ClockSources,
                        Interface->InternalInfo.ClockSourceId
                        );
  if (*ClockSourceEntry == NULL) {
    return EFI_NOT_FOUND;
  }

  Interface->InternalInfo.FeatureUnitId = UsbAudioFindFeatureUnitForTerminal (
                                            Config,
                                            ConfigLength,
                                            AsGeneral->TerminalLink
                                            );
  UsbAudio20ExtractFeatureUnitControls (
    Config,
    ConfigLength,
    Interface->InternalInfo.FeatureUnitId,
    &Interface->InternalInfo.VolumeControlMask,
    &Interface->InternalInfo.MuteControlMask
    );

  *HaveGeneral = TRUE;
  return EFI_SUCCESS;
}

/**
  Find all PCM formats supported for each interface in the given list of USB_AUDIO_STREAMING_INTERFACE entries.

  @param[in]       UsbIo          The audio control USB I/O protocol instance.
  @param[in]       Config         The configuration descriptor buffer.
  @param[in]       ConfigLength   The total length of the configuration descriptor buffer.
  @param[in]       AcInterface    The audio control interface number.
  @param[in,out]   InterfaceList  Pointer to the list of USB_AUDIO_STREAMING_INTERFACE entries to populate with format information.
  @param[out]      NumFormats     Pointer to the number of formats found across all interfaces.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory to complete the operation.
  @retval EFI_DEVICE_ERROR      A control transfer failed or returned invalid data.
**/
STATIC
EFI_STATUS
UsbAudio20FindSubFormatsForInterface (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo,
  IN UINT8                 *Config,
  IN UINT16                ConfigLength,
  IN UINT8                 AcInterface,
  IN OUT LIST_ENTRY        *InterfaceList,
  OUT UINTN                *NumFormats
  )
{
  EFI_STATUS                       Status;
  LIST_ENTRY                       *Entry;
  USB_AUDIO_STREAMING_INTERFACE    *Interface;
  USB_AUDIO_20_FORMAT_DESC_ENTRY   *FormatDescEntry;
  UINT8                            *CurPtr;
  UINT16                           RemainingLength;
  UINT16                           Offset;
  USB_DESC_HEAD                    *Head;
  USB_AUDIO_20_AS_GENERAL_DESC     *AsGeneral;
  BOOLEAN                          HaveGeneral;
  UINT8                            SubType;
  USB_AUDIO_20_CLOCK_SOURCE_ENTRY  ClockSources;
  USB_AUDIO_20_CLOCK_SOURCE_ENTRY  *ClockSourceEntry;
  UINTN                            Index;

  Status = EFI_SUCCESS;

  BASE_LIST_FOR_EACH (Entry, InterfaceList) {
    Interface       = INTERFACE_LIST_CONTAINER (Entry);
    CurPtr          = Config + Interface->Offset;
    RemainingLength = ConfigLength - Interface->Offset;
    Offset          = 0;

    AsGeneral        = NULL;
    HaveGeneral      = FALSE;
    ClockSourceEntry = NULL;

    InitializeListHead (&ClockSources.ListEntry);

    DEBUG ((
      DEBUG_INFO,
      "UsbAudioDxe: UAC2.0 scan interface %u alt %u (config offset %u, %u bytes remaining)\n",
      Interface->InternalInfo.InterfaceNumber,
      Interface->InternalInfo.AltSetting,
      Interface->Offset,
      RemainingLength
      ));

    Status = UsbAudio20GetClockRanges (
               UsbIo,
               Config,
               ConfigLength,
               AcInterface,
               &ClockSources
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "UsbAudioDxe:   get clock ranges for interface %u failed: %r\n",
        Interface->InternalInfo.InterfaceNumber,
        Status
        ));
      return Status;
    }

    while ((Head = UsbAudioNextDescriptor (CurPtr, RemainingLength, &Offset)) != NULL) {
      if (Head->Type == USB_DESC_TYPE_INTERFACE) {
        break;
      }

      SubType = USB_AUDIO_DESC_SUBTYPE (Head);
      DEBUG ((
        DEBUG_VERBOSE,
        "UsbAudioDxe:   desc type 0x%02x subtype 0x%02x len %u\n",
        Head->Type,
        SubType,
        Head->Len
        ));
      if ((SubType == USB_AUDIO_20_AS_SUBTYPE_GENERAL) &&
          (Head->Len >= sizeof (USB_AUDIO_20_AS_GENERAL_DESC)))
      {
        AsGeneral = (USB_AUDIO_20_AS_GENERAL_DESC *)Head;
        DEBUG ((
          DEBUG_INFO,
          "UsbAudioDxe:   AS_GENERAL bFormatType %u, bmFormats 0x%08x, bTerminalLink %u\n",
          AsGeneral->FormatType,
          AsGeneral->Formats,
          AsGeneral->TerminalLink
          ));

        Status = UsbAudio20HandleAsGeneral (
                   Config,
                   ConfigLength,
                   AcInterface,
                   AsGeneral,
                   Interface,
                   &ClockSources.ListEntry,
                   &ClockSourceEntry,
                   &HaveGeneral
                   );

        if (Status == EFI_NOT_FOUND) {
          //
          // The terminal or its clock source could not be resolved for this
          // alternate setting; skip it rather than aborting format discovery
          // for the whole device.
          //
          DEBUG ((
            DEBUG_WARN,
            "UsbAudioDxe:   no usable clock source for interface %u alt %u, skipping\n",
            Interface->InternalInfo.InterfaceNumber,
            Interface->InternalInfo.AltSetting
            ));
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "UsbAudioDxe:   handle AS_GENERAL for interface %u failed: %r\n",
            Interface->InternalInfo.InterfaceNumber,
            Status
            ));
          FreeClockSourceList (&ClockSources.ListEntry);
          return Status;
        }
      } else if (HaveGeneral &&
                 (Head->Type == USB_AUDIO_CS_INTERFACE) &&
                 (Head->Len >= 4) &&
                 (SubType == USB_AUDIO_20_AS_SUBTYPE_FORMAT_TYPE) &&
                 (USB_AUDIO_FORMAT_TYPE (Head) == USB_AUDIO_FORMAT_TYPE_I))
      {
        DEBUG ((
          DEBUG_INFO,
          "UsbAudioDxe:   FORMAT_TYPE_I, creating %u format(s) from clock ranges\n",
          ClockSourceEntry->NumRanges
          ));
        for (Index = 0; Index < ClockSourceEntry->NumRanges; Index++) {
          FormatDescEntry = AllocatePool (sizeof (USB_AUDIO_20_FORMAT_DESC_ENTRY));
          if (FormatDescEntry == NULL) {
            FreeClockSourceList (&ClockSources.ListEntry);
            return EFI_OUT_OF_RESOURCES;
          }

          FormatDescEntry->FormatTypeDesc            = (USB_AUDIO_20_FORMAT_TYPE_I_DESC *)Head;
          FormatDescEntry->AsGeneralDesc             = AsGeneral;
          FormatDescEntry->SampleRates               = ClockSourceEntry->Subranges[Index];
          FormatDescEntry->FrequencyControlSupported = (ClockSourceEntry->Controls & USB_AUDIO_20_CS_FREQ_CONTROL_MASK) != 0;

          DEBUG ((
            DEBUG_INFO,
            "UsbAudioDxe:     range[%u] %u..%u Hz (step %u)\n",
            Index,
            FormatDescEntry->SampleRates.Min,
            FormatDescEntry->SampleRates.Max,
            FormatDescEntry->SampleRates.Res
            ));

          InsertTailList (&Interface->FormatTypeList, &FormatDescEntry->ListEntry);
          (*NumFormats)++;
        }
      }
    }

    DEBUG ((
      DEBUG_INFO,
      "UsbAudioDxe: UAC2.0 interface %u alt %u: %u format(s) so far\n",
      Interface->InternalInfo.InterfaceNumber,
      Interface->InternalInfo.AltSetting,
      *NumFormats
      ));

    FreeClockSourceList (&ClockSources.ListEntry);
  }
  return Status;
}

/**
  Allocates and initializes a USB_AUDIO_10_FORMAT_DESC_ENTRY structure.

  @param[in] FormatTypeDesc  Pointer to the USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER descriptor.
  @param[in] AsGeneral       Pointer to the USB_AUDIO_10_AS_GENERAL_DESC descriptor.
  @param[in] MinSampleRate   The minimum sample rate supported by this format.
  @param[in] MaxSampleRate   The maximum sample rate supported by this format.
  @param[in] SampleRateStep  The step size for sample rates supported by this format

  @return A pointer to the allocated USB_AUDIO_10_FORMAT_DESC_ENTRY structure, or NULL if allocation fails.
**/
STATIC
USB_AUDIO_10_FORMAT_DESC_ENTRY *
UsbAudio10CreateFormatDescEntry (
  IN USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER  *FormatTypeDesc,
  IN USB_AUDIO_10_AS_GENERAL_DESC            *AsGeneral,
  IN UINT32                                  MinSampleRate,
  IN UINT32                                  MaxSampleRate,
  IN UINT32                                  SampleRateStep
  )
{
  USB_AUDIO_10_FORMAT_DESC_ENTRY  *FormatDescEntry;

  FormatDescEntry = AllocatePool (sizeof (USB_AUDIO_10_FORMAT_DESC_ENTRY));
  if (FormatDescEntry == NULL) {
    return NULL;
  }

  FormatDescEntry->FormatTypeDesc            = FormatTypeDesc;
  FormatDescEntry->AsGeneralDesc             = AsGeneral;
  FormatDescEntry->SampleRates.Min           = MinSampleRate;
  FormatDescEntry->SampleRates.Max           = MaxSampleRate;
  FormatDescEntry->SampleRates.Res           = SampleRateStep;
  FormatDescEntry->FrequencyControlSupported = FALSE;

  return FormatDescEntry;
}

/**
  Create and insert a USB_AUDIO_10_FORMAT_DESC_ENTRY for a continuous sample rate range

  @param[in]      FormatType  UAC 1.0 FORMAT_TYPE_I_DESC_HEADER descriptor.
  @param[in]      AsGeneral   UAC 1.0 AS_GENERAL descriptor
  @param[in]      Interface   The USB_AUDIO_STREAMING_INTERFACE to populate.
  @param[in,out]  NumFormats  The number of formats found across all interfaces.
 **/
STATIC
EFI_STATUS
UsbAudio10CreateContinuousRateEntry (
  IN USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER  *FormatType,
  IN USB_AUDIO_10_AS_GENERAL_DESC            *AsGeneral,
  IN USB_AUDIO_STREAMING_INTERFACE           *Interface,
  IN OUT UINTN                               *NumFormats
  )
{
  USB_AUDIO_10_FORMAT_DESC_ENTRY  *Entry;

  Entry = UsbAudio10CreateFormatDescEntry (
            FormatType,
            AsGeneral,
            ReadUnaligned24 (FormatType->SamFreqs),
            ReadUnaligned24 (FormatType->SamFreqs + 3),
            1
            );
  if (Entry == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  InsertTailList (&Interface->FormatTypeList, &Entry->ListEntry);
  (*NumFormats)++;
  return EFI_SUCCESS;
}

/**
  Create and insert a USB_AUDIO_10_FORMAT_DESC_ENTRY for a set of discrete sample rates

  @param[in]      FormatType  UAC 1.0 FORMAT_TYPE_I_DESC_HEADER descriptor.
  @param[in]      AsGeneral   UAC 1.0 AS_GENERAL descriptor
  @param[in]      Interface   The USB_AUDIO_STREAMING_INTERFACE to populate.
  @param[in,out]  NumFormats  The number of formats found across all interfaces.
 **/
STATIC
EFI_STATUS
UsbAudio10CreateDiscreteRateEntries (
  IN USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER  *FormatType,
  IN USB_AUDIO_10_AS_GENERAL_DESC            *AsGeneral,
  IN USB_AUDIO_STREAMING_INTERFACE           *Interface,
  IN OUT UINTN                               *NumFormats
  )
{
  UINTN                           Index;
  UINT32                          Frequency;
  USB_AUDIO_10_FORMAT_DESC_ENTRY  *Entry;

  for (Index = 0; Index < FormatType->SamFreqType; Index++) {
    Frequency = ReadUnaligned24 (FormatType->SamFreqs + Index * 3);
    Entry     = UsbAudio10CreateFormatDescEntry (FormatType, AsGeneral, Frequency, Frequency, 0);
    if (Entry == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    InsertTailList (&Interface->FormatTypeList, &Entry->ListEntry);
    (*NumFormats)++;
  }

  return EFI_SUCCESS;
}

/**
  Find all PCM formats supported for each interface in the given list of USB_AUDIO_STREAMING_INTERFACE entries.

  @param[in]       Config         The configuration descriptor buffer.
  @param[in]       ConfigLength   The total length of the configuration descriptor buffer.
  @param[in,out]   InterfaceList  Pointer to the list of USB_AUDIO_STREAMING_INTERFACE entries to populate with format information.
  @param[out]      NumFormats     Pointer to the number of formats found across all interfaces.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory to complete the operation.
  @retval EFI_DEVICE_ERROR      A control transfer failed or returned invalid data.
**/
STATIC
EFI_STATUS
UsbAudio10FindFormatsForInterface (
  IN UINT8           *Config,
  IN UINT16          ConfigLength,
  IN OUT LIST_ENTRY  *InterfaceList,
  OUT UINTN          *NumFormats
  )
{
  EFI_STATUS                              Status;
  LIST_ENTRY                              *Entry;
  LIST_ENTRY                              *SubEntry;
  USB_AUDIO_STREAMING_INTERFACE           *Interface;
  UINT8                                   *CurPtr;
  UINT16                                  RemainingLength;
  UINT16                                  Offset;
  USB_DESC_HEAD                           *Head;
  USB_AUDIO_10_AS_GENERAL_DESC            *AsGeneral;
  USB_AUDIO_10_AS_ISO_DATA_EP_DESC        *EpGeneral;
  BOOLEAN                                 HaveGeneral;
  BOOLEAN                                 FreqCtrlSupported;
  UINT8                                   SubType;
  USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER  *FormatType;

  Status = EFI_SUCCESS;

  BASE_LIST_FOR_EACH (Entry, InterfaceList) {
    Interface       = INTERFACE_LIST_CONTAINER (Entry);
    CurPtr          = Config + Interface->Offset;
    RemainingLength = ConfigLength - Interface->Offset;
    Offset          = 0;

    AsGeneral         = NULL;
    HaveGeneral       = FALSE;
    FreqCtrlSupported = FALSE;

    DEBUG ((
      DEBUG_INFO,
      "UsbAudioDxe: UAC1.0 scan interface %u alt %u (config offset %u, %u bytes remaining)\n",
      Interface->InternalInfo.InterfaceNumber,
      Interface->InternalInfo.AltSetting,
      Interface->Offset,
      RemainingLength
      ));

    while ((Head = UsbAudioNextDescriptor (CurPtr, RemainingLength, &Offset)) != NULL) {
      if (Head->Type == USB_DESC_TYPE_INTERFACE) {
        DEBUG ((
          DEBUG_VERBOSE,
          "UsbAudioDxe:   stop scan at offset %u (type 0x%02x len %u)\n",
          Offset,
          Head->Type,
          Head->Len
          ));
        break;
      }

      SubType = USB_AUDIO_DESC_SUBTYPE (Head);
      DEBUG ((
        DEBUG_VERBOSE,
        "UsbAudioDxe:   desc type 0x%02x subtype 0x%02x len %u\n",
        Head->Type,
        SubType,
        Head->Len
        ));
      if ((Head->Type == USB_AUDIO_CS_ENDPOINT) &&
          (Head->Len >= sizeof (USB_AUDIO_10_AS_ISO_DATA_EP_DESC)) &&
          (SubType == USB_AUDIO_10_EP_SUBTYPE_GENERAL))
      {
        EpGeneral         = (USB_AUDIO_10_AS_ISO_DATA_EP_DESC *)Head;
        FreqCtrlSupported = (BOOLEAN)((EpGeneral->Attributes &
                                       USB_AUDIO_10_EP_ATTR_SAMPLING_FREQ_CTRL) != 0);
        DEBUG ((
          DEBUG_INFO,
          "UsbAudioDxe:   EP_GENERAL bmAttributes 0x%02x -> freq control %a\n",
          EpGeneral->Attributes,
          FreqCtrlSupported ? "supported" : "not supported"
          ));
      } else if ((SubType == USB_AUDIO_10_AS_SUBTYPE_GENERAL) &&
                 (Head->Len >= sizeof (USB_AUDIO_10_AS_GENERAL_DESC)))
      {
        AsGeneral = (USB_AUDIO_10_AS_GENERAL_DESC *)Head;
        if ((AsGeneral->FormatTag == USB_AUDIO_10_FORMAT_PCM) ||
            (AsGeneral->FormatTag == USB_AUDIO_10_FORMAT_PCM8))
        {
          HaveGeneral = TRUE;
          DEBUG ((
            DEBUG_INFO,
            "UsbAudioDxe:   AS_GENERAL wFormatTag 0x%04x (PCM) accepted\n",
            AsGeneral->FormatTag
            ));

          //
          // Resolve the Feature Unit on this stream's signal path.
          //
          Interface->InternalInfo.FeatureUnitId = UsbAudioFindFeatureUnitForTerminal (
                                                    Config,
                                                    ConfigLength,
                                                    AsGeneral->TerminalLink
                                                    );
          UsbAudio10ExtractFeatureUnitControls (
            Config,
            ConfigLength,
            Interface->InternalInfo.FeatureUnitId,
            &Interface->InternalInfo.VolumeControlMask,
            &Interface->InternalInfo.MuteControlMask
            );

          DEBUG ((
            DEBUG_INFO,
            "UsbAudioDxe:   feature unit id %u, volume mask 0x%x, mute mask 0x%x\n",
            Interface->InternalInfo.FeatureUnitId,
            Interface->InternalInfo.VolumeControlMask,
            Interface->InternalInfo.MuteControlMask
            ));
        } else {
          DEBUG ((
            DEBUG_WARN,
            "UsbAudioDxe:   AS_GENERAL wFormatTag 0x%04x not PCM/PCM8, skipping this alt\n",
            AsGeneral->FormatTag
            ));
        }
      } else if (HaveGeneral &&
                 (Head->Type == USB_AUDIO_CS_INTERFACE) &&
                 (Head->Len >= sizeof (USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER)) &&
                 (SubType == USB_AUDIO_10_AS_SUBTYPE_FORMAT_TYPE) &&
                 (USB_AUDIO_FORMAT_TYPE (Head)  == USB_AUDIO_FORMAT_TYPE_I))
      {
        FormatType = (USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER *)Head;
        DEBUG ((
          DEBUG_INFO,
          "UsbAudioDxe:   FORMAT_TYPE_I len %u: %u ch, %u-bit, subslot %u, SamFreqType %u\n",
          Head->Len,
          FormatType->NrChannels,
          FormatType->BitResolution,
          FormatType->SubframeSize,
          FormatType->SamFreqType
          ));
        if ((FormatType->SamFreqType == 0) &&
            (Head->Len >= sizeof (USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER) + 5))
        {
          Status = UsbAudio10CreateContinuousRateEntry (FormatType, AsGeneral, Interface, NumFormats);
          if (EFI_ERROR (Status)) {
            break;
          }
        } else if ((FormatType->SamFreqType > 0) &&
                   (Head->Len >= sizeof (USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER) + (FormatType->SamFreqType * 3) - 1))
        {
          Status = UsbAudio10CreateDiscreteRateEntries (FormatType, AsGeneral, Interface, NumFormats);
          if (EFI_ERROR (Status)) {
            break;
          }
        } else {
          DEBUG ((
            DEBUG_WARN,
            "UsbAudioDxe:   FORMAT_TYPE_I rejected: len %u too short for SamFreqType %u "
            "(need %u for continuous / %u for discrete)\n",
            Head->Len,
            FormatType->SamFreqType,
            (UINTN)(sizeof (USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER) + 5),
            (UINTN)(sizeof (USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER) + (FormatType->SamFreqType * 3) - 1)
            ));
        }
      }
    }

    DEBUG ((
      DEBUG_INFO,
      "UsbAudioDxe: UAC1.0 interface %u alt %u: %u format(s) so far\n",
      Interface->InternalInfo.InterfaceNumber,
      Interface->InternalInfo.AltSetting,
      *NumFormats
      ));

    //
    // Apply the alt setting's Sampling Frequency control capability
    // to every format entry created for this alt.
    //
    BASE_LIST_FOR_EACH (SubEntry, &Interface->FormatTypeList) {
      FORMAT_DESC_10_LIST_CONTAINER (SubEntry)->FrequencyControlSupported = FreqCtrlSupported;
    }

    if (EFI_ERROR (Status)) {
      break;
    }
  }
  return Status;
}

/**
  Fill common format information into a USB_AUDIO_FORMAT_INFO structure.

  @param[out] Out            Pointer to the USB_AUDIO_FORMAT_INFO structure to fill.
  @param[in]  Interface      Pointer to the USB_AUDIO_STREAMING_INTERFACE structure.
  @param[in]  Rates          Pointer to the USB_AUDIO_FREQUENCY_RANGE structure containing the sample rate range.
  @param[in]  FreqCtrl       Boolean indicating if frequency control is supported.
  @param[in]  ClockSourceId  The ID of the clock source (0 for UAC1).
  @param[in]  FormatTag      The format tag (0 for UAC2).
  @param[in]  Channels       The number of audio channels.
  @param[in]  SubslotSize    The size of the audio subslot in bytes.
  @param[in]  BitsPerSample  The number of bits per audio sample.
 **/
STATIC
VOID
FillCommonFormatInfo (
  OUT USB_AUDIO_FORMAT_INFO          *Out,
  IN  USB_AUDIO_STREAMING_INTERFACE  *Interface,
  IN  USB_AUDIO_FREQUENCY_RANGE      *Rates,
  IN  BOOLEAN                        FreqCtrl,
  IN  UINT8                          ClockSourceId,   /* 0 for UAC1 */
  IN  UINT16                         FormatTag,       /* 0 for UAC2 */
  IN  UINT8                          Channels,
  IN  UINT8                          SubslotSize,
  IN  UINT8                          BitsPerSample
  )
{
  CopyMem (
    (VOID *)&Out->InternalInfo,
    (VOID *)&Interface->InternalInfo,
    sizeof (Interface->InternalInfo)
    );
  Out->FormatTag                 = FormatTag;
  Out->FrequencyControlSupported = FreqCtrl;
  Out->Info.MinSampleRateHz      = Rates->Min;
  Out->Info.MaxSampleRateHz      = Rates->Max;
  Out->Info.SampleRateStepHz     = Rates->Res;
  Out->Info.Channels             = Channels;
  Out->Info.SubslotSize          = SubslotSize;
  Out->Info.BitsPerSample        = BitsPerSample;
}

/**
  Fill the USB_AUDIO_FORMAT_INFO array with information from the list of USB_AUDIO_STREAMING_INTERFACE entries.

  @param[in]       InterfaceListHead  Pointer to the head of the list of USB_AUDIO_STREAMING_INTERFACE entries.
  @param[in]       NumFormats         Pointer to the number of formats found across all interfaces, including sublists.
  @param[in,out]   Formats            Pointer to the array of USB_AUDIO_FORMAT_INFO structures
**/
STATIC
VOID
UsbAudio10FillFormatInfo (
  IN LIST_ENTRY                 *InterfaceListHead,
  IN UINTN                      *NumFormats,
  IN OUT USB_AUDIO_FORMAT_INFO  *Formats
  )
{
  USB_AUDIO_STREAMING_INTERFACE   *Interface;
  USB_AUDIO_10_FORMAT_DESC_ENTRY  *FormatDescEntry;
  LIST_ENTRY                      *Entry;
  LIST_ENTRY                      *SubEntry;
  UINTN                           Index;

  Index = 0;
  BASE_LIST_FOR_EACH (Entry, InterfaceListHead) {
    Interface = INTERFACE_LIST_CONTAINER (Entry);

    BASE_LIST_FOR_EACH (SubEntry, &Interface->FormatTypeList) {
      ASSERT (Index < *NumFormats);

      FormatDescEntry = FORMAT_DESC_10_LIST_CONTAINER (SubEntry);

      Interface->InternalInfo.ClockSourceId = 0;
      FillCommonFormatInfo (
        &Formats[Index],
        Interface,
        &FormatDescEntry->SampleRates,
        FormatDescEntry->FrequencyControlSupported,
        0, /* ClockSourceId */
        FormatDescEntry->AsGeneralDesc->FormatTag,
        FormatDescEntry->FormatTypeDesc->NrChannels,
        FormatDescEntry->FormatTypeDesc->SubframeSize,
        FormatDescEntry->FormatTypeDesc->BitResolution
        );

      Index++;
    }
  }
}

/**
  Fill the USB_AUDIO_FORMAT_INFO array with information from the list of USB_AUDIO_STREAMING_INTERFACE entries.

  @param[in]       InterfaceListHead  Pointer to the head of the list of USB_AUDIO_STREAMING_INTERFACE entries.
  @param[in]       NumFormats         Pointer to the number of formats found across all interfaces, including sublists.
  @param[in,out]   Formats            Pointer to the array of USB_AUDIO_FORMAT_INFO structures
**/
STATIC
VOID
UsbAudio20FillFormatInfo (
  IN LIST_ENTRY                 *InterfaceListHead,
  IN UINTN                      *NumFormats,
  IN OUT USB_AUDIO_FORMAT_INFO  *Formats
  )
{
  USB_AUDIO_STREAMING_INTERFACE   *Interface;
  USB_AUDIO_20_FORMAT_DESC_ENTRY  *FormatDescEntry;
  LIST_ENTRY                      *Entry;
  LIST_ENTRY                      *SubEntry;
  UINTN                           Index;

  Index = 0;
  BASE_LIST_FOR_EACH (Entry, InterfaceListHead) {
    Interface = INTERFACE_LIST_CONTAINER (Entry);

    BASE_LIST_FOR_EACH (SubEntry, &Interface->FormatTypeList) {
      ASSERT (Index < *NumFormats);
      FormatDescEntry = FORMAT_DESC_20_LIST_CONTAINER (SubEntry);

      FillCommonFormatInfo (
        &Formats[Index],
        Interface,
        &FormatDescEntry->SampleRates,
        FormatDescEntry->FrequencyControlSupported,
        Interface->InternalInfo.ClockSourceId,
        0, /* FormatTag */
        FormatDescEntry->AsGeneralDesc->NumChannels,
        FormatDescEntry->FormatTypeDesc->SubslotSize,
        FormatDescEntry->FormatTypeDesc->BitResolution
        );

      Index++;
    }
  }
}

/**
  Find all Type I OUT streaming interfaces in the given configuration descriptor.

  @param[in]      Config          The configuration descriptor buffer.
  @param[in]      ConfigLength    The total length of the configuration descriptor buffer.
  @param[out]     TypeICount      The number of Type I OUT streaming interfaces found.
  @param[in,out]  InterfaceList   The initialized list of USB_AUDIO_STREAMING_INTERFACE entries to populate.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  Insufficient memory to complete the operation.
 **/
STATIC
EFI_STATUS
FindTypeIInterfaces (
  IN UINT8           *Config,
  IN UINT16          ConfigLength,
  OUT UINTN          *TypeICount,
  IN OUT LIST_ENTRY  *InterfaceList
  )
{
  EFI_STATUS                     Status;
  UINT8                          *CurPtr;
  UINT16                         Offset;
  UINT16                         RemainingLength;
  BOOLEAN                        FoundTypeI;
  UINT8                          AsInterface;
  UINT8                          AltSetting;
  USB_AUDIO_STREAMING_INTERFACE  *Interface;
  USB_DESC_HEAD                  *Head;

  CurPtr          = Config;
  RemainingLength = ConfigLength;

  while (RemainingLength >= sizeof (USB_DESC_HEAD)) {
    FoundTypeI = UsbAudioFindFirstTypeIStream (
                   CurPtr,
                   RemainingLength,
                   &AsInterface,
                   &Offset,
                   &AltSetting
                   );
    if (!FoundTypeI) {
      DEBUG ((
        DEBUG_INFO,
        "UsbAudioDxe: DiscoverFormats: no further Type I OUT stream found (%u so far)\n",
        *TypeICount
        ));
      break;
    }

    Head = (USB_DESC_HEAD *)(CurPtr + Offset);
    if ((Head->Len < sizeof (USB_DESC_HEAD)) ||
        (Offset + Head->Len > RemainingLength))
    {
      DEBUG ((
        DEBUG_WARN,
        "UsbAudioDxe: DiscoverFormats: interface %u alt %u descriptor length %u exceeds remaining %u\n",
        AsInterface,
        AltSetting,
        Head->Len,
        RemainingLength - Offset
        ));
      break;
    }

    Interface = AllocatePool (sizeof (USB_AUDIO_STREAMING_INTERFACE));
    if (Interface == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Interface->InternalInfo.InterfaceNumber = AsInterface;
    //
    // Offset points at the standard AS interface descriptor. Skip past it so the
    // format walkers start at the class-specific descriptors that follow.
    //
    Interface->Offset                  = (CurPtr - Config) + Offset + Head->Len;
    Interface->InternalInfo.AltSetting = AltSetting;

    Interface->InternalInfo.FeatureUnitId     = 0;
    Interface->InternalInfo.VolumeControlMask = 0;
    Interface->InternalInfo.MuteControlMask   = 0;

    DEBUG ((
      DEBUG_INFO,
      "UsbAudioDxe: DiscoverFormats: Type I OUT stream interface %u alt %u, body at offset %u\n",
      AsInterface,
      AltSetting,
      Interface->Offset
      ));

    RemainingLength -= Offset;
    RemainingLength -= Head->Len;
    CurPtr          += Offset + Head->Len;

    Status = UsbAudioFindOutEndpoint (
               Config,
               ConfigLength,
               Interface->InternalInfo.InterfaceNumber,
               Interface->InternalInfo.AltSetting,
               &Interface->InternalInfo.EndpointAddr,
               &Interface->InternalInfo.MaxPacketSize,
               &Interface->InternalInfo.Interval
               );

    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_WARN,
        "UsbAudioDxe: DiscoverFormats: no iso OUT endpoint for interface %u alt %u: %r\n",
        AsInterface,
        AltSetting,
        Status
        ));
      FreePool (Interface);
      continue;
    }

    DEBUG ((
      DEBUG_INFO,
      "UsbAudioDxe: DiscoverFormats:   endpoint 0x%02x, maxpacket %u, interval %u\n",
      Interface->InternalInfo.EndpointAddr,
      Interface->InternalInfo.MaxPacketSize,
      Interface->InternalInfo.Interval
      ));

    InitializeListHead (&Interface->FormatTypeList);

    InsertTailList (InterfaceList, &Interface->ListEntry);
    (*TypeICount)++;
  }

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS                Status;
  UINT8                     *Config;
  UINT16                    ConfigLength;
  LIST_ENTRY                InterfaceList;
  UINTN                     TypeICount;
  USB_INTERFACE_DESCRIPTOR  AcInterfaceDescriptor;

  *Formats    = NULL;
  *NumFormats = 0;

  InitializeListHead (&InterfaceList);

  Config     = NULL;
  TypeICount = 0;

  Status = UsbAudioGetConfigDescriptor (UsbIo, &Config, &ConfigLength);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UsbAudioDxe: DiscoverFormats: get config descriptor failed: %r\n", Status));
    goto Done;
  }

  DEBUG ((
    DEBUG_INFO,
    "UsbAudioDxe: DiscoverFormats: spec %a, config descriptor %u bytes\n",
    (SpecVersion == UsbAudioSpec20) ? "UAC2.0" : "UAC1.0",
    ConfigLength
    ));

  Status = FindTypeIInterfaces (Config, ConfigLength, &TypeICount, &InterfaceList);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (TypeICount == 0) {
    DEBUG ((DEBUG_ERROR, "UsbAudioDxe: DiscoverFormats: no Type I OUT streaming interface found\n"));
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "UsbAudioDxe: DiscoverFormats: %u Type I interface(s) found\n", TypeICount));

  switch (SpecVersion) {
    case UsbAudioSpec10:
      Status = UsbAudio10FindFormatsForInterface (
                 Config,
                 ConfigLength,
                 &InterfaceList,
                 NumFormats
                 );
      if (EFI_ERROR (Status)) {
        goto Done;
      }

      DEBUG ((DEBUG_INFO, "UsbAudioDxe: DiscoverFormats() found %u Type I interfaces for UAC 1.0\n", *NumFormats));
      break;

    case UsbAudioSpec20:
      Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &AcInterfaceDescriptor);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "UsbAudioDxe: DiscoverFormats: get interface descriptor failed: %r\n", Status));
        goto Done;
      }

      Status = UsbAudio20FindSubFormatsForInterface (
                 UsbIo,
                 Config,
                 ConfigLength,
                 AcInterfaceDescriptor.InterfaceNumber,
                 &InterfaceList,
                 NumFormats
                 );
      if (EFI_ERROR (Status)) {
        goto Done;
      }

      DEBUG ((DEBUG_INFO, "UsbAudioDxe: DiscoverFormats() found %u Type I formats for UAC 2.0\n", *NumFormats));
      break;

    default:
      Status = EFI_INVALID_PARAMETER;
      goto Done;
  }

  if (*NumFormats == 0) {
    DEBUG ((DEBUG_ERROR, "UsbAudioDxe: DiscoverFormats: Type I interfaces present but no usable formats parsed\n"));
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  *Formats = (USB_AUDIO_FORMAT_INFO *)AllocateZeroPool (sizeof (USB_AUDIO_FORMAT_INFO) * (*NumFormats));
  if (*Formats == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  switch (SpecVersion) {
    case UsbAudioSpec10:
      UsbAudio10FillFormatInfo (&InterfaceList, NumFormats, *Formats);
      break;

    case UsbAudioSpec20:
      UsbAudio20FillFormatInfo (&InterfaceList, NumFormats, *Formats);
      break;

    default:
      break;
  }

Done:
  if (TypeICount > 0) {
    FreeInterfaceList (SpecVersion, &InterfaceList);
  }

  if (Config != NULL) {
    FreePool (Config);
  }

  return Status;
}

/**
  Free the memory allocated for the audio format information.

  @param[in] NumFormats   The number of supported formats.
  @param[in] Formats      A pointer to the array of supported formats.
**/
VOID
FreeFormatInfo (
  IN UINTN                  NumFormats,
  IN USB_AUDIO_FORMAT_INFO  *Formats
  )
{
  if (Formats != NULL) {
    FreePool (Formats);
  }
}

/**
  Derive the isochronous packet pacing of the stream for the selected format.

  One packet is sent per endpoint service interval.

  @param[in]     Dev     The USB audio device context (spec version).
  @param[in,out] Stream  The stream context.

  @retval EFI_SUCCESS      Pacing derived.
  @retval EFI_UNSUPPORTED  The sample rate needs more bytes per service
                           interval than the endpoint's max packet size.
**/
STATIC
EFI_STATUS
ComputeStreamPacing (
  IN     USB_AUDIO_DEV         *Dev,
  IN OUT USB_AUDIO_STREAM_CTX  *Stream
  )
{
  UINTN  FrameSize;
  UINTN  Interval;
  UINTN  IntervalsPerSecond;
  UINTN  MaxFramesPerInterval;
  UINTN  PacketsPerPumpPeriod;

  FrameSize = MAX (1u, (UINTN)Stream->DeviceChannels * Stream->DeviceSubframeSize);

  Interval = MAX (1u, (UINTN)Stream->Interval);
  if (Dev->SpecVersion == UsbAudioSpec20) {
    IntervalsPerSecond = 8000u >> (MIN (Interval, 14u) - 1);
  } else {
    IntervalsPerSecond = 1000u / Interval;
  }

  IntervalsPerSecond = MAX (IntervalsPerSecond, 1u);

  MaxFramesPerInterval = (Stream->DeviceSampleRateHz + IntervalsPerSecond - 1) / IntervalsPerSecond;

  Stream->IntervalsPerSecond = IntervalsPerSecond;
  Stream->MaxFramesPerPacket = ((UINTN)Stream->MaxPacketSize & 0x7FF) / FrameSize;

  if (MaxFramesPerInterval > Stream->MaxFramesPerPacket) {
    DEBUG ((
      DEBUG_ERROR,
      "UsbAudioDxe: %u Hz needs %u frames per service interval, endpoint packet holds only %u\n",
      Stream->DeviceSampleRateHz,
      MaxFramesPerInterval,
      Stream->MaxFramesPerPacket
      ));
    return EFI_UNSUPPORTED;
  }

  //
  // Keep USB_AUDIO_PUMP_PERIODS of packets queued so the ring stays fed across
  // late timer ticks
  //
  PacketsPerPumpPeriod = MAX (1u, (IntervalsPerSecond * USB_AUDIO_TRANSFER_SLOT_INTERVAL_MS) / 1000u);

  Stream->TargetInFlight = MIN (USB_AUDIO_PUMP_PERIODS * PacketsPerPumpPeriod, (UINTN)USB_AUDIO_TRANSFER_SLOT_COUNT);

  DEBUG ((
    DEBUG_INFO,
    "UsbAudioDxe: pacing: %u service intervals/s, <=%u frames/packet, %u packets in flight\n",
    Stream->IntervalsPerSecond,
    Stream->MaxFramesPerPacket,
    Stream->TargetInFlight
    ));

  return EFI_SUCCESS;
}

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
  )
{
  USB_AUDIO_STREAM_CTX   *Stream;
  EFI_STATUS             Status;
  USB_AUDIO_FORMAT_INFO  *SelectedFormat;
  EFI_TPL                OldTpl;

  ASSERT (ControlUsbIo != NULL);
  ASSERT (Formats != NULL);
  ASSERT (Dev != NULL);

  if ((SelectedFormatIndex >= Dev->SupportedFormatCount) ||
      (SampleRateHz < Formats[SelectedFormatIndex].Info.MinSampleRateHz)  ||
      (SampleRateHz > Formats[SelectedFormatIndex].Info.MaxSampleRateHz))
  {
    return EFI_INVALID_PARAMETER;
  }

  Stream = &Dev->Stream;
  ASSERT (Stream != NULL);

  if (!StopAudio (Dev)) {
    DEBUG ((DEBUG_ERROR, "UsbAudioDxe: Failed to drain audio stream\n"));
    return EFI_DEVICE_ERROR;
  }

  OldTpl = gBS->RaiseTPL (USB_AUDIO_TPL);

  SelectedFormat = &Formats[SelectedFormatIndex];

  Stream->InterfaceNumber     = SelectedFormat->InternalInfo.InterfaceNumber;
  Stream->EndpointAddr        = SelectedFormat->InternalInfo.EndpointAddr;
  Stream->MaxPacketSize       = SelectedFormat->InternalInfo.MaxPacketSize;
  Stream->Interval            = SelectedFormat->InternalInfo.Interval;
  Stream->DeviceSampleRateHz  = SampleRateHz;
  Stream->DeviceChannels      = SelectedFormat->Info.Channels;
  Stream->DeviceSubframeSize  = SelectedFormat->Info.SubslotSize;
  Stream->DeviceBitResolution = SelectedFormat->Info.BitsPerSample;
  Stream->DeviceFormatTag     = SelectedFormat->FormatTag;
  Dev->FormatValid            = FALSE;
  Dev->FeatureUnitId          = SelectedFormat->InternalInfo.FeatureUnitId;
  Dev->VolumeControlMask      = SelectedFormat->InternalInfo.VolumeControlMask;
  Dev->MuteControlMask        = SelectedFormat->InternalInfo.MuteControlMask;

  Status = ComputeStreamPacing (Dev, Stream);

  gBS->RestoreTPL (OldTpl);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Dev->Stream.AsHandle != NULL) {
    gBS->CloseProtocol (
           Dev->Stream.AsHandle,
           &gEfiUsbIoProtocolGuid,
           AgentHandle,
           Dev->ControllerHandle
           );
  }

  Status = OpenAudioStreamIo (Dev, AgentHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InitStreams (Stream);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UsbAudioDxe: InitStreams failed: %r\n", Status));
    return Status;
  }

  Status = UsbAudioSetInterface (Stream->UsbIo, Stream->InterfaceNumber, SelectedFormat->InternalInfo.AltSetting);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UsbAudioDxe: SET_INTERFACE alt %u failed: %r\n", SelectedFormat->InternalInfo.AltSetting, Status));
    return Status;
  }

  if (SelectedFormat->FrequencyControlSupported) {
    if (Dev->SpecVersion == UsbAudioSpec20) {
      if (SelectedFormat->InternalInfo.ClockSourceId != 0 ) {
        Status = UsbAudio20SetSampleRate (ControlUsbIo, Dev->AcInterfaceNumber, SelectedFormat->InternalInfo.ClockSourceId, SampleRateHz);
      }
    } else {
      Status = UsbAudio10SetSampleRate (ControlUsbIo, SelectedFormat->InternalInfo.EndpointAddr, SampleRateHz);
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_WARN,
        "UsbAudioDxe: set sample rate %u failed: %r (continuing)\n",
        SampleRateHz,
        Status
        ));
      Status = EFI_SUCCESS;
    }
  } else {
    DEBUG ((DEBUG_INFO, "UsbAudioDxe: Selected format doesn't support frequency control, skipping sample rate set\n"));
  }

  OldTpl = gBS->RaiseTPL (USB_AUDIO_TPL);

  Dev->FormatValid        = TRUE;
  Dev->Format->Format     = (UINT32)SelectedFormatIndex;
  Dev->Format->Info       = &SelectedFormat->Info;
  Dev->CurrentFormatIndex = SelectedFormatIndex;

  gBS->RestoreTPL (OldTpl);

  return Status;
}

/**
  Get the size of the current audio frame in bytes.

  @param[in]  Dev  The USB audio device context.

  @return The size of the current audio frame in bytes, but not less than 1.
**/
UINTN
GetCurrentFrameSize (
  IN USB_AUDIO_DEV  *Dev
  )
{
  return MAX (1, Dev->Stream.DeviceChannels * Dev->Stream.DeviceSubframeSize);
}
