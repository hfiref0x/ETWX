/*******************************************************************************
*
*  (C) COPYRIGHT hfiref0x, 2026
*
*  TITLE:       COMMONDLG.CPP
*
*  VERSION:     1.05
*
*  DATE:        19 Aug 2026
*
*  Common dialogs handlers including:
*  About, filtering, event details/metadata, and event search dialogs.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/

#include "global.h"

static HWND g_hEventDetailsDlg = NULL;
static ETWX_DIALOG_CONTEXT g_eventDetailsDlgContext;

/*
* InitializeMetadataDetailsDialog
*
* Purpose:
*
* Initialize controls for ETW metadata viewer and set data display.
*
*/
LONG InitializeMetadataDetailsDialog(
    _In_ HWND hwnd,
    _In_ PETWX_DIALOG_CONTEXT Context
)
{
    INT dpi;
    HDC hdc;
    WCHAR buffer[512];

    StringCchPrintf(buffer, RTL_NUMBER_OF(buffer), TEXT("Provider: %s"), Context->pszProviderName);
    supCreateControl(0,
        WC_STATIC,
        buffer,
        WS_CHILD | WS_VISIBLE,
        12,
        12,
        800,
        20,
        hwnd,
        NULL);

    Context->hEdit = supCreateControl(WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
        12,
        40,
        826,
        510,
        hwnd,
        NULL);

    if (Context->hEdit == NULL)
        return -1;

    SendMessage(Context->hEdit, EM_EXLIMITTEXT, 0, (LPARAM)0x7FFFFFFE);

    hdc = GetDC(hwnd);

    if (hdc) {
        dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hwnd, hdc);
    }
    else {
        dpi = USER_DEFAULT_SCREEN_DPI;
    }

    Context->hFixedFont = CreateFont(-MulDiv(9, dpi, 72),
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | FF_MODERN,
        TEXT("Consolas"));

    if (Context->hFixedFont) {
        SendMessage(Context->hEdit, WM_SETFONT, (WPARAM)Context->hFixedFont, TRUE);
    }

    Context->hCloseButton = supCreateControl(0,
        WC_BUTTON,
        TEXT("Close"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_DEFPUSHBUTTON,
        758,
        558,
        80,
        26,
        hwnd,
        (HMENU)(INT_PTR)IDCANCEL);

    if (Context->hCloseButton == NULL)
        return -1;

    return 0;
}

/*
* InitializeSystemInformationDialog
*
* Purpose:
*
* Initializes the ETW system information dialog and builds a
* formatted summary of the current ETW subsystem state.
*
*/
BOOL InitializeSystemInformationDialog(
    _In_ HWND hwnd,
    _In_ PETWX_DIALOG_CONTEXT Context
)
{
    TEXT_BUFFER textBuffer;
    WCHAR buffer[512];
    HWND hEdit;
    HWND hCloseButton;
    double utilization;

    if (hwnd == NULL || Context == NULL)
        return FALSE;

    RtlSecureZeroMemory(&textBuffer, sizeof(textBuffer));

    if (!supTbInitialize(&textBuffer, PAGE_SIZE))
        return FALSE;

    if (!supTbAppend(&textBuffer,
        TEXT("ETW System Information\r\n")
        TEXT("=======================\r\n")
        TEXT("\r\n")
        TEXT("Sessions\r\n")
        TEXT("--------\r\n")))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (FAILED(StringCchPrintf(buffer,
        ARRAYSIZE(buffer),
        TEXT("  Active sessions:     %lu\r\n")
        TEXT("  Normal sessions:     %lu\r\n")
        TEXT("  Kernel sessions:     %lu\r\n")
        TEXT("  Private sessions:    %lu\r\n")
        TEXT("  Real-time sessions:  %lu\r\n")
        TEXT("  File sessions:       %lu\r\n"),
        g_ctx.etwSystemInformation.ActiveSessionCount,
        g_ctx.etwSystemInformation.NormalSessionCount,
        g_ctx.etwSystemInformation.KernelSessionCount,
        g_ctx.etwSystemInformation.PrivateSessionCount,
        g_ctx.etwSystemInformation.RealTimeSessionCount,
        g_ctx.etwSystemInformation.FileSessionCount)))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer, buffer) ||
        !supTbAppend(&textBuffer,
            TEXT("\r\nBuffers\r\n")
            TEXT("-------\r\n")))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (FAILED(StringCchPrintf(buffer,
        ARRAYSIZE(buffer),
        TEXT("  Total buffers:       %lu\r\n")
        TEXT("  Free buffers:        %lu\r\n")
        TEXT("  Buffers in use:      %lu\r\n"),
        g_ctx.etwSystemInformation.TotalBuffers,
        g_ctx.etwSystemInformation.FreeBuffers,
        g_ctx.etwSystemInformation.TotalBuffers -
        g_ctx.etwSystemInformation.FreeBuffers)))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer, buffer)) {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (g_ctx.etwSystemInformation.TotalBuffers != 0) {

        utilization = ((double)(g_ctx.etwSystemInformation.TotalBuffers -
            g_ctx.etwSystemInformation.FreeBuffers) *
            100.0) /
            (double)g_ctx.etwSystemInformation.TotalBuffers;

        if (FAILED(StringCchPrintf(buffer,
            ARRAYSIZE(buffer),
            TEXT("  Buffer utilization:  %.1f%%\r\n"),
            utilization)))
        {
            supTbDestroy(&textBuffer);
            return FALSE;
        }

        if (!supTbAppend(&textBuffer, buffer)) {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }
    else {
        if (!supTbAppend(&textBuffer,
            TEXT("  Buffer utilization:  N/A\r\n")))
        {
            supTbDestroy(&textBuffer);
            return FALSE;
        }
    }

    if (FAILED(StringCchPrintf(buffer,
        ARRAYSIZE(buffer),
        TEXT("  Events lost:         %lu\r\n"),
        g_ctx.etwSystemInformation.EventsLost)))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer, buffer)) {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer,
        TEXT("\r\n")
        TEXT("Kernel Logger\r\n")
        TEXT("-------------\r\n")
        TEXT("  Status:              ")))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer,
        g_ctx.etwSystemInformation.KernelLoggerActive ?
        TEXT("Active\r\n") :
        TEXT("Inactive\r\n")))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer,
        TEXT("\r\n")
        TEXT("Provider Information\r\n")
        TEXT("--------------------\r\n")))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (FAILED(StringCchPrintf(
        buffer,
        ARRAYSIZE(buffer),
        TEXT("  Registered providers:     %lu\r\n")
        TEXT("  Manifest providers:       %lu\r\n")
        TEXT("  Non-manifest providers:   %lu\r\n"),
        g_ctx.etwSystemInformation.ProviderCount,
        g_ctx.etwSystemInformation.ManifestProviderCount,
        g_ctx.etwSystemInformation.NonManifestProviderCount)))
    {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    if (!supTbAppend(&textBuffer, buffer)) {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    hEdit = CreateWindowEx(WS_EX_CLIENTEDGE,
        TEXT("EDIT"),
        NULL,
        WS_CHILD |
        WS_VISIBLE |
        WS_VSCROLL |
        ES_MULTILINE |
        ES_READONLY |
        ES_AUTOVSCROLL,
        12,
        12,
        596,
        340,
        hwnd,
        NULL,
        g_ctx.hInstance,
        NULL);

    if (hEdit == NULL) {
        supTbDestroy(&textBuffer);
        return FALSE;
    }

    Context->hEdit = hEdit;

    Context->hFixedFont = CreateFont(
        -11,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | FF_MODERN,
        TEXT("Consolas"));

    if (Context->hFixedFont) {
        SendMessage(hEdit, WM_SETFONT, (WPARAM)Context->hFixedFont, TRUE);
    }

    Context->pszText = textBuffer.Buffer;
    Context->cchText = textBuffer.Length;
    textBuffer.Buffer = NULL;
    supTbDestroy(&textBuffer);

    SetWindowText(hEdit, Context->pszText);

    hCloseButton = CreateWindowEx(0,
        TEXT("BUTTON"),
        TEXT("Close"),
        WS_CHILD |
        WS_VISIBLE |
        BS_DEFPUSHBUTTON,
        528,
        362,
        80,
        26,
        hwnd,
        (HMENU)IDCANCEL,
        g_ctx.hInstance,
        NULL);

    if (hCloseButton == NULL)
        return FALSE;

    Context->hCloseButton = hCloseButton;
    supApplyUIFont(hCloseButton);

    return TRUE;
}

/*
* EtwxCommonDialogProc
*
* Purpose:
*
* Common window procedure for ETW Explorer modal dialogs.
*
*/
LRESULT CALLBACK EtwxCommonDialogProc(
    _In_ HWND hwnd,
    _In_ UINT msg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    INT width, height;
    UINT command;
    HMENU hMenu;
    HWND ownerWindow;
    POINT point;
    CHARRANGE selection;

    LPMINMAXINFO minMaxInfo;
    PETWX_DIALOG_CONTEXT context;

    switch (msg) {

    case WM_NCCREATE:

        SetWindowLongPtr(hwnd, GWLP_USERDATA,
            (LONG_PTR)((CREATESTRUCT*)lParam)->lpCreateParams);

        break; //this is intentional, do not change.

    case WM_CREATE:

        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (context == NULL)
            break;

        if (context->Type == EtwxDialogMetadata) {
            return InitializeMetadataDetailsDialog(hwnd, context);
        }
        if (context->Type == EtwxDialogSystemInformation) {
            return InitializeSystemInformationDialog(hwnd, context);
        }
        return 0;

    case WM_GETMINMAXINFO:

        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (context &&
            context->Type == EtwxDialogMetadata)
        {
            minMaxInfo = (LPMINMAXINFO)lParam;
            minMaxInfo->ptMinTrackSize.x = 500;
            minMaxInfo->ptMinTrackSize.y = 300;
        }
        return 0;

    case WM_SIZE:

        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (context == NULL)
            break;

        if (context->Type == EtwxDialogMetadata) {

            width = LOWORD(lParam);
            height = HIWORD(lParam);

            if (context->hEdit) {

                if (width > 24 && height > 82) {

                    MoveWindow(context->hEdit,
                        12,
                        40,
                        width - 24,
                        height - 82,
                        TRUE);
                }
            }

            if (context->hCloseButton) {

                MoveWindow(context->hCloseButton,
                    width - 92,
                    height - 34,
                    80,
                    26,
                    TRUE);
            }
        }
        else if (context->Type == EtwxDialogSystemInformation) {

            width = LOWORD(lParam);
            height = HIWORD(lParam);

            if (context->hEdit) {

                if (width > 24 && height > 58) {

                    MoveWindow(context->hEdit,
                        12,
                        12,
                        width - 24,
                        height - 58,
                        TRUE);
                }
            }

            if (context->hCloseButton) {

                MoveWindow(context->hCloseButton,
                    width - 92,
                    height - 34,
                    80,
                    26,
                    TRUE);
            }
        }

        return 0;

    case WM_COMMAND:

        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (context == NULL)
            break;

        if (context->Type == EtwxDialogMetadata) {
            if (LOWORD(wParam) == IDCANCEL ||
                LOWORD(wParam) == IDOK)
            {
                DestroyWindow(hwnd);
                return 0;
            }
        }
        else {
            switch (LOWORD(wParam)) {

            case IDOK:

                switch (context->Type) {
                case EtwxDialogMetadata:
                case EtwxDialogSystemInformation:
                    break;
                default:
                    if (context->hEdit) {
                        GetWindowText(context->hEdit, context->pszText, (INT)context->cchText);
                    }
                    break;
                }
                if (context->DialogResult)
                    *context->DialogResult = TRUE;

                DestroyWindow(hwnd);
                return 0;

            case IDCANCEL:

                if (context->DialogResult)
                    *context->DialogResult = FALSE;

                DestroyWindow(hwnd);
                return 0;
            }
        }
        break;

    case WM_KEYDOWN:

        if (wParam == VK_ESCAPE) {
            context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (context && context->DialogResult)
                *context->DialogResult = FALSE;

            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CONTEXTMENU:

        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (context &&
            context->Type == EtwxDialogMetadata &&
            (HWND)wParam == context->hEdit)
        {
            hMenu = CreatePopupMenu();
            if (hMenu == NULL)
                return 0;

            SendMessage(context->hEdit, EM_EXGETSEL, 0, (LPARAM)&selection);
            AppendMenu(hMenu, MF_STRING | (selection.cpMin != selection.cpMax ? MF_ENABLED : MF_GRAYED), IDM_EDIT_COPY, TEXT("Copy"));
            AppendMenu(hMenu, MF_STRING, IDM_EDIT_SELECT_ALL, TEXT("Select All"));
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, IDM_EDIT_COPY_ALL, TEXT("Copy All"));

            point.x = GET_X_LPARAM(lParam);
            point.y = GET_Y_LPARAM(lParam);

            if (point.x == -1 && point.y == -1) {
                point.x = 0;
                point.y = 0;
                ClientToScreen(context->hEdit, &point);
            }

            command = TrackPopupMenu(hMenu,
                TPM_RETURNCMD | TPM_RIGHTBUTTON,
                point.x,
                point.y,
                0,
                hwnd,
                NULL);

            DestroyMenu(hMenu);

            switch (command) {
            case IDM_EDIT_COPY:
                SendMessage(context->hEdit, WM_COPY, 0, 0);
                break;
            case IDM_EDIT_SELECT_ALL:
                SendMessage(context->hEdit, EM_SETSEL, 0, -1);
                break;
            case IDM_EDIT_COPY_ALL:
                SendMessage(context->hEdit, EM_SETSEL, 0, -1);
                SendMessage(context->hEdit, WM_COPY, 0, 0);
                break;
            }
            return 0;
        }
        return 0;

    case WM_CLOSE:

        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (context && context->DialogResult)
            *context->DialogResult = FALSE;

        DestroyWindow(hwnd);
        return 0;

    case WM_NCDESTROY:
        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (context) {

            ownerWindow = GetWindow(hwnd, GW_OWNER);

            if (ownerWindow &&
                IsWindow(ownerWindow))
            {
                SetForegroundWindow(ownerWindow);
            }

            if (context->hFixedFont)
                DeleteObject(context->hFixedFont);

            switch (context->Type) {
            case EtwxDialogMetadata:
                if (context->pszText)
                    supHeapFree(context->pszText);
                if (context->pszProviderName)
                    supHeapFree(context->pszProviderName);
                break;
            default:
                break;
            }

            if (context->Allocated)
                supHeapFree(context);

            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        return 0;

    case WM_APP_LOAD_METADATA:

        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (context &&
            context->Type == EtwxDialogMetadata &&
            context->hEdit)
        {
            SendMessage(context->hEdit, WM_SETREDRAW, FALSE, 0);
            SetWindowText(context->hEdit, context->pszText);
            SendMessage(context->hEdit, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(context->hEdit, NULL, TRUE);
        }

        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*
* ShowEventIdFilterDialog
*
* Purpose:
*
* Displays the Event ID filter dialog and updates the configured
* Event ID filter text when the user accepts the dialog.
*
*/
BOOL ShowEventIdFilterDialog(
    _In_ HWND ParentWindow
)
{
    BOOL dialogResult = FALSE;
    HWND hDlg;
    ETWX_DIALOG_CONTEXT Context;

    RtlSecureZeroMemory(&Context, sizeof(Context));

    Context.pszText = g_ctx.eventIdFilterText;
    Context.cchText = RTL_NUMBER_OF(g_ctx.eventIdFilterText);
    Context.DialogResult = &dialogResult;
    Context.Type = EtwxDialogEventId;

    hDlg = supCreateModalDialog(ParentWindow,
        TEXT("Set Event ID Filter"),
        420,
        110,
        &Context);

    if (!hDlg)
        return FALSE;

    supCreateControl(0,
        WC_STATIC,
        TEXT("Event IDs to keep, comma-separated decimal (empty = capture all):"),
        WS_CHILD | WS_VISIBLE,
        12,
        12,
        380,
        18,
        hDlg,
        NULL);

    Context.hEdit = supCreateControl(WS_EX_CLIENTEDGE,
        WC_EDIT,
        g_ctx.eventIdFilterText,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        ES_LEFT |
        ES_AUTOHSCROLL,
        12,
        34,
        380,
        22,
        hDlg,
        (HMENU)ID_EVIDDLG_EDIT);

    supCreateControl(0,
        WC_BUTTON,
        TEXT("OK"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_DEFPUSHBUTTON,
        230,
        70,
        80,
        26,
        hDlg,
        (HMENU)(INT_PTR)IDOK);

    supCreateControl(0,
        WC_BUTTON,
        TEXT("Cancel"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON,
        316,
        70,
        80,
        26,
        hDlg,
        (HMENU)(INT_PTR)IDCANCEL);

    supRunModalDialog(ParentWindow, &hDlg, Context.hEdit);
    return dialogResult;
}

/*
* ShowKeywordDialog
*
* Purpose:
*
* Displays the keyword filter dialog and updates the configured
* keyword filter text when the user accepts the dialog.
*
*/
BOOL ShowKeywordDialog(
    _In_ HWND ParentWindow
)
{
    BOOL dialogResult = FALSE;
    HWND hDlg;
    ETWX_DIALOG_CONTEXT Context;

    RtlSecureZeroMemory(&Context, sizeof(Context));

    Context.pszText = g_ctx.keywordFilterText;
    Context.cchText = RTL_NUMBER_OF(g_ctx.keywordFilterText);
    Context.DialogResult = &dialogResult;
    Context.Type = EtwxDialogKeyword;

    hDlg = supCreateModalDialog(ParentWindow,
        TEXT("Set Keyword Filter"),
        320,
        110,
        &Context);

    if (!hDlg)
        return FALSE;

    supCreateControl(0,
        WC_STATIC,
        TEXT("MatchAnyKeyword, hex (0 = match any event):"),
        WS_CHILD | WS_VISIBLE,
        12,
        12,
        280,
        18,
        hDlg,
        NULL);

    Context.hEdit = supCreateControl(WS_EX_CLIENTEDGE,
        WC_EDIT,
        g_ctx.keywordFilterText,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        ES_LEFT |
        ES_AUTOHSCROLL,
        12,
        34,
        280,
        22,
        hDlg,
        (HMENU)ID_KWDLG_EDIT);

    supCreateControl(0,
        WC_BUTTON,
        TEXT("OK"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_DEFPUSHBUTTON,
        130,
        70,
        80,
        26,
        hDlg,
        (HMENU)(INT_PTR)IDOK);

    supCreateControl(0,
        WC_BUTTON,
        TEXT("Cancel"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON,
        216,
        70,
        80,
        26,
        hDlg,
        (HMENU)(INT_PTR)IDCANCEL);

    supRunModalDialog(ParentWindow, &hDlg, Context.hEdit);
    return dialogResult;
}

/*
* ShowLiveEventLimitDialog
*
* Purpose:
*
* Displays the retained live-event limit dialog and updates the limit
* when the user enters a valid decimal value.
*
*/
BOOL ShowLiveEventLimitDialog(
    _In_ HWND ParentWindow
)
{
    BOOL dialogResult;
    HWND hDlg;
    ULONGLONG value;
    ETWX_DIALOG_CONTEXT Context;
    WCHAR text[128];

    dialogResult = FALSE;
    value = 0;

    RtlSecureZeroMemory(&Context, sizeof(Context));

    Context.pszText = text;
    Context.cchText = ARRAYSIZE(text);
    Context.DialogResult = &dialogResult;

    hDlg = supCreateModalDialog(ParentWindow,
        TEXT("Set Live Event Limit"),
        420,
        110,
        &Context);

    if (!hDlg)
        return FALSE;

    StringCchPrintf(text, ARRAYSIZE(text), TEXT("Maximum retained events, decimal (1 - %lu):"), MAX_LIVE_EVENT_LIMIT);
    supCreateControl(0,
        WC_STATIC,
        text,
        WS_CHILD | WS_VISIBLE,
        12,
        12,
        380,
        18,
        hDlg,
        NULL);

    StringCchPrintf(text, ARRAYSIZE(text), TEXT("%lu"), g_ctx.liveEventLimit);

    Context.hEdit = supCreateControl(WS_EX_CLIENTEDGE,
        WC_EDIT,
        text,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        ES_LEFT |
        ES_AUTOHSCROLL,
        12,
        34,
        380,
        22,
        hDlg,
        NULL);

    supCreateControl(0,
        WC_BUTTON,
        TEXT("OK"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_DEFPUSHBUTTON,
        230,
        70,
        80,
        26,
        hDlg,
        (HMENU)(INT_PTR)IDOK);

    supCreateControl(0,
        WC_BUTTON,
        TEXT("Cancel"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON,
        316,
        70,
        80,
        26,
        hDlg,
        (HMENU)(INT_PTR)IDCANCEL);

    supRunModalDialog(ParentWindow, &hDlg, Context.hEdit);

    if (!dialogResult)
        return FALSE;

    if (!supStrToUInt64(text,
        &value) ||
        value == 0 ||
        value > MAX_LIVE_EVENT_LIMIT)
    {
        StringCchPrintf(text, ARRAYSIZE(text),
            TEXT("Enter a decimal live-event limit from 1 through %lu."), MAX_LIVE_EVENT_LIMIT);

        MessageBox(ParentWindow, text, TEXT("Set Live Event Limit"), MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    g_ctx.liveEventLimit = (ULONG)value;
    return TRUE;
}

/*
* ShowAboutDialog
*
* Purpose:
*
* Displays the ETW Explorer About dialog with the application icon,
* copyright information, version information, and an OK button.
*
* In debug builds, also displays process heap allocation statistics.
*
*/
BOOL ShowAboutDialog(
    _In_ HWND ParentWindow
)
{
    BOOL dialogResult = FALSE;
    SIZE size;
#ifdef _DEBUG
    SIZE statsSize;
#endif
    HWND hDlg, hIcon, hText;
#ifdef _DEBUG
    HWND hStats;
#endif
    ETWX_DIALOG_CONTEXT Context;
    WCHAR szText[256];
#ifdef _DEBUG
    WCHAR szStats[256];
    SUP_HEAP_STATS heapStats;
#endif
    INT buttonY, dialogWidth, dialogHeight;

    RtlSecureZeroMemory(&Context, sizeof(Context));

    Context.DialogResult = &dialogResult;
    Context.Type = EtwxDialogAbout;

    StringCchPrintf(szText,
        RTL_NUMBER_OF(szText),
        TEXT("Event Tracing for Windows Explorer\r\n%s\r\nVersion %lu.%lu.%lu.%lu"),
        PROGRAM_COPYRIGHT,
        PROGRAM_MAJOR_VERSION,
        PROGRAM_MINOR_VERSION,
        PROGRAM_REVISION_NUMBER,
        PROGRAM_BUILD_NUMBER);

    size = supMeasureText(szText, g_ctx.hFont);
    dialogWidth = 360;
    if (70 + size.cx + 12 > dialogWidth)
        dialogWidth = 70 + size.cx + 12;

#ifdef _DEBUG

    supHeapGetStats(&heapStats);

    StringCchPrintf(szStats,
        RTL_NUMBER_OF(szStats),
        TEXT("Heap statistics\r\n")
        TEXT("Allocations:\t%lld (%lld bytes)\r\n")
        TEXT("Reallocations:\t%lld (%lld bytes)\r\n")
        TEXT("Number of Frees:\t%lld\r\n")
        TEXT("Outstanding:\t%lld"),
        heapStats.AllocCount,
        heapStats.AllocBytes,
        heapStats.ReAllocCount,
        heapStats.ReAllocBytes,
        heapStats.FreeCount,
        heapStats.OutstandingCount);

    statsSize = supMeasureText(szStats, g_ctx.hFont);
    if (70 + statsSize.cx + 12 > dialogWidth)
        dialogWidth = 70 + statsSize.cx + 12;

    buttonY = 10 + size.cy + 8 + statsSize.cy + 10;

#else

    buttonY = 10 + size.cy + 12;

#endif

    dialogHeight = buttonY + 26 + 10;
    hDlg = supCreateModalDialog(ParentWindow,
        TEXT("About ETW Explorer"),
        dialogWidth,
        dialogHeight,
        &Context);

    if (!hDlg)
        return FALSE;

    hIcon = supCreateControl(0,
        WC_STATIC,
        NULL,
        WS_CHILD | WS_VISIBLE | SS_ICON,
        12,
        10,
        48,
        48,
        hDlg,
        NULL);

    SendMessage(hIcon, STM_SETICON, (WPARAM)g_ctx.hMainIcon, 0);

    hText = supCreateControl(0,
        WC_STATIC,
        szText,
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        70,
        10,
        dialogWidth - 82,
        size.cy,
        hDlg,
        NULL);

#ifdef _DEBUG

    hStats = supCreateControl(0,
        WC_STATIC,
        szStats,
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        70,
        10 + size.cy + 8,
        dialogWidth - 82,
        statsSize.cy,
        hDlg,
        NULL);

#endif

    supCreateControl(0,
        WC_BUTTON,
        TEXT("OK"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_DEFPUSHBUTTON,
        dialogWidth - 92,
        buttonY,
        80,
        26,
        hDlg,
        (HMENU)(INT_PTR)IDOK);

    supRunModalDialog(ParentWindow, &hDlg, GetDlgItem(hDlg, IDOK));
    return dialogResult;
}

/*
 * EventDetailsDlgProc
 *
 * Purpose:
 *
 * Handles commands and closing of the modeless event details dialog.
 *
 */
LRESULT CALLBACK EventDetailsDlgProc(
    _In_ HWND hDlg,
    _In_ UINT msg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (msg) {

    case WM_COMMAND:

        if (LOWORD(wParam) == IDOK ||
            LOWORD(wParam) == IDCANCEL)
        {
            DestroyWindow(hDlg);
        }

        return 0;

    case WM_CLOSE:

        DestroyWindow(hDlg);
        return 0;

    case WM_NCDESTROY:

        if (hDlg == g_hEventDetailsDlg)
            g_hEventDetailsDlg = NULL;

        if (g_eventDetailsDlgContext.hFixedFont) {
            DeleteObject(g_eventDetailsDlgContext.hFixedFont);
            g_eventDetailsDlgContext.hFixedFont = NULL;
        }
        break;
    }

    return DefWindowProc(hDlg, msg, wParam, lParam);
}

/*
 * ShowEventDetailsDialog
 *
 * Purpose:
 *
 * Displays the event details dialog for the specified live event and
 * updates the existing modeless dialog with the selected event data.
 *
 */
VOID ShowEventDetailsDialog(
    _In_ HWND ParentWindow,
    _In_ INT Row
)
{
    LIVE_EVENT_ROW rowCopy;
    BOOL valid;
    WCHAR timeBuf[32];
    WCHAR providerGuidBuf[64];
    WCHAR activityBuf[64];
    WCHAR relatedBuf[64];
    WCHAR header[700];
    WNDCLASSEX wc;

    valid = FALSE;
    RtlSecureZeroMemory(&rowCopy, sizeof(rowCopy));

    if (Row >= 0)
        valid = supCopyLiveEvent((ULONG)Row, &rowCopy);

    if (!valid)
        return;

    //
    // Create the dialog and controls only once.
    //
    if (!g_hEventDetailsDlg) {

        RtlSecureZeroMemory(&g_eventDetailsDlgContext, sizeof(g_eventDetailsDlgContext));
        g_eventDetailsDlgContext.Type = ExtwDialogEventDetails;
        g_eventDetailsDlgContext.hFixedFont = supCreateFixedFont(8);

        RtlSecureZeroMemory(&wc, sizeof(wc));

        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = EventDetailsDlgProc;
        wc.hInstance = g_ctx.hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = ETWXDLG_DETAILS_WNDCLASS;

        RegisterClassEx(&wc);

        g_hEventDetailsDlg = supCreateModelessDialog(ParentWindow,
            TEXT("Event Details"),
            560,
            420,
            ETWXDLG_DETAILS_WNDCLASS,
            &g_eventDetailsDlgContext);

        if (!g_hEventDetailsDlg)
            return;

        g_eventDetailsDlgContext.hHeaderEdit = supCreateControl(WS_EX_CLIENTEDGE,
            WC_EDIT,
            TEXT(""),
            WS_CHILD |
            WS_VISIBLE |
            ES_MULTILINE |
            ES_READONLY |
            ES_LEFT,
            12,
            12,
            536,
            110,
            g_hEventDetailsDlg,
            NULL);

        if (g_eventDetailsDlgContext.hHeaderEdit && g_eventDetailsDlgContext.hFixedFont) {
            SendMessage(g_eventDetailsDlgContext.hHeaderEdit,
                WM_SETFONT,
                (WPARAM)g_eventDetailsDlgContext.hFixedFont,
                TRUE);
        }

        supCreateControl(0,
            WC_STATIC,
            TEXT("Properties:"),
            WS_CHILD |
            WS_VISIBLE,
            12,
            130,
            200,
            18,
            g_hEventDetailsDlg,
            NULL);

        g_eventDetailsDlgContext.hPropertiesEdit = supCreateControl(
            WS_EX_CLIENTEDGE,
            WC_EDIT,
            TEXT(""),
            WS_CHILD |
            WS_VISIBLE |
            WS_VSCROLL |
            ES_MULTILINE |
            ES_READONLY |
            ES_AUTOVSCROLL |
            ES_LEFT,
            12,
            150,
            536,
            232,
            g_hEventDetailsDlg,
            NULL);

        if (g_eventDetailsDlgContext.hPropertiesEdit && g_eventDetailsDlgContext.hFixedFont) {
            SendMessage(
                g_eventDetailsDlgContext.hPropertiesEdit,
                WM_SETFONT,
                (WPARAM)g_eventDetailsDlgContext.hFixedFont,
                TRUE);
        }

        g_eventDetailsDlgContext.hCloseButton = supCreateControl(0,
            WC_BUTTON,
            TEXT("Close"),
            WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            BS_DEFPUSHBUTTON,
            468,
            390,
            80,
            26,
            g_hEventDetailsDlg,
            (HMENU)(INT_PTR)IDOK);

        if (g_eventDetailsDlgContext.hHeaderEdit)
            ShowWindow(g_eventDetailsDlgContext.hHeaderEdit, SW_SHOW);

        if (g_eventDetailsDlgContext.hPropertiesEdit)
            ShowWindow(g_eventDetailsDlgContext.hPropertiesEdit, SW_SHOW);

        if (g_eventDetailsDlgContext.hCloseButton)
            ShowWindow(g_eventDetailsDlgContext.hCloseButton, SW_SHOW);
    }

    //
    // Format event time.
    //
    StringCchPrintf(timeBuf,
        ARRAYSIZE(timeBuf),
        TEXT("%02u:%02u:%02u.%03u"),
        rowCopy.localTime.wHour,
        rowCopy.localTime.wMinute,
        rowCopy.localTime.wSecond,
        rowCopy.localTime.wMilliseconds);

    //
    // Format provider GUID.
    //
    if (IsEqualGUID(rowCopy.providerGuid,
        GUID_NULL) ||
        !StringFromGUID2(
            rowCopy.providerGuid,
            providerGuidBuf,
            ARRAYSIZE(providerGuidBuf)))
    {
        supStrCopy(
            providerGuidBuf,
            TEXT("(none)"));
    }

    //
    // Format ActivityId.
    //
    if (IsEqualGUID(rowCopy.activityId,
        GUID_NULL) ||
        !StringFromGUID2(rowCopy.activityId,
            activityBuf,
            ARRAYSIZE(activityBuf)))
    {
        supStrCopy(activityBuf, TEXT("(none)"));
    }

    //
    // Format RelatedActivityId.
    //
    if (!rowCopy.hasRelatedActivityId ||
        IsEqualGUID(rowCopy.relatedActivityId,
            GUID_NULL) ||
        !StringFromGUID2(rowCopy.relatedActivityId,
            relatedBuf,
            ARRAYSIZE(relatedBuf)))
    {
        supStrCopy(relatedBuf, TEXT("(none)"));
    }

    //
    // Avoid displaying the provider GUID twice when the provider
    // friendly name is unavailable and providerLabel already contains
    // the GUID.
    //
    if (supStrCmpI(rowCopy.providerLabel,
        providerGuidBuf) == 0)
    {
        StringCchPrintf(header,
            ARRAYSIZE(header),
            TEXT("Time:              %s\r\n")
            TEXT("Provider:          %s\r\n")
            TEXT("EventId:           %u\r\n")
            TEXT("Level:             %u\r\n")
            TEXT("Keyword:           0x%016llX\r\n")
            TEXT("ActivityId:        %s\r\n")
            TEXT("RelatedActivityId: %s"),
            timeBuf,
            rowCopy.providerLabel,
            rowCopy.eventId,
            rowCopy.level,
            rowCopy.keyword,
            activityBuf,
            relatedBuf);
    }
    else
    {
        StringCchPrintf(header,
            ARRAYSIZE(header),
            TEXT("Time:              %s\r\n")
            TEXT("Provider:          %s\r\n")
            TEXT("                   %s\r\n")
            TEXT("EventId:           %u\r\n")
            TEXT("Level:             %u\r\n")
            TEXT("Keyword:           0x%016llX\r\n")
            TEXT("ActivityId:        %s\r\n")
            TEXT("RelatedActivityId: %s"),
            timeBuf,
            rowCopy.providerLabel,
            providerGuidBuf,
            rowCopy.eventId,
            rowCopy.level,
            rowCopy.keyword,
            activityBuf,
            relatedBuf);
    }

    //
    // Update the existing controls.
    //
    SetWindowText(g_eventDetailsDlgContext.hHeaderEdit, header);
    SetWindowText(g_eventDetailsDlgContext.hPropertiesEdit, rowCopy.properties);

    //
    // Show the existing dialog with the new event.
    //
    ShowWindow(g_hEventDetailsDlg, SW_SHOW);

    SetForegroundWindow(g_hEventDetailsDlg);

    SetFocus(g_eventDetailsDlgContext.hCloseButton);
}

/*
 * EtwxEventFindProc
 *
 * Purpose:
 *
 * Dedicated find dialog proc.
 *
 */
VOID EtwxEventFindProc(
    _In_ HWND hwnd
)
{
    INT selection;
    ULONG providerIndex, value, foundIndex;
    ULONGLONG keyword;
    PETWX_DIALOG_CONTEXT context;
    WCHAR buffer[256];

    context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!context)
        return;

    RtlSecureZeroMemory(&g_ctx.liveEventSearch, sizeof(g_ctx.liveEventSearch));

    //
    // Provider.
    //
    selection = ComboBox_GetCurSel(context->hProviderCombo);
    if (selection > 0) {

        providerIndex = (ULONG)ComboBox_GetItemData(context->hProviderCombo, selection);
        if (providerIndex < g_ctx.providerCount) {
            g_ctx.liveEventSearch.MatchProvider = TRUE;
            g_ctx.liveEventSearch.ProviderGuid = g_ctx.providers[providerIndex].guid;
        }
    }

    //
    // Event ID.
    //
    Edit_GetText(context->hEventIdEdit, buffer, ARRAYSIZE(buffer));
    if (buffer[0] != UNICODE_NULL) {

        if (supParseUlong(buffer,
            &value) &&
            value <= USHRT_MAX)
        {
            g_ctx.liveEventSearch.MatchEventId = TRUE;
            g_ctx.liveEventSearch.EventId = (USHORT)value;
        }
        else {

            MessageBox(hwnd,
                TEXT("Invalid Event ID."),
                TEXT("Find Event"),
                MB_OK | MB_ICONWARNING);

            SetFocus(context->hEventIdEdit);
            return;
        }
    }

    //
    // Level.
    //
    selection = ComboBox_GetCurSel(context->hLevelCombo);
    if (selection > 0) {
        value = (ULONG)ComboBox_GetItemData(context->hLevelCombo, selection);
        g_ctx.liveEventSearch.MatchLevel = TRUE;
        g_ctx.liveEventSearch.Level = (UCHAR)value;
    }

    //
    // Keyword.
    //
    Edit_GetText(context->hEdit, buffer, ARRAYSIZE(buffer));
    if (buffer[0] != UNICODE_NULL) {

        if (!supParseUlonglong(buffer, &keyword)) {
            MessageBox(hwnd,
                TEXT("Invalid keyword."),
                TEXT("Find Event"),
                MB_OK | MB_ICONWARNING);

            SetFocus(context->hEdit);
            return;
        }

        g_ctx.liveEventSearch.MatchKeyword = TRUE;
        g_ctx.liveEventSearch.KeyWord = keyword;

        selection = ComboBox_GetCurSel(context->hKeywordModCombo);
        if (selection >= 0) {
            g_ctx.liveEventSearch.KeywordMatch =
                (ETWX_KEYWORD_MATCH)ComboBox_GetItemData(context->hKeywordModCombo, selection);
        }
        else {
            g_ctx.liveEventSearch.KeywordMatch = EtwxKeywordExact;
        }
    }

    //
    // Properties.
    //
    Edit_GetText(context->hPropertiesEdit, g_ctx.liveEventSearch.Properties, ARRAYSIZE(g_ctx.liveEventSearch.Properties));
    if (g_ctx.liveEventSearch.Properties[0] != UNICODE_NULL)
        g_ctx.liveEventSearch.MatchProperties = TRUE;

    //
    // Match case.
    //
    g_ctx.liveEventSearch.MatchCase = (Button_GetCheck(context->hMatchCase) == BST_CHECKED);
    g_ctx.liveEventSearch.MatchWholeWord = (Button_GetCheck(context->hWholeWord) == BST_CHECKED);

    //
    // Start a new search from the beginning.
    //
    if (supFindNextLiveEvent(&g_ctx.liveEventSearch, &foundIndex)) {

        ListView_SetItemState(g_ctx.hList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);

        ListView_SetItemState(g_ctx.hList,
            foundIndex,
            LVIS_SELECTED | LVIS_FOCUSED,
            LVIS_SELECTED | LVIS_FOCUSED);

        ListView_EnsureVisible(g_ctx.hList, foundIndex, FALSE);
    }
    else {
        MessageBox(hwnd, TEXT("No matching events found."), TEXT("Find Event"), MB_OK | MB_ICONINFORMATION);
    }
}

/*
* EtwxEventFindDialogProc
*
* Purpose:
*
* Handles the Find Event modeless dialog window.
*
*/
LRESULT CALLBACK EtwxEventFindDialogProc(
    _In_ HWND hwnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
)
{
    PETWX_DIALOG_CONTEXT context;

    switch (uMsg) {

    case WM_NCCREATE:

        context = (PETWX_DIALOG_CONTEXT)((LPCREATESTRUCT)lParam)->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)context);

        break;

    case WM_NCDESTROY:

        context = (PETWX_DIALOG_CONTEXT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        if (context)
            supHeapFree(context);

        break;

    case WM_KEYDOWN:

        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:

        g_ctx.hEventFindDlg = NULL;
        DestroyWindow(hwnd);

        return 0;

    case WM_COMMAND:

        switch (LOWORD(wParam)) {

        case ID_EVENTFIND_FINDNEXT:
            EtwxEventFindProc(hwnd);
            return 0;

        case ID_EVENTFIND_CLOSE:

            g_ctx.hEventFindDlg = NULL;
            DestroyWindow(hwnd);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*
* CompareProviderComboIndices
*
* Purpose:
*
* Comparator for Find Event properties part.
*
*/
INT CALLBACK CompareProviderComboIndices(
    _In_ const VOID* Element1,
    _In_ const VOID* Element2
)
{
    ULONG index1;
    ULONG index2;

    index1 = *(const ULONG*)Element1;
    index2 = *(const ULONG*)Element2;

    return supStrCmpI(g_ctx.providers[index1].name, g_ctx.providers[index2].name);
}

/*
* ShowEventFindDialog
*
* Purpose:
*
* Creates and displays the modeless Find Event dialog.
*
*/
VOID ShowEventFindDialog(
    _In_ HWND ParentWindow
)
{
    INT index;
    ULONG i;
    INT comboIndex;
    HWND hwnd;
    PULONG providerIndices;
    PETWX_DIALOG_CONTEXT context;

    static const struct {
        LPCWSTR Name;
        ULONG Value;
    } levelItems[] = {
        { TEXT("Any"),         0 },
        { TEXT("Critical"),    1 },
        { TEXT("Error"),       2 },
        { TEXT("Warning"),     3 },
        { TEXT("Information"), 4 },
        { TEXT("Verbose"),     5 }
    };

    static const struct {
        LPCWSTR Name;
        ETWX_KEYWORD_MATCH Value;
    } keywordMatchItems[] = {
        { TEXT("Exact"), EtwxKeywordExact },
        { TEXT("Contains any bits"), EtwxKeywordAnyBits },
        { TEXT("Contains all bits"), EtwxKeywordAllBits }
    };

    if (g_ctx.hEventFindDlg &&
        IsWindow(g_ctx.hEventFindDlg))
    {
        SetForegroundWindow(g_ctx.hEventFindDlg);
        SetFocus(g_ctx.hEventFindDlg);
        return;
    }

    context = (PETWX_DIALOG_CONTEXT)supHeapAlloc(sizeof(ETWX_DIALOG_CONTEXT));
    if (!context)
        return;

    context->Allocated = TRUE;
    context->Type = EtwxDialogEventFind;

    hwnd = supCreateModelessDialog(ParentWindow,
        TEXT("Find Event"),
        500,
        286,
        ETWXDLG_FIND_WNDCLASS,
        context);

    if (!hwnd) {
        supHeapFree(context);
        return;
    }

    g_ctx.hEventFindDlg = hwnd;

    context->hProviderCombo = supCreateControl(0,
        WC_COMBOBOX,
        NULL,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        CBS_DROPDOWNLIST |
        WS_VSCROLL,
        100,
        12,
        370,
        200,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_PROVIDER);

    context->hEventIdEdit = supCreateControl(WS_EX_CLIENTEDGE,
        WC_EDIT,
        NULL,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        ES_LEFT |
        ES_AUTOHSCROLL,
        100,
        45,
        370,
        22,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_EVENTID);

    context->hLevelCombo = supCreateControl(0,
        WC_COMBOBOX,
        NULL,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        CBS_DROPDOWNLIST |
        WS_VSCROLL,
        100,
        78,
        370,
        150,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_LEVEL);

    context->hEdit = supCreateControl(WS_EX_CLIENTEDGE,
        WC_EDIT,
        NULL,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        ES_LEFT |
        ES_AUTOHSCROLL,
        100,
        111,
        370,
        22,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_KEYWORD);

    context->hKeywordModCombo = supCreateControl(0,
        WC_COMBOBOX,
        NULL,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        CBS_DROPDOWNLIST,
        100,
        137,
        370,
        80,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_KEYWORDMOD);

    context->hPropertiesEdit = supCreateControl(WS_EX_CLIENTEDGE,
        WC_EDIT,
        NULL,
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        ES_LEFT |
        ES_AUTOHSCROLL,
        100,
        170,
        370,
        22,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_PROPERTIES);

    context->hMatchCase = supCreateControl(0,
        WC_BUTTON,
        TEXT("Match case"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_AUTOCHECKBOX,
        100,
        199,
        120,
        22,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_MATCHCASE);

    context->hWholeWord = supCreateControl(0,
        WC_BUTTON,
        TEXT("Watch whole word"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_AUTOCHECKBOX,
        100,
        222,
        140,
        22,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_WHOLEWORD);

    supCreateControl(0,
        WC_STATIC,
        TEXT("Provider:"),
        WS_CHILD | WS_VISIBLE,
        12,
        15,
        80,
        18,
        hwnd,
        NULL);

    supCreateControl(0,
        WC_STATIC,
        TEXT("Event ID:"),
        WS_CHILD | WS_VISIBLE,
        12,
        48,
        80,
        18,
        hwnd,
        NULL);

    supCreateControl(0,
        WC_STATIC,
        TEXT("Level:"),
        WS_CHILD | WS_VISIBLE,
        12,
        81,
        80,
        18,
        hwnd,
        NULL);

    supCreateControl(0,
        WC_STATIC,
        TEXT("Keyword:"),
        WS_CHILD | WS_VISIBLE,
        12,
        114,
        80,
        18,
        hwnd,
        NULL);

    supCreateControl(0,
        WC_STATIC,
        TEXT("Properties:"),
        WS_CHILD | WS_VISIBLE,
        12,
        173,
        80,
        18,
        hwnd,
        NULL);

    supCreateControl(0,
        WC_BUTTON,
        TEXT("Find Next"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_DEFPUSHBUTTON,
        300,
        250,
        80,
        26,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_FINDNEXT);

    context->hCloseButton = supCreateControl(0,
        WC_BUTTON,
        TEXT("Close"),
        WS_CHILD |
        WS_VISIBLE |
        WS_TABSTOP |
        BS_PUSHBUTTON,
        390,
        250,
        80,
        26,
        hwnd,
        (HMENU)(INT_PTR)ID_EVENTFIND_CLOSE);

    //
    // Provider list.
    //   
    ComboBox_AddString(context->hProviderCombo, TEXT("All providers"));
    ComboBox_SetItemData(context->hProviderCombo, 0, (LPARAM)-1);

    providerIndices = NULL;

    if (g_ctx.providerCount != 0) {

        providerIndices = (PULONG)supHeapAlloc(g_ctx.providerCount * sizeof(ULONG));
        if (providerIndices) {

            for (i = 0; i < g_ctx.providerCount; i++)
                providerIndices[i] = i;

            supSort(providerIndices, g_ctx.providerCount, sizeof(ULONG), CompareProviderComboIndices);

            for (i = 0; i < g_ctx.providerCount; i++) {

                comboIndex = ComboBox_AddString(context->hProviderCombo, g_ctx.providers[providerIndices[i]].name);
                if (comboIndex != CB_ERR &&
                    comboIndex != CB_ERRSPACE)
                {
                    ComboBox_SetItemData(context->hProviderCombo, comboIndex, providerIndices[i]);
                }
            }

            supHeapFree(providerIndices);
        }
    }

    ComboBox_SetCurSel(context->hProviderCombo, 0);

    //
    // Level list.
    //
    ComboBox_AddString(context->hProviderCombo, TEXT("All providers"));
    ComboBox_SetItemData(context->hProviderCombo, 0, (LPARAM)-1);

    for (i = 0; i < ARRAYSIZE(levelItems); i++) {

        index = ComboBox_AddString(context->hLevelCombo, levelItems[i].Name);

        if (index != CB_ERR &&
            index != CB_ERRSPACE)
        {
            ComboBox_SetItemData(context->hLevelCombo, index, levelItems[i].Value);
        }
    }

    ComboBox_SetCurSel(context->hLevelCombo, 0);

    //
    // Keyword matching mode.
    //
    for (i = 0; i < ARRAYSIZE(keywordMatchItems); i++) {

        index = ComboBox_AddString(context->hKeywordModCombo, keywordMatchItems[i].Name);

        if (index != CB_ERR &&
            index != CB_ERRSPACE)
        {
            ComboBox_SetItemData(context->hKeywordModCombo, index, keywordMatchItems[i].Value);
        }
    }

    ComboBox_SetCurSel(context->hKeywordModCombo, 0);

    //
    // Initialize the dialog from the current search context.
    //
    if (g_ctx.liveEventSearch.MatchCase) {
        Button_SetCheck(context->hMatchCase, BST_CHECKED);
    }

    if (g_ctx.liveEventSearch.MatchWholeWord) {
        Button_SetCheck(context->hWholeWord, BST_CHECKED);
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}

/*
* ShowMetadataDialog
*
* Purpose:
*
* Creates and displays a modeless dialog containing ETW provider
* metadata represented as reconstructed MOF or XML schema.
*
* The dialog owns private copies of the provider name and metadata text.
*
*/
HWND ShowMetadataDialog(
    _In_ HWND ParentWindow,
    _In_ LPCWSTR ProviderName,
    _In_ LPCWSTR MetaData
)
{
    PETWX_DIALOG_CONTEXT context;
    HWND hDlg;

    if (ProviderName == NULL || MetaData == NULL)
        return NULL;

    context = (PETWX_DIALOG_CONTEXT)supHeapAlloc(sizeof(ETWX_DIALOG_CONTEXT));
    if (!context)
        return NULL;

    context->Type = EtwxDialogMetadata;
    context->Allocated = TRUE;

    context->pszProviderName = supStrDup(ProviderName);
    if (!context->pszProviderName) {
        supHeapFree(context);
        return NULL;
    }

    context->pszText = supStrDup(MetaData);

    if (!context->pszText) {
        supHeapFree(context->pszProviderName);
        supHeapFree(context);
        return NULL;
    }

    context->cchText = supStrLen(MetaData);
    hDlg = supCreateMetadataDialog(ParentWindow,
        TEXT("ETW Provider Metadata"),
        850,
        600,
        context);

    if (!hDlg) {
        supHeapFree(context->pszText);
        supHeapFree(context->pszProviderName);
        supHeapFree(context);
        return NULL;
    }

    ShowWindow(hDlg, SW_SHOW);
    SetForegroundWindow(hDlg);

    PostMessage(hDlg, WM_APP_LOAD_METADATA, 0, 0);
    return hDlg;
}

/*
* InitEtwxCommonDialogs
*
* Purpose:
*
* Registers the shared ETW Explorer dialog window classes used by
* application dialogs.
*
*/
BOOL InitEtwxCommonDialogs()
{
    BOOL result;

    result = supRegisterDialogClass(ETWXDLG_WNDCLASS, EtwxCommonDialogProc);

    if (!result)
        return FALSE;

    result = supRegisterDialogClass(ETWXDLG_FIND_WNDCLASS, EtwxEventFindDialogProc);

    return result;
}

/*
* EtwxShowSystemInformationDialog
*
* Purpose:
*
* Displays general information about the currently active ETW tracing
* environment.
*
*/
VOID ShowSystemInformationDialog(
    _In_ HWND ParentWindow
)
{
    PETWX_DIALOG_CONTEXT context;
    HWND hwndDialog;

    context = (PETWX_DIALOG_CONTEXT)supHeapAlloc(sizeof(ETWX_DIALOG_CONTEXT));
    if (context == NULL)
        return;

    context->Allocated = TRUE;
    context->Type = EtwxDialogSystemInformation;
    hwndDialog = supCreateModalDialog(ParentWindow,
        TEXT("ETW System Information"),
        400,
        400,
        context);

    if (hwndDialog == NULL) {
        supHeapFree(context);
        return;
    }

    supRunModalDialog(ParentWindow, &hwndDialog, NULL);
}
