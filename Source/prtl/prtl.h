/************************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2025 - 2026 UGN/HE
*
*  TITLE:       PRTL.H
*
*  VERSION:     1.05
*
*  DATE:        15 Aug 2026
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
************************************************************************************/

#define ENABLE_C_EXTERN

#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef _PRTL_
#define _PRTL_

#pragma warning(push)
#pragma warning(disable: 4201) // nameless struct/union
#pragma warning(disable: 4214) // nonstandard extension used : bit field types other than int
#pragma warning(disable: 26812) // enum type % is unscoped

#ifndef _WINDOWS_
#include <Windows.h>
#endif

FORCEINLINE INT supHexDigitToInt(
    _In_ UINT ch
)
{
    if ((ch >= '0') && (ch <= '9'))
        return (INT)(ch - '0');

    ch = (UINT)((ch >= 'A' && ch <= 'Z') ? (ch + ('a' - 'A')) : ch);

    if ((ch >= 'a') && (ch <= 'f'))
        return (INT)(ch - 'a' + 10);

    return -1;
}

FORCEINLINE BOOLEAN supIsDigitA(
    _In_ CHAR Ch
)
{
    return (BOOLEAN)(Ch >= '0' && Ch <= '9');
}

FORCEINLINE BOOLEAN supIsDigitW(
    _In_ WCHAR Ch
)
{
    return (BOOLEAN)(Ch >= L'0' && Ch <= L'9');
}

//
// Minirtl section START
//
FORCEINLINE CHAR supLowerCharA(
    _In_ CHAR c
)
{
    if ((c >= 'A') && (c <= 'Z'))
        return c + 0x20;
    else
        return c;
}

FORCEINLINE WCHAR supLowerCharW(
    _In_ WCHAR c
)
{
    if ((c >= L'A') && (c <= L'Z'))
        return c + 0x20;
    else
        return c;
}

LPWSTR supStrChrW(
    _In_z_ LPCWSTR String,
    _In_ WCHAR Character);
LPSTR supStrChrA(
    _In_z_ LPCSTR String,
    _In_ CHAR Character);

SIZE_T supStrLenA(
    _In_opt_ LPCSTR String);
SIZE_T supStrLenW(
    _In_opt_ LPCWSTR String);

INT supStrCmpA(
    _In_opt_ LPCSTR String1,
    _In_opt_ LPCSTR String2);
INT supStrCmpW(
    _In_opt_ LPCWSTR String1,
    _In_opt_ LPCWSTR String2);

INT supStrCmpIA(
    _In_opt_ LPCSTR String1,
    _In_opt_ LPCSTR String2);
INT supStrCmpIW(
    _In_opt_ LPCWSTR String1,
    _In_opt_ LPCWSTR String2);

LPSTR supStrCopyA(
    _Out_writes_z_(_String_length_(Source) + 1) LPSTR Destination,
    _In_z_ LPCSTR Source);
LPWSTR supStrCopyW(
    _Out_writes_z_(_String_length_(Source) + 1) LPWSTR Destination,
    _In_z_ LPCWSTR Source);

LPSTR supStrNCopyA(
    _Out_writes_z_(DestinationCount) LPSTR Destination,
    _In_ SIZE_T DestinationCount,
    _In_reads_(SourceCount) LPCSTR Source,
    _In_ SIZE_T SourceCount);
LPWSTR supStrNCopyW(
    _Out_writes_z_(DestinationCount) LPWSTR Destination,
    _In_ SIZE_T DestinationCount,
    _In_reads_(SourceCount) LPCWSTR Source,
    _In_ SIZE_T SourceCount);

INT supStrNCmpA(
    _In_opt_ LPCSTR String1,
    _In_opt_ LPCSTR String2,
    _In_ SIZE_T Count);
INT supStrNCmpW(
    _In_opt_ LPCWSTR String1,
    _In_opt_ LPCWSTR String2,
    _In_ SIZE_T Count);

LPCSTR supStrStrIA(
    _In_ LPCSTR String,
    _In_ LPCSTR SubString);
LPCWSTR supStrStrIW(
    _In_ LPCWSTR String,
    _In_ LPCWSTR SubString);

LPCSTR supStrStrA(
    _In_ LPCSTR String,
    _In_ LPCSTR SubString);
LPCWSTR supStrStrW(
    _In_ LPCWSTR String,
    _In_ LPCWSTR SubString);

LPSTR supStrCatA(
    _Inout_ LPSTR Destination,
    _In_ LPCSTR Source);
LPWSTR supStrCatW(
    _Inout_ LPWSTR Destination,
    _In_ LPCWSTR Source);

LPSTR supStrCatExA(
    _Inout_updates_z_(DestinationCount) LPSTR Destination,
    _In_ SIZE_T DestinationCount,
    _In_ LPCSTR Source);
LPWSTR supStrCatExW(
    _Inout_updates_z_(DestinationCount) LPWSTR Destination,
    _In_ SIZE_T DestinationCount,
    _In_ LPCWSTR Source);

BOOL supStrToUInt64A(
    _In_ LPCSTR String,
    _Out_ PULONGLONG Value);
BOOL supStrToUInt64W(
    _In_ LPCWSTR String,
    _Out_ PULONGLONG Value);

SIZE_T supUInt64ToStrW(
    _In_ ULONGLONG Value,
    _Out_writes_z_(BufferCount) LPWSTR Buffer,
    _In_ SIZE_T BufferCount);
SIZE_T supUInt64ToStrA(
    _In_ ULONGLONG Value,
    _Out_writes_z_(BufferCount) LPSTR Buffer,
    _In_ SIZE_T BufferCount);

ULONGLONG supHexToUInt64A(
    _In_opt_ LPCSTR String);
ULONGLONG supHexToUInt64W(
    _In_opt_ LPCWSTR String);

#ifdef _UNICODE
#define supLowerChar supLowerCharW
#define supStrChr supStrChrW
#define supStrLen supStrLenW
#define supStrCmp supStrCmpW
#define supStrCmpI supStrCmpIW
#define supStrCopy supStrCopyW
#define supStrNCopy supStrNCopyW
#define supStrNCmp supStrNCmpW
#define supStrStrI supStrStrIW
#define supStrStr supStrStrW
#define supStrCat supStrCatW
#define supStrCatEx supStrCatExW
#define supStrToUInt64 supStrToUInt64W
#define supUInt64ToStr supUInt64ToStrW
#define supHexToUInt64 supHexToUInt64W
#define supIsDigit supIsDigitW
#else
#define supLowerChar supLowerCharA
#define supStrChr supStrChrA
#define supStrLen supStrLenA
#define supStrCmp supStrCmpA
#define supStrCmpI supStrCmpIA
#define supStrCopy supStrCopyA
#define supStrNCopy supStrNCopyA
#define supStrNCmp supStrNCmpA
#define supStrStrI supStrStrIA
#define supStrStr supStrStrA
#define supStrCat supStrCatA
#define supStrCatEx supStrCatExA
#define supStrToUInt64 supStrToUInt64A
#define supUInt64ToStr supUInt64ToStrA
#define supHexToUInt64 supHexToUInt64A
#define supIsDigit supIsDigitA
#endif

#endif /* _PRTL_ */
