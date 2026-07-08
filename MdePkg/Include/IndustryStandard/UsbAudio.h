/** @file
  USB Audio Class descriptor and constant definitions for both UAC 1.0 and UAC 2.0.

  Covers:
  - USB Device Class Definition for Audio Devices, Release 1.0 (March 18, 1998)
  - USB Device Class Definition for Audio Devices, Release 2.0
  - USB Device Class Definition for Audio Data Formats, Release 1.0

  UAC 1.0 symbols use the USB_AUDIO_10_ prefix.
  UAC 2.0 symbols use the USB_AUDIO_20_ prefix.
  Common symbols use the USB_AUDIO_ prefix.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi/UefiBaseType.h>

//
// Common class / subclass / protocol codes
//
#define USB_AUDIO_CLASS  0x01u

#define USB_AUDIO_SUBCLASS_UNDEFINED       0x00u
#define USB_AUDIO_SUBCLASS_CONTROL         0x01u
#define USB_AUDIO_SUBCLASS_STREAMING       0x02u
#define USB_AUDIO_SUBCLASS_MIDI_STREAMING  0x03u

#define USB_AUDIO_PROTOCOL_NONE           0x00u   /* UAC 1.0 */
#define USB_AUDIO_PROTOCOL_VERSION_02_00  0x20u   /* UAC 2.0 */

//
// Common descriptor type codes (CS_INTERFACE / CS_ENDPOINT)
//
#define USB_AUDIO_CS_INTERFACE  0x24u
#define USB_AUDIO_CS_ENDPOINT   0x25u

//
// Format type codes  (Audio Data Formats Section A.2)
//
#define USB_AUDIO_FORMAT_TYPE_UNDEFINED  0x00u
#define USB_AUDIO_FORMAT_TYPE_I          0x01u  /* PCM-family */
#define USB_AUDIO_FORMAT_TYPE_II         0x02u  /* Encoded (MPEG, AC-3) */
#define USB_AUDIO_FORMAT_TYPE_III        0x03u  /* IEC 61937 packed */

//
// UAC 1.0 wFormatTag codes  (Audio Data Formats Section A.1)
//
#define USB_AUDIO_10_FORMAT_TYPE_I_UNDEFINED  0x0000u
#define USB_AUDIO_10_FORMAT_PCM               0x0001u
#define USB_AUDIO_10_FORMAT_PCM8              0x0002u
#define USB_AUDIO_10_FORMAT_IEEE_FLOAT        0x0003u
#define USB_AUDIO_10_FORMAT_ALAW              0x0004u
#define USB_AUDIO_10_FORMAT_MULAW             0x0005u

#define USB_AUDIO_10_FORMAT_TYPE_II_UNDEFINED  0x1000u
#define USB_AUDIO_10_FORMAT_MPEG               0x1001u
#define USB_AUDIO_10_FORMAT_AC3                0x1002u

#define USB_AUDIO_10_FORMAT_TYPE_III_UNDEFINED        0x2000u
#define USB_AUDIO_10_FORMAT_IEC1937_AC3               0x2001u
#define USB_AUDIO_10_FORMAT_IEC1937_MPEG1_LAYER1      0x2002u
#define USB_AUDIO_10_FORMAT_IEC1937_MPEG1_LAYER23     0x2003u
#define USB_AUDIO_10_FORMAT_IEC1937_MPEG2_NOEXT       0x2003u
#define USB_AUDIO_10_FORMAT_IEC1937_MPEG2_EXT         0x2004u
#define USB_AUDIO_10_FORMAT_IEC1937_MPEG2_LAYER1_LS   0x2005u
#define USB_AUDIO_10_FORMAT_IEC1937_MPEG2_LAYER23_LS  0x2006u

//
// UAC 1.0 AC interface descriptor subtypes
//
#define USB_AUDIO_10_AC_SUBTYPE_HEADER           0x01u
#define USB_AUDIO_10_AC_SUBTYPE_INPUT_TERMINAL   0x02u
#define USB_AUDIO_10_AC_SUBTYPE_OUTPUT_TERMINAL  0x03u
#define USB_AUDIO_10_AC_SUBTYPE_MIXER_UNIT       0x04u
#define USB_AUDIO_10_AC_SUBTYPE_SELECTOR_UNIT    0x05u
#define USB_AUDIO_10_AC_SUBTYPE_FEATURE_UNIT     0x06u
#define USB_AUDIO_10_AC_SUBTYPE_PROCESSING_UNIT  0x07u
#define USB_AUDIO_10_AC_SUBTYPE_EXTENSION_UNIT   0x08u

//
// UAC 1.0 AS interface descriptor subtypes
//
#define USB_AUDIO_10_AS_SUBTYPE_GENERAL          0x01u
#define USB_AUDIO_10_AS_SUBTYPE_FORMAT_TYPE      0x02u
#define USB_AUDIO_10_AS_SUBTYPE_FORMAT_SPECIFIC  0x03u

//
// UAC 1.0 class request codes
//
#define USB_AUDIO_10_REQ_SET_CUR   0x01u
#define USB_AUDIO_10_REQ_GET_CUR   0x81u
#define USB_AUDIO_10_REQ_SET_MIN   0x02u
#define USB_AUDIO_10_REQ_GET_MIN   0x82u
#define USB_AUDIO_10_REQ_SET_MAX   0x03u
#define USB_AUDIO_10_REQ_GET_MAX   0x83u
#define USB_AUDIO_10_REQ_SET_RES   0x04u
#define USB_AUDIO_10_REQ_GET_RES   0x84u
#define USB_AUDIO_10_REQ_SET_MEM   0x05u
#define USB_AUDIO_10_REQ_GET_MEM   0x85u
#define USB_AUDIO_10_REQ_GET_STAT  0xFFu

// bmRequestType values
#define USB_AUDIO_10_REQTYPE_SET_INTERFACE  0x21u  /* host-to-device, class, interface */
#define USB_AUDIO_10_REQTYPE_GET_INTERFACE  0xA1u  /* device-to-host, class, interface */
#define USB_AUDIO_10_REQTYPE_SET_ENDPOINT   0x22u  /* host-to-device, class, endpoint  */
#define USB_AUDIO_10_REQTYPE_GET_ENDPOINT   0xA2u  /* device-to-host, class, endpoint  */

//
// UAC 1.0 control selectors
//

// Endpoint control selectors (wValue high byte)
#define USB_AUDIO_10_EP_CS_SAMPLING_FREQ  0x01u
#define USB_AUDIO_10_EP_CS_PITCH          0x02u

// Class-Specific AS iso data endpoint descriptor (EP_GENERAL)
#define USB_AUDIO_10_EP_SUBTYPE_GENERAL  0x01u
// bmAttributes bits (UAC 1.0 Table 4-21)
#define USB_AUDIO_10_EP_ATTR_SAMPLING_FREQ_CTRL  BIT0

// Feature Unit control selectors (wValue high byte)
#define USB_AUDIO_10_FU_CS_MUTE            0x01u
#define USB_AUDIO_10_FU_CS_VOLUME          0x02u
#define USB_AUDIO_10_FU_CS_BASS            0x03u
#define USB_AUDIO_10_FU_CS_MID             0x04u
#define USB_AUDIO_10_FU_CS_TREBLE          0x05u
#define USB_AUDIO_10_FU_CS_GRAPHIC_EQ      0x06u
#define USB_AUDIO_10_FU_CS_AUTOMATIC_GAIN  0x07u
#define USB_AUDIO_10_FU_CS_DELAY           0x08u
#define USB_AUDIO_10_FU_CS_BASS_BOOST      0x09u
#define USB_AUDIO_10_FU_CS_LOUDNESS        0x0Au

//
// UAC 1.0 packed descriptor structures
//

#pragma pack(1)

/** Generic descriptor head used for walking descriptor lists. **/
typedef struct {
  UINT8    Len;
  UINT8    Type;
} USB_DESC_HEAD;
STATIC_ASSERT (sizeof (USB_DESC_HEAD) == 2);

/** UAC 1.0 AC HEADER descriptor. **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT16    BcdADC;
  UINT16    TotalLength;
  UINT8     InCollection;
  UINT8     BaInterfaceNr[]; /* InCollection entries */
} USB_AUDIO_10_AC_HEADER_DESC;

/** UAC 1.0 AC INPUT TERMINAL descriptor (12 bytes). **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     TerminalId;
  UINT16    TerminalType;
  UINT8     AssocTerminal;
  UINT8     NumChannels;
  UINT16    ChannelConfig;
  UINT8     ChannelNames;
  UINT8     Terminal;
} USB_AUDIO_10_INPUT_TERMINAL_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_10_INPUT_TERMINAL_DESC) == 12);

/** UAC 1.0 AC OUTPUT TERMINAL descriptor (9 bytes). **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     TerminalId;
  UINT16    TerminalType;
  UINT8     AssocTerminal;
  UINT8     SourceId;
  UINT8     Terminal;
} USB_AUDIO_10_OUTPUT_TERMINAL_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_10_OUTPUT_TERMINAL_DESC) == 9);

/**
  UAC 1.0 AC FEATURE UNIT descriptor header.
  Full size: 7 + (NrChannels+1)*bControlSize bytes.
  BmaControls[0] is master; BmaControls[1..N] are per-channel.
  Trailing iFeature byte follows the last BmaControls entry.
**/
typedef struct {
  UINT8    Len;
  UINT8    Type;
  UINT8    SubType;
  UINT8    UnitId;
  UINT8    SourceId;
  UINT8    ControlSize;
  UINT8    BmaControls[];
} USB_AUDIO_10_FEATURE_UNIT_DESC_HEADER;

/** UAC 1.0 AS GENERAL descriptor. **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     TerminalLink;
  UINT8     Delay;
  UINT16    FormatTag;
} USB_AUDIO_10_AS_GENERAL_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_10_AS_GENERAL_DESC) == 7);

/**
  UAC 1.0 FORMAT_TYPE_I descriptor header.
  When SamFreqType == 0: SamFreqs contains tLowerSamFreq[3] + tUpperSamFreq[3].
  When SamFreqType > 0: SamFreqs contains SamFreqType * 3 bytes (each a 3-byte LE Hz value).
**/
typedef struct {
  UINT8    Len;
  UINT8    Type;
  UINT8    SubType;
  UINT8    FormatType;
  UINT8    NrChannels;
  UINT8    SubframeSize;
  UINT8    BitResolution;
  UINT8    SamFreqType;
  UINT8    SamFreqs[];
} USB_AUDIO_10_FORMAT_TYPE_I_DESC_HEADER;

/** UAC 1.0 CS endpoint (EP_GENERAL) descriptor. **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     Attributes;
  UINT8     LockDelayUnits;
  UINT16    LockDelay;
} USB_AUDIO_10_AS_ISO_DATA_EP_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_10_AS_ISO_DATA_EP_DESC) == 7);

#pragma pack()

//
// UAC 2.0 AC interface descriptor subtypes
//
#define USB_AUDIO_20_AC_SUBTYPE_HEADER                 0x01u
#define USB_AUDIO_20_AC_SUBTYPE_INPUT_TERMINAL         0x02u
#define USB_AUDIO_20_AC_SUBTYPE_OUTPUT_TERMINAL        0x03u
#define USB_AUDIO_20_AC_SUBTYPE_MIXER_UNIT             0x04u
#define USB_AUDIO_20_AC_SUBTYPE_SELECTOR_UNIT          0x05u
#define USB_AUDIO_20_AC_SUBTYPE_FEATURE_UNIT           0x06u
#define USB_AUDIO_20_AC_SUBTYPE_EFFECT_UNIT            0x07u
#define USB_AUDIO_20_AC_SUBTYPE_PROCESSING_UNIT        0x08u
#define USB_AUDIO_20_AC_SUBTYPE_EXTENSION_UNIT         0x09u
#define USB_AUDIO_20_AC_SUBTYPE_CLOCK_SOURCE           0x0Au
#define USB_AUDIO_20_AC_SUBTYPE_CLOCK_SELECTOR         0x0Bu
#define USB_AUDIO_20_AC_SUBTYPE_CLOCK_MULTIPLIER       0x0Cu
#define USB_AUDIO_20_AC_SUBTYPE_SAMPLE_RATE_CONVERTER  0x0Du

//
// UAC 2.0 AS interface descriptor subtypes
//
#define USB_AUDIO_20_AS_SUBTYPE_GENERAL      0x01u
#define USB_AUDIO_20_AS_SUBTYPE_FORMAT_TYPE  0x02u
#define USB_AUDIO_20_AS_SUBTYPE_ENCODER      0x03u
#define USB_AUDIO_20_AS_SUBTYPE_DECODER      0x04u

//
// UAC 2.0 class request codes
//
#define USB_AUDIO_20_REQ_CUR    0x01u
#define USB_AUDIO_20_REQ_RANGE  0x02u
#define USB_AUDIO_20_REQ_MEM    0x03u

// bmRequestType values
#define USB_AUDIO_20_REQTYPE_SET_INTERFACE  0x21u  /* host-to-device, class, interface */
#define USB_AUDIO_20_REQTYPE_GET_INTERFACE  0xA1u  /* device-to-host, class, interface */

//
// UAC 2.0 control selectors
//

// Clock Source control selectors
#define USB_AUDIO_20_CS_SAM_FREQ_CONTROL     0x01u
#define USB_AUDIO_20_CS_CLOCK_VALID_CONTROL  0x02u

// Feature Unit control selectors
#define USB_AUDIO_20_FU_CS_MUTE            0x01u
#define USB_AUDIO_20_FU_CS_VOLUME          0x02u
#define USB_AUDIO_20_FU_CS_BASS            0x03u
#define USB_AUDIO_20_FU_CS_MID             0x04u
#define USB_AUDIO_20_FU_CS_TREBLE          0x05u
#define USB_AUDIO_20_FU_CS_GRAPHIC_EQ      0x06u
#define USB_AUDIO_20_FU_CS_AUTOMATIC_GAIN  0x07u
#define USB_AUDIO_20_FU_CS_DELAY           0x08u
#define USB_AUDIO_20_FU_CS_BASS_BOOST      0x09u
#define USB_AUDIO_20_FU_CS_LOUDNESS        0x0Au
#define USB_AUDIO_20_FU_CS_INPUT_GAIN      0x0Bu
#define USB_AUDIO_20_FU_CS_INPUT_GAIN_PAD  0x0Cu
#define USB_AUDIO_20_FU_CS_PHASE_INVERTER  0x0Du
#define USB_AUDIO_20_FU_CS_UNDERFLOW       0x0Eu
#define USB_AUDIO_20_FU_CS_OVERFLOW        0x0Fu
#define USB_AUDIO_20_FU_CS_LATENCY         0x10u
#define USB_AUDIO_20_FU_CS_HIGHPASS        0x11u

//
// UAC 2.0 packed descriptor structures
//

#pragma pack(1)

/** UAC 2.0 AC HEADER descriptor. **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT16    BcdADC;
  UINT8     Category;
  UINT16    TotalLength;
  UINT8     Controls;
} USB_AUDIO_20_AC_HEADER_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_20_AC_HEADER_DESC) == 9);

/** UAC 2.0 AC INPUT TERMINAL descriptor (17 bytes). **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     TerminalId;
  UINT16    TerminalType;
  UINT8     AssocTerminal;
  UINT8     ClockSourceId;
  UINT8     NumChannels;
  UINT32    ChannelConfig;
  UINT8     ChannelNames;
  UINT16    Controls;
  UINT8     Terminal;
} USB_AUDIO_20_INPUT_TERMINAL_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_20_INPUT_TERMINAL_DESC) == 17);

/** UAC 2.0 AC OUTPUT TERMINAL descriptor (12 bytes). **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     TerminalId;
  UINT16    TerminalType;
  UINT8     AssocTerminal;
  UINT8     SourceId;
  UINT8     ClockSourceId;
  UINT16    Controls;
  UINT8     Terminal;
} USB_AUDIO_20_OUTPUT_TERMINAL_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_20_OUTPUT_TERMINAL_DESC) == 12);

/** UAC 2.0 AC CLOCK SOURCE descriptor (8 bytes). **/
typedef struct {
  UINT8    Len;
  UINT8    Type;
  UINT8    SubType;
  UINT8    ClockId;
  UINT8    Attributes;
  UINT8    Controls;
  UINT8    AssocTerminal;
  UINT8    ClockSource;
} USB_AUDIO_20_CLOCK_SOURCE_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_20_CLOCK_SOURCE_DESC) == 8);

/** UAC 2.0 AC CLOCK SELECTOR descriptor header. **/
typedef struct {
  UINT8    Len;
  UINT8    Type;
  UINT8    SubType;
  UINT8    ClockId;
  UINT8    NumInputPins;
  UINT8    ClockSourceIds[];
} USB_AUDIO_20_CLOCK_SELECTOR_DESC_HEADER;

/** UAC 2.0 AC CLOCK MULTIPLIER descriptor. **/
typedef struct {
  UINT8    Len;
  UINT8    Type;
  UINT8    SubType;
  UINT8    ClockId;
  UINT8    ClockSourceId;
  UINT8    Controls;
  UINT8    ClockMultiplier;
} USB_AUDIO_20_CLOCK_MULTIPLIER_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_20_CLOCK_MULTIPLIER_DESC) == 7);

/**
  UAC 2.0 AC FEATURE UNIT descriptor header.
  Full size: 6 + (NrChannels+1)*4 bytes.
  BmaControls[0] is master; [1..N] are per-channel.
  Trailing iFeature byte follows the last UINT32 entry.
**/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     UnitId;
  UINT8     SourceId;
  UINT32    BmaControls[];
} USB_AUDIO_20_FEATURE_UNIT_DESC_HEADER;

/** UAC 2.0 AS GENERAL descriptor. **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     TerminalLink;
  UINT8     Controls;
  UINT8     FormatType;
  UINT32    Formats;          /* bmFormats: bit 0 = PCM, bit 1 = PCM8, ... */
  UINT8     NumChannels;
  UINT32    ChannelConfig;
  UINT8     ChannelNames;
} USB_AUDIO_20_AS_GENERAL_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_20_AS_GENERAL_DESC) == 16);

/**
  UAC 2.0 FORMAT_TYPE_I descriptor (6 bytes).
  Channels are carried in AS_GENERAL; sample rate via Clock Source.
**/
typedef struct {
  UINT8    Len;
  UINT8    Type;
  UINT8    SubType;
  UINT8    FormatType;      /* USB_AUDIO_FORMAT_TYPE_I */
  UINT8    SubslotSize;     /* 1, 2, 3, or 4 bytes per sample slot */
  UINT8    BitResolution;   /* significant bits <= SubslotSize * 8 */
} USB_AUDIO_20_FORMAT_TYPE_I_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_20_FORMAT_TYPE_I_DESC) == 6);

/** UAC 2.0 CS endpoint (EP_GENERAL) descriptor. **/
typedef struct {
  UINT8     Len;
  UINT8     Type;
  UINT8     SubType;
  UINT8     Attributes;
  UINT8     Controls;
  UINT8     LockDelayUnits;
  UINT16    LockDelay;
} USB_AUDIO_20_AS_ISO_DATA_EP_DESC;
STATIC_ASSERT (sizeof (USB_AUDIO_20_AS_ISO_DATA_EP_DESC) == 8);

/**
  UAC 2.0 Layout-3 RANGE sub-range triplet for UINT32 parameters
  (e.g. CS_SAM_FREQ_CONTROL).
**/
typedef struct {
  UINT32    Min;
  UINT32    Max;
  UINT32    Res;
} USB_AUDIO_20_RANGE_UINT32_SUBRANGE;
STATIC_ASSERT (sizeof (USB_AUDIO_20_RANGE_UINT32_SUBRANGE) == 12);

/** UAC 2.0 Layout-2 RANGE sub-range triplet for INT16 parameters (volume). **/
typedef struct {
  INT16    Min;
  INT16    Max;
  INT16    Res;
} USB_AUDIO_20_RANGE_INT16_SUBRANGE;
STATIC_ASSERT (sizeof (USB_AUDIO_20_RANGE_INT16_SUBRANGE) == 6);

#pragma pack()

//
// UAC 2.0 bmFormats bit positions for Type I (AS_GENERAL.Formats)
//

#define USB_AUDIO_20_FORMAT_PCM         BIT0
#define USB_AUDIO_20_FORMAT_PCM8        BIT1
#define USB_AUDIO_20_FORMAT_IEEE_FLOAT  BIT2
#define USB_AUDIO_20_FORMAT_ALAW        BIT3
#define USB_AUDIO_20_FORMAT_MULAW       BIT4
