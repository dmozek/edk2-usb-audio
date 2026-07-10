/** @file
  Helpers for drawing a menu

Copyright (c) 2026, David Mozek. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Uefi.h>
#include <Library/UefiLib.h>

typedef struct {
  CHAR16    *Text;
} MENU_ITEM;

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
  );

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
  );

/**
  Read a line of input from the user into the provided buffer, echoing characters as they are typed.

  @param[out] Buffer        The buffer to store the input.
  @param[in]  BufferSize    The size of the buffer.
 **/
VOID
ReadLine (
  OUT CHAR16  *Buffer,
  IN  UINTN   BufferSize
  );

/**
  Wait for the user to press any key before continuing.
 **/
VOID
PressKeyToContinue (
  VOID
  );
