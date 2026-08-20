/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       SECURITY.H
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
*  Common header file a security dialog implementation.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#pragma once

typedef struct _ETWX_SECURITY_INFORMATION_VTBL {
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(
        _In_ ISecurityInformation* This,
        _In_ REFIID riid,
        _Out_ PVOID* ppvObject);

    ULONG(STDMETHODCALLTYPE* AddRef)(
        _In_ ISecurityInformation* This);

    ULONG(STDMETHODCALLTYPE* Release)(
        _In_ ISecurityInformation* This);

    HRESULT(STDMETHODCALLTYPE* GetObjectInformation)(
        _In_ ISecurityInformation* This,
        _Out_ PSI_OBJECT_INFO pObjectInfo);

    HRESULT(STDMETHODCALLTYPE* GetSecurity)(
        _In_ ISecurityInformation* This,
        _In_ SECURITY_INFORMATION RequestedInformation,
        _Out_ PSECURITY_DESCRIPTOR* ppSecurityDescriptor,
        _In_ BOOL fDefault);

    HRESULT(STDMETHODCALLTYPE* SetSecurity)(
        _In_ ISecurityInformation* This,
        _In_ SECURITY_INFORMATION SecurityInformation,
        _In_ PSECURITY_DESCRIPTOR pSecurityDescriptor);

    HRESULT(STDMETHODCALLTYPE* GetAccessRights)(
        _In_ ISecurityInformation* This,
        _In_ const GUID* pguidObjectType,
        _In_ DWORD dwFlags,
        _Out_ PSI_ACCESS* ppAccess,
        _Out_ ULONG* pcAccesses,
        _Out_ ULONG* piDefaultAccess);

    HRESULT(STDMETHODCALLTYPE* MapGeneric)(
        _In_ ISecurityInformation* This,
        _In_ const GUID* pguidObjectType,
        _In_ UCHAR* pAceFlags,
        _Inout_ ACCESS_MASK* pMask);

    HRESULT(STDMETHODCALLTYPE* GetInheritTypes)(
        _In_ ISecurityInformation* This,
        _Out_ PSI_INHERIT_TYPE* ppInheritTypes,
        _Out_ ULONG* pcInheritTypes);

    HRESULT(STDMETHODCALLTYPE* PropertySheetPageCallback)(
        _In_ HWND hwnd,
        _In_ UINT uMsg,
        _In_ SI_PAGE_TYPE uPage);
} ETWX_SECURITY_INFORMATION_VTBL;

typedef enum _ETWX_SECURITY_OBJECT_TYPE {
    EtwxSecurityProvider,
    EtwxSecuritySession
} ETWX_SECURITY_OBJECT_TYPE;

typedef struct _ETWX_SECURITY_CONTEXT {
    ETWX_SECURITY_INFORMATION_VTBL* lpVtbl;
    LONG ReferenceCount;
    GUID ObjectGuid;
    ETWX_SECURITY_OBJECT_TYPE ObjectType;
} ETWX_SECURITY_CONTEXT, * PETWX_SECURITY_CONTEXT;

VOID SecurityRunDialogForEtwObject(
    _In_ LPGUID ObjectGuid,
    _In_ ETWX_SECURITY_OBJECT_TYPE ObjectType);
