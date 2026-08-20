/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       COMMONDLG.H
*
*  VERSION:     1.05
*
*  DATE:        15 Aug 2026
*
*  Common header file for the program common dialogs.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#pragma once

BOOL InitEtwxCommonDialogs();

BOOL ShowEventIdFilterDialog(
    _In_ HWND ParentWindow);

BOOL ShowKeywordDialog(
    _In_ HWND ParentWindow);

BOOL ShowLiveEventLimitDialog(
    _In_ HWND ParentWindow);

VOID ShowEventDetailsDialog(
    _In_ HWND ParentWindow,
    _In_ INT row);

VOID ShowEventFindDialog(
    _In_ HWND ParentWindow);

BOOL ShowAboutDialog(
    _In_ HWND ParentWindow);

HWND ShowMetadataDialog(
    _In_ HWND ParentWindow,
    _In_ LPCWSTR ProviderName,
    _In_ LPCWSTR SchemaXml);

VOID ShowSystemInformationDialog(
    _In_ HWND ParentWindow);
