/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       ETWMETA.CPP
*
*  VERSION:     1.05
*
*  DATE:        17 Aug 2026
*
*  ETW metadata handling through TDH/WMI mess.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"

static const GUID g_SystemTraceControlGuid = {
    0x9e814aad, 0x3204, 0x11d2, { 0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39 }
};

/*
* EtpLoadProviders
*
* Purpose:
*
* Enumerates registered ETW providers and builds the internal provider table.
*
*/
VOID EtpLoadProviders(
    VOID
)
{
    ULONG i, status, bufferSize = 0;
    LPWSTR name;
    PPROVIDER_ENUMERATION_INFO pInfo = NULL;
    PTRACE_PROVIDER_INFO src;
    PPROVIDER_ENTRY dst;

    do {
        status = TdhEnumerateProviders(NULL, &bufferSize);
        if (status != ERROR_INSUFFICIENT_BUFFER)
            break;

        pInfo = (PPROVIDER_ENUMERATION_INFO)supHeapAlloc(bufferSize);
        if (!pInfo)
            break;

        status = TdhEnumerateProviders(pInfo, &bufferSize);
        if (status != ERROR_SUCCESS)
            break;

        if (pInfo->NumberOfProviders == 0)
            break;

        g_ctx.providerCount = pInfo->NumberOfProviders;
        g_ctx.etwSystemInformation.ProviderCount = pInfo->NumberOfProviders;
        g_ctx.providers = (PROVIDER_ENTRY*)supHeapAlloc(g_ctx.providerCount * sizeof(PROVIDER_ENTRY));
        if (!g_ctx.providers) {
            g_ctx.providerCount = 0;
            break;
        }

        g_ctx.etwSystemInformation.ManifestProviderCount = 0;
        g_ctx.etwSystemInformation.NonManifestProviderCount = 0;

        for (i = 0; i < g_ctx.providerCount; i++) {
            src = &pInfo->TraceProviderInfoArray[i];
            dst = &g_ctx.providers[i];

            dst->guid = src->ProviderGuid;
            dst->isManifestProvider = (src->SchemaSource == 0);

            name = (LPWSTR)RtlOffsetToPointer(pInfo, src->ProviderNameOffset);
            StringCchCopy(dst->name, ARRAYSIZE(dst->name), name);

            if (dst->isManifestProvider) {
                g_ctx.etwSystemInformation.ManifestProviderCount++;
                EtpLoadProviderResourceInformation(&dst->guid,
                    dst->resourceFileName,
                    ARRAYSIZE(dst->resourceFileName),
                    dst->messageFileName,
                    ARRAYSIZE(dst->messageFileName));
            }
            else {
                g_ctx.etwSystemInformation.NonManifestProviderCount++;
            }
        }

    } while (FALSE);

    if (pInfo) supHeapFree(pInfo);
}

/*
* EtpxFreeTracePropertiesArray
*
* Purpose:
*
* Frees an array of EVENT_TRACE_PROPERTIES structures allocated by
* EtpxAllocateTracePropertiesArray.
*
*/
VOID EtpxFreeTracePropertiesArray(
    PEVENT_TRACE_PROPERTIES* Array,
    ULONG CountAllocated
)
{
    ULONG i;

    if (Array) {
        for (i = 0; i < CountAllocated; i++)
            supHeapFree(Array[i]);

        supHeapFree(Array);
    }
}

/*
* EtpxAllocateTracePropertiesArray
*
* Purpose:
*
* Allocates and initializes an array of EVENT_TRACE_PROPERTIES
* structures for use with QueryAllTraces.
*
*/
PEVENT_TRACE_PROPERTIES* EtpxAllocateTracePropertiesArray(
    _In_ ULONG Count,
    _In_ ULONG PropertySize
)
{
    ULONG i;
    PEVENT_TRACE_PROPERTIES* propArray;

    propArray = (PEVENT_TRACE_PROPERTIES*)supHeapAlloc(Count * sizeof(PEVENT_TRACE_PROPERTIES));
    if (!propArray)
        return NULL;

    for (i = 0; i < Count; i++) {

        propArray[i] = (PEVENT_TRACE_PROPERTIES)supHeapAlloc(PropertySize);
        if (!propArray[i]) {
            EtpxFreeTracePropertiesArray(propArray, Count);
            return NULL;
        }

        propArray[i]->Wnode.BufferSize = PropertySize;
        propArray[i]->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        propArray[i]->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + MAX_PATH * sizeof(WCHAR);
    }

    return propArray;
}

/*
* EtpLoadSessions
*
* Purpose:
*
* Enumerates the ETW trace sessions visible to the current process
* and populates the global session list.
*
*/
ULONG EtpLoadSessions(
    VOID
)
{
    ULONG status, i;
    ULONG returned = 0;
    ULONG sessionCap = 64;
    ULONG propSize = sizeof(EVENT_TRACE_PROPERTIES) + (2 * MAX_PATH * sizeof(WCHAR));

    LPWSTR loggerName, logFileName;
    PEVENT_TRACE_PROPERTIES p;
    PEVENT_TRACE_PROPERTIES* propArray;
    PSESSION_ENTRY sessions;

    propArray = EtpxAllocateTracePropertiesArray(sessionCap, propSize);
    if (!propArray)
        return ERROR_NOT_ENOUGH_MEMORY;

    status = QueryAllTraces(propArray, sessionCap, &returned);
    if (status == ERROR_MORE_DATA &&
        returned > sessionCap)
    {
        EtpxFreeTracePropertiesArray(propArray, sessionCap);

        sessionCap = returned;
        propArray = EtpxAllocateTracePropertiesArray(sessionCap, propSize);
        if (!propArray)
            return ERROR_NOT_ENOUGH_MEMORY;

        status = QueryAllTraces(propArray, sessionCap, &returned);
    }

    if (status != ERROR_SUCCESS) {
        EtpxFreeTracePropertiesArray(propArray, sessionCap);
        return status;
    }

    sessions = (PSESSION_ENTRY)supHeapAlloc(returned * sizeof(SESSION_ENTRY));
    if (!sessions) {
        EtpxFreeTracePropertiesArray(propArray, sessionCap);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    g_ctx.sessions = sessions;
    g_ctx.sessionCount = returned;

    for (i = 0; i < returned; i++) {

        p = propArray[i];
        loggerName = (LPWSTR)RtlOffsetToPointer(p, p->LoggerNameOffset);

        StringCchCopy(sessions[i].loggerName,
            ARRAYSIZE(sessions[i].loggerName),
            loggerName);

        sessions[i].logFileName[0] = L'\0';

        if (p->LogFileNameOffset != 0) {
            logFileName = (LPWSTR)RtlOffsetToPointer(
                p,
                p->LogFileNameOffset);

            if (logFileName[0] != L'\0') {
                StringCchCopy(
                    sessions[i].logFileName,
                    ARRAYSIZE(sessions[i].logFileName),
                    logFileName);
            }
        }

        sessions[i].sessionId = p->Wnode.HistoricalContext;
        sessions[i].logFileMode = p->LogFileMode;
        sessions[i].bufferSize = p->BufferSize;

        sessions[i].guid = p->Wnode.Guid;
        sessions[i].type = EtwxSessionNormal;

        if (IsEqualGUID(p->Wnode.Guid, g_SystemTraceControlGuid)) {
            g_ctx.etwSystemInformation.KernelSessionCount++;
            sessions[i].type = EtwxSessionKernel;
        }
        else if (p->LogFileMode & EVENT_TRACE_PRIVATE_LOGGER_MODE)
        {
            sessions[i].type = EtwxSessionPrivate;
            g_ctx.etwSystemInformation.PrivateSessionCount++;
        }
        else {
            g_ctx.etwSystemInformation.NormalSessionCount++;
        }

        g_ctx.etwSystemInformation.ActiveSessionCount++;
        if (p->LogFileMode & EVENT_TRACE_REAL_TIME_MODE) {
            g_ctx.etwSystemInformation.RealTimeSessionCount++;
        }
        else {
            g_ctx.etwSystemInformation.FileSessionCount++;
        }

        g_ctx.etwSystemInformation.TotalBuffers += p->NumberOfBuffers;
        g_ctx.etwSystemInformation.FreeBuffers += p->FreeBuffers;
        g_ctx.etwSystemInformation.EventsLost += p->EventsLost;

        if (sessions[i].type == EtwxSessionKernel)
            g_ctx.etwSystemInformation.KernelLoggerActive = TRUE;
    }

    EtpxFreeTracePropertiesArray(propArray, sessionCap);
    return ERROR_SUCCESS;
}

/*
* EtpGetProviderInstanceInfo
*
* Purpose:
*
* Validates and retrieves a provider instance record together with
* its associated enable information from a TRACE_GUID_INFO buffer.
*
*/
_Success_(return != FALSE)
BOOL EtpGetProviderInstanceInfo(
    _In_reads_bytes_(BufferSize) PVOID Buffer,
    _In_ ULONG BufferSize,
    _In_ ULONG Offset,
    _Out_ PTRACE_PROVIDER_INSTANCE_INFO * InstanceInfo,
    _Out_ PTRACE_ENABLE_INFO * EnableInfo
)
{
    PVOID end;
    PTRACE_ENABLE_INFO enableInfo;
    PTRACE_PROVIDER_INSTANCE_INFO instance;

    instance = (PTRACE_PROVIDER_INSTANCE_INFO)RtlOffsetToPointer(Buffer, Offset);

    if (!IN_REGION(instance, Buffer, BufferSize))
        return FALSE;

    end = RtlOffsetToPointer(instance, sizeof(TRACE_PROVIDER_INSTANCE_INFO) - 1);
    if (!IN_REGION(end, Buffer, BufferSize))
        return FALSE;

    enableInfo = (PTRACE_ENABLE_INFO)RtlOffsetToPointer(instance, sizeof(TRACE_PROVIDER_INSTANCE_INFO));

    if (instance->EnableCount != 0) {

        if (instance->EnableCount >
            (ULONG_MAX / sizeof(TRACE_ENABLE_INFO)))
        {
            return FALSE;
        }

        end = RtlOffsetToPointer(enableInfo,
            instance->EnableCount * sizeof(TRACE_ENABLE_INFO) - 1);

        if (!IN_REGION(end, Buffer, BufferSize))
            return FALSE;
    }

    *InstanceInfo = instance;
    *EnableInfo = enableInfo;

    return TRUE;
}

/*
* EtpLoadProviderSessionCrossReference
*
* Purpose:
*
* Enumerates ETW provider enablement information and builds the
* association between providers and the tracing sessions in which
* they are currently enabled.
*
*/
VOID EtpLoadProviderSessionCrossReference(
    VOID
)
{
    ULONG i, j, inst;
    ULONG status, infoSize, offset;
    ULONG rowIndex, totalRows;

    PVOID infoBuffer;
    PPROVIDER_ENTRY provider;
    PTRACE_GUID_INFO guidInfo;
    PTRACE_ENABLE_INFO enableInfo;
    PTRACE_PROVIDER_INSTANCE_INFO instanceInfo;

    for (i = 0; i < g_ctx.providerCount; i++) {

        provider = &g_ctx.providers[i];

        infoBuffer = NULL;
        infoSize = 0;

        do {

            status = EnumerateTraceGuidsEx(TraceGuidQueryInfo,
                &provider->guid,
                sizeof(GUID),
                NULL,
                0,
                &infoSize);

            if (status != ERROR_INSUFFICIENT_BUFFER ||
                infoSize < sizeof(TRACE_GUID_INFO))
            {
                break;
            }

            infoBuffer = supHeapAlloc(infoSize);
            if (!infoBuffer)
                break;

            status = EnumerateTraceGuidsEx(TraceGuidQueryInfo,
                &provider->guid,
                sizeof(GUID),
                infoBuffer,
                infoSize,
                &infoSize);

            if (status != ERROR_SUCCESS ||
                infoSize < sizeof(TRACE_GUID_INFO))
            {
                break;
            }

            guidInfo = (PTRACE_GUID_INFO)infoBuffer;

            //
            // Pass #1 - count rows.
            //
            totalRows = 0;
            offset = sizeof(TRACE_GUID_INFO);

            for (inst = 0; inst < guidInfo->InstanceCount; inst++) {

                if (!EtpGetProviderInstanceInfo(infoBuffer,
                    infoSize,
                    offset,
                    &instanceInfo,
                    &enableInfo))
                {
                    break;
                }

                totalRows += instanceInfo->EnableCount;

                if (instanceInfo->NextOffset == 0)
                    break;

                if (instanceInfo->NextOffset <= offset)
                    break;

                offset = instanceInfo->NextOffset;
            }

            if (totalRows == 0)
                break;

            provider->enableRows = (PENABLE_ROW)supHeapAlloc(totalRows * sizeof(ENABLE_ROW));
            if (!provider->enableRows)
                break;

            //
            // Pass #2 - fill rows.
            //
            rowIndex = 0;
            offset = sizeof(TRACE_GUID_INFO);

            for (inst = 0;
                inst < guidInfo->InstanceCount &&
                rowIndex < totalRows;
                inst++)
            {
                if (!EtpGetProviderInstanceInfo(infoBuffer,
                    infoSize,
                    offset,
                    &instanceInfo,
                    &enableInfo))
                {
                    break;
                }

                for (j = 0;
                    j < instanceInfo->EnableCount &&
                    rowIndex < totalRows;
                    j++)
                {
                    provider->enableRows[rowIndex].loggerId = enableInfo[j].LoggerId;
                    provider->enableRows[rowIndex].isEnabled = (UCHAR)enableInfo[j].IsEnabled;
                    provider->enableRows[rowIndex].level = enableInfo[j].Level;
                    provider->enableRows[rowIndex].matchAnyKeyword = enableInfo[j].MatchAnyKeyword;
                    provider->enableRows[rowIndex].matchAllKeyword = enableInfo[j].MatchAllKeyword;
                    rowIndex++;
                }

                if (instanceInfo->NextOffset == 0)
                    break;

                if (instanceInfo->NextOffset <= offset)
                    break;

                offset = instanceInfo->NextOffset;
            }

            provider->enableRowCount = rowIndex;

        } while (FALSE);

        if (infoBuffer)
            supHeapFree(infoBuffer);
    }
}

/*
* EtpBuildSessionProviderIndex
*
* Purpose:
*
* Builds the association between ETW sessions and their enabled
* providers.
*
*/
VOID EtpBuildSessionProviderIndex(
    VOID
)
{
    USHORT sessionLoggerId;

    ULONG s, p, r;
    ULONG matchCount, index;

    PSESSION_ENTRY session;
    PPROVIDER_ENTRY provider;
    PENABLE_ROW enableRow;
    PSESSION_PROVIDER_ROW providerRow;

    for (s = 0; s < g_ctx.sessionCount; s++) {

        session = &g_ctx.sessions[s];
        sessionLoggerId = (USHORT)(session->sessionId & 0xFFFF);

        //
        // Pass #1 - count matching providers.
        //
        matchCount = 0;
        for (p = 0; p < g_ctx.providerCount; p++) {

            provider = &g_ctx.providers[p];
            for (r = 0; r < provider->enableRowCount; r++) {

                enableRow = &provider->enableRows[r];
                if ((USHORT)(enableRow->loggerId & 0xFFFF) ==
                    sessionLoggerId)
                {
                    matchCount++;
                }
            }
        }

        if (matchCount == 0)
            continue;

        session->enabledProviders = (PSESSION_PROVIDER_ROW)supHeapAlloc(matchCount * sizeof(SESSION_PROVIDER_ROW));
        if (!session->enabledProviders)
            continue;

        //
        // Pass #2 - build index.
        //
        index = 0;
        for (p = 0; p < g_ctx.providerCount; p++) {

            provider = &g_ctx.providers[p];

            for (r = 0; r < provider->enableRowCount; r++) {

                enableRow = &provider->enableRows[r];

                if ((USHORT)(enableRow->loggerId & 0xFFFF) !=
                    sessionLoggerId)
                {
                    continue;
                }

                providerRow = &session->enabledProviders[index];
                providerRow->providerIdx = p;
                providerRow->level = enableRow->level;
                providerRow->matchAnyKeyword = enableRow->matchAnyKeyword;
                providerRow->matchAllKeyword = enableRow->matchAllKeyword;

                index++;
            }
        }

        session->enabledProviderCount = index;
    }
}

/*
* EtpLoadEventSchema
*
* Purpose:
*
* Loads the schema information for a single ETW event and populates
* an internal event schema entry.
*
*/
_Success_(return != FALSE)
BOOL EtpLoadEventSchema(
    _In_ PPROVIDER_ENTRY Provider,
    _In_ const EVENT_DESCRIPTOR * Descriptor,
    _Out_ PEVENT_SCHEMA_ROW Row
)
{
    BOOL bResult = FALSE;
    ULONG i, status;
    ULONG infoSize = 0;
    SIZE_T remaining;
    PWSTR cursor, end;
    PTRACE_EVENT_INFO eventInfo = NULL;

    status = TdhGetManifestEventInformation(&Provider->guid,
        (PEVENT_DESCRIPTOR)Descriptor,
        NULL,
        &infoSize);

    if (status != ERROR_INSUFFICIENT_BUFFER)
        return FALSE;

    eventInfo = (PTRACE_EVENT_INFO)supHeapAlloc(infoSize);
    if (!eventInfo)
        return FALSE;

    do {
        status = TdhGetManifestEventInformation(&Provider->guid,
            (PEVENT_DESCRIPTOR)Descriptor,
            eventInfo,
            &infoSize);

        if (status != ERROR_SUCCESS)
            break;

        Row->id = Descriptor->Id;
        Row->version = Descriptor->Version;
        Row->level = Descriptor->Level;
        Row->opcode = Descriptor->Opcode;
        Row->task = Descriptor->Task;
        Row->keyword = Descriptor->Keyword;

        if (eventInfo->TaskNameOffset) {
            StringCchCopy(Row->taskName,
                ARRAYSIZE(Row->taskName),
                (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->TaskNameOffset));
        }

        if (eventInfo->OpcodeNameOffset) {
            StringCchCopy(Row->opcodeName,
                ARRAYSIZE(Row->opcodeName),
                (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->OpcodeNameOffset));
        }

        if (eventInfo->LevelNameOffset) {
            StringCchCopy(Row->levelName,
                ARRAYSIZE(Row->levelName),
                (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->LevelNameOffset));
        }

        cursor = Row->propertyNames;
        remaining = ARRAYSIZE(Row->propertyNames);

        for (i = 0;
            i < eventInfo->TopLevelPropertyCount && remaining > 1;
            i++)
        {
            if (FAILED(StringCchPrintfEx(cursor,
                remaining,
                &end,
                &remaining,
                0,
                TEXT("%s%s"),
                (LPCWSTR)RtlOffsetToPointer(eventInfo,
                    eventInfo->EventPropertyInfoArray[i].NameOffset),
                (i + 1 < eventInfo->TopLevelPropertyCount) ?
                TEXT(", ") : TEXT(""))))
            {
                break;
            }

            cursor = end;
        }

        bResult = TRUE;

    } while (FALSE);

    supHeapFree(eventInfo);
    return bResult;
}

/*
* EtpLoadManifestProviderSchema
*
* Purpose:
*
* Loads and caches the event schema for the specified ETW provider.
* Enumerates all manifest-defined events and builds an internal schema
* table used for event decoding and display.
*
*/
VOID EtpLoadManifestProviderSchema(
    _In_ ULONG ProviderIndex
)
{
    ULONG i, status, eventCount, rowCount = 0, bufferSize = 0;
    PPROVIDER_ENTRY provider;
    PPROVIDER_EVENT_INFO eventInfo;

    provider = &g_ctx.providers[ProviderIndex];
    if (provider->schemaLoaded)
        return;

    provider->schemaLoaded = TRUE;

    status = TdhEnumerateManifestProviderEvents(&provider->guid,
        NULL,
        &bufferSize);

    if (status != ERROR_INSUFFICIENT_BUFFER)
        return;

    eventInfo = (PPROVIDER_EVENT_INFO)supHeapAlloc(bufferSize);
    if (!eventInfo)
        return;

    status = TdhEnumerateManifestProviderEvents(&provider->guid,
        eventInfo,
        &bufferSize);

    if (status != ERROR_SUCCESS) {
        supHeapFree(eventInfo);
        return;
    }

    eventCount = eventInfo->NumberOfEvents;

    provider->schemaRows = (PEVENT_SCHEMA_ROW)supHeapAlloc(max(eventCount, 1) * sizeof(EVENT_SCHEMA_ROW));
    if (!provider->schemaRows) {
        supHeapFree(eventInfo);
        return;
    }

    for (i = 0; i < eventCount; i++) {
        if (EtpLoadEventSchema(provider,
            &eventInfo->EventDescriptorsArray[i],
            &provider->schemaRows[rowCount]))
        {
            rowCount++;
        }
    }

    provider->schemaRowCount = rowCount;
    supHeapFree(eventInfo);
}

/*
* EtpEnsureLiveEventCapacity
*
* Purpose:
*
* Ensures that the live event buffer has sufficient capacity to store
* additional events. Expands the buffer when necessary.
*
*/
BOOL EtpEnsureLiveEventCapacity(
    VOID
)
{
    ULONG newCapacity, index, physicalIndex;
    PLIVE_EVENT_ROW newBuffer;

    if (g_ctx.liveEventCount < g_ctx.liveEventCapacity)
        return TRUE;

    if (g_ctx.liveEventCapacity >= g_ctx.liveEventLimit)
        return TRUE;

    newCapacity = (g_ctx.liveEventCapacity != 0) ?
        (g_ctx.liveEventCapacity * 2) : 256;

    if (newCapacity > g_ctx.liveEventLimit)
        newCapacity = g_ctx.liveEventLimit;

    newBuffer = (PLIVE_EVENT_ROW)supHeapAlloc(newCapacity * sizeof(LIVE_EVENT_ROW));
    if (!newBuffer)
        return FALSE;

    for (index = 0; index < g_ctx.liveEventCount; index++) {
        physicalIndex = (g_ctx.liveEventHead + index) % g_ctx.liveEventCapacity;
        newBuffer[index] = g_ctx.liveEvents[physicalIndex];
    }

    supHeapFree(g_ctx.liveEvents);

    g_ctx.liveEvents = newBuffer;
    g_ctx.liveEventCapacity = newCapacity;
    g_ctx.liveEventHead = 0;
    return TRUE;
}

/*
* EtpGetElementPropertyLength
*
* Purpose:
*
* Retrieves the size, in bytes, of an ETW event property or array
* element using the TDH property query interface.
*
*/
USHORT EtpGetElementPropertyLength(
    _In_ PEVENT_RECORD EventRecord,
    _In_ LPCWSTR PropertyName,
    _In_ ULONG ArrayIndex
)
{
    ULONG propertySize = 0;
    PROPERTY_DATA_DESCRIPTOR descriptor;

    RtlSecureZeroMemory(&descriptor, sizeof(descriptor));

    descriptor.PropertyName = (ULONGLONG)(ULONG_PTR)PropertyName;
    descriptor.ArrayIndex = ArrayIndex;

    if (TdhGetPropertySize(EventRecord,
        0,
        NULL,
        1,
        &descriptor,
        &propertySize) != ERROR_SUCCESS)
    {
        return 0;
    }

    if (propertySize > USHRT_MAX)
        return 0;

    return (USHORT)propertySize;
}

/*
* EtpGetMapInfoForProperty
*
* Purpose:
*
* Retrieves event map information associated with an ETW property.
*
*/
PEVENT_MAP_INFO EtpGetMapInfoForProperty(
    _In_ PEVENT_RECORD EventRecord,
    _In_ PCTRACE_EVENT_INFO EventInfo,
    _In_ PCEVENT_PROPERTY_INFO PropertyInfo
)
{
    ULONG mapSize = 0;
    LPWSTR mapName;
    PEVENT_MAP_INFO mapInfo = NULL;

    do {

        if (PropertyInfo->Flags & PropertyStruct)
            break;

        if (PropertyInfo->nonStructType.MapNameOffset == 0)
            break;

        mapName = (LPWSTR)RtlOffsetToPointer(EventInfo, PropertyInfo->nonStructType.MapNameOffset);
        if (TdhGetEventMapInformation(EventRecord,
            mapName,
            NULL,
            &mapSize) != ERROR_INSUFFICIENT_BUFFER)
        {
            break;
        }

        mapInfo = (PEVENT_MAP_INFO)supHeapAlloc(mapSize);
        if (!mapInfo)
            break;

        if (TdhGetEventMapInformation(EventRecord,
            mapName,
            mapInfo,
            &mapSize) != ERROR_SUCCESS)
        {
            break;
        }

        return mapInfo;

    } while (FALSE);

    if (mapInfo)
        supHeapFree(mapInfo);

    return NULL;
}

/*
* EtpAppendText
*
* Purpose:
*
* Appends formatted text to an output buffer.
*
*/
BOOL EtpAppendText(
    _Inout_ PWSTR * Cursor,
    _Inout_ SIZE_T * Remaining,
    _In_ LPCWSTR Format,
    ...
)
{
    HRESULT hr;
    PWSTR end;
    va_list args;

    va_start(args, Format);

    hr = StringCchVPrintfEx(*Cursor,
        *Remaining,
        &end,
        Remaining,
        0,
        Format,
        args);

    va_end(args);

    if (FAILED(hr))
        return FALSE;

    *Cursor = end;
    return TRUE;
}

/*
* EtpGetPropertyElementCount
*
* Purpose:
*
* Determines the number of elements represented by an ETW property
* and reports whether the property is an array.
*
*/
USHORT EtpGetPropertyElementCount(
    _In_ PCEVENT_PROPERTY_INFO PropertyInfo,
    _In_reads_(DecodedCount) PULONGLONG DecodedScalars,
    _In_ ULONG DecodedCount,
    _Out_ PBOOL IsArray
)
{
    USHORT count = 1;

    *IsArray = FALSE;

    if (PropertyInfo->Flags & PropertyParamCount) {

        *IsArray = TRUE;

        if ((ULONG)PropertyInfo->countPropertyIndex < DecodedCount)
            count = (USHORT)DecodedScalars[PropertyInfo->countPropertyIndex];
        else
            count = 0;
    }
    else if (PropertyInfo->count > 1) {

        *IsArray = TRUE;
        count = PropertyInfo->count;
    }

    return count;
}

/*
* EtpFormatPropertyValue
*
* Purpose:
*
* Formats an ETW event property value using TDH services and stores
* the resulting string in a fixed-size output buffer.
*
*/
BOOL EtpFormatPropertyValue(
    _In_ PCTRACE_EVENT_INFO Info,
    _In_opt_ PEVENT_MAP_INFO MapInfo,
    _In_ ULONG PointerSize,
    _In_ USHORT InType,
    _In_ USHORT OutType,
    _In_ USHORT PropertyLength,
    _In_reads_bytes_(UserDataLength) PBYTE UserData,
    _In_ USHORT UserDataLength,
    _Out_ PTDH_FORMAT_RESULT Result
)
{
    BOOLEAN retry;
    USHORT formatLength;
    ULONG status, chars, bufferSize;
    PWSTR largeBuffer;

    RtlSecureZeroMemory(Result, sizeof(TDH_FORMAT_RESULT));

    formatLength = PropertyLength;

    do {

        retry = FALSE;
        bufferSize = sizeof(Result->Buffer);

        status = TdhFormatProperty((PTRACE_EVENT_INFO)Info,
            MapInfo,
            PointerSize,
            InType,
            OutType,
            formatLength,
            UserDataLength,
            UserData,
            &bufferSize,
            Result->Buffer,
            &Result->UserDataConsumed);

        if (status == ERROR_EVT_INVALID_EVENT_DATA &&
            formatLength == 0 &&
            InType == TDH_INTYPE_UNICODESTRING)
        {
            formatLength = UserDataLength;
            retry = TRUE;
        }

    } while (retry);

    if (InType == TDH_INTYPE_UNICODESTRING &&
        UserDataLength >= sizeof(WCHAR))
    {
        chars = UserDataLength / sizeof(WCHAR);

        while (chars &&
            ((PWCHAR)UserData)[chars - 1] == L'\0')
        {
            chars--;
        }

        StringCchCopyN(Result->Buffer, ARRAYSIZE(Result->Buffer), (PWCHAR)UserData, chars);

        Result->UserDataConsumed = UserDataLength;
        Result->Status = ERROR_SUCCESS;

        return TRUE;
    }

    if (status == ERROR_SUCCESS) {
        Result->Status = status;
        return TRUE;
    }

    if (status != ERROR_INSUFFICIENT_BUFFER) {
        Result->Status = status;
        return FALSE;
    }

    largeBuffer = (PWSTR)supHeapAlloc(bufferSize);
    if (!largeBuffer) {
        Result->Status = ERROR_OUTOFMEMORY;
        return FALSE;
    }

    status = TdhFormatProperty((PTRACE_EVENT_INFO)Info,
        MapInfo,
        PointerSize,
        InType,
        OutType,
        formatLength,
        UserDataLength,
        UserData,
        &bufferSize,
        largeBuffer,
        &Result->UserDataConsumed);

    if (status == ERROR_SUCCESS) {
        StringCchCopy(Result->Buffer, ARRAYSIZE(Result->Buffer), largeBuffer);
    }

    Result->Status = status;

    supHeapFree(largeBuffer);

    return (status == ERROR_SUCCESS);
}

/*
* EtpDecodePropertyRange
*
* Purpose:
*
* Decodes and formats a range of ETW event properties into a text
* representation.
*
*/
VOID EtpDecodePropertyRange(
    _In_ PCTRACE_EVENT_INFO Info,
    _In_ PEVENT_RECORD EventRecord,
    _In_ ULONG StartIndex,
    _In_ ULONG IndexCount,
    _In_ ULONG PointerSize,
    _Inout_ PBYTE * UserData,
    _Inout_ PUSHORT UserDataLeft,
    _Inout_updates_(DecodedCount) PULONGLONG DecodedScalars,
    _In_ ULONG DecodedCount,
    _Inout_ PWSTR * Cursor,
    _Inout_ SIZE_T * Remaining,
    _In_ ULONG Depth
)
{
    BOOL isArray;
    USHORT element, elementCount;
    USHORT inType, outType, propLength, lengthIndex;
    ULONG idx;
    ULONGLONG value;
    LPCWSTR propName;
    PEVENT_MAP_INFO mapInfo;
    TDH_FORMAT_RESULT result;
    PCEVENT_PROPERTY_INFO propInfo;

    if (Depth > ETP_MAX_PROPERTY_NESTING) {
        EtpAppendText(Cursor, Remaining, TEXT("<nesting too deep> "));
        return;
    }

    for (idx = StartIndex;
        idx < (ULONG)(StartIndex + IndexCount);
        idx++)
    {
        if (*UserDataLeft == 0 || *Remaining <= 1)
            break;

        if (idx >= Info->PropertyCount)
            break;

        propInfo = &Info->EventPropertyInfoArray[idx];
        propName = (LPCWSTR)RtlOffsetToPointer(Info, propInfo->NameOffset);

        elementCount = EtpGetPropertyElementCount(propInfo,
            DecodedScalars,
            DecodedCount,
            &isArray);

        if (isArray) {
            EtpAppendText(Cursor, Remaining, TEXT("%s=["), propName);
        }

        if (elementCount == 0) {
            if (isArray)
                EtpAppendText(Cursor, Remaining, TEXT("] "));
            continue;
        }

        mapInfo = EtpGetMapInfoForProperty(EventRecord,
            (PTRACE_EVENT_INFO)Info,
            (PEVENT_PROPERTY_INFO)propInfo);

        for (element = 0;
            element < elementCount &&
            *UserDataLeft &&
            *Remaining > 1;
            element++)
        {
            if (propInfo->Flags & PropertyStruct) {

                EtpAppendText(Cursor, Remaining, TEXT("{ "));
                EtpDecodePropertyRange(Info,
                    EventRecord,
                    propInfo->structType.StructStartIndex,
                    propInfo->structType.NumOfStructMembers,
                    PointerSize,
                    UserData,
                    UserDataLeft,
                    DecodedScalars,
                    DecodedCount,
                    Cursor,
                    Remaining,
                    Depth + 1);

                EtpAppendText(Cursor, Remaining, TEXT("} "));
                continue;
            }

            inType = propInfo->nonStructType.InType;
            outType = propInfo->nonStructType.OutType;
            propLength = propInfo->length;

            if (propInfo->Flags & PropertyParamLength) {
                lengthIndex = propInfo->lengthPropertyIndex;
                if ((ULONG)lengthIndex < DecodedCount)
                    propLength = (USHORT)DecodedScalars[lengthIndex];
            }
            else if (propLength == 0 &&
                inType != TDH_INTYPE_UNICODESTRING &&
                inType != TDH_INTYPE_ANSISTRING)
            {
                propLength = EtpGetElementPropertyLength(EventRecord,
                    propName,
                    isArray ? element : ULONG_MAX);
            }

            if (!EtpFormatPropertyValue(Info,
                mapInfo,
                PointerSize,
                inType,
                outType,
                propLength,
                *UserData,
                *UserDataLeft,
                &result))
            {
                EtpAppendText(Cursor, Remaining, TEXT("%s=<format error> "), propName);
                break;
            }

            if (isArray) {
                EtpAppendText(Cursor, Remaining, TEXT("%s,"), result.Buffer);
            }
            else {
                EtpAppendText(Cursor, Remaining, TEXT("%s=%s "), propName, result.Buffer);

                if ((ULONG)idx < DecodedCount &&
                    supIsIntegerInType(inType))
                {
                    supStrToUInt64(result.Buffer, &value);
                    DecodedScalars[idx] = value;
                }
            }

            if (result.UserDataConsumed > *UserDataLeft)
                break;

            *UserData += result.UserDataConsumed;
            *UserDataLeft -= result.UserDataConsumed;
        }

        if (isArray) {
            EtpAppendText(Cursor, Remaining, TEXT("] "));
        }

        if (mapInfo)
            supHeapFree(mapInfo);
    }
}

/*
* EtpDecodeEventProperties
*
* Purpose:
*
* Retrieves the TDH schema for an ETW event and decodes its properties
* into a formatted text representation.
*
*/
VOID EtpDecodeEventProperties(
    _In_ PEVENT_RECORD EventRecord,
    _Out_writes_(OutBufChars) PWSTR OutBuf,
    _In_ SIZE_T OutBufChars
)
{
    USHORT userDataLeft;
    ULONG bufferSize, status, pointerSize, scalarCount, messageSize = 0;
    SIZE_T remaining;
    PBYTE userData, messageBuffer;
    PWSTR cursor;
    PTRACE_EVENT_INFO info = NULL;
    PULONGLONG decodedScalars = NULL;
    TDH_CONTEXT context;

    do {
        OutBuf[0] = 0;
        bufferSize = 0;
        status = TdhGetEventInformation(EventRecord,
            0,
            NULL,
            NULL,
            &bufferSize);

        if (status != ERROR_INSUFFICIENT_BUFFER) {
            StringCchCopy(OutBuf, OutBufChars, TEXT("(no schema available)"));
            break;
        }

        info = (PTRACE_EVENT_INFO)supHeapAlloc(bufferSize);
        if (!info)
            break;

        status = TdhGetEventInformation(EventRecord,
            0,
            NULL,
            info,
            &bufferSize);

        if (status != ERROR_SUCCESS) {
            StringCchCopy(OutBuf, OutBufChars, TEXT("(schema lookup failed)"));
            break;
        }

        if (info->DecodingSource == DecodingSourceWPP) {
            supHeapFree(info);
            info = NULL;

            //
            // WPP events have no manifest - decoding them needs the matching
            // .tmf control file generated at compile time for that exact
            // build, located via a search path the user configures (Capture
            // > Set WPP TMF Search Path...). Without one, there's nothing to
            // decode against, same as before - this is inherent to WPP, 
            // Microsoft's own tracefmt requires the same -p <TMF folder> argument.
            //
            if (g_ctx.wppTmfPath[0] == L'\0') {
                StringCchCopy(OutBuf,
                    OutBufChars,
                    TEXT("(WPP event - no TMF search path configured; set one via Capture menu)"));
                break;
            }

            //
            // Cached for the whole capture rather than opened/closed per event.
            //
            if (!g_ctx.wppDecodingHandle) {
                if (TdhOpenDecodingHandle(&g_ctx.wppDecodingHandle) != ERROR_SUCCESS) {
                    g_ctx.wppDecodingHandle = NULL;
                    StringCchCopy(OutBuf,
                        OutBufChars,
                        TEXT("(WPP decode: failed to open decoding handle)"));
                    break;
                }

                RtlSecureZeroMemory(&context, sizeof(context));
                context.ParameterValue = (ULONGLONG)(ULONG_PTR)g_ctx.wppTmfPath;
                context.ParameterType = TDH_CONTEXT_WPP_TMFSEARCHPATH;

                status = TdhSetDecodingParameter(g_ctx.wppDecodingHandle, &context);
                if (status != ERROR_SUCCESS) {
                    TdhCloseDecodingHandle(g_ctx.wppDecodingHandle);
                    g_ctx.wppDecodingHandle = NULL;
                    StringCchCopy(OutBuf,
                        OutBufChars,
                        TEXT("(WPP decode: failed to set TMF search path)"));
                    break;
                }
            }

            status = TdhGetWppMessage(g_ctx.wppDecodingHandle,
                EventRecord,
                &messageSize,
                NULL);

            if (status == ERROR_INSUFFICIENT_BUFFER &&
                messageSize)
            {
                messageBuffer = (PBYTE)supHeapAlloc(messageSize);
                if (messageBuffer) {
                    status = TdhGetWppMessage(g_ctx.wppDecodingHandle,
                        EventRecord,
                        &messageSize,
                        messageBuffer);

                    if (status == ERROR_SUCCESS) {
                        StringCchCopy(OutBuf, OutBufChars, (LPCWSTR)messageBuffer);
                    }
                    else {
                        StringCchCopy(OutBuf,
                            OutBufChars,
                            TEXT("(WPP decode failed - no matching TMF found for this event)"));
                    }

                    supHeapFree(messageBuffer);
                }
                else
                {
                    StringCchCopy(OutBuf, OutBufChars, TEXT("(out of memory decoding WPP event)"));
                }
            }
            else {
                StringCchCopy(OutBuf, OutBufChars, TEXT("(WPP decode failed - no matching TMF found for this event)"));
            }
            break;

        } //DecodingSourceWPP

        pointerSize = (EventRecord->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? sizeof(ULONG) : sizeof(ULONGLONG);
        userData = (PBYTE)EventRecord->UserData;
        userDataLeft = EventRecord->UserDataLength;

        scalarCount = info->PropertyCount ? info->PropertyCount : 1;
        decodedScalars = (PULONGLONG)supHeapAlloc(scalarCount * sizeof(ULONGLONG));
        if (!decodedScalars) {
            StringCchCopy(OutBuf, OutBufChars, TEXT("(out of memory decoding)"));
            break;
        }

        cursor = OutBuf;
        remaining = OutBufChars;

        EtpDecodePropertyRange(info,
            EventRecord,
            0,
            info->TopLevelPropertyCount,
            pointerSize,
            &userData,
            &userDataLeft,
            decodedScalars,
            info->PropertyCount,
            &cursor,
            &remaining,
            0);

    } while (FALSE);

    if (decodedScalars)
        supHeapFree(decodedScalars);

    if (info)
        supHeapFree(info);
}

/*
* EtpGatherCheckedProviders
*
* Purpose:
*
* Collects the indices of all currently selected ETW providers.
*
*/
ULONG EtpGatherCheckedProviders(
    _In_ ULONG * outIndices,
    _In_ ULONG maxCount
)
{
    ULONG i, count = 0;

    if (!g_ctx.providerChecked)
        return 0;

    for (i = 0; i < g_ctx.providerCount && count < maxCount; i++) {
        if (g_ctx.providerChecked[i])
            outIndices[count++] = i;
    }

    return count;
}

/*
* EtpInitializeTraceProperties
*
* Purpose:
*
* Initializes an EVENT_TRACE_PROPERTIES structure for an ETW session.
*
*/
VOID EtpInitializeTraceProperties(
    _Inout_ PEVENT_TRACE_PROPERTIES Properties,
    _In_ ULONG BufferSize,
    _In_ ULONG LoggerNameBytes
)
{
    Properties->Wnode.BufferSize = BufferSize;
    Properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    Properties->Wnode.ClientContext = 1;

    Properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    if (g_ctx.saveToEtlEnabled) {

        Properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_FILE_MODE_SEQUENTIAL;
        Properties->LogFileNameOffset = Properties->LoggerNameOffset + LoggerNameBytes;

        StringCchCopy((LPWSTR)RtlOffsetToPointer(
            Properties,
            Properties->LogFileNameOffset),
            ARRAYSIZE(g_ctx.etlSavePath),
            g_ctx.etlSavePath);
    }
    else {
        Properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    }
}

/*
* EtpCloseWppDecodingHandleIfOpen
*
* Purpose:
*
* Closes the cached WPP decoding handle if one is currently open.
*
*/
VOID EtpCloseWppDecodingHandleIfOpen(
    VOID
)
{
    if (g_ctx.wppDecodingHandle) {
        TdhCloseDecodingHandle(g_ctx.wppDecodingHandle);
        g_ctx.wppDecodingHandle = NULL;
    }
}

/*
* EtpConnectWmi
*
* Purpose:
*
* Connect to WMI namespace.
*
*/
BOOL EtpConnectWmi(
    _Out_ IWbemServices * *Services
)
{
    HRESULT hr;
    BSTR namespaceName = NULL;
    IWbemLocator* locator = NULL;
    IWbemServices* services = NULL;

    *Services = NULL;

    hr = CoCreateInstance(CLSID_WbemLocator,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (PVOID*)&locator);

    if (FAILED(hr))
        return FALSE;

    namespaceName = SysAllocString(TEXT("ROOT\\WMI"));
    if (namespaceName == NULL) {
        locator->Release();
        return FALSE;
    }

    hr = locator->ConnectServer(namespaceName,
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        &services);

    SysFreeString(namespaceName);
    locator->Release();

    if (FAILED(hr))
        return FALSE;

    hr = CoSetProxyBlanket((IUnknown*)services,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE);

    if (FAILED(hr)) {
        services->Release();
        return FALSE;
    }

    *Services = services;
    return TRUE;
}

/*
 * EtpGetWmiQualifierString
 *
 * Purpose:
 *
 * Retrieves a string-valued qualifier from a WMI class and copies it
 * into the specified output buffer.
 *
 */
BOOL EtpGetWmiQualifierString(
    _In_ IWbemClassObject * ClassObject,
    _In_ LPCWSTR Name,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    HRESULT hr;
    IWbemQualifierSet* qualifierSet = NULL;
    VARIANT value;

    Buffer[0] = UNICODE_NULL;

    if (!ClassObject)
        return FALSE;

    hr = ClassObject->GetQualifierSet(&qualifierSet);
    if (FAILED(hr) || !qualifierSet)
        return FALSE;

    VariantInit(&value);

    hr = qualifierSet->Get(Name, 0, &value, NULL);
    if (FAILED(hr)) {
        VariantClear(&value);
        qualifierSet->Release();
        return FALSE;
    }

    if (value.vt != VT_BSTR ||
        value.bstrVal == NULL)
    {
        VariantClear(&value);
        qualifierSet->Release();
        return FALSE;
    }

    StringCchCopy(Buffer, BufferChars, value.bstrVal);

    VariantClear(&value);
    qualifierSet->Release();

    return TRUE;
}

/*
 * EtpGetWmiQualifierGuid
 *
 * Purpose:
 *
 * Retrieves a string-valued WMI qualifier and converts it to a GUID.
 *
 */
_Success_(return != FALSE)
BOOL EtpGetWmiQualifierGuid(
    _In_ IWbemClassObject * ClassObject,
    _In_ LPCWSTR Name,
    _Out_ GUID * Guid
)
{
    WCHAR buffer[64];

    if (!EtpGetWmiQualifierString(ClassObject,
        Name,
        buffer,
        ARRAYSIZE(buffer)))
    {
        return FALSE;
    }

    return SUCCEEDED(CLSIDFromString(buffer, Guid));
}

/*
* EtpGetWmiQualifierUInt32
*
* Purpose:
*
* Retrieves a numeric WMI qualifier and converts supported integer
* types to a 32-bit unsigned value.
*
*/
BOOL EtpGetWmiQualifierUInt32(
    _In_ IWbemClassObject * ClassObject,
    _In_ LPCWSTR Name,
    _Out_ PULONG Value
)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    VARIANT variant;

    *Value = 0;

    VariantInit(&variant);

    do {

        hr = ClassObject->Get(Name,
            0,
            &variant,
            NULL,
            NULL);

        if (FAILED(hr))
            break;

        if (variant.vt != VT_UI4 &&
            variant.vt != VT_I4 &&
            variant.vt != VT_UI2 &&
            variant.vt != VT_I2)
        {
            break;
        }

        switch (variant.vt) {

        case VT_UI4:
            *Value = variant.ulVal;
            break;

        case VT_I4:
            *Value = (ULONG)variant.lVal;
            break;

        case VT_UI2:
            *Value = variant.uiVal;
            break;

        case VT_I2:
            *Value = (ULONG)variant.iVal;
            break;
        }

        bResult = TRUE;

    } while (FALSE);

    VariantClear(&variant);
    return bResult;
}

/*
* EtpGetWmiPropertyDataId
*
* Purpose:
*
* Retrieves the WmiDataId qualifier associated with a WMI property.
*
*/
BOOL EtpGetWmiPropertyDataId(
    _In_ IWbemClassObject * ClassObject,
    _In_ LPCWSTR PropertyName,
    _Out_ PULONG DataId
)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    IWbemQualifierSet* qualifierSet = NULL;
    VARIANT variant;

    *DataId = 0;

    VariantInit(&variant);

    do {

        hr = ClassObject->GetPropertyQualifierSet(PropertyName, &qualifierSet);
        if (FAILED(hr) || !qualifierSet)
            break;

        hr = qualifierSet->Get(TEXT("WmiDataId"),
            0,
            &variant,
            NULL);

        if (FAILED(hr))
            break;

        if (variant.vt != VT_UI4 &&
            variant.vt != VT_I4)
        {
            break;
        }

        if (variant.vt == VT_UI4)
            *DataId = variant.ulVal;
        else
            *DataId = (ULONG)variant.lVal;

        bResult = TRUE;

    } while (FALSE);

    if (qualifierSet)
        qualifierSet->Release();

    VariantClear(&variant);
    return bResult;
}

/*
* EtpGetWmiClassName
*
* Purpose:
*
* Retrieves the __CLASS system property from a WMI class.
*
*/
BOOL EtpGetWmiClassName(
    _In_ IWbemClassObject * ClassObject,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    BOOL bResult = FALSE;
    HRESULT hr;
    VARIANT variant;

    Buffer[0] = UNICODE_NULL;
    VariantInit(&variant);

    do {
        hr = ClassObject->Get(TEXT("__CLASS"),
            0,
            &variant,
            NULL,
            NULL);

        if (FAILED(hr))
            break;

        if (variant.vt != VT_BSTR || variant.bstrVal == NULL)
            break;

        bResult = SUCCEEDED(StringCchCopy(Buffer, BufferChars, variant.bstrVal));

    } while (FALSE);

    VariantClear(&variant);
    return bResult;
}

/*
* EtpGetWmiSystemString
*
* Purpose:
*
* Retrieves a string-valued system property from a WMI class.
*
*/
BOOL EtpGetWmiSystemString(
    _In_ IWbemClassObject * ClassObject,
    _In_ LPCWSTR Name,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    HRESULT hr;
    VARIANT variant;

    Buffer[0] = 0;

    VariantInit(&variant);

    hr = ClassObject->Get(Name,
        0,
        &variant,
        NULL,
        NULL);

    if (FAILED(hr)) {
        VariantClear(&variant);
        return FALSE;
    }

    if (variant.vt == VT_BSTR && variant.bstrVal) {
        StringCchCopy(Buffer, BufferChars, variant.bstrVal);
        VariantClear(&variant);
        return TRUE;
    }

    VariantClear(&variant);
    return FALSE;
}

/*
* EtpAddMofSchemaRow
*
* Purpose:
*
* Adds a new MOF schema row to a provider, growing the row array
* when the current capacity is not enough.
*
*/
PMOF_SCHEMA_ROW EtpAddMofSchemaRow(
    _In_ PPROVIDER_ENTRY Provider
)
{
    ULONG newCapacity;
    SIZE_T allocSize;
    PMOF_SCHEMA_ROW rows;

    if (!Provider)
        return NULL;

    if (Provider->mofSchemaRowCount >= Provider->mofSchemaRowCapacity) {

        newCapacity = Provider->mofSchemaRowCapacity ?
            Provider->mofSchemaRowCapacity * 2 :
            16;

        if (newCapacity < Provider->mofSchemaRowCapacity)
            return NULL;

        if (FAILED(SizeTMult((SIZE_T)newCapacity, sizeof(MOF_SCHEMA_ROW), &allocSize))) {
            return NULL;
        }

        rows = (PMOF_SCHEMA_ROW)supHeapReAlloc(Provider->mofSchemaRows,
            newCapacity * sizeof(MOF_SCHEMA_ROW));

        if (!rows)
            return NULL;

        Provider->mofSchemaRows = rows;
        Provider->mofSchemaRowCapacity = newCapacity;
    }

    return &Provider->mofSchemaRows[Provider->mofSchemaRowCount++];
}

/*
* EtpBuildMofPropertyList
*
* Purpose:
*
* Builds a comma-separated list of MOF property names ordered by their
* WMI data identifiers.
*
*/
BOOL EtpBuildMofPropertyList(
    _In_ IWbemClassObject * ClassObject,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    BOOL enumerationStarted;
    ULONG i, j, dataId, propertyCount;
    HRESULT hr;
    SIZE_T remaining;
    BSTR propertyName = NULL;
    PMOF_PROPERTY properties;
    MOF_PROPERTY temporary;
    PWSTR cursor, end;

    if (!ClassObject ||
        !Buffer ||
        BufferChars == 0)
    {
        return FALSE;
    }

    Buffer[0] = UNICODE_NULL;
    propertyCount = 0;
    enumerationStarted = FALSE;
    properties = NULL;

    do {

        properties = (PMOF_PROPERTY)supHeapAlloc(ETP_MAX_MOF_PROPERTIES * sizeof(MOF_PROPERTY));
        if (!properties)
            break;

        hr = ClassObject->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY);
        if (FAILED(hr))
            break;

        enumerationStarted = TRUE;

        while (TRUE) {

            propertyName = NULL;

            hr = ClassObject->Next(0,
                &propertyName,
                NULL,
                NULL,
                NULL);

            if (hr != WBEM_S_NO_ERROR)
                break;

            if (propertyCount < ETP_MAX_MOF_PROPERTIES &&
                EtpGetWmiPropertyDataId(ClassObject,
                    propertyName,
                    &dataId))
            {
                properties[propertyCount].dataId = dataId;

                StringCchCopy(properties[propertyCount].name,
                    ARRAYSIZE(properties[propertyCount].name),
                    propertyName);

                propertyCount++;
            }

            SysFreeString(propertyName);
            propertyName = NULL;
        }

        if (propertyCount == 0)
            break;

        for (i = 0; i < propertyCount; i++) {

            for (j = i + 1; j < propertyCount; j++) {

                if (properties[j].dataId <
                    properties[i].dataId)
                {
                    temporary = properties[i];
                    properties[i] = properties[j];
                    properties[j] = temporary;
                }
            }
        }

        cursor = Buffer;
        remaining = BufferChars;

        for (i = 0; i < propertyCount && remaining > 1; i++) {
            if (FAILED(StringCchPrintfEx(cursor,
                remaining,
                &end,
                &remaining,
                0,
                TEXT("%s%s"),
                properties[i].name,
                (i + 1 < propertyCount) ?
                TEXT(", ") :
                TEXT(""))))
            {
                propertyCount = 0;
                break;
            }

            cursor = end;
        }

    } while (FALSE);

    if (propertyName)
        SysFreeString(propertyName);

    if (enumerationStarted)
        ClassObject->EndEnumeration();

    if (properties)
        supHeapFree(properties);

    return (propertyCount != 0);
}

/*
* EtpGetMofClassEventType
*
* Purpose:
*
* Retrieves the EventType qualifier from a WMI class.
*
*/
BOOL EtpGetMofClassEventType(
    _In_ IWbemClassObject * ClassObject,
    _Out_ PULONG EventType
)
{
    HRESULT hr;
    IWbemQualifierSet* qualifierSet = NULL;
    VARIANT value;
    LONG flavor = 0;
    BOOL result = FALSE;

    if (!ClassObject || !EventType)
        return FALSE;

    *EventType = 0;

    VariantInit(&value);

    hr = ClassObject->GetQualifierSet(&qualifierSet);
    if (SUCCEEDED(hr) && qualifierSet) {

        hr = qualifierSet->Get(TEXT("EventType"),
            0,
            &value,
            &flavor);

        if (SUCCEEDED(hr)) {

            if (value.vt == VT_UI1) {
                *EventType = value.bVal;
                result = TRUE;
            }
            else if (value.vt == VT_I1) {
                *EventType = (ULONG)value.cVal;
                result = TRUE;
            }
            else if (value.vt == VT_UI2) {
                *EventType = value.uiVal;
                result = TRUE;
            }
            else if (value.vt == VT_I2) {
                *EventType = (ULONG)value.iVal;
                result = TRUE;
            }
            else if (value.vt == VT_UI4) {
                *EventType = value.ulVal;
                result = TRUE;
            }
            else if (value.vt == VT_I4) {
                *EventType = (ULONG)value.lVal;
                result = TRUE;
            }
        }
    }

    if (qualifierSet)
        qualifierSet->Release();

    VariantClear(&value);

    return result;
}

/*
* EtpMofSchemaRowExists
*
* Purpose:
*
* Determines whether a MOF schema row with the specified class name
* already exists for the provider.
*
*/
BOOL EtpMofSchemaRowExists(
    _In_ PPROVIDER_ENTRY Provider,
    _In_ PCWSTR ClassName
)
{
    ULONG i;

    if (!Provider || !ClassName)
        return FALSE;

    for (i = 0; i < Provider->mofSchemaRowCount; i++) {

        if (supStrCmpI(
            Provider->mofSchemaRows[i].className,
            ClassName) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/*
 * EtpGetWmiEventType
 *
 * Purpose:
 *
 * Retrieves the EventType qualifier from a WMI class and formats
 * scalar or array EventType values into a display string.
 *
 */
BOOL EtpGetWmiEventType(
    _In_ IWbemClassObject * ClassObject,
    _Out_ ULONG * EventType,
    _Out_writes_(EventTypesChars) PWSTR EventTypes,
    _In_ SIZE_T EventTypesChars
)
{
    BOOL result = FALSE;
    HRESULT hr;
    LONG index, lowerBound, upperBound;
    ULONG firstValue;
    SIZE_T remaining;
    PWSTR current;
    IWbemQualifierSet* qualifierSet = NULL;
    VARIANT value;
    SAFEARRAY* array;
    VARTYPE arrayType;
    WCHAR numberBuffer[32];

    *EventType = 0;
    EventTypes[0] = 0;

    VariantInit(&value);

    do {

        hr = ClassObject->GetQualifierSet(&qualifierSet);
        if (FAILED(hr) || !qualifierSet)
            break;

        hr = qualifierSet->Get(TEXT("EventType"),
            0,
            &value,
            NULL);

        if (FAILED(hr))
            break;

        //
        // Scalar values.
        //
        if (value.vt == VT_UI1) {

            *EventType = value.bVal;

            StringCchPrintf(EventTypes,
                EventTypesChars,
                TEXT("%u"),
                (ULONG)value.bVal);

            result = TRUE;
            break;
        }

        if (value.vt == VT_UI2) {

            *EventType = value.uiVal;

            StringCchPrintf(EventTypes,
                EventTypesChars,
                TEXT("%u"),
                (ULONG)value.uiVal);

            result = TRUE;
            break;
        }

        if (value.vt == VT_UI4) {

            *EventType = value.ulVal;

            StringCchPrintf(EventTypes,
                EventTypesChars,
                TEXT("%lu"),
                value.ulVal);

            result = TRUE;
            break;
        }

        if (value.vt == VT_I1) {

            *EventType = (ULONG)value.cVal;

            StringCchPrintf(EventTypes,
                EventTypesChars,
                TEXT("%ld"),
                (LONG)value.cVal);

            result = TRUE;
            break;
        }

        if (value.vt == VT_I2) {

            *EventType = (ULONG)value.iVal;

            StringCchPrintf(EventTypes,
                EventTypesChars,
                TEXT("%ld"),
                (LONG)value.iVal);

            result = TRUE;
            break;
        }

        if (value.vt == VT_I4) {

            *EventType = (ULONG)value.lVal;

            StringCchPrintf(EventTypes,
                EventTypesChars,
                TEXT("%ld"),
                value.lVal);

            result = TRUE;
            break;
        }

        //
        // Not an array.
        //
        if ((value.vt & VT_ARRAY) == 0)
            break;

        array = value.parray;

        if (!array)
            break;

        //
        // Determine the actual SAFEARRAY element type.
        //
        hr = SafeArrayGetVartype(array, &arrayType);
        if (FAILED(hr))
            break;

        //
        // EventType arrays should be one-dimensional.
        //
        if (SafeArrayGetDim(array) != 1)
            break;

        hr = SafeArrayGetLBound(array,
            1,
            &lowerBound);

        if (FAILED(hr))
            break;

        hr = SafeArrayGetUBound(array,
            1,
            &upperBound);

        if (FAILED(hr))
            break;

        if (upperBound < lowerBound)
            break;

        //
        // VT_I4 array.
        //
        if (arrayType == VT_I4) {

            current = EventTypes;
            remaining = EventTypesChars;
            firstValue = 0;
            result = TRUE;

            for (index = lowerBound; index <= upperBound; index++) {

                LONG element;
                SIZE_T numberLength;

                element = 0;

                hr = SafeArrayGetElement(array,
                    &index,
                    &element);

                if (FAILED(hr)) {
                    result = FALSE;
                    break;
                }

                if (index == lowerBound)
                    firstValue = (ULONG)element;

                if (index != lowerBound) {

                    if (remaining <= 2) {
                        result = FALSE;
                        break;
                    }

                    *current++ = L',';
                    *current++ = L' ';
                    *current = 0;

                    remaining -= 2;
                }

                StringCchPrintf(numberBuffer,
                    ARRAYSIZE(numberBuffer),
                    TEXT("%ld"),
                    element);

                if (FAILED(StringCchLength(numberBuffer,
                    ARRAYSIZE(numberBuffer),
                    &numberLength)))
                {
                    result = FALSE;
                    break;
                }

                if (numberLength >= remaining) {
                    result = FALSE;
                    break;
                }

                RtlCopyMemory(current,
                    numberBuffer,
                    (numberLength + 1) * sizeof(WCHAR));

                current += numberLength;
                remaining -= numberLength;
            }

            if (result)
                *EventType = firstValue;

            break;
        }

        //
        // VT_UI2 array.
        //
        if (arrayType == VT_UI2) {

            current = EventTypes;
            remaining = EventTypesChars;
            firstValue = 0;
            result = TRUE;

            for (index = lowerBound; index <= upperBound; index++) {
                USHORT element;
                SIZE_T numberLength;

                element = 0;

                hr = SafeArrayGetElement(array,
                    &index,
                    &element);

                if (FAILED(hr)) {
                    result = FALSE;
                    break;
                }

                if (index == lowerBound)
                    firstValue = element;

                if (index != lowerBound) {

                    if (remaining <= 2) {
                        result = FALSE;
                        break;
                    }

                    *current++ = L',';
                    *current++ = L' ';
                    *current = 0;

                    remaining -= 2;
                }

                StringCchPrintf(numberBuffer,
                    ARRAYSIZE(numberBuffer),
                    TEXT("%u"),
                    (ULONG)element);

                if (FAILED(StringCchLength(numberBuffer,
                    ARRAYSIZE(numberBuffer),
                    &numberLength)))
                {
                    result = FALSE;
                    break;
                }

                if (numberLength >= remaining) {
                    result = FALSE;
                    break;
                }

                RtlCopyMemory(current, numberBuffer, (numberLength + 1) * sizeof(WCHAR));

                current += numberLength;
                remaining -= numberLength;
            }

            if (result)
                *EventType = firstValue;

            break;
        }

    } while (FALSE);

    if (qualifierSet)
        qualifierSet->Release();

    VariantClear(&value);

    return result;
}

/*
* EtpProcessMofEventClass
*
* Purpose:
*
* Processes a WMI event class and adds its event type, description,
* class name, and property information to the provider's MOF schema.
*
*/
VOID EtpProcessMofEventClass(
    _In_ IWbemClassObject * ClassObject,
    _In_ PPROVIDER_ENTRY Provider
)
{
    WCHAR className[256];
    WCHAR propertyNames[600];
    PMOF_SCHEMA_ROW row;

    if (!ClassObject || !Provider)
        return;

    RtlSecureZeroMemory(className, sizeof(className));
    RtlSecureZeroMemory(propertyNames, sizeof(propertyNames));

    if (!EtpGetWmiClassName(ClassObject,
        className,
        ARRAYSIZE(className)))
    {
        return;
    }

    //
    // Do not add the same WMI event class more than once.
    //
    if (EtpMofSchemaRowExists(Provider, className))
        return;

    if (!EtpBuildMofPropertyList(ClassObject,
        propertyNames,
        ARRAYSIZE(propertyNames)))
    {
        return;
    }

    row = EtpAddMofSchemaRow(Provider);
    if (!row)
        return;

    RtlSecureZeroMemory(row, sizeof(MOF_SCHEMA_ROW));

    row->eventType = 0;

    EtpGetWmiEventType(ClassObject,
        &row->eventType,
        row->eventTypes,
        ARRAYSIZE(row->eventTypes));

    if (FAILED(StringCchCopy(row->className,
        ARRAYSIZE(row->className),
        className)))
    {
        return;
    }

    EtpGetWmiQualifierString(ClassObject,
        TEXT("Description"),
        row->description,
        ARRAYSIZE(row->description));

    StringCchCopy(row->propertyNames,
        ARRAYSIZE(row->propertyNames),
        propertyNames);
}

/*
* EtpEnumerateMofDescendants
*
* Purpose:
*
* Recursively enumerates the WMI subclasses derived from a specified
* class and processes each descendant as part of the provider's
* MOF schema.
*
*/
HRESULT EtpEnumerateMofDescendants(
    _In_ IWbemServices * Services,
    _In_ IWbemClassObject * ParentClass,
    _In_ PPROVIDER_ENTRY Provider,
    _In_ ULONG Depth,
    _Inout_ PBOOL DepthLimitReached
)
{
    HRESULT hr, childStatus;
    ULONG returned;

    VARIANT value;

    BSTR queryLanguage = NULL;
    BSTR queryString = NULL;

    IEnumWbemClassObject* enumerator = NULL;
    IWbemClassObject* childClass = NULL;

    WCHAR parentName[256];
    WCHAR query[512];

    if (!Services ||
        !ParentClass ||
        !Provider ||
        !DepthLimitReached)
    {
        return E_INVALIDARG;
    }

    if (Depth >= ETP_MAX_MOF_TEXT_DEPTH) {
        *DepthLimitReached = TRUE;
        return S_OK;
    }

    RtlSecureZeroMemory(parentName, sizeof(parentName));
    VariantInit(&value);

    //
    // Get __CLASS.
    //
    hr = ParentClass->Get(TEXT("__CLASS"),
        0,
        &value,
        NULL,
        NULL);

    if (FAILED(hr)) {
        VariantClear(&value);
        return hr;
    }

    if (value.vt != VT_BSTR ||
        value.bstrVal == NULL)
    {
        VariantClear(&value);
        return E_FAIL;
    }

    StringCchCopy(parentName, ARRAYSIZE(parentName), value.bstrVal);
    VariantClear(&value);

    //
    // Build query.
    //
    hr = StringCchPrintf(query,
        ARRAYSIZE(query),
        TEXT("SELECT * FROM meta_class WHERE __SUPERCLASS = '%s'"),
        parentName);

    if (FAILED(hr))
        return hr;

    //
    // Execute.
    //
    queryLanguage = SysAllocString(TEXT("WQL"));
    if (queryLanguage == NULL)
        return E_OUTOFMEMORY;

    queryString = SysAllocString(query);
    if (queryString == NULL) {
        SysFreeString(queryLanguage);
        return E_OUTOFMEMORY;
    }

    hr = Services->ExecQuery(queryLanguage,
        queryString,
        WBEM_FLAG_RETURN_IMMEDIATELY |
        WBEM_FLAG_FORWARD_ONLY,
        NULL,
        &enumerator);

    SysFreeString(queryString);
    SysFreeString(queryLanguage);

    if (FAILED(hr)) {
        return hr;
    }

    //
    // Enumerate children.
    //
    while (TRUE) {

        returned = 0;
        childClass = NULL;

        hr = enumerator->Next(WBEM_INFINITE,
            1,
            &childClass,
            &returned);

        if (hr == WBEM_S_FALSE ||
            returned == 0)
        {
            hr = S_OK;
            break;
        }

        if (FAILED(hr))
            break;

        EtpProcessMofEventClass(childClass, Provider);

        //
        // Always recurse.
        //
        childStatus = EtpEnumerateMofDescendants(Services,
            childClass,
            Provider,
            Depth + 1,
            DepthLimitReached);

        childClass->Release();
        childClass = NULL;

        if (FAILED(childStatus)) {
            hr = childStatus;
            break;
        }
    }

    if (childClass)
        childClass->Release();

    if (enumerator)
        enumerator->Release();

    return hr;
}

/*
* EtpWmiClassGuidMatchesProvider
*
* Purpose:
*
* Determines whether the WMI class Guid qualifier matches the specified
* provider GUID.
*
*/
BOOL EtpWmiClassGuidMatchesProvider(
    _In_ IWbemClassObject * ClassObject,
    _In_ const GUID * ProviderGuid
)
{
    HRESULT hr;
    IWbemQualifierSet* qualifierSet = NULL;
    VARIANT value;
    GUID guid;

    if (!ClassObject || !ProviderGuid)
        return FALSE;

    VariantInit(&value);

    //
    // Guid is a class qualifier, not a class property.
    //
    hr = ClassObject->GetQualifierSet(&qualifierSet);
    if (FAILED(hr) || !qualifierSet) {
        VariantClear(&value);
        return FALSE;
    }

    hr = qualifierSet->Get(TEXT("Guid"),
        0,
        &value,
        NULL);

    if (SUCCEEDED(hr) &&
        value.vt == VT_BSTR &&
        value.bstrVal != NULL)
    {
        hr = CLSIDFromString(value.bstrVal, &guid);
        if (SUCCEEDED(hr) && IsEqualGUID(guid, *ProviderGuid)) {
            qualifierSet->Release();
            VariantClear(&value);
            return TRUE;
        }
    }

    qualifierSet->Release();

    VariantClear(&value);
    return FALSE;
}

/*
* MofSchemaLoadThreadProc
*
* Purpose:
*
* Loads the selected MOF provider schema on a worker thread so WMI
* metadata enumeration does not block the main window.
*
*/
DWORD WINAPI MofSchemaLoadThreadProc(
    _In_ PVOID Parameter
)
{
    HRESULT hr;
    ULONG providerIndex;

    providerIndex = (ULONG)(ULONG_PTR)Parameter;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (SUCCEEDED(hr)) {
        EtpLoadMofProviderSchema(providerIndex);
        CoUninitialize();
    }
    else {
        if (providerIndex < g_ctx.providerCount) {
            g_ctx.providers[providerIndex].mofSchemaLoadStatus = MofSchemaWmiUnavailable;
            g_ctx.providers[providerIndex].mofSchemaLoadStatusCode = hr;
        }
    }

    InterlockedExchange(&g_ctx.mofSchemaWorkerActive, FALSE);

    if (g_ctx.hMainWnd) {
        PostMessage(g_ctx.hMainWnd, WM_APP_MOF_SCHEMA_LOADED, providerIndex, 0);
    }

    return 0;
}

/*
* EtpStartMofProviderSchemaLoad
*
* Purpose:
*
* Starts one asynchronous MOF schema discovery operation.
*
*/
BOOL EtpStartMofProviderSchemaLoad(
    _In_ ULONG ProviderIndex
)
{
    PPROVIDER_ENTRY provider;

    if (ProviderIndex >= g_ctx.providerCount)
        return FALSE;

    if (InterlockedCompareExchange(&g_ctx.mofSchemaWorkerActive,
        TRUE,
        FALSE) != FALSE)
    {
        return FALSE;
    }

    provider = &g_ctx.providers[ProviderIndex];
    provider->mofSchemaLoadStatus = MofSchemaLoading;
    provider->mofSchemaLoadStatusCode = S_OK;

    g_ctx.mofSchemaProviderIndex = ProviderIndex;

    g_ctx.mofSchemaThread = CreateThread(NULL,
        0,
        MofSchemaLoadThreadProc,
        (PVOID)(ULONG_PTR)ProviderIndex,
        0,
        NULL);

    if (!g_ctx.mofSchemaThread) {
        provider->mofSchemaLoadStatus = MofSchemaEnumerationFailed;
        provider->mofSchemaLoadStatusCode = HRESULT_FROM_WIN32(GetLastError());
        InterlockedExchange(&g_ctx.mofSchemaWorkerActive, FALSE);
        return FALSE;
    }

    return TRUE;
}

/*
* EtpWaitForMofSchemaLoad
*
* Purpose:
*
* Waits for the MOF schema worker and releases its thread handle.
* It is used before application shutdown and before freeing providers.
*
*/
VOID EtpWaitForMofSchemaLoad(
    VOID
)
{
    if (g_ctx.mofSchemaThread) {
        WaitForSingleObject(g_ctx.mofSchemaThread, INFINITE);
        CloseHandle(g_ctx.mofSchemaThread);
        g_ctx.mofSchemaThread = NULL;
    }
}

/*
* EtpLoadMofSchema
*
* Purpose:
*
* Enumerates WMI classes, identifies event classes belonging to the
* specified provider, processes their MOF schema and descendants,
* and records the resulting schema load status.
*
*/
BOOL EtpLoadMofProviderSchema(
    _In_ ULONG ProviderIndex
)
{
    BOOL depthLimitReached;
    HRESULT hr;
    ULONG returned;
    PPROVIDER_ENTRY provider;

    BSTR queryLanguage = NULL;
    BSTR queryString = NULL;

    IEnumWbemClassObject* enumerator = NULL;
    IWbemClassObject* classObject = NULL;
    IWbemServices* services;

    provider = &g_ctx.providers[ProviderIndex];

    //
    // Clear previous schema.
    //
    provider->mofSchemaRowCount = 0;
    provider->mofSchemaRowCapacity = 0;
    provider->mofSchemaLoadStatus = MofSchemaLoading;
    provider->mofSchemaLoadStatusCode = S_OK;

    if (provider->mofSchemaRows) {
        supHeapFree(provider->mofSchemaRows);
        provider->mofSchemaRows = NULL;
    }

    //
    // Connect WMI.
    //
    if (!EtpConnectWmi(&services)) {
        provider->mofSchemaLoadStatus = MofSchemaWmiUnavailable;
        provider->mofSchemaLoadStatusCode = E_FAIL;
        return FALSE;
    }

    //
    // Enumerate all WMI classes.
    //
    queryLanguage = SysAllocString(TEXT("WQL"));
    if (queryLanguage == NULL) {
        provider->mofSchemaLoadStatus = MofSchemaEnumerationFailed;
        provider->mofSchemaLoadStatusCode = E_OUTOFMEMORY;
        services->Release();
        return FALSE;
    }

    queryString = SysAllocString(TEXT("SELECT * FROM meta_class"));
    if (queryString == NULL) {
        SysFreeString(queryLanguage);
        provider->mofSchemaLoadStatus = MofSchemaEnumerationFailed;
        provider->mofSchemaLoadStatusCode = E_OUTOFMEMORY;
        services->Release();
        return FALSE;
    }

    hr = services->ExecQuery(queryLanguage,
        queryString,
        WBEM_FLAG_RETURN_IMMEDIATELY |
        WBEM_FLAG_FORWARD_ONLY,
        NULL,
        &enumerator);

    SysFreeString(queryString);
    SysFreeString(queryLanguage);

    if (FAILED(hr)) {
        provider->mofSchemaLoadStatus = MofSchemaEnumerationFailed;
        provider->mofSchemaLoadStatusCode = hr;
        services->Release();
        return FALSE;
    }

    depthLimitReached = FALSE;

    while (TRUE) {

        returned = 0;
        classObject = NULL;

        hr = enumerator->Next(WBEM_INFINITE,
            1,
            &classObject,
            &returned);

        if (FAILED(hr)) {
            break;
        }

        if (hr == WBEM_S_FALSE ||
            returned == 0)
        {
            hr = S_OK;
            break;
        }

        //
        // Test GUID.
        //
        if (EtpWmiClassGuidMatchesProvider(classObject,
            &provider->guid))
        {
            EtpProcessMofEventClass(classObject, provider);

            hr = EtpEnumerateMofDescendants(services,
                classObject,
                provider,
                0,
                &depthLimitReached);

            if (FAILED(hr)) {
                classObject->Release();
                classObject = NULL;
                break;
            }
        }

        classObject->Release();
        classObject = NULL;
    }

    if (classObject)
        classObject->Release();

    if (enumerator)
        enumerator->Release();

    if (services)
        services->Release();

    //
    // Set error state.
    //
    if (FAILED(hr)) {
        provider->mofSchemaLoadStatus = MofSchemaEnumerationFailed;
        provider->mofSchemaLoadStatusCode = hr;
        return FALSE;
    }
    if (depthLimitReached) {
        provider->mofSchemaLoadStatus = MofSchemaDepthLimitReached;
        provider->mofSchemaLoadStatusCode = S_OK;
        return TRUE;
    }

    if (provider->mofSchemaRowCount == 0) {
        provider->mofSchemaLoadStatus = MofSchemaNoEventClasses;
        provider->mofSchemaLoadStatusCode = S_OK;
        return TRUE;
    }

    provider->mofSchemaLoadStatus = MofSchemaLoaded;
    provider->mofSchemaLoadStatusCode = S_OK;
    return TRUE;
}

/*
* EtpLoadProviderSchema
*
* Purpose:
*
* Schema dispatch.
*
*/
VOID EtpLoadProviderSchema(
    _In_ ULONG ProviderIndex
)
{
    PPROVIDER_ENTRY provider;

    provider = &g_ctx.providers[ProviderIndex];
    if (provider->schemaLoaded)
        return;

    if (provider->isManifestProvider) {
        EtpLoadManifestProviderSchema(ProviderIndex);
    }
    else {
        EtpStartMofProviderSchemaLoad(ProviderIndex);
    }

    provider->schemaLoaded = TRUE;
}

/*
* EtpLoadProviderResourceInformation
*
* Purpose:
*
* Retrieve Resource/Message filename for given provider.
*
*/
_Success_(return != FALSE)
BOOL EtpLoadProviderResourceInformation(
    _In_ LPGUID ProviderGuid,
    _Out_writes_(ResourceFileNameLength) LPWSTR ResourceFileName,
    _In_ ULONG ResourceFileNameLength,
    _Out_writes_(MessageFileNameLength) LPWSTR MessageFileName,
    _In_ ULONG MessageFileNameLength
)
{
    BOOL bResult = FALSE;
    DWORD error, type, size, expandedLen;
    HKEY hKey = NULL;
    WCHAR guidString[64];
    WCHAR keyPath[256];
    WCHAR resourceBuffer[MAX_PATH];
    WCHAR messageBuffer[MAX_PATH];

    if (!ProviderGuid ||
        !ResourceFileName ||
        ResourceFileNameLength == 0 ||
        !MessageFileName ||
        MessageFileNameLength == 0)
    {
        return FALSE;
    }

    ResourceFileName[0] = 0;
    MessageFileName[0] = 0;

    RtlSecureZeroMemory(guidString, sizeof(guidString));
    RtlSecureZeroMemory(keyPath, sizeof(keyPath));
    RtlSecureZeroMemory(resourceBuffer, sizeof(resourceBuffer));
    RtlSecureZeroMemory(messageBuffer, sizeof(messageBuffer));

    do {

        if (StringFromGUID2(*ProviderGuid,
            guidString,
            ARRAYSIZE(guidString)) == 0)
        {
            break;
        }

        if (FAILED(StringCchPrintf(keyPath,
            ARRAYSIZE(keyPath),
            TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WINEVT\\Publishers\\%s"),
            guidString)))
        {
            break;
        }

        error = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            keyPath,
            0,
            KEY_QUERY_VALUE,
            &hKey);

        if (error != ERROR_SUCCESS)
            break;

        //
        // ResourceFileName
        //
        size = sizeof(resourceBuffer);
        type = 0;

        error = RegQueryValueEx(hKey,
            TEXT("ResourceFileName"),
            NULL,
            &type,
            (LPBYTE)resourceBuffer,
            &size);

        if (error == ERROR_SUCCESS &&
            (type == REG_SZ || type == REG_EXPAND_SZ))
        {
            expandedLen = ExpandEnvironmentStrings(resourceBuffer, ResourceFileName, ResourceFileNameLength);
            if (expandedLen == 0 || expandedLen > ResourceFileNameLength) {
                ResourceFileName[0] = 0;
            }
        }

        //
        // MessageFileName
        //
        size = sizeof(messageBuffer);
        type = 0;

        error = RegQueryValueEx(hKey,
            TEXT("MessageFileName"),
            NULL,
            &type,
            (LPBYTE)messageBuffer,
            &size);

        if (error == ERROR_SUCCESS &&
            (type == REG_SZ || type == REG_EXPAND_SZ))
        {
            expandedLen = ExpandEnvironmentStrings(messageBuffer, MessageFileName, MessageFileNameLength);
            if (expandedLen == 0 || expandedLen > MessageFileNameLength) {
                MessageFileName[0] = 0;
            }
        }

        bResult = TRUE;

    } while (FALSE);

    if (hKey)
        RegCloseKey(hKey);

    return bResult;
}

/*
* EtpGetMofSchemaLoadStatusName
*
* Purpose:
*
* Return MOF schema load status as text.
*
*/
LPCWSTR EtpGetMofSchemaLoadStatusName(
    _In_ MOF_SCHEMA_LOAD_STATUS Status
)
{
    switch (Status) {
    case MofSchemaLoading:
        return TEXT("Loading");

    case MofSchemaLoaded:
        return TEXT("Loaded");

    case MofSchemaWmiUnavailable:
        return TEXT("WMI unavailable");

    case MofSchemaEnumerationFailed:
        return TEXT("Enumeration failed");

    case MofSchemaNoEventClasses:
        return TEXT("No event classes");

    case MofSchemaDepthLimitReached:
        return TEXT("Depth limit reached");

    default:
        return TEXT("Unknown");
    }
}
