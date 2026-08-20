/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       ETWMETA.H
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
*  Common header file for the ETW metadata handling.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#pragma once

typedef const TRACE_EVENT_INFO* PCTRACE_EVENT_INFO;
typedef const EVENT_PROPERTY_INFO* PCEVENT_PROPERTY_INFO;

typedef struct _TDH_FORMAT_RESULT {
    WCHAR Buffer[512];
    USHORT UserDataConsumed;
    ULONG Status;
} TDH_FORMAT_RESULT, * PTDH_FORMAT_RESULT;

VOID EtpLoadProviders(
    VOID);

ULONG EtpLoadSessions(
    VOID);

VOID EtpLoadProviderSessionCrossReference(
    VOID);

VOID EtpBuildSessionProviderIndex(
    VOID);

VOID EtpLoadProviderSchema(
    _In_ ULONG ProviderIndex);

BOOL EtpLoadMofProviderSchema(
    _In_ ULONG ProviderIndex);

VOID EtpWaitForMofSchemaLoad(
    VOID);

BOOL EtpEnsureLiveEventCapacity(
    VOID);

USHORT EtpGetElementPropertyLength(
    _In_ PEVENT_RECORD EventRecord,
    _In_ LPCWSTR PropertyName,
    _In_ ULONG ArrayIndex);

PEVENT_MAP_INFO EtpGetMapInfoForProperty(
    _In_ PEVENT_RECORD EventRecord,
    _In_ PCTRACE_EVENT_INFO EventInfo,
    _In_ PCEVENT_PROPERTY_INFO PropertyInfo);

VOID EtpDecodePropertyRange(
    _In_ PCTRACE_EVENT_INFO Info,
    _In_ PEVENT_RECORD EventRecord,
    _In_ ULONG StartIndex,
    _In_ ULONG IndexCount,
    _In_ ULONG PointerSize,
    _Inout_ PBYTE* UserData,
    _Inout_ PUSHORT UserDataLeft,
    _Inout_updates_(DecodedCount) PULONGLONG DecodedScalars,
    _In_ ULONG DecodedCount,
    _Inout_ PWSTR* Cursor,
    _Inout_ SIZE_T* Remaining,
    _In_ ULONG Depth);

VOID EtpDecodeEventProperties(
    _In_ PEVENT_RECORD EventRecord,
    _Out_writes_(OutBufChars) PWSTR OutBuf,
    _In_ SIZE_T OutBufChars);

ULONG EtpGatherCheckedProviders(
    _In_ ULONG* outIndices,
    _In_ ULONG maxCount);

VOID EtpInitializeTraceProperties(
    _Inout_ PEVENT_TRACE_PROPERTIES Properties,
    _In_ ULONG BufferSize,
    _In_ ULONG LoggerNameBytes);

VOID EtpCloseWppDecodingHandleIfOpen(
    VOID);

BOOL EtpConnectWmi(
    _Out_ IWbemServices** Services);

BOOL EtpWmiClassGuidMatchesProvider(
    _In_ IWbemClassObject* ClassObject,
    _In_ const GUID* ProviderGuid);

BOOL EtpGetWmiSystemString(
    _In_ IWbemClassObject* ClassObject,
    _In_ LPCWSTR Name,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars);

BOOL EtpGetWmiQualifierString(
    _In_ IWbemClassObject* ClassObject,
    _In_ LPCWSTR Name,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars);

_Success_(return != FALSE)
BOOL EtpGetWmiQualifierGuid(
    _In_ IWbemClassObject * ClassObject,
    _In_ LPCWSTR Name,
    _Out_ GUID * Guid);

BOOL EtpGetWmiQualifierUInt32(
    _In_ IWbemClassObject * ClassObject,
    _In_ LPCWSTR Name,
    _Out_ PULONG Value);

BOOL EtpGetWmiClassName(
    _In_ IWbemClassObject * ClassObject,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars);

BOOL EtpGetMofClassEventType(
    _In_ IWbemClassObject * ClassObject,
    _Out_ PULONG EventType);

BOOL EtpGetWmiPropertyDataId(
    _In_ IWbemClassObject * ClassObject,
    _In_ LPCWSTR PropertyName,
    _Out_ PULONG DataId);

_Success_(return != FALSE)
BOOL EtpLoadProviderResourceInformation(
    _In_ LPGUID ProviderGuid,
    _Out_writes_(ResourceFileNameLength) LPWSTR ResourceFileName,
    _In_ ULONG ResourceFileNameLength,
    _Out_writes_(MessageFileNameLength) LPWSTR MessageFileName,
    _In_ ULONG MessageFileNameLength);

LPCWSTR EtpGetMofSchemaLoadStatusName(
    _In_ MOF_SCHEMA_LOAD_STATUS Status);

//
//etwman.cpp
// 
DWORD EtmBuildManifestProviderSchema(
    _In_ LPGUID ProviderGuid,
    _In_ LPCWSTR ProviderName,
    _Out_ PWSTR * SchemaXml);

//
// etwmof.cpp
//
DWORD EtpGetMofProviderText(
    _In_ GUID ProviderGuid,
    _In_ LPCWSTR ProviderName,
    _Out_ PWSTR * MofText);
