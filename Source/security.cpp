/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       SECURITY.CPP
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
*  Security dialog implementation for ETWX.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"

static const SI_ACCESS g_EtwProviderAccess[] = {
    {
        &GUID_NULL,
        TRACELOG_GUID_ENABLE,
        TEXT("Enable provider"),
        SI_ACCESS_GENERAL
    },
    {
        &GUID_NULL,
        TRACELOG_REGISTER_GUIDS,
        TEXT("Register provider"),
        SI_ACCESS_GENERAL
    },
    {
        &GUID_NULL,
        TRACELOG_LOG_EVENT,
        TEXT("Log event"),
        SI_ACCESS_GENERAL
    }
};

static const SI_ACCESS g_EtwSessionAccess[] = {
    {
        &GUID_NULL,
        WMIGUID_QUERY,
        TEXT("Query session"),
        SI_ACCESS_GENERAL
    },
    {
        &GUID_NULL,
        TRACELOG_CREATE_REALTIME,
        TEXT("Start/update real-time"),
        SI_ACCESS_GENERAL
    },
    {
        &GUID_NULL,
        TRACELOG_CREATE_ONDISK,
        TEXT("Start/update on-disk"),
        SI_ACCESS_GENERAL
    },
    {
        &GUID_NULL,
        TRACELOG_ACCESS_REALTIME,
        TEXT("Consume real-time"),
        SI_ACCESS_GENERAL
    }
};

/*
* EtwxSecurityAddRef
*
* Purpose:
*
* Increments the reference count of an ETW security information object.
*
*/
ULONG STDMETHODCALLTYPE EtwxSecurityAddRef(
    _In_ ISecurityInformation* This
)
{
    PETWX_SECURITY_CONTEXT context;

    context = (PETWX_SECURITY_CONTEXT)This;
    return (ULONG)InterlockedIncrement(&context->ReferenceCount);
}

/*
* EtwxSecurityQueryInterface
*
* Purpose:
*
* Returns a requested interface implemented by the ETW security information
* object.
*
*/
HRESULT STDMETHODCALLTYPE EtwxSecurityQueryInterface(
    _In_ ISecurityInformation* This,
    _In_ REFIID riid,
    _Out_ PVOID* ppvObject
)
{
    if (!ppvObject)
        return E_POINTER;

    *ppvObject = NULL;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ISecurityInformation))
    {
        *ppvObject = This;

        EtwxSecurityAddRef(This);

        return S_OK;
    }

    return E_NOINTERFACE;
}

/*
* EtwxSecurityRelease
*
* Purpose:
*
* Decrements the reference count of an ETW security information object.
*
*/
ULONG STDMETHODCALLTYPE EtwxSecurityRelease(
    _In_ ISecurityInformation* This
)
{
    LONG referenceCount;
    PETWX_SECURITY_CONTEXT context;

    context = (PETWX_SECURITY_CONTEXT)This;

    referenceCount = InterlockedDecrement(&context->ReferenceCount);
    if (referenceCount == 0)
        supHeapFree(context);

    return (ULONG)referenceCount;
}

/*
* EtwxSecurityGetObjectInformation
*
* Purpose:
*
* Supplies general information about the ETW object to the Windows security
* property sheet.
*
*/
HRESULT STDMETHODCALLTYPE EtwxSecurityGetObjectInformation(
    _In_ ISecurityInformation* This,
    _Out_ PSI_OBJECT_INFO pObjectInfo
)
{
    UNREFERENCED_PARAMETER(This);

    if (!pObjectInfo)
        return E_POINTER;

    RtlSecureZeroMemory(pObjectInfo, sizeof(SI_OBJECT_INFO));
    pObjectInfo->dwFlags = 0;
    pObjectInfo->hInstance = g_ctx.hInstance;
    pObjectInfo->pszObjectName = (LPWSTR)TEXT("ETW object");

    return S_OK;
}

/*
* EtwxSecurityGetSecurity
*
* Purpose:
*
* Retrieves the security descriptor associated with the ETW provider or
* session represented by the security information object.
*
*/
HRESULT STDMETHODCALLTYPE EtwxSecurityGetSecurity(
    _In_ ISecurityInformation* This,
    _In_ SECURITY_INFORMATION RequestedInformation,
    _Out_ PSECURITY_DESCRIPTOR* ppSecurityDescriptor,
    _In_ BOOL fDefault
)
{
    PETWX_SECURITY_CONTEXT context;
    PSECURITY_DESCRIPTOR securityDescriptor;
    ULONG securityDescriptorSize;
    ULONG status;

    UNREFERENCED_PARAMETER(RequestedInformation);
    UNREFERENCED_PARAMETER(fDefault);

    if (!ppSecurityDescriptor)
        return E_POINTER;

    *ppSecurityDescriptor = NULL;

    context = (PETWX_SECURITY_CONTEXT)This;

    securityDescriptor = NULL;
    securityDescriptorSize = 0;

    status = EventAccessQuery(&context->ObjectGuid, NULL, &securityDescriptorSize);
    if (status != ERROR_MORE_DATA)
        return HRESULT_FROM_WIN32(status);

    securityDescriptor = (PSECURITY_DESCRIPTOR)supHeapAlloc(securityDescriptorSize);
    if (!securityDescriptor)
        return E_OUTOFMEMORY;

    status = EventAccessQuery(&context->ObjectGuid, securityDescriptor, &securityDescriptorSize);
    if (status != ERROR_SUCCESS) {
        supHeapFree(securityDescriptor);
        return HRESULT_FROM_WIN32(status);
    }

    *ppSecurityDescriptor = securityDescriptor;

    return S_OK;
}

/*
* SecurityApplyEtwObjectDacl
*
* Purpose:
*
* Applies the supplied DACL to an ETW object.
*
*/
BOOL SecurityApplyEtwObjectDacl(
    _In_ LPGUID ProviderGuid,
    _In_ PACL Dacl,
    _Out_ PULONG ErrorCode
)
{
    BOOLEAN allowOrDeny;
    BOOL result;
    ULONG operation, i, error, rights;
    PVOID ace;
    PACE_HEADER aceHeader;
    PACCESS_ALLOWED_ACE allowedAce;
    PACCESS_DENIED_ACE deniedAce;
    PSID sid;
    WCHAR buffer[256];

    if (!ProviderGuid ||
        !Dacl ||
        !ErrorCode)
    {
        return FALSE;
    }

    *ErrorCode = ERROR_SUCCESS;

    //
    // Validate the complete DACL before modifying the provider.
    //
    for (i = 0; i < Dacl->AceCount; i++) {

        ace = NULL;
        if (!GetAce(Dacl, i, &ace)) {
            *ErrorCode = GetLastError();
            return FALSE;
        }

        aceHeader = (PACE_HEADER)ace;

        switch (aceHeader->AceType) {

        case ACCESS_ALLOWED_ACE_TYPE:
            break;

        case ACCESS_DENIED_ACE_TYPE:
            break;

        default:

            StringCchPrintf(buffer,
                ARRAYSIZE(buffer),
                L"[Security] Unsupported ACE type: %u\r\n",
                aceHeader->AceType);

            OutputDebugString(buffer);
            *ErrorCode = ERROR_NOT_SUPPORTED;
            return FALSE;
        }
    }

    //
    // An empty DACL cannot be represented by EventAccessControl,
    // because there is no ACE on which EventSecuritySetDACL can operate.
    //
    if (Dacl->AceCount == 0) {
        OutputDebugString(L"[Security] Empty DACL cannot be applied through "
            L"EventAccessControl.\r\n");

        *ErrorCode = ERROR_NOT_SUPPORTED;
        return FALSE;
    }

    //
    // Apply the validated DACL.
    //
    for (i = 0; i < Dacl->AceCount; i++) {

        ace = NULL;
        if (!GetAce(Dacl, i, &ace)) {
            *ErrorCode = GetLastError();
            return FALSE;
        }

        aceHeader = (PACE_HEADER)ace;
        if (aceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE) {
            allowedAce = (PACCESS_ALLOWED_ACE)ace;
            sid = (PSID)&allowedAce->SidStart;
            rights = allowedAce->Mask;
            allowOrDeny = TRUE;
        }
        else {
            deniedAce = (PACCESS_DENIED_ACE)ace;
            sid = (PSID)&deniedAce->SidStart;
            rights = deniedAce->Mask;
            allowOrDeny = FALSE;
        }

        operation = (i == 0) ? EventSecuritySetDACL : EventSecurityAddDACL;

        error = EventAccessControl(ProviderGuid, operation, sid, rights, allowOrDeny);
        if (error != ERROR_SUCCESS) {

            StringCchPrintf(buffer,
                ARRAYSIZE(buffer),
                L"[Security] EventAccessControl failed: "
                L"ACE=%lu Operation=%lu Rights=0x%08lX "
                L"Allow=%u Error=%lu\r\n",
                i,
                operation,
                rights,
                allowOrDeny,
                error);

            OutputDebugString(buffer);

            *ErrorCode = error;

            return FALSE;
        }
    }

    result = TRUE;

    return result;
}

/*
* EtwxSecuritySetSecurity
*
* Purpose:
*
* Applies the modified DACL from the Windows security property sheet to
* the represented ETW object.
*
* The current implementation supports DACL changes only.
*
*/
HRESULT EtwxSecuritySetSecurity(
    _In_ ISecurityInformation* This,
    _In_ SECURITY_INFORMATION SecurityInformation,
    _In_ PSECURITY_DESCRIPTOR SecurityDescriptor
)
{
    BOOL present;
    BOOL defaulted;
    BOOL result;
    DWORD error;
    PACL dacl;
    PETWX_SECURITY_CONTEXT context;

    UNREFERENCED_PARAMETER(SecurityInformation);

    context = (PETWX_SECURITY_CONTEXT)This;

    if (!context ||
        !SecurityDescriptor)
    {
        return E_INVALIDARG;
    }

    dacl = NULL;

    if (!GetSecurityDescriptorDacl(SecurityDescriptor,
        &present,
        &dacl,
        &defaulted))
    {
        error = GetLastError();

        return HRESULT_FROM_WIN32(error);
    }

    if (!present || !dacl)
        return E_FAIL;

    result = SecurityApplyEtwObjectDacl(&context->ObjectGuid, dacl, &error);
    if (!result)
        return HRESULT_FROM_WIN32(error);

    return S_OK;
}

/*
* EtwxSecurityGetAccessRights
*
* Purpose:
*
* Returns the access rights supported by the ETW provider or session represented
* by the security information object.
*
*/
HRESULT STDMETHODCALLTYPE EtwxSecurityGetAccessRights(
    _In_ ISecurityInformation* This,
    _In_ const GUID* pguidObjectType,
    _In_ DWORD dwFlags,
    _Out_ PSI_ACCESS* ppAccess,
    _Out_ ULONG* pcAccesses,
    _Out_ ULONG* piDefaultAccess
)
{
    PETWX_SECURITY_CONTEXT context;

    UNREFERENCED_PARAMETER(pguidObjectType);
    UNREFERENCED_PARAMETER(dwFlags);

    if (ppAccess == NULL ||
        pcAccesses == NULL ||
        piDefaultAccess == NULL)
    {
        return E_POINTER;
    }

    *ppAccess = NULL;
    *pcAccesses = 0;
    *piDefaultAccess = 0;

    context = (PETWX_SECURITY_CONTEXT)This;
    if (context == NULL)
        return E_POINTER;

    switch (context->ObjectType) {

    case EtwxSecurityProvider:
        *ppAccess = (PSI_ACCESS)g_EtwProviderAccess;
        *pcAccesses = ARRAYSIZE(g_EtwProviderAccess);
        break;
    case EtwxSecuritySession:
        *ppAccess = (PSI_ACCESS)g_EtwSessionAccess;
        *pcAccesses = ARRAYSIZE(g_EtwSessionAccess);
        break;
    default:
        return E_INVALIDARG;
    }

    return S_OK;
}

/*
* EtwxSecurityMapGeneric
*
* Purpose:
*
* Maps generic Windows access rights to the specific access rights supported
* by the represented ETW provider or ETW session.
*
*/
HRESULT STDMETHODCALLTYPE EtwxSecurityMapGeneric(
    _In_ ISecurityInformation* This,
    _In_ const GUID* pguidObjectType,
    _In_ UCHAR* pAceFlags,
    _Inout_ ACCESS_MASK* pMask
)
{
    PETWX_SECURITY_CONTEXT context;
    GENERIC_MAPPING mapping;

    UNREFERENCED_PARAMETER(This);
    UNREFERENCED_PARAMETER(pguidObjectType);
    UNREFERENCED_PARAMETER(pAceFlags);

    if (pMask == NULL)
        return E_POINTER;

    context = (PETWX_SECURITY_CONTEXT)This;
    if (context == NULL)
        return E_POINTER;

    if (context->ObjectType == EtwxSecurityProvider) {

        mapping.GenericRead = TRACELOG_GUID_ENABLE;
        mapping.GenericWrite = TRACELOG_REGISTER_GUIDS;
        mapping.GenericExecute = 0;
        mapping.GenericAll =
            TRACELOG_GUID_ENABLE |
            TRACELOG_REGISTER_GUIDS |
            TRACELOG_LOG_EVENT;

    }
    else if (context->ObjectType == EtwxSecuritySession) {

        mapping.GenericRead = WMIGUID_QUERY;
        mapping.GenericWrite =
            TRACELOG_CREATE_REALTIME |
            TRACELOG_CREATE_ONDISK;
        mapping.GenericExecute = TRACELOG_ACCESS_REALTIME;
        mapping.GenericAll =
            WMIGUID_QUERY |
            TRACELOG_CREATE_REALTIME |
            TRACELOG_CREATE_ONDISK |
            TRACELOG_ACCESS_REALTIME |
            TRACELOG_JOIN_GROUP;

    }
    else {
        return E_INVALIDARG;
    }

    MapGenericMask(pMask, &mapping);
    return S_OK;
}

/*
* EtwxSecurityGetInheritTypes
*
* Purpose:
*
* Indicates that ETW provider and session security objects do not support
* ACE inheritance.
*
*/
HRESULT STDMETHODCALLTYPE EtwxSecurityGetInheritTypes(
    _In_ ISecurityInformation* This,
    _Out_ PSI_INHERIT_TYPE* ppInheritTypes,
    _Out_ ULONG* pcInheritTypes
)
{
    UNREFERENCED_PARAMETER(This);

    if (!ppInheritTypes ||
        !pcInheritTypes)
    {
        return E_POINTER;
    }

    *ppInheritTypes = NULL;
    *pcInheritTypes = 0;

    return S_OK;
}

/*
* EtwxSecurityPropertySheetPageCallback
*
* Purpose:
*
* Handles notifications from the Windows security property sheet.
*
*/
HRESULT STDMETHODCALLTYPE EtwxSecurityPropertySheetPageCallback(
    _In_ HWND hwnd,
    _In_ UINT uMsg,
    _In_ SI_PAGE_TYPE uPage
)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(uPage);

    return S_OK;
}

static ETWX_SECURITY_INFORMATION_VTBL g_EtwSecurityInformationVtbl = {
    EtwxSecurityQueryInterface,
    EtwxSecurityAddRef,
    EtwxSecurityRelease,
    EtwxSecurityGetObjectInformation,
    EtwxSecurityGetSecurity,
    EtwxSecuritySetSecurity,
    EtwxSecurityGetAccessRights,
    EtwxSecurityMapGeneric,
    EtwxSecurityGetInheritTypes,
    EtwxSecurityPropertySheetPageCallback
};

/*
* SecurityCanQueryEtwObject
*
* Purpose:
*
* Determines whether the current process can retrieve the security descriptor
* of an ETW object.
*
* If access is denied and the process is not elevated, the user is offered
* the option to restart ETW Explorer with administrator privileges.
*
*/
BOOL SecurityCanQueryEtwObject(
    _In_ LPGUID ObjectGuid,
    _In_ LPCWSTR ObjectName
)
{
    INT result;
    ULONG status;
    ULONG securityDescriptorSize;
    WCHAR message[512];

    if (!ObjectGuid)
        return FALSE;

    //
    // An elevated process can query the security descriptor directly.
    //
    if (g_ctx.isAdmin)
        return TRUE;

    securityDescriptorSize = 0;

    status = EventAccessQuery(ObjectGuid, NULL, &securityDescriptorSize);
    if (status == ERROR_ACCESS_DENIED) {

        StringCchPrintf(message,
            ARRAYSIZE(message),
            TEXT("Access to the %s security information was denied.\r\n\r\n"
                "ETW Explorer needs administrator privileges to view this information.\r\n\r\n"
                "Do you want to restart ETW Explorer with administrator privileges?"),
            ObjectName);

        result = MessageBox(g_ctx.hMainWnd,
            message,
            TEXT("ETW Security"),
            MB_YESNO | MB_ICONWARNING);

        if (result == IDYES)
            supRunAsAdministrator(g_ctx.hMainWnd);

        return FALSE;
    }

    if (status != ERROR_MORE_DATA &&
        status != ERROR_SUCCESS)
    {
        StringCchPrintf(message,
            ARRAYSIZE(message),
            TEXT("Unable to query %s security information."),
            ObjectName);

        MessageBox(g_ctx.hMainWnd,
            message,
            TEXT("ETW Security"),
            MB_OK | MB_ICONWARNING);

        return FALSE;
    }

    return TRUE;
}

/*
* SecurityRunDialogForEtwObject
*
* Purpose:
*
* Displays the Windows security property sheet for the specified ETW provider
* or session.
*
*/
VOID SecurityRunDialogForEtwObject(
    _In_ LPGUID ObjectGuid,
    _In_ ETWX_SECURITY_OBJECT_TYPE ObjectType
)
{
    LPCWSTR objectName;
    PETWX_SECURITY_CONTEXT context;

    if (!ObjectGuid)
        return;

    if (ObjectType == EtwxSecurityProvider)
        objectName = TEXT("provider");
    else if (ObjectType == EtwxSecuritySession)
        objectName = TEXT("session");
    else
        return;

    if (!SecurityCanQueryEtwObject(ObjectGuid, objectName))
        return;

    context = (PETWX_SECURITY_CONTEXT)supHeapAlloc(sizeof(ETWX_SECURITY_CONTEXT));
    if (!context)
        return;

    context->lpVtbl = &g_EtwSecurityInformationVtbl;
    context->ReferenceCount = 1;
    context->ObjectGuid = *ObjectGuid;
    context->ObjectType = ObjectType;

    EditSecurity(g_ctx.hMainWnd, (ISecurityInformation*)context);
    EtwxSecurityRelease((ISecurityInformation*)context);
}
