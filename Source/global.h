/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       GLOBALS.H
*
*  VERSION:     1.05
*
*  DATE:        18 Aug 2026
*
*  Common header file for the ETW Explorer.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#pragma once

#if defined (_MSC_VER)
#if (_MSC_VER >= 1920)
#pragma comment(linker,"/merge:_RDATA=.rdata")
#endif
#endif

#include <Windows.h>
#include <strsafe.h>
#include <intsafe.h>
#include <Windowsx.h>

#include <objbase.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <Richedit.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <evntrace.h>
#include <evntcons.h>
#include <wbemidl.h>
#include <tdh.h>
#include <aclui.h>
#include <sddl.h>
#include <DbgHelp.h>

#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "Aclui.lib")
#pragma comment(lib, "Dbghelp.lib")


#include "prtl/prtl.h"
#include "rsrc/resource.h"

#include "const.h"
#include "types.h"
#include "etwmeta.h"
#include "sup.h"
#include "tests/tests.h"
#include "security.h"
#include "commonDlg.h"

typedef struct _APP_CTX {
    HINSTANCE hInstance;
    CRITICAL_SECTION liveCs;

    BOOL showingLivePane;
    BOOL showingSchemaPane;
    BOOL isAdmin;
    BOOL saveToEtlEnabled;

    HIMAGELIST hImageList;
    HWND hMainWnd;
    HFONT hFont;

    HMENU hMenu;
    HMENU hMenuFile;
    HMENU hMenuCapture;
    HMENU hMenuVerbosity;
    HMENU hMenuSchema;
    HMENU hMenuView;
    HMENU hMenuFind;

    HWND hTree;
    HWND hList;
    HWND hEditFilter;  // Provider search/filter box
    HWND hStatusBar;  
    HWND hToolbar;

    HIMAGELIST hToolbarImageList;
    HANDLE hMainIcon;

    INT treeWidth;

    INT sortColumn;
    BOOL sortAscending;

    BOOL splitterDragging;

    ULONG providerCount;
    ULONG selectedProviderIdx;

    UCHAR selectedLevel;
    PROVIDER_ENTRY* providers;
    BOOL* providerChecked; // Providers checked state

    HANDLE mofSchemaThread;
    volatile LONG mofSchemaWorkerActive;
    ULONG mofSchemaProviderIndex;

    ULONG sessionCount;
    ULONG sessionsLoadLastError;
    SESSION_ENTRY* sessions;

    HTREEITEM ctxTreeItem;
    INT ctxListRow;
    HTREEITEM hProvRoot;

    TDH_HANDLE wppDecodingHandle;

    //
    // Filtering.
    //
    WCHAR eventIdFilterText[256];
    USHORT eventIdFilter[MAX_EVENTID_FILTERS];
    ULONG eventIdFilterCount;

    //
    // Virtual listview.
    //
    LIST_MODE listMode;
    DETAIL_ROW* detailRows;
    ULONG detailRowCount;
    ULONG detailRowCapacity;
    
    //
    // Live capturing.
    //
    CAPTURE_MODE captureMode;
    CAPTURE_END_REASON captureEndReason;
    ULONG captureEndStatus;
    volatile LONG captureStopRequested;
    ULONG providersFailedToEnable;
    volatile LONG liveCapturing;
    LIVE_EVENT_ROW* liveEvents;
    ULONG liveEventCount;
    ULONG liveEventCapacity;
    ULONG liveEventLimit;
    ULONG liveEventHead;
    ULONG64 liveEventGeneration;
    ULONG* liveSortedIndices;
    ULONG liveSortedIndexCount;
    ULONG liveDisplayedCount;
    ULONG64 liveDisplayedGeneration;
    ULONGLONG liveEventTotalCount;
    HANDLE liveThread;
    TRACEHANDLE liveTraceHandle;
    TRACEHANDLE liveSessionHandle;
    WCHAR* liveSessionName;
    UCHAR liveLevel;
    ULONGLONG liveMatchAnyKeyword;
    ULONG liveProviderGuidCount;
    GUID liveProviderGuids[MAX_LIVE_PROVIDERS];

    //
    // Live event search.   
    //
    HWND hEventFindDlg;
    ETWX_SEARCH_CONTEXT liveEventSearch;
    ULONG liveEventSearchIndex;

    //
    // Keyword dialog.
    //
    WCHAR keywordFilterText[32];

    BOOL displayPaused;    // Capture > Pause Display - capture keeps running, list just stops refreshing
    BOOL autoScrollEnabled; // View > Auto-scroll
    BOOL colorizeEnabled;   // View > Colorize by Level
    GUID highlightActivityId;
    BOOL highlightActivityIdActive;

    //
    // Replay, etl, wpp paths.
    //
    WCHAR wppTmfPath[MAX_PATH + 1];
    WCHAR replayFilePath[MAX_PATH + 1];
    WCHAR etlSavePath[MAX_PATH + 1];

    //
    // System information.
    //
    ETW_SYSTEM_INFORMATION etwSystemInformation;
} APP_CTX,* PAPP_CTX;

extern APP_CTX g_ctx;
extern HANDLE g_Heap;
