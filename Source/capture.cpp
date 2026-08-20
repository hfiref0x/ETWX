/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       CAPTURE.CPP
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
* 
*  Capture/Replay main logic.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"

/*
* EventRecordCallback
*
* Purpose:
*
* Processes each captured ETW event, applies the configured Event ID
* filter, resolves provider information, decodes event properties,
* updates provider statistics, and appends the event to the live view.
*
*/
VOID WINAPI EventRecordCallback(
    _In_ PEVENT_RECORD EventRecord
)
{
    BOOLEAN found, idMatches;
    USHORT eid;
    ULONG i;
    PEVENT_EXTENDED_ITEM_RELATED_ACTIVITYID pRelated;
    PROVIDER_ENTRY* provider;
    LIVE_EVENT_ROW row;
    FILETIME fileTime, localFileTime;

    //
    // Applied before any decoding work, not just before display - the
    // whole point of this filter is cutting captured volume from a
    // noisy provider, not just hiding rows after the fact.
    //
    if (g_ctx.eventIdFilterCount > 0) {
        eid = EventRecord->EventHeader.EventDescriptor.Id;
        idMatches = FALSE;
        for (i = 0; i < g_ctx.eventIdFilterCount; i++) {
            if (g_ctx.eventIdFilter[i] == eid) {
                idMatches = TRUE;
                break;
            }
        }
        if (!idMatches)
            return;
    }

    RtlSecureZeroMemory(&row, sizeof(row));

    fileTime.dwLowDateTime = EventRecord->EventHeader.TimeStamp.LowPart;
    fileTime.dwHighDateTime = EventRecord->EventHeader.TimeStamp.HighPart;

    FileTimeToLocalFileTime(&fileTime, &localFileTime);
    FileTimeToSystemTime(&localFileTime, &row.localTime);

    row.providerGuid = EventRecord->EventHeader.ProviderId;
    row.eventId = EventRecord->EventHeader.EventDescriptor.Id;
    row.level = EventRecord->EventHeader.EventDescriptor.Level;
    row.keyword = EventRecord->EventHeader.EventDescriptor.Keyword;
    row.activityId = EventRecord->EventHeader.ActivityId;

    //
    // RelatedActivityId isn't in the fixed EVENT_HEADER - it's an
    // optional item in the ExtendedData array (ETW always includes it
    // when the provider passed one via EventWriteTransfer, regardless
    // of EnableProperty flags).
    //
    for (i = 0; i < EventRecord->ExtendedDataCount; i++) {
        if (EventRecord->ExtendedData[i].ExtType == EVENT_HEADER_EXT_TYPE_RELATED_ACTIVITYID) {
            pRelated = (PEVENT_EXTENDED_ITEM_RELATED_ACTIVITYID)(ULONG_PTR)EventRecord->ExtendedData[i].DataPtr;
            if (pRelated) {
                row.relatedActivityId = pRelated->RelatedActivityId;
                row.hasRelatedActivityId = TRUE;
            }
            break;
        }
    }

    //
    // Prefer the friendly name we already know from provider enumeration;
    // fall back to the raw GUID if this event came from something we
    // didn't have in our provider table.
    //
    found = FALSE;

    for (i = 0; i < g_ctx.providerCount; i++) {

        provider = &g_ctx.providers[i];

        if (sizeof(GUID) == RtlCompareMemory(&provider->guid,
            &EventRecord->EventHeader.ProviderId, sizeof(GUID)))
        {
            StringCchCopy(row.providerLabel,
                ARRAYSIZE(row.providerLabel),
                provider->name);

            InterlockedIncrement64((LONGLONG*)&provider->liveEventCount);
            provider->liveLastEventTime = row.localTime;
            provider->liveHasLastEvent = TRUE;

            found = TRUE;
            break;
        }
    }

    if (!found) {

        if (0 == StringFromGUID2(EventRecord->EventHeader.ProviderId,
            row.providerLabel,
            ARRAYSIZE(row.providerLabel)))
        {
            supStrCopy(row.providerLabel, TEXT("(unknown)"));
        }
    }

    EtpDecodeEventProperties(EventRecord, row.properties, ARRAYSIZE(row.properties));
    supAppendLiveEvent(&row);
}

/*
* LiveCaptureThreadProc
*
* Purpose:
*
* Runs live ETW capture or ETL replay on a worker thread, managing
* session creation, provider enabling, event processing, and cleanup.
*
*/
DWORD WINAPI LiveCaptureThreadProc(
    _In_ PVOID Parameter
)
{
    ULONG i, loggerNameBytes, logFileNameBytes;
    ULONG propertySize, status;
    PEVENT_TRACE_PROPERTIES properties;
    PENABLE_TRACE_PARAMETERS pTraceParams = NULL;
    PEVENT_FILTER_EVENT_ID pFilterEventId = NULL;
    EVENT_TRACE_LOGFILE logFile;

    ENABLE_TRACE_PARAMETERS traceParams;
    EVENT_FILTER_DESCRIPTOR filterDesc;
    BYTE filterBuf[sizeof(EVENT_FILTER_EVENT_ID) + (MAX_EVENTID_FILTERS - 1) * sizeof(USHORT)];

    UNREFERENCED_PARAMETER(Parameter);

    g_ctx.captureEndReason = CAPTURE_END_OK;
    g_ctx.captureEndStatus = ERROR_SUCCESS;
    g_ctx.providersFailedToEnable = 0;

    //
    // Replay mode.
    //
    if (g_ctx.captureMode == CAPTURE_MODE_REPLAY) {

        RtlSecureZeroMemory(&logFile, sizeof(logFile));
        logFile.LogFileName = g_ctx.replayFilePath;
        logFile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD;
        logFile.EventRecordCallback = EventRecordCallback;

        g_ctx.liveTraceHandle = OpenTrace(&logFile);
        if (g_ctx.liveTraceHandle == (TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE) {
            g_ctx.captureEndReason = CAPTURE_END_OPEN_TRACE_FAILED;
            g_ctx.captureEndStatus = GetLastError();
            supNotifyCaptureEnded();
            return 1;
        }

        status = ProcessTrace(&g_ctx.liveTraceHandle, 1, NULL, NULL);
        if (status != ERROR_SUCCESS &&
            InterlockedCompareExchange(&g_ctx.captureStopRequested,
                FALSE,
                FALSE) == FALSE)
        {
            g_ctx.captureEndReason = CAPTURE_END_PROCESS_TRACE_FAILED;
            g_ctx.captureEndStatus = status;
        }

        CloseTrace(g_ctx.liveTraceHandle);

        g_ctx.liveTraceHandle = (TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE;
        supNotifyCaptureEnded();
        return 0;
    }

    //
    // Live capture.
    //
    loggerNameBytes = ((ULONG)supStrLen(g_ctx.liveSessionName) + 1) * sizeof(WCHAR);
    logFileNameBytes = g_ctx.saveToEtlEnabled ? (((ULONG)supStrLen(g_ctx.etlSavePath) + 1) * sizeof(WCHAR)) : 0;
    propertySize = sizeof(EVENT_TRACE_PROPERTIES) + loggerNameBytes + logFileNameBytes;

    properties = (PEVENT_TRACE_PROPERTIES)supHeapAlloc(propertySize);
    if (!properties) {
        g_ctx.captureEndReason = CAPTURE_END_OUT_OF_MEMORY;
        g_ctx.captureEndStatus = ERROR_OUTOFMEMORY;
        supNotifyCaptureEnded();
        return 1;
    }

    EtpInitializeTraceProperties(properties, propertySize, loggerNameBytes);

    //
    // Remove a stale session if one exists.
    //
    ControlTrace(0, g_ctx.liveSessionName, properties, EVENT_TRACE_CONTROL_STOP);

    RtlSecureZeroMemory(properties, propertySize);
    EtpInitializeTraceProperties(properties, propertySize, loggerNameBytes);

    status = StartTrace(&g_ctx.liveSessionHandle, g_ctx.liveSessionName, properties);
    if (status != ERROR_SUCCESS) {

        g_ctx.captureEndReason = CAPTURE_END_START_TRACE_FAILED;
        g_ctx.captureEndStatus = status;

        supHeapFree(properties);
        supNotifyCaptureEnded();
        return 1;
    }

    if (g_ctx.eventIdFilterCount > 0) {
        RtlSecureZeroMemory(&traceParams, sizeof(traceParams));
        RtlSecureZeroMemory(&filterBuf, sizeof(filterBuf));
        RtlSecureZeroMemory(&filterDesc, sizeof(filterDesc));

        pFilterEventId = (PEVENT_FILTER_EVENT_ID)filterBuf;
        pFilterEventId->FilterIn = TRUE;
        pFilterEventId->Count = (USHORT)g_ctx.eventIdFilterCount;
        for (i = 0; i < g_ctx.eventIdFilterCount; i++)
            pFilterEventId->Events[i] = g_ctx.eventIdFilter[i];

        filterDesc.Ptr = (ULONGLONG)(ULONG_PTR)pFilterEventId;
        filterDesc.Size = (ULONG)(sizeof(EVENT_FILTER_EVENT_ID) + (g_ctx.eventIdFilterCount - 1) * sizeof(USHORT));
        filterDesc.Type = EVENT_FILTER_TYPE_EVENT_ID;
        traceParams.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
        traceParams.EnableFilterDesc = &filterDesc;
        traceParams.FilterDescCount = 1;
        pTraceParams = &traceParams;
    }

    //
    // Enable providers.
    //
    for (i = 0; i < g_ctx.liveProviderGuidCount; i++) {

        status = EnableTraceEx2(g_ctx.liveSessionHandle,
            &g_ctx.liveProviderGuids[i],
            EVENT_CONTROL_CODE_ENABLE_PROVIDER,
            g_ctx.liveLevel,
            g_ctx.liveMatchAnyKeyword,
            0,
            0,
            pTraceParams);

        if (status != ERROR_SUCCESS)
            g_ctx.providersFailedToEnable++;
    }

    RtlSecureZeroMemory(&logFile, sizeof(logFile));
    logFile.LoggerName = (LPWSTR)g_ctx.liveSessionName;
    logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = EventRecordCallback;

    g_ctx.liveTraceHandle = OpenTrace(&logFile);

    if (g_ctx.liveTraceHandle == (TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE) {
        g_ctx.captureEndReason = CAPTURE_END_OPEN_TRACE_FAILED;
        g_ctx.captureEndStatus = GetLastError();
        ControlTrace(g_ctx.liveSessionHandle, g_ctx.liveSessionName, properties, EVENT_TRACE_CONTROL_STOP);

        supHeapFree(properties);
        supNotifyCaptureEnded();
        return 1;
    }

    //
    // Wait until the session is stopped.
    //
    status = ProcessTrace(&g_ctx.liveTraceHandle, 1, NULL, NULL);
    if (status != ERROR_SUCCESS &&
        InterlockedCompareExchange(&g_ctx.captureStopRequested,
            FALSE,
            FALSE) == FALSE)
    {
        g_ctx.captureEndReason = CAPTURE_END_PROCESS_TRACE_FAILED;
        g_ctx.captureEndStatus = status;
    }

    CloseTrace(g_ctx.liveTraceHandle);
    g_ctx.liveTraceHandle = (TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE;

    ControlTrace(g_ctx.liveSessionHandle, g_ctx.liveSessionName, properties, EVENT_TRACE_CONTROL_STOP);
    g_ctx.liveSessionHandle = 0;

    supHeapFree(properties);
    supNotifyCaptureEnded();
    return 0;
}

/*
* StopLiveCapture
*
* Purpose:
*
* Stops the active ETW capture.
*
*/
VOID StopLiveCapture(
    VOID
)
{
    if (!g_ctx.liveCapturing)
        return;

    InterlockedExchange(&g_ctx.captureStopRequested, TRUE);

    if (g_ctx.liveTraceHandle != (TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(g_ctx.liveTraceHandle);
    }

    if (g_ctx.liveThread) {
        WaitForSingleObject(g_ctx.liveThread, 5000);
        CloseHandle(g_ctx.liveThread);
        g_ctx.liveThread = NULL;
    }

    EtpCloseWppDecodingHandleIfOpen();

    KillTimer(g_ctx.hMainWnd, ID_TIMER_LIVE_REFRESH);
    if (g_ctx.showingLivePane)
        supRefreshLivePaneIncremental(g_ctx.autoScrollEnabled);
    supUpdateStatusBar();

    EnableMenuItem(g_ctx.hMenuCapture, ID_BTN_START, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem(g_ctx.hMenuCapture, ID_BTN_STOP, MF_BYCOMMAND | MF_GRAYED);
}

/*
* StartLiveCapture
*
* Purpose:
*
* Initializes a live ETW capture from the checked providers.
*
*/
VOID StartLiveCapture(
    VOID
)
{
    ULONG i, found;
    ULONG indices[MAX_LIVE_PROVIDERS];

    if (InterlockedCompareExchange(&g_ctx.liveCapturing, TRUE, FALSE) != 0)
        return;

    InterlockedExchange(&g_ctx.captureStopRequested, FALSE);

    RtlSecureZeroMemory(&indices, sizeof(indices));
    found = EtpGatherCheckedProviders(indices, MAX_LIVE_PROVIDERS);
    if (found == 0) {
        InterlockedExchange(&g_ctx.liveCapturing, 0);

        MessageBox(g_ctx.hMainWnd,
            TEXT("Check one or more providers in the tree (checkbox, not just selection) before starting a capture."),
            TEXT("No providers selected"),
            MB_OK | MB_ICONINFORMATION);

        return;
    }

    g_ctx.captureMode = CAPTURE_MODE_LIVE;
    EtpCloseWppDecodingHandleIfOpen();
    supResetProviderStats();
    g_ctx.liveProviderGuidCount = found;

    for (i = 0; i < found; i++) {
        g_ctx.liveProviderGuids[i] = g_ctx.providers[indices[i]].guid;
    }

    g_ctx.liveLevel = g_ctx.selectedLevel;
    g_ctx.liveMatchAnyKeyword = supHexToUInt64(g_ctx.keywordFilterText);

    supClearLiveEvents();

    g_ctx.liveThread = CreateThread(NULL, 0, LiveCaptureThreadProc, NULL, 0, NULL);
    if (g_ctx.liveThread == NULL) {
        InterlockedExchange(&g_ctx.liveCapturing, 0);
        return;
    }

    EnableMenuItem(g_ctx.hMenuCapture, ID_BTN_START, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(g_ctx.hMenuCapture, ID_BTN_STOP, MF_BYCOMMAND | MF_ENABLED);
    SetTimer(g_ctx.hMainWnd, ID_TIMER_LIVE_REFRESH, LIVE_REFRESH_INTERVAL_MS, NULL);
}

/*
* OpenEtlForReplay
*
* Purpose:
*
* Opens an ETL file selected by the user and starts replay processing
* through the existing live-event pipeline.
*
*/
VOID OpenEtlForReplay(
    _In_ HWND hWnd
)
{
    TVITEM item;
    HTREEITEM hChild;
    WCHAR szPath[MAX_PATH];
    OPENFILENAME ofn;

    if (g_ctx.liveCapturing) {
        MessageBox(hWnd, TEXT("Stop the current capture or replay first."), TEXT("Busy"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    szPath[0] = 0;

    RtlSecureZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = TEXT("ETL Files (*.etl)\0*.etl\0All Files\0*.*\0");
    ofn.lpstrFile = szPath;
    ofn.nMaxFile = ARRAYSIZE(szPath);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileName(&ofn))
        return;

    if (InterlockedCompareExchange(&g_ctx.liveCapturing, TRUE, FALSE) != FALSE)
        return;

    InterlockedExchange(&g_ctx.captureStopRequested, FALSE);

    g_ctx.captureMode = CAPTURE_MODE_REPLAY;

    EtpCloseWppDecodingHandleIfOpen();

    supResetProviderStats();

    g_ctx.replayFilePath[0] = 0;
    StringCchCopy(g_ctx.replayFilePath, ARRAYSIZE(g_ctx.replayFilePath), szPath);

    supClearLiveEvents();

    g_ctx.liveThread = CreateThread(NULL,
        0,
        LiveCaptureThreadProc,
        NULL,
        0,
        NULL);

    if (g_ctx.liveThread == NULL) {
        InterlockedExchange(&g_ctx.liveCapturing, FALSE);
        return;
    }

    EnableMenuItem(g_ctx.hMenuCapture, ID_BTN_START, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(g_ctx.hMenuCapture, ID_BTN_STOP, MF_BYCOMMAND | MF_ENABLED);
    SetTimer(g_ctx.hMainWnd, ID_TIMER_LIVE_REFRESH, LIVE_REFRESH_INTERVAL_MS, NULL);

    hChild = TreeView_GetChild(g_ctx.hTree, TVI_ROOT);
    while (hChild) {

        RtlSecureZeroMemory(&item, sizeof(item));

        item.mask = TVIF_PARAM;
        item.hItem = hChild;

        TreeView_GetItem(g_ctx.hTree, &item);

        if (NODE_KIND(item.lParam) == NODE_KIND_LIVE) {
            TreeView_SelectItem(g_ctx.hTree, hChild);
            break;
        }

        hChild = TreeView_GetNextSibling(g_ctx.hTree, hChild);
    }
}
