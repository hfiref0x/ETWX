/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2025 - 2026
*
*  TITLE:       PRTL.CPP
*
*  VERSION:     1.05
*
*  DATE:        14 Aug 2026
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "prtl.h"

/*
* supStrChrW
*
* Purpose:
*
* Returns a pointer to the first occurrence of Character in String
* (including the terminator if Character is 0), or NULL if not found.
*
*/
LPWSTR supStrChrW(
    _In_z_ LPCWSTR String,
    _In_ WCHAR Character
)
{
    while (*String != UNICODE_NULL) {
        if (*String == Character)
            return (LPWSTR)String;
        String++;
    }

    if (Character == UNICODE_NULL)
        return (LPWSTR)String;

    return NULL;
}

/*
* supStrChrA
*
* Purpose:
*
* Returns a pointer to the first occurrence of Character in String
* (including the terminator if Character is 0), or NULL if not found.
*
*/
LPSTR supStrChrA(
    _In_z_ LPCSTR String,
    _In_ CHAR Character
)
{
    while (*String != ANSI_NULL) {
        if (*String == Character)
            return (LPSTR)String;
        String++;
    }

    if (Character == ANSI_NULL)
        return (LPSTR)String;

    return NULL;
}

/*
* supStrLenA
*
* Purpose:
*
* Returns the length, in characters, of a null-terminated ANSI string, or 0 if the pointer is NULL.
*
*/
SIZE_T supStrLenA(
    _In_opt_ LPCSTR String
)
{
    LPCSTR String0 = String;

    if (String == NULL)
        return 0;

    while (*String != ANSI_NULL)
        String++;

    return (SIZE_T)(String - String0);
}

/*
* supStrLenW
*
* Purpose:
*
* Returns the length, in characters, of a null-terminated UTF-16 string, or 0 if the pointer is NULL.
*
*/
SIZE_T supStrLenW(
    _In_opt_ LPCWSTR String
)
{
    LPCWSTR String0 = String;

    if (String == NULL)
        return 0;

    while (*String != UNICODE_NULL)
        String++;

    return (SIZE_T)(String - String0);
}

/*
* supStrCmpA
*
* Purpose:
*
* Performs a case-sensitive comparison of two null-terminated ANSI strings.
*
*/
INT supStrCmpA(
    _In_opt_ LPCSTR String1,
    _In_opt_ LPCSTR String2
)
{
    CHAR c1, c2;

    if (String1 == String2)
        return 0;

    if (String1 == NULL)
        return -1;

    if (String2 == NULL)
        return 1;

    do {
        c1 = *String1;
        c2 = *String2;
        String1++;
        String2++;
    } while ((c1 != ANSI_NULL) && (c1 == c2));

    return (INT)(c1 - c2);
}

/*
* supStrCmpW
*
* Purpose:
*
* Performs a case-sensitive comparison of two null-terminated UTF-16 strings.
*
*/
INT supStrCmpW(
    _In_opt_ LPCWSTR String1,
    _In_opt_ LPCWSTR String2
)
{
    WCHAR c1, c2;

    if (String1 == String2)
        return 0;

    if (String1 == NULL)
        return -1;

    if (String2 == NULL)
        return 1;

    do {
        c1 = *String1;
        c2 = *String2;
        String1++;
        String2++;
    } while ((c1 != UNICODE_NULL) && (c1 == c2));

    return (INT)(c1 - c2);
}

/*
* supStrCmpIA
*
* Purpose:
*
* Performs a case-insensitive comparison of two null-terminated ANSI strings.
*
*/
INT supStrCmpIA(
    _In_opt_ LPCSTR String1,
    _In_opt_ LPCSTR String2
)
{
    CHAR c1, c2;

    if (String1 == String2)
        return 0;

    if (String1 == NULL)
        return -1;

    if (String2 == NULL)
        return 1;

    do {
        c1 = supLowerCharA(*String1);
        c2 = supLowerCharA(*String2);
        String1++;
        String2++;
    } while ((c1 != ANSI_NULL) && (c1 == c2));

    return (INT)(c1 - c2);
}

/*
* supStrCmpIW
*
* Purpose:
*
* Performs a case-insensitive comparison of two null-terminated UTF-16 strings.
*
*/
INT supStrCmpIW(
    _In_opt_ LPCWSTR String1,
    _In_opt_ LPCWSTR String2
)
{
    WCHAR c1, c2;

    if (String1 == String2)
        return 0;

    if (String1 == NULL)
        return -1;

    if (String2 == NULL)
        return 1;

    do {
        c1 = supLowerCharW(*String1);
        c2 = supLowerCharW(*String2);
        String1++;
        String2++;
    } while ((c1 != UNICODE_NULL) && (c1 == c2));

    return (INT)(c1 - c2);
}

/*
* supStrCopyA
*
* Purpose:
*
* Copies a null-terminated ANSI string from Source to Destination
* (unbounded, caller-guaranteed capacity).
*
*/
LPSTR supStrCopyA(
    _Out_writes_z_(_String_length_(Source) + 1) LPSTR Destination,
    _In_z_ LPCSTR Source
)
{
    LPSTR p;

    if ((Destination == NULL) || (Source == NULL))
        return Destination;

    p = Destination;

    while (*Source != ANSI_NULL) {
        *p = *Source;
        p++;
        Source++;
    }

    *p = ANSI_NULL;

    return Destination;
}

/*
* supStrCopyW
*
* Purpose:
*
* Copies a null-terminated wide-character (UTF-16) string from Source to Destination
* (unbounded, caller-guaranteed capacity)
*
*/
LPWSTR supStrCopyW(
    _Out_writes_z_(_String_length_(Source) + 1) LPWSTR Destination,
    _In_z_ LPCWSTR Source
)
{
    LPWSTR p;

    if ((Destination == NULL) || (Source == NULL))
        return Destination;

    p = Destination;

    while (*Source != UNICODE_NULL) {
        *p = *Source;
        p++;
        Source++;
    }

    *p = UNICODE_NULL;

    return Destination;
}

/*
* supStrNCopyA
*
* Purpose:
*
* Copies up to SourceCount characters from an ANSI source string
* into a fixed-size destination buffer, always null-terminating.
*
*/
LPSTR supStrNCopyA(
    _Out_writes_z_(DestinationCount) LPSTR Destination,
    _In_ SIZE_T DestinationCount,
    _In_reads_(SourceCount) LPCSTR Source,
    _In_ SIZE_T SourceCount
)
{
    LPSTR p;

    if ((Destination == NULL) || (Source == NULL) || (DestinationCount == 0))
        return Destination;

    DestinationCount--;
    p = Destination;

    while ((*Source != ANSI_NULL) &&
        (DestinationCount > 0) &&
        (SourceCount > 0))
    {
        *p = *Source;
        p++;
        Source++;
        DestinationCount--;
        SourceCount--;
    }

    *p = ANSI_NULL;

    return Destination;
}

/*
* supStrNCopyW
*
* Purpose:
*
* Copies up to SourceCount characters from a UTF-16 source string
* into a fixed-size destination buffer, always null-terminating.
*
*/
LPWSTR supStrNCopyW(
    _Out_writes_z_(DestinationCount) LPWSTR Destination,
    _In_ SIZE_T DestinationCount,
    _In_reads_(SourceCount) LPCWSTR Source,
    _In_ SIZE_T SourceCount
)
{
    LPWSTR p;

    if ((Destination == NULL) || (Source == NULL) || (DestinationCount == 0))
        return Destination;

    DestinationCount--;
    p = Destination;

    while ((*Source != UNICODE_NULL) &&
        (DestinationCount > 0) &&
        (SourceCount > 0))
    {
        *p = *Source;
        p++;
        Source++;
        DestinationCount--;
        SourceCount--;
    }

    *p = UNICODE_NULL;

    return Destination;
}

INT supStrNCmpA(
    _In_opt_ LPCSTR String1,
    _In_opt_ LPCSTR String2,
    _In_ SIZE_T Count
)
{
    CHAR c1, c2;

    if (String1 == String2)
        return 0;

    if (String1 == NULL)
        return -1;

    if (String2 == NULL)
        return 1;

    if (Count == 0)
        return 0;

    do {
        c1 = *String1;
        c2 = *String2;
        String1++;
        String2++;
        Count--;
    } while ((c1 != ANSI_NULL) &&
        (c1 == c2) &&
        (Count > 0));

    return (INT)(c1 - c2);
}

INT supStrNCmpW(
    _In_opt_ LPCWSTR String1,
    _In_opt_ LPCWSTR String2,
    _In_ SIZE_T Count
)
{
    WCHAR c1, c2;

    if (String1 == String2)
        return 0;

    if (String1 == NULL)
        return -1;

    if (String2 == NULL)
        return 1;

    if (Count == 0)
        return 0;

    do {
        c1 = *String1;
        c2 = *String2;
        String1++;
        String2++;
        Count--;
    } while ((c1 != UNICODE_NULL) &&
        (c1 == c2) &&
        (Count > 0));

    return (INT)(c1 - c2);
}

/*
* supStrStrIA
*
* Purpose:
*
* Case insensitive string search.
*
*/
LPCSTR supStrStrIA(
    _In_ LPCSTR String,
    _In_ LPCSTR SubString
)
{
    CHAR c0, c1, c2;
    LPCSTR tmpString;
    LPCSTR tmpSubString;

    if (String == SubString)
        return String;

    if (String == NULL)
        return NULL;

    if (SubString == NULL)
        return NULL;

    //
    // Empty substring matches at beginning.
    //
    if (*SubString == 0)
        return String;

    c0 = supLowerCharA(*SubString);

    while (c0 != 0) {

        while (*String != 0) {

            c2 = supLowerCharA(*String);

            if (c2 == c0)
                break;

            String++;
        }

        if (*String == 0)
            return NULL;

        tmpString = String;
        tmpSubString = SubString;

        do {

            c1 = supLowerCharA(*tmpString);
            c2 = supLowerCharA(*tmpSubString);

            tmpString++;
            tmpSubString++;

        } while ((c1 == c2) && (c2 != 0));


        if (c2 == 0)
            return String;

        String++;
    }

    return NULL;
}

/*
* supStrStrIW
*
* Purpose:
*
* Case insensitive string search.
*
*/
LPCWSTR supStrStrIW(
    _In_ LPCWSTR String,
    _In_ LPCWSTR SubString
)
{
    WCHAR c0, c1, c2;
    LPCWSTR tmpString;
    LPCWSTR tmpSubString;

    if (String == SubString)
        return String;

    if (String == NULL)
        return NULL;

    if (SubString == NULL)
        return NULL;

    //
    // Empty substring matches at beginning.
    //
    if (*SubString == 0)
        return String;

    c0 = supLowerCharW(*SubString);

    while (c0 != 0) {

        while (*String != 0) {

            c2 = supLowerCharW(*String);

            if (c2 == c0)
                break;

            String++;
        }

        if (*String == 0)
            return NULL;

        tmpString = String;
        tmpSubString = SubString;

        do {

            c1 = supLowerCharW(*tmpString);
            c2 = supLowerCharW(*tmpSubString);

            tmpString++;
            tmpSubString++;

        } while ((c1 == c2) && (c2 != 0));


        if (c2 == 0)
            return String;

        String++;
    }

    return NULL;
}

/*
* supStrStrA
*
* Purpose:
*
* Case sensitive string search.
*
*/
LPCSTR supStrStrA(
    _In_ LPCSTR String,
    _In_ LPCSTR SubString
)
{
    CHAR c0, c1, c2;
    LPCSTR tmpString;
    LPCSTR tmpSubString;

    if (String == SubString)
        return String;

    if (String == NULL)
        return NULL;

    if (SubString == NULL)
        return NULL;

    //
    // Empty substring matches at beginning.
    //
    if (*SubString == 0)
        return String;

    c0 = *SubString;

    while (c0 != 0) {

        while (*String != 0) {

            c2 = *String;

            if (c2 == c0)
                break;

            String++;
        }

        if (*String == 0)
            return NULL;

        tmpString = String;
        tmpSubString = SubString;

        do {

            c1 = *tmpString;
            c2 = *tmpSubString;

            tmpString++;
            tmpSubString++;

        } while ((c1 == c2) && (c2 != 0));


        if (c2 == 0)
            return String;

        String++;
    }

    return NULL;
}

/*
* supStrStrW
*
* Purpose:
*
* Case sensitive string search.
*
*/
LPCWSTR supStrStrW(
    _In_ LPCWSTR String,
    _In_ LPCWSTR SubString
)
{
    WCHAR c0, c1, c2;
    LPCWSTR tmpString;
    LPCWSTR tmpSubString;

    if (String == SubString)
        return String;

    if (String == NULL)
        return NULL;

    if (SubString == NULL)
        return NULL;

    //
    // Empty substring matches at beginning.
    //
    if (*SubString == 0)
        return String;

    c0 = *SubString;

    while (c0 != 0) {

        while (*String != 0) {

            c2 = *String;

            if (c2 == c0)
                break;

            String++;
        }

        if (*String == 0)
            return NULL;

        tmpString = String;
        tmpSubString = SubString;

        do {

            c1 = *tmpString;
            c2 = *tmpSubString;

            tmpString++;
            tmpSubString++;

        } while ((c1 == c2) && (c2 != 0));


        if (c2 == 0)
            return String;

        String++;
    }

    return NULL;
}

/*
* supStrCatA
*
* Purpose:
*
* Append source string to destination string.
*
*/
LPSTR supStrCatA(
    _Inout_ LPSTR Destination,
    _In_ LPCSTR Source
)
{
    if ((Destination == NULL) || (Source == NULL))
        return Destination;

    while (*Destination != 0)
        Destination++;

    while (*Source != 0) {

        *Destination = *Source;

        Destination++;
        Source++;
    }

    *Destination = 0;

    return Destination;
}


/*
* supStrCatW
*
* Purpose:
*
* Append source string to destination string.
*
*/
LPWSTR supStrCatW(
    _Inout_ LPWSTR Destination,
    _In_ LPCWSTR Source
)
{
    if ((Destination == NULL) || (Source == NULL))
        return Destination;

    while (*Destination != 0)
        Destination++;

    while (*Source != 0) {

        *Destination = *Source;

        Destination++;
        Source++;
    }

    *Destination = 0;

    return Destination;
}

/*
* supStrCatExA
*
* Purpose:
*
* Append source string to destination string.
*
* Destination buffer is always NULL terminated if size permits.
*
*/
LPSTR supStrCatExA(
    _Inout_updates_z_(DestinationCount) LPSTR Destination,
    _In_ SIZE_T DestinationCount,
    _In_ LPCSTR Source
)
{
    LPSTR p;

    if ((Destination == NULL) ||
        (Source == NULL) ||
        (DestinationCount == 0))
    {
        return Destination;
    }

    p = Destination;
    while ((*p != 0) && (DestinationCount > 1)) {
        p++;
        DestinationCount--;
    }

    while ((*Source != 0) && (DestinationCount > 1)) {

        *p = *Source;

        p++;
        Source++;

        DestinationCount--;
    }

    *p = 0;
    return Destination;
}

/*
* supStrCatExW
*
* Purpose:
*
* Append source string to destination string.
*
* Destination buffer is always NULL terminated if size permits.
*
*/
LPWSTR supStrCatExW(
    _Inout_updates_z_(DestinationCount) LPWSTR Destination,
    _In_ SIZE_T DestinationCount,
    _In_ LPCWSTR Source
)
{
    LPWSTR p;

    if ((Destination == NULL) ||
        (Source == NULL) ||
        (DestinationCount == 0))
    {
        return Destination;
    }

    p = Destination;
    while ((*p != 0) && (DestinationCount > 1)) {
        p++;
        DestinationCount--;
    }

    while ((*Source != 0) && (DestinationCount > 1)) {

        *p = *Source;

        p++;
        Source++;

        DestinationCount--;
    }

    *p = 0;
    return Destination;
}

/*
* supStrToUInt64A
*
* Purpose:
*
* Converts a decimal ANSI string to its ULONGLONG value.
*
*/
BOOL supStrToUInt64A(
    _In_ LPCSTR String,
    _Out_ PULONGLONG Value
)
{
    ULONGLONG result = 0;
    ULONGLONG digit;

    *Value = 0;

    if (!String || !*String)
        return FALSE;

    while (*String) {

        if (!supIsDigitA(*String))
            return FALSE;

        digit = (ULONGLONG)(*String - '0');

        if (result > (ULLONG_MAX - digit) / 10)
            return FALSE;

        result = result * 10 + digit;
        String++;
    }

    *Value = result;

    return TRUE;
}

/*
* supStrToUInt64W
*
* Purpose:
*
* Converts a UTF-16 string to its ULONGLONG value.
*
*/
BOOL supStrToUInt64W(
    _In_ LPCWSTR String,
    _Out_ PULONGLONG Value
)
{
    ULONGLONG result = 0;
    ULONGLONG digit;

    *Value = 0;

    if (!String || !*String)
        return FALSE;

    while (*String) {

        if (!supIsDigitW(*String))
            return FALSE;

        digit = (ULONGLONG)(*String - L'0');

        if (result > (ULLONG_MAX - digit) / 10)
            return FALSE;

        result = result * 10 + digit;
        String++;
    }

    *Value = result;

    return TRUE;
}

/*
* supUInt64ToStrW
*
* Purpose:
*
* Converts a ULONGLONG value to its UTF-16 string representation.
*
*/
SIZE_T supUInt64ToStrW(
    _In_ ULONGLONG Value,
    _Out_writes_z_(BufferCount) LPWSTR Buffer,
    _In_ SIZE_T BufferCount
)
{
    WCHAR temp[21];
    SIZE_T length;
    SIZE_T i;

    if (!Buffer || BufferCount == 0)
        return 0;

    Buffer[0] = 0;

    if (Value == 0) {
        if (BufferCount < 2)
            return 0;

        Buffer[0] = L'0';
        Buffer[1] = 0;
        return 1;
    }

    length = 0;

    while (Value != 0 && length < RTL_NUMBER_OF(temp)) {
        temp[length++] = (WCHAR)(L'0' + (Value % 10));
        Value /= 10;
    }

    if (Value != 0) //msvc shut up
        return 0;

    if ((length + 1) > BufferCount)
        return 0;

    for (i = 0; i < length; i++)
        Buffer[i] = temp[length - i - 1];

    Buffer[length] = 0;

    return length;
}

/*
* supUInt64ToStrA
*
* Purpose:
*
* Converts a ULONGLONG value to its ANSI string representation.
*
*/
SIZE_T supUInt64ToStrA(
    _In_ ULONGLONG Value,
    _Out_writes_z_(BufferCount) LPSTR Buffer,
    _In_ SIZE_T BufferCount
)
{
    CHAR temp[21];
    SIZE_T length;
    SIZE_T i;

    if (!Buffer || BufferCount == 0)
        return 0;

    Buffer[0] = 0;

    if (Value == 0) {
        if (BufferCount < 2)
            return 0;

        Buffer[0] = '0';
        Buffer[1] = 0;

        return 1;
    }

    length = 0;

    while (Value != 0 && length < RTL_NUMBER_OF(temp)) {
        temp[length++] = (CHAR)('0' + (Value % 10));
        Value /= 10;
    }

    if (Value != 0) //msvc shut up
        return 0;

    if ((length + 1) > BufferCount)
        return 0;

    for (i = 0; i < length; i++)
        Buffer[i] = temp[length - i - 1];

    Buffer[length] = 0;

    return length;
}

/*
* supHexToUInt64A
*
* Purpose:
*
* Converts a ANSI hexademical string to the ULONGLONG value.
*
*/
ULONGLONG supHexToUInt64A(
    _In_opt_ LPCSTR String
)
{
    INT digit;
    ULONGLONG value = 0;

    if (!String)
        return 0;

    while (*String) {

        digit = supHexDigitToInt((UCHAR)*String);
        if (digit < 0)
            break;

        value = (value << 4) | (ULONG)digit;
        String++;
    }

    return value;
}

/*
* supHexToUInt64W
*
* Purpose:
*
* Converts a UTF-16 hexademical string to the ULONGLONG value.
*
*/
ULONGLONG supHexToUInt64W(
    _In_opt_ LPCWSTR String
)
{
    INT digit;
    ULONGLONG value = 0;

    if (!String)
        return 0;

    while (*String) {

        digit = supHexDigitToInt((WCHAR)*String);
        if (digit < 0)
            break;

        value = (value << 4) | (ULONG)digit;
        String++;
    }

    return value;
}
