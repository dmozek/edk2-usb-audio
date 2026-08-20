/** @file
  USB Audio isochronous stream manager.

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "AudioStream.h"
#include "AudioFormats.h"

#define USB_AUDIO_DRAIN_POLL_US    1000u   /* 1 ms per poll */
#define USB_AUDIO_DRAIN_MAX_POLLS  50u     /* ~50 ms total before giving up */

/**
  Initialize per-slot states, allocate the per-slot packet buffers and reset
  queue state.

  @param[in,out] Stream  Stream context to initialize. MaxPacketSize must
                         already reflect the selected alternate setting.

  @retval EFI_SUCCESS            Slots initialized.
  @retval EFI_INVALID_PARAMETER  The stream has no usable max packet size.
  @retval EFI_OUT_OF_RESOURCES   A packet buffer allocation failed.
**/
EFI_STATUS
InitStreams (
  IN OUT USB_AUDIO_STREAM_CTX  *Stream
  )
{
  UINTN  Index;

  if (Stream->MaxPacketSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Stream->PendingHead           = 0;
  Stream->PendingCount          = 0;
  Stream->NextPendingToTransfer = 0;
  Stream->Stopped               = FALSE;
  Stream->NextSlotToFill        = 0;
  Stream->RateCarry             = 0;

  for (Index = 0; Index < AUDIO_OUTPUT_MAX_PENDING_BUFFERS; Index++) {
    Stream->PendingQueue[Index].IsValid = FALSE;
  }

  for (Index = 0; Index < USB_AUDIO_TRANSFER_SLOT_COUNT; Index++) {
    if (Stream->Slots[Index].PacketBuffer != NULL) {
      FreePool (Stream->Slots[Index].PacketBuffer);
    }

    Stream->Slots[Index].PacketBuffer = AllocatePool (Stream->MaxPacketSize);
    if (Stream->Slots[Index].PacketBuffer == NULL) {
      ResetStreams (Stream);
      return EFI_OUT_OF_RESOURCES;
    }

    Stream->Slots[Index].InUse             = FALSE;
    Stream->Slots[Index].PendingEntryIndex = 0;
    Stream->Slots[Index].IsLast            = FALSE;
  }

  return EFI_SUCCESS;
}

/**
  Reset per-slot transfer state and release the per-slot packet buffers.

  @param[in] Stream  Stream context to tear down.
**/
VOID
ResetStreams (
  IN USB_AUDIO_STREAM_CTX  *Stream
  )
{
  UINTN  Index;

  DEBUG ((DEBUG_INFO, "UsbAudioDxe: ResetStreams: releasing slot buffers\n"));

  Stream->NextSlotToFill        = 0;
  Stream->PendingCount          = 0;
  Stream->NextPendingToTransfer = 0;
  Stream->RateCarry             = 0;

  for (Index = 0; Index < USB_AUDIO_TRANSFER_SLOT_COUNT; Index++) {
    if (Stream->Slots[Index].PacketBuffer != NULL) {
      FreePool (Stream->Slots[Index].PacketBuffer);
      Stream->Slots[Index].PacketBuffer = NULL;
    }

    Stream->Slots[Index].InUse             = FALSE;
    Stream->Slots[Index].PendingEntryIndex = 0;
    Stream->Slots[Index].IsLast            = FALSE;
  }
}

/**
  Enqueue a caller buffer for async playback.

  @param[in] Dev         Main device context.
  @param[in] Buffer      PCM audio data in the configured format (not copied).
  @param[in] BufferSize  Length in bytes. Must be a non-zero multiple of
                         channels times subslot size.
  @param[in] Callback    Called when Buffer may be reused.
  @param[in] Context     Forwarded to Callback.

  @retval EFI_SUCCESS            Buffer enqueued.
  @retval EFI_OUT_OF_RESOURCES   Pending queue is full.
  @retval EFI_INVALID_PARAMETER  BufferSize is zero or not frame-aligned.
**/
EFI_STATUS
EnqueueAudioStream (
  IN USB_AUDIO_DEV                     *Dev,
  IN VOID                              *Buffer,
  IN UINTN                             BufferSize,
  IN EFI_AUDIO_OUTPUT_BUFFER_COMPLETE  Callback,
  IN VOID                              *Context   OPTIONAL
  )
{
  EFI_STATUS  Status;
  UINTN       TailIndex;
  EFI_TPL     OldTpl;

  ASSERT (Dev != NULL);
  ASSERT (Dev->SupportedFormats != NULL);
  ASSERT (Dev->CurrentFormatIndex < Dev->SupportedFormatCount);

  if ((BufferSize == 0) ||
      ((BufferSize % GetCurrentFrameSize (Dev)) != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "UsbAudioDxe: EnqueueAudioStream: bad BufferSize %u (not a non-zero frame multiple)\n",
      BufferSize
      ));
    return EFI_INVALID_PARAMETER;
  }

  OldTpl = gBS->RaiseTPL (USB_AUDIO_TPL);

  if (Dev->Stream.PendingCount >= AUDIO_OUTPUT_MAX_PENDING_BUFFERS) {
    DEBUG ((
      DEBUG_WARN,
      "UsbAudioDxe: EnqueueAudioStream: pending queue full (%u), rejecting buffer\n",
      Dev->Stream.PendingCount
      ));
    gBS->RestoreTPL (OldTpl);
    return EFI_OUT_OF_RESOURCES;
  } else if ((Dev->Stream.PendingCount == 0) ||
             (Dev->Stream.Stopped))
  {
    DEBUG ((
      DEBUG_INFO,
      "UsbAudioDxe: EnqueueAudioStream: first buffer, starting %u ms pump timer\n",
      (UINTN)USB_AUDIO_TRANSFER_SLOT_INTERVAL_MS
      ));
    Status = gBS->SetTimer (
                    Dev->StreamTimer,
                    TimerPeriodic,
                    EFI_TIMER_PERIOD_MILLISECONDS (USB_AUDIO_TRANSFER_SLOT_INTERVAL_MS)
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "UsbAudioDxe: EnqueueAudioStream: SetTimer failed: %r\n", Status));
      gBS->RestoreTPL (OldTpl);
      return Status;
    }

    Dev->Stream.RateCarry = 0;
  }

  Dev->Stream.Stopped = FALSE;

  TailIndex = (Dev->Stream.PendingHead + Dev->Stream.PendingCount) % AUDIO_OUTPUT_MAX_PENDING_BUFFERS;

  Dev->Stream.PendingQueue[TailIndex].Buffer            = Buffer;
  Dev->Stream.PendingQueue[TailIndex].BufferSize        = BufferSize;
  Dev->Stream.PendingQueue[TailIndex].Callback          = Callback;
  Dev->Stream.PendingQueue[TailIndex].Context           = Context;
  Dev->Stream.PendingQueue[TailIndex].IsValid           = TRUE;
  Dev->Stream.PendingQueue[TailIndex].Cancelled         = FALSE;
  Dev->Stream.PendingQueue[TailIndex].FinalSliceSeen    = FALSE;
  Dev->Stream.PendingQueue[TailIndex].HadError          = FALSE;
  Dev->Stream.PendingQueue[TailIndex].ConsumedFrames    = 0;
  Dev->Stream.PendingQueue[TailIndex].OutstandingSlices = 0;

  Dev->Stream.PendingCount++;

  DEBUG ((
    DEBUG_INFO,
    "UsbAudioDxe: EnqueueAudioStream: queued %u bytes at slot %u, pending now %u\n",
    BufferSize,
    TailIndex,
    Dev->Stream.PendingCount
    ));

  gBS->RestoreTPL (OldTpl);
  return EFI_SUCCESS;
}

/**
  Retire the given pending entry.

  Advances the pending queue head and count past all invalidated entries.

  @param[in] Stream             Stream context.
  @param[in] PendingEntry       The pending entry to retire.
  @param[in] CompletionStatus   Status to pass to the caller's callback.
  @param[in,out] OldTpl         Pointer to the caller's TPL, which may be dropped for the callback.
  @param[in] DropTplForCallback TRUE to drop TPL for the callback, FALSE to keep it raised.
**/
STATIC
VOID
RetirePendingEntry (
  IN     USB_AUDIO_STREAM_CTX     *Stream,
  IN     USB_AUDIO_PENDING_ENTRY  *PendingEntry,
  IN     EFI_STATUS               CompletionStatus,
  IN OUT EFI_TPL                  *OldTpl,
  IN     BOOLEAN                  DropTplForCallback
  )
{
  PendingEntry->IsValid = FALSE;

  while ((Stream->PendingCount > 0) &&
         !Stream->PendingQueue[Stream->PendingHead].IsValid)
  {
    Stream->PendingHead = (Stream->PendingHead + 1) % AUDIO_OUTPUT_MAX_PENDING_BUFFERS;
    Stream->PendingCount--;
  }

  if (PendingEntry->Callback != NULL) {
    if (DropTplForCallback) {
      gBS->RestoreTPL (*OldTpl);
    }

    PendingEntry->Callback (
                    PendingEntry->Buffer,
                    PendingEntry->BufferSize,
                    PendingEntry->Context,
                    CompletionStatus
                    );

    if (DropTplForCallback) {
      *OldTpl = gBS->RaiseTPL (USB_AUDIO_TPL);
    }
  }
}

/**
  Cancel the pending entry at the given index, firing its callback if it is no longer in-flight.

  @param[in] Stream      Stream context.
  @param[in] EntryIndex  Index of the pending entry to cancel.

**/
STATIC
VOID
CancelPendingEntry (
  IN USB_AUDIO_STREAM_CTX  *Stream,
  IN UINTN                 EntryIndex
  )
{
  USB_AUDIO_PENDING_ENTRY  *PendingEntry;

  PendingEntry = &Stream->PendingQueue[EntryIndex];
  if (PendingEntry->IsValid) {
    DEBUG ((DEBUG_INFO, "UsbAudioDxe: CancelPendingEntry: aborting pending entry %u\n", EntryIndex));
    PendingEntry->Cancelled = TRUE;
    if (PendingEntry->OutstandingSlices == 0) {
      RetirePendingEntry (
        Stream,
        PendingEntry,
        EFI_ABORTED,
        NULL,
        FALSE
        );
    }
  }
}

/**
  Cancel all pending transfers and drain the in-flight ones.

  @param[in] Dev  Main device context.

  @retval TRUE   All transfers drained.
  @retval FALSE  Timeout occurred while draining transfers.
**/
BOOLEAN
StopAudio (
  IN USB_AUDIO_DEV  *Dev
  )
{
  UINTN    Index;
  EFI_TPL  OldTpl;

  ASSERT (Dev != NULL);

  DEBUG ((
    DEBUG_INFO,
    "UsbAudioDxe: StopAudio: cancelling %u pending buffers, stopping pump\n",
    Dev->Stream.PendingCount
    ));

  OldTpl = gBS->RaiseTPL (USB_AUDIO_TPL);

  Dev->Stream.Stopped = TRUE;

  gBS->SetTimer (
         Dev->StreamTimer,
         TimerCancel,
         0
         );

  gBS->RestoreTPL (OldTpl);
  if (!DrainStreams (&Dev->Stream)) {
    return FALSE;
  }

  OldTpl = gBS->RaiseTPL (USB_AUDIO_TPL);

  for (Index = 0; Index < AUDIO_OUTPUT_MAX_PENDING_BUFFERS; Index++) {
    CancelPendingEntry (&Dev->Stream, Index);
  }

  Dev->Stream.PendingCount          = 0;
  Dev->Stream.PendingHead           = 0;
  Dev->Stream.NextPendingToTransfer = 0;
  Dev->Stream.NextSlotToFill        = 0;

  gBS->RestoreTPL (OldTpl);

  return TRUE;
}

/**
  Callback for USB async isochronous audio transfers.  Called when a transfer completes.
  Calls the original caller's callback when the last slice of a buffer completes, or when a cancelled buffer completes.

  @param[in] Data        Pointer to the transfer buffer (not owned).
  @param[in] DataLength  Length of the transfer buffer in bytes.
  @param[in] Context     Pointer to the STREAM_CALLBACK_CTX for this transfer.
  @param[in] Status      EFI_SUCCESS if the transfer completed successfully, or an error code.

  @retval EFI_SUCCESS  Callback handled successfully.
**/
STATIC
EFI_STATUS
EFIAPI
TransferCallback (
  IN VOID    *Data,
  IN UINTN   DataLength,
  IN VOID    *Context,
  IN UINT32  Status
  )
{
  STREAM_CALLBACK_CTX      *CallbackCtx;
  USB_AUDIO_STREAM_CTX     *Stream;
  USB_AUDIO_TRANSFER_SLOT  *Slot;
  USB_AUDIO_PENDING_ENTRY  *PendingEntry;
  EFI_TPL                  OldTpl;

  CallbackCtx = (STREAM_CALLBACK_CTX *)Context;
  ASSERT (CallbackCtx != NULL);

  Stream = CallbackCtx->Stream;
  ASSERT (Stream != NULL);

  Slot = &Stream->Slots[CallbackCtx->SlotIndex];
  ASSERT (Slot != NULL);

  PendingEntry = &Stream->PendingQueue[Slot->PendingEntryIndex];
  ASSERT (PendingEntry != NULL);

  OldTpl = gBS->RaiseTPL (USB_AUDIO_TPL);
  if (!PendingEntry->IsValid) {
    DEBUG ((
      DEBUG_VERBOSE,
      "UsbAudioDxe: TransferCallback: slot %u completed for cancelled buffer, discarding\n",
      CallbackCtx->SlotIndex
      ));
    Slot->InUse = FALSE;
    Stream->InFlightTransfers--;
    gBS->RestoreTPL (OldTpl);
    return EFI_SUCCESS; /* This can happen if the buffer was cancelled while the transfer was in flight */
  }

  if (Status != EFI_USB_NOERROR) {
    DEBUG ((
      DEBUG_WARN,
      "UsbAudioDxe: TransferCallback: slot %u USB status 0x%x, %u bytes\n",
      CallbackCtx->SlotIndex,
      Status,
      DataLength
      ));
    PendingEntry->HadError = TRUE;
  }

  PendingEntry->OutstandingSlices--;
  if (Slot->IsLast) {
    PendingEntry->FinalSliceSeen = TRUE;
  }

  //
  // Retire only once every submitted packet of the entry has completed.
  //
  if (PendingEntry->OutstandingSlices == 0) {
    if (PendingEntry->Cancelled) {
      RetirePendingEntry (
        Stream,
        PendingEntry,
        EFI_ABORTED,
        &OldTpl,
        TRUE
        );
    } else if (PendingEntry->FinalSliceSeen) {
      RetirePendingEntry (
        Stream,
        PendingEntry,
        PendingEntry->HadError ? EFI_DEVICE_ERROR : EFI_SUCCESS,
        &OldTpl,
        TRUE
        );
    }
  }

  Slot->InUse = FALSE;
  Stream->InFlightTransfers--;

  gBS->RestoreTPL (OldTpl);

  return EFI_SUCCESS;
}

/**
  Fill free transfer slots with one rate-matched isochronous packet each and
  submit them, keeping TargetInFlight packets queued with the host controller.

  @param[in] Event    Timer event that triggered this call (unused).
  @param[in] Context  Pointer to the USB_AUDIO_DEV structure.
**/
VOID
EFIAPI
ManageAudioStreams (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  USB_AUDIO_DEV            *Dev;
  USB_AUDIO_PENDING_ENTRY  *PendingEntry;
  USB_AUDIO_TRANSFER_SLOT  *Slot;
  USB_AUDIO_STREAM_CTX     *Stream;
  STREAM_CALLBACK_CTX      *CallbackCtx;
  UINTN                    TotalFramesRemaining;
  UINTN                    OutFrameCount;
  EFI_STATUS               Status;
  EFI_TPL                  OldTpl;
  UINTN                    InputFrameSize;

  Dev = (USB_AUDIO_DEV *)Context;
  ASSERT (Dev != NULL);

  Stream = &Dev->Stream;
  ASSERT (Stream != NULL);

  ASSERT (Dev->SupportedFormats != NULL);
  ASSERT (Dev->CurrentFormatIndex < Dev->SupportedFormatCount);

  InputFrameSize = GetCurrentFrameSize (Dev);

  //
  // The whole fill loop runs at USB_AUDIO_TPL, including the submits, since
  // a second pump instance entering between two submits could interleave
  // packets and reorder audio on the wire.
  //
  OldTpl = gBS->RaiseTPL (USB_AUDIO_TPL);

  Slot = &Stream->Slots[Stream->NextSlotToFill];

  while ( !Stream->Stopped &&
          (Stream->PendingQueue[Stream->NextPendingToTransfer].IsValid) &&
          (Slot->InUse == FALSE) &&
          (Stream->InFlightTransfers < Stream->TargetInFlight))
  {
    PendingEntry = &Stream->PendingQueue[Stream->NextPendingToTransfer];

    TotalFramesRemaining = (PendingEntry->BufferSize / InputFrameSize) - PendingEntry->ConsumedFrames;

    //
    // Frames for one service interval
    //
    Stream->RateCarry += Stream->DeviceSampleRateHz;
    OutFrameCount      = Stream->RateCarry / Stream->IntervalsPerSecond;
    OutFrameCount      = MIN (OutFrameCount, Stream->MaxFramesPerPacket);
    OutFrameCount      = MIN (OutFrameCount, TotalFramesRemaining);
    if (OutFrameCount == 0) {
      break;
    }

    Stream->RateCarry -= OutFrameCount * Stream->IntervalsPerSecond;

    CopyMem (
      Slot->PacketBuffer,
      (UINT8 *)PendingEntry->Buffer + (PendingEntry->ConsumedFrames * InputFrameSize),
      OutFrameCount * InputFrameSize
      );
    PendingEntry->ConsumedFrames += OutFrameCount;

    Slot->InUse             = TRUE;
    Slot->PendingEntryIndex = Stream->NextPendingToTransfer;

    Slot->IsLast = OutFrameCount == TotalFramesRemaining;

    if (Slot->IsLast) {
      Stream->NextPendingToTransfer = (Stream->NextPendingToTransfer + 1) % AUDIO_OUTPUT_MAX_PENDING_BUFFERS;
    }

    CallbackCtx            = &Stream->CallbackCtxPool[Stream->NextSlotToFill];
    CallbackCtx->Stream    = Stream;
    CallbackCtx->SlotIndex = Stream->NextSlotToFill;

    PendingEntry->OutstandingSlices++;
    Stream->InFlightTransfers++;

    Status = Stream->UsbIo->UsbAsyncIsochronousTransfer (
                              Stream->UsbIo,
                              Stream->EndpointAddr,
                              Slot->PacketBuffer,
                              OutFrameCount * InputFrameSize,
                              TransferCallback,
                              CallbackCtx
                              );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "UsbAudioDxe: ManageAudioStreams: packet submit failed: %r\n", Status));
      Slot->InUse = FALSE;
      PendingEntry->OutstandingSlices--;
      Stream->InFlightTransfers--;
      CancelPendingEntry (Stream, Slot->PendingEntryIndex);
      if (!Slot->IsLast) {
        Stream->NextPendingToTransfer = (Stream->NextPendingToTransfer + 1) % AUDIO_OUTPUT_MAX_PENDING_BUFFERS;
      }

      break;
    }

    Stream->NextSlotToFill = (Stream->NextSlotToFill + 1) % USB_AUDIO_TRANSFER_SLOT_COUNT;
    Slot                   = &Stream->Slots[Stream->NextSlotToFill];
  }

  gBS->RestoreTPL (OldTpl);

  if (Stream->PendingCount == 0) {
    gBS->SetTimer (
           Dev->StreamTimer,
           TimerCancel,
           0
           );
  }
}

/**
  Compare two USB I/O device paths for equality.

  @param[in] DevicePath1  First device path to compare.
  @param[in] DevicePath2  Second device path to compare.

  @retval TRUE   Device paths are equal.
  @retval FALSE  Device paths are not equal.
**/
STATIC
BOOLEAN
CompareUsbIoDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath1,
  IN EFI_DEVICE_PATH_PROTOCOL  *DevicePath2
  )
{
  UINTN            Len1;
  UINTN            Len2;
  BOOLEAN          IsUsbNode;
  BOOLEAN          IsTerminal;
  USB_DEVICE_PATH  *Usb1;
  USB_DEVICE_PATH  *Usb2;

  while (!IsDevicePathEnd (DevicePath1) && !IsDevicePathEnd (DevicePath2)) {
    Len1 = DevicePathNodeLength (DevicePath1);
    Len2 = DevicePathNodeLength (DevicePath2);

    if (Len1 != Len2) {
      return FALSE;
    } else if (Len1 == 0) {
      return TRUE;
    }

    IsUsbNode = (DevicePath1->Type    == MESSAGING_DEVICE_PATH)
                && (DevicePath1->SubType == MSG_USB_DP);

    IsTerminal = IsDevicePathEnd (NextDevicePathNode (DevicePath1))
                 && IsDevicePathEnd (NextDevicePathNode (DevicePath2));

    if (IsUsbNode && IsTerminal) {
      //
      // Last USB node: same port, different interface means the same physical device.
      // Compare only ParentPortNumber; ignore InterfaceNumber.
      //
      Usb1 = (USB_DEVICE_PATH *)DevicePath1;
      Usb2 = (USB_DEVICE_PATH *)DevicePath2;

      return Usb1->ParentPortNumber == Usb2->ParentPortNumber;
    }

    // All other nodes must match exactly.
    if (CompareMem (DevicePath1, DevicePath2, Len1) != 0) {
      return FALSE;
    }

    DevicePath1 = NextDevicePathNode (DevicePath1);
    DevicePath2 = NextDevicePathNode (DevicePath2);
  }

  // Paths must both terminate at the same depth.
  return IsDevicePathEnd (DevicePath1) && IsDevicePathEnd (DevicePath2);
}

/**
  Open the USB I/O protocol for the audio stream.

  @param[in,out] Dev         Main device context.
  @param[in]     AgentHandle Handle of the agent opening the protocol.

  @retval EFI_SUCCESS           Protocol opened successfully.
  @retval EFI_NOT_FOUND         No matching USB I/O protocol found.
**/
EFI_STATUS
OpenAudioStreamIo (
  IN OUT USB_AUDIO_DEV  *Dev,
  IN     EFI_HANDLE     AgentHandle
  )
{
  EFI_STATUS                    Status;
  EFI_HANDLE                    *UsbHandles;
  UINTN                         HandleCount;
  UINTN                         Index;
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_INTERFACE_DESCRIPTOR  IfDesc;
  USB_AUDIO_STREAM_CTX          *Stream;

  ASSERT ((Dev != NULL) && (Dev->DevicePath != NULL));
  Stream           = &Dev->Stream;
  Stream->UsbIo    = NULL;
  Stream->AsHandle = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiUsbIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &UsbHandles
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EFI_NOT_FOUND;
  for (Index = 0; Index < HandleCount; Index++) {
    if (UsbHandles[Index] == Dev->ControllerHandle) {
      continue;
    }

    if (EFI_ERROR (
          gBS->OpenProtocol (
                 UsbHandles[Index],
                 &gEfiDevicePathProtocolGuid,
                 (VOID **)&DevicePath,
                 AgentHandle,
                 Dev->ControllerHandle,
                 EFI_OPEN_PROTOCOL_GET_PROTOCOL
                 )
          ))
    {
      continue;
    }

    if (!CompareUsbIoDevicePath (DevicePath, Dev->DevicePath)) {
      continue;
    }

    Status = gBS->OpenProtocol (
                    UsbHandles[Index],
                    &gEfiUsbIoProtocolGuid,
                    (VOID **)&UsbIo,
                    AgentHandle,
                    Dev->ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (!EFI_ERROR (UsbIo->UsbGetInterfaceDescriptor (UsbIo, &IfDesc)) &&
        (IfDesc.InterfaceNumber == Stream->InterfaceNumber))
    {
      Stream->UsbIo    = UsbIo;
      Stream->AsHandle = UsbHandles[Index];
      Status           = EFI_SUCCESS;
      DEBUG ((
        DEBUG_INFO,
        "UsbAudioDxe: OpenAudioStreamIo: bound AS interface %u (handle %p)\n",
        Stream->InterfaceNumber,
        UsbHandles[Index]
        ));
      break;
    }

    gBS->CloseProtocol (
           UsbHandles[Index],
           &gEfiUsbIoProtocolGuid,
           AgentHandle,
           Dev->ControllerHandle
           );
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "UsbAudioDxe: OpenAudioStreamIo: no sibling UsbIo for AS interface %u: %r\n",
      Stream->InterfaceNumber,
      Status
      ));
  }

  FreePool (UsbHandles);
  return Status;
}

/**
  Drain all pending transfers from the audio stream.

  @param[in] Stream  Stream context to drain.

  @retval TRUE   All transfers completed.
  @retval FALSE  Timeout occurred.
**/
BOOLEAN
DrainStreams (
  IN USB_AUDIO_STREAM_CTX  *Stream
  )
{
  UINTN  Count;

  ASSERT (Stream != NULL);

  for (Count = 0; Stream->InFlightTransfers > 0 && Count < USB_AUDIO_DRAIN_MAX_POLLS; Count++) {
    gBS->Stall (USB_AUDIO_DRAIN_POLL_US);
  }

  if (Stream->InFlightTransfers > 0) {
    DEBUG ((
      DEBUG_WARN,
      "UsbAudioDxe: DrainStreams: timed out with %u transfers still in flight\n",
      Stream->InFlightTransfers
      ));
    return FALSE;
  }

  return TRUE;
}
