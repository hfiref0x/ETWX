/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       TYPES.H
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
*  Common header file for types definitions.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#pragma once

typedef struct _LIVE_EVENT_ROW {
    SYSTEMTIME localTime;
    GUID providerGuid;
    WCHAR providerLabel[300];  // friendly name if known, else GUID string
    USHORT eventId;
    UCHAR level;
    ULONGLONG keyword;
    WCHAR properties[800];     // "Name=Value Name2=Value2 ..." - see DecodeEventProperties
    GUID activityId;           // always present (EVENT_HEADER::ActivityId), may be all-zero if unused
    GUID relatedActivityId;    // only present if hasRelatedActivityId
    BOOL hasRelatedActivityId; // parsed from EventRecord->ExtendedData, not always present
} LIVE_EVENT_ROW, * PLIVE_EVENT_ROW;
typedef const LIVE_EVENT_ROW* PCLIVE_EVENT_ROW;

typedef struct _ENABLE_ROW {
    ULONG loggerId;
    UCHAR isEnabled;
    UCHAR level;
    ULONGLONG matchAnyKeyword;
    ULONGLONG matchAllKeyword;
} ENABLE_ROW, * PENABLE_ROW;

typedef struct _EVENT_SCHEMA_ROW {
    USHORT id;
    UCHAR version;
    UCHAR level;
    UCHAR opcode;
    USHORT task;
    ULONGLONG keyword;
    WCHAR taskName[128];
    WCHAR opcodeName[128];
    WCHAR levelName[64];
    WCHAR propertyNames[600];
} EVENT_SCHEMA_ROW, * PEVENT_SCHEMA_ROW;

typedef struct _MOF_SCHEMA_ROW {
    ULONG eventType;
    WCHAR eventTypes[256];
    WCHAR className[256];
    WCHAR description[512];
    WCHAR propertyNames[600];
} MOF_SCHEMA_ROW, * PMOF_SCHEMA_ROW;

typedef struct _MOF_PROPERTY {
    ULONG dataId;
    LONG cimType;
    WCHAR name[256];
} MOF_PROPERTY, * PMOF_PROPERTY;

typedef enum _MOF_SCHEMA_LOAD_STATUS {
    MofSchemaNotLoaded = 0,
    MofSchemaLoading,
    MofSchemaLoaded,
    MofSchemaNoEventClasses,
    MofSchemaWmiUnavailable,
    MofSchemaEnumerationFailed,
    MofSchemaDepthLimitReached
} MOF_SCHEMA_LOAD_STATUS;

typedef struct _PROVIDER_ENTRY {
    GUID guid;
    WCHAR name[256];
    BOOL isManifestProvider;     // TRUE = manifest-based, FALSE = MOF/legacy

    WCHAR resourceFileName[MAX_PATH];
    WCHAR messageFileName[MAX_PATH];

    ULONG enableRowCount;
    ENABLE_ROW* enableRows;

    BOOL schemaLoaded;           // TRUE once we've attempted a load (success or not)
    ULONG schemaRowCount;
    EVENT_SCHEMA_ROW* schemaRows;

    ULONG mofSchemaRowCount;
    ULONG mofSchemaRowCapacity;
    MOF_SCHEMA_ROW* mofSchemaRows;
    MOF_SCHEMA_LOAD_STATUS mofSchemaLoadStatus;
    HRESULT mofSchemaLoadStatusCode;

    ULONGLONG liveEventCount;
    ULONGLONG liveEventCountAtLastTick;
    DOUBLE liveEventsPerSecond;
    SYSTEMTIME liveLastEventTime;
    BOOL liveHasLastEvent;
} PROVIDER_ENTRY, * PPROVIDER_ENTRY;

typedef struct _SESSION_PROVIDER_ROW {
    ULONG providerIdx;         // index into g_ctx.providers
    UCHAR level;
    ULONGLONG matchAnyKeyword;
    ULONGLONG matchAllKeyword;
} SESSION_PROVIDER_ROW, * PSESSION_PROVIDER_ROW;

typedef enum _ETWX_SESSION_TYPE {
    EtwxSessionNormal,
    EtwxSessionKernel,
    EtwxSessionPrivate
} ETWX_SESSION_TYPE, * PETWX_SESSION_TYPE;

typedef struct _SESSION_ENTRY {
    ETWX_SESSION_TYPE type;
    WCHAR loggerName[MAX_PATH];
    WCHAR logFileName[MAX_PATH];
    ULONGLONG sessionId;
    ULONG logFileMode;
    ULONG bufferSize;
    SESSION_PROVIDER_ROW* enabledProviders;
    ULONG enabledProviderCount;
    GUID guid;
} SESSION_ENTRY, * PSESSION_ENTRY;

typedef enum _CAPTURE_MODE {
    CAPTURE_MODE_LIVE,
    CAPTURE_MODE_REPLAY
} CAPTURE_MODE;

typedef enum _CAPTURE_END_REASON {
    CAPTURE_END_OK,
    CAPTURE_END_START_TRACE_FAILED,
    CAPTURE_END_OPEN_TRACE_FAILED,
    CAPTURE_END_PROCESS_TRACE_FAILED,
    CAPTURE_END_OUT_OF_MEMORY
} CAPTURE_END_REASON;

typedef enum _TREE_CONTEXT_KIND {
    TreeContextInvalid = 0,
    TreeContextProvider,
    TreeContextSession
} TREE_CONTEXT_KIND, * PTREE_CONTEXT_KIND;

typedef struct _TEXT_BUFFER {
    LPWSTR Buffer;
    SIZE_T Length;
    SIZE_T Capacity;
} TEXT_BUFFER, * PTEXT_BUFFER;

// The ListView is LVS_OWNERDATA (virtual) - it stores no item data itself,
// so every pane's content is served on demand from one of these backing
// stores, selected by which mode is currently active.
typedef enum _LIST_MODE {
    LIST_MODE_NONE,
    LIST_MODE_PROVIDER,
    LIST_MODE_SESSION,
    LIST_MODE_LIVE,
    LIST_MODE_SCHEMA
} LIST_MODE;

typedef struct _DETAIL_ROW {
    WCHAR field[160];
    WCHAR value[900];
} DETAIL_ROW, * PDETAIL_ROW;

typedef enum _ETWX_DIALOG_TYPE {
    EtwxDialogAbout = 1,
    EtwxDialogKeyword,
    EtwxDialogEventId,
    ExtwDialogEventDetails,
    EtwxDialogEventFind,
    EtwxDialogMetadata,
    EtwxDialogSystemInformation,
    EtwxDialogMax = 0x7fff
} ETWX_DIALOG_TYPE;

typedef struct _ETWX_DIALOG_CONTEXT {
    BOOL Allocated;
    ETWX_DIALOG_TYPE Type;
    HWND hEdit;
    HWND hHeaderEdit;
    HWND hPropertiesEdit;
    HWND hCloseButton;

    HWND hProviderCombo;
    HWND hEventIdEdit;
    HWND hLevelCombo;
    HWND hKeywordModCombo;
    HWND hMatchCase;
    HWND hWholeWord;

    HFONT hFixedFont;
    PWSTR pszText;
    SIZE_T cchText;
    PWSTR pszProviderName;
    PBOOL DialogResult;
} ETWX_DIALOG_CONTEXT, * PETWX_DIALOG_CONTEXT;

typedef enum _ETWX_KEYWORD_MATCH {
    EtwxKeywordExact = 1,
    EtwxKeywordAnyBits,
    EtwxKeywordAllBits
} ETWX_KEYWORD_MATCH;

typedef struct _ETWX_SEARCH_CONTEXT {
    BOOL MatchProvider;
    GUID ProviderGuid;

    BOOL MatchEventId;
    USHORT EventId;

    BOOL MatchLevel;
    UCHAR Level;

    BOOL MatchKeyword;
    ULONGLONG KeyWord;
    ETWX_KEYWORD_MATCH KeywordMatch;

    BOOL MatchProperties;
    WCHAR Properties[256];

    BOOL MatchCase;
    BOOL MatchWholeWord;
} ETWX_SEARCH_CONTEXT, * PETWX_SEARCH_CONTEXT;

typedef struct _ETWX_SCHEMA_DIALOG_CONTEXT {
    ETWX_DIALOG_CONTEXT Base;
    LPWSTR ProviderName;
    LPWSTR SchemaXml;
} ETWX_SCHEMA_DIALOG_CONTEXT, * PETWX_SCHEMA_DIALOG_CONTEXT;

typedef struct _ETW_SYSTEM_INFORMATION {
    ULONG ActiveSessionCount;
    ULONG NormalSessionCount;
    ULONG KernelSessionCount;
    ULONG PrivateSessionCount;
    ULONG RealTimeSessionCount;
    ULONG FileSessionCount;

    ULONG TotalBuffers;
    ULONG FreeBuffers;
    ULONG EventsLost;

    ULONG ProviderCount;
    ULONG ManifestProviderCount;
    ULONG NonManifestProviderCount;

    BOOL KernelLoggerActive;
} ETW_SYSTEM_INFORMATION, * PETW_SYSTEM_INFORMATION;
