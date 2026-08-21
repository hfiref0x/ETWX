/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       SUP.H
*
*  VERSION:     1.05
*
*  DATE:        15 Aug 2026
*
*  Common header file for the program support routines.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#pragma once

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#ifndef RtlOffsetToPointer
#define RtlOffsetToPointer(Base, Offset)  ((PCHAR)( ((PCHAR)(Base)) + ((ULONG_PTR)(Offset))  ))
#endif

#ifndef RtlPointerToOffset
#define RtlPointerToOffset(Base, Pointer)  ((ULONG)( ((PCHAR)(Pointer)) - ((PCHAR)(Base))  ))
#endif

#ifndef IN_REGION
#define IN_REGION(x, Base, Size) ( \
    (((ULONG_PTR)(Base) + (ULONG_PTR)(Size)) > (ULONG_PTR)(Base)) && \
    /* x within [Base, Base+Size) */ \
    (((ULONG_PTR)(x) >= (ULONG_PTR)(Base)) && ((ULONG_PTR)(x) < ((ULONG_PTR)(Base) + (ULONG_PTR)(Size)))))
#endif

typedef INT(CALLBACK* PSUP_COMPARE_ROUTINE)(
    _In_ const VOID* Element1,
    _In_ const VOID* Element2
    );

#ifdef _DEBUG
typedef struct _SUP_HEAP_STATS {
    volatile LONG64 AllocCount;
    volatile LONG64 AllocBytes;

    volatile LONG64 ReAllocCount;
    volatile LONG64 ReAllocBytes;

    volatile LONG64 FreeCount;
    volatile LONG64 OutstandingCount;
} SUP_HEAP_STATS, * PSUP_HEAP_STATS;

extern SUP_HEAP_STATS g_HeapStats;
#endif

PVOID NTAPI supHeapAlloc(
    _In_ SIZE_T Size);

PVOID NTAPI supHeapReAlloc(
    _In_ PVOID BaseAddress,
    _In_ SIZE_T Size);

BOOL NTAPI supHeapFree(
    _In_ PVOID BaseAddress);

#ifdef _DEBUG

VOID supHeapGetStats(
    _Out_ PSUP_HEAP_STATS Statistics);

#endif

BOOLEAN supUserIsFullAdmin(
    VOID);

PWSTR supStrDup(
    _In_ LPCWSTR String);

VOID supLoadSettings(
    _Inout_ INT* X,
    _Inout_ INT* Y,
    _Inout_ INT* Width,
    _Inout_ INT* Height);

VOID supSaveSettings(
    VOID);

HICON supGetStockIcon(
    _In_ SHSTOCKICONID siid,
    _In_ UINT uFlags);

VOID supSetMenuIcon(
    _In_ HMENU hMenu,
    _In_ UINT iItem,
    _In_ HICON hIcon);

VOID supAddRow(
    _In_ LPCWSTR field,
    _In_ LPCWSTR value);

VOID supSetupListViewColumnsLive(
    _In_ HWND hList);

VOID supSetupListViewColumnsSchema(
    _In_ HWND hList,
    _In_ BOOL IsManifestProvider);

VOID supSetupListViewColumns(
    _In_ HWND hList,
    _In_ BOOL forProvider);

VOID supLayoutChildren(
    _In_ HWND hWnd);

INT supClampedTreeWidth(
    _In_ INT clientWidth);

VOID supCreateTreeImageList(
    _In_ HWND hTree);

VOID supCreateUIFont(
    VOID);

HFONT supCreateFixedFont(
    _In_ INT PointSize);

VOID supCreateMainMenu(
    _In_ HWND hWnd);

VOID supUpdateMenuState(
    VOID);

VOID supRemoveCheckbox(
    _In_ HWND hTree,
    _In_ HTREEITEM hItem);

INT supIconForLevel(
    _In_ UCHAR level);

VOID supApplyUIFont(
    _In_ HWND hCtrl);

BOOL supIsInSplitterZone(
    _In_ INT x,
    _In_ INT treeWidth);

VOID supNotifyCaptureEnded(
    VOID);

VOID supUpdateStatusBar(
    VOID);

VOID supCopyTextToClipboard(
    _In_ HWND hOwner,
    _In_ LPCWSTR text);

VOID supShowTreeContextMenu(
    _In_ HTREEITEM hItem,
    _In_ POINT screenPt);

BOOL supIsIntegerInType(
    _In_ USHORT inType);

VOID supFreeAllData(
    VOID);

VOID supAppendLiveEvent(
    _In_ PCLIVE_EVENT_ROW Event);

_Success_(return != FALSE)
BOOL supCopyLiveEvent(
    _In_ ULONG Index,
    _Out_ PLIVE_EVENT_ROW Event);

VOID supClearLiveEvents(
    VOID);

HTREEITEM supInsertTreeRootItem(
    _In_ HWND hTree,
    _In_ LPCWSTR Text,
    _In_ LPARAM Param,
    _In_ INT Image);

VOID supInsertListColumn(
    _In_ HWND hList,
    _In_ INT Index,
    _In_ INT Width,
    _In_ LPCWSTR Text);

VOID supToggleSaveToEtl(
    _In_ HWND WindowHandle);

VOID supRunAsAdministrator(
    _In_ HWND WindowHandle);

VOID supSetWppTmfPath(
    _In_ HWND WindowHandle);

VOID supShowListContextMenu(
    _In_ INT Row,
    _In_ POINT ScreenPoint);

VOID supEscapeCsvField(
    _In_ LPCWSTR Input,
    _Out_writes_(OutputChars) LPWSTR Output,
    _In_ SIZE_T OutputChars);

BOOL supWriteUtf8String(
    _In_ HANDLE FileHandle,
    _In_ LPCWSTR String);

BOOL supTreeContextGetSelection(
    _Out_ PTREE_CONTEXT_KIND ContextKind,
    _Out_ PULONG Index);

BOOL supTbInitialize(
    _Out_ PTEXT_BUFFER Buffer,
    _In_ SIZE_T InitialCapacity);

VOID supTbDestroy(
    _Inout_ PTEXT_BUFFER Buffer);

BOOL supTbReserve(
    _Inout_ PTEXT_BUFFER Buffer,
    _In_ SIZE_T ExtraChars);

BOOL supTbAppend(
    _Inout_ PTEXT_BUFFER Buffer,
    _In_ LPCWSTR Text);

VOID supFormatSchemaRowColumn(
    _In_ PEVENT_SCHEMA_ROW Row,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars);

VOID supFormatLiveRowColumn(
    _In_ PCLIVE_EVENT_ROW Row,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars);

VOID supGetVirtualRowColumnText(
    _In_ INT Row,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars);

INT supGetVirtualRowIcon(
    _In_ INT Row);

BOOL supTryParseFullyNumeric(
    _In_ LPCWSTR Text,
    _Out_ PULONGLONG Value);

INT supCompareTextNumericAware(
    _In_ LPCWSTR String1,
    _In_ LPCWSTR String2);

VOID supUncheckAllProviders(
    VOID);

INT CALLBACK supCompareDetailRows(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2);

INT CALLBACK supCompareMofSchemaRows(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2);

INT CALLBACK supCompareSchemaRows(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2);

INT CALLBACK supCompareLiveRows(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2);

VOID supSort(
    _Inout_updates_bytes_(Count * ElementSize) PVOID Base,
    _In_ ULONG Count,
    _In_ SIZE_T ElementSize,
    _In_ PSUP_COMPARE_ROUTINE CompareRoutine);

VOID supParseEventIdFilterText(
    VOID);

VOID supGoToProvider(
    _In_ CONST GUID * ProviderGuid);

VOID supRefreshLivePaneIncremental(
    _In_ BOOL bAutoScroll);

VOID supInsertProviderTreeItem(
    _In_ HWND hTree,
    _In_ HTREEITEM hParent,
    _In_ ULONG idx);

VOID supResetProviderStats(
    VOID);

VOID supSortLiveEvents(
    VOID);

INT CALLBACK supCompareLiveRowIndices(
    _In_ const VOID * Element1,
    _In_ const VOID * Element2);

VOID supUpdateProviderEventRates(
    VOID);

HWND supCreateModalDialog(
    _In_ HWND ParentWindow,
    _In_ LPCTSTR Title,
    _In_ INT Width,
    _In_ INT Height,
    _In_ PETWX_DIALOG_CONTEXT Context);

HWND supCreateModelessDialog(
    _In_ HWND ParentWindow,
    _In_ LPCTSTR Title,
    _In_ INT Width,
    _In_ INT Height,
    _In_ LPCTSTR ClassName,
    _In_opt_ LPVOID Context);

HWND supCreateMetadataDialog(
    _In_ HWND ParentWindow,
    _In_ LPCTSTR Title,
    _In_ INT Width,
    _In_ INT Height,
    _In_ PETWX_DIALOG_CONTEXT Context);

VOID supRunModalDialog(
    _In_ HWND ParentWindow,
    _Inout_ HWND * Dialog,
    _In_opt_ HWND FocusControl);

BOOL supRegisterDialogClass(
    _In_ LPCWSTR ClassName,
    _In_ WNDPROC WindowProc);

VOID supCenterWindowRect(
    _In_ HWND ParentWindow,
    _Inout_ PRECT WindowRect);

VOID supRegisterDialogClassOnce(
    _In_ PBOOLEAN Registered,
    _In_ LPCTSTR ClassName,
    _In_ WNDPROC DialogProc);

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
    _In_opt_ HMENU Id);

SIZE supMeasureText(
    _In_ LPCWSTR Text,
    _In_ HFONT Font);

HWND supCreateToolbar(
    _In_ HWND hWnd,
    _Out_ HIMAGELIST * phToolbarImageList);

VOID supUpdateToolbarState(
    VOID);

VOID supExportCurrentViewToCsv(
    VOID);

BOOL supGetListViewColumnText(
    _In_ HWND ListView,
    _In_ INT Column,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars);

BOOL supMatchLiveEvent(
    _In_ PLIVE_EVENT_ROW Row,
    _In_ PETWX_SEARCH_CONTEXT Search);

BOOL supFindNextLiveEvent(
    _In_ PETWX_SEARCH_CONTEXT Search,
    _Out_ PULONG FoundIndex);

BOOL supParseUlong(
    _In_ LPCWSTR String,
    _Out_ PULONG Value);

BOOL supParseUlonglong(
    _In_ LPCWSTR String,
    _Out_ PULONGLONG Value);

BOOL supGetDisplayedLiveEvent(
    _In_ ULONG DisplayIndex,
    _Out_ LIVE_EVENT_ROW * EventRow);

VOID supRunHighlightSimulation(VOID);

BOOL supInitializeRichEdit(
    VOID);

BOOL supWriteMiniDump(
    _In_opt_ PEXCEPTION_POINTERS ExceptionInfo);

LONG WINAPI supUnhandledExceptionFilter(
    _In_ PEXCEPTION_POINTERS ExceptionInfo);
