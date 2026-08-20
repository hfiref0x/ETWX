/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       ETWMAN.CPP
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
*  ETW manifest metadata reconstruction (best-effort).
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"

typedef struct _PROVIDER_OPCODE {
    ULONG Value;
    LPCWSTR Name;
} PROVIDER_OPCODE, * PPROVIDER_OPCODE;

typedef struct _TASK_OPCODE {
    WCHAR OpcodeName[128];
    ULONG Value;
} TASK_OPCODE, * PTASK_OPCODE;

typedef struct _TASK_OPCODES {
    WCHAR TaskName[128];
    TASK_OPCODE Opcodes[64];
    ULONG OpcodeCount;
} TASK_OPCODES, * PTASK_OPCODES;

typedef struct _UNIQUE_MAPS {
    WCHAR Name[256];
    EVENT_DESCRIPTOR Descriptor;
} UNIQUE_MAPS, * PUNIQUE_MAPS;

UNIQUE_MAPS g_uniqueMaps[256];
UNIQUE_MAPS g_uniqueMapsManifestXml[256];

/*
* EtmpGetManifestEventInformation
*
* Purpose:
*
* Retrieves the manifest event information for the specified ETW
* provider event descriptor and returns the allocated event information
* buffer to the caller.
*
*/
BOOL EtmpGetManifestEventInformation(
    _In_ LPGUID ProviderGuid,
    _In_ const EVENT_DESCRIPTOR* Descriptor,
    _Out_ PTRACE_EVENT_INFO* EventInfo
)
{
    BOOL bResult = FALSE;
    ULONG status, bufferSize;
    PTRACE_EVENT_INFO eventInfo = NULL;

    if (!ProviderGuid || !Descriptor || !EventInfo) {
        return FALSE;
    }

    *EventInfo = NULL;
    bufferSize = 0;

    do {

        status = TdhGetManifestEventInformation(ProviderGuid,
            (PEVENT_DESCRIPTOR)Descriptor,
            NULL,
            &bufferSize);

        if (status != ERROR_INSUFFICIENT_BUFFER)
            break;

        eventInfo = (PTRACE_EVENT_INFO)supHeapAlloc(bufferSize);
        if (!eventInfo)
            break;

        status = TdhGetManifestEventInformation(ProviderGuid,
            (PEVENT_DESCRIPTOR)Descriptor,
            eventInfo,
            &bufferSize);

        bResult = (status == ERROR_SUCCESS);

    } while (FALSE);

    if (!bResult) {
        if (eventInfo)
            supHeapFree(eventInfo);
        return FALSE;
    }

    *EventInfo = eventInfo;
    return TRUE;
}

/*
* EtmpAreManifestEventTemplatesEqual
*
* Purpose:
*
* Compares the event property metadata of two manifest events and
* determines whether their event templates are equivalent.
*
*/
BOOL EtmpAreManifestEventTemplatesEqual(
    _In_ PTRACE_EVENT_INFO EventInfo1,
    _In_ PTRACE_EVENT_INFO EventInfo2
)
{
    ULONG i;
    PCEVENT_PROPERTY_INFO propertyInfo1, propertyInfo2;
    LPCWSTR propertyName1, propertyName2;
    LPCWSTR mapName1, mapName2;

    if (EventInfo1 == NULL || EventInfo2 == NULL)
        return FALSE;

    if (EventInfo1->TopLevelPropertyCount != EventInfo2->TopLevelPropertyCount)
        return FALSE;

    for (i = 0; i < EventInfo1->TopLevelPropertyCount; i++) {
        propertyInfo1 = &EventInfo1->EventPropertyInfoArray[i];
        propertyInfo2 = &EventInfo2->EventPropertyInfoArray[i];

        if (propertyInfo1->Flags != propertyInfo2->Flags)
            return FALSE;

        if (propertyInfo1->nonStructType.InType != propertyInfo2->nonStructType.InType)
            return FALSE;

        if (propertyInfo1->nonStructType.OutType != propertyInfo2->nonStructType.OutType)
            return FALSE;

        if (propertyInfo1->countPropertyIndex != propertyInfo2->countPropertyIndex)
            return FALSE;

        if (propertyInfo1->lengthPropertyIndex != propertyInfo2->lengthPropertyIndex)
            return FALSE;

        propertyName1 = propertyInfo1->NameOffset ?
            (LPCWSTR)RtlOffsetToPointer(EventInfo1, propertyInfo1->NameOffset) : NULL;
        propertyName2 = propertyInfo2->NameOffset ?
            (LPCWSTR)RtlOffsetToPointer(EventInfo2, propertyInfo2->NameOffset) : NULL;

        if (!propertyName1 || !propertyName2) {
            if (propertyName1 != propertyName2)
                return FALSE;
        }
        else if (supStrCmp(propertyName1, propertyName2) != 0) {
            return FALSE;
        }

        mapName1 = propertyInfo1->nonStructType.MapNameOffset ?
            (LPCWSTR)RtlOffsetToPointer(EventInfo1, propertyInfo1->nonStructType.MapNameOffset) : NULL;
        mapName2 = propertyInfo2->nonStructType.MapNameOffset ?
            (LPCWSTR)RtlOffsetToPointer(EventInfo2, propertyInfo2->nonStructType.MapNameOffset) : NULL;

        if (!mapName1 || !mapName2) {
            if (mapName1 != mapName2)
                return FALSE;
        }
        else if (supStrCmp(mapName1, mapName2) != 0) {
            return FALSE;
        }
    }

    return TRUE;
}

/*
* EtmpFindManifestTemplateEvent
*
* Purpose:
*
* Finds a previously encountered event with an equivalent manifest
* event template and returns its index. If no matching event is found,
* the supplied event index is retained as the template event index.
*
*/
BOOL EtmpFindManifestTemplateEvent(
    _In_ LPGUID ProviderGuid,
    _In_ PEVENT_DESCRIPTOR EventDescriptors,
    _In_ ULONG EventIndex,
    _Out_ PULONG TemplateEventIndex
)
{
    BOOL bResult = TRUE;
    ULONG i;
    PTRACE_EVENT_INFO eventInfo = NULL, candidateInfo = NULL;

    if (ProviderGuid == NULL ||
        EventDescriptors == NULL ||
        TemplateEventIndex == NULL)
    {
        return FALSE;
    }

    *TemplateEventIndex = EventIndex;

    do {

        if (!EtmpGetManifestEventInformation(ProviderGuid, &EventDescriptors[EventIndex], &eventInfo)) {
            bResult = FALSE;
            break;
        }

        if (eventInfo->TopLevelPropertyCount == 0) {
            break;
        }

        for (i = 0; i < EventIndex; i++) {
            candidateInfo = NULL;
            if (!EtmpGetManifestEventInformation(ProviderGuid, &EventDescriptors[i], &candidateInfo))
                continue;

            if (candidateInfo->TopLevelPropertyCount != 0 &&
                EtmpAreManifestEventTemplatesEqual(eventInfo, candidateInfo))
            {
                *TemplateEventIndex = i;
                break;
            }

            supHeapFree(candidateInfo);
            candidateInfo = NULL;
        }

    } while (FALSE);

    if (candidateInfo)
        supHeapFree(candidateInfo);

    if (eventInfo)
        supHeapFree(eventInfo);

    return bResult;
}

/*
* EtmpEndsWithIgnoreCase
*
* Purpose:
*
* Determines whether the specified string ends with the supplied
* suffix using a case-insensitive comparison.
*
*/
BOOL EtmpEndsWithIgnoreCase(
    _In_ LPCWSTR str,
    _In_ LPCWSTR suffix
)
{
    WCHAR c1, c2;
    ULONG strLen, suffixLen;
    LPCWSTR pStr, pSuffix;

    if (str == NULL || suffix == NULL)
        return FALSE;

    strLen = (ULONG)supStrLen(str);
    suffixLen = (ULONG)supStrLen(suffix);
    if (suffixLen > strLen)
        return FALSE;

    pStr = str + strLen - suffixLen;
    pSuffix = suffix;

    while (*pSuffix != L'\0') {
        c1 = *pStr;
        c2 = *pSuffix;

        if (c1 >= L'A' && c1 <= L'Z')
            c1 += 32;

        if (c2 >= L'A' && c2 <= L'Z')
            c2 += 32;

        if (c1 != c2)
            return FALSE;

        pStr++;
        pSuffix++;
    }

    return TRUE;
}

/*
* EtmpBuildManifestMapsXml
*
* Purpose:
*
* Enumerates the event properties of the specified ETW provider, identifies
* the event maps referenced by those properties, and appends XML definitions
* for the unique maps to the supplied text buffer.
*
* The function supports both explicitly named maps and maps discovered from
* property names when the map name is not present in the event metadata.
* Each map is queried through TDH and emitted as either a valueMap or bitMap
* containing its values and corresponding localization string references.
*
* Returns:
*
* TRUE if the map definitions were successfully generated and appended to
* the text buffer; otherwise FALSE.
*
*/
BOOL EtmpBuildManifestMapsXml(
    _In_ LPGUID ProviderGuid,
    _In_ PEVENT_DESCRIPTOR EventDescriptors,
    _In_ ULONG EventCount,
    _Inout_ PTEXT_BUFFER TextBuffer
)
{
    ULONG i, j, k, e;
    ULONG status, bufferSize;
    PTRACE_EVENT_INFO eventInfo;
    PCEVENT_PROPERTY_INFO propInfo;
    LPCWSTR mapName;
    LPCWSTR propName;
    PEVENT_MAP_INFO mapInfo;
    PEVENT_MAP_ENTRY mapEntry;
    LPCWSTR message;
    WCHAR stringId[1024];
    WCHAR valueBuffer[32];
    WCHAR guessedMapName[256];
    EVENT_RECORD dummy;

    ULONG uniqueMapCount = 0;

    if (ProviderGuid == NULL ||
        EventDescriptors == NULL ||
        TextBuffer == NULL)
    {
        return FALSE;
    }

    RtlSecureZeroMemory(&g_uniqueMapsManifestXml, sizeof(g_uniqueMapsManifestXml));

    for (i = 0; i < EventCount; i++) {
        eventInfo = NULL;
        if (!EtmpGetManifestEventInformation(ProviderGuid,
            &EventDescriptors[i],
            &eventInfo))
        {
            continue;
        }

        for (j = 0; j < eventInfo->PropertyCount; j++) {

            propInfo = &eventInfo->EventPropertyInfoArray[j];
            mapName = NULL;
            propName = propInfo->NameOffset ? (LPCWSTR)RtlOffsetToPointer(eventInfo, propInfo->NameOffset) : NULL;

            if (propInfo->nonStructType.MapNameOffset != 0) {

                LPCWSTR tempName = (LPCWSTR)RtlOffsetToPointer(eventInfo, propInfo->nonStructType.MapNameOffset);
                if (tempName && tempName[0] != L'\0') {
                    StringCchCopy(g_uniqueMapsManifestXml[uniqueMapCount].Name,
                        ARRAYSIZE(g_uniqueMapsManifestXml[uniqueMapCount].Name),
                        tempName);

                    mapName = g_uniqueMapsManifestXml[uniqueMapCount].Name;
                }
            }
            else if (propName && propName[0] != L'\0') {

                StringCchPrintf(guessedMapName,
                    ARRAYSIZE(guessedMapName),
                    TEXT("%sMap"),
                    propName);

                ULONG testSize = 0;

                RtlSecureZeroMemory(&dummy, sizeof(dummy));
                dummy.EventHeader.ProviderId = *ProviderGuid;
                dummy.EventHeader.EventDescriptor = EventDescriptors[i];

                if (TdhGetEventMapInformation(&dummy,
                    (PWSTR)propName,
                    NULL, &testSize) == ERROR_INSUFFICIENT_BUFFER)
                {
                    StringCchCopy(g_uniqueMapsManifestXml[uniqueMapCount].Name,
                        ARRAYSIZE(g_uniqueMapsManifestXml[uniqueMapCount].Name),
                        propName);

                    mapName = g_uniqueMapsManifestXml[uniqueMapCount].Name;
                }
                else if (TdhGetEventMapInformation(&dummy,
                    guessedMapName,
                    NULL,
                    &testSize) == ERROR_INSUFFICIENT_BUFFER)
                {
                    StringCchCopy(g_uniqueMapsManifestXml[uniqueMapCount].Name,
                        ARRAYSIZE(g_uniqueMapsManifestXml[uniqueMapCount].Name),
                        guessedMapName);

                    mapName = g_uniqueMapsManifestXml[uniqueMapCount].Name;
                }
            }

            if (mapName && mapName[0] != L'\0') {
                BOOL found = FALSE;
                for (k = 0; k < uniqueMapCount; k++) {
                    if (supStrCmp(g_uniqueMapsManifestXml[k].Name, mapName) == 0) {
                        found = TRUE;
                        break;
                    }
                }
                if (!found && uniqueMapCount < 256) {
                    g_uniqueMapsManifestXml[uniqueMapCount].Descriptor = EventDescriptors[i];
                    uniqueMapCount++;
                }
            }
        }

        supHeapFree(eventInfo);
    }

    if (uniqueMapCount == 0)
        return TRUE;

    if (!supTbAppend(TextBuffer, TEXT("        <maps>\r\n")))
        return FALSE;

    for (k = 0; k < uniqueMapCount; k++) {
        mapName = g_uniqueMapsManifestXml[k].Name;
        bufferSize = 0;

        RtlSecureZeroMemory(&dummy, sizeof(dummy));
        dummy.EventHeader.ProviderId = *ProviderGuid;
        dummy.EventHeader.EventDescriptor = g_uniqueMapsManifestXml[k].Descriptor;

        status = TdhGetEventMapInformation(&dummy,
            (PWSTR)mapName,
            NULL,
            &bufferSize);

        if (status != ERROR_INSUFFICIENT_BUFFER)
            continue;

        mapInfo = (PEVENT_MAP_INFO)supHeapAlloc(bufferSize);
        if (!mapInfo)
            continue;

        status = TdhGetEventMapInformation(&dummy,
            (PWSTR)mapName,
            mapInfo,
            &bufferSize);

        if (status == ERROR_SUCCESS) {

            LPCWSTR mapTag = (mapInfo->Flag & 2) ? TEXT("bitMap") : TEXT("valueMap");

            if (!supTbAppend(TextBuffer, TEXT("          <")) ||
                !supTbAppend(TextBuffer, mapTag) ||
                !supTbAppend(TextBuffer, TEXT(" name=\"")) ||
                !supTbAppend(TextBuffer, mapName) ||
                !supTbAppend(TextBuffer, TEXT("\">\r\n")))
            {
                supHeapFree(mapInfo); return FALSE;
            }

            for (e = 0; e < mapInfo->EntryCount; e++) {
                mapEntry = &mapInfo->MapEntryArray[e];
                message = (LPCWSTR)RtlOffsetToPointer(mapInfo, mapEntry->OutputOffset);

                StringCchPrintf(stringId,
                    ARRAYSIZE(stringId),
                    TEXT("map_%s%s"),
                    mapName,
                    message);

                StringCchPrintf(valueBuffer,
                    ARRAYSIZE(valueBuffer),
                    TEXT("0x%X"),
                    mapEntry->Value);

                if (!supTbAppend(TextBuffer, TEXT("            <map value=\"")) ||
                    !supTbAppend(TextBuffer, valueBuffer) ||
                    !supTbAppend(TextBuffer, TEXT("\" message=\"$(string.")) ||
                    !supTbAppend(TextBuffer, stringId) ||
                    !supTbAppend(TextBuffer, TEXT(")\" />\r\n")))
                {
                    supHeapFree(mapInfo);
                    return FALSE;
                }
            }

            if (!supTbAppend(TextBuffer, TEXT("          </")) ||
                !supTbAppend(TextBuffer, mapTag) ||
                !supTbAppend(TextBuffer, TEXT(">\r\n")))
            {
                supHeapFree(mapInfo);
                return FALSE;
            }
        }
        supHeapFree(mapInfo);
    }

    if (!supTbAppend(TextBuffer, TEXT("        </maps>\r\n")))
        return FALSE;

    return TRUE;
}

/*
* EtmpBuildManifestEventXml
*
* Purpose:
*
* Builds the XML representation of a single ETW event definition from the
* supplied TRACE_EVENT_INFO structure and returns the generated XML string
* to the caller.
* 
* The resulting XML is allocated through the text buffer and
* returned through EventXml; the caller is responsible for freeing it with
* supHeapFree.
*
* Returns:
*
* TRUE if the event XML was successfully generated; otherwise FALSE.
*
*/
BOOL EtmpBuildManifestEventXml(
    _In_ PTRACE_EVENT_INFO EventInfo,
    _In_opt_ LPCWSTR TemplateName,
    _In_opt_ LPCWSTR KeywordName,
    _In_opt_ LPCWSTR SymbolOverride,
    _In_opt_ LPCWSTR OpcodeNameOverride,
    _Out_ PWSTR* EventXml
)
{
    TEXT_BUFFER textBuffer;
    LPCWSTR taskName;
    LPCWSTR levelName;
    WCHAR value[32];

    if (EventInfo == NULL || EventXml == NULL)
        return FALSE;

    *EventXml = NULL;
    RtlSecureZeroMemory(&textBuffer, sizeof(textBuffer));
    if (!supTbInitialize(&textBuffer, 1024))
        return FALSE;

    taskName = NULL;
    levelName = NULL;

    if (EventInfo->TaskNameOffset)
        taskName = (LPCWSTR)RtlOffsetToPointer(EventInfo, EventInfo->TaskNameOffset);

    WCHAR genTaskName[64];
    if (EventInfo->EventDescriptor.Task == 0 && (!taskName || taskName[0] == L'\0')) {
        if (SUCCEEDED(StringCchPrintf(genTaskName,
            ARRAYSIZE(genTaskName),
            TEXT("task_0"))))
        {
            taskName = genTaskName;
        }
    }

    switch (EventInfo->EventDescriptor.Level) {
    case TRACE_LEVEL_CRITICAL: levelName = TEXT("win:Critical"); break;
    case TRACE_LEVEL_ERROR: levelName = TEXT("win:Error"); break;
    case TRACE_LEVEL_WARNING: levelName = TEXT("win:Warning"); break;
    case TRACE_LEVEL_INFORMATION: levelName = TEXT("win:Informational"); break;
    case TRACE_LEVEL_VERBOSE: levelName = TEXT("win:Verbose"); break;
    }

    if (FAILED(StringCchPrintf(value,
        ARRAYSIZE(value),
        TEXT("%u"),
        EventInfo->EventDescriptor.Id)))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer, TEXT("          <event value=\"")) ||
        !supTbAppend(&textBuffer, value) ||
        !supTbAppend(&textBuffer, TEXT("\" symbol=\"")) ||
        !supTbAppend(&textBuffer, SymbolOverride ? SymbolOverride : TEXT("Unknown")))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (FAILED(StringCchPrintf(value,
        ARRAYSIZE(value),
        TEXT("%u"),
        EventInfo->EventDescriptor.Version)))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer, TEXT("\" version=\"")) ||
        !supTbAppend(&textBuffer, value))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (taskName && taskName[0] != L'\0') {
        if (!supTbAppend(&textBuffer, TEXT("\" task=\"")) ||
            !supTbAppend(&textBuffer, taskName))
        {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }

    LPCWSTR opcodeId = NULL;
    switch (EventInfo->EventDescriptor.Opcode) {
    case EVENT_TRACE_TYPE_START: opcodeId = TEXT("win:Start"); break;
    case EVENT_TRACE_TYPE_END: opcodeId = TEXT("win:Stop"); break;
    case EVENT_TRACE_TYPE_DC_START: opcodeId = TEXT("win:DC_Start"); break;
    case EVENT_TRACE_TYPE_DC_END: opcodeId = TEXT("win:DC_Stop"); break;
    case EVENT_TRACE_TYPE_EXTENSION: opcodeId = TEXT("win:Extension"); break;
    case EVENT_TRACE_TYPE_REPLY: opcodeId = TEXT("win:Reply"); break;
    case EVENT_TRACE_TYPE_DEQUEUE: opcodeId = TEXT("win:Resume"); break;
    case EVENT_TRACE_TYPE_CHECKPOINT: opcodeId = TEXT("win:Checkpoint"); break;
    default:
        if (EventInfo->EventDescriptor.Opcode != EVENT_TRACE_TYPE_INFO) {
            opcodeId = OpcodeNameOverride;
        }
        break;
    }

    if (opcodeId) {
        if (!supTbAppend(&textBuffer, TEXT("\" opcode=\"")) ||
            !supTbAppend(&textBuffer, opcodeId))
        {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }

    if (levelName) {
        if (!supTbAppend(&textBuffer, TEXT("\" level=\"")) ||
            !supTbAppend(&textBuffer, levelName))
        {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }

    if (KeywordName && KeywordName[0] != L'\0') {
        if (!supTbAppend(&textBuffer, TEXT("\" keywords=\"")) ||
            !supTbAppend(&textBuffer, KeywordName))
        {
            supTbDestroy(&textBuffer); return FALSE;
        }
    }

    if (TemplateName && TemplateName[0] != L'\0') {
        if (!supTbAppend(&textBuffer, TEXT("\" template=\"")) ||
            !supTbAppend(&textBuffer, TemplateName))
        {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }

    if (!supTbAppend(&textBuffer, TEXT("\" />\r\n"))) {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    *EventXml = textBuffer.Buffer;
    textBuffer.Buffer = NULL;
    supTbDestroy(&textBuffer);
    return TRUE;
}

/*
* EtmpBuildManifestKeywordsXml
*
* Purpose:
*
* Enumerates the keyword metadata registered for the specified ETW provider
* and appends the corresponding keyword definitions to the supplied
* instrumentation manifest text buffer.
*
* Returns:
*
* TRUE if the keyword definitions were successfully generated and appended
* to the text buffer; otherwise FALSE.
*
*/
BOOL EtmpBuildManifestKeywordsXml(
    _In_ LPGUID ProviderGuid,
    _Inout_ PTEXT_BUFFER TextBuffer
)
{
    ULONG status;
    ULONG bufferSize;
    PPROVIDER_FIELD_INFOARRAY fieldInfo;
    PPROVIDER_FIELD_INFO fieldInfoEntry;
    ULONG i;
    ULONG keywordCount;
    WCHAR stringId[512];
    WCHAR maskBuffer[32];

    if (ProviderGuid == NULL || TextBuffer == NULL)
        return FALSE;

    fieldInfo = NULL;
    bufferSize = 0;

    status = TdhEnumerateProviderFieldInformation(ProviderGuid,
        EventKeywordInformation,
        NULL,
        &bufferSize);

    if (status != ERROR_INSUFFICIENT_BUFFER)
        return TRUE;

    fieldInfo = (PPROVIDER_FIELD_INFOARRAY)supHeapAlloc(bufferSize);
    if (!fieldInfo)
        return FALSE;

    status = TdhEnumerateProviderFieldInformation(ProviderGuid,
        EventKeywordInformation,
        fieldInfo,
        &bufferSize);

    if (status != ERROR_SUCCESS) {
        supHeapFree(fieldInfo);
        return FALSE;
    }

    fieldInfoEntry = fieldInfo->FieldInfoArray;
    keywordCount = 0;

    for (i = 0; i < fieldInfo->NumberOfElements; i++) {
        if (fieldInfoEntry[i].NameOffset == 0)
            continue;
        if (fieldInfoEntry[i].Value == 0x8000000000000000ULL) // Skip the reserved high-bit keyword mask
            continue;
        keywordCount++;
    }

    if (keywordCount != 0) {
        if (!supTbAppend(TextBuffer, TEXT("        <keywords>\r\n"))) {
            supHeapFree(fieldInfo);
            return FALSE;
        }

        for (i = 0; i < fieldInfo->NumberOfElements; i++) {
            if (fieldInfoEntry[i].NameOffset == 0)
                continue;
            if (fieldInfoEntry[i].Value == 0x8000000000000000ULL) // Skip the reserved high-bit keyword mask
                continue;

            if (FAILED(StringCchPrintf(stringId,
                ARRAYSIZE(stringId),
                TEXT("keyword_%s"),
                (PWSTR)RtlOffsetToPointer(fieldInfo, fieldInfoEntry[i].NameOffset))))
            {
                supHeapFree(fieldInfo);
                return FALSE;
            }

            if (FAILED(StringCchPrintf(maskBuffer,
                ARRAYSIZE(maskBuffer),
                TEXT("0x%llX"),
                fieldInfoEntry[i].Value)))
            {
                supHeapFree(fieldInfo);
                return FALSE;
            }

            if (!supTbAppend(TextBuffer, TEXT("          <keyword name=\"")) ||
                !supTbAppend(TextBuffer, (PWSTR)RtlOffsetToPointer(fieldInfo, fieldInfoEntry[i].NameOffset)) ||
                !supTbAppend(TextBuffer, TEXT("\" message=\"$(string.")) ||
                !supTbAppend(TextBuffer, stringId) ||
                !supTbAppend(TextBuffer, TEXT(")\" mask=\"")) ||
                !supTbAppend(TextBuffer, maskBuffer) ||
                !supTbAppend(TextBuffer, TEXT("\" />\r\n")))
            {
                supHeapFree(fieldInfo);
                return FALSE;
            }
        }

        if (!supTbAppend(TextBuffer, TEXT("        </keywords>\r\n"))) {
            supHeapFree(fieldInfo);
            return FALSE;
        }
    }
    else {
        if (!supTbAppend(TextBuffer, TEXT("        <keywords></keywords>\r\n"))) {
            supHeapFree(fieldInfo);
            return FALSE;
        }
    }

    supHeapFree(fieldInfo);
    return TRUE;
}

/*
* EtmpBuildManifestLocalizationXml
*
* Purpose:
*
* Builds the localization section of an ETW provider instrumentation
* manifest and returns the generated XML through LocalizationXml.
*
* Returns:
*
* TRUE if the localization XML was successfully generated; otherwise FALSE.
*
*/
BOOL EtmpBuildManifestLocalizationXml(
    _In_ LPGUID ProviderGuid,
    _In_ PEVENT_DESCRIPTOR EventDescriptors,
    _In_ ULONG EventCount,
    _Out_ PWSTR* LocalizationXml,
    _In_ TASK_OPCODES* TaskOpcodesList,
    _In_ ULONG TaskOpcodesCount
)
{
    ULONG status;
    ULONG bufferSize;
    PPROVIDER_FIELD_INFOARRAY fieldInfo;
    ULONG i, j, p;
    PTRACE_EVENT_INFO eventInfo;
    TEXT_BUFFER textBuffer;
    LPCWSTR fieldName;
    LPCWSTR taskName;
    BOOL duplicate;
    BOOL isGeneratedTaskName;
    ULONG uniqueMapCount;
    PCEVENT_PROPERTY_INFO propInfo;
    LPCWSTR mapName, propName;
    PEVENT_MAP_INFO mapInfo;
    PEVENT_MAP_ENTRY mapEntry;
    LPCWSTR mapMessage;
    ULONG mapBufferSize, mapStatus;
    EVENT_RECORD dummyEvent;
    WCHAR guessedMapName[256];
    WCHAR mapStringId[1024];
    WCHAR generatedTaskName[64];
    WCHAR stringId[512];

    if (ProviderGuid == NULL ||
        (EventCount != 0 && EventDescriptors == NULL) ||
        LocalizationXml == NULL)
    {
        return FALSE;
    }

    *LocalizationXml = NULL;
    RtlSecureZeroMemory(&textBuffer, sizeof(textBuffer));
    if (!supTbInitialize(&textBuffer, 2048))
        return FALSE;

    RtlSecureZeroMemory(g_uniqueMaps, sizeof(g_uniqueMaps));

    fieldInfo = NULL;
    bufferSize = 0;
    status = TdhEnumerateProviderFieldInformation(ProviderGuid,
        EventKeywordInformation,
        NULL,
        &bufferSize);

    if (status == ERROR_INSUFFICIENT_BUFFER) {
        fieldInfo = (PPROVIDER_FIELD_INFOARRAY)supHeapAlloc(bufferSize);
        if (!fieldInfo) {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
        status = TdhEnumerateProviderFieldInformation(ProviderGuid,
            EventKeywordInformation,
            fieldInfo,
            &bufferSize);

        if (status != ERROR_SUCCESS) {
            supHeapFree(fieldInfo);
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }

    if (!supTbAppend(&textBuffer, TEXT("  <localization>\r\n")
        TEXT("    <resources culture=\"en-US\">\r\n")
        TEXT("      <stringTable>\r\n")))
    {
        if (fieldInfo)
            supHeapFree(fieldInfo);

        supTbDestroy(&textBuffer);
        return FALSE;
    }

    // 1. Keywords Localization
    if (fieldInfo) {
        for (i = 0; i < fieldInfo->NumberOfElements; i++) {

            fieldName = (LPCWSTR)RtlOffsetToPointer(fieldInfo, fieldInfo->FieldInfoArray[i].NameOffset);
            if (fieldName[0] == L'\0')
                continue;

            if (FAILED(StringCchPrintf(stringId,
                ARRAYSIZE(stringId),
                TEXT("keyword_%s"),
                fieldName)))
            {
                supHeapFree(fieldInfo);
                supTbDestroy(&textBuffer);
                return FALSE;
            }
            if (!supTbAppend(&textBuffer, TEXT("        <string id=\"")) ||
                !supTbAppend(&textBuffer, stringId) ||
                !supTbAppend(&textBuffer, TEXT("\" value=\"")) ||
                !supTbAppend(&textBuffer, fieldName) ||
                !supTbAppend(&textBuffer, TEXT("\" />\r\n")))
            {
                supHeapFree(fieldInfo);
                supTbDestroy(&textBuffer);
                return FALSE;
            }
        }
        supHeapFree(fieldInfo);
        fieldInfo = NULL;
    }

    // 2. Maps Localization (with fallback)
    uniqueMapCount = 0;
    for (i = 0; i < EventCount; i++) {

        eventInfo = NULL;
        if (!EtmpGetManifestEventInformation(ProviderGuid,
            &EventDescriptors[i],
            &eventInfo))
        {
            continue;
        }

        for (p = 0; p < eventInfo->PropertyCount; p++) {
            propInfo = &eventInfo->EventPropertyInfoArray[p];
            mapName = NULL;
            propName = propInfo->NameOffset ? (LPCWSTR)RtlOffsetToPointer(eventInfo, propInfo->NameOffset) : NULL;

            if (propInfo->nonStructType.MapNameOffset != 0) {
                LPCWSTR tempName = (LPCWSTR)RtlOffsetToPointer(eventInfo, propInfo->nonStructType.MapNameOffset);
                if (tempName && tempName[0] != L'\0') {
                    mapName = tempName;
                }
            }
            else if (propName && propName[0] != L'\0') {

                StringCchPrintf(guessedMapName,
                    ARRAYSIZE(guessedMapName),
                    TEXT("%sMap"),
                    propName);

                ULONG testSize = 0;

                RtlSecureZeroMemory(&dummyEvent, sizeof(dummyEvent));
                dummyEvent.EventHeader.ProviderId = *ProviderGuid;
                dummyEvent.EventHeader.EventDescriptor = EventDescriptors[i];

                if (TdhGetEventMapInformation(&dummyEvent,
                    (PWSTR)propName,
                    NULL,
                    &testSize) == ERROR_INSUFFICIENT_BUFFER)
                {
                    mapName = propName;
                }
                else if (TdhGetEventMapInformation(&dummyEvent,
                    guessedMapName,
                    NULL,
                    &testSize) == ERROR_INSUFFICIENT_BUFFER)
                {
                    mapName = guessedMapName;
                }
            }

            if (mapName && mapName[0] != L'\0') {
                BOOL found = FALSE;
                for (j = 0; j < uniqueMapCount; j++) {
                    if (supStrCmp(g_uniqueMaps[j].Name, mapName) == 0) {
                        found = TRUE;
                        break;
                    }
                }
                if (!found && uniqueMapCount < 256) {
                    StringCchCopy(g_uniqueMaps[uniqueMapCount].Name,
                        ARRAYSIZE(g_uniqueMaps[uniqueMapCount].Name),
                        mapName);
                    g_uniqueMaps[uniqueMapCount].Descriptor = EventDescriptors[i];
                    uniqueMapCount++;
                }
            }
        }
        supHeapFree(eventInfo);
    }

    for (i = 0; i < uniqueMapCount; i++) {
        mapName = g_uniqueMaps[i].Name;
        mapBufferSize = 0;

        RtlSecureZeroMemory(&dummyEvent, sizeof(dummyEvent));
        dummyEvent.EventHeader.ProviderId = *ProviderGuid;
        dummyEvent.EventHeader.EventDescriptor = g_uniqueMaps[i].Descriptor;

        mapStatus = TdhGetEventMapInformation(&dummyEvent,
            (PWSTR)mapName,
            NULL,
            &mapBufferSize);

        if (mapStatus != ERROR_INSUFFICIENT_BUFFER)
            continue;

        mapInfo = (PEVENT_MAP_INFO)supHeapAlloc(mapBufferSize);
        if (!mapInfo)
            continue;

        mapStatus = TdhGetEventMapInformation(&dummyEvent,
            (PWSTR)mapName,
            mapInfo,
            &mapBufferSize);

        if (mapStatus == ERROR_SUCCESS) {

            for (p = 0; p < mapInfo->EntryCount; p++) {

                mapEntry = &mapInfo->MapEntryArray[p];
                mapMessage = (LPCWSTR)RtlOffsetToPointer(mapInfo, mapEntry->OutputOffset);

                if (SUCCEEDED(StringCchPrintf(mapStringId,
                    ARRAYSIZE(mapStringId),
                    TEXT("map_%s%s"),
                    mapName,
                    mapMessage)))
                {

                    if (!supTbAppend(&textBuffer, TEXT("        <string id=\"")) ||
                        !supTbAppend(&textBuffer, mapStringId) ||
                        !supTbAppend(&textBuffer, TEXT("\" value=\"")) ||
                        !supTbAppend(&textBuffer, mapMessage) ||
                        !supTbAppend(&textBuffer, TEXT("\" />\r\n")))
                    {
                        supHeapFree(mapInfo);
                        supTbDestroy(&textBuffer);
                        return FALSE;
                    }
                }
            }
        }

        supHeapFree(mapInfo);
    }

    // 3. Opcodes Localization
    for (ULONG t = 0; t < TaskOpcodesCount; t++) {

        LPCWSTR tName = TaskOpcodesList[t].TaskName;

        for (ULONG o = 0; o < TaskOpcodesList[t].OpcodeCount; o++) {

            LPCWSTR opName = TaskOpcodesList[t].Opcodes[o].OpcodeName;
            WCHAR opStringId[512];

            StringCchPrintf(opStringId,
                ARRAYSIZE(opStringId),
                TEXT("opcode_%s%s"),
                tName,
                opName);

            if (!supTbAppend(&textBuffer, TEXT("        <string id=\"")) ||
                !supTbAppend(&textBuffer, opStringId) ||
                !supTbAppend(&textBuffer, TEXT("\" value=\"")) ||
                !supTbAppend(&textBuffer, opName) ||
                !supTbAppend(&textBuffer, TEXT("\" />\r\n")))
            {
                supTbDestroy(&textBuffer);
                return FALSE;
            }
        }
    }

    // 4. Tasks Localization
    for (i = 0; i < EventCount; i++) {

        duplicate = FALSE;
        for (j = 0; j < i; j++) {
            if (EventDescriptors[j].Task == EventDescriptors[i].Task) {
                duplicate = TRUE;
                break;
            }
        }

        if (duplicate)
            continue;

        eventInfo = NULL;
        if (!EtmpGetManifestEventInformation(ProviderGuid,
            &EventDescriptors[i],
            &eventInfo))
        {
            supTbDestroy(&textBuffer);
            return FALSE;
        }

        taskName = NULL;
        isGeneratedTaskName = FALSE;

        if (eventInfo->TaskNameOffset != 0)
            taskName = (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->TaskNameOffset);

        if (EventDescriptors[i].Task == 0) {
            if (!taskName || taskName[0] == L'\0') {
                if (FAILED(StringCchPrintf(generatedTaskName,
                    ARRAYSIZE(generatedTaskName),
                    TEXT("task_0"))))
                {
                    supHeapFree(eventInfo);
                    supTbDestroy(&textBuffer);
                    return FALSE;
                }
                taskName = generatedTaskName;
                isGeneratedTaskName = TRUE;
            }
        }
        else {
            if (!taskName || taskName[0] == L'\0') {
                supHeapFree(eventInfo);
                continue;
            }
        }

        if (isGeneratedTaskName) {
            if (FAILED(StringCchCopy(stringId,
                ARRAYSIZE(stringId),
                TEXT("task_0"))))
            {
                supHeapFree(eventInfo);
                supTbDestroy(&textBuffer);
                return FALSE;
            }
        }
        else {
            if (FAILED(StringCchPrintf(stringId,
                ARRAYSIZE(stringId),
                TEXT("task_%s"),
                taskName)))
            {
                supHeapFree(eventInfo);
                supTbDestroy(&textBuffer);
                return FALSE;
            }
        }

        if (!supTbAppend(&textBuffer, TEXT("        <string id=\"")) ||
            !supTbAppend(&textBuffer, stringId) ||
            !supTbAppend(&textBuffer, TEXT("\" value=\"")) ||
            !supTbAppend(&textBuffer, taskName) ||
            !supTbAppend(&textBuffer, TEXT("\" />\r\n")))
        {
            supHeapFree(eventInfo);
            supTbDestroy(&textBuffer);
            return FALSE;
        }

        supHeapFree(eventInfo);
    }

    if (!supTbAppend(&textBuffer, TEXT("      </stringTable>\r\n")
        TEXT("    </resources>\r\n")
        TEXT("  </localization>\r\n"))) {
        supTbDestroy(&textBuffer); return FALSE;
    }

    *LocalizationXml = textBuffer.Buffer;
    textBuffer.Buffer = NULL;
    supTbDestroy(&textBuffer);
    return TRUE;
}

/*
* EtmpBuildManifestTasksXml
*
* Purpose:
*
* Builds the task definitions section of an ETW provider instrumentation
* manifest and appends the generated XML to the supplied text buffer.
*
* Returns:
*
* TRUE if the task definitions were successfully generated and appended to
* the text buffer; otherwise FALSE.
*
*/
BOOL EtmpBuildManifestTasksXml(
    _In_ LPGUID ProviderGuid,
    _In_ PEVENT_DESCRIPTOR EventDescriptors,
    _In_ ULONG EventCount,
    _Inout_ PTEXT_BUFFER TextBuffer,
    _In_ TASK_OPCODES* TaskOpcodesList,
    _In_ ULONG TaskOpcodesCount
)
{
    ULONG i, j;
    BOOL hasTaskZero = FALSE;
    ULONG taskCount = 0;

    if (ProviderGuid == NULL ||
        (EventCount != 0 && EventDescriptors == NULL) ||
        TextBuffer == NULL)
    {
        return FALSE;
    }

    for (i = 0; i < EventCount; i++) {
        if (EventDescriptors[i].Task == 0) {
            hasTaskZero = TRUE;
            break;
        }
    }

    for (i = 0; i < EventCount; i++) {
        BOOL duplicate = FALSE;
        if (EventDescriptors[i].Task == 0)
            continue;
        for (j = 0; j < i; j++) {
            if (EventDescriptors[j].Task == EventDescriptors[i].Task) {
                duplicate = TRUE;
                break;
            }
        }
        if (!duplicate) taskCount++;
    }

    if (!hasTaskZero && taskCount == 0)
        return TRUE;

    if (!supTbAppend(TextBuffer, TEXT("        <tasks>\r\n")))
        return FALSE;

    if (hasTaskZero) {
        if (!supTbAppend(TextBuffer, TEXT("          <task name=\"task_0\" message=\"$(string.task_task_0)\" value=\"0\" />\r\n")))
            return FALSE;
    }

    for (i = 0; i < EventCount; i++) {
        BOOL duplicate = FALSE;
        PEVENT_DESCRIPTOR descriptor = &EventDescriptors[i];
        PTRACE_EVENT_INFO eventInfo;
        LPCWSTR taskName;
        WCHAR valueBuffer[32];

        if (descriptor->Task == 0)
            continue;

        for (j = 0; j < i; j++) {
            if (EventDescriptors[j].Task == descriptor->Task) {
                duplicate = TRUE;
                break;
            }
        }

        if (duplicate)
            continue;

        eventInfo = NULL;
        if (!EtmpGetManifestEventInformation(ProviderGuid, descriptor, &eventInfo))
            return FALSE;

        taskName = NULL;
        if (eventInfo->TaskNameOffset != 0) taskName = (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->TaskNameOffset);
        if (!taskName || taskName[0] == L'\0') {
            supHeapFree(eventInfo);
            continue;
        }

        StringCchPrintf(valueBuffer, ARRAYSIZE(valueBuffer), TEXT("%lu"), descriptor->Task);

        ULONG taskOpcodeIdx = (ULONG)-1;
        for (ULONG t = 0; t < TaskOpcodesCount; t++) {
            if (supStrCmp(TaskOpcodesList[t].TaskName, taskName) == 0) {
                taskOpcodeIdx = t;
                break;
            }
        }

        if (taskOpcodeIdx != (ULONG)-1 && TaskOpcodesList[taskOpcodeIdx].OpcodeCount > 0) {
            if (!supTbAppend(TextBuffer, TEXT("          <task name=\"")) ||
                !supTbAppend(TextBuffer, taskName) ||
                !supTbAppend(TextBuffer, TEXT("\" message=\"$(string.task_")) ||
                !supTbAppend(TextBuffer, taskName) ||
                !supTbAppend(TextBuffer, TEXT(")\" value=\"")) ||
                !supTbAppend(TextBuffer, valueBuffer) ||
                !supTbAppend(TextBuffer, TEXT("\">\r\n            <opcodes>\r\n")))
            {
                supHeapFree(eventInfo);
                return FALSE;
            }

            for (ULONG o = 0; o < TaskOpcodesList[taskOpcodeIdx].OpcodeCount; o++) {
                LPCWSTR opName = TaskOpcodesList[taskOpcodeIdx].Opcodes[o].OpcodeName;
                ULONG opVal = TaskOpcodesList[taskOpcodeIdx].Opcodes[o].Value;
                WCHAR opValStr[32], opStringId[512];

                StringCchPrintf(opValStr,
                    ARRAYSIZE(opValStr),
                    TEXT("%lu"),
                    opVal);

                StringCchPrintf(opStringId,
                    ARRAYSIZE(opStringId),
                    TEXT("opcode_%s%s"),
                    taskName,
                    opName);

                if (!supTbAppend(TextBuffer, TEXT("              <opcode name=\"")) ||
                    !supTbAppend(TextBuffer, opName) ||
                    !supTbAppend(TextBuffer, TEXT("\" message=\"$(string.")) ||
                    !supTbAppend(TextBuffer, opStringId) ||
                    !supTbAppend(TextBuffer, TEXT(")\" value=\"")) ||
                    !supTbAppend(TextBuffer, opValStr) ||
                    !supTbAppend(TextBuffer, TEXT("\" />\r\n")))
                {
                    supHeapFree(eventInfo);
                    return FALSE;
                }
            }

            if (!supTbAppend(TextBuffer, TEXT("            </opcodes>\r\n          </task>\r\n"))) {
                supHeapFree(eventInfo);
                return FALSE;
            }
        }
        else {
            if (!supTbAppend(TextBuffer, TEXT("          <task name=\"")) ||
                !supTbAppend(TextBuffer, taskName) ||
                !supTbAppend(TextBuffer, TEXT("\" message=\"$(string.task_")) ||
                !supTbAppend(TextBuffer, taskName) ||
                !supTbAppend(TextBuffer, TEXT(")\" value=\"")) ||
                !supTbAppend(TextBuffer, valueBuffer) ||
                !supTbAppend(TextBuffer, TEXT("\" />\r\n")))
            {
                supHeapFree(eventInfo);
                return FALSE;
            }
        }
        supHeapFree(eventInfo);
    }

    if (!supTbAppend(TextBuffer, TEXT("        </tasks>\r\n")))
        return FALSE;

    return TRUE;
}

/*
* EtmpBuildManifestTemplatesXml
*
* Purpose:
*
*  Builds the <templates> section of the manifest XML for the specified provider
*  and event descriptors. Identifies events that define unique event templates,
*  generates template identifiers based on the event task, ID, and version, and
*  adds the event properties as manifest <data> elements.
*
*  The resulting XML buffer is allocated dynamically and returned through
*  TemplatesXml. The caller is responsible for freeing the returned buffer
*  with supHeapFree.
*
* Return Value:
*
*  TRUE if the manifest template XML was successfully built and returned;
*  otherwise, FALSE.
*/
BOOL EtmpBuildManifestTemplatesXml(
    _In_ LPGUID ProviderGuid,
    _In_ PEVENT_DESCRIPTOR EventDescriptors,
    _In_ ULONG EventCount,
    _Out_ PWSTR* TemplatesXml
)
{
    ULONG i;
    ULONG propertyIndex;
    ULONG templateEventIndex;
    PTRACE_EVENT_INFO eventInfo;
    PCEVENT_PROPERTY_INFO propertyInfo;
    PCEVENT_PROPERTY_INFO relatedProperty;
    LPCWSTR propertyName;
    LPCWSTR relatedPropertyName;
    LPCWSTR typeName;
    LPCWSTR mapName, propName;
    TEXT_BUFFER textBuffer;
    WCHAR templateId[128];
    WCHAR fallbackMapName[256];

    if (ProviderGuid == NULL ||
        (EventCount != 0 && EventDescriptors == NULL) ||
        TemplatesXml == NULL)
    {
        return FALSE;
    }

    *TemplatesXml = NULL;
    RtlSecureZeroMemory(&textBuffer, sizeof(textBuffer));
    if (!supTbInitialize(&textBuffer, 4096)) return FALSE;

    BOOL hasTemplates = FALSE;

    for (i = 0; i < EventCount; i++) {

        eventInfo = NULL;
        if (!EtmpGetManifestEventInformation(ProviderGuid,
            &EventDescriptors[i],
            &eventInfo))
        {
            continue;
        }

        if (eventInfo->TopLevelPropertyCount == 0) {
            supHeapFree(eventInfo);
            continue;
        }

        templateEventIndex = i;
        if (!EtmpFindManifestTemplateEvent(ProviderGuid,
            EventDescriptors,
            i,
            &templateEventIndex))
        {
            supHeapFree(eventInfo);
            break;
        }

        if (templateEventIndex != i) {
            supHeapFree(eventInfo);
            continue;
        }

        if (!hasTemplates) {
            if (!supTbAppend(&textBuffer, TEXT("        <templates>\r\n"))) {
                supHeapFree(eventInfo);
                break;
            }
            hasTemplates = TRUE;
        }

        LPCWSTR tmplTaskName = NULL;
        WCHAR genTmplTaskName[64];
        if (eventInfo->TaskNameOffset)
            tmplTaskName = (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->TaskNameOffset);

        if (EventDescriptors[i].Task == 0) {
            if (!tmplTaskName || tmplTaskName[0] == L'\0') {
                if (SUCCEEDED(StringCchPrintf(genTmplTaskName,
                    ARRAYSIZE(genTmplTaskName),
                    TEXT("task_0"))))
                {
                    tmplTaskName = genTmplTaskName;
                }
            }
        }

        if (tmplTaskName && tmplTaskName[0] != L'\0') {
            if (EventDescriptors[i].Version > 0) {
                if (FAILED(StringCchPrintf(templateId,
                    ARRAYSIZE(templateId),
                    TEXT("%s%uArgs_V%u"),
                    tmplTaskName,
                    EventDescriptors[i].Id,
                    EventDescriptors[i].Version)))
                {
                    supHeapFree(eventInfo);
                    break;
                }
            }
            else {
                if (FAILED(StringCchPrintf(templateId,
                    ARRAYSIZE(templateId),
                    TEXT("%s%uArgs"),
                    tmplTaskName,
                    EventDescriptors[i].Id)))
                {
                    supHeapFree(eventInfo);
                    break;
                }
            }
        }
        else {
            if (EventDescriptors[i].Version > 0) {
                if (FAILED(StringCchPrintf(templateId,
                    ARRAYSIZE(templateId),
                    TEXT("Event_%u_%u_V%u"),
                    EventDescriptors[i].Id,
                    EventDescriptors[i].Version,
                    EventDescriptors[i].Version)))
                {
                    supHeapFree(eventInfo);
                    break;
                }
            }
            else {
                if (FAILED(StringCchPrintf(templateId,
                    ARRAYSIZE(templateId),
                    TEXT("Event_%u_%u"),
                    EventDescriptors[i].Id,
                    EventDescriptors[i].Version)))
                {
                    supHeapFree(eventInfo);
                    break;
                }
            }
        }

        if (!supTbAppend(&textBuffer, TEXT("          <template tid=\"")) ||
            !supTbAppend(&textBuffer, templateId) ||
            !supTbAppend(&textBuffer, TEXT("\">\r\n")))
        {
            supHeapFree(eventInfo);
            break;
        }

        for (propertyIndex = 0; propertyIndex < eventInfo->TopLevelPropertyCount; propertyIndex++) {
            propertyInfo = &eventInfo->EventPropertyInfoArray[propertyIndex];

            if (propertyInfo->Flags & PropertyStruct)
                continue;

            propertyName = propertyInfo->NameOffset ? (LPCWSTR)RtlOffsetToPointer(eventInfo, propertyInfo->NameOffset) : NULL;
            if (!propertyName || propertyName[0] == L'\0')
                continue;

            switch (propertyInfo->nonStructType.InType) {
            case TDH_INTYPE_UNICODESTRING: typeName = TEXT("UnicodeString"); break;
            case TDH_INTYPE_ANSISTRING: typeName = TEXT("AnsiString"); break;
            case TDH_INTYPE_INT8: typeName = TEXT("Int8"); break;
            case TDH_INTYPE_UINT8: typeName = TEXT("UInt8"); break;
            case TDH_INTYPE_INT16: typeName = TEXT("Int16"); break;
            case TDH_INTYPE_UINT16: typeName = TEXT("UInt16"); break;
            case TDH_INTYPE_INT32: typeName = TEXT("Int32"); break;
            case TDH_INTYPE_UINT32: typeName = TEXT("UInt32"); break;
            case TDH_INTYPE_INT64: typeName = TEXT("Int64"); break;
            case TDH_INTYPE_UINT64: typeName = TEXT("UInt64"); break;
            case TDH_INTYPE_FLOAT: typeName = TEXT("Float"); break;
            case TDH_INTYPE_DOUBLE: typeName = TEXT("Double"); break;
            case TDH_INTYPE_BOOLEAN: typeName = TEXT("Boolean"); break;
            case TDH_INTYPE_BINARY: typeName = TEXT("Binary"); break;
            case TDH_INTYPE_GUID: typeName = TEXT("GUID"); break;
            case TDH_INTYPE_POINTER: typeName = TEXT("Pointer"); break;
            case TDH_INTYPE_FILETIME: typeName = TEXT("FILETIME"); break;
            case TDH_INTYPE_SYSTEMTIME: typeName = TEXT("SYSTEMTIME"); break;
            case TDH_INTYPE_SID: typeName = TEXT("SID"); break;
            case TDH_INTYPE_HEXINT32: typeName = TEXT("HexInt32"); break;
            case TDH_INTYPE_HEXINT64: typeName = TEXT("HexInt64"); break;
            case TDH_INTYPE_COUNTEDSTRING: typeName = TEXT("CountedString"); break;
            case TDH_INTYPE_COUNTEDANSISTRING: typeName = TEXT("CountedAnsiString"); break;
            case TDH_INTYPE_REVERSEDCOUNTEDSTRING: typeName = TEXT("ReversedCountedString"); break;
            case TDH_INTYPE_REVERSEDCOUNTEDANSISTRING: typeName = TEXT("ReversedCountedAnsiString"); break;
            case TDH_INTYPE_NONNULLTERMINATEDSTRING: typeName = TEXT("NonNullTerminatedString"); break;
            case TDH_INTYPE_NONNULLTERMINATEDANSISTRING: typeName = TEXT("NonNullTerminatedAnsiString"); break;
            case TDH_INTYPE_UNICODECHAR: typeName = TEXT("UnicodeChar"); break;
            case TDH_INTYPE_ANSICHAR: typeName = TEXT("AnsiChar"); break;
            case TDH_INTYPE_SIZET: typeName = TEXT("SizeT"); break;
            default: typeName = TEXT("Binary"); break;
            }

            if (!supTbAppend(&textBuffer, TEXT("            <data name=\"")) ||
                !supTbAppend(&textBuffer, propertyName) ||
                !supTbAppend(&textBuffer, TEXT("\" inType=\"win:")) ||
                !supTbAppend(&textBuffer, typeName))
            {
                break;
            }

            if (propertyInfo->Flags & PropertyParamCount) {

                if (propertyInfo->countPropertyIndex < eventInfo->TopLevelPropertyCount) {

                    relatedProperty = &eventInfo->EventPropertyInfoArray[propertyInfo->countPropertyIndex];
                    if (relatedProperty->NameOffset) {

                        relatedPropertyName = (LPCWSTR)RtlOffsetToPointer(eventInfo, relatedProperty->NameOffset);
                        if (relatedPropertyName && relatedPropertyName[0] != L'\0') {
                            if (!supTbAppend(&textBuffer, TEXT("\" count=\"")) ||
                                !supTbAppend(&textBuffer, relatedPropertyName))
                            {
                                break;
                            }
                        }
                    }
                }
            }
            else if (propertyInfo->Flags & PropertyParamLength) {

                if (propertyInfo->lengthPropertyIndex < eventInfo->TopLevelPropertyCount) {

                    relatedProperty = &eventInfo->EventPropertyInfoArray[propertyInfo->lengthPropertyIndex];
                    if (relatedProperty->NameOffset) {

                        relatedPropertyName = (LPCWSTR)RtlOffsetToPointer(eventInfo, relatedProperty->NameOffset);
                        if (relatedPropertyName && relatedPropertyName[0] != L'\0') {

                            if (!supTbAppend(&textBuffer, TEXT("\" length=\"")) ||
                                !supTbAppend(&textBuffer, relatedPropertyName))
                            {
                                break;
                            }
                        }
                    }
                }
            }

            mapName = NULL;
            propName = propertyInfo->NameOffset ? (LPCWSTR)RtlOffsetToPointer(eventInfo, propertyInfo->NameOffset) : NULL;

            if (propertyInfo->nonStructType.MapNameOffset != 0) {
                mapName = (LPCWSTR)RtlOffsetToPointer(eventInfo, propertyInfo->nonStructType.MapNameOffset);
            }
            else if (propName && propName[0] != L'\0') {

                StringCchPrintf(fallbackMapName,
                    ARRAYSIZE(fallbackMapName),
                    TEXT("%sMap"),
                    propName);

                ULONG testSize = 0;
                EVENT_RECORD dummy = { 0 };
                dummy.EventHeader.ProviderId = *ProviderGuid;
                dummy.EventHeader.EventDescriptor = EventDescriptors[i];

                if (TdhGetEventMapInformation(&dummy,
                    (PWSTR)propName,
                    NULL,
                    &testSize) == ERROR_INSUFFICIENT_BUFFER)
                {
                    mapName = propName;
                }
                else if (TdhGetEventMapInformation(&dummy,
                    fallbackMapName,
                    NULL,
                    &testSize) == ERROR_INSUFFICIENT_BUFFER)
                {
                    mapName = fallbackMapName;
                }
            }

            if (mapName && mapName[0] != L'\0') {
                if (!supTbAppend(&textBuffer, TEXT("\" map=\"")) ||
                    !supTbAppend(&textBuffer, mapName))
                {
                    break;
                }
            }

            if (!supTbAppend(&textBuffer, TEXT("\" />\r\n"))) break;
        }

        if (propertyIndex != eventInfo->TopLevelPropertyCount) {
            supHeapFree(eventInfo);
            break;
        }

        if (!supTbAppend(&textBuffer, TEXT("          </template>\r\n"))) {
            supHeapFree(eventInfo);
            break;
        }

        supHeapFree(eventInfo);
    }

    if (i != EventCount) {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!hasTemplates) {
        if (!supTbAppend(&textBuffer, TEXT("        <templates></templates>\r\n"))) {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }
    else {
        if (!supTbAppend(&textBuffer, TEXT("        </templates>\r\n"))) {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }

    *TemplatesXml = textBuffer.Buffer;
    textBuffer.Buffer = NULL;
    supTbDestroy(&textBuffer);

    return TRUE;
}

/*
* EtmpGetOpcodeName
*
* Purpose:
*
* Searches the specified provider opcode table for the specified
* opcode value and returns the corresponding opcode name.
*
*/
LPCWSTR EtmpGetOpcodeName(
    _In_ ULONG OpcodeValue,
    _In_ PROVIDER_OPCODE* Opcodes,
    _In_ ULONG OpcodeCount
)
{
    ULONG i;

    if (Opcodes == NULL || OpcodeCount == 0)
        return NULL;

    for (i = 0; i < OpcodeCount; i++) {
        if (Opcodes[i].Value == OpcodeValue)
            return Opcodes[i].Name;
    }

    return NULL;
}

/*
* EtmpBuildManifestProviderXml
*
* Purpose:
*
* Builds an XML instrumentation manifest representation for the specified
* ETW provider from its event descriptors and associated manifest metadata.
*
* The resulting XML buffer is returned to the caller through SchemaXml and
* must be released with supHeapFree.
*
* Returns:
*
* TRUE if the provider manifest was successfully built; otherwise FALSE.
*
*/
BOOL EtmpBuildManifestProviderXml(
    _In_ LPGUID ProviderGuid,
    _In_ LPCWSTR ProviderName,
    _In_ PEVENT_DESCRIPTOR EventDescriptors,
    _In_ ULONG EventCount,
    _Out_ PWSTR* SchemaXml
)
{
    ULONG i;

    PTRACE_EVENT_INFO eventInfo;
    PWSTR eventXml, templatesXml, localizationXml;

    ULONG TaskOpcodesCount = 0;
    PTASK_OPCODES TaskOpcodesList;

    ULONG ProviderOpcodeCount = 0;
    PROVIDER_OPCODE ProviderOpcodes[256];

    PPROVIDER_FIELD_INFOARRAY opFieldInfo = NULL;
    TEXT_BUFFER textBuffer;
    WCHAR guidBuffer[64], providerSymbol[256], templateId[128];

    if (ProviderGuid == NULL ||
        ProviderName == NULL ||
        (EventCount != 0 && EventDescriptors == NULL) ||
        SchemaXml == NULL)
    {
        return FALSE;
    }

    TaskOpcodesList = (PTASK_OPCODES)supHeapAlloc(256 * sizeof(TASK_OPCODES));
    if (TaskOpcodesList == NULL)
        return FALSE;

    *SchemaXml = NULL;
    RtlSecureZeroMemory(&textBuffer, sizeof(textBuffer));
    RtlSecureZeroMemory(providerSymbol, sizeof(providerSymbol));

    if (!supTbInitialize(&textBuffer, PAGE_SIZE)) {
        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    ULONG opBufferSize = 0;
    ULONG status = TdhEnumerateProviderFieldInformation(ProviderGuid,
        EventOpcodeInformation, NULL, &opBufferSize);

    if (status == ERROR_INSUFFICIENT_BUFFER) {

        opFieldInfo = (PPROVIDER_FIELD_INFOARRAY)supHeapAlloc(opBufferSize);
        if (opFieldInfo) {

            status = TdhEnumerateProviderFieldInformation(ProviderGuid,
                EventOpcodeInformation, opFieldInfo, &opBufferSize);
            if (status == ERROR_SUCCESS) {

                for (i = 0; i < opFieldInfo->NumberOfElements && ProviderOpcodeCount < 256; i++) {
                    if (opFieldInfo->FieldInfoArray[i].NameOffset != 0) {

                        ProviderOpcodes[ProviderOpcodeCount].Value = (ULONG)opFieldInfo->FieldInfoArray[i].Value;
                        ProviderOpcodes[ProviderOpcodeCount].Name = (LPCWSTR)RtlOffsetToPointer(opFieldInfo, opFieldInfo->FieldInfoArray[i].NameOffset);
                        ProviderOpcodeCount++;
                    }
                }
            }
        }
    }

    if (FAILED(StringCchPrintf(guidBuffer,
        ARRAYSIZE(guidBuffer),
        TEXT("{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"),
        ProviderGuid->Data1,
        ProviderGuid->Data2,
        ProviderGuid->Data3,
        ProviderGuid->Data4[0],
        ProviderGuid->Data4[1],
        ProviderGuid->Data4[2],
        ProviderGuid->Data4[3],
        ProviderGuid->Data4[4],
        ProviderGuid->Data4[5],
        ProviderGuid->Data4[6],
        ProviderGuid->Data4[7])))
    {
        supTbDestroy(&textBuffer);

        if (opFieldInfo)
            supHeapFree(opFieldInfo);

        supHeapFree(TaskOpcodesList);

        return FALSE;
    }

    ULONG s = 0, t = 0;
    while (ProviderName[s] != L'\0' &&
        t + 1 < ARRAYSIZE(providerSymbol))
    {
        WCHAR ch = ProviderName[s++];
        if ((ch >= L'A' && ch <= L'Z') ||
            (ch >= L'a' && ch <= L'z') ||
            (ch >= L'0' && ch <= L'9') ||
            ch == L'_')
        {
            providerSymbol[t++] = ch;
        }
    }
    providerSymbol[t] = L'\0';

    if (!supTbAppend(&textBuffer,
        TEXT("<instrumentationManifest ")
        TEXT("xmlns=\"http://schemas.microsoft.com/win/2004/08/events\">\r\n")
        TEXT("  <instrumentation ")
        TEXT("xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" ")
        TEXT("xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ")
        TEXT("xmlns:win=\"http://manifests.microsoft.com/win/2004/08/windows/events\">\r\n")
        TEXT("    <events>\r\n")
        TEXT("      <provider name=\"")) ||
        !supTbAppend(&textBuffer, ProviderName) ||
        !supTbAppend(&textBuffer, TEXT("\" guid=\"")) ||
        !supTbAppend(&textBuffer, guidBuffer) ||
        !supTbAppend(&textBuffer, TEXT("\" resourceFileName=\"")) ||
        !supTbAppend(&textBuffer, ProviderName) ||
        !supTbAppend(&textBuffer, TEXT("\" messageFileName=\"")) ||
        !supTbAppend(&textBuffer, ProviderName) ||
        !supTbAppend(&textBuffer, TEXT("\" symbol=\"")) ||
        !supTbAppend(&textBuffer, providerSymbol) ||
        !supTbAppend(&textBuffer, TEXT("\" source=\"Xml\">\r\n")))
    {
        supTbDestroy(&textBuffer);

        if (opFieldInfo)
            supHeapFree(opFieldInfo);

        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    if (!EtmpBuildManifestKeywordsXml(ProviderGuid, &textBuffer)) {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);

        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    for (i = 0; i < EventCount; i++) {

        eventInfo = NULL;
        if (!EtmpGetManifestEventInformation(ProviderGuid,
            &EventDescriptors[i],
            &eventInfo))
        {
            continue;
        }

        ULONG opcodeVal = eventInfo->EventDescriptor.Opcode;
        if (opcodeVal > 9) {

            LPCWSTR opcodeName = EtmpGetOpcodeName(opcodeVal, ProviderOpcodes, ProviderOpcodeCount);
            LPCWSTR evtTaskName = NULL;
            WCHAR genTaskName[64];

            if (eventInfo->TaskNameOffset)
                evtTaskName = (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->TaskNameOffset);
            if (eventInfo->EventDescriptor.Task == 0 &&
                (!evtTaskName || evtTaskName[0] == L'\0'))
            {
                StringCchPrintf(genTaskName, ARRAYSIZE(genTaskName), TEXT("task_0"));
                evtTaskName = genTaskName;
            }

            if (opcodeName && evtTaskName && evtTaskName[0] != L'\0') {

                ULONG tIdx = 0;
                for (; tIdx < TaskOpcodesCount; tIdx++) {
                    if (supStrCmp(TaskOpcodesList[tIdx].TaskName, evtTaskName) == 0)
                        break;
                }

                if (tIdx == TaskOpcodesCount && TaskOpcodesCount < 256) {
                    StringCchCopy(TaskOpcodesList[tIdx].TaskName, 128, evtTaskName);
                    TaskOpcodesList[tIdx].OpcodeCount = 0;
                    TaskOpcodesCount++;
                }

                if (tIdx < TaskOpcodesCount) {

                    BOOL found = FALSE;
                    for (ULONG oIdx = 0; oIdx < TaskOpcodesList[tIdx].OpcodeCount; oIdx++) {
                        if (supStrCmp(TaskOpcodesList[tIdx].Opcodes[oIdx].OpcodeName, opcodeName) == 0) {
                            found = TRUE;
                            break;
                        }
                    }

                    if (!found && TaskOpcodesList[tIdx].OpcodeCount < 64) {

                        PTASK_OPCODE opcode;

                        opcode = &TaskOpcodesList[tIdx].Opcodes[TaskOpcodesList[tIdx].OpcodeCount];
                        StringCchCopy(opcode->OpcodeName,
                            ARRAYSIZE(opcode->OpcodeName),
                            opcodeName);

                        opcode->Value = opcodeVal;

                        TaskOpcodesList[tIdx].OpcodeCount++;
                    }

                }
            }
        }
        supHeapFree(eventInfo);
    }

    if (!EtmpBuildManifestTasksXml(ProviderGuid,
        EventDescriptors,
        EventCount,
        &textBuffer,
        TaskOpcodesList,
        TaskOpcodesCount))
    {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);

        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    if (!EtmpBuildManifestMapsXml(ProviderGuid,
        EventDescriptors,
        EventCount,
        &textBuffer))
    {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);

        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer, TEXT("        <events>\r\n"))) {
        supTbDestroy(&textBuffer);
        if (opFieldInfo) supHeapFree(opFieldInfo);
        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    for (i = 0; i < EventCount; i++) {

        ULONG templateEventIndex;

        eventInfo = NULL;
        if (!EtmpGetManifestEventInformation(ProviderGuid, &EventDescriptors[i], &eventInfo))
            continue;

        templateEventIndex = i;
        if (eventInfo->TopLevelPropertyCount != 0) {
            if (!EtmpFindManifestTemplateEvent(ProviderGuid, EventDescriptors, i, &templateEventIndex)) {
                supHeapFree(eventInfo);
                break;
            }

            LPCWSTR tmplTaskName = NULL; WCHAR genTmplTaskName[64];
            if (eventInfo->TaskNameOffset)
                tmplTaskName = (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->TaskNameOffset);

            if (EventDescriptors[templateEventIndex].Task == 0 &&
                (!tmplTaskName || tmplTaskName[0] == L'\0'))
            {
                StringCchPrintf(genTmplTaskName, ARRAYSIZE(genTmplTaskName), TEXT("task_0"));
                tmplTaskName = genTmplTaskName;
            }

            if (tmplTaskName && tmplTaskName[0] != L'\0') {
                if (EventDescriptors[templateEventIndex].Version > 0) {
                    StringCchPrintf(templateId,
                        ARRAYSIZE(templateId),
                        TEXT("%s%uArgs_V%u"),
                        tmplTaskName,
                        EventDescriptors[templateEventIndex].Id,
                        EventDescriptors[templateEventIndex].Version);
                }
                else {
                    StringCchPrintf(templateId,
                        ARRAYSIZE(templateId),
                        TEXT("%s%uArgs"),
                        tmplTaskName,
                        EventDescriptors[templateEventIndex].Id);
                }
            }
            else {
                if (EventDescriptors[templateEventIndex].Version > 0) {
                    StringCchPrintf(templateId,
                        ARRAYSIZE(templateId),
                        TEXT("Event_%u_%u_V%u"),
                        EventDescriptors[templateEventIndex].Id,
                        EventDescriptors[templateEventIndex].Version,
                        EventDescriptors[templateEventIndex].Version);
                }
                else {
                    StringCchPrintf(templateId,
                        ARRAYSIZE(templateId),
                        TEXT("Event_%u_%u"),
                        EventDescriptors[templateEventIndex].Id,
                        EventDescriptors[templateEventIndex].Version);
                }
            }
        }
        else {
            templateId[0] = L'\0';
        }

        LPCWSTR eventSymbol = NULL;
        WCHAR generatedSymbol[256];
        LPCWSTR evtTaskName = NULL;

        if (eventInfo->TaskNameOffset)
            evtTaskName = (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->TaskNameOffset);

        WCHAR genTaskName[64];
        if (eventInfo->EventDescriptor.Task == 0 &&
            (!evtTaskName || evtTaskName[0] == L'\0'))
        {
            StringCchPrintf(genTaskName, 64, TEXT("task_0"));
            evtTaskName = genTaskName;
        }

        if (evtTaskName && evtTaskName[0] != L'\0') {
            if (eventInfo->EventDescriptor.Opcode == EVENT_TRACE_TYPE_INFO) {
                BOOL isFirstForTask = TRUE;
                for (ULONG k = 0; k < i; k++) {
                    if (EventDescriptors[k].Task == eventInfo->EventDescriptor.Task)
                    {
                        isFirstForTask = FALSE;
                        break;
                    }
                }
                if (isFirstForTask) {
                    StringCchCopy(generatedSymbol, ARRAYSIZE(generatedSymbol), evtTaskName);
                }
                else {
                    StringCchPrintf(generatedSymbol,
                        ARRAYSIZE(generatedSymbol), TEXT("%s%u"),
                        evtTaskName,
                        eventInfo->EventDescriptor.Id);
                }
            }
            else {
                LPCWSTR opcodeName = EtmpGetOpcodeName(eventInfo->EventDescriptor.Opcode,
                    ProviderOpcodes, ProviderOpcodeCount);

                if (eventInfo->EventDescriptor.Opcode == EVENT_TRACE_TYPE_START) {
                    if (EtmpEndsWithIgnoreCase(evtTaskName, TEXT("Start")))
                        StringCchCopy(generatedSymbol, ARRAYSIZE(generatedSymbol), evtTaskName);
                    else
                        StringCchPrintf(generatedSymbol, ARRAYSIZE(generatedSymbol), TEXT("%sStart"), evtTaskName);
                }
                else if (eventInfo->EventDescriptor.Opcode == EVENT_TRACE_TYPE_END) {
                    if (EtmpEndsWithIgnoreCase(evtTaskName, TEXT("Stop")))
                        StringCchCopy(generatedSymbol, ARRAYSIZE(generatedSymbol), evtTaskName);
                    else
                        StringCchPrintf(generatedSymbol, ARRAYSIZE(generatedSymbol), TEXT("%sStop"), evtTaskName);
                }
                else {
                    if (opcodeName && opcodeName[0] != L'\0') {
                        if (EtmpEndsWithIgnoreCase(evtTaskName, opcodeName))
                            StringCchCopy(generatedSymbol, ARRAYSIZE(generatedSymbol), evtTaskName);
                        else
                            StringCchPrintf(generatedSymbol, ARRAYSIZE(generatedSymbol), TEXT("%s%s"), evtTaskName, opcodeName);
                    }
                    else StringCchCopy(generatedSymbol, ARRAYSIZE(generatedSymbol), evtTaskName);
                }
            }

            if (eventInfo->EventDescriptor.Version > 0) {
                WCHAR verSuffix[32];
                StringCchPrintf(verSuffix, ARRAYSIZE(verSuffix), TEXT("_V%u"), eventInfo->EventDescriptor.Version);
                StringCchCat(generatedSymbol, ARRAYSIZE(generatedSymbol), verSuffix);
            }
            eventSymbol = generatedSymbol;
        }
        else {

            StringCchPrintf(generatedSymbol,
                ARRAYSIZE(generatedSymbol),
                TEXT("Event%u"),
                eventInfo->EventDescriptor.Id);

            eventSymbol = generatedSymbol;
        }

        WCHAR keywordList[512] = { 0 };
        LPCWSTR eventKeywordName = NULL;

        if (eventInfo->KeywordsNameOffset != 0) {

            LPCWSTR kw = (LPCWSTR)RtlOffsetToPointer(eventInfo, eventInfo->KeywordsNameOffset);
            if (kw && kw[0] != L'\0') {
                ULONG i_kw = 0, j_kw = 0;
                while (kw[i_kw] != L'\0' || kw[i_kw + 1] != L'\0') {
                    if (kw[i_kw] == L'\0') {
                        keywordList[j_kw++] = L' ';
                        i_kw++;
                    }
                    else {
                        keywordList[j_kw++] = kw[i_kw++];
                    }
                    if (j_kw >= 511)
                        break;
                }

                keywordList[j_kw] = L'\0';
                if (j_kw > 0 && keywordList[j_kw - 1] == L' ')
                    keywordList[j_kw - 1] = L'\0';

                eventKeywordName = keywordList;
            }
        }

        LPCWSTR reliableOpcodeName = EtmpGetOpcodeName(eventInfo->EventDescriptor.Opcode,
            ProviderOpcodes, ProviderOpcodeCount);

        eventXml = NULL;

        if (!EtmpBuildManifestEventXml(eventInfo,
            eventInfo->TopLevelPropertyCount != 0 ? templateId : NULL,
            eventKeywordName,
            eventSymbol,
            reliableOpcodeName,
            &eventXml))
        {
            supHeapFree(eventInfo);
            continue;
        }

        if (!supTbAppend(&textBuffer, eventXml)) {
            supHeapFree(eventXml);
            supHeapFree(eventInfo);
            break;
        }
        supHeapFree(eventXml);
        supHeapFree(eventInfo);
    }

    if (i != EventCount) {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);
        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer, TEXT("        </events>\r\n"))) {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);
        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    templatesXml = NULL;
    if (!EtmpBuildManifestTemplatesXml(ProviderGuid, EventDescriptors, EventCount, &templatesXml)) {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);
        supHeapFree(TaskOpcodesList);
        return FALSE;
    }
    if (templatesXml) {
        if (!supTbAppend(&textBuffer, templatesXml)) {
            supHeapFree(templatesXml);
            supTbDestroy(&textBuffer);
            if (opFieldInfo)
                supHeapFree(opFieldInfo);
            supHeapFree(TaskOpcodesList);
            return FALSE;
        }
        supHeapFree(templatesXml);
    }

    if (!supTbAppend(&textBuffer, TEXT("      </provider>\r\n    </events>\r\n  </instrumentation>\r\n"))) {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);
        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    localizationXml = NULL;
    if (!EtmpBuildManifestLocalizationXml(ProviderGuid,
        EventDescriptors,
        EventCount,
        &localizationXml,
        TaskOpcodesList,
        TaskOpcodesCount))
    {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);
        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    if (localizationXml) {
        if (!supTbAppend(&textBuffer, localizationXml)) {
            supHeapFree(localizationXml);
            supTbDestroy(&textBuffer);
            if (opFieldInfo)
                supHeapFree(opFieldInfo);
            supHeapFree(TaskOpcodesList);
            return FALSE;
        }
        supHeapFree(localizationXml);
    }

    if (!supTbAppend(&textBuffer, TEXT("</instrumentationManifest>\r\n"))) {
        supTbDestroy(&textBuffer);
        if (opFieldInfo)
            supHeapFree(opFieldInfo);
        supHeapFree(TaskOpcodesList);
        return FALSE;
    }

    *SchemaXml = textBuffer.Buffer;
    textBuffer.Buffer = NULL;
    supTbDestroy(&textBuffer);

    if (opFieldInfo)
        supHeapFree(opFieldInfo);

    supHeapFree(TaskOpcodesList);

    //
    // This is a really big function (as well as EtmpBuildManifestTemplatesXml) 
    // and it is hard to follow as it combines everything into consolidated Xml output.
    //
    return TRUE;
}

/*
* EtmBuildManifestProviderSchema
*
* Purpose:
*
* Enumerates the manifest events registered for the specified ETW
* provider and builds an XML schema containing the provider metadata,
* event definitions, templates, and localization information.
*
*/
DWORD EtmBuildManifestProviderSchema(
    _In_ LPGUID ProviderGuid,
    _In_ LPCWSTR ProviderName,
    _Out_ PWSTR* SchemaXml
)
{
    ULONG status = ERROR_SUCCESS, bufferSize = 0;
    PPROVIDER_EVENT_INFO eventInfo = NULL;

    if (ProviderGuid == NULL || ProviderName == NULL || SchemaXml == NULL)
        return ERROR_INVALID_PARAMETER;

    *SchemaXml = NULL;

    do {

        status = TdhEnumerateManifestProviderEvents(ProviderGuid, NULL, &bufferSize);
        if (status != ERROR_INSUFFICIENT_BUFFER)
            break;

        eventInfo = (PPROVIDER_EVENT_INFO)supHeapAlloc(bufferSize);
        if (!eventInfo) {
            status = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        status = TdhEnumerateManifestProviderEvents(ProviderGuid, eventInfo, &bufferSize);
        if (status != ERROR_SUCCESS)
            break;

        __try {
            if (!EtmpBuildManifestProviderXml(ProviderGuid,
                ProviderName,
                eventInfo->EventDescriptorsArray,
                eventInfo->NumberOfEvents,
                SchemaXml))
            {
                status = ERROR_INTERNAL_ERROR;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
#ifdef _DEBUG
            OutputDebugString(TEXT("[Xml] Exception during building XML for provider manifest\r\n"));
#endif
            status = GetExceptionCode();
        }

    } while (FALSE);

    if (eventInfo)
        supHeapFree(eventInfo);

    return status;
}
