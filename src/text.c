#include <stdlib.h>
#include <string.h>

#include "app.h"

static int App_FontHeightFromPoints(APP_STATE* app, int points) {
    HDC hdc;
    int pixels_per_inch;
    int height;

    hdc = GetDC(app->window);
    pixels_per_inch = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if (hdc) {
        ReleaseDC(app->window, hdc);
    }

    height = -((points * pixels_per_inch + 36) / 72);
    if (height == 0) {
        height = -12;
    }
    return height;
}

static void App_DestroyTextEditWindow(APP_STATE* app) {
    if (app->text_edit_window) {
        DestroyWindow(app->text_edit_window);
        app->text_edit_window = NULL;
    }
}

static RECT App_TextCanvasToWindowRect(const APP_STATE* app) {
    RECT rect;

    rect.left = app->canvas_rect.left + ((app->text_canvas_rect.left - app->scroll_x) * app->zoom);
    rect.top = app->canvas_rect.top + ((app->text_canvas_rect.top - app->scroll_y) * app->zoom);
    rect.right = app->canvas_rect.left + ((app->text_canvas_rect.right - app->scroll_x) * app->zoom);
    rect.bottom = app->canvas_rect.top + ((app->text_canvas_rect.bottom - app->scroll_y) * app->zoom);

    return rect;
}

int App_TextUiVisible(const APP_STATE* app) {
    return app->active_tool == TOOL_TEXT || app->text_edit_active;
}

void App_UpdateTextPreviewBrush(APP_STATE* app) {
    if (app->text_preview_brush) {
        DeleteObject(app->text_preview_brush);
    }

    app->text_preview_brush = CreateSolidBrush(
        app->text_background_opaque ? app->background_color : RGB(255, 255, 255)
    );

    if (app->text_edit_window) {
        InvalidateRect(app->text_edit_window, NULL, TRUE);
    }
}

void App_ApplyTextFont(APP_STATE* app) {
    HFONT new_font;

    new_font = CreateFontIndirect(&app->text_logfont);
    if (!new_font) {
        return;
    }

    if (app->text_font) {
        DeleteObject(app->text_font);
    }
    app->text_font = new_font;

    if (app->text_edit_window) {
        SendMessage(app->text_edit_window, WM_SETFONT, (WPARAM)app->text_font, TRUE);
    }
}

void App_InitTextState(APP_STATE* app) {
    HFONT system_font;

    memset(&app->text_logfont, 0, sizeof(app->text_logfont));
    system_font = (HFONT)GetStockObject(SYSTEM_FONT);
    if (system_font) {
        GetObject(system_font, sizeof(app->text_logfont), &app->text_logfont);
    }

    app->text_logfont.lfHeight = App_FontHeightFromPoints(app, 10);
    app->text_logfont.lfWeight = FW_NORMAL;
    app->text_logfont.lfItalic = FALSE;
    app->text_logfont.lfUnderline = FALSE;
    if (!app->text_logfont.lfFaceName[0]) {
        lstrcpyn(app->text_logfont.lfFaceName, TEXT("Tahoma"), LF_FACESIZE);
    }

    app->text_background_opaque = 0;
    SetRectEmpty(&app->text_canvas_rect);
    App_ApplyTextFont(app);
    App_UpdateTextPreviewBrush(app);
}

void App_DestroyTextState(APP_STATE* app) {
    App_DestroyTextEditWindow(app);

    if (app->text_font) {
        DeleteObject(app->text_font);
        app->text_font = NULL;
    }
    if (app->text_preview_brush) {
        DeleteObject(app->text_preview_brush);
        app->text_preview_brush = NULL;
    }

    app->text_edit_active = 0;
    SetRectEmpty(&app->text_canvas_rect);
}

void App_PositionTextEdit(APP_STATE* app) {
    RECT window_rect;
    RECT visible_rect;

    if (!app->text_edit_window) {
        return;
    }

    window_rect = App_TextCanvasToWindowRect(app);
    if (!IntersectRect(&visible_rect, &window_rect, &app->canvas_rect)
        || App_RectWidth(&visible_rect) < 8
        || App_RectHeight(&visible_rect) < 8) {
        ShowWindow(app->text_edit_window, SW_HIDE);
        return;
    }

    MoveWindow(
        app->text_edit_window,
        visible_rect.left,
        visible_rect.top,
        App_RectWidth(&visible_rect),
        App_RectHeight(&visible_rect),
        TRUE
    );
    ShowWindow(app->text_edit_window, SW_SHOW);
}

static void App_NormalizeTextRect(APP_STATE* app, POINT canvas_point) {
    int left;
    int top;
    int width;
    int height;

    width = min(DEFAULT_TEXT_BOX_WIDTH, app->canvas_width);
    height = min(DEFAULT_TEXT_BOX_HEIGHT, app->canvas_height);
    left = canvas_point.x;
    top = canvas_point.y;

    if (left + width > app->canvas_width) {
        left = max(0, app->canvas_width - width);
    }
    if (top + height > app->canvas_height) {
        top = max(0, app->canvas_height - height);
    }

    SetRect(&app->text_canvas_rect, left, top, left + width, top + height);
}

int App_BeginTextEdit(APP_STATE* app, POINT canvas_point) {
    if (app->text_edit_active) {
        if (!App_CommitTextEdit(app)) {
            return 0;
        }
    }

    App_NormalizeTextRect(app, canvas_point);
    app->text_edit_window = CreateWindowEx(
        0,
        TEXT("EDIT"),
        TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
        0,
        0,
        0,
        0,
        app->window,
        (HMENU)ID_TEXT_EDIT_CONTROL,
        app->instance,
        NULL
    );
    if (!app->text_edit_window) {
        MessageBox(app->window, TEXT("Unable to create the text editor control."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    app->text_edit_active = 1;
    App_ApplyTextFont(app);
    App_UpdateTextPreviewBrush(app);
    App_Layout(app);
    App_PositionTextEdit(app);
    SetFocus(app->text_edit_window);
    App_SetStatus(app, TEXT("Type text, then Commit or choose another tool"));
    InvalidateRect(app->window, &app->text_panel_rect, FALSE);
    return 1;
}

static int App_TextEditLength(APP_STATE* app) {
    if (!app->text_edit_window) {
        return 0;
    }
    return GetWindowTextLength(app->text_edit_window);
}

void App_CancelTextEdit(APP_STATE* app) {
    int was_active;

    was_active = app->text_edit_active;
    App_DestroyTextEditWindow(app);
    app->text_edit_active = 0;
    SetRectEmpty(&app->text_canvas_rect);
    App_Layout(app);

    if (was_active) {
        App_SetStatus(app, TEXT("Text cancelled"));
        InvalidateRect(app->window, NULL, FALSE);
    }
}

int App_CommitTextEdit(APP_STATE* app) {
    TCHAR* text;
    int length;
    RECT dirty_rect;

    if (!app->text_edit_active || !app->text_edit_window) {
        return 1;
    }

    length = App_TextEditLength(app);
    if (length <= 0) {
        App_CancelTextEdit(app);
        return 1;
    }

    text = (TCHAR*)malloc((size_t)(length + 1) * sizeof(TCHAR));
    if (!text) {
        MessageBox(app->window, TEXT("Not enough memory to commit the text."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    GetWindowText(app->text_edit_window, text, length + 1);
    App_CopyCanvasToUndo(app);
    dirty_rect = app->text_canvas_rect;
    App_DrawTextBlock(app, text, &dirty_rect);
    free(text);

    App_DestroyTextEditWindow(app);
    app->text_edit_active = 0;
    SetRectEmpty(&app->text_canvas_rect);
    App_Layout(app);
    App_MarkDirty(app);
    App_SetStatus(app, TEXT("Text committed"));
    App_InvalidateCanvasRect(app, &dirty_rect);
    InvalidateRect(app->window, &app->text_panel_rect, FALSE);
    return 1;
}

int App_ChooseTextFont(APP_STATE* app) {
    CHOOSEFONT choose_font;
    LOGFONT logfont;

    logfont = app->text_logfont;
    memset(&choose_font, 0, sizeof(choose_font));
    choose_font.lStructSize = sizeof(choose_font);
    choose_font.hwndOwner = app->window;
    choose_font.lpLogFont = &logfont;
    choose_font.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;

    if (!ChooseFont(&choose_font)) {
        return 0;
    }

    app->text_logfont = logfont;
    App_ApplyTextFont(app);
    InvalidateRect(app->window, &app->text_panel_rect, FALSE);
    return 1;
}

LRESULT App_HandleTextCtlColor(APP_STATE* app, HDC hdc, HWND child_window) {
    if (child_window != app->text_edit_window || !app->text_preview_brush) {
        return 0;
    }

    SetTextColor(hdc, app->foreground_color);
    SetBkColor(hdc, app->text_background_opaque ? app->background_color : RGB(255, 255, 255));
    return (LRESULT)app->text_preview_brush;
}
