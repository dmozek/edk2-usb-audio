/** @file
  Helpers for drawing a menu

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Menu.h"
#include <Library/UefiBootServicesTableLib.h>

STATIC
VOID
PrintStringAt (
  IN UINTN   Column,
  IN UINTN   Row,
  IN CHAR16  *String
  )
{
  gST->ConOut->SetCursorPosition (gST->ConOut, Column, Row);
  Print (L" %s", String);
}

/**
  Draw a menu with the given title and items, highlighting the selected item.

  If PreviousSelected is larger than or equal to MenuSize, the entire menu is redrawn.
  Otherwise, only the previously selected and currently selected items are updated.

  @param[in] Selected          The index of the currently selected menu item.
  @param[in] PreviousSelected  The index of the previously selected menu item.
  @param[in] Title             The title of the menu.
  @param[in] Menu              The menu items.
  @param[in] MenuSize          The number of items in the menu.
 **/
VOID
DrawMenu (
  IN UINTN      Selected,
  IN UINTN      PreviousSelected,
  IN CHAR16     *Title,
  IN MENU_ITEM  *Menu,
  IN UINTN      MenuSize
  )
{
  if (PreviousSelected < MenuSize) {
    gST->ConOut->SetAttribute (
                   gST->ConOut,
                   EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK
                   );
    PrintStringAt (0, PreviousSelected + 2, Menu[PreviousSelected].Text);
    gST->ConOut->SetAttribute (
                   gST->ConOut,
                   EFI_WHITE | EFI_BACKGROUND_BLUE
                   );
    PrintStringAt (0, Selected + 2, Menu[Selected].Text);
  } else {
    gST->ConOut->ClearScreen (gST->ConOut);
    Print (L"=== %s ===\n\n", Title);
    for (UINTN i = 0; i < MenuSize; i++) {
      if (i == Selected) {
        gST->ConOut->SetAttribute (
                       gST->ConOut,
                       EFI_WHITE | EFI_BACKGROUND_BLUE
                       );
      } else {
        gST->ConOut->SetAttribute (
                       gST->ConOut,
                       EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK
                       );
      }

      Print (L" %s\n", Menu[i].Text);
    }
  }

  gST->ConOut->SetAttribute (
                 gST->ConOut,
                 EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK
                 );
}

/**
  Draw a menu and handle user input to select an item.

  Arrow keys move the selection, Enter selects the current item, and Backspace or Escape cancels the menu.

  @param[in]  Menu              The menu items.
  @param[in]  MenuSize          The number of items in the menu.
  @param[in]  Title             The title of the menu.
  @param[out] SelectedIndex     The index of the selected item, or MenuSize if cancelled.
 **/
VOID
HandleMenuInput (
  IN MENU_ITEM  *Menu,
  IN UINTN      MenuSize,
  IN CHAR16     *Title,
  OUT UINTN     *SelectedIndex
  )
{
  EFI_INPUT_KEY  Key;
  UINTN          Index;
  UINTN          Selected         = 0;
  UINTN          PreviousSelected = MenuSize;

  DrawMenu (Selected, PreviousSelected, Title, Menu, MenuSize);

  while (TRUE) {
    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
    if (EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) {
      continue;
    }

    PreviousSelected = Selected;

    switch (Key.UnicodeChar) {
      case CHAR_CARRIAGE_RETURN:
        *SelectedIndex = Selected;
        gST->ConOut->ClearScreen (gST->ConOut);
        return;
      case CHAR_BACKSPACE:
        *SelectedIndex = MenuSize;
        gST->ConOut->ClearScreen (gST->ConOut);
        return;
      default:
        break;
    }

    switch (Key.ScanCode) {
      case SCAN_UP:
        if (Selected > 0) {
          Selected--;
        } else {
          Selected = MenuSize - 1;
        }

        break;
      case SCAN_DOWN:
        if (Selected < MenuSize - 1) {
          Selected++;
        } else {
          Selected = 0;
        }

        break;

      case SCAN_ESC:
        *SelectedIndex = MenuSize;
        gST->ConOut->ClearScreen (gST->ConOut);
        return;
      default:
        break;
    }

    if (Selected != PreviousSelected) {
      DrawMenu (Selected, PreviousSelected, Title, Menu, MenuSize);
    }
  }
}

/**
  Read a line of input from the user into the provided buffer, echoing characters as they are typed.

  @param[out] Buffer        The buffer to store the input.
  @param[in]  BufferSize    The size of the buffer.
 **/
VOID
ReadLine (
  OUT CHAR16  *Buffer,
  IN  UINTN   BufferSize
  )
{
  EFI_INPUT_KEY  Key;
  UINTN          Index;
  UINTN          Length;
  CHAR16         Echo[2];

  Length  = 0;
  Echo[1] = L'\0';

  for ( ; ;) {
    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
    if (EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) {
      continue;
    }

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      Print (L"\n");
      break;
    }

    if (Key.UnicodeChar == CHAR_BACKSPACE) {
      if (Length > 0) {
        Length--;
        Print (L"\b \b");
      }

      continue;
    }

    // Ignore non-printing keys (arrows, F-keys arrive with UnicodeChar == 0)
    if ((Key.UnicodeChar != 0) && (Length < BufferSize - 1)) {
      Buffer[Length++] = Key.UnicodeChar;
      Echo[0]          = Key.UnicodeChar;
      Print (L"%s", Echo);
    }
  }

  Buffer[Length] = L'\0';
}

/**
  Wait for the user to press any key before continuing.
 **/
VOID
PressKeyToContinue (
  VOID
  )
{
  EFI_INPUT_KEY  Key;
  UINTN          Index;

  Print (L"\nPress any key to continue...\n");
  gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
  gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
}
