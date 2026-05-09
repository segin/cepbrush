#include <stdlib.h>
#include <string.h>

#include "app.h"

const TOOL_DEF g_tools[TOOL_COUNT] = {
    { TEXT("Free-form Select"), IDC_DUMMY },
    { TEXT("Select"), IDC_XDUMMY },
    { TEXT("Airbrush"), IDC_CROSSH },
    { TEXT("Text"), IDC_PB_IBEAM },
    { TEXT("Color Eraser"), IDC_BOXX3 },
    { TEXT("Eraser"), IDC_BOX3 },
    { TEXT("Fill With Color"), IDC_FLOOD },
    { TEXT("Brush"), IDC_CROSSH },
    { TEXT("Curve"), IDC_CROSSH },
    { TEXT("Line"), IDC_CROSSH },
    { TEXT("Rectangle"), IDC_RECT1 },
    { TEXT("Filled Rectangle"), IDC_RECT1 },
    { TEXT("Rounded Rectangle"), IDC_BOX1 },
    { TEXT("Filled Rounded Rectangle"), IDC_BOX1 },
    { TEXT("Ellipse"), IDC_OVAL1 },
    { TEXT("Filled Ellipse"), IDC_OVAL1 },
    { TEXT("Polygon"), IDC_CROSSH },
    { TEXT("Filled Polygon"), IDC_CROSSH }
};

const COLORREF g_palette_colors[PALETTE_COLOR_COUNT] = {
    RGB(0, 0, 0),
    RGB(128, 128, 128),
    RGB(128, 0, 0),
    RGB(128, 128, 0),
    RGB(0, 128, 0),
    RGB(0, 128, 128),
    RGB(0, 0, 128),
    RGB(128, 0, 128),
    RGB(255, 255, 255),
    RGB(192, 192, 192),
    RGB(255, 0, 0),
    RGB(255, 255, 0),
    RGB(0, 255, 0),
    RGB(0, 255, 255),
    RGB(0, 0, 255),
    RGB(255, 0, 255)
};

const TCHAR* g_menu_labels[MENU_ITEM_COUNT] = {
    TEXT("File"),
    TEXT("Edit"),
    TEXT("View"),
    TEXT("Help")
};

APP_STATE g_app;

int App_RectWidth(const RECT* rect) {
    return rect->right - rect->left;
}

int App_RectHeight(const RECT* rect) {
    return rect->bottom - rect->top;
}

void App_CopyString(TCHAR* destination, size_t destination_count, LPCTSTR source) {
    if (!destination || destination_count == 0) {
        return;
    }

    if (!source) {
        destination[0] = 0;
        return;
    }

    lstrcpyn(destination, source, (int)destination_count);
    destination[destination_count - 1] = 0;
}

static LPCTSTR App_BaseName(LPCTSTR path) {
    LPCTSTR cursor;
    LPCTSTR base_name;

    if (!path || !path[0]) {
        return TEXT("Untitled");
    }

    base_name = path;
    for (cursor = path; *cursor != 0; ++cursor) {
        if (*cursor == TEXT('\\') || *cursor == TEXT('/')) {
            base_name = cursor + 1;
        }
    }

    return base_name;
}

void App_UpdateTitle(APP_STATE* app) {
    TCHAR title[512];
    LPCTSTR base_name;

    base_name = App_BaseName(app->document_path);
    _sntprintf(title, ARRAYSIZE(title), app->dirty ? TEXT("%s - *%s") : TEXT("%s - %s"), APP_TITLE, base_name);
    title[ARRAYSIZE(title) - 1] = 0;
    SetWindowText(app->window, title);
}

void App_SetStatus(APP_STATE* app, LPCTSTR text) {
    if (lstrcmp(app->status_text, text ? text : TEXT("")) == 0) {
        return;
    }
    App_CopyString(app->status_text, ARRAYSIZE(app->status_text), text);
    InvalidateRect(app->window, &app->status_rect, FALSE);
}

void App_DrawBevel(HDC hdc, const RECT* rect, COLORREF light, COLORREF dark) {
    HPEN light_pen;
    HPEN dark_pen;
    HPEN old_pen;

    light_pen = CreatePen(PS_SOLID, 1, light);
    dark_pen = CreatePen(PS_SOLID, 1, dark);
    old_pen = SelectObject(hdc, light_pen);

    MoveToEx(hdc, rect->left, rect->bottom - 1, NULL);
    LineTo(hdc, rect->left, rect->top);
    LineTo(hdc, rect->right - 1, rect->top);

    SelectObject(hdc, dark_pen);
    MoveToEx(hdc, rect->right - 1, rect->top, NULL);
    LineTo(hdc, rect->right - 1, rect->bottom - 1);
    LineTo(hdc, rect->left, rect->bottom - 1);

    SelectObject(hdc, old_pen);
    DeleteObject(light_pen);
    DeleteObject(dark_pen);
}

void App_DrawSunkenPanel(HDC hdc, const RECT* rect) {
    HBRUSH brush;
    RECT inner_rect;

    brush = CreateSolidBrush(RGB(212, 208, 200));
    FillRect(hdc, rect, brush);
    DeleteObject(brush);
    App_DrawBevel(hdc, rect, RGB(128, 128, 128), RGB(255, 255, 255));
    inner_rect = *rect;
    InflateRect(&inner_rect, -1, -1);
    App_DrawBevel(hdc, &inner_rect, RGB(128, 128, 128), RGB(255, 255, 255));
}

void App_DrawRaisedPanel(HDC hdc, const RECT* rect) {
    HBRUSH brush;

    brush = CreateSolidBrush(RGB(212, 208, 200));
    FillRect(hdc, rect, brush);
    DeleteObject(brush);
    App_DrawBevel(hdc, rect, RGB(255, 255, 255), RGB(128, 128, 128));
}

int App_CreateShellMenuBar(APP_STATE* app) {
    if (app->command_bar) {
        return 1;
    }

    app->command_bar = CommandBar_Create(app->instance, app->window, 1);
    if (!app->command_bar) {
        app->command_bar_height = 0;
        return 0;
    }

    if (!CommandBar_InsertMenubar(app->command_bar, app->instance, IDR_MAIN_MENU, 0)) {
        CommandBar_Destroy(app->command_bar);
        app->command_bar = NULL;
        app->command_bar_height = 0;
        return 0;
    }
    if (!CommandBar_DrawMenuBar(app->command_bar, 0)) {
        CommandBar_Destroy(app->command_bar);
        app->command_bar = NULL;
        app->command_bar_height = 0;
        return 0;
    }
    app->menu = CommandBar_GetMenu(app->command_bar, 0);
    app->command_bar_height = CommandBar_Height(app->command_bar);
    if (app->command_bar_height < 1) {
        app->command_bar_height = 26;
    }
    return 1;
}

void App_DestroyShellMenuBar(APP_STATE* app) {
    if (app->command_bar) {
        CommandBar_Destroy(app->command_bar);
    }
    app->command_bar = NULL;
    app->command_bar_height = 0;
    app->menu = NULL;
}

void App_Layout(APP_STATE* app) {
    RECT client;
    int content_bottom;
    int content_left;
    int content_top;
    int canvas_top;
    int index;
    int button_left;
    int button_top;
    int button_bottom;

    GetClientRect(app->window, &client);

    if (app->command_bar) {
        app->command_bar_height = CommandBar_Height(app->command_bar);
        MoveWindow(app->command_bar, 0, 0, App_RectWidth(&client), app->command_bar_height, TRUE);
        SetRectEmpty(&app->menu_bar_rect);
        for (index = 0; index < MENU_ITEM_COUNT; ++index) {
            SetRectEmpty(&app->menu_item_rects[index]);
        }
        content_top = client.top + app->command_bar_height + LAYOUT_MARGIN;
        content_bottom = client.bottom;
    } else {
        SetRect(
            &app->menu_bar_rect,
            client.left,
            client.top,
            client.right,
            client.top + LAYOUT_MENU_HEIGHT
        );
        button_left = client.left + LAYOUT_MARGIN;
        for (index = 0; index < MENU_ITEM_COUNT; ++index) {
            SetRect(
                &app->menu_item_rects[index],
                button_left,
                app->menu_bar_rect.top + LAYOUT_MENU_ITEM_TOP,
                button_left + LAYOUT_MENU_ITEM_WIDTH,
                app->menu_bar_rect.bottom - LAYOUT_MENU_ITEM_BOTTOM
            );
            button_left += LAYOUT_MENU_ITEM_STEP;
        }
        content_top = app->menu_bar_rect.bottom + LAYOUT_MARGIN;
        content_bottom = client.bottom;
    }

    if (content_bottom < content_top) {
        content_bottom = content_top;
    }

    SetRect(
        &app->status_rect,
        client.left,
        max(client.top, content_bottom - LAYOUT_STATUS_HEIGHT),
        client.right,
        content_bottom
    );

    SetRect(
        &app->toolbox_rect,
        client.left + LAYOUT_MARGIN,
        content_top,
        client.left + LAYOUT_MARGIN + LAYOUT_TOOLBOX_WIDTH,
        app->status_rect.top - LAYOUT_MARGIN
    );

    SetRect(
        &app->toolbox_art_rect,
        app->toolbox_rect.left + (App_RectWidth(&app->toolbox_rect) - TOOLBOX_ART_WIDTH) / 2,
        app->toolbox_rect.top + LAYOUT_TOOLBOX_ART_TOP,
        app->toolbox_rect.left + (App_RectWidth(&app->toolbox_rect) - TOOLBOX_ART_WIDTH) / 2 + TOOLBOX_ART_WIDTH,
        app->toolbox_rect.top + LAYOUT_TOOLBOX_ART_TOP + TOOLBOX_ART_HEIGHT
    );

    SetRect(
        &app->text_panel_rect,
        0,
        0,
        0,
        0
    );

    content_left = app->toolbox_rect.right + LAYOUT_MARGIN;
    canvas_top = content_top;
    if (App_TextUiVisible(app)) {
        SetRect(
            &app->text_panel_rect,
            content_left,
            content_top,
            client.right - LAYOUT_MARGIN,
            content_top + LAYOUT_TEXT_PANEL_HEIGHT
        );
        canvas_top = app->text_panel_rect.bottom + LAYOUT_MARGIN;
    }

    SetRect(
        &app->palette_rect,
        content_left,
        max(canvas_top + LAYOUT_MARGIN, app->status_rect.top - LAYOUT_PALETTE_HEIGHT - LAYOUT_MARGIN),
        client.right - LAYOUT_MARGIN,
        app->status_rect.top - LAYOUT_MARGIN
    );

    SetRect(
        &app->canvas_area_rect,
        content_left,
        canvas_top,
        client.right - LAYOUT_MARGIN,
        app->palette_rect.top - LAYOUT_MARGIN
    );

    app->canvas_frame_rect = app->canvas_area_rect;
    InflateRect(&app->canvas_frame_rect, -LAYOUT_MARGIN, -LAYOUT_MARGIN);
    if (App_RectWidth(&app->canvas_frame_rect) < LAYOUT_MIN_CANVAS_FRAME) {
        app->canvas_frame_rect.right = app->canvas_frame_rect.left + LAYOUT_MIN_CANVAS_FRAME;
    }
    if (App_RectHeight(&app->canvas_frame_rect) < LAYOUT_MIN_CANVAS_FRAME) {
        app->canvas_frame_rect.bottom = app->canvas_frame_rect.top + LAYOUT_MIN_CANVAS_FRAME;
    }

    app->canvas_rect = app->canvas_frame_rect;
    InflateRect(&app->canvas_rect, -LAYOUT_CANVAS_FRAME_INSET, -LAYOUT_CANVAS_FRAME_INSET);
    if (App_RectWidth(&app->canvas_rect) < 1) {
        app->canvas_rect.right = app->canvas_rect.left + 1;
    }
    if (App_RectHeight(&app->canvas_rect) < 1) {
        app->canvas_rect.bottom = app->canvas_rect.top + 1;
    }

    SetRect(
        &app->foreground_rect,
        app->palette_rect.left + LAYOUT_FOREGROUND_LEFT,
        app->palette_rect.top + LAYOUT_FOREGROUND_TOP,
        app->palette_rect.left + LAYOUT_FOREGROUND_LEFT + LAYOUT_SWATCH_SIZE,
        app->palette_rect.top + LAYOUT_FOREGROUND_TOP + LAYOUT_SWATCH_SIZE
    );

    SetRect(
        &app->background_rect,
        app->palette_rect.left + LAYOUT_BACKGROUND_LEFT,
        app->palette_rect.top + LAYOUT_BACKGROUND_TOP,
        app->palette_rect.left + LAYOUT_BACKGROUND_LEFT + LAYOUT_SWATCH_SIZE,
        app->palette_rect.top + LAYOUT_BACKGROUND_TOP + LAYOUT_SWATCH_SIZE
    );

    for (index = 0; index < PALETTE_COLOR_COUNT; ++index) {
        int row;
        int column;
        int cell_left;
        int cell_top;

        row = index / 8;
        column = index % 8;
        cell_left = app->palette_rect.left + LAYOUT_PALETTE_GRID_LEFT + (column * LAYOUT_PALETTE_CELL_STEP);
        cell_top = app->palette_rect.top + LAYOUT_PALETTE_GRID_TOP + (row * LAYOUT_PALETTE_CELL_STEP);
        SetRect(&app->palette_cells[index], cell_left, cell_top, cell_left + LAYOUT_PALETTE_CELL_SIZE, cell_top + LAYOUT_PALETTE_CELL_SIZE);
    }

    if (App_TextUiVisible(app)) {
        button_left = app->text_panel_rect.left + 8;
        button_top = app->text_panel_rect.top + (App_RectHeight(&app->text_panel_rect) - LAYOUT_TEXT_BUTTON_HEIGHT) / 2;
        button_bottom = button_top + LAYOUT_TEXT_BUTTON_HEIGHT;

        SetRect(&app->text_font_button_rect, button_left, button_top, button_left + LAYOUT_TEXT_FONT_BUTTON_WIDTH, button_bottom);
        button_left = app->text_font_button_rect.right + 6;
        SetRect(&app->text_bold_rect, button_left, button_top, button_left + LAYOUT_TEXT_STYLE_BUTTON_WIDTH, button_bottom);
        button_left = app->text_bold_rect.right + 4;
        SetRect(&app->text_italic_rect, button_left, button_top, button_left + LAYOUT_TEXT_STYLE_BUTTON_WIDTH, button_bottom);
        button_left = app->text_italic_rect.right + 4;
        SetRect(&app->text_underline_rect, button_left, button_top, button_left + LAYOUT_TEXT_STYLE_BUTTON_WIDTH, button_bottom);
        button_left = app->text_underline_rect.right + 8;
        SetRect(&app->text_opaque_rect, button_left, button_top, button_left + LAYOUT_TEXT_BG_BUTTON_WIDTH, button_bottom);
        button_left = app->text_opaque_rect.right + 8;
        SetRect(&app->text_commit_rect, button_left, button_top, button_left + LAYOUT_TEXT_ACTION_BUTTON_WIDTH, button_bottom);
        button_left = app->text_commit_rect.right + 4;
        SetRect(&app->text_cancel_rect, button_left, button_top, button_left + LAYOUT_TEXT_ACTION_BUTTON_WIDTH, button_bottom);
        button_left = app->text_cancel_rect.right + 8;
        SetRect(
            &app->text_sample_rect,
            button_left,
            app->text_panel_rect.top + 5,
            app->text_panel_rect.right - 8,
            app->text_panel_rect.bottom - 5
        );
    } else {
        SetRectEmpty(&app->text_font_button_rect);
        SetRectEmpty(&app->text_bold_rect);
        SetRectEmpty(&app->text_italic_rect);
        SetRectEmpty(&app->text_underline_rect);
        SetRectEmpty(&app->text_opaque_rect);
        SetRectEmpty(&app->text_commit_rect);
        SetRectEmpty(&app->text_cancel_rect);
        SetRectEmpty(&app->text_sample_rect);
    }

    App_UpdateScrollBars(app);
    App_PositionTextEdit(app);
}

void App_LoadResources(APP_STATE* app) {
    int index;
    int small_icon_width;
    int small_icon_height;
    int big_icon_width;
    int big_icon_height;

    app->toolbox_color_bitmap = LoadBitmap(app->instance, MAKEINTRESOURCE(IDB_TOOLBOX_COLOR));
    app->toolbox_bw_bitmap = LoadBitmap(app->instance, MAKEINTRESOURCE(IDB_TOOLBOX_BW));
    app->arrow_bitmap = LoadBitmap(app->instance, MAKEINTRESOURCE(IDB_ARROW));
    small_icon_width = GetSystemMetrics(SM_CXSMICON);
    small_icon_height = GetSystemMetrics(SM_CYSMICON);
    big_icon_width = GetSystemMetrics(SM_CXICON);
    big_icon_height = GetSystemMetrics(SM_CYICON);
    if (small_icon_width < 1) {
        small_icon_width = 16;
    }
    if (small_icon_height < 1) {
        small_icon_height = 16;
    }
    if (big_icon_width < 1) {
        big_icon_width = 32;
    }
    if (big_icon_height < 1) {
        big_icon_height = 32;
    }
    app->app_icon_small = (HICON)LoadImage(
        app->instance,
        MAKEINTRESOURCE(IDI_APP),
        IMAGE_ICON,
        small_icon_width,
        small_icon_height,
        LR_DEFAULTCOLOR
    );
    app->app_icon_big = (HICON)LoadImage(
        app->instance,
        MAKEINTRESOURCE(IDI_APP),
        IMAGE_ICON,
        big_icon_width,
        big_icon_height,
        LR_DEFAULTCOLOR
    );
    if (!app->app_icon_small) {
        app->app_icon_small = LoadIcon(app->instance, MAKEINTRESOURCE(IDI_APP));
    }
    if (!app->app_icon_big) {
        app->app_icon_big = app->app_icon_small;
    }
    for (index = 0; index < TOOL_COUNT; ++index) {
        app->tool_cursors[index] = LoadCursor(app->instance, MAKEINTRESOURCE(g_tools[index].cursor_id));
    }
    for (index = 0; index < PALETTE_COLOR_COUNT; ++index) {
        app->palette_brushes[index] = CreateSolidBrush(g_palette_colors[index]);
    }
    App_InitTextState(app);
}

void App_DestroyResources(APP_STATE* app) {
    int index;
    HICON small_icon;
    HICON big_icon;

    if (app->toolbox_color_bitmap) {
        DeleteObject(app->toolbox_color_bitmap);
        app->toolbox_color_bitmap = NULL;
    }
    if (app->toolbox_bw_bitmap) {
        DeleteObject(app->toolbox_bw_bitmap);
        app->toolbox_bw_bitmap = NULL;
    }
    if (app->arrow_bitmap) {
        DeleteObject(app->arrow_bitmap);
        app->arrow_bitmap = NULL;
    }
    small_icon = app->app_icon_small;
    big_icon = app->app_icon_big;
    if (small_icon) {
        DestroyIcon(small_icon);
    }
    if (big_icon && big_icon != small_icon) {
        DestroyIcon(big_icon);
    }
    app->app_icon_small = NULL;
    app->app_icon_big = NULL;
    for (index = 0; index < TOOL_COUNT; ++index) {
        app->tool_cursors[index] = NULL;
    }
    for (index = 0; index < PALETTE_COLOR_COUNT; ++index) {
        if (app->palette_brushes[index]) {
            DeleteObject(app->palette_brushes[index]);
            app->palette_brushes[index] = NULL;
        }
    }
    App_DestroyTextState(app);
}

void App_UpdateMenuState(APP_STATE* app) {
    if (!app->menu) {
        return;
    }

    CheckMenuItem(app->menu, ID_VIEW_COLOR_TOOLBOX, MF_BYCOMMAND | (app->use_bw_toolbox ? MF_UNCHECKED : MF_CHECKED));
    CheckMenuItem(app->menu, ID_VIEW_BW_TOOLBOX, MF_BYCOMMAND | (app->use_bw_toolbox ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuRadioItem(
        app->menu,
        ID_VIEW_ZOOM_1X,
        ID_VIEW_ZOOM_8X,
        app->zoom == 8 ? ID_VIEW_ZOOM_8X : (app->zoom == 4 ? ID_VIEW_ZOOM_4X : (app->zoom == 2 ? ID_VIEW_ZOOM_2X : ID_VIEW_ZOOM_1X)),
        MF_BYCOMMAND
    );
    CheckMenuItem(app->menu, ID_VIEW_PIXEL_GRID, MF_BYCOMMAND | (app->show_pixel_grid ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(app->menu, ID_EDIT_UNDO, MF_BYCOMMAND | (app->has_undo ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(app->menu, ID_VIEW_PIXEL_GRID, MF_BYCOMMAND | (app->zoom >= 4 ? MF_ENABLED : MF_GRAYED));
}

void App_SetZoom(APP_STATE* app, int zoom) {
    app->zoom = zoom;
    App_Layout(app);
    App_UpdateMenuState(app);
    InvalidateRect(app->window, NULL, TRUE);
}

void App_SetActiveTool(APP_STATE* app, TOOL_ID tool) {
    TCHAR status[128];
    int text_ui_was_visible;

    text_ui_was_visible = App_TextUiVisible(app);
    app->active_tool = tool;
    _sntprintf(status, ARRAYSIZE(status), TEXT("Tool: %s"), g_tools[tool].name);
    status[ARRAYSIZE(status) - 1] = 0;
    App_SetStatus(app, status);
    if (text_ui_was_visible != App_TextUiVisible(app)) {
        App_Layout(app);
        InvalidateRect(app->window, NULL, TRUE);
        return;
    }

    InvalidateRect(app->window, &app->toolbox_rect, FALSE);
    if (App_TextUiVisible(app)) {
        InvalidateRect(app->window, &app->text_panel_rect, FALSE);
    }
}

void App_ResetDocument(APP_STATE* app, int width, int height) {
    if (app->text_edit_active) {
        App_CancelTextEdit(app);
    }
    if (!App_CreateCanvas(app, width, height)) {
        MessageBox(app->window, TEXT("Unable to create the drawing surface."), APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }

    app->document_path[0] = 0;
    app->dirty = 0;
    app->scroll_x = 0;
    app->scroll_y = 0;
    App_UpdateTitle(app);
    App_Layout(app);
    App_SetStatus(app, TEXT("New image"));
    InvalidateRect(app->window, NULL, TRUE);
}
