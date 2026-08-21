/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       SUP.CPP
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#include "global.h"

#ifdef _DEBUG
SUP_HEAP_STATS g_HeapStats;
#endif

/*
* supHeapAlloc
*
* Purpose:
*
* Wrapper for process heap allocation.
*
*/
PVOID NTAPI supHeapAlloc(
    _In_ SIZE_T Size
)
{
    PVOID BaseAddress;

    BaseAddress = HeapAlloc(g_Heap, HEAP_ZERO_MEMORY, Size);
#ifdef _DEBUG
    if (BaseAddress != NULL) {
        InterlockedIncrement64(&g_HeapStats.AllocCount);
        InterlockedAdd64(&g_HeapStats.AllocBytes, (LONG64)Size);
        InterlockedIncrement64(&g_HeapStats.OutstandingCount);
    }
#endif
    return BaseAddress;
}

/*
* supHeapReAlloc
*
* Purpose:
*
* Wrapper for process heap reallocation.
*
*/
PVOID NTAPI supHeapReAlloc(
    _In_ PVOID BaseAddress,
    _In_ SIZE_T Size
)
{
    PVOID NewAddress;

    if (BaseAddress == NULL)
        return supHeapAlloc(Size);

    NewAddress = HeapReAlloc(g_Heap, HEAP_ZERO_MEMORY, BaseAddress, Size);
#ifdef _DEBUG
    if (NewAddress != NULL) {
        InterlockedIncrement64(&g_HeapStats.ReAllocCount);
        InterlockedAdd64(&g_HeapStats.ReAllocBytes, (LONG64)Size);
    }
#endif
    return NewAddress;
}

/*
* supHeapFree
*
* Purpose:
*
* Wrapper for process heap free.
*
*/
BOOL NTAPI supHeapFree(
    _In_ PVOID BaseAddress
)
{
    BOOL Result;

    if (BaseAddress == NULL)
        return TRUE;

    Result = HeapFree(g_Heap, 0, BaseAddress);
#ifdef _DEBUG
    if (Result) {
        InterlockedIncrement64(&g_HeapStats.FreeCount);
        if (InterlockedDecrement64(&g_HeapStats.OutstandingCount) < 0) {
            OutputDebugString(TEXT("[Heap] Outstanding allocation count became negative.\r\n"));
        }
    }
#endif
    return Result;
}

#ifdef _DEBUG

/*
* supHeapGetStats
*
* Purpose:
*
* Retrieves a consistent snapshot of the current process heap statistics.
*
*/
VOID supHeapGetStats(
    _Out_ PSUP_HEAP_STATS Statistics
)
{
    if (!Statistics)
        return;

    Statistics->AllocCount = InterlockedCompareExchange64(&g_HeapStats.AllocCount, 0, 0);
    Statistics->AllocBytes = InterlockedCompareExchange64(&g_HeapStats.AllocBytes, 0, 0);
    Statistics->ReAllocCount = InterlockedCompareExchange64(&g_HeapStats.ReAllocCount, 0, 0);
    Statistics->ReAllocBytes = InterlockedCompareExchange64(&g_HeapStats.ReAllocBytes, 0, 0);
    Statistics->FreeCount = InterlockedCompareExchange64(&g_HeapStats.FreeCount, 0, 0);
    Statistics->OutstandingCount = InterlockedCompareExchange64(&g_HeapStats.OutstandingCount, 0, 0);
}

#endif

/*
* supUserIsFullAdmin
*
* Purpose:
*
* Determines whether the current user has membership in the built-in
* Administrators group.
*
*/
BOOLEAN supUserIsFullAdmin(
    VOID
)
{
    BOOL isMember;
    DWORD SidSize;
    BYTE SidBuffer[SECURITY_MAX_SID_SIZE];

    isMember = FALSE;
    SidSize = sizeof(SidBuffer);

    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid,
        NULL,
        SidBuffer,
        &SidSize))
    {
        return FALSE;
    }

    if (!CheckTokenMembership(NULL,
        (PSID)SidBuffer,
        &isMember))
    {
        return FALSE;
    }

    return isMember ? TRUE : FALSE;
}

/*
* supShowListContextMenu
*
* Purpose:
*
* Displays the context menu for a list view row, adjusting the
* selection when necessary and providing commands for copying the
* selected row(s) and navigating to the corresponding provider.
*
*/
VOID supShowListContextMenu(
    _In_ INT Row,
    _In_ POINT ScreenPoint
)
{
    BOOL haveActivityId = FALSE, skipSeparator = FALSE;
    BOOL haveRow = Row >= 0;
    UINT state;
    INT i, selectedCount, itemCount;
    HMENU menuHandle;
    LIVE_EVENT_ROW liveRow;

    //
    // Schema view may be invoked without a specific row.
    //
    if (haveRow) {
        state = ListView_GetItemState(g_ctx.hList, Row, LVIS_SELECTED);

        //
        // If the right-clicked row isn't already part of the selection,
        // replace the selection with just this row. Otherwise preserve
        // an existing multi-selection so "Copy Rows" acts on it.
        //
        if (!(state & LVIS_SELECTED)) {
            itemCount = ListView_GetItemCount(g_ctx.hList);

            for (i = 0; i < itemCount; i++) {
                ListView_SetItemState(g_ctx.hList, i, 0, LVIS_SELECTED);
            }

            ListView_SetItemState(g_ctx.hList, Row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
        g_ctx.ctxListRow = Row;
    }

    selectedCount = ListView_GetSelectedCount(g_ctx.hList);

    menuHandle = CreatePopupMenu();
    if (!menuHandle)
        return;

    //
    // Common command.
    //
    AppendMenu(menuHandle, MF_STRING, ID_CTX_COPY_ROW, (selectedCount > 1) ? TEXT("Copy Rows") : TEXT("Copy Row"));
    if (!haveRow) {
        EnableMenuItem(menuHandle, ID_CTX_COPY_ROW, MF_BYCOMMAND | MF_GRAYED);
    }

    AppendMenu(menuHandle, MF_SEPARATOR, 0, NULL);

    switch (g_ctx.listMode) {
    case LIST_MODE_SCHEMA:
        AppendMenu(menuHandle, MF_STRING, ID_CTX_SCHEMA_BACK_TO_PROVIDER, TEXT("Back to Provider"));
        break;
    case LIST_MODE_PROVIDER:
        AppendMenu(menuHandle, MF_STRING, ID_BTN_SCHEMA, TEXT("Browse Schema"));
        AppendMenu(menuHandle, MF_STRING, ID_CTX_VIEW_METADATA, TEXT("View Metadata"));
        break;
    case LIST_MODE_LIVE:
        AppendMenu(menuHandle, MF_STRING, ID_CTX_EVENT_DETAILS, TEXT("Event Details..."));
        AppendMenu(menuHandle, MF_STRING, ID_CTX_GOTO_PROVIDER, TEXT("Go to Provider"));

        if (g_ctx.ctxListRow >= 0) {
            if (supGetDisplayedLiveEvent((ULONG)g_ctx.ctxListRow, &liveRow)) {
                haveActivityId = !IsEqualGUID(liveRow.activityId, GUID_NULL);
            }
        }

        AppendMenu(menuHandle, MF_STRING, ID_CTX_COPY_ACTIVITYID, TEXT("Copy ActivityId"));
        EnableMenuItem(menuHandle, ID_CTX_COPY_ACTIVITYID, MF_BYCOMMAND | (haveActivityId ? MF_ENABLED : MF_GRAYED));

        //
        // Highlight Related Events.
        //
        // Keep this command available even when a highlight is already active. 
        // This allows the user to move the highlight directly to another event.
        //
        AppendMenu(menuHandle, MF_STRING, ID_CTX_HIGHLIGHT_RELATED, TEXT("Highlight Related Events"));
        EnableMenuItem(menuHandle, ID_CTX_HIGHLIGHT_RELATED, MF_BYCOMMAND | (haveActivityId ? MF_ENABLED : MF_GRAYED));

        //
        // Clear Highlight.
        //
        // Always show the command, but disable it when there is currently no active highlight.
        //
        AppendMenu(menuHandle, MF_STRING, ID_CTX_CLEAR_HIGHLIGHT, TEXT("Clear Highlight"));
        EnableMenuItem(menuHandle, ID_CTX_CLEAR_HIGHLIGHT, MF_BYCOMMAND | (g_ctx.highlightActivityIdActive ? MF_ENABLED : MF_GRAYED));

        break;
    default:
        skipSeparator = TRUE;
        break;
    }

    if (!skipSeparator)
        AppendMenu(menuHandle, MF_SEPARATOR, 0, NULL);

    //
    // Common command #2.
    //
    AppendMenu(menuHandle, MF_STRING, ID_BTN_EXPORT, TEXT("Export Current View"));

    TrackPopupMenu(menuHandle, TPM_RIGHTBUTTON, ScreenPoint.x, ScreenPoint.y, 0, g_ctx.hMainWnd, NULL);
    DestroyMenu(menuHandle);
}

/*
* supStrDup
*
* Purpose:
*
* Creates a heap-allocated copy of the specified Unicode string.
*
* The returned string must be released with supHeapFree.
*
*/
PWSTR supStrDup(
    _In_ LPCWSTR String
)
{
    SIZE_T length;
    PWSTR buffer;

    if (!String)
        return NULL;

    length = supStrLen(String);
    if (length > (SIZE_MAX / sizeof(WCHAR)) - 1)
        return NULL;

    buffer = (PWSTR)supHeapAlloc((length + 1) * sizeof(WCHAR));
    if (!buffer)
        return NULL;

    CopyMemory(buffer, String, (length + 1) * sizeof(WCHAR));
    return buffer;
}

/*
* supxRegQueryDword
*
* Purpose:
*
* Reads DWORD value from registry.
*
*/
BOOL supxRegQueryDword(
    _In_ HKEY Key,
    _In_ LPCWSTR Name,
    _Out_ PDWORD Value
)
{
    DWORD cb = sizeof(DWORD);
    return RegQueryValueEx(Key, Name, NULL, NULL, (LPBYTE)Value, &cb) == ERROR_SUCCESS;
}

/*
* supGetStockIcon
*
* Purpose:
*
* Retrieve stock icon of given id.
*
*/
HICON supGetStockIcon(
    _In_ SHSTOCKICONID siid,
    _In_ UINT uFlags
)
{
    SHSTOCKICONINFO sii;

    RtlSecureZeroMemory(&sii, sizeof(sii));
    sii.cbSize = sizeof(sii);

    if (SHGetStockIconInfo(siid, uFlags, &sii) == S_OK) {
        return sii.hIcon;
    }
    return NULL;
}

/*
* supSetMenuIcon
*
* Purpose:
*
* Associates icon data with given menu item.
*
*/
VOID supSetMenuIcon(
    _In_ HMENU hMenu,
    _In_ UINT iItem,
    _In_ HICON hIcon
)
{
    MENUITEMINFO mii;

    RtlSecureZeroMemory(&mii, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_BITMAP | MIIM_DATA;
    mii.hbmpItem = HBMMENU_CALLBACK;
    mii.dwItemData = (ULONG_PTR)hIcon;
    SetMenuItemInfo(hMenu, iItem, FALSE, &mii);
}

/*
* supNotifyCaptureEnded
*
* Purpose:
*
* Marks the live capture as no longer active and notifies the main
* window that the capture thread has finished.
*
*/
VOID supNotifyCaptureEnded(
    VOID
)
{
    InterlockedExchange(&g_ctx.liveCapturing, 0);
    if (g_ctx.hMainWnd) {
        PostMessage(g_ctx.hMainWnd, WM_APP_CAPTURE_ENDED, 0, 0);
    }
}

/*
* supUpdateStatusBar
*
* Purpose:
*
* Update status bar with information depending on currently active mode.
*
*/
VOID supUpdateStatusBar(
    VOID
)
{
    static BOOL s_AutoScrollEnabled = FALSE;
    static ULONG s_ProviderCount = MAXDWORD;
    static ULONG s_SessionCount = MAXDWORD;
    static ULONG s_EventCount = MAXDWORD;
    static ULONG s_SessionsLoadLastError = MAXDWORD;
    static ULONGLONG s_LiveEventTotalCount = MAXULONGLONG;
    static ULONG s_LiveEventLimit = MAXDWORD;
    static LONG s_Capturing = -1;

    WCHAR szText[MAX_PATH];
    WCHAR sessionText[64];

    if (s_ProviderCount == g_ctx.providerCount &&
        s_SessionCount == g_ctx.sessionCount &&
        s_EventCount == g_ctx.liveEventCount &&
        s_LiveEventTotalCount == g_ctx.liveEventTotalCount &&
        s_Capturing == g_ctx.liveCapturing &&
        s_SessionsLoadLastError == g_ctx.sessionsLoadLastError &&
        s_LiveEventLimit == g_ctx.liveEventLimit &&
        s_AutoScrollEnabled == g_ctx.autoScrollEnabled)
    {
        return;
    }

    s_ProviderCount = g_ctx.providerCount;
    s_SessionCount = g_ctx.sessionCount;
    s_EventCount = g_ctx.liveEventCount;
    s_LiveEventTotalCount = g_ctx.liveEventTotalCount;
    s_SessionsLoadLastError = g_ctx.sessionsLoadLastError;
    s_Capturing = g_ctx.liveCapturing;
    s_AutoScrollEnabled = g_ctx.autoScrollEnabled;
    s_LiveEventLimit = g_ctx.liveEventLimit;

    if (g_ctx.sessionsLoadLastError == ERROR_SUCCESS) {
        StringCchPrintf(sessionText, ARRAYSIZE(sessionText), TEXT("Sessions: %lu"), g_ctx.sessionCount);
    }
    else if (g_ctx.sessionsLoadLastError == ERROR_ACCESS_DENIED) {
        StringCchCopy(sessionText, ARRAYSIZE(sessionText), TEXT("Sessions: access denied"));
    }
    else {
        StringCchCopy(sessionText, ARRAYSIZE(sessionText), TEXT("Sessions: unavailable"));
    }

    if (g_ctx.liveCapturing) {
        StringCchPrintf(szText,
            ARRAYSIZE(szText),
            TEXT("Providers: %lu %s Capturing: %llu events  Retained: %lu/%lu  Auto-scroll: %s"),
            g_ctx.providerCount,
            sessionText,
            g_ctx.liveEventTotalCount,
            g_ctx.liveEventCount,
            g_ctx.liveEventLimit,
            g_ctx.autoScrollEnabled ? TEXT("On") : TEXT("Off"));
    }
    else {
        StringCchPrintf(szText,
            ARRAYSIZE(szText),
            TEXT("Providers: %lu %s Captured: %llu events  Retained: %lu/%lu  Auto-scroll: %s"),
            g_ctx.providerCount,
            sessionText,
            g_ctx.liveEventTotalCount,
            g_ctx.liveEventCount,
            g_ctx.liveEventLimit,
            g_ctx.autoScrollEnabled ? TEXT("On") : TEXT("Off"));
    }

    SendMessage(g_ctx.hStatusBar, SB_SETTEXT, 0, (LPARAM)szText);
}

/*
* supCopyTextToClipboard
*
* Purpose:
*
* Copies given text into clipboard.
*
*/
VOID supCopyTextToClipboard(
    _In_ HWND hOwner,
    _In_ LPCWSTR text
)
{
    BOOL clipboardOpened = FALSE;
    SIZE_T chars;
    HGLOBAL hMem = NULL;
    LPWSTR dst = NULL;

    do {

        if (!text)
            break;

        if (FAILED(StringCchLength(text, STRSAFE_MAX_CCH, &chars)))
            break;

        chars++;
        hMem = GlobalAlloc(GMEM_MOVEABLE, chars * sizeof(WCHAR));
        if (!hMem)
            break;

        dst = (LPWSTR)GlobalLock(hMem);
        if (!dst)
            break;

        if (FAILED(StringCchCopy(dst, chars, text)))
            break;

        GlobalUnlock(hMem);
        dst = NULL;

        if (!OpenClipboard(hOwner))
            break;

        clipboardOpened = TRUE;

        if (!EmptyClipboard())
            break;

        if (!SetClipboardData(CF_UNICODETEXT, hMem))
            break;

        hMem = NULL;

    } while (FALSE);


    if (dst)
        GlobalUnlock(hMem);

    if (clipboardOpened)
        CloseClipboard();

    if (hMem)
        GlobalFree(hMem);
}

/*
* supIsInSplitterZone
*
* Purpose:
*
* Determines whether a horizontal coordinate falls within the
* tree/list splitter area.
*
*/
BOOL supIsInSplitterZone(
    _In_ INT x,
    _In_ INT treeWidth
)
{
    return x >= treeWidth && x < treeWidth + SPLITTER_GAP_WIDTH;
}

/*
* supApplyUIFont
*
* Purpose:
*
* Applies the application's shared UI font to a window or control.
*
*/
VOID supApplyUIFont(
    _In_ HWND hCtrl
)
{
    if (g_ctx.hFont)
        SendMessage(hCtrl, WM_SETFONT, (WPARAM)g_ctx.hFont, TRUE);
}

/*
* supIconForLevel
*
* Purpose:
*
* Returns the image-list icon corresponding to a trace event
* severity level.
*
*/
INT supIconForLevel(
    _In_ UCHAR level
)
{
    switch (level) {
    case TRACE_LEVEL_CRITICAL:
    case TRACE_LEVEL_ERROR:
        return ICON_ERROR;
    case TRACE_LEVEL_WARNING:
        return ICON_WARN;
    case TRACE_LEVEL_NONE:
    case TRACE_LEVEL_INFORMATION:
    case TRACE_LEVEL_VERBOSE:
        return ICON_INFO;
    default:
        return ICON_UNKNOWN;
    }
}

/*
* supRemoveCheckbox
*
* Purpose:
*
* Removes the checkbox state image from a specified tree view item.
*
*/
VOID supRemoveCheckbox(
    _In_ HWND hTree,
    _In_ HTREEITEM hItem
)
{
    TVITEM item;

    RtlSecureZeroMemory(&item, sizeof(item));
    item.mask = TVIF_STATE | TVIF_HANDLE;
    item.hItem = hItem;
    item.stateMask = TVIS_STATEIMAGEMASK;
    item.state = INDEXTOSTATEIMAGEMASK(0);
    TreeView_SetItem(hTree, &item);
}

/*
* supUpdateMenuState
*
* Purpose:
*
* Updates the enabled, disabled, and checked state of application
* menu items according to the current capture, provider, refresh
* and verbosity state.
*
*/
VOID supUpdateMenuState(
    VOID
)
{
    BOOL mofSchemaWorkerActive = InterlockedCompareExchange(&g_ctx.mofSchemaWorkerActive, FALSE, FALSE) != FALSE;

    EnableMenuItem(g_ctx.hMenuCapture, ID_BTN_START, MF_BYCOMMAND | (g_ctx.liveCapturing ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(g_ctx.hMenuCapture, ID_BTN_STOP, MF_BYCOMMAND | (g_ctx.liveCapturing ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(g_ctx.hMenuSchema, ID_BTN_SCHEMA, MF_BYCOMMAND |
        (!mofSchemaWorkerActive && g_ctx.selectedProviderIdx < g_ctx.providerCount ? MF_ENABLED : MF_GRAYED));

    EnableMenuItem(g_ctx.hMenuFile, ID_MENU_REFRESH, MF_BYCOMMAND |
        (g_ctx.liveCapturing || mofSchemaWorkerActive ? MF_GRAYED : MF_ENABLED));

    EnableMenuItem(g_ctx.hMenuFile, ID_MENU_OPEN_ETL, MF_BYCOMMAND | (g_ctx.liveCapturing ? MF_GRAYED : MF_ENABLED));

    if (g_ctx.hMenuView) {

        EnableMenuItem(g_ctx.hMenuView, ID_MENU_SET_LIVE_EVENT_LIMIT,
            MF_BYCOMMAND | (g_ctx.liveCapturing ? MF_GRAYED : MF_ENABLED));

        ModifyMenu(g_ctx.hMenuView,
            ID_MENU_PAUSE_DISPLAY,
            MF_BYCOMMAND | MF_STRING,
            ID_MENU_PAUSE_DISPLAY,
            g_ctx.displayPaused ?
            TEXT("&Resume Display") :
            TEXT("&Pause Display"));

        CheckMenuItem(g_ctx.hMenuView, ID_MENU_PAUSE_DISPLAY,
            MF_BYCOMMAND | (g_ctx.displayPaused ? MF_CHECKED : MF_UNCHECKED));

        CheckMenuItem(g_ctx.hMenuView, ID_MENU_AUTOSCROLL,
            MF_BYCOMMAND | (g_ctx.autoScrollEnabled ? MF_CHECKED : MF_UNCHECKED));

        CheckMenuItem(g_ctx.hMenuView, ID_MENU_COLORIZE,
            MF_BYCOMMAND | (g_ctx.colorizeEnabled ? MF_CHECKED : MF_UNCHECKED));
    }

    CheckMenuRadioItem(g_ctx.hMenuVerbosity,
        ID_MENU_LEVEL_BASE + TRACE_LEVEL_CRITICAL,
        ID_MENU_LEVEL_BASE + TRACE_LEVEL_VERBOSE,
        ID_MENU_LEVEL_BASE + g_ctx.selectedLevel,
        MF_BYCOMMAND);
}

/*
* supCreateMainMenu
*
* Purpose:
*
* Creates and initializes the application's main menu and its
* submenus, then attaches the menu to the main window.
*
*/
VOID supCreateMainMenu(
    _In_ HWND hWnd
)
{
    HMENU hSubMenu;
    HICON hIcon;

    g_ctx.hMenu = CreateMenu();
    if (g_ctx.hMenu == NULL)
        return;

    g_ctx.hMenuFile = CreatePopupMenu();
    if (g_ctx.hMenuFile) {
        AppendMenu(g_ctx.hMenuFile, MF_STRING, ID_MENU_REFRESH, TEXT("&Refresh\tF5"));
        AppendMenu(g_ctx.hMenuFile, MF_STRING, ID_MENU_OPEN_ETL, TEXT("&Open ETL File for Replay..."));
        AppendMenu(g_ctx.hMenuFile, MF_SEPARATOR, 0, NULL);
        AppendMenu(g_ctx.hMenuFile, MF_STRING, ID_BTN_EXPORT, TEXT("&Export to CSV...\tCtrl+E"));

        if (g_ctx.isAdmin == FALSE) {
            AppendMenu(g_ctx.hMenuFile, MF_STRING, ID_MENU_RUNASADMIN, TEXT("Run as &Administrator..."));
            hIcon = supGetStockIcon(SIID_SHIELD, SHGSI_ICON | SHGFI_SMALLICON);
            if (hIcon) {
                supSetMenuIcon(g_ctx.hMenuFile, ID_MENU_RUNASADMIN, hIcon);
            }
        }

        AppendMenu(g_ctx.hMenuFile, MF_SEPARATOR, 0, NULL);
        AppendMenu(g_ctx.hMenuFile, MF_STRING, ID_MENU_EXIT, TEXT("E&xit"));
        AppendMenu(g_ctx.hMenu, MF_POPUP, (UINT_PTR)g_ctx.hMenuFile, TEXT("&File"));
    }

    g_ctx.hMenuVerbosity = CreatePopupMenu();
    if (g_ctx.hMenuVerbosity) {
        AppendMenu(g_ctx.hMenuVerbosity, MF_STRING, ID_MENU_LEVEL_BASE + TRACE_LEVEL_CRITICAL, TEXT("&1 - Critical"));
        AppendMenu(g_ctx.hMenuVerbosity, MF_STRING, ID_MENU_LEVEL_BASE + TRACE_LEVEL_ERROR, TEXT("&2 - Error"));
        AppendMenu(g_ctx.hMenuVerbosity, MF_STRING, ID_MENU_LEVEL_BASE + TRACE_LEVEL_WARNING, TEXT("&3 - Warning"));
        AppendMenu(g_ctx.hMenuVerbosity, MF_STRING, ID_MENU_LEVEL_BASE + TRACE_LEVEL_INFORMATION, TEXT("&4 - Information"));
        AppendMenu(g_ctx.hMenuVerbosity, MF_STRING, ID_MENU_LEVEL_BASE + TRACE_LEVEL_VERBOSE, TEXT("&5 - Verbose"));
    }

    g_ctx.hMenuCapture = CreatePopupMenu();
    if (g_ctx.hMenuCapture) {
        AppendMenu(g_ctx.hMenuCapture, MF_STRING, ID_BTN_START, TEXT("&Start Capture\tCtrl+F5"));
        AppendMenu(g_ctx.hMenuCapture, MF_STRING, ID_BTN_STOP, TEXT("S&top Capture\tShift+F5"));
        AppendMenu(g_ctx.hMenuCapture, MF_SEPARATOR, 0, NULL);
        AppendMenu(g_ctx.hMenuCapture, MF_POPUP, (UINT_PTR)g_ctx.hMenuVerbosity, TEXT("&Verbosity"));
        AppendMenu(g_ctx.hMenuCapture, MF_STRING, ID_MENU_SET_KEYWORD, TEXT("Set &Keyword Filter...\tCtrl+K"));
        AppendMenu(g_ctx.hMenuCapture, MF_STRING, ID_MENU_EVENTID_FILTER, TEXT("Set Event &ID Filter...\tCtrl+I"));
        AppendMenu(g_ctx.hMenuCapture, MF_STRING, ID_MENU_SET_WPP_TMF, TEXT("Set &WPP TMF Search Path...\tCtrl+Shift+T"));
        AppendMenu(g_ctx.hMenuCapture, MF_STRING, ID_MENU_SAVE_ETL_TOGGLE, TEXT("&Also Save to ETL File"));
        AppendMenu(g_ctx.hMenuCapture, MF_SEPARATOR, 0, NULL);
        AppendMenu(g_ctx.hMenuCapture, MF_STRING, ID_MENU_UNCHECK_ALL, TEXT("&Uncheck All Providers"));
        AppendMenu(g_ctx.hMenu, MF_POPUP, (UINT_PTR)g_ctx.hMenuCapture, TEXT("&Capture"));
    }

    g_ctx.hMenuFind = CreatePopupMenu();
    if (g_ctx.hMenuFind) {
        AppendMenu(g_ctx.hMenuFind, MF_STRING | MF_GRAYED, ID_MENU_FIND, TEXT("&Find Event\tCtrl+F"));
        AppendMenu(g_ctx.hMenu, MF_POPUP, (UINT_PTR)g_ctx.hMenuFind, TEXT("Fi&nd"));
    }

    g_ctx.hMenuSchema = CreatePopupMenu();
    if (g_ctx.hMenuSchema) {
        AppendMenu(g_ctx.hMenuSchema, MF_STRING, ID_BTN_SCHEMA, TEXT("&Browse Schema\tCtrl+O"));
        AppendMenu(g_ctx.hMenu, MF_POPUP, (UINT_PTR)g_ctx.hMenuSchema, TEXT("&Schema"));
    }

    g_ctx.hMenuView = CreatePopupMenu();
    if (g_ctx.hMenuView) {
        AppendMenu(g_ctx.hMenuView, MF_STRING, ID_MENU_PAUSE_DISPLAY, TEXT("&Pause Display"));
        AppendMenu(g_ctx.hMenuView, MF_STRING, ID_MENU_AUTOSCROLL, TEXT("&Auto-scroll"));
        AppendMenu(g_ctx.hMenuView, MF_STRING, ID_MENU_COLORIZE, TEXT("&Colorize by Level"));
        AppendMenu(g_ctx.hMenuView, MF_SEPARATOR, 0, NULL);
        AppendMenu(g_ctx.hMenuView, MF_STRING, ID_MENU_SET_LIVE_EVENT_LIMIT, TEXT("Set Live Event &Limit..."));
        AppendMenu(g_ctx.hMenuView, MF_SEPARATOR, 0, NULL);
        AppendMenu(g_ctx.hMenuView, MF_STRING, ID_MENU_SYSTEM_INFORMATION, TEXT("&System Information..."));
#ifdef _DEBUG
        //
        // We need a special debug for this bugfest shit.
        //
        AppendMenu(g_ctx.hMenuView, MF_STRING, ID_VIEW_HIGHLIGHT_SIMULATION, TEXT("Highlight Simulation"));
#endif
        AppendMenu(g_ctx.hMenu, MF_POPUP, (UINT_PTR)g_ctx.hMenuView, TEXT("&View"));
    }

    //
    // We don't need it to be global.
    //
    hSubMenu = CreatePopupMenu();
    if (hSubMenu) {
        AppendMenu(hSubMenu, MF_STRING, ID_MENU_ABOUT, TEXT("&About"));
        AppendMenu(g_ctx.hMenu, MF_POPUP, (UINT_PTR)hSubMenu, TEXT("&Help"));
    }

    SetMenu(hWnd, g_ctx.hMenu);
    supUpdateMenuState();
    supUpdateToolbarState();
}

/*
* supCreateUIFont
*
* Purpose:
*
* Creates the application's shared UI font using the system
* message font, with a default shell dialog font as a fallback.
*
*/
VOID supCreateUIFont(
    VOID
)
{
    NONCLIENTMETRICS ncm;

    RtlSecureZeroMemory(&ncm, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);

    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        g_ctx.hFont = CreateFontIndirect(&ncm.lfMessageFont);
    }
    if (!g_ctx.hFont) {
        g_ctx.hFont = CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("MS Shell Dlg 2"));
    }
}

/*
 * supCreateFixedFont
 *
 * Purpose:
 *
 * Creates a modern fixed-pitch font for controls that require
 * character-aligned text.
 *
 */
HFONT supCreateFixedFont(
    _In_ INT PointSize
)
{
    INT height;
    HDC hdc;
    HFONT hFont;

    hdc = GetDC(NULL);
    if (!hdc)
        return NULL;

    height = -MulDiv(PointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);

    ReleaseDC(NULL, hdc);

    hFont = CreateFont(height,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | FF_MODERN,
        TEXT("Consolas"));

    if (!hFont) {
        hFont = CreateFont(height,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            FIXED_PITCH | FF_MODERN,
            TEXT("Cascadia Mono"));
    }

    return hFont;
}

/*
* supCreateTreeImageList
*
* Purpose:
*
* Creates the tree view image list if it does not already exist,
* adds the standard application icons, and associates the image
* list with the specified tree view.
*
*/
VOID supCreateTreeImageList(
    _In_ HWND hTree
)
{
    if (g_ctx.hImageList) {
        TreeView_SetImageList(hTree, g_ctx.hImageList, TVSIL_NORMAL);
        return;
    }

    g_ctx.hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 10, 1);
    if (g_ctx.hImageList) {
        //defaults
        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_INFORMATION)));
        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_WARNING)));
        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_ERROR)));
        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_QUESTION)));

        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_PROVIDERS)));
        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_PROVIDER)));
        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_SESSIONS)));
        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_SESSION)));
        ImageList_AddIcon(g_ctx.hImageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_LIVECAPTURE)));

        TreeView_SetImageList(hTree, g_ctx.hImageList, TVSIL_NORMAL);
    }
}

/*
* supClampedTreeWidth
*
* Purpose:
*
* Clamps the configured tree view width to the minimum tree width
* and the maximum width allowed while preserving the splitter and
* minimum list view width.
*
*/
INT supClampedTreeWidth(
    _In_ INT clientWidth
)
{
    INT tw = g_ctx.treeWidth, maxTw;

    if (tw < SPLITTER_MIN_TREE)
        tw = SPLITTER_MIN_TREE;

    maxTw = clientWidth - SPLITTER_GAP_WIDTH - SPLITTER_MIN_LIST;
    if (maxTw < SPLITTER_MIN_TREE)
        maxTw = SPLITTER_MIN_TREE;

    if (tw > maxTw)
        tw = maxTw;
    return tw;
}

/*
* supLayoutChildren
*
* Purpose:
*
* Calculates and applies the positions and sizes of the main window
* child controls, including the toolbar, filter, tree view, list view,
* and status bar, and invalidates the splitter area for repainting.
*
*/
VOID supLayoutChildren(
    _In_ HWND hWnd
)
{
    DWORD buttonSize;
    INT width, height, treeWidth, filterHeight, contentHeight, rightX, rightWidth, toolbarHeight;
    HDWP hdwp;
    RECT rc, splitterRc;

    GetClientRect(hWnd, &rc);

    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    if (width < 0)
        width = 0;

    if (height < 0)
        height = 0;

    treeWidth = supClampedTreeWidth(width);
    filterHeight = FILTER_HEIGHT;

    toolbarHeight = TOOLBAR_HEIGHT;
    buttonSize = (DWORD)SendMessage(g_ctx.hToolbar, TB_GETBUTTONSIZE, 0, 0);

    if (HIWORD(buttonSize) != 0)
        toolbarHeight = HIWORD(buttonSize);

    contentHeight = height - STATUS_BAR_HEIGHT - toolbarHeight;
    if (contentHeight < 0)
        contentHeight = 0;

    rightX = treeWidth + SPLITTER_GAP_WIDTH;

    rightWidth = width - rightX;
    if (rightWidth < 0)
        rightWidth = 0;

    hdwp = BeginDeferWindowPos(4);
    if (hdwp) {
        hdwp = DeferWindowPos(hdwp, g_ctx.hToolbar, NULL, 0, 0, width, toolbarHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (hdwp) {
        hdwp = DeferWindowPos(hdwp, g_ctx.hEditFilter, NULL, 0, toolbarHeight, treeWidth, filterHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (hdwp) {
        hdwp = DeferWindowPos(hdwp, g_ctx.hTree, NULL, 0, toolbarHeight + filterHeight, treeWidth,
            contentHeight - filterHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (hdwp) {
        hdwp = DeferWindowPos(hdwp, g_ctx.hList, NULL, rightX, toolbarHeight, rightWidth, contentHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (hdwp) {
        EndDeferWindowPos(hdwp);
    }

    SendMessage(g_ctx.hStatusBar, WM_SIZE, 0, 0);

    splitterRc.left = treeWidth;
    splitterRc.top = toolbarHeight;
    splitterRc.right = treeWidth + SPLITTER_GAP_WIDTH;
    splitterRc.bottom = toolbarHeight + contentHeight;
    InvalidateRect(hWnd, &splitterRc, FALSE);
}

/*
* supSetupListViewColumns
*
* Purpose:
*
* Removes the existing list view columns and creates the columns
* used to display provider or session detail fields and values.
*
*/
VOID supSetupListViewColumns(
    _In_ HWND hList,
    _In_ BOOL forProvider
)
{
    INT fieldWidth, valueWidth;

    while (ListView_DeleteColumn(hList, 0));

    fieldWidth = (forProvider) ? 140 : 180;
    valueWidth = (forProvider) ? 420 : 380;

    supInsertListColumn(hList, LISTVIEW_COLUMN_FIELD, fieldWidth, TEXT("Field"));
    supInsertListColumn(hList, LISTVIEW_COLUMN_VALUE, valueWidth, TEXT("Value"));
}

/*
* supSetupListViewColumnsSchema
*
* Purpose:
*
* Removes the existing list view columns and creates the columns
* used to display event schema information.
*
*/
VOID supSetupListViewColumnsSchema(
    _In_ HWND hList,
    _In_ BOOL IsManifestProvider
)
{
    while (ListView_DeleteColumn(hList, 0));

    if (IsManifestProvider) {
        supInsertListColumn(hList, SCHEMA_COLUMN_EVENT_ID, 60, TEXT("EventId"));
        supInsertListColumn(hList, SCHEMA_COLUMN_VERSION, 60, TEXT("Version"));
        supInsertListColumn(hList, SCHEMA_COLUMN_LEVEL, 100, TEXT("Level"));
        supInsertListColumn(hList, SCHEMA_COLUMN_TASK, 140, TEXT("Task"));
        supInsertListColumn(hList, SCHEMA_COLUMN_OPCODE, 120, TEXT("Opcode"));
        supInsertListColumn(hList, SCHEMA_COLUMN_KEYWORD, 140, TEXT("Keyword"));
        supInsertListColumn(hList, SCHEMA_COLUMN_PROPERTIES, 400, TEXT("Properties"));
    }
    else {
        supInsertListColumn(hList, MOF_SCHEMA_COLUMN_EVENT_TYPE, 100, TEXT("EventType"));
        supInsertListColumn(hList, MOF_SCHEMA_COLUMN_EVENT_TYPES, 120, TEXT("EventTypes"));
        supInsertListColumn(hList, MOF_SCHEMA_COLUMN_CLASS, 220, TEXT("Class"));
        supInsertListColumn(hList, MOF_SCHEMA_COLUMN_PROPERTIES, 500, TEXT("Properties"));
        supInsertListColumn(hList, MOF_SCHEMA_COLUMN_DESCRIPTION, 360, TEXT("Description"));
    }
}

/*
* supSetupListViewColumnsLive
*
* Purpose:
*
* Removes the existing list view columns and creates the columns
* used to display captured live events.
*
*/
VOID supSetupListViewColumnsLive(
    _In_ HWND hList
)
{
    while (ListView_DeleteColumn(hList, 0));

    supInsertListColumn(hList, LIVE_COLUMN_TIME, 110, TEXT("Time"));
    supInsertListColumn(hList, LIVE_COLUMN_PROVIDER, 220, TEXT("Provider"));
    supInsertListColumn(hList, LIVE_COLUMN_EVENT_ID, 70, TEXT("EventId"));
    supInsertListColumn(hList, LIVE_COLUMN_LEVEL, 60, TEXT("Level"));
    supInsertListColumn(hList, LIVE_COLUMN_KEYWORD, 140, TEXT("Keyword"));
    supInsertListColumn(hList, LIVE_COLUMN_PROPERTIES, 500, TEXT("Properties"));
}

/*
* supAddRow
*
* Purpose:
*
* Appends a field/value pair to the current detail row array,
* growing the array when additional capacity is required.
*
*/
VOID supAddRow(
    _In_ LPCWSTR field,
    _In_ LPCWSTR value
)
{
    ULONG newCap, sz;
    DETAIL_ROW* grown;

    if (g_ctx.detailRowCount >= g_ctx.detailRowCapacity) {
        newCap = g_ctx.detailRowCapacity ? g_ctx.detailRowCapacity * 2 : 32;
        grown = (DETAIL_ROW*)supHeapReAlloc(g_ctx.detailRows, newCap * sizeof(DETAIL_ROW));
        if (!grown)
            return;

        g_ctx.detailRows = grown;
        g_ctx.detailRowCapacity = newCap;
    }

    sz = (ULONG)supStrLen(field);
    supStrNCopy(g_ctx.detailRows[g_ctx.detailRowCount].field,
        ARRAYSIZE(g_ctx.detailRows[0].field),
        field,
        sz);

    sz = (ULONG)supStrLen(value);
    supStrNCopy(g_ctx.detailRows[g_ctx.detailRowCount].value,
        ARRAYSIZE(g_ctx.detailRows[0].value),
        value,
        sz);

    g_ctx.detailRowCount++;
}

/*
* supShowTreeContextMenu
*
* Purpose:
*
* Displays the context menu appropriate for the selected tree view
* item, providing commands for provider, session, or root nodes.
*
*/
VOID supShowTreeContextMenu(
    _In_ HTREEITEM hItem,
    _In_ POINT ScreenPt
)
{
    BOOL bNeedShield = FALSE;
    ULONG kind, index;
    HMENU hMenu;
    HICON hIcon;
    TVITEM item;

    RtlSecureZeroMemory(&item, sizeof(item));

    item.mask = TVIF_PARAM;
    item.hItem = hItem;

    if (!TreeView_GetItem(g_ctx.hTree, &item))
        return;

    kind = NODE_KIND(item.lParam);
    index = NODE_INDEX(item.lParam);

    hMenu = CreatePopupMenu();
    if (!hMenu)
        return;

    do {

        //
        // Root nodes.
        //
        if (index == NODE_ROOT_MARKER) {

            //
            // Only the Providers root currently has a context menu.
            //
            if (kind == NODE_KIND_PROVIDER) {

                AppendMenu(hMenu, MF_STRING, ID_MENU_UNCHECK_ALL, TEXT("Uncheck All Providers"));

                TrackPopupMenu(hMenu,
                    TPM_RIGHTBUTTON,
                    ScreenPt.x,
                    ScreenPt.y,
                    0,
                    g_ctx.hMainWnd,
                    NULL);
            }

            break;
        }

        g_ctx.ctxTreeItem = hItem;

        TreeView_SelectItem(g_ctx.hTree, hItem);

        if (kind == NODE_KIND_PROVIDER &&
            index < g_ctx.providerCount)
        {
            AppendMenu(hMenu, MF_STRING, ID_CTX_COPY_GUID, TEXT("Copy GUID"));
            AppendMenu(hMenu, MF_STRING, ID_CTX_COPY_PROVIDER_NAME, TEXT("Copy Provider Name"));
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_BTN_SCHEMA, TEXT("Browse Schema"));
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_MENU_VIEW_SECURITY, TEXT("View Security"));
            bNeedShield = TRUE;
        }
        else if (kind == NODE_KIND_SESSION &&
            index < g_ctx.sessionCount)
        {
            AppendMenu(hMenu, MF_STRING, ID_CTX_COPY_SESSION_NAME, TEXT("Copy Session Name"));
            AppendMenu(hMenu, MF_STRING, ID_CTX_COPY_SESSION_ID, TEXT("Copy Session ID"));
            AppendMenu(hMenu, MF_STRING, ID_CTX_COPY_ENABLED_PROVIDERS, TEXT("Copy Enabled Providers"));
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_MENU_VIEW_SECURITY, TEXT("View Security"));
            bNeedShield = TRUE;
        }
        else {
            break;
        }

        if (bNeedShield && g_ctx.isAdmin == FALSE) {
            hIcon = supGetStockIcon(SIID_SHIELD, SHGSI_ICON | SHGFI_SMALLICON);
            if (hIcon) {
                supSetMenuIcon(hMenu, ID_MENU_VIEW_SECURITY, hIcon);
            }
        }

        TrackPopupMenu(hMenu,
            TPM_RIGHTBUTTON,
            ScreenPt.x,
            ScreenPt.y,
            0,
            g_ctx.hMainWnd,
            NULL);

    } while (FALSE);

    DestroyMenu(hMenu);
}

/*
* supFreeAllData
*
* Purpose:
*
* Releases the dynamically allocated provider, session, schema,
* enabled-provider, and provider-selection data and resets the
* corresponding application state.
*
*/
VOID supFreeAllData(
    VOID
)
{
    ULONG i;

    for (i = 0; i < g_ctx.providerCount; i++) {
        supHeapFree(g_ctx.providers[i].enableRows);
        supHeapFree(g_ctx.providers[i].schemaRows);
        supHeapFree(g_ctx.providers[i].mofSchemaRows);
    }

    supHeapFree(g_ctx.providers);

    g_ctx.providers = NULL;
    g_ctx.providerCount = 0;

    for (i = 0; i < g_ctx.sessionCount; i++) {
        supHeapFree(g_ctx.sessions[i].enabledProviders);
    }

    supHeapFree(g_ctx.sessions);

    g_ctx.sessions = NULL;
    g_ctx.sessionCount = 0;

    supHeapFree(g_ctx.providerChecked);
    g_ctx.providerChecked = NULL;

    g_ctx.selectedProviderIdx = DEFAULT_PROVIDER_IDX;
}

/*
* supIsIntegerInType
*
* Purpose:
*
* Determines whether a TDH input type represents an integer,
* hexadecimal integer, or pointer-sized integer value.
*
*/
BOOL supIsIntegerInType(
    _In_ USHORT inType
)
{
    switch (inType) {
    case TDH_INTYPE_INT8:
    case TDH_INTYPE_UINT8:
    case TDH_INTYPE_INT16:
    case TDH_INTYPE_UINT16:
    case TDH_INTYPE_INT32:
    case TDH_INTYPE_UINT32:
    case TDH_INTYPE_INT64:
    case TDH_INTYPE_UINT64:
    case TDH_INTYPE_HEXINT32:
    case TDH_INTYPE_HEXINT64:
    case TDH_INTYPE_POINTER:
        return TRUE;
    default:
        return FALSE;
    }
}

/*
* supAppendLiveEvent
*
* Purpose:
*
* Appends a captured live event to the live event array while
* synchronizing access to the array with the live-event critical
* section.
*
*/
VOID supAppendLiveEvent(
    _In_ PCLIVE_EVENT_ROW Event
)
{
    ULONG physicalIndex;

    EnterCriticalSection(&g_ctx.liveCs);

    do {

        if (!EtpEnsureLiveEventCapacity())
            break;

        if (g_ctx.liveSortedIndices) {
            supHeapFree(g_ctx.liveSortedIndices);
            g_ctx.liveSortedIndices = NULL;
            g_ctx.liveSortedIndexCount = 0;
        }

        if (g_ctx.liveEventCount < g_ctx.liveEventCapacity) {
            physicalIndex = (g_ctx.liveEventHead + g_ctx.liveEventCount) % g_ctx.liveEventCapacity;
            g_ctx.liveEvents[physicalIndex] = *Event;
            g_ctx.liveEventCount++;
        }
        else {
            g_ctx.liveEvents[g_ctx.liveEventHead] = *Event;
            g_ctx.liveEventHead = (g_ctx.liveEventHead + 1) %
                g_ctx.liveEventCapacity;
        }

        g_ctx.liveEventTotalCount++;
        g_ctx.liveEventGeneration++;

    } while (FALSE);

    LeaveCriticalSection(&g_ctx.liveCs);
}

/*
* supCopyLiveEvent
*
* Purpose:
*
* Copies a logical live-event row while handling ring-buffer wrapping.
*
*/
_Success_(return != FALSE)
BOOL supCopyLiveEvent(
    _In_ ULONG Index,
    _Out_ PLIVE_EVENT_ROW Event)
{
    BOOL result = FALSE;
    ULONG physicalIndex;

    if (!Event)
        return FALSE;

    EnterCriticalSection(&g_ctx.liveCs);

    if (Index < g_ctx.liveEventCount &&
        g_ctx.liveEventCapacity != 0)
    {
        if (g_ctx.liveSortedIndices &&
            g_ctx.liveSortedIndexCount == g_ctx.liveEventCount)
        {
            physicalIndex = g_ctx.liveSortedIndices[Index];
        }
        else {
            physicalIndex = (g_ctx.liveEventHead + Index) % g_ctx.liveEventCapacity;
        }

        *Event = g_ctx.liveEvents[physicalIndex];
        result = TRUE;
    }

    LeaveCriticalSection(&g_ctx.liveCs);

    return result;
}

/*
* supClearLiveEvents
*
* Purpose:
*
* Releases all captured live event data and resets the live event
* count, capacity, and displayed item count.
*
*/
VOID supClearLiveEvents(
    VOID
)
{
    EnterCriticalSection(&g_ctx.liveCs);

    supHeapFree(g_ctx.liveEvents);
    supHeapFree(g_ctx.liveSortedIndices);

    g_ctx.liveEvents = NULL;
    g_ctx.liveSortedIndices = NULL;
    g_ctx.liveEventCount = 0;
    g_ctx.liveEventCapacity = 0;
    g_ctx.liveEventHead = 0;
    g_ctx.liveEventTotalCount = 0;
    g_ctx.liveSortedIndexCount = 0;
    g_ctx.liveEventGeneration++;

    LeaveCriticalSection(&g_ctx.liveCs);

    g_ctx.liveDisplayedCount = 0;
    g_ctx.liveDisplayedGeneration = 0;

    if (g_ctx.listMode == LIST_MODE_LIVE &&
        g_ctx.hList)
    {
        ListView_SetItemCountEx(g_ctx.hList, 0, LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
        InvalidateRect(g_ctx.hList, NULL, FALSE);
    }
}

/*
* supInsertListColumn
*
* Purpose:
*
* Inserts a column into a list view control with the specified index,
* width, and display text.
*
*/
VOID supInsertListColumn(
    _In_ HWND hList,
    _In_ INT Index,
    _In_ INT Width,
    _In_ LPCWSTR Text
)
{
    LVCOLUMN column;

    RtlSecureZeroMemory(&column, sizeof(column));

    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = Width;
    column.pszText = (LPWSTR)Text;
    ListView_InsertColumn(hList, Index, &column);
}

/*
* supInsertTreeRootItem
*
* Purpose:
*
* Inserts a root-level item into a tree view control and initializes
* its associated parameter and image information.
*
*/
HTREEITEM supInsertTreeRootItem(
    _In_ HWND hTree,
    _In_ LPCWSTR Text,
    _In_ LPARAM Param,
    _In_ INT Image
)
{
    TVINSERTSTRUCT tvis;

    RtlSecureZeroMemory(&tvis, sizeof(tvis));

    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvis.item.pszText = (LPWSTR)Text;
    tvis.item.lParam = Param;
    tvis.item.iImage = Image;
    tvis.item.iSelectedImage = Image;

    return TreeView_InsertItem(hTree, &tvis);
}

/*
* supToggleSaveToEtl
*
* Purpose:
*
* Enables or disables saving live capture data to an ETL file.
* When enabling the option, prompts the user to select the output
* file and stores the selected path.
*
*/
VOID supToggleSaveToEtl(
    _In_ HWND WindowHandle
)
{
    OPENFILENAME ofn;
    WCHAR path[MAX_PATH];

    if (!g_ctx.saveToEtlEnabled) {

        StringCchCopy(path, ARRAYSIZE(path), TEXT("capture.etl"));
        RtlSecureZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = WindowHandle;
        ofn.lpstrFilter = TEXT("ETL Files (*.etl)\0*.etl\0All Files\0*.*\0");
        ofn.lpstrFile = path;
        ofn.nMaxFile = ARRAYSIZE(path);
        ofn.lpstrDefExt = TEXT("etl");
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (GetSaveFileName(&ofn)) {
            StringCchCopy(g_ctx.etlSavePath, ARRAYSIZE(g_ctx.etlSavePath), path);
            g_ctx.saveToEtlEnabled = TRUE;
        }
    }
    else {
        g_ctx.saveToEtlEnabled = FALSE;
    }

    CheckMenuItem(g_ctx.hMenuCapture, ID_MENU_SAVE_ETL_TOGGLE,
        MF_BYCOMMAND | (g_ctx.saveToEtlEnabled ? MF_CHECKED : MF_UNCHECKED));
}

/*
* supRunAsAdministrator
*
* Purpose:
*
* Relaunches the application with administrator privileges using the
* ShellExecute "runas" operation and closes the current main window
* when the elevated instance is successfully started.
*
*/
VOID supRunAsAdministrator(
    _In_ HWND WindowHandle
)
{
    DWORD error;
    SHELLEXECUTEINFO sh;
    WCHAR executablePath[MAX_PATH];

    GetModuleFileName(NULL, executablePath, ARRAYSIZE(executablePath));

    RtlSecureZeroMemory(&sh, sizeof(sh));
    sh.cbSize = sizeof(sh);
    sh.fMask = SEE_MASK_DEFAULT;
    sh.hwnd = WindowHandle;
    sh.lpVerb = TEXT("runas");
    sh.lpFile = executablePath;
    sh.nShow = SW_SHOWNORMAL;

    if (ShellExecuteEx(&sh)) {
        DestroyWindow(WindowHandle);
    }
    else {
        error = GetLastError();
        if (error != ERROR_CANCELLED) {
            MessageBox(WindowHandle, TEXT("Failed to relaunch elevated."), TEXT("Run as Administrator"), MB_OK | MB_ICONERROR);
        }
    }
}

/*
* supParseEventIdFilterText
*
* Purpose:
*
* Parses the configured event ID filter text into the internal array
* of numeric event IDs, accepting comma-, space-, and tab-separated
* values within the supported event ID range.
*
*/
VOID supParseEventIdFilterText(
    VOID
)
{
    ULONGLONG value;
    PWCHAR token, next;
    WCHAR buffer[ARRAYSIZE(g_ctx.eventIdFilterText)];

    g_ctx.eventIdFilterCount = 0;

    StringCchCopy(buffer, ARRAYSIZE(buffer), g_ctx.eventIdFilterText);
    token = buffer;

    while (*token &&
        g_ctx.eventIdFilterCount < MAX_EVENTID_FILTERS)
    {
        //
        // Skip separators.
        //
        while (*token == L',' ||
            *token == L' ' ||
            *token == L'\t')
        {
            token++;
        }

        if (*token == 0)
            break;

        //
        // Find end of current token.
        //
        next = token;
        while (*next &&
            *next != L',' &&
            *next != L' ' &&
            *next != L'\t')
        {
            next++;
        }

        if (*next) {
            *next = 0;
            next++;
        }

        if (supStrToUInt64(token, &value)) {
            if (value <= USHRT_MAX) {
                g_ctx.eventIdFilter[g_ctx.eventIdFilterCount++] = (USHORT)value;
            }
        }

        token = next;
    }
}

/*
* supxValidateWindowPlacement
*
* Purpose:
*
* Validates the saved main window position and dimensions against
* the available monitor work area and replaces invalid or completely
* off-screen placement values with safe defaults.
*
*/
VOID supxValidateWindowPlacement(
    _Inout_ INT * X,
    _Inout_ INT * Y,
    _Inout_ INT * Width,
    _Inout_ INT * Height
)
{
    BOOL useDefault;
    HMONITOR hMonitor;
    POINT pt;
    MONITORINFO mi;
    RECT rcWindow;

    //
    // CW_USEDEFAULT must be consistent for X and Y together.
    //
    useDefault = (*X == CW_USEDEFAULT || *Y == CW_USEDEFAULT);

    if (useDefault) {
        *X = CW_USEDEFAULT;
        *Y = CW_USEDEFAULT;

        pt.x = 0;
        pt.y = 0;
        hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }
    else {
        rcWindow.left = *X;
        rcWindow.top = *Y;
        rcWindow.right = *X + *Width;
        rcWindow.bottom = *Y + *Height;

        hMonitor = MonitorFromRect(&rcWindow, MONITOR_DEFAULTTONEAREST);
    }

    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfo(hMonitor, &mi))
        return;

    if (*Width <= 0 ||
        *Width > (mi.rcWork.right - mi.rcWork.left))
    {
        *Width = MAINWND_DEFAULT_WIDTH;
    }

    if (*Height <= 0 ||
        *Height > (mi.rcWork.bottom - mi.rcWork.top))
    {
        *Height = MAINWND_DEFAULT_HEIGHT;
    }

    if (!useDefault) {

        rcWindow.left = *X;
        rcWindow.top = *Y;
        rcWindow.right = *X + *Width;
        rcWindow.bottom = *Y + *Height;

        if (rcWindow.right < mi.rcWork.left ||
            rcWindow.bottom < mi.rcWork.top ||
            rcWindow.left > mi.rcWork.right ||
            rcWindow.top > mi.rcWork.bottom)
        {
            *X = CW_USEDEFAULT;
            *Y = CW_USEDEFAULT;
        }
    }
}

/*
* supLoadSettings
*
* Purpose:
*
* Loads application settings from the current user's registry.
*
*/
VOID supLoadSettings(
    _Inout_ INT * X,
    _Inout_ INT * Y,
    _Inout_ INT * Width,
    _Inout_ INT * Height
)
{
    HKEY hKey;
    DWORD value, valueSize, eventIdSize;

    g_ctx.selectedLevel = TRACE_LEVEL_VERBOSE;
    g_ctx.liveEventLimit = DEFAULT_LIVE_EVENT_LIMIT;
    g_ctx.autoScrollEnabled = TRUE;
    StringCchCopy(g_ctx.keywordFilterText, ARRAYSIZE(g_ctx.keywordFilterText), TEXT("0"));

    g_ctx.eventIdFilterText[0] = UNICODE_NULL;

    if (RegOpenKeyEx(
        HKEY_CURRENT_USER,
        REG_KEY_PATH,
        0,
        KEY_READ,
        &hKey) != ERROR_SUCCESS)
    {
        supParseEventIdFilterText();
        supxValidateWindowPlacement(X, Y, Width, Height);
        return;
    }

    value = 0;
    if (supxRegQueryDword(hKey, TEXT("WindowX"), &value))
        *X = (INT)value;

    if (supxRegQueryDword(hKey, TEXT("WindowY"), &value))
        *Y = (INT)value;

    if (supxRegQueryDword(hKey, TEXT("WindowW"), &value))
        *Width = (INT)value;

    if (supxRegQueryDword(hKey, TEXT("WindowH"), &value))
        *Height = (INT)value;

    valueSize = sizeof(value);
    value = 0;
    if (RegQueryValueEx(hKey,
        TEXT("Level"),
        NULL,
        NULL,
        (LPBYTE)&value,
        &valueSize) == ERROR_SUCCESS)
    {
        if (value >= TRACE_LEVEL_CRITICAL &&
            value <= TRACE_LEVEL_VERBOSE)
        {
            g_ctx.selectedLevel = (UCHAR)value;
        }
    }

    value = 0;
    if (supxRegQueryDword(hKey, TEXT("LiveEventLimit"), &value)) {
        if (value != 0 &&
            value <= MAX_LIVE_EVENT_LIMIT)
        {
            g_ctx.liveEventLimit = value;
        }
    }

    value = TRUE;
    if (supxRegQueryDword(hKey, TEXT("AutoScroll"), &value))
        g_ctx.autoScrollEnabled = (value != FALSE);

    valueSize = sizeof(g_ctx.keywordFilterText);
    if (RegQueryValueEx(hKey,
        TEXT("Keyword"),
        NULL,
        NULL,
        (LPBYTE)g_ctx.keywordFilterText,
        &valueSize) == ERROR_SUCCESS)
    {
        g_ctx.keywordFilterText[ARRAYSIZE(g_ctx.keywordFilterText) - 1] = UNICODE_NULL;
    }

    eventIdSize = (DWORD)(ARRAYSIZE(g_ctx.eventIdFilterText) * sizeof(WCHAR));
    if (RegQueryValueEx(
        hKey,
        TEXT("EventIdFilter"),
        NULL,
        NULL,
        (LPBYTE)g_ctx.eventIdFilterText,
        &eventIdSize) == ERROR_SUCCESS)
    {
        g_ctx.eventIdFilterText[ARRAYSIZE(g_ctx.eventIdFilterText) - 1] = UNICODE_NULL;
    }

    RegCloseKey(hKey);

    supParseEventIdFilterText();
    supxValidateWindowPlacement(X, Y, Width, Height);
}

/*
* supSaveSettings
*
* Purpose:
*
* Saves the current application settings to the current user's
* registry.
*
*/
VOID supSaveSettings(
    VOID
)
{
    DWORD value;

    HKEY hKey;
    WINDOWPLACEMENT wp;
    RECT rc;

    if (RegCreateKeyEx(HKEY_CURRENT_USER,
        REG_KEY_PATH,
        0,
        NULL,
        0,
        KEY_WRITE,
        NULL,
        &hKey,
        NULL) != ERROR_SUCCESS)
    {
        return;
    }

    RtlSecureZeroMemory(&wp, sizeof(wp));
    wp.length = sizeof(wp);
    GetWindowPlacement(g_ctx.hMainWnd, &wp);

    rc = wp.rcNormalPosition;
    value = (DWORD)rc.left;

    RegSetValueEx(hKey,
        TEXT("WindowX"),
        0,
        REG_DWORD,
        (const BYTE*)&value,
        sizeof(value));

    value = (DWORD)rc.top;

    RegSetValueEx(hKey,
        TEXT("WindowY"),
        0,
        REG_DWORD,
        (const BYTE*)&value,
        sizeof(value));

    value = (DWORD)(rc.right - rc.left);

    RegSetValueEx(hKey,
        TEXT("WindowW"),
        0,
        REG_DWORD,
        (const BYTE*)&value,
        sizeof(value));

    value = (DWORD)(rc.bottom - rc.top);

    RegSetValueEx(hKey,
        TEXT("WindowH"),
        0,
        REG_DWORD,
        (const BYTE*)&value,
        sizeof(value));

    value = (DWORD)g_ctx.selectedLevel;

    RegSetValueEx(hKey,
        TEXT("Level"),
        0,
        REG_DWORD,
        (const BYTE*)&value,
        sizeof(value));

    value = g_ctx.liveEventLimit;
    RegSetValueEx(hKey,
        TEXT("LiveEventLimit"),
        0,
        REG_DWORD,
        (const BYTE*)&value,
        sizeof(value));

    value = g_ctx.autoScrollEnabled;
    RegSetValueEx(hKey,
        TEXT("AutoScroll"),
        0,
        REG_DWORD,
        (const BYTE*)&value,
        sizeof(value));

    RegSetValueEx(hKey,
        TEXT("Keyword"),
        0,
        REG_SZ,
        (const BYTE*)g_ctx.keywordFilterText,
        (DWORD)((supStrLen(g_ctx.keywordFilterText) + 1) * sizeof(TCHAR)));

    RegSetValueEx(hKey,
        TEXT("EventIdFilter"),
        0,
        REG_SZ,
        (const BYTE*)g_ctx.eventIdFilterText,
        (DWORD)((supStrLen(g_ctx.eventIdFilterText) + 1) * sizeof(WCHAR)));

    RegCloseKey(hKey);
}

/*
* supSetWppTmfPath
*
* Purpose:
*
* Displays a folder selection dialog for choosing the search path
* used to locate WPP TMF files and stores the selected directory.
*
*/
VOID supSetWppTmfPath(
    _In_ HWND WindowHandle
)
{
    BROWSEINFO browseInfo;
    PIDLIST_ABSOLUTE pidl;

    WCHAR path[MAX_PATH];

    RtlSecureZeroMemory(&browseInfo, sizeof(browseInfo));
    RtlSecureZeroMemory(path, sizeof(path));

    browseInfo.hwndOwner = WindowHandle;
    browseInfo.lpszTitle = TEXT("Select a folder containing .tmf files for WPP decoding");
    browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    pidl = SHBrowseForFolder(&browseInfo);
    if (!pidl)
        return;

    if (SHGetPathFromIDList(pidl, path)) {
        StringCchCopy(g_ctx.wppTmfPath, ARRAYSIZE(g_ctx.wppTmfPath), path);
    }

    CoTaskMemFree(pidl);
}

/*
* supEscapeCsvField
*
* Purpose:
*
* Escapes a string for use as a CSV field.
*
* Fields containing commas, quotes, or line breaks are enclosed in
* double quotes, and embedded double quotes are doubled according
* to CSV escaping rules.
*
* The output is always null-terminated when OutputChars is nonzero.
* If the output buffer is too small, the field is truncated safely.
*
*/
VOID supEscapeCsvField(
    _In_ LPCWSTR Input,
    _Out_writes_(OutputChars) LPWSTR Output,
    _In_ SIZE_T OutputChars
)
{
    BOOLEAN needsQuote;
    SIZE_T inputIndex, outputIndex;

    if (!Output ||
        OutputChars == 0)
    {
        return;
    }

    if (!Input)
        Input = TEXT("");

    needsQuote = (supStrChr(Input, TEXT(',')) != NULL) ||
        (supStrChr(Input, TEXT('"')) != NULL) ||
        (supStrChr(Input, TEXT('\n')) != NULL) ||
        (supStrChr(Input, TEXT('\r')) != NULL);

    if (!needsQuote) {
        supStrNCopy(Output, OutputChars, Input, MAXULONG_PTR);
        return;
    }

    outputIndex = 0;

    //
    // Reserve one character for the terminating null.
    //
    if (outputIndex + 1 < OutputChars)
        Output[outputIndex++] = TEXT('"');

    for (inputIndex = 0;
        Input[inputIndex] != UNICODE_NULL;
        inputIndex++)
    {
        if (Input[inputIndex] == TEXT('"')) {

            //
            // An embedded quote requires two output characters.
            // Leave one character available for the null terminator.
            //
            if (outputIndex + 2 >= OutputChars)
                break;

            Output[outputIndex++] = TEXT('"');
            Output[outputIndex++] = TEXT('"');
        }
        else {

            //
            // One output character plus the null terminator.
            //
            if (outputIndex + 1 >= OutputChars)
                break;

            Output[outputIndex++] = Input[inputIndex];
        }
    }

    //
    // Add the closing quote when there is room.
    //
    if (outputIndex + 1 < OutputChars)
        Output[outputIndex++] = TEXT('"');

    Output[outputIndex] = UNICODE_NULL;
}

/*
* supWriteUtf8String
*
* Purpose:
*
* Converts a Unicode string to UTF-8 and writes the resulting bytes
* to the specified file handle.
*
*/
BOOL supWriteUtf8String(
    _In_ HANDLE FileHandle,
    _In_ LPCWSTR String
)
{
    BOOL bResult = FALSE;
    INT cch, cb;
    DWORD bytesWritten;
    SIZE_T length;
    PCHAR buffer;

    length = supStrLen(String);
    if (length > INT_MAX)
        return FALSE;

    cch = (INT)length;
    if (cch == 0)
        return TRUE;

    cb = WideCharToMultiByte(CP_UTF8,
        0,
        String,
        cch,
        NULL,
        0,
        NULL,
        NULL);

    if (cb <= 0)
        return FALSE;

    buffer = (PCHAR)supHeapAlloc(cb);
    if (!buffer)
        return FALSE;

    do {

        if (WideCharToMultiByte(CP_UTF8,
            0,
            String,
            cch,
            buffer,
            cb,
            NULL,
            NULL) != cb)
        {
            break;
        }

        if (!WriteFile(FileHandle,
            buffer,
            cb,
            &bytesWritten,
            NULL))
        {
            break;
        }

        bResult = (bytesWritten == (DWORD)cb);

    } while (FALSE);

    supHeapFree(buffer);

    return bResult;
}

/*
* supTreeContextGetSelection
*
* Purpose:
*
* Retrieves and validates the tree item stored as the current context
* menu selection.
*
* The routine resolves the tree item's node parameter and returns the
* corresponding context kind and item index when the selection refers
* to a supported node type.
*
*/
BOOL supTreeContextGetSelection(
    _Out_ PTREE_CONTEXT_KIND ContextKind,
    _Out_ PULONG Index
)
{
    TVITEM item;

    *ContextKind = TreeContextInvalid;
    *Index = 0;

    if (g_ctx.ctxTreeItem == NULL)
        return FALSE;

    RtlSecureZeroMemory(&item, sizeof(item));

    item.mask = TVIF_PARAM;
    item.hItem = g_ctx.ctxTreeItem;

    if (!TreeView_GetItem(g_ctx.hTree, &item))
        return FALSE;

    switch (NODE_KIND(item.lParam)) {

    case NODE_KIND_SESSION:
        *ContextKind = TreeContextSession;
        break;

    case NODE_KIND_PROVIDER:
        *ContextKind = TreeContextProvider;
        break;
    default:
        return FALSE;
    }

    *Index = NODE_INDEX(item.lParam);

    return TRUE;
}

/*
* supTbInitialize
*
* Purpose:
*
* Initializes a TEXT_BUFFER structure and allocates its initial
* character buffer.
*
*/
BOOL supTbInitialize(
    _Out_ PTEXT_BUFFER Buffer,
    _In_ SIZE_T InitialCapacity
)
{
    RtlSecureZeroMemory(Buffer, sizeof(TEXT_BUFFER));

    Buffer->Buffer = (LPWSTR)supHeapAlloc(InitialCapacity * sizeof(WCHAR));
    if (Buffer->Buffer == NULL)
        return FALSE;

    Buffer->Capacity = InitialCapacity;
    return TRUE;
}

/*
* supTbDestroy
*
* Purpose:
*
* Releases the storage owned by a TEXT_BUFFER and resets the structure
* to its initial state.
*
*/
VOID supTbDestroy(
    _Inout_ PTEXT_BUFFER Buffer
)
{
    if (Buffer->Buffer)
        supHeapFree(Buffer->Buffer);

    RtlSecureZeroMemory(Buffer, sizeof(TEXT_BUFFER));
}

/*
* supTbReserve
*
* Purpose:
*
* Ensures that a TEXT_BUFFER has sufficient capacity for the specified
* number of additional characters.
*
*/
BOOL supTbReserve(
    _Inout_ PTEXT_BUFFER Buffer,
    _In_ SIZE_T ExtraChars
)
{
    SIZE_T required, newCapacity, allocationSize;
    LPWSTR newBuffer;

    if (ExtraChars > (SIZE_MAX - Buffer->Length - 1))
        return FALSE;

    required = Buffer->Length + ExtraChars + 1;

    if (required <= Buffer->Capacity)
        return TRUE;

    newCapacity = Buffer->Capacity;

    if (newCapacity == 0)
        newCapacity = 64;

    while (newCapacity < required) {

        if (newCapacity > SIZE_MAX / 2) {
            newCapacity = required;
            break;
        }

        newCapacity *= 2;
    }

    if (newCapacity > SIZE_MAX / sizeof(WCHAR))
        return FALSE;

    allocationSize = newCapacity * sizeof(WCHAR);
    newBuffer = (LPWSTR)supHeapReAlloc(Buffer->Buffer, allocationSize);
    if (newBuffer == NULL)
        return FALSE;

    Buffer->Buffer = newBuffer;
    Buffer->Capacity = newCapacity;

    return TRUE;
}

/*
* supTbAppend
*
* Purpose:
*
* Appends a null-terminated Unicode string to a TEXT_BUFFER.
*
*/
BOOL supTbAppend(
    _Inout_ PTEXT_BUFFER Buffer,
    _In_ LPCWSTR Text
)
{
    SIZE_T length;

    length = supStrLen(Text);
    if (!supTbReserve(Buffer, length))
        return FALSE;

    StringCchCopy(Buffer->Buffer + Buffer->Length,
        Buffer->Capacity - Buffer->Length,
        Text);

    Buffer->Length += length;

    return TRUE;
}

/*
* supFormatSchemaRowColumn
*
* Purpose:
*
* Formats the specified schema row field for display in the requested
* ListView column.
*
*/
VOID supFormatSchemaRowColumn(
    _In_ PEVENT_SCHEMA_ROW Row,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    Buffer[0] = 0;

    switch (Column) {

    case SCHEMA_COLUMN_EVENT_ID:
        StringCchPrintf(Buffer, BufferChars, TEXT("%u"), Row->id);
        break;

    case SCHEMA_COLUMN_VERSION:
        StringCchPrintf(Buffer, BufferChars, TEXT("%u"), Row->version);
        break;

    case SCHEMA_COLUMN_LEVEL:
        StringCchCopy(Buffer, BufferChars, Row->levelName);
        break;

    case SCHEMA_COLUMN_TASK:
        StringCchCopy(Buffer, BufferChars, Row->taskName);
        break;

    case SCHEMA_COLUMN_OPCODE:
        StringCchCopy(Buffer, BufferChars, Row->opcodeName);
        break;

    case SCHEMA_COLUMN_KEYWORD:
        StringCchPrintf(Buffer, BufferChars, TEXT("0x%016llX"), Row->keyword);
        break;

    case SCHEMA_COLUMN_PROPERTIES:
        StringCchCopy(Buffer, BufferChars, Row->propertyNames);
        break;
    }
}

/*
* supFormatMofSchemaRowColumn
*
* Purpose:
*
* Formats the specified MOF row field for display in the
* requested ListView column.
*
*/
VOID supFormatMofSchemaRowColumn(
    _In_ PMOF_SCHEMA_ROW Row,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    Buffer[0] = 0;

    switch (Column) {

    case MOF_SCHEMA_COLUMN_EVENT_TYPE:
        StringCchPrintf(Buffer, BufferChars, TEXT("%lu"), Row->eventType);
        break;

    case MOF_SCHEMA_COLUMN_EVENT_TYPES:
        StringCchCopy(Buffer, BufferChars, Row->eventTypes);
        break;

    case MOF_SCHEMA_COLUMN_CLASS:
        StringCchCopy(Buffer, BufferChars, Row->className);
        break;

    case MOF_SCHEMA_COLUMN_PROPERTIES:
        StringCchCopy(Buffer, BufferChars, Row->propertyNames);
        break;

    case MOF_SCHEMA_COLUMN_DESCRIPTION:
        StringCchCopy(Buffer, BufferChars, Row->description);
        break;
    }
}

/*
* supFormatLiveRowColumn
*
* Purpose:
*
* Formats the specified live event row field for display in the
* requested ListView column.
*
*/
VOID supFormatLiveRowColumn(
    _In_ PCLIVE_EVENT_ROW Row,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    Buffer[0] = 0;

    switch (Column) {

    case LIVE_COLUMN_TIME:
        StringCchPrintf(Buffer, BufferChars, TEXT("%02u:%02u:%02u.%03u"),
            Row->localTime.wHour, Row->localTime.wMinute,
            Row->localTime.wSecond, Row->localTime.wMilliseconds);
        break;

    case LIVE_COLUMN_PROVIDER:
        StringCchCopy(Buffer, BufferChars, Row->providerLabel);
        break;

    case LIVE_COLUMN_EVENT_ID:
        StringCchPrintf(Buffer, BufferChars, TEXT("%u"), Row->eventId);
        break;

    case LIVE_COLUMN_LEVEL:
        StringCchPrintf(Buffer, BufferChars, TEXT("%u"), Row->level);
        break;

    case LIVE_COLUMN_KEYWORD:
        StringCchPrintf(Buffer, BufferChars, TEXT("0x%016llX"), Row->keyword);
        break;

    case LIVE_COLUMN_PROPERTIES:
        StringCchCopy(Buffer, BufferChars, Row->properties);
        break;
    }
}

VOID supxFitListColumnText(
    _In_ INT Column,
    _In_ PCWSTR Text
)
{
    INT width;
    HDC hdc;
    HFONT hFont, oldFont;
    SIZE size;
    LVCOLUMN column;

    if (!Text || !Text[0])
        return;

    hdc = GetDC(g_ctx.hList);
    if (!hdc)
        return;

    hFont = (HFONT)SendMessage(g_ctx.hList, WM_GETFONT, 0, 0);
    oldFont = NULL;

    if (hFont)
        oldFont = (HFONT)SelectObject(hdc, hFont);

    if (GetTextExtentPoint32(hdc,
        Text,
        (INT)supStrLen(Text),
        &size))
    {
        RtlSecureZeroMemory(&column, sizeof(column));
        column.mask = LVCF_WIDTH;

        if (ListView_GetColumn(g_ctx.hList, Column, &column)) {

            width = size.cx + 32;
            if (width > column.cx)
                ListView_SetColumnWidth(g_ctx.hList,
                    Column,
                    width);
        }
    }

    if (oldFont)
        SelectObject(hdc, oldFont);

    ReleaseDC(g_ctx.hList, hdc);
}
/*
* supGetVirtualRowColumnText
*
* Purpose:
*
* Retrieves and formats the text for a virtual ListView row according
* to the current list mode and requested column.
*
* Synchronizes access to live event data while retrieving live rows.
*
*/
VOID supGetVirtualRowColumnText(
    _In_ INT Row,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    PPROVIDER_ENTRY provider;
    LIVE_EVENT_ROW liveRow;

    Buffer[0] = 0;

    switch (g_ctx.listMode) {

    case LIST_MODE_PROVIDER:
    case LIST_MODE_SESSION:

        if (Row < 0 ||
            (ULONG)Row >= g_ctx.detailRowCount)
        {
            return;
        }

        StringCchCopy(Buffer,
            BufferChars,
            (Column == 0) ?
            g_ctx.detailRows[Row].field :
            g_ctx.detailRows[Row].value);

        break;

    case LIST_MODE_SCHEMA:

        if (g_ctx.selectedProviderIdx >= g_ctx.providerCount)
            return;

        provider = &g_ctx.providers[g_ctx.selectedProviderIdx];

        if (provider->isManifestProvider) {

            if (provider->schemaRowCount == 0) {

                if (Column == 0) {

                    StringCchCopy(Buffer,
                        BufferChars,
                        TEXT("(no manifest events found for this provider)"));
                }

                return;
            }

            if (Row < 0 ||
                (ULONG)Row >= provider->schemaRowCount)
            {
                return;
            }

            supFormatSchemaRowColumn(&provider->schemaRows[Row],
                Column,
                Buffer,
                BufferChars);
        }
        else {
            if (provider->mofSchemaRowCount == 0) {

                if (Column == 0) {

                    switch (provider->mofSchemaLoadStatus) {

                    case MofSchemaLoading:
                        StringCchCopy(Buffer,
                            BufferChars,
                            TEXT("(loading MOF schema from WMI...)"));
                        break;

                    case MofSchemaNoEventClasses:
                        StringCchCopy(Buffer,
                            BufferChars,
                            TEXT("(no decodable MOF event classes found for this provider)"));
                        break;

                    case MofSchemaWmiUnavailable:
                        StringCchCopy(Buffer,
                            BufferChars,
                            TEXT("(WMI is unavailable; MOF schema could not be queried)"));
                        break;

                    case MofSchemaEnumerationFailed:
                        StringCchPrintf(Buffer,
                            BufferChars,
                            TEXT("(MOF schema enumeration failed: 0x%08lX)"),
                            provider->mofSchemaLoadStatusCode);
                        break;

                    case MofSchemaDepthLimitReached:
                        StringCchCopy(Buffer,
                            BufferChars,
                            TEXT("(MOF schema traversal reached the inheritance-depth limit)"));
                        break;

                    default:
                        StringCchCopy(Buffer,
                            BufferChars,
                            TEXT("(no MOF schema found for this provider)"));
                        break;

                    }

                    supxFitListColumnText(0, Buffer);
                }

                return;
            }

            if (Row < 0 ||
                (ULONG)Row >= provider->mofSchemaRowCount)
            {
                return;
            }

            supFormatMofSchemaRowColumn(&provider->mofSchemaRows[Row],
                Column,
                Buffer,
                BufferChars);

            if (provider->mofSchemaLoadStatus ==
                MofSchemaDepthLimitReached &&
                Row == 0 &&
                Column == MOF_SCHEMA_COLUMN_DESCRIPTION)
            {
                if (Buffer[0] != UNICODE_NULL) {
                    StringCchCat(Buffer,
                        BufferChars,
                        TEXT(" (schema traversal truncated)"));
                }
                else {
                    StringCchCopy(Buffer,
                        BufferChars,
                        TEXT("(schema traversal truncated at depth limit)"));
                }
            }
        }
        break;

    case LIST_MODE_LIVE:

        if (Row < 0 ||
            !supCopyLiveEvent((ULONG)Row, &liveRow))
        {
            return;
        }

        supFormatLiveRowColumn(&liveRow, Column, Buffer, BufferChars);
        break;
    }
}

/*
* supGetVirtualRowIcon
*
* Purpose:
*
* Returns the appropriate ListView icon for a virtual row based on its
* event level. Handles schema and live event rows according to the
* current list mode.
*
*/
INT supGetVirtualRowIcon(
    _In_ INT Row
)
{
    UCHAR level;
    PPROVIDER_ENTRY provider;
    LIVE_EVENT_ROW liveRow;

    switch (g_ctx.listMode) {

    case LIST_MODE_SCHEMA:

        if (g_ctx.selectedProviderIdx >= g_ctx.providerCount)
            return ICON_INFO;

        provider = &g_ctx.providers[g_ctx.selectedProviderIdx];

        if (Row < 0 ||
            (ULONG)Row >= provider->schemaRowCount)
        {
            return ICON_INFO;
        }

        return supIconForLevel(provider->schemaRows[Row].level);

    case LIST_MODE_LIVE:

        level = TRACE_LEVEL_NONE;

        if (Row >= 0 &&
            supCopyLiveEvent((ULONG)Row, &liveRow))
        {
            level = liveRow.level;
        }

        return supIconForLevel(level);
    }

    return ICON_INFO;
}

/*
* supTryParseFullyNumeric
*
* Purpose:
*
* Determines whether the supplied string contains only a valid decimal
* or hexadecimal integer and, if so, converts it to an unsigned 64-bit
* value.
*
*/
BOOL supTryParseFullyNumeric(
    _In_ LPCWSTR Text,
    _Out_ PULONGLONG Value
)
{
    LPCWSTR p;

    *Value = 0;
    if (!Text || !*Text)
        return FALSE;

    p = Text;

    if (p[0] == L'0' &&
        (p[1] == L'x' || p[1] == L'X'))
    {
        p += 2;

        if (!*p)
            return FALSE;

        while (*p) {

            if (!supIsDigit(*p))
                return FALSE;

            p++;
        }

        *Value = supHexToUInt64(Text + 2);
    }
    else {

        while (*p) {

            if (!supIsDigit(*p))
                return FALSE;

            p++;
        }

        supStrToUInt64(Text, Value);
    }

    return TRUE;
}

/*
* supCompareTextNumericAware
*
* Purpose:
*
* Compares two strings numerically when both contain valid integer
* values. Falls back to a text comparison when either string is not
* fully numeric.
*
*/
INT supCompareTextNumericAware(
    _In_ LPCWSTR String1,
    _In_ LPCWSTR String2
)
{
    ULONGLONG value1, value2;

    if (supTryParseFullyNumeric(String1, &value1) &&
        supTryParseFullyNumeric(String2, &value2))
    {
        if (value1 < value2)
            return -1;

        if (value1 > value2)
            return 1;

        return 0;
    }

    return supStrCmp(String1, String2);
}

/*
* supUncheckAllProviders
*
* Purpose:
*
* Clears the provider selection state in both the internal provider
* checkbox array and the corresponding TreeView checkbox states.
*
*/
VOID supUncheckAllProviders(
    VOID
)
{
    ULONG i;
    HTREEITEM hItem;
    TVITEM item;

    if (g_ctx.providerChecked) {

        for (i = 0; i < g_ctx.providerCount; i++)
            g_ctx.providerChecked[i] = FALSE;
    }

    if (!g_ctx.hProvRoot)
        return;

    RtlSecureZeroMemory(&item, sizeof(item));

    item.mask = TVIF_HANDLE | TVIF_STATE;
    item.stateMask = TVIS_STATEIMAGEMASK;
    item.state = INDEXTOSTATEIMAGEMASK(1);

    hItem = TreeView_GetChild(g_ctx.hTree, g_ctx.hProvRoot);
    while (hItem) {
        item.hItem = hItem;
        TreeView_SetItem(g_ctx.hTree, &item);
        hItem = TreeView_GetNextSibling(g_ctx.hTree, hItem);
    }
}

/*
* supGoToProvider
*
* Purpose:
*
* Locates a provider by GUID and selects its corresponding TreeView
* item. Clears the provider filter when necessary so that the provider
* can be found, then ensures the item is visible and focused.
*
*/
VOID supGoToProvider(
    _In_ const GUID * ProviderGuid
)
{
    ULONG foundIndex, index;
    HTREEITEM hChild;
    TVITEM item;
    WCHAR filterText[128];

    foundIndex = g_ctx.providerCount;

    for (index = 0; index < g_ctx.providerCount; index++) {

        if (sizeof(GUID) == RtlCompareMemory(&g_ctx.providers[index].guid,
            ProviderGuid, sizeof(GUID)))
        {
            foundIndex = index;
            break;
        }
    }

    if (foundIndex >= g_ctx.providerCount) {

        MessageBox(g_ctx.hMainWnd,
            TEXT("This provider isn't in the current provider list "
                "(it may have appeared after the last Refresh)."),
            TEXT("Go to Provider"),
            MB_OK | MB_ICONINFORMATION);

        return;
    }

    filterText[0] = TEXT('\0');
    GetWindowText(g_ctx.hEditFilter, filterText, ARRAYSIZE(filterText));
    if (filterText[0] != TEXT('\0')) {
        //
        // EN_CHANGE handler will rebuild the provider tree.
        //
        SetWindowText(g_ctx.hEditFilter, TEXT(""));
    }

    if (!g_ctx.hProvRoot)
        return;

    hChild = TreeView_GetChild(g_ctx.hTree, g_ctx.hProvRoot);
    while (hChild) {

        RtlSecureZeroMemory(&item, sizeof(item));

        item.mask = TVIF_PARAM;
        item.hItem = hChild;

        if (TreeView_GetItem(g_ctx.hTree, &item)) {
            if (NODE_KIND(item.lParam) == NODE_KIND_PROVIDER &&
                NODE_INDEX(item.lParam) == foundIndex)
            {
                TreeView_SelectItem(g_ctx.hTree, hChild);
                TreeView_EnsureVisible(g_ctx.hTree, hChild);
                SetFocus(g_ctx.hTree);
                return;
            }
        }

        hChild = TreeView_GetNextSibling(g_ctx.hTree, hChild);
    }
}

/*
* supCompareDetailRows
*
* Purpose:
*
* Compares two detail rows using the currently selected column and
* sort direction.
*
*/
INT CALLBACK supCompareDetailRows(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2
)
{
    INT result;
    PDETAIL_ROW row1, row2;
    LPCWSTR text1, text2;

    row1 = (PDETAIL_ROW)Element1;
    row2 = (PDETAIL_ROW)Element2;

    text1 = (g_ctx.sortColumn == 0) ?
        row1->field :
        row1->value;

    text2 = (g_ctx.sortColumn == 0) ?
        row2->field :
        row2->value;

    result = supCompareTextNumericAware(text1, text2);

    if (!g_ctx.sortAscending)
        result = -result;

    return result;
}

/*
* supCompareMofSchemaRows
*
* Purpose:
*
* Compares two MOF schema rows using the currently selected column and
* sort direction.
*
*/
INT CALLBACK supCompareMofSchemaRows(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2
)
{
    INT result;
    WCHAR buffer1[512];
    WCHAR buffer2[512];

    supFormatMofSchemaRowColumn((PMOF_SCHEMA_ROW)Element1,
        g_ctx.sortColumn,
        buffer1,
        ARRAYSIZE(buffer1));

    supFormatMofSchemaRowColumn((PMOF_SCHEMA_ROW)Element2,
        g_ctx.sortColumn,
        buffer2,
        ARRAYSIZE(buffer2));

    result = supCompareTextNumericAware(buffer1, buffer2);
    if (!g_ctx.sortAscending)
        result = -result;

    return result;
}

/*
* supCompareSchemaRows
*
* Purpose:
*
* Compares two schema rows using the currently selected column and
* sort direction.
*
*/
INT CALLBACK supCompareSchemaRows(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2
)
{
    INT result;
    WCHAR buffer1[512];
    WCHAR buffer2[512];

    supFormatSchemaRowColumn((EVENT_SCHEMA_ROW*)Element1,
        g_ctx.sortColumn,
        buffer1,
        ARRAYSIZE(buffer1));

    supFormatSchemaRowColumn((EVENT_SCHEMA_ROW*)Element2,
        g_ctx.sortColumn,
        buffer2,
        ARRAYSIZE(buffer2));

    result = supCompareTextNumericAware(buffer1, buffer2);
    if (!g_ctx.sortAscending)
        result = -result;

    return result;
}

/*
* supxCompareSystemTime
*
* Purpose:
*
* Compares the time-of-day values of two live event rows by hour,
* minute, second, and millisecond.
*
*/
INT supxCompareSystemTime(
    _In_ PCLIVE_EVENT_ROW RowA,
    _In_ PCLIVE_EVENT_ROW RowB
)
{
    if (RowA->localTime.wHour != RowB->localTime.wHour)
        return RowA->localTime.wHour < RowB->localTime.wHour ? -1 : 1;

    if (RowA->localTime.wMinute != RowB->localTime.wMinute)
        return RowA->localTime.wMinute < RowB->localTime.wMinute ? -1 : 1;

    if (RowA->localTime.wSecond != RowB->localTime.wSecond)
        return RowA->localTime.wSecond < RowB->localTime.wSecond ? -1 : 1;

    if (RowA->localTime.wMilliseconds != RowB->localTime.wMilliseconds)
        return RowA->localTime.wMilliseconds < RowB->localTime.wMilliseconds ? -1 : 1;

    return 0;
}

/*
* supxCompareUInt64
*
* Purpose:
*
* Compares two unsigned 64-bit integer values.
*
*/
INT supxCompareUInt64(
    _In_ ULONGLONG ValueA,
    _In_ ULONGLONG ValueB
)
{
    if (ValueA < ValueB)
        return -1;

    if (ValueA > ValueB)
        return 1;

    return 0;
}

/*
* supxCompareUInt16
*
* Purpose:
*
* Compares two unsigned 16-bit integer values.
*
*/
INT supxCompareUInt16(
    _In_ USHORT ValueA,
    _In_ USHORT ValueB
)
{
    if (ValueA < ValueB)
        return -1;

    if (ValueA > ValueB)
        return 1;

    return 0;
}

/*
* supxCompareUInt8
*
* Purpose:
*
* Compares two unsigned 8-bit integer values.
*
*/
INT supxCompareUInt8(
    _In_ UCHAR ValueA,
    _In_ UCHAR ValueB
)
{
    if (ValueA < ValueB)
        return -1;

    if (ValueA > ValueB)
        return 1;

    return 0;
}

/*
* supCompareLiveRows
*
* Purpose:
*
* Compares two live event rows according to the currently selected
* ListView column and sort direction. Numeric fields are compared
* directly without formatting them into temporary strings.
*
*/
INT CALLBACK supCompareLiveRows(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2
)
{
    INT result;
    PCLIVE_EVENT_ROW row1, row2;

    row1 = (PCLIVE_EVENT_ROW)Element1;
    row2 = (PCLIVE_EVENT_ROW)Element2;

    switch (g_ctx.sortColumn) {

    case LIVE_COLUMN_TIME:

        result = supxCompareSystemTime(row1, row2);
        break;

    case LIVE_COLUMN_PROVIDER:

        result = supStrCmpI(row1->providerLabel, row2->providerLabel);
        break;

    case LIVE_COLUMN_EVENT_ID:

        result = supxCompareUInt16(row1->eventId, row2->eventId);
        break;

    case LIVE_COLUMN_LEVEL:

        result = supxCompareUInt8(row1->level, row2->level);
        break;

    case LIVE_COLUMN_KEYWORD:

        result = supxCompareUInt64(row1->keyword, row2->keyword);
        break;

    case LIVE_COLUMN_PROPERTIES:

        result = supStrCmpI(row1->properties, row2->properties);
        break;

    default:
        result = 0;
        break;
    }

    if (!g_ctx.sortAscending)
        result = -result;

    return result;
}

/*
* supxSwapMemory
*
* Purpose:
*
* Swaps two memory blocks using a caller-provided temporary buffer.
* The temporary buffer is reused by the caller to avoid per-swap
* allocations.
*
*/
VOID supxSwapMemory(
    _Inout_updates_bytes_(Size) PBYTE Left,
    _Inout_updates_bytes_(Size) PBYTE Right,
    _In_ SIZE_T Size,
    _Out_writes_bytes_(Size) PBYTE Temporary
)
{
    if (Left == Right)
        return;

    RtlCopyMemory(Temporary, Left, Size);
    RtlCopyMemory(Left, Right, Size);
    RtlCopyMemory(Right, Temporary, Size);
}

/*
* supxSiftDown
*
* Purpose:
*
* Restores the heap property for a subtree by moving the root
* element downward until it reaches its correct position.
*
*/
VOID supxSiftDown(
    _Inout_updates_bytes_(Count * ElementSize) PBYTE Base,
    _In_ ULONG Root,
    _In_ ULONG Count,
    _In_ SIZE_T ElementSize,
    _In_ PSUP_COMPARE_ROUTINE CompareRoutine,
    _Out_writes_bytes_(ElementSize) PBYTE Temporary
)
{
    ULONG child;

    while (Root < (Count / 2)) {

        child = (Root * 2) + 1;

        if ((child + 1) < Count &&
            CompareRoutine(Base + (child * ElementSize),
                Base + ((child + 1) * ElementSize)) < 0)
        {
            child++;
        }

        if (CompareRoutine(Base + (Root * ElementSize),
            Base + (child * ElementSize)) >= 0)
        {
            break;
        }

        supxSwapMemory(Base + (Root * ElementSize),
            Base + (child * ElementSize),
            ElementSize,
            Temporary);

        Root = child;

    }
}

/*
* supSort
*
* Purpose:
*
* Sorts an array of fixed-size elements in ascending order using
* the supplied comparison routine.
*
*/
VOID supSort(
    _Inout_updates_bytes_(Count * ElementSize) PVOID Base,
    _In_ ULONG Count,
    _In_ SIZE_T ElementSize,
    _In_ PSUP_COMPARE_ROUTINE CompareRoutine
)
{
    ULONG root, end;
    PBYTE base, temporary;

    if (!Base ||
        Count < 2 ||
        ElementSize == 0 ||
        !CompareRoutine)
    {
        return;
    }

    temporary = (PBYTE)supHeapAlloc(ElementSize);
    if (!temporary)
        return;

    base = (PBYTE)Base;

    root = Count / 2;
    while (root != 0) {
        root--;
        supxSiftDown(base,
            root,
            Count,
            ElementSize,
            CompareRoutine,
            temporary);

    }

    end = Count;
    while (end > 1) {

        end--;

        supxSwapMemory(base,
            base + (end * ElementSize),
            ElementSize,
            temporary);

        supxSiftDown(base,
            0,
            end,
            ElementSize,
            CompareRoutine,
            temporary);

    }

    supHeapFree(temporary);
}

/*
* supCompareLiveRowIndices
*
* Purpose:
*
* Compares two live-event array indices by comparing the
* corresponding live event rows.
*
*/
INT CALLBACK supCompareLiveRowIndices(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2
)
{
    ULONG index1, index2;

    index1 = *(const ULONG*)Element1;
    index2 = *(const ULONG*)Element2;
    return supCompareLiveRows(&g_ctx.liveEvents[index1], &g_ctx.liveEvents[index2]);
}

/*
* supSortLiveEvents
*
* Purpose:
*
* Builds and sorts an index array for the live-event ring buffer
* without changing the physical event order.
*
*/
VOID supSortLiveEvents(
    VOID
)
{
    ULONG index, physicalIndex, count;
    PULONG indices;

    EnterCriticalSection(&g_ctx.liveCs);

    do {

        count = g_ctx.liveEventCount;
        if (count < 2 ||
            g_ctx.liveEventCapacity == 0)
        {
            break;
        }

        indices = (PULONG)supHeapAlloc(count * sizeof(ULONG));
        if (!indices)
            break;

        for (index = 0; index < count; index++) {
            physicalIndex = (g_ctx.liveEventHead + index) % g_ctx.liveEventCapacity;
            indices[index] = physicalIndex;
        }

        supSort(indices, count, sizeof(ULONG), supCompareLiveRowIndices);

        supHeapFree(g_ctx.liveSortedIndices);
        g_ctx.liveSortedIndices = indices;
        g_ctx.liveSortedIndexCount = count;
        g_ctx.liveEventGeneration++;


    } while (FALSE);

    LeaveCriticalSection(&g_ctx.liveCs);
}

/*
* supRegisterDialogClass
*
* Purpose:
*
* Registers a window class used by the application's custom modal
* dialogs.
*
* The class is treated as successfully registered when it either
* registers normally or already exists.
*
*/
BOOL supRegisterDialogClass(
    _In_ LPCWSTR ClassName,
    _In_ WNDPROC WindowProc
)
{
    WNDCLASSEX wc;

    RtlSecureZeroMemory(&wc, sizeof(wc));

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = g_ctx.hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = ClassName;
    wc.hIcon = (HICON)g_ctx.hMainIcon;
    wc.hIconSm = (HICON)g_ctx.hMainIcon;

    return RegisterClassEx(&wc) != 0 ||
        GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

/*
* supCenterWindowRect
*
* Purpose:
*
* Centers a window rectangle relative to the specified parent window.
*
*/
VOID supCenterWindowRect(
    _In_ HWND ParentWindow,
    _Inout_ PRECT WindowRect
)
{
    INT width, height;
    RECT parentRect;

    GetWindowRect(ParentWindow, &parentRect);

    width = WindowRect->right - WindowRect->left;
    height = WindowRect->bottom - WindowRect->top;

    WindowRect->left = parentRect.left + ((parentRect.right - parentRect.left) - width) / 2;
    WindowRect->top = parentRect.top + ((parentRect.bottom - parentRect.top) - height) / 2;
    WindowRect->right = WindowRect->left + width;
    WindowRect->bottom = WindowRect->top + height;
}

/*
* supRefreshLivePaneIncremental
*
* Purpose:
*
* Updates the virtual live-event ListView when the number of captured
* events has changed.
*
*/
VOID supRefreshLivePaneIncremental(
    _In_ BOOL bAutoScroll
)
{
    INT last;
    ULONG count;
    ULONG64 generation;

    EnterCriticalSection(&g_ctx.liveCs);
    count = g_ctx.liveEventCount;
    generation = g_ctx.liveEventGeneration;
    LeaveCriticalSection(&g_ctx.liveCs);

    if (count == g_ctx.liveDisplayedCount &&
        generation == g_ctx.liveDisplayedGeneration)
    {
        return;
    }

    if (count != g_ctx.liveDisplayedCount) {
        ListView_SetItemCountEx(g_ctx.hList, count, LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
    }
    else if (count != 0) {
        ListView_RedrawItems(g_ctx.hList, 0, (INT)count - 1);
    }
    g_ctx.liveDisplayedCount = count;
    g_ctx.liveDisplayedGeneration = generation;

    // Keep the latest event visible.
    if (bAutoScroll) {
        last = (int)count - 1;
        if (last >= 0)
            ListView_EnsureVisible(g_ctx.hList, last, FALSE);
    }
}

/*
* supInsertProviderTreeItem
*
* Purpose:
*
* Inserts a provider entry into the provider tree.
*
*/
VOID supInsertProviderTreeItem(
    _In_ HWND hTree,
    _In_ HTREEITEM hParent,
    _In_ ULONG idx
)
{
    TVINSERTSTRUCT tvis;
    TVITEM checkItem;
    HTREEITEM hItem;
    PPROVIDER_ENTRY provider;

    if (idx >= g_ctx.providerCount)
        return;

    RtlSecureZeroMemory(&tvis, sizeof(tvis));
    RtlSecureZeroMemory(&checkItem, sizeof(checkItem));

    provider = &g_ctx.providers[idx];

    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvis.item.pszText = provider->name[0] ?
        (LPWSTR)provider->name : (LPWSTR)TEXT("(unnamed)");

    tvis.item.lParam = MAKE_NODE_PARAM(NODE_KIND_PROVIDER, idx);
    tvis.item.iImage = provider->isManifestProvider ? ICON_PROVIDER : ICON_WARN;
    tvis.item.iSelectedImage = tvis.item.iImage;

    hItem = TreeView_InsertItem(hTree, &tvis);
    if (!hItem)
        return;

    if (g_ctx.providerChecked &&
        g_ctx.providerChecked[idx])
    {
        checkItem.mask = TVIF_STATE | TVIF_HANDLE;
        checkItem.hItem = hItem;
        checkItem.stateMask = TVIS_STATEIMAGEMASK;
        checkItem.state = INDEXTOSTATEIMAGEMASK(2);
        TreeView_SetItem(hTree, &checkItem);
    }
}

/*
* supResetProviderStats
*
* Purpose:
*
* Resets the live capture statistics maintained for all providers.
*
*/
VOID supResetProviderStats(
    VOID
)
{
    ULONG i;
    PPROVIDER_ENTRY entry;

    for (i = 0; i < g_ctx.providerCount; i++) {
        entry = &g_ctx.providers[i];
        entry->liveEventCount = 0;
        entry->liveEventCountAtLastTick = 0;
        entry->liveEventsPerSecond = 0.0;
        entry->liveHasLastEvent = FALSE;
    }
}

/*
* supUpdateProviderEventRates
*
* Purpose:
*
* Updates the per-second event rate for providers participating in the
* current live capture.
*
*/
VOID supUpdateProviderEventRates(
    VOID
)
{
    ULONG i, j;
    DOUBLE intervalSec;
    ULONGLONG current, last, delta;
    PPROVIDER_ENTRY provEntry;

    if (!g_ctx.liveCapturing || g_ctx.captureMode != CAPTURE_MODE_LIVE)
        return;

    intervalSec = LIVE_REFRESH_INTERVAL_MS / 1000.0;

    for (i = 0; i < g_ctx.liveProviderGuidCount; i++) {
        for (j = 0; j < g_ctx.providerCount; j++) {

            provEntry = &g_ctx.providers[j];

            if (sizeof(GUID) == RtlCompareMemory(&provEntry->guid,
                &g_ctx.liveProviderGuids[i],
                sizeof(GUID)))
            {
                current = provEntry->liveEventCount;
                last = provEntry->liveEventCountAtLastTick;
                delta = (current >= last) ? (current - last) : 0;
                provEntry->liveEventsPerSecond = (double)delta / intervalSec;
                provEntry->liveEventCountAtLastTick = current;
                break;
            }
        }
    }
}

/*
 * supxCreateDialog
 *
 * Purpose:
 *
 * Creates and initializes a dialog window using the specified
 * registered dialog window class.
 *
 */
HWND supxCreateDialog(
    _In_ HWND ParentWindow,
    _In_ LPCTSTR Title,
    _In_ INT Width,
    _In_ INT Height,
    _In_ LPCTSTR ClassName,
    _In_opt_ LPVOID Context
)
{
    DWORD dialogStyle, dialogExStyle;
    HWND hwnd;
    RECT windowRect;

    dialogStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dialogExStyle = WS_EX_DLGMODALFRAME;

    SetRect(&windowRect, 0, 0, Width, Height);
    AdjustWindowRectEx(&windowRect, dialogStyle, FALSE, dialogExStyle);
    supCenterWindowRect(ParentWindow, &windowRect);

    hwnd = CreateWindowEx(dialogExStyle,
        ClassName,
        Title,
        dialogStyle,
        windowRect.left,
        windowRect.top,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        ParentWindow,
        NULL,
        g_ctx.hInstance,
        Context);

    if (hwnd)
        supApplyUIFont(hwnd);

    return hwnd;
}

/*
 * supCreateModalDialog
 *
 * Purpose:
 *
 * Creates and initializes a modal dialog window using the application's
 * standard dialog window class.
 *
 */
HWND supCreateModalDialog(
    _In_ HWND ParentWindow,
    _In_ LPCTSTR Title,
    _In_ INT Width,
    _In_ INT Height,
    _In_ PETWX_DIALOG_CONTEXT Context
)
{
    return supxCreateDialog(ParentWindow,
        Title,
        Width,
        Height,
        ETWXDLG_WNDCLASS,
        Context);
}


/*
 * supCreateModelessDialog
 *
 * Purpose:
 *
 * Creates and initializes a modeless dialog window using the specified
 * registered dialog window class.
 *
 */
HWND supCreateModelessDialog(
    _In_ HWND ParentWindow,
    _In_ LPCTSTR Title,
    _In_ INT Width,
    _In_ INT Height,
    _In_ LPCTSTR ClassName,
    _In_opt_ LPVOID Context
)
{
    return supxCreateDialog(ParentWindow,
        Title,
        Width,
        Height,
        ClassName,
        Context);
}

/*
 * supCreateMetadataDialog
 *
 * Purpose:
 *
 * Creates and initializes the resizable ETW provider metadata
 * viewer window.
 *
 */
HWND supCreateMetadataDialog(
    _In_ HWND ParentWindow,
    _In_ LPCTSTR Title,
    _In_ INT Width,
    _In_ INT Height,
    _In_ PETWX_DIALOG_CONTEXT Context
)
{
    DWORD dialogStyle;
    DWORD dialogExStyle;
    HWND hwnd;
    RECT windowRect;

    dialogStyle =
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU |
        WS_THICKFRAME |
        WS_MINIMIZEBOX |
        WS_MAXIMIZEBOX;

    dialogExStyle = WS_EX_DLGMODALFRAME;

    SetRect(&windowRect, 0, 0, Width, Height);
    AdjustWindowRectEx(&windowRect, dialogStyle, FALSE, dialogExStyle);
    supCenterWindowRect(ParentWindow, &windowRect);

    hwnd = CreateWindowEx(dialogExStyle,
        ETWXDLG_WNDCLASS,
        Title,
        dialogStyle,
        windowRect.left,
        windowRect.top,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        ParentWindow,
        NULL,
        g_ctx.hInstance,
        Context);

    if (hwnd)
        supApplyUIFont(hwnd);

    return hwnd;
}

/*
* supRunModalDialog
*
* Purpose:
*
* Runs a modal dialog message loop while disabling the parent window.
*
*/
VOID supRunModalDialog(
    _In_ HWND ParentWindow,
    _Inout_ HWND * Dialog,
    _In_opt_ HWND FocusControl
)
{
    INT result;
    HWND hDialog = *Dialog;
    PETWX_DIALOG_CONTEXT Context;
    MSG msg;

    Context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hDialog, GWLP_USERDATA);

    EnableWindow(ParentWindow, FALSE);

    ShowWindow(hDialog, SW_SHOW);

    if (FocusControl) {
        SetFocus(FocusControl);
        SendMessage(FocusControl, EM_SETSEL, 0, -1);
    }

    while (IsWindow(hDialog)) {

        result = GetMessage(&msg, NULL, 0, 0);
        if (result <= 0)
            break;

        if (!IsDialogMessage(hDialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(ParentWindow, TRUE);

    SetForegroundWindow(ParentWindow);

    if (IsWindow(hDialog))
        DestroyWindow(hDialog);

    *Dialog = NULL;
}

/*
* supRegisterDialogClassOnce
*
* Purpose:
*
* Registers a dialog window class only once by tracking whether the
* class has already been registered.
*
*/
VOID supRegisterDialogClassOnce(
    _In_ PBOOLEAN Registered,
    _In_ LPCTSTR ClassName,
    _In_ WNDPROC DialogProc
)
{
    if (!*Registered) {
        supRegisterDialogClass(ClassName, DialogProc);
        *Registered = TRUE;
    }
}

/*
* supCreateControl
*
* Purpose:
*
* Creates a child window or control using the application's instance
* handle and applies the application's standard UI font to it.
*
*/
HWND supCreateControl(
    _In_ DWORD ExStyle,
    _In_ LPCTSTR ClassName,
    _In_opt_ LPCTSTR Text,
    _In_ DWORD Style,
    _In_ INT X,
    _In_ INT Y,
    _In_ INT Width,
    _In_ INT Height,
    _In_ HWND Parent,
    _In_opt_ HMENU Id
)
{
    HWND hwnd;

    hwnd = CreateWindowEx(ExStyle,
        ClassName,
        Text,
        Style,
        X,
        Y,
        Width,
        Height,
        Parent,
        Id,
        g_ctx.hInstance,
        NULL);

    if (hwnd)
        supApplyUIFont(hwnd);

    return hwnd;
}

/*
* supMeasureText
*
* Purpose:
*
* Measures the dimensions required to display the specified text using
* the supplied font.
*
*/
SIZE supMeasureText(
    _In_ LPCWSTR Text,
    _In_ HFONT Font
)
{
    HDC hdc;
    HFONT oldFont;
    SIZE size;
    RECT rc;

    SetRect(&rc, 0, 0, 0, 0);

    hdc = GetDC(NULL);

    oldFont = NULL;

    if (Font)
        oldFont = (HFONT)SelectObject(hdc, Font);

    DrawText(hdc,
        Text,
        -1,
        &rc,
        DT_CALCRECT |
        DT_LEFT |
        DT_NOPREFIX);

    if (oldFont)
        SelectObject(hdc, oldFont);

    ReleaseDC(NULL, hdc);

    size.cx = rc.right - rc.left;
    size.cy = rc.bottom - rc.top;

    return size;
}

/*
* supCreateToolbar
*
* Purpose:
*
* Create and initialize toolbar component.
*
*/
HWND supCreateToolbar(
    _In_ HWND hWnd,
    _Out_ HIMAGELIST * phToolbarImageList
)
{
    HWND hToolbar;
    HIMAGELIST imageList;

    TBBUTTON tbButtons[] = {
        { TBLIST_ICON_START_CAPTURE, ID_BTN_START, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0, (INT_PTR)TEXT("Start") },
        { TBLIST_ICON_STOP_CAPTURE, ID_BTN_STOP, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0, (INT_PTR)TEXT("Stop") },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, { 0 }, 0, 0 },
        { TBLIST_ICON_FIND, ID_MENU_FIND, 0, BTNS_BUTTON, { 0 }, 0, (INT_PTR)TEXT("Find") },
        { TBLIST_ICON_BROWSE_FOR_SCHEMA, ID_BTN_SCHEMA, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0, (INT_PTR)TEXT("Schema") },
        { TBLIST_ICON_REFRESH, ID_MENU_REFRESH, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0, (INT_PTR)TEXT("Refresh") },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, { 0 }, 0, 0 },
        { TBLIST_ICON_EXPORT, ID_BTN_EXPORT, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0, (INT_PTR)TEXT("Export") }

    };

    hToolbar = CreateWindowEx(0,
        TOOLBARCLASSNAME,
        NULL,
        WS_CHILD |
        WS_VISIBLE |
        TBSTYLE_FLAT |
        TBSTYLE_TOOLTIPS |
        CCS_NORESIZE |
        CCS_NOPARENTALIGN,
        0,
        0,
        0,
        TOOLBAR_HEIGHT,
        hWnd,
        NULL,
        g_ctx.hInstance,
        NULL);

    *phToolbarImageList = NULL;

    if (!hToolbar)
        return NULL;

    SendMessage(hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    imageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 6, 0);

    if (imageList) {
        ImageList_AddIcon(imageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_START_CAPTURE)));
        ImageList_AddIcon(imageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_STOP_CAPTURE)));
        ImageList_AddIcon(imageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_BROWSE_FOR_SCHEMA)));
        ImageList_AddIcon(imageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_REFRESH)));
        ImageList_AddIcon(imageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_EXPORT)));
        ImageList_AddIcon(imageList, LoadIcon(g_ctx.hInstance, MAKEINTRESOURCE(IDI_ICON_FIND)));

        SendMessage(hToolbar, TB_SETIMAGELIST, 0, (LPARAM)imageList);
        *phToolbarImageList = imageList;
    }

    SendMessage(hToolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(80, 28));
    SendMessage(hToolbar, TB_ADDBUTTONS, ARRAYSIZE(tbButtons), (LPARAM)tbButtons);
    supApplyUIFont(hToolbar);

    return hToolbar;
}

/*
* supUpdateToolbarState
*
* Purpose:
*
* Updates the enabled state of toolbar buttons to reflect
* the current application state.
*
*/
VOID supUpdateToolbarState(
    VOID
)
{
    BOOL mofSchemaWorkerActive = InterlockedCompareExchange(&g_ctx.mofSchemaWorkerActive, FALSE, FALSE) != FALSE;

    SendMessage(g_ctx.hToolbar, TB_ENABLEBUTTON, ID_BTN_START, MAKELONG(!g_ctx.liveCapturing, 0));
    SendMessage(g_ctx.hToolbar, TB_ENABLEBUTTON, ID_BTN_STOP, MAKELONG(g_ctx.liveCapturing, 0));
    SendMessage(g_ctx.hToolbar, TB_ENABLEBUTTON, ID_BTN_SCHEMA, MAKELONG(!mofSchemaWorkerActive &&
        g_ctx.selectedProviderIdx < g_ctx.providerCount, 0));

    SendMessage(g_ctx.hToolbar, TB_ENABLEBUTTON, ID_MENU_REFRESH,
        MAKELONG(!g_ctx.liveCapturing && !mofSchemaWorkerActive, 0));

    SendMessage(g_ctx.hToolbar, TB_ENABLEBUTTON, ID_BTN_EXPORT, MAKELONG(TRUE, 0));
}

/*
 * supGetListViewColumnText
 *
 * Purpose:
 *
 * Retrieves the text of a ListView column header.
 */
BOOL supGetListViewColumnText(
    _In_ HWND ListView,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    LVCOLUMN column;

    if (!ListView ||
        !Buffer ||
        BufferChars == 0)
    {
        return FALSE;
    }

    Buffer[0] = UNICODE_NULL;

    RtlSecureZeroMemory(&column, sizeof(column));

    column.mask = LVCF_TEXT;
    column.pszText = Buffer;
    column.cchTextMax = (INT)BufferChars;
    return ListView_GetColumn(ListView, Column, &column);
}

/*
 * supExportLiveCaptureToCsv
 *
 * Purpose:
 *
 * Exports the currently captured live events to a UTF-8 CSV file.
 */
VOID supExportLiveCaptureToCsv(
    _In_ HANDLE hFile
)
{
    ULONG i, count, physicalIndex;
    PLIVE_EVENT_ROW snapshot, snapshotEntry;
    WCHAR line[2048];
    WCHAR timeBuf[32];
    WCHAR providerEsc[300];
    WCHAR propsEsc[900];

    snapshot = NULL;

    EnterCriticalSection(&g_ctx.liveCs);

    count = g_ctx.liveEventCount;

    if (count != 0) {

        snapshot = (LIVE_EVENT_ROW*)supHeapAlloc(count * sizeof(LIVE_EVENT_ROW));
        if (snapshot) {

            for (i = 0; i < count; i++) {

                if (g_ctx.liveSortedIndices &&
                    g_ctx.liveSortedIndexCount == count)
                {
                    physicalIndex =
                        g_ctx.liveSortedIndices[i];
                }
                else {
                    physicalIndex =
                        (g_ctx.liveEventHead + i) %
                        g_ctx.liveEventCapacity;
                }

                snapshot[i] =
                    g_ctx.liveEvents[physicalIndex];
            }
        }
    }

    LeaveCriticalSection(&g_ctx.liveCs);
    supWriteUtf8String(hFile, TEXT("Time,Provider,EventId,Level,Keyword,Properties\r\n"));

    if (!snapshot)
        return;

    for (i = 0; i < count; i++) {

        snapshotEntry = &snapshot[i];

        StringCchPrintf(timeBuf,
            ARRAYSIZE(timeBuf),
            TEXT("%02u:%02u:%02u.%03u"),
            snapshotEntry->localTime.wHour,
            snapshotEntry->localTime.wMinute,
            snapshotEntry->localTime.wSecond,
            snapshotEntry->localTime.wMilliseconds);

        supEscapeCsvField(snapshotEntry->providerLabel, providerEsc, ARRAYSIZE(providerEsc));
        supEscapeCsvField(snapshotEntry->properties, propsEsc, ARRAYSIZE(propsEsc));

        StringCchPrintf(line,
            ARRAYSIZE(line),
            TEXT("%s,%s,%u,%u,0x%016llX,%s\r\n"),
            timeBuf,
            providerEsc,
            snapshotEntry->eventId,
            snapshotEntry->level,
            snapshotEntry->keyword,
            propsEsc);

        supWriteUtf8String(hFile, line);
    }

    supHeapFree(snapshot);
}

/*
 * supExportListViewToCsv
 *
 * Purpose:
 *
 * Exports the contents of the specified ListView to a CSV file.
 * Column names are written as the first row, followed by the currently
 * displayed ListView rows. Virtual ListView cell values are obtained
 * through the application's virtual row formatter.
 *
 */
VOID supExportListViewToCsv(
    _In_ HANDLE hFile
)
{
    INT row, column;
    INT rowCount, columnCount;
    HWND headerControl;
    PWSTR columnText, escapedText, line;

    headerControl = ListView_GetHeader(g_ctx.hList);
    if (!headerControl)
        return;

    columnCount = Header_GetItemCount(headerControl);
    rowCount = ListView_GetItemCount(g_ctx.hList);

    columnText = (PWSTR)supHeapAlloc(2048 * sizeof(WCHAR));
    escapedText = (PWSTR)supHeapAlloc(4096 * sizeof(WCHAR));
    line = (PWSTR)supHeapAlloc(8192 * sizeof(WCHAR));

    if (!columnText ||
        !escapedText ||
        !line)
    {
        if (columnText)
            supHeapFree(columnText);

        if (escapedText)
            supHeapFree(escapedText);

        if (line)
            supHeapFree(line);

        return;
    }

    //
    // Export column names.
    //
    for (column = 0; column < columnCount; column++) {

        columnText[0] = UNICODE_NULL;

        if (supGetListViewColumnText(g_ctx.hList, column, columnText, 2048)) {
            supEscapeCsvField(columnText, escapedText, 4096);
            supWriteUtf8String(hFile, escapedText);
        }

        if (column + 1 < columnCount)
            supWriteUtf8String(hFile, TEXT(","));
    }

    supWriteUtf8String(hFile, TEXT("\r\n"));

    //
    // Export rows.
    //
    for (row = 0; row < rowCount; row++) {

        line[0] = UNICODE_NULL;

        for (column = 0; column < columnCount; column++) {

            if (column != 0) {
                StringCchCat(line, 8192, TEXT(","));
            }

            supGetVirtualRowColumnText(row, column, columnText, 2048);
            supEscapeCsvField(columnText, escapedText, 4096);
            StringCchCat(line, 8192, escapedText);
        }

        StringCchCat(line, 8192, TEXT("\r\n"));
        supWriteUtf8String(hFile, line);
    }

    supHeapFree(columnText);
    supHeapFree(escapedText);
    supHeapFree(line);
}

/*
 * supExportCurrentViewToCsv
 *
 * Purpose:
 *
 * Exports the currently displayed ListView contents to a
 * UTF-8 CSV file.
 */
VOID supExportCurrentViewToCsv(
    VOID
)
{
    DWORD bytesWritten;
    HANDLE hFile;
    WCHAR path[MAX_PATH];
    OPENFILENAME ofn;

    static const BYTE Utf8Bom[] = {
        0xEF,
        0xBB,
        0xBF
    };

    RtlSecureZeroMemory(path, sizeof(path));
    RtlSecureZeroMemory(&ofn, sizeof(ofn));
    StringCchCopy(path, ARRAYSIZE(path), TEXT("export.csv"));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_ctx.hMainWnd;
    ofn.lpstrFilter = TEXT("CSV Files (*.csv)\0*.csv\0")
        TEXT("All Files\0*.*\0");
    ofn.lpstrFile = path;
    ofn.nMaxFile = ARRAYSIZE(path);
    ofn.lpstrDefExt = TEXT("csv");
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileName(&ofn))
        return;

    hFile = CreateFile(path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {

        MessageBox(g_ctx.hMainWnd,
            TEXT("Failed to open the file for writing."),
            TEXT("Export failed"),
            MB_OK | MB_ICONERROR);

        return;
    }

    WriteFile(hFile, Utf8Bom, sizeof(Utf8Bom), &bytesWritten, NULL);

    if (g_ctx.listMode == LIST_MODE_LIVE)
        supExportLiveCaptureToCsv(hFile);
    else
        supExportListViewToCsv(hFile);

    CloseHandle(hFile);

    MessageBox(g_ctx.hMainWnd,
        TEXT("Export complete."),
        TEXT("Export"),
        MB_OK | MB_ICONINFORMATION);
}

static __forceinline BOOL supxIsWordChar(
    _In_ WCHAR Ch
)
{
    return (Ch >= L'A' && Ch <= L'Z') ||
        (Ch >= L'a' && Ch <= L'z') ||
        (Ch >= L'0' && Ch <= L'9') ||
        (Ch == L'_');
}

/*
 * supMatchLiveEvent
 *
 * Purpose:
 *
 * Determines whether a live event matches the specified search criteria.
 *
 */
BOOL supMatchLiveEvent(
    _In_ PLIVE_EVENT_ROW Row,
    _In_ PETWX_SEARCH_CONTEXT Search
)
{
    BOOL leftOk, rightOk;
    PCWSTR match;
    PCWSTR text;
    SIZE_T searchLength;

    if (!Row || !Search)
        return FALSE;

    if (Search->MatchProvider &&
        !IsEqualGUID(Row->providerGuid, Search->ProviderGuid))
    {
        return FALSE;
    }

    if (Search->MatchEventId &&
        Row->eventId != Search->EventId)
    {
        return FALSE;
    }

    if (Search->MatchLevel &&
        Row->level != Search->Level)
    {
        return FALSE;
    }

    if (Search->MatchKeyword) {

        switch (Search->KeywordMatch) {

        case EtwxKeywordExact:

            if (Row->keyword != Search->KeyWord)
                return FALSE;

            break;

        case EtwxKeywordAnyBits:

            if ((Row->keyword & Search->KeyWord) == 0)
                return FALSE;

            break;

        case EtwxKeywordAllBits:

            if ((Row->keyword & Search->KeyWord) !=
                Search->KeyWord)
            {
                return FALSE;
            }

            break;

        default:
            return FALSE;
        }
    }

    if (!Search->MatchProperties)
        return TRUE;

    if (FAILED(StringCchLength(Search->Properties,
        ARRAYSIZE(Search->Properties),
        &searchLength)) ||
        searchLength == 0)
    {
        return FALSE;
    }

    if (!Search->MatchWholeWord) {

        if (Search->MatchCase) {
            return supStrStr(Row->properties, Search->Properties) != NULL;
        }

        return supStrStrI(Row->properties, Search->Properties) != NULL;
    }

    text = Row->properties;

    while (*text != UNICODE_NULL) {

        match = Search->MatchCase ?
            supStrStr(text, Search->Properties) :
            supStrStrI(text, Search->Properties);

        if (!match)
            break;

        leftOk = (match == Row->properties) ||
            !supxIsWordChar(match[-1]);

        rightOk = (match[searchLength] == UNICODE_NULL) ||
            !supxIsWordChar(match[searchLength]);

        if (leftOk && rightOk)
            return TRUE;

        //
        // Continue after this occurrence.
        //
        text = match + 1;
    }

    return FALSE;
}

/*
 * supFindNextLiveEvent
 *
 * Purpose:
 *
 * Searches the currently captured live events for the next event matching
 * the specified search criteria. The search follows the current ListView
 * display order when sorted indices are available.
 *
 */
BOOL supFindNextLiveEvent(
    _In_ PETWX_SEARCH_CONTEXT Search,
    _Out_ PULONG FoundIndex
)
{
    ULONG count;
    ULONG i;
    ULONG index;
    ULONG physicalIndex;
    LIVE_EVENT_ROW row;

    if (!Search || !FoundIndex)
        return FALSE;

    *FoundIndex = 0;

    EnterCriticalSection(&g_ctx.liveCs);

    count = g_ctx.liveEventCount;
    if (count == 0) {
        LeaveCriticalSection(&g_ctx.liveCs);
        return FALSE;
    }

    if (g_ctx.liveEventSearchIndex >= count)
        g_ctx.liveEventSearchIndex = 0;

    for (i = 0; i < count; i++) {

        index = (g_ctx.liveEventSearchIndex + i) % count;
        if (g_ctx.liveSortedIndices &&
            g_ctx.liveSortedIndexCount == count)
        {
            physicalIndex = g_ctx.liveSortedIndices[index];
            if (physicalIndex >= g_ctx.liveEventCapacity)
                continue;
        }
        else {
            if (g_ctx.liveEventCapacity == 0)
                continue;

            physicalIndex = (g_ctx.liveEventHead + index) % g_ctx.liveEventCapacity;
        }

        row = g_ctx.liveEvents[physicalIndex];
        if (supMatchLiveEvent(&row, Search)) {
            *FoundIndex = index;

            g_ctx.liveEventSearchIndex = (index + 1 < count) ? index + 1 : 0;
            LeaveCriticalSection(&g_ctx.liveCs);
            return TRUE;
        }
    }

    LeaveCriticalSection(&g_ctx.liveCs);

    return FALSE;
}

/*
 * supParseUlong
 *
 * Purpose:
 *
 * Parses a null-terminated string containing a decimal or hexadecimal
 * unsigned integer and stores the result in the specified ULONG value.
 * The hexadecimal form is recognized by a "0x" or "0X" prefix.
 *
 */
BOOL supParseUlong(
    _In_ LPCWSTR String,
    _Out_ PULONG Value
)
{
    WCHAR ch;
    ULONG result, digit, base;

    if (!String || !Value)
        return FALSE;

    *Value = 0;

    if (*String == UNICODE_NULL)
        return FALSE;

    result = 0;
    base = 10;

    if (String[0] == L'0' &&
        (String[1] == L'x' ||
            String[1] == L'X'))
    {
        base = 16;
        String += 2;

        if (*String == UNICODE_NULL)
            return FALSE;
    }

    while (*String != UNICODE_NULL) {

        ch = *String++;

        if (ch >= L'0' && ch <= L'9') {
            digit = ch - L'0';
        }
        else if (base == 16 &&
            ch >= L'a' && ch <= L'f')
        {
            digit = ch - L'a' + 10;
        }
        else if (base == 16 &&
            ch >= L'A' && ch <= L'F')
        {
            digit = ch - L'A' + 10;
        }
        else {
            return FALSE;
        }

        if (digit >= base)
            return FALSE;

        if (result > (ULONG_MAX - digit) / base)
            return FALSE;

        result = result * base + digit;
    }

    *Value = result;

    return TRUE;
}

/*
 * supParseUlonglong
 *
 * Purpose:
 *
 * Parses a null-terminated string containing a decimal or hexadecimal
 * unsigned 64-bit integer and stores the result in the specified
 * ULONGLONG value. The hexadecimal form is recognized by a "0x" or
 * "0X" prefix.
 *
 */
BOOL supParseUlonglong(
    _In_ LPCWSTR String,
    _Out_ PULONGLONG Value
)
{
    WCHAR ch;
    ULONGLONG result, digit, base;

    if (!String || !Value)
        return FALSE;

    *Value = 0;

    if (*String == UNICODE_NULL)
        return FALSE;

    result = 0;
    base = 10;

    if (String[0] == L'0' &&
        (String[1] == L'x' ||
            String[1] == L'X'))
    {
        base = 16;
        String += 2;

        if (*String == UNICODE_NULL)
            return FALSE;
    }

    while (*String != UNICODE_NULL) {

        ch = *String++;

        if (ch >= L'0' && ch <= L'9') {
            digit = ch - L'0';
        }
        else if (base == 16 &&
            ch >= L'a' && ch <= L'f')
        {
            digit = ch - L'a' + 10;
        }
        else if (base == 16 &&
            ch >= L'A' && ch <= L'F')
        {
            digit = ch - L'A' + 10;
        }
        else {
            return FALSE;
        }

        if (digit >= base)
            return FALSE;

        if (result > (MAXULONGLONG - digit) / base)
            return FALSE;

        result = result * base + digit;
    }

    *Value = result;

    return TRUE;
}

/*
* supGetDisplayedLiveEvent
*
* Purpose:
*
* Retrieves the live event corresponding to a displayed ListView row.
* Used for highlighting events for a live capture view.
*
*/
BOOL supGetDisplayedLiveEvent(
    _In_ ULONG DisplayIndex,
    _Out_ LIVE_EVENT_ROW * EventRow
)
{
    ULONG eventIndex;

    if (!EventRow)
        return FALSE;

    RtlSecureZeroMemory(EventRow, sizeof(LIVE_EVENT_ROW));

    EnterCriticalSection(&g_ctx.liveCs);

    if (DisplayIndex >= g_ctx.liveDisplayedCount) {
        LeaveCriticalSection(&g_ctx.liveCs);
        return FALSE;
    }

    //
    // Normally the displayed row maps through liveSortedIndices.
    //
    if (g_ctx.liveSortedIndices &&
        DisplayIndex < g_ctx.liveSortedIndexCount)
    {
        eventIndex = g_ctx.liveSortedIndices[DisplayIndex];
    }
    else {
        eventIndex = DisplayIndex;
    }

    if (eventIndex >= g_ctx.liveEventCount) {
        LeaveCriticalSection(&g_ctx.liveCs);
        return FALSE;
    }

    *EventRow = g_ctx.liveEvents[eventIndex];

    LeaveCriticalSection(&g_ctx.liveCs);

    return TRUE;
}

/*
* supRunHighlightSimulation
*
* Purpose:
*
* Debug routine used during highlight tests.
*
*/
VOID supRunHighlightSimulation(
    VOID
)
{
    static const GUID ActivityA =
    {
        0xaaaaaaaa, 0xaaaa, 0xaaaa,
        { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa }
    };

    static const GUID ActivityB =
    {
        0xbbbbbbbb, 0xbbbb, 0xbbbb,
        {  0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb }
    };

    static const GUID ActivityC =
    {
        0xcccccccc, 0xcccc, 0xcccc,
        { 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc }
    };

    LIVE_EVENT_ROW* events = NULL;
    ULONG* sortedIndices = NULL;

    //
    // Do not modify the live-event buffers while an actual
    // capture is running.
    //
    if (InterlockedCompareExchange(&g_ctx.liveCapturing, 0, 0) != 0) {
        MessageBox(g_ctx.hMainWnd,
            TEXT("Stop the live capture before running the highlight simulation."),
            TEXT("Highlight Simulation"),
            MB_OK | MB_ICONINFORMATION);

        return;
    }

    //
    // Allocate the simulation buffers first. This way, if an
    // allocation fails, the existing live-event state is left
    // untouched.
    //
    events = (LIVE_EVENT_ROW*)supHeapAlloc(sizeof(LIVE_EVENT_ROW) * 5);
    sortedIndices = (ULONG*)supHeapAlloc(sizeof(ULONG) * 5);
    if (!events || !sortedIndices) {

        if (events)
            supHeapFree(events);

        if (sortedIndices)
            supHeapFree(sortedIndices);

        MessageBox(g_ctx.hMainWnd,
            TEXT("Unable to allocate highlight simulation data."),
            TEXT("Highlight Simulation"),
            MB_OK | MB_ICONERROR);

        return;
    }

    //
    // Five deterministic events.
    //
    //
    // Event 0 -> Activity A
    // Event 1 -> Activity B
    // Event 2 -> Activity A
    // Event 3 -> Activity C
    // Event 4 -> Activity A
    //
    events[0].activityId = ActivityA;
    events[1].activityId = ActivityB;
    events[2].activityId = ActivityA;
    events[3].activityId = ActivityC;
    events[4].activityId = ActivityA;

    //
    // Give every event a different level so that the existing
    // "Colorize by Level" functionality can be tested at the
    // same time as highlighting.
    //
    events[0].level = TRACE_LEVEL_INFORMATION;
    events[1].level = TRACE_LEVEL_WARNING;
    events[2].level = TRACE_LEVEL_ERROR;
    events[3].level = TRACE_LEVEL_VERBOSE;
    events[4].level = TRACE_LEVEL_CRITICAL;

    //
    // Initial display order:
    //
    // Display 0 -> Event 3 -> Activity C
    // Display 1 -> Event 1 -> Activity B
    // Display 2 -> Event 4 -> Activity A
    // Display 3 -> Event 0 -> Activity A
    // Display 4 -> Event 2 -> Activity A
    //
    sortedIndices[0] = 3;
    sortedIndices[1] = 1;
    sortedIndices[2] = 4;
    sortedIndices[3] = 0;
    sortedIndices[4] = 2;

    //
    // Replace the existing live-event buffers.
    //
    EnterCriticalSection(&g_ctx.liveCs);

    if (g_ctx.liveEvents) {
        supHeapFree(g_ctx.liveEvents);
        g_ctx.liveEvents = NULL;
    }

    if (g_ctx.liveSortedIndices) {
        supHeapFree(g_ctx.liveSortedIndices);
        g_ctx.liveSortedIndices = NULL;
    }

    g_ctx.liveEvents = events;
    g_ctx.liveEventCount = 5;
    g_ctx.liveEventCapacity = 5;
    g_ctx.liveSortedIndices = sortedIndices;
    g_ctx.liveSortedIndexCount = 5;
    g_ctx.liveDisplayedCount = 5;

    //
    // Reset the display-generation state because we are
    // replacing the complete virtual-list data set.
    //
    g_ctx.liveEventGeneration++;
    g_ctx.liveDisplayedGeneration = g_ctx.liveEventGeneration;

    g_ctx.liveEventTotalCount = 5;

    LeaveCriticalSection(&g_ctx.liveCs);

    g_ctx.listMode = LIST_MODE_LIVE;
    g_ctx.showingLivePane = TRUE;
    g_ctx.showingSchemaPane = FALSE;
    g_ctx.colorizeEnabled = TRUE;
    g_ctx.highlightActivityId = GUID_NULL;
    g_ctx.highlightActivityIdActive = FALSE;
    g_ctx.ctxListRow = -1;
    ListView_SetItemCountEx(g_ctx.hList, (INT)g_ctx.liveDisplayedCount, 0);

    InvalidateRect(g_ctx.hList, NULL, TRUE);
    UpdateWindow(g_ctx.hList);
    SetFocus(g_ctx.hList);

    MessageBox(g_ctx.hMainWnd,
        TEXT(
            "Highlight simulation loaded.\n\n"
            "Five events were inserted using three ActivityIds:\n\n"
            "  Activity A - events 0, 2, 4\n"
            "  Activity B - event 1\n"
            "  Activity C - event 3\n\n"
            "Initial display order:\n"
            "  C, B, A, A, A\n\n"
            "Right-click an A event and select:\n"
            "  Highlight Related Events\n\n"
            "Then sort the ListView and verify that all A events "
            "remain highlighted."
        ),
        TEXT("Highlight Simulation"),
        MB_OK | MB_ICONINFORMATION);
}

/*
* supInitializeRichEdit
*
* Purpose:
*
* Just load richedit dll.
*
*/
BOOL supInitializeRichEdit(
    VOID
)
{
    HMODULE hRichEdit;

    hRichEdit = LoadLibrary(TEXT("Msftedit.dll"));

    return hRichEdit != NULL;
}

/*
* supWriteMiniDump
*
* Purpose:
*
* Write a minidump containing the current process state and,
* when available, the exception information supplied by the
* unhandled exception filter.
*
*/
BOOL supWriteMiniDump(
    _In_opt_ PEXCEPTION_POINTERS ExceptionInfo
)
{
    BOOL bResult = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    WCHAR dumpPath[MAX_PATH];
    WCHAR tempPath[MAX_PATH];
    WCHAR fileName[64];
    SYSTEMTIME systemTime;
    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    MINIDUMP_TYPE dumpType;

    RtlSecureZeroMemory(&exceptionInfo, sizeof(exceptionInfo));
    RtlSecureZeroMemory(&systemTime, sizeof(systemTime));

    //
    // Get a writable temporary directory.
    //
    if (GetTempPath(ARRAYSIZE(tempPath), tempPath) == 0)
        return FALSE;

    //
    // Create a unique filename containing the timestamp
    // and current process ID.
    //
    GetLocalTime(&systemTime);

    StringCchPrintf(fileName,
        ARRAYSIZE(fileName),
        TEXT("ETWX_%04hu%02hu%02hu_%02hu%02hu%02hu_%lu.dmp"),
        systemTime.wYear,
        systemTime.wMonth,
        systemTime.wDay,
        systemTime.wHour,
        systemTime.wMinute,
        systemTime.wSecond,
        GetCurrentProcessId());

    if (FAILED(StringCchPrintf(dumpPath,
        ARRAYSIZE(dumpPath),
        TEXT("%s%s"),
        tempPath,
        fileName)))
    {
        return FALSE;
    }

    hFile = CreateFile(dumpPath,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    if (ExceptionInfo) {
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = ExceptionInfo;
        exceptionInfo.ClientPointers = FALSE;
    }

    dumpType = (MINIDUMP_TYPE)(MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules |
        MiniDumpWithProcessThreadData |
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpScanMemory);

    bResult = MiniDumpWriteDump(GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        dumpType,
        ExceptionInfo ? &exceptionInfo : NULL,
        NULL,
        NULL);

    CloseHandle(hFile);

    return bResult;
}

/*
* supUnhandledExceptionFilter
*
* Purpose:
*
* Handle unhandled process exceptions by writing a minidump
* containing the exception context and then terminating the
* process.
*
*/
LONG WINAPI supUnhandledExceptionFilter(
    _In_ PEXCEPTION_POINTERS ExceptionInfo
)
{
    supWriteMiniDump(ExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}
