/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       MAIN.CPP
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
*  Program entry point and main window handler.
*
*  Codename: Lemuel
*
*  Version history:
*
*  Jan 2025: created  1.0
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"
#include "capture.h"

APP_CTX g_ctx;
HANDLE g_Heap;

/*
* BuildTree
*
* Purpose:
*
* Builds the main tree view hierarchy by creating the Providers,
* Sessions, and Live Capture root nodes and populating their children
* from the currently loaded provider and session data.
*
*/
VOID BuildTree(
    _In_ HWND hTree
)
{
    ULONG i;
    HTREEITEM hProvRoot, hSessRoot, hLiveRoot, hItem;
    TVINSERTSTRUCT tvis;

    supCreateTreeImageList(hTree);
    RtlSecureZeroMemory(&tvis, sizeof(tvis));

    hProvRoot = supInsertTreeRootItem(hTree,
        TEXT("Providers"),
        MAKE_NODE_PARAM(NODE_KIND_PROVIDER, NODE_ROOT_MARKER),
        ICON_PROVIDERS);

    if (hProvRoot) {
        g_ctx.hProvRoot = hProvRoot;
        supRemoveCheckbox(hTree, hProvRoot);
    }

    hSessRoot = supInsertTreeRootItem(hTree,
        TEXT("Sessions"),
        MAKE_NODE_PARAM(NODE_KIND_SESSION, NODE_ROOT_MARKER),
        ICON_SESSIONS);

    if (hSessRoot)
        supRemoveCheckbox(hTree, hSessRoot);

    hLiveRoot = supInsertTreeRootItem(hTree,
        TEXT("Live Capture"),
        MAKE_NODE_PARAM(NODE_KIND_LIVE, NODE_ROOT_MARKER),
        ICON_LIVECAPTURE);

    if (hLiveRoot)
        supRemoveCheckbox(hTree, hLiveRoot);

    if (hProvRoot) {
        for (i = 0; i < g_ctx.providerCount; i++)
            supInsertProviderTreeItem(hTree, hProvRoot, i);
    }

    if (hSessRoot) {

        tvis.hParent = hSessRoot;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

        for (i = 0; i < g_ctx.sessionCount; i++) {

            tvis.item.pszText = g_ctx.sessions[i].loggerName;
            tvis.item.lParam = MAKE_NODE_PARAM(NODE_KIND_SESSION, i);
            tvis.item.iImage = ICON_SESSION;
            tvis.item.iSelectedImage = ICON_SESSION;
            hItem = TreeView_InsertItem(hTree, &tvis);
            if (hItem)
                supRemoveCheckbox(hTree, hItem);
        }
    }
}

/*
* RefreshAllData
*
* Purpose:
*
* Reloads all provider and session information.
*
*/
VOID RefreshAllData(
    VOID
)
{
    ULONG cb;

    //
    // Prevent refresh or another schema request while the provider array is in use.
    //
    if (InterlockedCompareExchange(&g_ctx.mofSchemaWorkerActive,
        FALSE,
        FALSE) != FALSE)
    {
        return;
    }

    supFreeAllData();

    EtpLoadProviders();
    g_ctx.sessionsLoadLastError = EtpLoadSessions();
    EtpLoadProviderSessionCrossReference();
    EtpBuildSessionProviderIndex();

    cb = (g_ctx.providerCount > 0) ? g_ctx.providerCount : 1;
    g_ctx.providerChecked = (BOOL*)supHeapAlloc(cb * sizeof(BOOL));

    TreeView_DeleteAllItems(g_ctx.hTree);
    BuildTree(g_ctx.hTree);
    g_ctx.selectedProviderIdx = DEFAULT_PROVIDER_IDX;
    g_ctx.showingSchemaPane = FALSE;
    g_ctx.listMode = LIST_MODE_NONE;
    ListView_SetItemCountEx(g_ctx.hList, 0, 0);
    supSetupListViewColumns(g_ctx.hList, TRUE);

    supUpdateStatusBar();
    supUpdateMenuState();
    supUpdateToolbarState();
}

/*
* InitializeAppContext
*
* Purpose:
*
* Initializes the global application context with its default values.
*
*/
VOID InitializeAppContext(
    _In_ HINSTANCE hInstance
)
{
    RtlSecureZeroMemory(&g_ctx, sizeof(g_ctx));

    InitializeCriticalSection(&g_ctx.liveCs);

    g_ctx.isAdmin = supUserIsFullAdmin();
    g_ctx.liveTraceHandle = (TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE;
    g_ctx.selectedProviderIdx = DEFAULT_PROVIDER_IDX;
    g_ctx.selectedLevel = TRACE_LEVEL_VERBOSE;
    g_ctx.treeWidth = 280;
    g_ctx.sortColumn = -1;
    g_ctx.hInstance = hInstance;
    g_ctx.captureEndReason = CAPTURE_END_OK;
    g_ctx.captureEndStatus = ERROR_SUCCESS;
    g_ctx.captureMode = CAPTURE_MODE_LIVE;
    g_ctx.liveSessionName = (WCHAR*)PROGRAM_SESSION;
    g_ctx.liveLevel = TRACE_LEVEL_VERBOSE;
    g_ctx.liveEventLimit = DEFAULT_LIVE_EVENT_LIMIT;
    g_ctx.ctxListRow = -1;
    g_ctx.listMode = LIST_MODE_NONE;
    g_ctx.autoScrollEnabled = TRUE;
    g_ctx.colorizeEnabled = TRUE;
}

/*
* InitializeUIComponents
*
* Purpose:
*
* Creates and initializes the main application controls, menus, fonts,
* tree view, list view, filter edit control, and status bar, then builds
* the initial tree and list layout.
*
*/
LPARAM InitializeUIComponents(
    _In_ HWND hWnd
)
{
    g_ctx.hMainWnd = hWnd;

    supCreateUIFont();
    supCreateMainMenu(hWnd);

    //
    // Toolbar.
    //
    g_ctx.hToolbar = supCreateToolbar(hWnd, &g_ctx.hToolbarImageList);

    //
    // Filter edit.
    //
    g_ctx.hEditFilter = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)ID_EDIT_FILTER,
        g_ctx.hInstance, NULL);

    SendMessage(g_ctx.hEditFilter, EM_SETCUEBANNER, TRUE, (LPARAM)TEXT("Filter providers..."));
    supApplyUIFont(g_ctx.hEditFilter);

    //
    // Tree view.
    //
    g_ctx.hTree = CreateWindowEx(0, WC_TREEVIEWW, TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_CHECKBOXES,
        0, 0, 0, 0, hWnd, (HMENU)ID_TREEVIEW, GetModuleHandle(NULL), NULL);

    TreeView_SetExtendedStyle(g_ctx.hTree, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

    supApplyUIFont(g_ctx.hTree);
    SetWindowTheme(g_ctx.hTree, TEXT("Explorer"), NULL);

    //
    // List view.
    //
    g_ctx.hList = CreateWindowEx(0, WC_LISTVIEWW, TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDATA,
        0, 0, 0, 0, hWnd, (HMENU)ID_LISTVIEW, GetModuleHandle(NULL), NULL);

    ListView_SetExtendedListViewStyle(g_ctx.hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP | LVS_EX_DOUBLEBUFFER);

    supApplyUIFont(g_ctx.hList);
    SetWindowTheme(g_ctx.hList, TEXT("Explorer"), NULL);

    //
    // Status bar.
    //
    g_ctx.hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, TEXT(""),
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hWnd, (HMENU)ID_STATUS_BAR, GetModuleHandle(NULL), NULL);
    supApplyUIFont(g_ctx.hStatusBar);

    //
    // Fill the data.
    //
    BuildTree(g_ctx.hTree);

    //
    // Sets the image list here because it is shared with treeview.
    //
    ListView_SetImageList(g_ctx.hList, g_ctx.hImageList, LVSIL_SMALL);

    supSetupListViewColumns(g_ctx.hList, TRUE);
    supUpdateStatusBar();

    //
    // Set windows layout.
    //
    supLayoutChildren(hWnd);
    return 0;
}

/*
* RefreshProviderTreeFilter
*
* Purpose:
*
* Rebuilds the Providers tree branch according to the text entered
* in the provider filter control, showing only providers whose names
* match the filter.
*
*/
VOID RefreshProviderTreeFilter(
    VOID
)
{
    BOOLEAN hasFilter;
    ULONG i;
    HTREEITEM hChild;
    WCHAR filterText[128];

    if (!g_ctx.hProvRoot)
        return;

    GetWindowText(g_ctx.hEditFilter, filterText, ARRAYSIZE(filterText));
    hasFilter = (filterText[0] != TEXT('\0'));
    SendMessage(g_ctx.hTree, WM_SETREDRAW, FALSE, 0);

    while ((hChild = TreeView_GetChild(g_ctx.hTree, g_ctx.hProvRoot))
        != NULL)
    {
        TreeView_DeleteItem(g_ctx.hTree, hChild);
    }

    for (i = 0; i < g_ctx.providerCount; i++) {

        if (hasFilter && !supStrStrI(g_ctx.providers[i].name, filterText)) {
            continue;
        }

        supInsertProviderTreeItem(g_ctx.hTree, g_ctx.hProvRoot, i);
    }

    SendMessage(g_ctx.hTree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_ctx.hTree, NULL, TRUE);
    TreeView_Expand(g_ctx.hTree, g_ctx.hProvRoot, TVE_EXPAND);
}

/*
* PopulateListForLive
*
* Purpose:
*
* Switches the list view to live capture mode and updates its item
* count to match the events currently stored in the live event buffer.
*
*/
VOID PopulateListForLive(
    VOID
)
{
    ULONG count;
    g_ctx.listMode = LIST_MODE_LIVE;
    supSetupListViewColumnsLive(g_ctx.hList);

    EnterCriticalSection(&g_ctx.liveCs);

    count = g_ctx.liveEventCount;

    LeaveCriticalSection(&g_ctx.liveCs);

    ListView_SetItemCountEx(g_ctx.hList, count, LVSICF_NOSCROLL);
    g_ctx.liveDisplayedCount = count;
}

/*
* PopulateListForSession
*
* Purpose:
*
* Populates the list view with details for the specified ETW session,
* including its logger properties and providers associated with the
* session.
*
*/
VOID PopulateListForSession(
    _In_ ULONG Index
)
{
    ULONG i;
    SESSION_ENTRY* session;
    LPCWSTR lpProviderName;
    WCHAR buffer[128];
    WCHAR szLabel[MAX_PATH];
    SESSION_PROVIDER_ROW* providerRow;
    PROVIDER_ENTRY* provider;

    session = &g_ctx.sessions[Index];
    g_ctx.listMode = LIST_MODE_SESSION;
    g_ctx.detailRowCount = 0;

    supAddRow(TEXT("Logger Name"), session->loggerName);

    if (SUCCEEDED(StringCchPrintf(buffer,
        ARRAYSIZE(buffer),
        TEXT("%llu"),
        session->sessionId)))
    {
        supAddRow(TEXT("Session ID"), buffer);
    }

    if (SUCCEEDED(StringCchPrintf(buffer,
        ARRAYSIZE(buffer),
        TEXT("0x%08lX"),
        session->logFileMode)))
    {
        supAddRow(TEXT("LogFileMode"), buffer);
    }

    if (StringFromGUID2(
        session->guid,
        buffer,
        ARRAYSIZE(buffer)) > 0)
    {
        supAddRow(TEXT("Wnode.GUID"), buffer);
    }

    if (session->logFileName[0] != 0) {
        supAddRow(TEXT("LogFileName"), session->logFileName);
    }

    if (SUCCEEDED(StringCchPrintf(buffer,
        ARRAYSIZE(buffer),
        TEXT("%lu bytes"),
        session->bufferSize)))
    {
        supAddRow(TEXT("BufferSize"), buffer);
    }

    if (session->enabledProviderCount == 0) {

        supAddRow(TEXT("Enabled Providers"),
            TEXT("None matched (or session predates capture)"));
    }
    else {

        supAddRow(TEXT("Enabled Providers"),
            TEXT("See rows below (best-effort match by LoggerId)"));

        for (i = 0; i < session->enabledProviderCount; i++) {

            providerRow = &session->enabledProviders[i];
            provider = &g_ctx.providers[providerRow->providerIdx];

            lpProviderName = provider->name[0] ? (LPWSTR)provider->name : TEXT("(unnamed)");
            szLabel[0] = 0;
            supStrCopy(szLabel, lpProviderName);

            if (!SUCCEEDED(StringCchPrintf(buffer,
                ARRAYSIZE(buffer),
                TEXT("Level=%u  AnyKw=0x%016llX  AllKw=0x%016llX"),
                providerRow->level,
                providerRow->matchAnyKeyword,
                providerRow->matchAllKeyword)))
            {
                continue;
            }

            supAddRow(szLabel, buffer);
        }
    }

    supSetupListViewColumns(g_ctx.hList, FALSE);
    ListView_SetItemCountEx(g_ctx.hList, g_ctx.detailRowCount, LVSICF_NOSCROLL);
}

/*
* PopulateListForProvider
*
* Purpose:
*
* Populates the list view with details for the specified ETW provider.
*
*/
VOID PopulateListForProvider(
    _In_ ULONG Index
)
{
    ULONG i;
    ENABLE_ROW* entry;
    PROVIDER_ENTRY* provider;
    WCHAR guidString[64];
    WCHAR buffer[128];
    WCHAR label[64];

    provider = &g_ctx.providers[Index];
    g_ctx.listMode = LIST_MODE_PROVIDER;
    g_ctx.detailRowCount = 0;

    RtlSecureZeroMemory(guidString, sizeof(guidString));
    if (StringFromGUID2(provider->guid, guidString, ARRAYSIZE(guidString)) == 0)
        guidString[0] = 0;

    supAddRow(TEXT("GUID"), guidString);
    supAddRow(TEXT("Name"), provider->name);
    supAddRow(TEXT("Schema Source"), provider->isManifestProvider ?
        TEXT("Manifest") : TEXT("MOF (legacy)"));

    if (provider->isManifestProvider) {
        if (provider->resourceFileName[0])
            supAddRow(TEXT("Resource"), provider->resourceFileName);
        if (provider->messageFileName[0])
            supAddRow(TEXT("Message"), provider->messageFileName);
    }
    else {
        if (provider->schemaLoaded) {

            StringCchPrintf(buffer,
                ARRAYSIZE(buffer),
                TEXT("%lu"),
                provider->mofSchemaRowCount);

            supAddRow(TEXT("MOF Event Classes"), buffer);

            supAddRow(TEXT("MOF Schema Status"),
                EtpGetMofSchemaLoadStatusName(provider->mofSchemaLoadStatus));

            if (provider->mofSchemaLoadStatusCode != S_OK) {

                StringCchPrintf(buffer,
                    ARRAYSIZE(buffer),
                    TEXT("0x%08lX"),
                    (ULONG)provider->mofSchemaLoadStatusCode);

                supAddRow(TEXT("MOF Status Code"), buffer);
            }
        }
    }

    supAddRow(TEXT("Active Sessions"), provider->enableRowCount ?
        TEXT("Yes - see rows below") : TEXT("None"));

    if (provider->liveHasLastEvent || provider->liveEventCount > 0) {

        StringCchPrintf(buffer, ARRAYSIZE(buffer), TEXT("%llu"), provider->liveEventCount);
        supAddRow(TEXT("Live Event Count"), buffer);

        StringCchPrintf(buffer, ARRAYSIZE(buffer), TEXT("%.1f"), provider->liveEventsPerSecond);
        supAddRow(TEXT("Live Events/sec"), buffer);

        if (provider->liveHasLastEvent) {

            StringCchPrintf(buffer, ARRAYSIZE(buffer), TEXT("%02u:%02u:%02u.%03u"),
                provider->liveLastEventTime.wHour, provider->liveLastEventTime.wMinute,
                provider->liveLastEventTime.wSecond, provider->liveLastEventTime.wMilliseconds);

            supAddRow(TEXT("Last Event Time"), buffer);
        }
    }

    for (i = 0; i < provider->enableRowCount; i++) {
        entry = &provider->enableRows[i];
        if (SUCCEEDED(StringCchPrintf(label,
            ARRAYSIZE(label),
            TEXT("Session #%lu"),
            entry->loggerId)))
        {
            if (SUCCEEDED(StringCchPrintf(buffer,
                ARRAYSIZE(buffer),
                TEXT("Level=%u  AnyKw=0x%016llX  AllKw=0x%016llX"),
                entry->level,
                entry->matchAnyKeyword,
                entry->matchAllKeyword)))
            {
                supAddRow(label, buffer);
            }
        }
    }

    supSetupListViewColumns(g_ctx.hList, TRUE);
    ListView_SetItemCountEx(g_ctx.hList, g_ctx.detailRowCount, LVSICF_NOSCROLL);
}

/*
* PopulateListForSchema
*
* Purpose:
*
* Loads the specified provider's event schema and configures the list
* view to display its available event schema rows.
*
*/
VOID PopulateListForSchema(
    _In_ ULONG ProviderIndex
)
{
    PROVIDER_ENTRY* p;

    EtpLoadProviderSchema(ProviderIndex);
    p = &g_ctx.providers[ProviderIndex];

    g_ctx.listMode = LIST_MODE_SCHEMA;

    supSetupListViewColumnsSchema(g_ctx.hList, p->isManifestProvider);

    if (p->isManifestProvider) {
        ListView_SetItemCountEx(g_ctx.hList, p->schemaRowCount ? p->schemaRowCount : 1, LVSICF_NOSCROLL);
    }
    else {
        ListView_SetItemCountEx(g_ctx.hList, p->mofSchemaRowCount ? p->mofSchemaRowCount : 1, LVSICF_NOSCROLL);
    }
}

/*
* CompareListViewItems
*
* Purpose:
*
* Compares two list view items for sorting.
*
*/
INT CALLBACK CompareListViewItems(
    _In_ LPARAM lParam1,
    _In_ LPARAM lParam2,
    _In_ LPARAM lParamSort
)
{
    INT result;
    ULONGLONG n1 = 0, n2 = 0;
    HWND hList = (HWND)lParamSort;
    WCHAR buf1[512], buf2[512];

    buf1[0] = 0;
    buf2[0] = 0;

    ListView_GetItemText(hList, (INT)lParam1, g_ctx.sortColumn, buf1, ARRAYSIZE(buf1));
    ListView_GetItemText(hList, (INT)lParam2, g_ctx.sortColumn, buf2, ARRAYSIZE(buf2));

    if (supStrToUInt64(buf1, &n1) &&
        supStrToUInt64(buf2, &n2))
    {
        result = (n1 < n2) ? -1 : (n1 > n2) ? 1 : 0;
    }
    else {
        result = supStrCmpI(buf1, buf2);
    }
    return g_ctx.sortAscending ? result : -result;
}

/*
* HandleCommand
*
* Purpose:
*
* Main window WM_COMMAND handler.
*
*/
LRESULT HandleCommand(
    _In_ HWND hWnd,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    BOOL haveGuid = FALSE;
    INT row, column, columnCount;
    ULONG i, idx, error;
    HWND hHeader;
    GUID targetGuid;
    HTREEITEM hItem;
    PWSTR providerData = NULL, providerDataType;
    PROVIDER_ENTRY* provider;
    SESSION_ENTRY* session;
    TREE_CONTEXT_KIND kind;
    TEXT_BUFFER tb;
    WCHAR szBuffer[512];
    TVITEM item;
    LIVE_EVENT_ROW liveRow;

    UNREFERENCED_PARAMETER(lParam);

    switch (GET_WM_COMMAND_ID(wParam, lParam)) {

    case ID_BTN_START:
        StartLiveCapture();
        supUpdateStatusBar();
        supUpdateToolbarState();
        break;

    case ID_BTN_STOP:
        StopLiveCapture();
        supUpdateStatusBar();
        supUpdateToolbarState();
        break;

    case ID_BTN_SCHEMA:
        //
        // Reject another schema request while the worker is active.
        //
        if (InterlockedCompareExchange(&g_ctx.mofSchemaWorkerActive,
            FALSE,
            FALSE) == FALSE &&
            g_ctx.selectedProviderIdx < g_ctx.providerCount)
        {
            g_ctx.showingLivePane = FALSE;
            g_ctx.showingSchemaPane = TRUE;
            PopulateListForSchema(g_ctx.selectedProviderIdx);
        }
        break;

    case ID_MENU_OPEN_ETL:
        OpenEtlForReplay(hWnd);
        break;

    case ID_MENU_SAVE_ETL_TOGGLE:
        supToggleSaveToEtl(hWnd);
        break;

    case ID_BTN_EXPORT:
        supExportCurrentViewToCsv(); ;
        break;

    case ID_MENU_SET_KEYWORD:
        ShowKeywordDialog(hWnd);
        break;

    case ID_MENU_SET_WPP_TMF:
        supSetWppTmfPath(hWnd);
        break;

    case ID_MENU_RUNASADMIN:
        supRunAsAdministrator(hWnd);
        break;

    case ID_MENU_REFRESH:
        RefreshAllData();
        break;

    case ID_CTX_EVENT_DETAILS:
        if (g_ctx.ctxListRow >= 0)
            ShowEventDetailsDialog(hWnd, g_ctx.ctxListRow);
        break;

    case ID_CTX_COPY_ACTIVITYID:
        if (g_ctx.ctxListRow >= 0) {

            haveGuid = FALSE;

            if (supCopyLiveEvent((ULONG)g_ctx.ctxListRow,
                &liveRow) &&
                !IsEqualGUID(liveRow.activityId,
                    GUID_NULL))
            {
                haveGuid = (0 != StringFromGUID2(liveRow.activityId,
                    szBuffer,
                    ARRAYSIZE(szBuffer)));
            }

            if (haveGuid)
                supCopyTextToClipboard(hWnd, szBuffer);
        }
        break;

    case ID_CTX_HIGHLIGHT_RELATED:
        if (g_ctx.ctxListRow >= 0) {

            haveGuid = FALSE;

            if (supCopyLiveEvent((ULONG)g_ctx.ctxListRow,
                &liveRow) &&
                !IsEqualGUID(liveRow.activityId,
                    GUID_NULL))
            {
                g_ctx.highlightActivityId = liveRow.activityId;
                haveGuid = TRUE;
            }

            if (haveGuid) {
                g_ctx.highlightActivityIdActive = TRUE;
                InvalidateRect(g_ctx.hList, NULL, FALSE);
            }
        }
        break;

    case ID_CTX_CLEAR_HIGHLIGHT:
        g_ctx.highlightActivityIdActive = FALSE;
        InvalidateRect(g_ctx.hList, NULL, FALSE);
        break;

    case ID_MENU_FIND:
        hItem = TreeView_GetSelection(g_ctx.hTree);
        if (!hItem)
            break;

        RtlSecureZeroMemory(&item, sizeof(item));
        item.mask = TVIF_PARAM;
        item.hItem = hItem;

        if (!TreeView_GetItem(g_ctx.hTree, &item))
            break;

        if (NODE_KIND(item.lParam) != NODE_KIND_LIVE)
            break;

        ShowEventFindDialog(g_ctx.hMainWnd);
        break;

    case ID_MENU_ABOUT:
        ShowAboutDialog(hWnd);
        break;

    case ID_CTX_COPY_ENABLED_PROVIDERS:

        if (!supTreeContextGetSelection(&kind, &idx))
            break;

        if ((kind != TreeContextSession) || (idx >= g_ctx.sessionCount))
            break;

        session = &g_ctx.sessions[idx];

        if (session->enabledProviderCount == 0) {
            supCopyTextToClipboard(hWnd, TEXT("(none)"));
            break;
        }

        if (!supTbInitialize(&tb, 4096))
            break;

        for (i = 0; i < session->enabledProviderCount; i++) {
            provider = &g_ctx.providers[session->enabledProviders[i].providerIdx];
            supTbAppend(&tb, provider->name[0] ? provider->name : TEXT("(unnamed)"));
            supTbAppend(&tb, TEXT("\r\n"));
        }

        supCopyTextToClipboard(hWnd, tb.Buffer);
        supTbDestroy(&tb);
        break;

    case ID_CTX_COPY_ROW:
        hHeader = ListView_GetHeader(g_ctx.hList);
        columnCount = Header_GetItemCount(hHeader);

        if (!supTbInitialize(&tb, PAGE_SIZE))
            break;

        row = -1;

        while ((row = ListView_GetNextItem(g_ctx.hList,
            row,
            LVNI_SELECTED)) != -1)
        {
            for (column = 0; column < columnCount; column++) {

                szBuffer[0] = 0;
                supGetVirtualRowColumnText(row, column, szBuffer, ARRAYSIZE(szBuffer));

                supTbAppend(&tb, szBuffer);

                if (column + 1 < columnCount)
                    supTbAppend(&tb, TEXT("\t"));
            }

            supTbAppend(&tb, TEXT("\r\n"));
        }

        supCopyTextToClipboard(hWnd, tb.Buffer);
        supTbDestroy(&tb);
        break;

    case ID_CTX_COPY_GUID:
        if (!supTreeContextGetSelection(&kind, &idx))
            break;

        if ((kind != TreeContextProvider) || (idx >= g_ctx.providerCount))
            break;

        RtlSecureZeroMemory(&szBuffer, sizeof(szBuffer));
        if (0 == StringFromGUID2(g_ctx.providers[idx].guid, szBuffer, ARRAYSIZE(szBuffer)))
            supStrCopy(szBuffer, TEXT("none"));
        supCopyTextToClipboard(hWnd, szBuffer);
        break;

    case ID_CTX_COPY_PROVIDER_NAME:
        if (!supTreeContextGetSelection(&kind, &idx))
            break;

        if ((kind != TreeContextProvider) || (idx >= g_ctx.providerCount))
            break;

        supCopyTextToClipboard(hWnd, g_ctx.providers[idx].name);
        break;

    case ID_CTX_COPY_SESSION_NAME:
        if (!supTreeContextGetSelection(&kind, &idx))
            break;

        if ((kind != TreeContextSession) || (idx >= g_ctx.sessionCount))
            break;

        supCopyTextToClipboard(hWnd, g_ctx.sessions[idx].loggerName);
        break;

    case ID_CTX_COPY_SESSION_ID:
        if (!supTreeContextGetSelection(&kind, &idx))
            break;

        if ((kind != TreeContextSession) || (idx >= g_ctx.sessionCount))
            break;

        StringCchPrintf(szBuffer, ARRAYSIZE(szBuffer), TEXT("%llu"), g_ctx.sessions[idx].sessionId);
        supCopyTextToClipboard(hWnd, szBuffer);
        break;

    case ID_CTX_SCHEMA_BACK_TO_PROVIDER:

        if (g_ctx.listMode == LIST_MODE_SCHEMA &&
            g_ctx.selectedProviderIdx < g_ctx.providerCount)
        {
            g_ctx.showingSchemaPane = FALSE;
            PopulateListForProvider(g_ctx.selectedProviderIdx);
        }

        break;

    case ID_CTX_VIEW_METADATA:
        if (g_ctx.listMode == LIST_MODE_PROVIDER &&
            g_ctx.selectedProviderIdx < g_ctx.providerCount)
        {
            g_ctx.showingSchemaPane = FALSE;
            provider = &g_ctx.providers[g_ctx.selectedProviderIdx];

            if (provider->isManifestProvider) {
                providerDataType = (PWSTR)TEXT("XML schema");
                error = EtmBuildManifestProviderSchema(&provider->guid, provider->name, &providerData);
            }
            else {
                providerDataType = (PWSTR)TEXT("Mof reconstruction");
                error = EtpGetMofProviderText(provider->guid, provider->name, &providerData);
            }

            if (error == ERROR_SUCCESS) {
                ShowMetadataDialog(hWnd, provider->name, providerData);
                supHeapFree(providerData);
            }
            else {

                StringCchPrintf(szBuffer,
                    RTL_NUMBER_OF(szBuffer),
                    TEXT("Unable to retrieve the %s for provider \"%s\".\r\n\r\n"
                        "Error: %lu"),
                    providerDataType,
                    provider->name,
                    error);

                MessageBox(hWnd, szBuffer, TEXT("Error"), MB_OK | MB_ICONWARNING);
            }
        }
        break;

    case ID_MENU_LEVEL_BASE + TRACE_LEVEL_CRITICAL:
    case ID_MENU_LEVEL_BASE + TRACE_LEVEL_ERROR:
    case ID_MENU_LEVEL_BASE + TRACE_LEVEL_WARNING:
    case ID_MENU_LEVEL_BASE + TRACE_LEVEL_INFORMATION:
    case ID_MENU_LEVEL_BASE + TRACE_LEVEL_VERBOSE:
        g_ctx.selectedLevel = (UCHAR)(LOWORD(wParam) - ID_MENU_LEVEL_BASE);
        supUpdateMenuState();
        supUpdateToolbarState();
        break;

    case ID_MENU_EVENTID_FILTER:
        if (ShowEventIdFilterDialog(hWnd)) {
            supParseEventIdFilterText();
        }
        break;

    case ID_MENU_PAUSE_DISPLAY:
        g_ctx.displayPaused = (~g_ctx.displayPaused) & 1;
        supUpdateMenuState();

        if (!g_ctx.displayPaused &&
            g_ctx.showingLivePane)
        {
            supRefreshLivePaneIncremental(g_ctx.autoScrollEnabled);
        }
        break;

    case ID_MENU_AUTOSCROLL:
        g_ctx.autoScrollEnabled = (~g_ctx.autoScrollEnabled) & 1;
        supUpdateMenuState();
        supUpdateStatusBar();
        break;

    case ID_MENU_COLORIZE:
        g_ctx.colorizeEnabled = (~g_ctx.colorizeEnabled) & 1;
        supUpdateMenuState();

        if (g_ctx.listMode == LIST_MODE_LIVE)
            InvalidateRect(g_ctx.hList, NULL, FALSE);

        break;

    case ID_MENU_SET_LIVE_EVENT_LIMIT:
        if (ShowLiveEventLimitDialog(hWnd))
            supUpdateStatusBar();
        break;

    case ID_MENU_UNCHECK_ALL:
        supUncheckAllProviders();
        break;

    case ID_CTX_GOTO_PROVIDER:
        if (g_ctx.ctxListRow >= 0) {

            haveGuid = FALSE;
            if (supCopyLiveEvent((ULONG)g_ctx.ctxListRow, &liveRow)) {
                targetGuid = liveRow.providerGuid;
                haveGuid = TRUE;
            }

            if (haveGuid)
                supGoToProvider(&targetGuid);
        }
        break;

    case ID_MENU_VIEW_SECURITY:

        if (!supTreeContextGetSelection(&kind, &idx))
            break;

        if (kind == TreeContextProvider) {

            if (idx >= g_ctx.providerCount)
                break;

            SecurityRunDialogForEtwObject(&g_ctx.providers[idx].guid, EtwxSecurityProvider);
        }
        else if (kind == TreeContextSession) {

            if (idx >= g_ctx.sessionCount)
                break;

            if (g_ctx.sessions[idx].type != EtwxSessionNormal)
                break;

            SecurityRunDialogForEtwObject(&g_ctx.sessions[idx].guid, EtwxSecuritySession);
        }
        break;

    case ID_MENU_SYSTEM_INFORMATION:
        ShowSystemInformationDialog(hWnd);
        break;

    case ID_MENU_EXIT:
        DestroyWindow(hWnd);
        break;
    }

    return 0;
}

/*
* HandleContextMenu
*
* Purpose:
*
* Main window WM_CONTEXTMENU handler.
*
*/
LRESULT HandleContextMenu(
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    INT iRow;
    HWND hCtrl;
    POINT pt, client;
    RECT rc;
    HTREEITEM hHit, hSel;
    TVHITTESTINFO tvHit;
    LVHITTESTINFO lvHit;

    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);

    hCtrl = (HWND)wParam;
    if (hCtrl == g_ctx.hTree) {
        if (pt.x == -1 && pt.y == -1) {

            hSel = TreeView_GetSelection(g_ctx.hTree);
            if (!hSel)
                return 0;

            TreeView_GetItemRect(g_ctx.hTree, hSel, &rc, TRUE);

            pt.x = rc.left;
            pt.y = rc.top;
            ClientToScreen(g_ctx.hTree, &pt);
            supShowTreeContextMenu(hSel, pt);
        }
        else {
            client = pt;

            ScreenToClient(g_ctx.hTree, &client);
            RtlSecureZeroMemory(&tvHit, sizeof(tvHit));

            tvHit.pt = client;
            hHit = TreeView_HitTest(g_ctx.hTree, &tvHit);
            if (hHit)
                supShowTreeContextMenu(hHit, pt);
        }
    }
    else  if (hCtrl == g_ctx.hList) {

        if (pt.x == -1 && pt.y == -1) {

            iRow = ListView_GetNextItem(g_ctx.hList, -1, LVNI_SELECTED);
            if (iRow >= 0) {
                ListView_GetItemRect(g_ctx.hList, iRow, &rc, LVIR_BOUNDS);
                pt.x = rc.left;
                pt.y = rc.top;
                ClientToScreen(g_ctx.hList, &pt);
            }
            else {
                GetClientRect(g_ctx.hList, &rc);
                pt.x = rc.left;
                pt.y = rc.top;
                ClientToScreen(g_ctx.hList, &pt);
            }

            supShowListContextMenu(iRow, pt);
        }
        else {

            client = pt;
            ScreenToClient(g_ctx.hList, &client);
            RtlSecureZeroMemory(&lvHit, sizeof(lvHit));
            lvHit.pt = client;
            iRow = ListView_HitTest(g_ctx.hList, &lvHit);

            //
            // iRow == -1 means that the user clicked empty space.
            // supShowListContextMenu handles that case.
            //
            supShowListContextMenu(iRow, pt);
        }
    }

    return 0;
}

static __forceinline VOID UpdateMainViewState(
    _In_ ULONG NodeKind,
    _In_ ULONG NodeIndex
)
{
    BOOL isLive;
    BOOL isProvider;

    isLive = (NodeKind == NODE_KIND_LIVE);
    isProvider =
        NodeKind == NODE_KIND_PROVIDER &&
        NodeIndex != NODE_ROOT_MARKER;

    EnableMenuItem(g_ctx.hMenuFind, ID_MENU_FIND, MF_BYCOMMAND | (isLive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(g_ctx.hMenuSchema, ID_BTN_SCHEMA, MF_BYCOMMAND | (isProvider ? MF_ENABLED : MF_GRAYED));
    SendMessage(g_ctx.hToolbar, TB_ENABLEBUTTON, ID_MENU_FIND, MAKELONG(isLive, 0));
    SendMessage(g_ctx.hToolbar, TB_ENABLEBUTTON, ID_BTN_SCHEMA, MAKELONG(isProvider, 0));
}

/*
* HandleNotify
*
* Purpose:
*
* Main window WM_NOTIFY handler.
*
*/
LRESULT HandleNotify(
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    BOOL valid = FALSE;
    UCHAR level;
    ULONG kind, idx, param;
    LPNMHDR nmhdr;
    LPNMTREEVIEW nmtv;
    NMTVITEMCHANGE* pic;
    LPNMLISTVIEW pnv;
    NMLVDISPINFOW* pdi;
    PROVIDER_ENTRY* p;
    LPNMLVCUSTOMDRAW pcd;
    LPNMTTDISPINFO pttdi;
    LIVE_EVENT_ROW liveRow;

    UNREFERENCED_PARAMETER(wParam);

    nmhdr = (LPNMHDR)lParam;

    if (nmhdr->code == TTN_GETDISPINFO) {

        pttdi = (LPNMTTDISPINFO)lParam;
        pttdi->lpszText = NULL;

        switch (nmhdr->idFrom) {

        case ID_BTN_START:
            pttdi->lpszText = (LPWSTR)TEXT("Start live capture");
            break;

        case ID_BTN_STOP:
            pttdi->lpszText = (LPWSTR)TEXT("Stop live capture");
            break;

        case ID_MENU_FIND:
            pttdi->lpszText = (LPWSTR)TEXT("Find captured event");
            break;

        case ID_BTN_SCHEMA:
            pttdi->lpszText = (LPWSTR)TEXT("Browse provider schema");
            break;

        case ID_MENU_REFRESH:
            pttdi->lpszText = (LPWSTR)TEXT("Refresh data");
            break;

        case ID_BTN_EXPORT:
            pttdi->lpszText = (LPWSTR)TEXT("Export list");
            break;
        }

        return 0;
    }

    if (nmhdr->idFrom == ID_TREEVIEW) {

        switch (nmhdr->code) {
        case TVN_SELCHANGED:

            nmtv = (LPNMTREEVIEW)lParam;
            param = (ULONG)nmtv->itemNew.lParam;
            kind = NODE_KIND(param);
            idx = NODE_INDEX(param);

            g_ctx.showingLivePane = FALSE;
            g_ctx.showingSchemaPane = FALSE;

            UpdateMainViewState(kind, idx);

            if (kind == NODE_KIND_LIVE) {
                g_ctx.showingLivePane = TRUE;
                PopulateListForLive();
            }
            else if (idx == NODE_ROOT_MARKER) {
                // "Providers" / "Sessions" root selected - nothing to show
                g_ctx.listMode = LIST_MODE_NONE;
                ListView_SetItemCountEx(g_ctx.hList, 0, 0);
            }
            else if (kind == NODE_KIND_PROVIDER &&
                idx < g_ctx.providerCount)
            {
                PopulateListForProvider(idx);
                g_ctx.selectedProviderIdx = idx;
            }
            else if (kind == NODE_KIND_SESSION &&
                idx < g_ctx.sessionCount)
            {
                PopulateListForSession(idx);
            }
            break;

        case TVN_ITEMCHANGED:
            pic = (NMTVITEMCHANGE*)lParam;
            if (NODE_KIND(pic->lParam) == NODE_KIND_PROVIDER) {
                idx = NODE_INDEX(pic->lParam);
                if (idx < g_ctx.providerCount && g_ctx.providerChecked) {
                    g_ctx.providerChecked[idx] = (TreeView_GetCheckState(g_ctx.hTree, pic->hItem) == 1);
                }
            }
            return 0;
        }
        return 0;
    }

    if (nmhdr->idFrom == ID_LISTVIEW) {

        switch (nmhdr->code) {

        case LVN_GETDISPINFO:

            pdi = (NMLVDISPINFO*)lParam;
            if (pdi->item.mask & LVIF_TEXT) {
                supGetVirtualRowColumnText(pdi->item.iItem,
                    pdi->item.iSubItem,
                    pdi->item.pszText,
                    pdi->item.cchTextMax);
            }
            if (pdi->item.mask & LVIF_IMAGE) {
                pdi->item.iImage = supGetVirtualRowIcon(pdi->item.iItem);
            }

            return 0;

        case NM_CUSTOMDRAW:

            pcd = (LPNMLVCUSTOMDRAW)lParam;

            if (!g_ctx.colorizeEnabled ||
                g_ctx.listMode != LIST_MODE_LIVE)
            {
                return CDRF_DODEFAULT;
            }

            if (pcd->nmcd.dwDrawStage == CDDS_PREPAINT)
                return CDRF_NOTIFYITEMDRAW;

            if (pcd->nmcd.dwDrawStage != CDDS_ITEMPREPAINT ||
                (pcd->nmcd.uItemState & CDIS_SELECTED))
            {
                return CDRF_DODEFAULT;
            }

            valid = FALSE;
            level = TRACE_LEVEL_NONE;

            EnterCriticalSection(&g_ctx.liveCs);

            if (pcd->nmcd.dwItemSpec < g_ctx.liveEventCount) {
                liveRow = g_ctx.liveEvents[pcd->nmcd.dwItemSpec];
                level = liveRow.level;
                valid = TRUE;
            }

            LeaveCriticalSection(&g_ctx.liveCs);

            if (!valid)
                return CDRF_DODEFAULT;

            switch (level) {

            case TRACE_LEVEL_CRITICAL:
            case TRACE_LEVEL_ERROR:
                pcd->clrText = RGB(192, 0, 0);
                break;

            case TRACE_LEVEL_WARNING:
                pcd->clrText = RGB(176, 96, 0);
                break;

            case TRACE_LEVEL_INFORMATION:
                pcd->clrText = RGB(0, 80, 160);
                break;

            case TRACE_LEVEL_VERBOSE:
                pcd->clrText = RGB(96, 96, 96);
                break;
            }

            return CDRF_NEWFONT;

        case NM_DBLCLK:

            pnv = (LPNMLISTVIEW)lParam;

            if (g_ctx.listMode == LIST_MODE_LIVE &&
                pnv->iItem >= 0)
            {
                ShowEventDetailsDialog(g_ctx.hMainWnd, pnv->iItem);
            }

            return 0;

        case LVN_COLUMNCLICK:

            pnv = (LPNMLISTVIEW)lParam;
            if (pnv->iSubItem == g_ctx.sortColumn)
                g_ctx.sortAscending = (~g_ctx.sortAscending) & 1;
            else {
                g_ctx.sortColumn = pnv->iSubItem;
                g_ctx.sortAscending = TRUE;
            }

            switch (g_ctx.listMode) {
            case LIST_MODE_PROVIDER:
            case LIST_MODE_SESSION:
                if (g_ctx.detailRowCount > 0) {

                    supSort(g_ctx.detailRows,
                        g_ctx.detailRowCount,
                        sizeof(DETAIL_ROW),
                        supCompareDetailRows);
                }
                break;

            case LIST_MODE_SCHEMA:
                if (g_ctx.selectedProviderIdx < g_ctx.providerCount) {
                    p = &g_ctx.providers[g_ctx.selectedProviderIdx];

                    if (p->isManifestProvider) {
                        if (p->schemaRowCount > 0) {

                            supSort(p->schemaRows,
                                p->schemaRowCount,
                                sizeof(EVENT_SCHEMA_ROW),
                                supCompareSchemaRows);

                        }
                    }
                    else {
                        if (p->mofSchemaRowCount > 0) {

                            supSort(p->mofSchemaRows,
                                p->mofSchemaRowCount,
                                sizeof(MOF_SCHEMA_ROW),
                                supCompareMofSchemaRows);
                        }
                    }
                }
                break;

            case LIST_MODE_LIVE:
                supSortLiveEvents();
                break;

            default:
                break;
            }

            ListView_RedrawItems(g_ctx.hList, 0, ListView_GetItemCount(g_ctx.hList) - 1);
            UpdateWindow(g_ctx.hList);
            return 0;
        }
    }

    return 0;
}

/*
* HandleCaptureEnd
*
* Purpose:
*
* Finalizes a completed ETW capture, refreshes the live view and UI
* state, and reports capture failures or partially enabled providers.
*
*/
VOID HandleCaptureEnd(
    _In_ HWND hWnd
)
{
    WCHAR message[320];

    KillTimer(g_ctx.hMainWnd, ID_TIMER_LIVE_REFRESH);
    if (g_ctx.showingLivePane)
        supRefreshLivePaneIncremental(g_ctx.autoScrollEnabled);

    supUpdateMenuState();
    supUpdateToolbarState();
    supUpdateStatusBar();

    if (g_ctx.captureEndReason == CAPTURE_END_START_TRACE_FAILED) {

        if (g_ctx.captureEndStatus == ERROR_ACCESS_DENIED) {

            StringCchCopy(message,
                ARRAYSIZE(message),
                TEXT("Could not start the trace session: access denied.\n\n")
                TEXT("Starting an ETW session usually requires administrator privileges. ")
                TEXT("Try File > Run as Administrator."));
        }
        else {

            StringCchPrintf(message,
                ARRAYSIZE(message),
                TEXT("Could not start the trace session (error %lu)."),
                g_ctx.captureEndStatus);
        }

        MessageBox(hWnd, message, TEXT("Capture failed"), MB_OK | MB_ICONWARNING);
    }
    else if (g_ctx.captureEndReason == CAPTURE_END_OPEN_TRACE_FAILED) {

        StringCchPrintf(message,
            ARRAYSIZE(message),
            TEXT("Could not read events (error %lu).%s"),
            g_ctx.captureEndStatus,
            (g_ctx.captureMode == CAPTURE_MODE_REPLAY) ?
            TEXT(" Check the .etl file path and that it isn't in use.") :
            TEXT(""));

        MessageBox(hWnd, message, TEXT("Capture failed"), MB_OK | MB_ICONWARNING);
    }
    else if ((g_ctx.captureEndReason == CAPTURE_END_OK) &&
        (g_ctx.providersFailedToEnable > 0))
    {
        StringCchPrintf(message,
            ARRAYSIZE(message),
            TEXT("%lu of %lu provider(s) could not be enabled ")
            TEXT("(often access denied on privileged providers). ")
            TEXT("Capture continued with the rest."),
            g_ctx.providersFailedToEnable,
            g_ctx.liveProviderGuidCount);

        MessageBox(hWnd, message, TEXT("Some providers were skipped"), MB_OK | MB_ICONINFORMATION);
    }
    else if (g_ctx.captureEndReason == CAPTURE_END_OUT_OF_MEMORY)
    {
        StringCchPrintf(message,
            ARRAYSIZE(message),
            TEXT("The capture could not continue because the system ran out of memory ")
            TEXT("(error %lu)."),
            g_ctx.captureEndStatus);

        MessageBox(hWnd, message, TEXT("Capture failed"), MB_OK | MB_ICONWARNING);
    }
    else if (g_ctx.captureEndReason == CAPTURE_END_PROCESS_TRACE_FAILED) {

        StringCchPrintf(message,
            ARRAYSIZE(message),
            TEXT("Event processing failed (error %lu)."),
            g_ctx.captureEndStatus);

        MessageBox(hWnd, message, TEXT("Capture failed"), MB_OK | MB_ICONWARNING);
    }
}

/*
* HandleMofSchemaLoaded
*
* Purpose:
*
* Finalizes the completed MOF schema worker operation and refreshes
* the schema view when it still displays the provider that was loaded.
*
*/
VOID HandleMofSchemaLoaded(
    _In_ ULONG ProviderIndex
)
{
    if (g_ctx.mofSchemaThread) {
        CloseHandle(g_ctx.mofSchemaThread);
        g_ctx.mofSchemaThread = NULL;
    }

    if (g_ctx.showingSchemaPane &&
        g_ctx.selectedProviderIdx == ProviderIndex &&
        ProviderIndex < g_ctx.providerCount)
    {
        PopulateListForSchema(ProviderIndex);
    }

    supUpdateMenuState();
    supUpdateToolbarState();
}

/*
* MainWndProc
*
* Purpose:
*
* Main window dispatch.
*
*/
LRESULT CALLBACK MainWndProc(
    _In_ HWND hWnd,
    _In_ UINT msg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    INT x, treeWidth, visualX;
    HDC hdc;
    POINT pt;
    RECT rc, splitterRc;
    PAINTSTRUCT ps;
    LPDRAWITEMSTRUCT pds;
    LPMEASUREITEMSTRUCT pms;

    switch (msg) {
    case WM_CREATE:
        return InitializeUIComponents(hWnd);

    case WM_SIZE:
        supLayoutChildren(hWnd);
        return 0;

    case WM_GETMINMAXINFO:
        if (lParam) {
            PMINMAXINFO(lParam)->ptMinTrackSize.x = 640;
            PMINMAXINFO(lParam)->ptMinTrackSize.y = 480;
        }
        return 0;

    case WM_LBUTTONDOWN:
        x = GET_X_LPARAM(lParam);
        GetClientRect(hWnd, &rc);
        treeWidth = supClampedTreeWidth(rc.right);
        if (supIsInSplitterZone(x, treeWidth)) {
            g_ctx.splitterDragging = TRUE;
            SetCapture(hWnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (g_ctx.splitterDragging) {
            x = GET_X_LPARAM(lParam);
            GetClientRect(hWnd, &rc);
            g_ctx.treeWidth = x;
            supLayoutChildren(hWnd);
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_ctx.splitterDragging) {
            g_ctx.splitterDragging = FALSE;
            ReleaseCapture();
            InvalidateRect(hWnd, NULL, TRUE);
            UpdateWindow(hWnd);
        }
        return 0;

    case WM_MEASUREITEM:
        pms = (LPMEASUREITEMSTRUCT)lParam;
        if (pms && pms->CtlType == ODT_MENU) {
            pms->itemWidth = 16;
            pms->itemHeight = 16;
        }
        return 0;

    case WM_DRAWITEM:
        pds = (LPDRAWITEMSTRUCT)lParam;
        if (pds && pds->CtlType == ODT_MENU) {
            DrawIconEx(pds->hDC, pds->rcItem.left - 8,
                pds->rcItem.top,
                (HICON)pds->itemData,
                16, 16, 0, NULL, DI_NORMAL);
        }
        return 0;

    case WM_SETCURSOR:
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);
        GetClientRect(hWnd, &rc);
        treeWidth = supClampedTreeWidth(rc.right);
        if (g_ctx.splitterDragging || supIsInSplitterZone(pt.x, treeWidth)) {
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            return TRUE;
        }
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rc);
        treeWidth = supClampedTreeWidth(rc.right);
        visualX = treeWidth + (SPLITTER_GAP_WIDTH - SPLITTER_VISUAL_WIDTH) / 2;

        splitterRc.left = visualX;
        splitterRc.top = 0;
        splitterRc.right = visualX + SPLITTER_VISUAL_WIDTH;
        splitterRc.bottom = rc.bottom - STATUS_BAR_HEIGHT;
        FillRect(hdc, &splitterRc, (HBRUSH)(COLOR_3DFACE + 1));
        EndPaint(hWnd, &ps);
        return 0;

    case WM_NOTIFY:
        return HandleNotify(wParam, lParam);

    case WM_CONTEXTMENU:
        return HandleContextMenu(wParam, lParam);

    case WM_TIMER:
        if (wParam == ID_TIMER_LIVE_REFRESH) {

            supUpdateProviderEventRates();

            if (g_ctx.showingLivePane && !g_ctx.displayPaused)
                supRefreshLivePaneIncremental(g_ctx.autoScrollEnabled);

            if ((g_ctx.listMode == LIST_MODE_PROVIDER) &&
                (g_ctx.selectedProviderIdx < g_ctx.providerCount) &&
                (g_ctx.liveCapturing != FALSE))
            {
                PopulateListForProvider(g_ctx.selectedProviderIdx);
            }

            supUpdateStatusBar();
            return 0;
        }
        break;

    case WM_APP_CAPTURE_ENDED:
        HandleCaptureEnd(hWnd);
        return 0;

    case WM_APP_MOF_SCHEMA_LOADED:
        HandleMofSchemaLoaded((ULONG)wParam);
        return 0;

    case WM_COMMAND:

        if (LOWORD(wParam) == ID_EDIT_FILTER) {
            if (HIWORD(wParam) == EN_CHANGE) {
                RefreshProviderTreeFilter();
            }
            return 0;
        }
        return HandleCommand(hWnd, wParam, lParam);

    case WM_KEYDOWN:

        if (wParam == VK_ESCAPE) {
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_DESTROY:
        StopLiveCapture();
        EtpWaitForMofSchemaLoad();
        supSaveSettings();
        if (g_ctx.hFont) {
            DeleteObject(g_ctx.hFont);
            g_ctx.hFont = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/*
* WinMain
*
* Purpose:
*
* Program entry point.
*
*/
int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ INT nCmdShow
)
{
    BOOL bResult;
    HRESULT hr;
    INT savedX = CW_USEDEFAULT, savedY = CW_USEDEFAULT, savedW = 1024, savedH = 768;
    SIZE_T cb;
    HWND hWnd;
    INITCOMMONCONTROLSEX icc;
    WNDCLASSEX wc;
    MSG msg;
    HACCEL acceleratorTable;

    WCHAR szWindowTitle[100];

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_Heap = GetProcessHeap();
    if (g_Heap == NULL)
        return ERROR_INTERNAL_ERROR;

#ifdef _DEBUG
    RtlSecureZeroMemory(&g_HeapStats, sizeof(g_HeapStats));
#endif

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
        return hr;

    supInitializeRichEdit(); // do not move anywhere
    InitializeAppContext(hInstance);

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    RtlSecureZeroMemory(&g_ctx.etwSystemInformation, sizeof(g_ctx.etwSystemInformation));

    //
    // ETW initialization, do not move.
    //
    EtpLoadProviders();
    g_ctx.sessionsLoadLastError = EtpLoadSessions();
    EtpLoadProviderSessionCrossReference();
    EtpBuildSessionProviderIndex();

    cb = g_ctx.providerCount ? g_ctx.providerCount : 1;
    g_ctx.providerChecked = (BOOL*)supHeapAlloc(cb * sizeof(BOOL));

    supLoadSettings(&savedX, &savedY, &savedW, &savedH);

    RtlSecureZeroMemory(&wc, sizeof(wc));

    g_ctx.hMainIcon = LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN), IMAGE_ICON, 0, 0, LR_SHARED);

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = g_ctx.hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = PROGRAM_WNDCLASS;
    wc.hIcon = (HICON)g_ctx.hMainIcon;

    RegisterClassEx(&wc);

    InitEtwxCommonDialogs();

    szWindowTitle[0] = 0;
    supStrCopy(szWindowTitle, PROGRAM_NAME);
    if (g_ctx.isAdmin != FALSE) {
        supStrCat(szWindowTitle, TEXT(" (Administrator)"));
    }

    hWnd = CreateWindowEx(0,
        PROGRAM_WNDCLASS,
        szWindowTitle,
        WS_OVERLAPPEDWINDOW,
        savedX,
        savedY,
        savedW,
        savedH,
        NULL,
        NULL,
        g_ctx.hInstance,
        NULL);

    supUpdateToolbarState();

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

#ifdef _DEBUG
    Test();
#endif

    acceleratorTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_MAIN_ACCELERATOR));

    do {

        bResult = GetMessage(&msg, NULL, 0, 0);
        if (bResult == -1)
            break;

        if (TranslateAccelerator(hWnd, acceleratorTable, &msg) == 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

    } while (bResult != 0);

    DeleteCriticalSection(&g_ctx.liveCs);
    if (acceleratorTable)
        DestroyAcceleratorTable(acceleratorTable);

    CoUninitialize();
    return 0;
}
