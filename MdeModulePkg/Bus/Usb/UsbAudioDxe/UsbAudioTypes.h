/** @file
  Shared internal types for the USB Audio Class DXE driver.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <IndustryStandard/UsbAudio.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Protocol/AudioCodec.h>
#include <Protocol/AudioOutput.h>
#include <Protocol/UsbIo.h>

// ---------------------------------------------------------------------------
// Private constants
// ---------------------------------------------------------------------------

/** The number of pump periods to keep queued. */
#define USB_AUDIO_PUMP_PERIODS  2

/** Period of the stream pump timer in milliseconds. */
#define USB_AUDIO_TRANSFER_SLOT_INTERVAL_MS  10u

/**
  Number of isochronous transfer slots maintained by the stream, one packet
  each. Sized for USB_AUDIO_PUMP_PERIODS periods of packets at the fastest service rate.
**/
#define USB_AUDIO_TRANSFER_SLOT_COUNT  (8 * USB_AUDIO_TRANSFER_SLOT_INTERVAL_MS * USB_AUDIO_PUMP_PERIODS)

#define USB_AUDIO_TPL  TPL_NOTIFY

/** Feature-unit control masks are UINT32: at most 32 addressable channels. */
#define USB_AUDIO_MAX_CHANNELS  (8 * sizeof (UINT32))

typedef enum {
  UsbAudioSpec10 = 0,
  UsbAudioSpec20 = 1,
} USB_AUDIO_SPEC_VERSION;

typedef USB_AUDIO_20_RANGE_UINT32_SUBRANGE USB_AUDIO_FREQUENCY_RANGE;

typedef struct _USB_AUDIO_STREAM_CTX USB_AUDIO_STREAM_CTX;

//
// Context for stream slot callbacks
//

typedef struct {
  USB_AUDIO_STREAM_CTX    *Stream;
  UINTN                   SlotIndex;
} STREAM_CALLBACK_CTX;

typedef struct {
  UINT8     AltSetting;
  UINT8     InterfaceNumber;
  UINT8     ClockSourceId;
  UINT8     EndpointAddr;
  UINT16    MaxPacketSize;
  UINT8     Interval;
  UINT8     FeatureUnitId;
  UINT32    VolumeControlMask;
  UINT32    MuteControlMask;
} USB_AUDIO_INTERNAL_FORMAT_INFO;

typedef struct {
  USB_AUDIO_INTERNAL_FORMAT_INFO        InternalInfo;
  UINT16                                FormatTag;
  BOOLEAN                               FrequencyControlSupported;
  EFI_AUDIO_CODEC_FORMAT_INFORMATION    Info;
} USB_AUDIO_FORMAT_INFO;

typedef struct {
  UINT8                                 Id;
  UINT8                                 Attributes;
  UINT8                                 Controls;
  UINT16                                NumRanges;
  USB_AUDIO_20_RANGE_UINT32_SUBRANGE    *Subranges;
  LIST_ENTRY                            ListEntry;
} USB_AUDIO_20_CLOCK_SOURCE_ENTRY;

// ---------------------------------------------------------------------------
// Per-entry in the pending audio queue
// ---------------------------------------------------------------------------

typedef struct {
  VOID                                *Buffer;        /* caller's buffer (not owned) */
  UINTN                               BufferSize;     /* bytes */
  UINTN                               ConsumedFrames; /* input frames already submitted */
  EFI_AUDIO_OUTPUT_BUFFER_COMPLETE    Callback;
  VOID                                *Context;
  BOOLEAN                             IsValid;
  BOOLEAN                             Cancelled;
  BOOLEAN                             FinalSliceSeen;
  BOOLEAN                             HadError;
  UINTN                               OutstandingSlices;
} USB_AUDIO_PENDING_ENTRY;

// ---------------------------------------------------------------------------
// One USB isochronous transfer slot, holding a single isochronous packet
// ---------------------------------------------------------------------------

typedef struct {
  BOOLEAN    InUse;
  BOOLEAN    IsLast;                      /* whether this packet includes the last frames from its pending entry */
  UINT8      *PacketBuffer;               /* driver-owned packet buffer of MaxPacketSize bytes */
  UINTN      PendingEntryIndex;           /* which pending entry this packet is from */
} USB_AUDIO_TRANSFER_SLOT;

// ---------------------------------------------------------------------------
// Per-AS-interface stream context
// ---------------------------------------------------------------------------

struct _USB_AUDIO_STREAM_CTX {
  EFI_USB_IO_PROTOCOL        *UsbIo;
  EFI_HANDLE                 AsHandle;
  UINT8                      InterfaceNumber;
  UINT8                      EndpointAddr;
  UINT16                     MaxPacketSize;
  UINT8                      Interval;

  /* Best alt selected at Start time */
  UINT32                     DeviceSampleRateHz;
  UINT8                      DeviceChannels;
  UINT8                      DeviceSubframeSize;
  UINT8                      DeviceBitResolution;
  UINT16                     DeviceFormatTag; /* UAC1 wFormatTag or 0 for UAC2 PCM */

  /* Packet pacing: one isochronous packet is sent per endpoint service interval */
  UINTN                      IntervalsPerSecond;
  UINTN                      MaxFramesPerPacket;
  UINTN                      RateCarry;
  UINTN                      TargetInFlight;

  /* Pending queue */
  USB_AUDIO_PENDING_ENTRY    PendingQueue[AUDIO_OUTPUT_MAX_PENDING_BUFFERS];
  UINTN                      PendingHead;     /* index of the oldest entry */
  UINTN                      NextPendingToTransfer;
  UINTN                      PendingCount;    /* number of in-use entries */

  /* Transfer slots */
  USB_AUDIO_TRANSFER_SLOT    Slots[USB_AUDIO_TRANSFER_SLOT_COUNT];
  UINTN                      NextSlotToFill;                                 /* round-robin index for the next slot to fill from pending */
  volatile UINTN             InFlightTransfers;                              /* number of transfers currently in-flight with the USB stack */
  STREAM_CALLBACK_CTX        CallbackCtxPool[USB_AUDIO_TRANSFER_SLOT_COUNT]; /* pre-allocated callback contexts, one per slot */

  BOOLEAN                    Stopped;
};

// ---------------------------------------------------------------------------
// Main device context
// ---------------------------------------------------------------------------

typedef struct {
  UINTN                              Signature;
  EFI_HANDLE                         ControllerHandle;
  EFI_USB_IO_PROTOCOL                *AcUsbIo;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;

  USB_AUDIO_SPEC_VERSION             SpecVersion;
  UINT8                              AcInterfaceNumber;
  UINT8                              FeatureUnitId;
  UINT32                             VolumeControlMask;
  UINT32                             MuteControlMask;

  USB_AUDIO_FORMAT_INFO              *SupportedFormats;
  UINTN                              SupportedFormatCount;
  UINTN                              CurrentFormatIndex;
  BOOLEAN                            FormatValid;

  USB_AUDIO_STREAM_CTX               Stream;
  EFI_HANDLE                         DriverBindingHandle;

  EFI_AUDIO_OUTPUT_PROTOCOL          AudioOutputProtocol;
  EFI_AUDIO_CODEC_PROTOCOL           AudioCodecProtocol;
  EFI_AUDIO_CODEC_PROTOCOL_FORMAT    *Format;
  EFI_UNICODE_STRING_TABLE           *ControllerNameTable;
  EFI_EVENT                          StreamTimer;
} USB_AUDIO_DEV;

#define CLOCK_SOURCE_LIST_CONTAINER(Entry)  BASE_CR(Entry, USB_AUDIO_20_CLOCK_SOURCE_ENTRY, ListEntry)

/**
  Free the memory allocated for a list with USB_AUDIO_20_CLOCK_SOURCE_ENTRY entries.

  @param[in] ClockSourceList  Pointer to the head of the list to free.
**/
VOID
FreeClockSourceList (
  IN OUT LIST_ENTRY  *ClockSourceList
  );
