#include <stdlib.h>
#include <string.h>

#include "app.h"

void App_DestroyCanvas(APP_STATE* app) {
    if (app->canvas_dc) {
        DeleteDC(app->canvas_dc);
        app->canvas_dc = NULL;
    }
    if (app->undo_dc) {
        DeleteDC(app->undo_dc);
        app->undo_dc = NULL;
    }
    if (app->canvas_bitmap) {
        DeleteObject(app->canvas_bitmap);
        app->canvas_bitmap = NULL;
    }
    if (app->undo_bitmap) {
        DeleteObject(app->undo_bitmap);
        app->undo_bitmap = NULL;
    }
    app->canvas_bits = NULL;
    app->undo_bits = NULL;
}

int App_CreateCanvas(APP_STATE* app, int width, int height) {
    BITMAPINFO bitmap_info;
    HBITMAP bitmap;
    HBITMAP undo_bitmap;
    HDC canvas_dc;
    HDC undo_dc;
    void* canvas_bits;
    void* undo_bits;
    RECT fill_rect;
    HBRUSH white_brush;

    memset(&bitmap_info, 0, sizeof(bitmap_info));
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    canvas_dc = CreateCompatibleDC(NULL);
    undo_dc = CreateCompatibleDC(NULL);
    if (!canvas_dc || !undo_dc) {
        if (canvas_dc) {
            DeleteDC(canvas_dc);
        }
        if (undo_dc) {
            DeleteDC(undo_dc);
        }
        return 0;
    }

    bitmap = CreateDIBSection(canvas_dc, &bitmap_info, DIB_RGB_COLORS, &canvas_bits, NULL, 0);
    undo_bitmap = CreateDIBSection(undo_dc, &bitmap_info, DIB_RGB_COLORS, &undo_bits, NULL, 0);
    if (!bitmap || !undo_bitmap || !canvas_bits || !undo_bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (undo_bitmap) {
            DeleteObject(undo_bitmap);
        }
        DeleteDC(canvas_dc);
        DeleteDC(undo_dc);
        return 0;
    }

    SelectObject(canvas_dc, bitmap);
    SelectObject(undo_dc, undo_bitmap);

    App_DestroyCanvas(app);
    app->canvas_dc = canvas_dc;
    app->undo_dc = undo_dc;
    app->canvas_bitmap = bitmap;
    app->undo_bitmap = undo_bitmap;
    app->canvas_bits = (COLORREF*)canvas_bits;
    app->undo_bits = (COLORREF*)undo_bits;
    app->canvas_width = width;
    app->canvas_height = height;
    app->has_undo = 0;

    SetRect(&fill_rect, 0, 0, width, height);
    white_brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(app->canvas_dc, &fill_rect, white_brush);
    FillRect(app->undo_dc, &fill_rect, white_brush);
    DeleteObject(white_brush);

    return 1;
}

int App_ResizeCanvas(APP_STATE* app, int width, int height) {
    COLORREF* old_bits;
    size_t old_pixel_count;
    int old_width;
    int old_height;
    int copy_width;
    int copy_height;
    int y;

    if (width <= 0 || height <= 0
        || width > APP_MAX_BITMAP_WIDTH
        || height > APP_MAX_BITMAP_HEIGHT
        || ((unsigned long)width * (unsigned long)height) > APP_MAX_BITMAP_PIXELS) {
        return 0;
    }

    old_width = app->canvas_width;
    old_height = app->canvas_height;
    old_pixel_count = (size_t)old_width * (size_t)old_height;
    old_bits = (COLORREF*)malloc(old_pixel_count * sizeof(COLORREF));
    if (!old_bits) {
        return 0;
    }

    memcpy(old_bits, app->canvas_bits, old_pixel_count * sizeof(COLORREF));
    copy_width = min(old_width, width);
    copy_height = min(old_height, height);

    if (!App_CreateCanvas(app, width, height)) {
        free(old_bits);
        return 0;
    }

    for (y = 0; y < copy_height; ++y) {
        memcpy(
            app->canvas_bits + ((size_t)y * (size_t)app->canvas_width),
            old_bits + ((size_t)y * (size_t)old_width),
            (size_t)copy_width * sizeof(COLORREF)
        );
    }

    free(old_bits);
    return 1;
}

void App_CopyCanvasToUndo(APP_STATE* app) {
    BitBlt(app->undo_dc, 0, 0, app->canvas_width, app->canvas_height, app->canvas_dc, 0, 0, SRCCOPY);
    app->has_undo = 1;
    App_UpdateMenuState(app);
}

void App_RestoreUndo(APP_STATE* app) {
    if (!app->has_undo || !app->undo_dc || !app->canvas_dc) {
        return;
    }

    BitBlt(app->canvas_dc, 0, 0, app->canvas_width, app->canvas_height, app->undo_dc, 0, 0, SRCCOPY);
}

void App_ConsumeUndo(APP_STATE* app) {
    app->has_undo = 0;
    App_UpdateMenuState(app);
}

void App_MarkDirty(APP_STATE* app) {
    if (app->dirty) {
        return;
    }
    app->dirty = 1;
    App_UpdateTitle(app);
}

void App_MarkSaved(APP_STATE* app, LPCTSTR path) {
    app->dirty = 0;
    if (path) {
        App_CopyString(app->document_path, ARRAYSIZE(app->document_path), path);
    }
    App_UpdateTitle(app);
}

int App_GetCanvasPageWidth(const APP_STATE* app) {
    int page_width;

    page_width = App_RectWidth(&app->canvas_rect) / max(1, app->zoom);
    if (page_width < 1) {
        page_width = 1;
    }
    if (page_width > app->canvas_width) {
        page_width = app->canvas_width;
    }
    return page_width;
}

int App_GetCanvasPageHeight(const APP_STATE* app) {
    int page_height;

    page_height = App_RectHeight(&app->canvas_rect) / max(1, app->zoom);
    if (page_height < 1) {
        page_height = 1;
    }
    if (page_height > app->canvas_height) {
        page_height = app->canvas_height;
    }
    return page_height;
}

void App_UpdateScrollBars(APP_STATE* app) {
    SCROLLINFO info;
    int page_width;
    int page_height;
    int max_x;
    int max_y;

    page_width = App_GetCanvasPageWidth(app);
    page_height = App_GetCanvasPageHeight(app);
    max_x = max(0, app->canvas_width - page_width);
    max_y = max(0, app->canvas_height - page_height);

    if (app->scroll_x > max_x) {
        app->scroll_x = max_x;
    }
    if (app->scroll_y > max_y) {
        app->scroll_y = max_y;
    }

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    info.nMin = 0;
    info.nMax = max(0, app->canvas_width - 1);
    info.nPage = (UINT)page_width;
    info.nPos = app->scroll_x;
    SetScrollInfo(app->window, SB_HORZ, &info, TRUE);

    info.nMax = max(0, app->canvas_height - 1);
    info.nPage = (UINT)page_height;
    info.nPos = app->scroll_y;
    SetScrollInfo(app->window, SB_VERT, &info, TRUE);
}

void App_SetScrollPosition(APP_STATE* app, int bar, int position) {
    int max_position;

    if (bar == SB_HORZ) {
        max_position = max(0, app->canvas_width - App_GetCanvasPageWidth(app));
        if (position < 0) {
            position = 0;
        }
        if (position > max_position) {
            position = max_position;
        }
        if (position == app->scroll_x) {
            return;
        }
        app->scroll_x = position;
    } else {
        max_position = max(0, app->canvas_height - App_GetCanvasPageHeight(app));
        if (position < 0) {
            position = 0;
        }
        if (position > max_position) {
            position = max_position;
        }
        if (position == app->scroll_y) {
            return;
        }
        app->scroll_y = position;
    }

    App_UpdateScrollBars(app);
    App_PositionTextEdit(app);
    InvalidateRect(app->window, &app->canvas_area_rect, FALSE);
}

void App_HandleScroll(APP_STATE* app, int bar, int request_code) {
    SCROLLINFO info;
    int position;
    int page_size;

    page_size = bar == SB_HORZ ? App_GetCanvasPageWidth(app) : App_GetCanvasPageHeight(app);
    position = bar == SB_HORZ ? app->scroll_x : app->scroll_y;

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    GetScrollInfo(app->window, bar, &info);

    switch (request_code) {
    case SB_LINEUP:
        position -= 16;
        break;
    case SB_LINEDOWN:
        position += 16;
        break;
    case SB_PAGEUP:
        position -= page_size;
        break;
    case SB_PAGEDOWN:
        position += page_size;
        break;
    case SB_TOP:
        position = 0;
        break;
    case SB_BOTTOM:
        position = info.nMax;
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        position = info.nTrackPos;
        break;
    default:
        return;
    }

    App_SetScrollPosition(app, bar, position);
}

int App_WindowToCanvas(const APP_STATE* app, POINT point, POINT* canvas_point) {
    if (!PtInRect(&app->canvas_rect, point)) {
        return 0;
    }

    canvas_point->x = ((point.x - app->canvas_rect.left) / app->zoom) + app->scroll_x;
    canvas_point->y = ((point.y - app->canvas_rect.top) / app->zoom) + app->scroll_y;

    if (canvas_point->x < 0 || canvas_point->x >= app->canvas_width || canvas_point->y < 0 || canvas_point->y >= app->canvas_height) {
        return 0;
    }

    return 1;
}

COLORREF App_GetDrawColor(const APP_STATE* app, int button) {
    if (app->active_tool == TOOL_ERASER || app->active_tool == TOOL_COLOR_ERASER) {
        return app->background_color;
    }
    if (button == MK_RBUTTON) {
        return app->background_color;
    }
    return app->foreground_color;
}

POINT App_PointFromLParam(LPARAM l_param) {
    POINT point;

    point.x = (int)(short)LOWORD(l_param);
    point.y = (int)(short)HIWORD(l_param);
    return point;
}

int App_GetStrokeWidth(TOOL_ID tool) {
    switch (tool) {
    case TOOL_BRUSH:
        return 4;
    case TOOL_COLOR_ERASER:
    case TOOL_ERASER:
        return 10;
    case TOOL_AIRBRUSH:
        return 2;
    default:
        return 1;
    }
}

int App_IsFilledShapeTool(TOOL_ID tool) {
    switch (tool) {
    case TOOL_RECT_FILL:
    case TOOL_ROUNDRECT_FILL:
    case TOOL_ELLIPSE_FILL:
    case TOOL_POLYGON_FILL:
        return 1;
    default:
        return 0;
    }
}

int App_IsPixelInside(const APP_STATE* app, int x, int y) {
    return x >= 0 && y >= 0 && x < app->canvas_width && y < app->canvas_height;
}

static COLORREF* App_GetPixelPtr(APP_STATE* app, int x, int y) {
    return app->canvas_bits + (y * app->canvas_width) + x;
}

COLORREF App_GetCanvasPixel(APP_STATE* app, int x, int y) {
    if (!App_IsPixelInside(app, x, y)) {
        return RGB(255, 255, 255);
    }
    return *App_GetPixelPtr(app, x, y);
}

void App_SetCanvasPixel(APP_STATE* app, int x, int y, COLORREF color) {
    if (!App_IsPixelInside(app, x, y)) {
        return;
    }
    *App_GetPixelPtr(app, x, y) = color;
}

void App_DrawLine(APP_STATE* app, POINT start, POINT end, COLORREF color, int width) {
    HPEN pen;
    HPEN old_pen;
    HGDIOBJ old_brush;

    pen = CreatePen(PS_SOLID, width, color);
    old_pen = SelectObject(app->canvas_dc, pen);
    old_brush = SelectObject(app->canvas_dc, GetStockObject(HOLLOW_BRUSH));
    MoveToEx(app->canvas_dc, start.x, start.y, NULL);
    LineTo(app->canvas_dc, end.x, end.y);
    SetPixel(app->canvas_dc, end.x, end.y, color);
    SelectObject(app->canvas_dc, old_brush);
    SelectObject(app->canvas_dc, old_pen);
    DeleteObject(pen);
}

void App_DrawSpray(APP_STATE* app, POINT center, COLORREF color) {
    int sample;

    for (sample = 0; sample < 24; ++sample) {
        int offset_x;
        int offset_y;

        app->spray_seed = (app->spray_seed * 1103515245u) + 12345u;
        offset_x = (int)((app->spray_seed >> 16) % 11) - 5;
        app->spray_seed = (app->spray_seed * 1103515245u) + 12345u;
        offset_y = (int)((app->spray_seed >> 16) % 11) - 5;

        if ((offset_x * offset_x) + (offset_y * offset_y) <= 25) {
            App_SetCanvasPixel(app, center.x + offset_x, center.y + offset_y, color);
        }
    }
}

void App_DrawShape(APP_STATE* app, TOOL_ID tool, POINT start, POINT end, COLORREF color) {
    HPEN pen;
    HPEN old_pen;
    HBRUSH fill_brush;
    HGDIOBJ old_brush;
    RECT rect;

    rect.left = min(start.x, end.x);
    rect.top = min(start.y, end.y);
    rect.right = max(start.x, end.x);
    rect.bottom = max(start.y, end.y);

    pen = CreatePen(PS_SOLID, 1, color);
    fill_brush = App_IsFilledShapeTool(tool) ? CreateSolidBrush(app->background_color) : (HBRUSH)GetStockObject(HOLLOW_BRUSH);
    old_pen = SelectObject(app->canvas_dc, pen);
    old_brush = SelectObject(app->canvas_dc, fill_brush);

    switch (tool) {
    case TOOL_LINE:
    case TOOL_CURVE:
    case TOOL_POLYGON:
    case TOOL_POLYGON_FILL:
        MoveToEx(app->canvas_dc, start.x, start.y, NULL);
        LineTo(app->canvas_dc, end.x, end.y);
        break;
    case TOOL_RECT:
    case TOOL_RECT_FILL:
        Rectangle(app->canvas_dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1);
        break;
    case TOOL_ELLIPSE:
    case TOOL_ELLIPSE_FILL:
        Ellipse(app->canvas_dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1);
        break;
    case TOOL_ROUNDRECT:
    case TOOL_ROUNDRECT_FILL:
        RoundRect(app->canvas_dc, rect.left, rect.top, rect.right + 1, rect.bottom + 1, 12, 12);
        break;
    default:
        break;
    }

    SelectObject(app->canvas_dc, old_brush);
    SelectObject(app->canvas_dc, old_pen);
    if (fill_brush != GetStockObject(HOLLOW_BRUSH)) {
        DeleteObject(fill_brush);
    }
    DeleteObject(pen);
}

void App_DrawTextBlock(APP_STATE* app, LPCTSTR text, const RECT* canvas_rect) {
    HGDIOBJ old_font;
    COLORREF old_color;
    COLORREF old_background;
    int old_mode;
    RECT draw_rect;
    HBRUSH background_brush;

    draw_rect = *canvas_rect;
    old_font = SelectObject(app->canvas_dc, app->text_font ? app->text_font : GetStockObject(SYSTEM_FONT));
    old_color = SetTextColor(app->canvas_dc, app->foreground_color);
    old_background = SetBkColor(app->canvas_dc, app->background_color);
    old_mode = SetBkMode(app->canvas_dc, app->text_background_opaque ? OPAQUE : TRANSPARENT);

    if (app->text_background_opaque) {
        background_brush = CreateSolidBrush(app->background_color);
        FillRect(app->canvas_dc, &draw_rect, background_brush);
        DeleteObject(background_brush);
    }

    DrawText(app->canvas_dc, text, -1, &draw_rect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

    SetBkMode(app->canvas_dc, old_mode);
    SetBkColor(app->canvas_dc, old_background);
    SetTextColor(app->canvas_dc, old_color);
    SelectObject(app->canvas_dc, old_font);
}

void App_FillAt(APP_STATE* app, POINT point, COLORREF color) {
    COLORREF target;
    POINT* stack;
    size_t stack_size;
    size_t stack_index;

    target = App_GetCanvasPixel(app, point.x, point.y);
    if (target == color) {
        return;
    }

    stack_size = (size_t)app->canvas_width * (size_t)app->canvas_height;
    stack = (POINT*)malloc(stack_size * sizeof(POINT));
    if (!stack) {
        App_SetStatus(app, TEXT("Not enough memory for fill operation"));
        return;
    }

    stack_index = 0;
    App_SetCanvasPixel(app, point.x, point.y, color);
    stack[stack_index++] = point;

    while (stack_index > 0) {
        POINT current;

        current = stack[--stack_index];
        if (current.x > 0) {
            if (App_GetCanvasPixel(app, current.x - 1, current.y) == target) {
                App_SetCanvasPixel(app, current.x - 1, current.y, color);
                if (stack_index < stack_size) {
                    stack[stack_index].x = current.x - 1;
                    stack[stack_index].y = current.y;
                    ++stack_index;
                }
            }
        }
        if (current.x + 1 < app->canvas_width) {
            if (App_GetCanvasPixel(app, current.x + 1, current.y) == target) {
                App_SetCanvasPixel(app, current.x + 1, current.y, color);
                if (stack_index < stack_size) {
                    stack[stack_index].x = current.x + 1;
                    stack[stack_index].y = current.y;
                    ++stack_index;
                }
            }
        }
        if (current.y > 0) {
            if (App_GetCanvasPixel(app, current.x, current.y - 1) == target) {
                App_SetCanvasPixel(app, current.x, current.y - 1, color);
                if (stack_index < stack_size) {
                    stack[stack_index].x = current.x;
                    stack[stack_index].y = current.y - 1;
                    ++stack_index;
                }
            }
        }
        if (current.y + 1 < app->canvas_height) {
            if (App_GetCanvasPixel(app, current.x, current.y + 1) == target) {
                App_SetCanvasPixel(app, current.x, current.y + 1, color);
                if (stack_index < stack_size) {
                    stack[stack_index].x = current.x;
                    stack[stack_index].y = current.y + 1;
                    ++stack_index;
                }
            }
        }
    }

    free(stack);
}

void App_InvalidateCanvasPixels(APP_STATE* app, int left, int top, int right, int bottom) {
    RECT dirty_rect;
    RECT clipped_rect;

    if (right < left) {
        int swap_value;

        swap_value = left;
        left = right;
        right = swap_value;
    }
    if (bottom < top) {
        int swap_value;

        swap_value = top;
        top = bottom;
        bottom = swap_value;
    }

    dirty_rect.left = app->canvas_rect.left + ((left - app->scroll_x) * app->zoom);
    dirty_rect.top = app->canvas_rect.top + ((top - app->scroll_y) * app->zoom);
    dirty_rect.right = app->canvas_rect.left + ((right - app->scroll_x + 1) * app->zoom);
    dirty_rect.bottom = app->canvas_rect.top + ((bottom - app->scroll_y + 1) * app->zoom);
    InflateRect(&dirty_rect, LAYOUT_CANVAS_FRAME_INSET + 2, LAYOUT_CANVAS_FRAME_INSET + 2);

    if (!IntersectRect(&clipped_rect, &dirty_rect, &app->canvas_area_rect)) {
        return;
    }

    InvalidateRect(app->window, &clipped_rect, FALSE);
}

void App_InvalidateCanvasRect(APP_STATE* app, const RECT* canvas_rect) {
    App_InvalidateCanvasPixels(app, canvas_rect->left, canvas_rect->top, canvas_rect->right, canvas_rect->bottom);
}
