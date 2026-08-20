/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       ETWMOF.CPP
*
*  VERSION:     1.05
*
*  DATE:        17 Aug 2026
*
*  ETW MOF metadata reconstruction (best-effort).
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"

typedef struct _MOF_TEXT_CONTEXT {
    PTEXT_BUFFER TextBuffer;
    WCHAR BaseClassName[256];
} MOF_TEXT_CONTEXT, * PMOF_TEXT_CONTEXT;

/*
* EtpFormatCimType
*
* Purpose:
*
* Converts a WMI CIMTYPE value (which may include CIM_FLAG_ARRAY and
* other flags) to its MOF string representation.
*
* Examples:
*   CIM_UINT32                  -> "uint32"
*   CIM_UINT32 | CIM_FLAG_ARRAY -> "uint32[]"
*   CIM_STRING                  -> "string"
*   CIM_STRING | CIM_FLAG_ARRAY -> "string[]"
*
*/
VOID EtpFormatCimType(
    _In_ LONG CimType,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    LONG baseType;
    BOOL isArray;
    LPCWSTR typeStr;

    baseType = CimType & ~(LONG)CIM_FLAG_ARRAY;
    isArray = (CimType & (LONG)CIM_FLAG_ARRAY) != 0;
    typeStr = TEXT("uint32");

    switch (baseType) {

    case CIM_SINT8:
        typeStr = TEXT("sint8");
        break;

    case CIM_UINT8:
        typeStr = TEXT("uint8");
        break;

    case CIM_SINT16:
        typeStr = TEXT("sint16");
        break;

    case CIM_UINT16:
        typeStr = TEXT("uint16");
        break;

    case CIM_SINT32:
        typeStr = TEXT("sint32");
        break;

    case CIM_UINT32:
        typeStr = TEXT("uint32");
        break;

    case CIM_SINT64:
        typeStr = TEXT("sint64");
        break;

    case CIM_UINT64:
        typeStr = TEXT("uint64");
        break;

    case CIM_REAL32:
        typeStr = TEXT("real32");
        break;

    case CIM_REAL64:
        typeStr = TEXT("real64");
        break;

    case CIM_BOOLEAN:
        typeStr = TEXT("boolean");
        break;

    case CIM_STRING:
        typeStr = TEXT("string");
        break;

    case CIM_DATETIME:
        typeStr = TEXT("datetime");
        break;

    case CIM_CHAR16:
        typeStr = TEXT("char16");
        break;

    case CIM_OBJECT:
        typeStr = TEXT("object");
        break;

    default:
        typeStr = TEXT("uint32");
        break;
    }

    if (isArray) {
        StringCchPrintf(Buffer, BufferChars, TEXT("%s[]"), typeStr);
    }
    else {
        StringCchCopy(Buffer, BufferChars, typeStr);
    }
}

/*
* EtpFormatVariantForMof
*
* Purpose:
*
* Formats a WMI VARIANT into a string suitable for MOF qualifier output.
* Handles scalar types and one-dimensional SAFEARRAY types commonly
* found in ETW MOF qualifiers (ValueMap, Values, etc.).
*
* Returns TRUE if the variant was formatted, FALSE otherwise.
*
*/
BOOL EtpFormatVariantForMof(
    _In_ const VARIANT* Value,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    HRESULT hr;
    LONG index, lowerBound, upperBound;
    VARTYPE arrayType;
    SAFEARRAY* array;
    PWSTR cursor;
    SIZE_T remaining;
    WCHAR elementBuffer[64];

    if (!Value || !Buffer || BufferChars == 0)
        return FALSE;

    Buffer[0] = UNICODE_NULL;

    switch (Value->vt) {

    case VT_BSTR:
        StringCchPrintf(Buffer, BufferChars, TEXT("\"%s\""),
            Value->bstrVal ? Value->bstrVal : TEXT(""));
        return TRUE;

    case VT_I1:
        StringCchPrintf(Buffer, BufferChars, TEXT("%d"), Value->cVal);
        return TRUE;

    case VT_I2:
        StringCchPrintf(Buffer, BufferChars, TEXT("%d"), Value->iVal);
        return TRUE;

    case VT_I4:
        StringCchPrintf(Buffer, BufferChars, TEXT("%ld"), Value->lVal);
        return TRUE;

    case VT_I8:
        StringCchPrintf(Buffer, BufferChars, TEXT("%lld"), Value->llVal);
        return TRUE;

    case VT_UI1:
        StringCchPrintf(Buffer, BufferChars, TEXT("%u"), Value->bVal);
        return TRUE;

    case VT_UI2:
        StringCchPrintf(Buffer, BufferChars, TEXT("%u"), Value->uiVal);
        return TRUE;

    case VT_UI4:
        StringCchPrintf(Buffer, BufferChars, TEXT("%lu"), Value->ulVal);
        return TRUE;

    case VT_UI8:
        StringCchPrintf(Buffer, BufferChars, TEXT("%llu"), Value->ullVal);
        return TRUE;

    case VT_BOOL:
        StringCchCopy(Buffer, BufferChars,
            Value->boolVal ? TEXT("true") : TEXT("false"));
        return TRUE;

    case VT_NULL:
    case VT_EMPTY:
        return FALSE;

    default:
        break;
    }

    //
    // Handle one-dimensional SAFEARRAY types.
    // Common in ValueMap, Values, and similar qualifiers.
    //
    if ((Value->vt & VT_ARRAY) == 0)
        return FALSE;

    array = Value->parray;
    if (!array)
        return FALSE;

    if (SafeArrayGetDim(array) != 1)
        return FALSE;

    hr = SafeArrayGetVartype(array, &arrayType);
    if (FAILED(hr))
        return FALSE;

    hr = SafeArrayGetLBound(array, 1, &lowerBound);
    if (FAILED(hr))
        return FALSE;

    hr = SafeArrayGetUBound(array, 1, &upperBound);
    if (FAILED(hr))
        return FALSE;

    if (upperBound < lowerBound)
        return FALSE;

    //
    // Format as {"elem1", "elem2", ...}
    //
    cursor = Buffer;
    remaining = BufferChars;

    if (FAILED(StringCchCopyEx(cursor, remaining, TEXT("{"), &cursor, &remaining, 0)))
        return FALSE;

    for (index = lowerBound; index <= upperBound; index++) {

        if (index > lowerBound) {
            if (FAILED(StringCchCopyEx(cursor, remaining, TEXT(", "), &cursor, &remaining, 0)))
                return FALSE;
        }

        elementBuffer[0] = UNICODE_NULL;

        switch (arrayType) {

        case VT_BSTR: {
            BSTR element = NULL;
            hr = SafeArrayGetElement(array, &index, &element);
            if (FAILED(hr))
                return FALSE;
            StringCchPrintf(elementBuffer, ARRAYSIZE(elementBuffer), TEXT("\"%s\""), element ? element : TEXT(""));
            SysFreeString(element);
            break;
        }

        case VT_I4: {
            LONG element = 0;
            hr = SafeArrayGetElement(array, &index, &element);
            if (FAILED(hr))
                return FALSE;
            StringCchPrintf(elementBuffer, ARRAYSIZE(elementBuffer), TEXT("%ld"), element);
            break;
        }

        case VT_UI4: {
            ULONG element = 0;
            hr = SafeArrayGetElement(array, &index, &element);
            if (FAILED(hr))
                return FALSE;
            StringCchPrintf(elementBuffer, ARRAYSIZE(elementBuffer), TEXT("%lu"), element);
            break;
        }

        case VT_I2: {
            SHORT element = 0;
            hr = SafeArrayGetElement(array, &index, &element);
            if (FAILED(hr))
                return FALSE;
            StringCchPrintf(elementBuffer, ARRAYSIZE(elementBuffer),
                TEXT("%d"), element);
            break;
        }

        case VT_UI2: {
            USHORT element = 0;
            hr = SafeArrayGetElement(array, &index, &element);
            if (FAILED(hr))
                return FALSE;
            StringCchPrintf(elementBuffer, ARRAYSIZE(elementBuffer), TEXT("%u"), element);
            break;
        }

        case VT_UI1: {
            UCHAR element = 0;
            hr = SafeArrayGetElement(array, &index, &element);
            if (FAILED(hr))
                return FALSE;
            StringCchPrintf(elementBuffer, ARRAYSIZE(elementBuffer), TEXT("%u"), element);
            break;
        }

        case VT_BOOL: {
            VARIANT_BOOL element = VARIANT_FALSE;
            hr = SafeArrayGetElement(array, &index, &element);
            if (FAILED(hr))
                return FALSE;
            StringCchCopy(elementBuffer, ARRAYSIZE(elementBuffer), element ? TEXT("true") : TEXT("false"));
            break;
        }

        default:
            //
            // Unsupported array element type, skip.
            //
            continue;
        }

        if (FAILED(StringCchCopyEx(cursor, remaining, elementBuffer, &cursor, &remaining, 0)))
            return FALSE;
    }

    if (FAILED(StringCchCopyEx(cursor, remaining, TEXT("}"), &cursor, &remaining, 0)))
        return FALSE;

    return TRUE;
}

/*
* EtpBuildMofQualifierString
*
* Purpose:
*
* Iterates an IWbemQualifierSet and builds a comma-separated qualifier
* string suitable for MOF output.
*
*/
BOOL EtpBuildMofQualifierString(
    _In_ IWbemQualifierSet* QualifierSet,
    _In_opt_ LPCWSTR PrimaryQualifier,
    _In_reads_(SkipCount) LPCWSTR* SkipQualifiers,
    _In_ ULONG SkipCount,
    _Out_writes_(BufferChars) PWSTR Buffer,
    _In_ SIZE_T BufferChars
)
{
    HRESULT hr;
    ULONG i;
    BOOL skip;
    BSTR qualName = NULL;
    VARIANT qualValue;
    LONG flavor = 0;
    WCHAR valueStr[512];
    WCHAR tempLine[1024];
    PWSTR cursor;
    SIZE_T remaining;

    if (QualifierSet == NULL || Buffer == NULL || BufferChars == 0)
        return FALSE;

    Buffer[0] = UNICODE_NULL;
    cursor = Buffer;
    remaining = BufferChars;

    //
    // Emit the primary qualifier first.
    //
    if (PrimaryQualifier && PrimaryQualifier[0] != UNICODE_NULL) {
        if (FAILED(StringCchCopyEx(cursor, remaining, PrimaryQualifier, &cursor, &remaining, 0)))
            return FALSE;
    }

    hr = QualifierSet->BeginEnumeration(0);
    if (FAILED(hr))
        return TRUE;

    while (TRUE) {

        qualName = NULL;
        VariantInit(&qualValue);
        flavor = 0;

        hr = QualifierSet->Next(0, &qualName, &qualValue, &flavor);
        if (hr != WBEM_S_NO_ERROR)
            break;

        if (!qualName || qualName[0] == L'_') {
            if (qualName) SysFreeString(qualName);
            VariantClear(&qualValue);
            continue;
        }

        //
        // Check if this qualifier should be skipped.
        //
        skip = FALSE;
        for (i = 0; i < SkipCount; i++) {
            if (_wcsicmp(qualName, SkipQualifiers[i]) == 0) {
                skip = TRUE;
                break;
            }
        }

        if (skip) {
            SysFreeString(qualName);
            VariantClear(&qualValue);
            continue;
        }

        //
        // Handle the "dynamic" qualifier specially.
        // In MOF syntax it is written as "dynamic: ToInstance"
        // rather than "dynamic(true)".
        //
        if (_wcsicmp(qualName, TEXT("dynamic")) == 0) {

            tempLine[0] = UNICODE_NULL;

            if (cursor != Buffer) {
                StringCchCopy(tempLine, ARRAYSIZE(tempLine), TEXT(", "));
            }

            StringCchCat(tempLine, ARRAYSIZE(tempLine), TEXT("dynamic"));

            if (flavor & WBEM_FLAVOR_FLAG_PROPAGATE_TO_INSTANCE) {
                StringCchCat(tempLine, ARRAYSIZE(tempLine), TEXT(": ToInstance"));
            }

            if (flavor & WBEM_FLAVOR_FLAG_PROPAGATE_TO_DERIVED_CLASS) {
                if (flavor & WBEM_FLAVOR_FLAG_PROPAGATE_TO_INSTANCE) {
                    StringCchCat(tempLine, ARRAYSIZE(tempLine), TEXT(", ToSubclass"));
                }
                else {
                    StringCchCat(tempLine, ARRAYSIZE(tempLine), TEXT(": ToSubclass"));
                }
            }

            if (FAILED(StringCchCopyEx(cursor, remaining, tempLine, &cursor, &remaining, 0))) {
                SysFreeString(qualName);
                VariantClear(&qualValue);
                break;
            }

            SysFreeString(qualName);
            VariantClear(&qualValue);
            continue;
        }

        //
        // Format the qualifier value.
        //
        if (!EtpFormatVariantForMof(&qualValue, valueStr, ARRAYSIZE(valueStr))) {
            SysFreeString(qualName);
            VariantClear(&qualValue);
            continue;
        }

        //
        // Append "name(value)" or "name = value".
        // Use name(value) for strings and arrays, name(value) for numbers.
        //
        if (cursor != Buffer) {
            StringCchPrintf(tempLine, ARRAYSIZE(tempLine), TEXT(", %s(%s)"), qualName, valueStr);
        }
        else {
            StringCchPrintf(tempLine, ARRAYSIZE(tempLine), TEXT("%s(%s)"), qualName, valueStr);
        }

        if (FAILED(StringCchCopyEx(cursor, remaining, tempLine, &cursor, &remaining, 0))) {
            SysFreeString(qualName);
            VariantClear(&qualValue);
            break;
        }

        SysFreeString(qualName);
        VariantClear(&qualValue);
    }

    QualifierSet->EndEnumeration();
    return TRUE;
}

/*
* EtpGetMofProperties
*
* Purpose:
*
* Retrieves all non-system properties of a WMI class along with their
* WmiDataId and raw CIMTYPE (including array flags). Properties are
* sorted by WmiDataId to ensure correct binary layout order.
*
* The caller must free *Properties with supHeapFree.
*
*/
BOOL EtpGetMofProperties(
    _In_ IWbemClassObject* ClassObject,
    _Out_ PMOF_PROPERTY* Properties,
    _Out_ PULONG PropertyCount
)
{
    HRESULT hr;
    ULONG i, j, count;
    ULONG dataId;
    BSTR propertyName = NULL;
    PMOF_PROPERTY props = NULL;
    VARIANT variant;
    LONG cimType = 0;
    LONG flavor = 0;
    MOF_PROPERTY temporary;

    if (ClassObject == NULL || Properties == NULL || PropertyCount == NULL)
        return FALSE;

    *Properties = NULL;
    *PropertyCount = 0;

    props = (PMOF_PROPERTY)supHeapAlloc(
        ETP_MAX_MOF_PROPERTIES * sizeof(MOF_PROPERTY));

    if (!props)
        return FALSE;

    count = 0;

    hr = ClassObject->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY);
    if (FAILED(hr)) {
        supHeapFree(props);
        return FALSE;
    }

    while (TRUE) {

        propertyName = NULL;

        hr = ClassObject->Next(0,
            &propertyName,
            NULL,
            NULL,
            NULL);

        if (hr != WBEM_S_NO_ERROR)
            break;

        if (count < ETP_MAX_MOF_PROPERTIES) {

            dataId = 0;

            if (EtpGetWmiPropertyDataId(ClassObject,
                propertyName,
                &dataId))
            {
                props[count].dataId = dataId;

                StringCchCopy(props[count].name,
                    ARRAYSIZE(props[count].name),
                    propertyName);

                //
                // Retrieve the raw CIMTYPE including array flags.
                //
                VariantInit(&variant);

                hr = ClassObject->Get(propertyName,
                    0,
                    &variant,
                    &cimType,
                    &flavor);

                if (SUCCEEDED(hr)) {
                    props[count].cimType = cimType;
                }
                else {
                    props[count].cimType = CIM_UINT32;
                }

                VariantClear(&variant);
                count++;
            }
        }

        if (propertyName) {
            SysFreeString(propertyName);
            propertyName = NULL;
        }
    }

    ClassObject->EndEnumeration();

    //
    // Sort properties by dataId.
    //
    for (i = 0; i < count; i++) {
        for (j = i + 1; j < count; j++) {
            if (props[j].dataId < props[i].dataId) {
                temporary = props[i];
                props[i] = props[j];
                props[j] = temporary;
            }
        }
    }

    *Properties = props;
    *PropertyCount = count;

    return (count > 0);
}

/*
* EtpAppendMofEventClassToText
*
* Purpose:
*
* Formats a single WMI event class into MOF syntax and appends it to
* the text buffer. Includes all class and property qualifiers.
*
*/
BOOL EtpAppendMofEventClassToText(
    _In_ IWbemClassObject* ClassObject,
    _In_ PMOF_TEXT_CONTEXT Context
)
{
    ULONG j;
    ULONG eventType = 0;
    ULONG propertyCount = 0;
    ULONG cchFixedBuffer = PAGE_SIZE;
    BOOL result = FALSE;
    PMOF_PROPERTY properties = NULL;
    IWbemQualifierSet* qualifierSet = NULL;
    IWbemQualifierSet* propQualifierSet = NULL;
    PWSTR qualString = NULL;
    PWSTR propLine = NULL;
    WCHAR className[256];
    WCHAR typeStr[128];
    WCHAR primaryQual[256];
    LPCWSTR skipQuals[] = {
        TEXT("EventType"),
        TEXT("EventTypeName"),
        TEXT("WmiDataId")
    };

    if (ClassObject == NULL || Context == NULL)
        return FALSE;

    if (!EtpGetWmiClassName(ClassObject, className, ARRAYSIZE(className)))
        return FALSE;

    //
    // Only emit classes that have an EventType qualifier.
    //
    if (!EtpGetMofClassEventType(ClassObject, &eventType))
        return TRUE;

    do {
        qualString = (PWSTR)supHeapAlloc(cchFixedBuffer * sizeof(WCHAR));
        if (!qualString)
            break;

        propLine = (PWSTR)supHeapAlloc(cchFixedBuffer * sizeof(WCHAR));
        if (!propLine)
            break;

        //
        // Build class qualifier string.
        //
        if (FAILED(StringCchPrintf(primaryQual,
            ARRAYSIZE(primaryQual),
            TEXT("EventType(%lu)"),
            eventType)))
        {
            break;
        }

        qualString[0] = UNICODE_NULL;

        if (SUCCEEDED(ClassObject->GetQualifierSet(&qualifierSet))) {

            EtpBuildMofQualifierString(qualifierSet,
                primaryQual,
                skipQuals,
                ARRAYSIZE(skipQuals),
                qualString,
                cchFixedBuffer);

            qualifierSet->Release();
            qualifierSet = NULL;
        }
        else {
            if (FAILED(StringCchCopy(qualString,
                cchFixedBuffer,
                primaryQual)))
            {
                break;
            }
        }

        //
        // Emit class header.
        //
        if (!supTbAppend(Context->TextBuffer, TEXT("[")))
            break;

        if (!supTbAppend(Context->TextBuffer, qualString))
            break;

        if (!supTbAppend(Context->TextBuffer, TEXT("]\r\nclass ")))
            break;

        if (!supTbAppend(Context->TextBuffer, className))
            break;

        if (!supTbAppend(Context->TextBuffer, TEXT(" : ")))
            break;

        if (!supTbAppend(Context->TextBuffer, Context->BaseClassName))
            break;

        if (!supTbAppend(Context->TextBuffer, TEXT("\r\n{\r\n")))
            break;

        //
        // Emit properties with full qualifier sets.
        //
        if (EtpGetMofProperties(ClassObject,
            &properties,
            &propertyCount))
        {
            for (j = 0; j < propertyCount; j++) {

                //
                // Format the CIM type (handles arrays and flags).
                //
                EtpFormatCimType(properties[j].cimType, typeStr, ARRAYSIZE(typeStr));

                //
                // Build the property qualifier string.
                //
                if (FAILED(StringCchPrintf(primaryQual,
                    ARRAYSIZE(primaryQual),
                    TEXT("WmiDataId(%lu)"),
                    properties[j].dataId)))
                {
                    break;
                }

                qualString[0] = UNICODE_NULL;
                propQualifierSet = NULL;

                if (SUCCEEDED(ClassObject->GetPropertyQualifierSet(properties[j].name,
                    &propQualifierSet)))
                {
                    LPCWSTR propSkipQuals[] = {
                        TEXT("WmiDataId")
                    };

                    EtpBuildMofQualifierString(propQualifierSet,
                        primaryQual,
                        propSkipQuals,
                        ARRAYSIZE(propSkipQuals),
                        qualString,
                        cchFixedBuffer);

                    propQualifierSet->Release();
                    propQualifierSet = NULL;
                }
                else {
                    if (FAILED(StringCchCopy(qualString,
                        cchFixedBuffer,
                        primaryQual)))
                    {
                        break;
                    }
                }

                //
                // Emit the property line.
                //
                if (FAILED(StringCchPrintf(propLine,
                    cchFixedBuffer,
                    TEXT("    [%s] %s %s;\r\n"),
                    qualString,
                    typeStr,
                    properties[j].name)))
                {
                    break;
                }

                if (!supTbAppend(Context->TextBuffer, propLine))
                    break;
            }

            if (j != propertyCount)
                break;
        }

        if (!supTbAppend(Context->TextBuffer, TEXT("};\r\n\r\n")))
            break;

        result = TRUE;

    } while (FALSE);

    if (propQualifierSet)
        propQualifierSet->Release();

    if (qualifierSet)
        qualifierSet->Release();

    if (properties)
        supHeapFree(properties);

    if (propLine)
        supHeapFree(propLine);

    if (qualString)
        supHeapFree(qualString);

    return result;
}

/*
* EtpEnumerateMofDescendantsForText
*
* Purpose:
*
* Recursively enumerates WMI subclasses derived from the specified
* parent class and appends each event class to the MOF text buffer.
*
*/
HRESULT EtpEnumerateMofDescendantsForText(
    _In_ IWbemServices* Services,
    _In_ LPCWSTR ParentClassName,
    _In_ PMOF_TEXT_CONTEXT Context,
    _In_ ULONG Depth,
    _Inout_ PBOOL DepthLimitReached
)
{
    HRESULT hr, childStatus;
    ULONG returned;
    BSTR queryLanguage = NULL;
    BSTR queryString = NULL;
    IEnumWbemClassObject* enumerator = NULL;
    IWbemClassObject* childClass = NULL;
    WCHAR query[512];
    WCHAR childClassName[256];

    if (!Services || !ParentClassName || !Context || !DepthLimitReached)
        return E_INVALIDARG;

    if (Depth >= ETP_MAX_MOF_TEXT_DEPTH) {
        *DepthLimitReached = TRUE;
        return S_OK;
    }

    hr = StringCchPrintf(query,
        ARRAYSIZE(query),
        TEXT("SELECT * FROM meta_class WHERE __SUPERCLASS = '%s'"),
        ParentClassName);

    if (FAILED(hr))
        return hr;

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
        WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY,
        NULL,
        &enumerator);

    SysFreeString(queryString);
    SysFreeString(queryLanguage);

    if (FAILED(hr))
        return hr;

    while (TRUE) {

        returned = 0;
        childClass = NULL;

        hr = enumerator->Next(WBEM_INFINITE,
            1,
            &childClass,
            &returned);

        if (hr == WBEM_S_FALSE || returned == 0) {
            hr = S_OK;
            break;
        }

        if (FAILED(hr))
            break;

        EtpAppendMofEventClassToText(childClass, Context);

        if (EtpGetWmiClassName(childClass, childClassName, ARRAYSIZE(childClassName))) {
            childStatus = EtpEnumerateMofDescendantsForText(Services,
                childClassName,
                Context,
                Depth + 1,
                DepthLimitReached);

            if (FAILED(childStatus)) {
                hr = childStatus;
                childClass->Release();
                childClass = NULL;
                break;
            }
        }

        childClass->Release();
        childClass = NULL;
    }

    if (childClass)
        childClass->Release();

    if (enumerator)
        enumerator->Release();

    return hr;
}

/*
* EtpGetMofProviderText
*
* Purpose:
*
* Reconstruct the MOF definition associated with an ETW provider.
*
*/
DWORD EtpGetMofProviderText(
    _In_ GUID ProviderGuid,
    _In_ LPCWSTR ProviderName,
    _Out_ PWSTR* MofText
)
{
    HRESULT hr;
    ULONG returned;
    DWORD status;
    BOOL foundBaseClass;
    BOOL depthLimitReached;

    IWbemServices* services = NULL;
    IEnumWbemClassObject* enumerator = NULL;
    IWbemClassObject* classObject = NULL;
    IWbemQualifierSet* qualifierSet = NULL;

    BSTR queryLanguage = NULL;
    BSTR queryString = NULL;

    PVOID qualString = NULL;
    PVOID classLine = NULL;

    TEXT_BUFFER textBuffer;
    MOF_TEXT_CONTEXT context;
    WCHAR targetGuidStr[64];
    WCHAR baseClassName[256];
    WCHAR guidQualStr[128];

    if (!ProviderName || !MofText)
        return ERROR_INVALID_PARAMETER;

    *MofText = NULL;

    RtlSecureZeroMemory(&textBuffer, sizeof(textBuffer));
    RtlSecureZeroMemory(&context, sizeof(context));

    qualString = supHeapAlloc(PAGE_SIZE * sizeof(WCHAR));
    if (!qualString)
        return ERROR_NOT_ENOUGH_MEMORY;

    classLine = supHeapAlloc((PAGE_SIZE * 2) * sizeof(WCHAR));
    if (!classLine) {
        supHeapFree(qualString);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    if (!supTbInitialize(&textBuffer, PAGE_SIZE)) {
        supHeapFree(classLine);
        supHeapFree(qualString);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    if (0 == StringFromGUID2(ProviderGuid, targetGuidStr, ARRAYSIZE(targetGuidStr))) {
        supTbDestroy(&textBuffer);
        supHeapFree(classLine);
        supHeapFree(qualString);
        return ERROR_INVALID_DATA;
    }

    //
    // Connect to WMI (ROOT\WMI).
    // EtpConnectWmi handles CoSetProxyBlanket internally.
    //
    if (!EtpConnectWmi(&services)) {
        supTbDestroy(&textBuffer);
        supHeapFree(classLine);
        supHeapFree(qualString);
        return ERROR_GEN_FAILURE;
    }

    foundBaseClass = FALSE;
    depthLimitReached = FALSE;

    //
    // Enumerate all WMI classes to find the base provider class
    // (the class whose Guid qualifier matches our target GUID).
    //
    queryLanguage = SysAllocString(TEXT("WQL"));
    if (queryLanguage == NULL) {
        services->Release();
        supTbDestroy(&textBuffer);
        supHeapFree(classLine);
        supHeapFree(qualString);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    queryString = SysAllocString(TEXT("SELECT * FROM meta_class"));
    if (queryString == NULL) {
        SysFreeString(queryLanguage);
        services->Release();
        supTbDestroy(&textBuffer);
        supHeapFree(classLine);
        supHeapFree(qualString);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    hr = services->ExecQuery(
        queryLanguage,
        queryString,
        WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY,
        NULL,
        &enumerator);

    SysFreeString(queryString);
    SysFreeString(queryLanguage);

    if (FAILED(hr)) {
        services->Release();
        supTbDestroy(&textBuffer);
        supHeapFree(classLine);
        supHeapFree(qualString);

        if (HRESULT_FACILITY(hr) == FACILITY_WIN32)
            return HRESULT_CODE(hr);

        if (hr == E_OUTOFMEMORY)
            return ERROR_NOT_ENOUGH_MEMORY;

        return ERROR_GEN_FAILURE;
    }

    while (TRUE) {

        returned = 0;
        classObject = NULL;

        hr = enumerator->Next(WBEM_INFINITE, 1, &classObject, &returned);
        if (hr == WBEM_S_FALSE || returned == 0) {
            hr = S_OK;
            break;
        }

        if (FAILED(hr)) {
            if (classObject)
                classObject->Release();

            enumerator->Release();
            services->Release();
            supTbDestroy(&textBuffer);
            supHeapFree(classLine);
            supHeapFree(qualString);

            if (HRESULT_FACILITY(hr) == FACILITY_WIN32)
                return HRESULT_CODE(hr);

            if (hr == E_OUTOFMEMORY)
                return ERROR_NOT_ENOUGH_MEMORY;

            return ERROR_GEN_FAILURE;
        }

        if (EtpWmiClassGuidMatchesProvider(classObject, &ProviderGuid)) {
            if (EtpGetWmiClassName(classObject,
                baseClassName,
                ARRAYSIZE(baseClassName)))
            {
                foundBaseClass = TRUE;

                //
                // Build the full class qualifier string for the
                // base provider class.
                //
                status = StringCchPrintf(guidQualStr,
                    ARRAYSIZE(guidQualStr),
                    TEXT("Guid(\"%s\")"),
                    targetGuidStr);

                if (FAILED(status)) {
                    classObject->Release();
                    enumerator->Release();
                    services->Release();
                    supTbDestroy(&textBuffer);
                    supHeapFree(classLine);
                    supHeapFree(qualString);

                    return ERROR_INSUFFICIENT_BUFFER;
                }

                ((PWSTR)qualString)[0] = UNICODE_NULL;

                if (SUCCEEDED(classObject->GetQualifierSet(&qualifierSet))) {
                    LPCWSTR classSkipQuals[] = {
                        TEXT("Guid"),
                        TEXT("dynamic"),
                        TEXT("provider")
                    };

                    EtpBuildMofQualifierString(qualifierSet,
                        guidQualStr,
                        classSkipQuals,
                        ARRAYSIZE(classSkipQuals),
                        (PWSTR)qualString,
                        4096);

                    qualifierSet->Release();
                    qualifierSet = NULL;
                }
                else {
                    status = StringCchCopy((PWSTR)qualString,
                        PAGE_SIZE,
                        guidQualStr);

                    if (FAILED(status)) {
                        classObject->Release();
                        enumerator->Release();
                        services->Release();
                        supTbDestroy(&textBuffer);
                        supHeapFree(classLine);
                        supHeapFree(qualString);

                        return ERROR_INSUFFICIENT_BUFFER;
                    }
                }

                //
                // Write MOF header and base class definition.
                //
                if (!supTbAppend(&textBuffer,
                    TEXT("#pragma namespace(\"\\\\\\\\.\\\\root\\\\WMI\")\r\n\r\n")))
                {
                    classObject->Release();
                    enumerator->Release();
                    services->Release();
                    supTbDestroy(&textBuffer);
                    supHeapFree(classLine);
                    supHeapFree(qualString);

                    return ERROR_NOT_ENOUGH_MEMORY;
                }

                status = StringCchPrintf((PWSTR)classLine,
                    8192,
                    TEXT("[Guid(\"%s\"), dynamic: ToInstance, provider(\"%s\")%s%s]\r\n"
                        TEXT("class %s : EventTrace\r\n{\r\n};\r\n\r\n")),
                    targetGuidStr,
                    ProviderName,
                    (((PWSTR)qualString)[0] != UNICODE_NULL) ?
                    TEXT(", ") : TEXT(""),
                    (PWSTR)qualString,
                    baseClassName);

                if (FAILED(status)) {
                    classObject->Release();
                    enumerator->Release();
                    services->Release();
                    supTbDestroy(&textBuffer);
                    supHeapFree(classLine);
                    supHeapFree(qualString);

                    return ERROR_INSUFFICIENT_BUFFER;
                }

                if (!supTbAppend(&textBuffer, (PWSTR)classLine)) {
                    classObject->Release();
                    enumerator->Release();
                    services->Release();
                    supTbDestroy(&textBuffer);
                    supHeapFree(classLine);
                    supHeapFree(qualString);

                    return ERROR_NOT_ENOUGH_MEMORY;
                }

                //
                // Set up context for recursive enumeration.
                //
                context.TextBuffer = &textBuffer;

                status = StringCchCopy(context.BaseClassName,
                    ARRAYSIZE(context.BaseClassName),
                    baseClassName);

                if (FAILED(status)) {
                    classObject->Release();
                    enumerator->Release();
                    services->Release();
                    supTbDestroy(&textBuffer);
                    supHeapFree(classLine);
                    supHeapFree(qualString);

                    return ERROR_INSUFFICIENT_BUFFER;
                }
            }

            classObject->Release();
            classObject = NULL;
            break;
        }

        classObject->Release();
        classObject = NULL;
    }

    if (classObject)
        classObject->Release();

    if (enumerator)
        enumerator->Release();

    //
    // Recursively enumerate and emit all descendant event classes.
    //
    if (foundBaseClass) {
        hr = EtpEnumerateMofDescendantsForText(services,
            baseClassName,
            &context,
            0,
            &depthLimitReached);

        if (FAILED(hr)) {
            services->Release();
            supTbDestroy(&textBuffer);
            supHeapFree(classLine);
            supHeapFree(qualString);

            if (HRESULT_FACILITY(hr) == FACILITY_WIN32)
                return HRESULT_CODE(hr);

            if (hr == E_OUTOFMEMORY)
                return ERROR_NOT_ENOUGH_MEMORY;

            return ERROR_GEN_FAILURE;
        }
    }

    services->Release();

    if (!foundBaseClass) {
        supTbDestroy(&textBuffer);
        supHeapFree(classLine);
        supHeapFree(qualString);
        return ERROR_NOT_FOUND;
    }

    *MofText = textBuffer.Buffer;
    textBuffer.Buffer = NULL;

    supTbDestroy(&textBuffer);
    supHeapFree(classLine);
    supHeapFree(qualString);

    return ERROR_SUCCESS;
}
