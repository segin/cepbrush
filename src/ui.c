#include "app.h"

enum {
    TEXT_HIT_NONE = 0,
    TEXT_HIT_FONT,
    TEXT_HIT_BOLD,
    TEXT_HIT_ITALIC,
    TEXT_HIT_UNDERLINE,
    TEXT_HIT_OPAQUE,
    TEXT_HIT_COMMIT,
    TEXT_HIT_CANCEL
};

static void App_DrawBitmap(HDC hdc, HBITMAP bitmap, int left, int top) {
    BITMAP info;
    HDC memory_dc;
    HGDIOBJ old_bitmap;

    if (!bitmap) {
        return;
    }

    GetObject(bitmap, sizeof(info), &info);
    memory_dc = CreateCompatibleDC(hdc);
    old_bitmap = SelectObject(memory_dc, bitmap);
    BitBlt(hdc, left, top, info.bmWidth, info.bmHeight, memory_dc, 0, 0, SRCCOPY);
    SelectObject(memory_dc, old_bitmap);
    DeleteDC(memory_dc);
}

static int App_RectIntersects(const RECT* left, const RECT* right) {
    RECT intersection;

    return IntersectRect(&intersection, left, right);
}

static int App_HitTestMenu(const APP_STATE* app, POINT point) {
    int menu_index;

    if (!PtInRect(&app->menu_bar_rect, point)) {
        return -1;
    }

    for (menu_index = 0; menu_index < MENU_ITEM_COUNT; ++menu_index) {
        if (PtInRect(&app->menu_item_rects[menu_index], point)) {
            return menu_index;
        }
    }

    return -1;
}

static int App_TextPointSize(const APP_STATE* app) {
    HDC hdc;
    int pixels_per_inch;
    int point_size;

    hdc = GetDC(app->window);
    pixels_per_inch = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if (hdc) {
        ReleaseDC(app->window, hdc);
    }

    point_size = (abs(app->text_logfont.lfHeight) * 72 + max(1, pixels_per_inch) / 2) / max(1, pixels_per_inch);
    return point_size > 0 ? point_size : 10;
}

static void App_DrawMenuStrip(HDC hdc, APP_STATE* app) {
    RECT text_rect;
    HBRUSH brush;
    int menu_index;

    brush = CreateSolidBrush(RGB(212, 208, 200));
    FillRect(hdc, &app->menu_bar_rect, brush);
    DeleteObject(brush);

    MoveToEx(hdc, app->menu_bar_rect.left, app->menu_bar_rect.bottom - 1, NULL);
    LineTo(hdc, app->menu_bar_rect.right, app->menu_bar_rect.bottom - 1);

    SetBkMode(hdc, TRANSPARENT);
    for (menu_index = 0; menu_index < MENU_ITEM_COUNT; ++menu_index) {
        if (menu_index == app->active_menu_index) {
            App_DrawSunkenPanel(hdc, &app->menu_item_rects[menu_index]);
        }

        text_rect = app->menu_item_rects[menu_index];
        DrawText(hdc, g_menu_labels[menu_index], -1, &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void App_DrawToolbox(HDC hdc, APP_STATE* app) {
    RECT header_rect;
    HBITMAP toolbox_bitmap;
    int index;

    App_DrawRaisedPanel(hdc, &app->toolbox_rect);

    header_rect = app->toolbox_rect;
    header_rect.left += 8;
    header_rect.top += 6;
    header_rect.bottom = header_rect.top + 16;
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, TEXT("Tools"), -1, &header_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    toolbox_bitmap = app->use_bw_toolbox ? app->toolbox_bw_bitmap : app->toolbox_color_bitmap;
    App_DrawBitmap(hdc, toolbox_bitmap, app->toolbox_art_rect.left, app->toolbox_art_rect.top);

    for (index = 0; index < TOOL_COUNT; ++index) {
        RECT rect;

        rect = App_GetToolRect(app, (TOOL_ID)index);
        if (index == (int)app->active_tool) {
            InflateRect(&rect, -1, -1);
            App_DrawBevel(hdc, &rect, RGB(128, 128, 128), RGB(255, 255, 255));
        }
    }
}

static void App_DrawPanelButton(HDC hdc, const RECT* rect, LPCTSTR label, int pressed, int enabled) {
    RECT text_rect;
    COLORREF old_color;

    if (pressed) {
        App_DrawSunkenPanel(hdc, rect);
    } else {
        App_DrawRaisedPanel(hdc, rect);
    }

    old_color = SetTextColor(hdc, enabled ? RGB(0, 0, 0) : RGB(128, 128, 128));
    SetBkMode(hdc, TRANSPARENT);
    text_rect = *rect;
    DrawText(hdc, label, -1, &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(hdc, old_color);
}

static void App_DrawTextPanel(HDC hdc, APP_STATE* app) {
    RECT info_rect;
    RECT sample_rect;
    HFONT old_font;
    HBRUSH sample_brush;
    COLORREF old_color;
    COLORREF old_background;
    int old_mode;
    TCHAR font_info[128];

    if (!App_TextUiVisible(app)) {
        return;
    }

    App_DrawRaisedPanel(hdc, &app->text_panel_rect);
    App_DrawPanelButton(hdc, &app->text_font_button_rect, TEXT("Font..."), 0, TRUE);
    App_DrawPanelButton(hdc, &app->text_bold_rect, TEXT("B"), app->text_logfont.lfWeight >= FW_BOLD, TRUE);
    App_DrawPanelButton(hdc, &app->text_italic_rect, TEXT("I"), app->text_logfont.lfItalic, TRUE);
    App_DrawPanelButton(hdc, &app->text_underline_rect, TEXT("U"), app->text_logfont.lfUnderline, TRUE);
    App_DrawPanelButton(hdc, &app->text_opaque_rect, TEXT("Opaque"), app->text_background_opaque, TRUE);
    App_DrawPanelButton(hdc, &app->text_commit_rect, TEXT("Commit"), 0, app->text_edit_active);
    App_DrawPanelButton(hdc, &app->text_cancel_rect, TEXT("Cancel"), 0, app->text_edit_active);

    App_DrawSunkenPanel(hdc, &app->text_sample_rect);
    sample_rect = app->text_sample_rect;
    InflateRect(&sample_rect, -4, -4);
    sample_brush = CreateSolidBrush(app->text_background_opaque ? app->background_color : RGB(255, 255, 255));
    FillRect(hdc, &sample_rect, sample_brush);
    DeleteObject(sample_brush);

    info_rect = sample_rect;
    info_rect.bottom = info_rect.top + 16;
    _sntprintf(font_info, ARRAYSIZE(font_info), TEXT("%s %dpt"), app->text_logfont.lfFaceName, App_TextPointSize(app));
    font_info[ARRAYSIZE(font_info) - 1] = 0;
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, font_info, -1, &info_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    sample_rect.top = info_rect.bottom + 2;
    old_font = SelectObject(hdc, app->text_font ? app->text_font : GetStockObject(SYSTEM_FONT));
    old_color = SetTextColor(hdc, app->foreground_color);
    old_background = SetBkColor(hdc, app->background_color);
    old_mode = SetBkMode(hdc, app->text_background_opaque ? OPAQUE : TRANSPARENT);
    DrawText(hdc, TEXT("AaBbYy 123"), -1, &sample_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetBkMode(hdc, old_mode);
    SetBkColor(hdc, old_background);
    SetTextColor(hdc, old_color);
    SelectObject(hdc, old_font);
}

static void App_DrawPalette(HDC hdc, APP_STATE* app) {
    HBRUSH brush;
    RECT label_rect;
    int index;

    App_DrawRaisedPanel(hdc, &app->palette_rect);

    label_rect = app->palette_rect;
    label_rect.left += 8;
    label_rect.top += 4;
    label_rect.bottom = label_rect.top + 16;
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, TEXT("Colors"), -1, &label_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    brush = CreateSolidBrush(app->background_color);
    FillRect(hdc, &app->background_rect, brush);
    DeleteObject(brush);
    App_DrawBevel(hdc, &app->background_rect, RGB(255, 255, 255), RGB(128, 128, 128));

    brush = CreateSolidBrush(app->foreground_color);
    FillRect(hdc, &app->foreground_rect, brush);
    DeleteObject(brush);
    App_DrawBevel(hdc, &app->foreground_rect, RGB(255, 255, 255), RGB(128, 128, 128));

    App_DrawBitmap(hdc, app->arrow_bitmap, app->palette_rect.left + 56, app->palette_rect.top + 34);

    for (index = 0; index < PALETTE_COLOR_COUNT; ++index) {
        FillRect(hdc, &app->palette_cells[index], app->palette_brushes[index]);
        App_DrawBevel(hdc, &app->palette_cells[index], RGB(255, 255, 255), RGB(96, 96, 96));
    }
}

static void App_DrawCanvas(HDC hdc, APP_STATE* app) {
    HBRUSH background_brush;
    HBRUSH white_brush;
    HPEN grid_pen;
    HPEN old_pen;
    int grid_index;
    int source_width;
    int source_height;
    int destination_width;
    int destination_height;

    background_brush = CreateSolidBrush(RGB(128, 128, 128));
    FillRect(hdc, &app->canvas_area_rect, background_brush);
    DeleteObject(background_brush);

    App_DrawSunkenPanel(hdc, &app->canvas_frame_rect);

    white_brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &app->canvas_rect, white_brush);
    DeleteObject(white_brush);

    source_width = min(app->canvas_width - app->scroll_x, App_GetCanvasPageWidth(app));
    source_height = min(app->canvas_height - app->scroll_y, App_GetCanvasPageHeight(app));
    if (source_width < 1) {
        source_width = 1;
    }
    if (source_height < 1) {
        source_height = 1;
    }
    destination_width = min(App_RectWidth(&app->canvas_rect), source_width * app->zoom);
    destination_height = min(App_RectHeight(&app->canvas_rect), source_height * app->zoom);

    StretchBlt(
        hdc,
        app->canvas_rect.left,
        app->canvas_rect.top,
        destination_width,
        destination_height,
        app->canvas_dc,
        app->scroll_x,
        app->scroll_y,
        source_width,
        source_height,
        SRCCOPY
    );

    if (app->show_pixel_grid && app->zoom >= 4) {
        grid_pen = CreatePen(PS_SOLID, 1, RGB(196, 196, 196));
        old_pen = SelectObject(hdc, grid_pen);

        for (grid_index = 1; grid_index < source_width; ++grid_index) {
            int grid_x;

            grid_x = app->canvas_rect.left + (grid_index * app->zoom);
            MoveToEx(hdc, grid_x, app->canvas_rect.top, NULL);
            LineTo(hdc, grid_x, app->canvas_rect.top + destination_height);
        }

        for (grid_index = 1; grid_index < source_height; ++grid_index) {
            int grid_y;

            grid_y = app->canvas_rect.top + (grid_index * app->zoom);
            MoveToEx(hdc, app->canvas_rect.left, grid_y, NULL);
            LineTo(hdc, app->canvas_rect.left + destination_width, grid_y);
        }

        SelectObject(hdc, old_pen);
        DeleteObject(grid_pen);
    }
}

static void App_DrawStatusBar(HDC hdc, APP_STATE* app) {
    RECT text_rect;
    TCHAR dimensions[96];
    HBRUSH brush;

    brush = CreateSolidBrush(RGB(212, 208, 200));
    FillRect(hdc, &app->status_rect, brush);
    DeleteObject(brush);

    MoveToEx(hdc, app->status_rect.left, app->status_rect.top, NULL);
    LineTo(hdc, app->status_rect.right, app->status_rect.top);

    text_rect = app->status_rect;
    text_rect.left += 6;
    text_rect.right -= 6;
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, app->status_text, -1, &text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    _sntprintf(dimensions, ARRAYSIZE(dimensions), TEXT("%dx%d   Zoom %dx"), app->canvas_width, app->canvas_height, app->zoom);
    dimensions[ARRAYSIZE(dimensions) - 1] = 0;
    DrawText(hdc, dimensions, -1, &text_rect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void App_DrawUi(HDC hdc, APP_STATE* app, const RECT* paint_rect) {
    HBRUSH background;

    background = CreateSolidBrush(RGB(192, 192, 192));
    FillRect(hdc, paint_rect, background);
    DeleteObject(background);

    if (!app->command_bar && App_RectIntersects(paint_rect, &app->menu_bar_rect)) {
        App_DrawMenuStrip(hdc, app);
    }
    if (App_RectIntersects(paint_rect, &app->toolbox_rect)) {
        App_DrawToolbox(hdc, app);
    }
    if (App_TextUiVisible(app) && App_RectIntersects(paint_rect, &app->text_panel_rect)) {
        App_DrawTextPanel(hdc, app);
    }
    if (App_RectIntersects(paint_rect, &app->canvas_area_rect)) {
        App_DrawCanvas(hdc, app);
    }
    if (App_RectIntersects(paint_rect, &app->palette_rect)) {
        App_DrawPalette(hdc, app);
    }
    if (App_RectIntersects(paint_rect, &app->status_rect)) {
        App_DrawStatusBar(hdc, app);
    }
}

static int App_HitTestTextPanel(const APP_STATE* app, POINT point) {
    if (!App_TextUiVisible(app) || !PtInRect(&app->text_panel_rect, point)) {
        return TEXT_HIT_NONE;
    }
    if (PtInRect(&app->text_font_button_rect, point)) {
        return TEXT_HIT_FONT;
    }
    if (PtInRect(&app->text_bold_rect, point)) {
        return TEXT_HIT_BOLD;
    }
    if (PtInRect(&app->text_italic_rect, point)) {
        return TEXT_HIT_ITALIC;
    }
    if (PtInRect(&app->text_underline_rect, point)) {
        return TEXT_HIT_UNDERLINE;
    }
    if (PtInRect(&app->text_opaque_rect, point)) {
        return TEXT_HIT_OPAQUE;
    }
    if (PtInRect(&app->text_commit_rect, point)) {
        return TEXT_HIT_COMMIT;
    }
    if (PtInRect(&app->text_cancel_rect, point)) {
        return TEXT_HIT_CANCEL;
    }
    return TEXT_HIT_NONE;
}

static void App_HandleTextPanelClick(APP_STATE* app, int hit) {
    switch (hit) {
    case TEXT_HIT_FONT:
        App_ChooseTextFont(app);
        App_SetStatus(app, TEXT("Text font updated"));
        break;
    case TEXT_HIT_BOLD:
        app->text_logfont.lfWeight = app->text_logfont.lfWeight >= FW_BOLD ? FW_NORMAL : FW_BOLD;
        App_ApplyTextFont(app);
        InvalidateRect(app->window, &app->text_panel_rect, FALSE);
        break;
    case TEXT_HIT_ITALIC:
        app->text_logfont.lfItalic = !app->text_logfont.lfItalic;
        App_ApplyTextFont(app);
        InvalidateRect(app->window, &app->text_panel_rect, FALSE);
        break;
    case TEXT_HIT_UNDERLINE:
        app->text_logfont.lfUnderline = !app->text_logfont.lfUnderline;
        App_ApplyTextFont(app);
        InvalidateRect(app->window, &app->text_panel_rect, FALSE);
        break;
    case TEXT_HIT_OPAQUE:
        app->text_background_opaque = !app->text_background_opaque;
        App_UpdateTextPreviewBrush(app);
        InvalidateRect(app->window, &app->text_panel_rect, FALSE);
        break;
    case TEXT_HIT_COMMIT:
        if (app->text_edit_active) {
            App_CommitTextEdit(app);
        }
        break;
    case TEXT_HIT_CANCEL:
        if (app->text_edit_active) {
            App_CancelTextEdit(app);
        }
        break;
    default:
        break;
    }
}

static void App_ShowMenuPopup(APP_STATE* app, int menu_index) {
    HMENU submenu;
    POINT popup_point;
    int command_id;

    submenu = GetSubMenu(app->menu, menu_index);
    if (!submenu) {
        return;
    }

    popup_point.x = app->menu_item_rects[menu_index].left;
    popup_point.y = app->menu_item_rects[menu_index].bottom;
    ClientToScreen(app->window, &popup_point);

    App_UpdateMenuState(app);
    app->active_menu_index = menu_index;
    InvalidateRect(app->window, &app->menu_bar_rect, FALSE);

    command_id = TrackPopupMenu(
        submenu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON,
        popup_point.x,
        popup_point.y,
        0,
        app->window,
        NULL
    );

    app->active_menu_index = -1;
    InvalidateRect(app->window, &app->menu_bar_rect, FALSE);

    if (command_id != 0) {
        App_HandleCommand(app, command_id);
    }
}

void App_HandlePointerDown(APP_STATE* app, POINT point, int button) {
    int menu_index;
    TOOL_ID tool;
    int palette_index;
    POINT canvas_point;
    int text_hit;

    text_hit = App_HitTestTextPanel(app, point);
    if (text_hit != TEXT_HIT_NONE) {
        App_HandleTextPanelClick(app, text_hit);
        return;
    }

    if (!app->command_bar) {
        menu_index = App_HitTestMenu(app, point);
        if (menu_index >= 0) {
            App_ShowMenuPopup(app, menu_index);
            return;
        }
    }

    tool = App_HitTestToolbox(app, point);
    if (tool != TOOL_COUNT) {
        if (app->text_edit_active && tool != TOOL_TEXT) {
            if (!App_CommitTextEdit(app)) {
                return;
            }
        }
        App_SetActiveTool(app, tool);
        return;
    }

    palette_index = App_HitTestPalette(app, point);
    if (palette_index >= 0) {
        if (button == MK_RBUTTON) {
            app->background_color = g_palette_colors[palette_index];
            App_SetStatus(app, TEXT("Background color selected"));
        } else {
            app->foreground_color = g_palette_colors[palette_index];
            App_SetStatus(app, TEXT("Foreground color selected"));
        }
        App_UpdateTextPreviewBrush(app);
        InvalidateRect(app->window, &app->palette_rect, FALSE);
        if (App_TextUiVisible(app)) {
            InvalidateRect(app->window, &app->text_panel_rect, FALSE);
        }
        return;
    }

    if (PtInRect(&app->foreground_rect, point) || PtInRect(&app->background_rect, point)) {
        COLORREF color;

        color = app->foreground_color;
        app->foreground_color = app->background_color;
        app->background_color = color;
        App_UpdateTextPreviewBrush(app);
        App_SetStatus(app, TEXT("Foreground and background colors swapped"));
        InvalidateRect(app->window, &app->palette_rect, FALSE);
        if (App_TextUiVisible(app)) {
            InvalidateRect(app->window, &app->text_panel_rect, FALSE);
        }
        return;
    }

    if (!App_WindowToCanvas(app, point, &canvas_point)) {
        return;
    }

    if (app->text_edit_active && app->active_tool == TOOL_TEXT) {
        if (!App_CommitTextEdit(app)) {
            return;
        }
    }

    if (!App_ToolUsesDrag(app->active_tool)) {
        App_DoImmediateTool(app, canvas_point, button);
        return;
    }

    App_BeginDrag(app, canvas_point, button);
}

void App_HandlePointerMove(APP_STATE* app, POINT point) {
    POINT canvas_point;
    TCHAR status[128];

    if (app->drawing && App_WindowToCanvas(app, point, &canvas_point)) {
        App_ContinueDrag(app, canvas_point);
        _sntprintf(status, ARRAYSIZE(status), TEXT("%s  (%d, %d)"), g_tools[app->active_tool].name, canvas_point.x, canvas_point.y);
        status[ARRAYSIZE(status) - 1] = 0;
        App_SetStatus(app, status);
        return;
    }

    if (App_WindowToCanvas(app, point, &canvas_point)) {
        _sntprintf(status, ARRAYSIZE(status), TEXT("%s  (%d, %d)"), g_tools[app->active_tool].name, canvas_point.x, canvas_point.y);
        status[ARRAYSIZE(status) - 1] = 0;
        App_SetStatus(app, status);
    }
}

void App_HandlePointerUp(APP_STATE* app, POINT point) {
    POINT canvas_point;

    if (!app->drawing) {
        return;
    }

    if (!App_WindowToCanvas(app, point, &canvas_point)) {
        canvas_point = app->drag_current;
    }

    App_EndDrag(app, canvas_point);
}
