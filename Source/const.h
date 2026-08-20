/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       CONST.H
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
*  Common header file for program consts.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#pragma once

#define PROGRAM_MAJOR_VERSION           1
#define PROGRAM_MINOR_VERSION           0
#define PROGRAM_REVISION_NUMBER         5
#define PROGRAM_BUILD_NUMBER            2608
#define PROGRAM_COPYRIGHT               TEXT("(c) 2025 - 2026 hfiref0x, UG North")

#define PROGRAM_NAME                    TEXT("ETW Explorer")

#define PROGRAM_WNDCLASS                TEXT("ETWXMainWnd")
#define ETWXDLG_WNDCLASS                TEXT("ETWXSimpleDialog")
#define ETWXDLG_DETAILS_WNDCLASS        TEXT("ETWXDetailsDialog")
#define ETWXDLG_FIND_WNDCLASS           TEXT("ETWXFindDialog")

#define PROGRAM_SESSION                 TEXT("ETWXLiveSession")

#define REG_KEY_PATH                    TEXT("Software\\ETWX")

#define CIM_FLAG_ARRAY                  0x2000 //for Mof handling

#define MAINWND_DEFAULT_WIDTH           1024
#define MAINWND_DEFAULT_HEIGHT          768

#define MAX_LIVE_PROVIDERS              64
#define DEFAULT_LIVE_EVENT_LIMIT        100000
#define MAX_LIVE_EVENT_LIMIT            350000

#define STATUS_BAR_HEIGHT               24

#define SPLITTER_VISUAL_WIDTH           3
#define SPLITTER_GAP_WIDTH              3
#define SPLITTER_MIN_TREE               150
#define SPLITTER_MIN_LIST               250

#define ICON_INFO                       0  // IDI_INFORMATION - manifest-based providers, Live Capture root, info-level events
#define ICON_WARN                       1  // IDI_WARNING     - MOF/legacy providers, warning-level events
#define ICON_ERROR                      2  // IDI_ERROR       - critical/error-level events
#define ICON_UNKNOWN                    3  // IDI_QUESTION    - unknown-level events

#define ICON_PROVIDERS                  4
#define ICON_PROVIDER                   5
#define ICON_SESSIONS                   6
#define ICON_SESSION                    7
#define ICON_LIVECAPTURE                8

#define TBLIST_ICON_START_CAPTURE       0
#define TBLIST_ICON_STOP_CAPTURE        1
#define TBLIST_ICON_BROWSE_FOR_SCHEMA   2
#define TBLIST_ICON_REFRESH             3
#define TBLIST_ICON_EXPORT              4
#define TBLIST_ICON_FIND                5

#define LISTVIEW_COLUMN_FIELD           0
#define LISTVIEW_COLUMN_VALUE           1

#define SCHEMA_COLUMN_EVENT_ID          0
#define SCHEMA_COLUMN_VERSION           1
#define SCHEMA_COLUMN_LEVEL             2
#define SCHEMA_COLUMN_TASK              3
#define SCHEMA_COLUMN_OPCODE            4
#define SCHEMA_COLUMN_KEYWORD           5
#define SCHEMA_COLUMN_PROPERTIES        6

#define MANIFEST_SCHEMA_COLUMN_COUNT    6

#define MOF_SCHEMA_COLUMN_EVENT_TYPE    0
#define MOF_SCHEMA_COLUMN_EVENT_TYPES   1
#define MOF_SCHEMA_COLUMN_CLASS         2
#define MOF_SCHEMA_COLUMN_PROPERTIES    3
#define MOF_SCHEMA_COLUMN_DESCRIPTION   4

#define MOF_SCHEMA_COLUMN_COUNT         5

#define LIVE_COLUMN_TIME                0
#define LIVE_COLUMN_PROVIDER            1
#define LIVE_COLUMN_EVENT_ID            2
#define LIVE_COLUMN_LEVEL               3
#define LIVE_COLUMN_KEYWORD             4
#define LIVE_COLUMN_PROPERTIES          5

#define LIVE_COLUMN_COUNT               5

#define NODE_KIND_PROVIDER              0
#define NODE_KIND_SESSION               1
#define NODE_KIND_LIVE                  2
#define NODE_ROOT_MARKER                0x3FFFFFFFu
#define MAKE_NODE_PARAM(kind, idx)      ((LPARAM)(((ULONG)(kind) << 30) | ((ULONG)(idx) & 0x3FFFFFFF)))
#define NODE_KIND(p)                    ((ULONG)(((ULONG)(p) >> 30) & 0x3))
#define NODE_INDEX(p)                   ((ULONG)((p) & 0x3FFFFFFF))

#define ID_TIMER_LIVE_REFRESH           100
#define LIVE_REFRESH_INTERVAL_MS        150
#define WM_APP_CAPTURE_ENDED            (WM_APP + 2)
#define WM_APP_MOF_SCHEMA_LOADED        (WM_APP + 3)
#define WM_APP_LOAD_METADATA            (WM_APP + 4)

#define DEFAULT_PROVIDER_IDX            0x3FFFFFFF

// Event ID filter (Capture > Set Event ID Filter...) - comma-separated
// list of decimal event IDs to keep; empty/count==0 means no filtering
// (match everything, same "0/empty = no restriction" convention as the
// keyword filter).
#define MAX_EVENTID_FILTERS             64
#define INITIAL_SESSION_CAPACITY        64


#define FILTER_HEIGHT                   24
#define TOOLBAR_HEIGHT                  34

#define ETP_MAX_PROPERTY_NESTING          8
#define ETP_MAX_MOF_TEXT_DEPTH           32
#define ETP_MAX_MOF_PROPERTIES          128


