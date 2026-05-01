#include "app.h"

static RECT App_LineDirtyRect(POINT start, POINT end, int padding) {
    RECT rect;

    rect.left = min(start.x, end.x) - padding;
    rect.top = min(start.y, end.y) - padding;
    rect.right = max(start.x, end.x) + padding;
    rect.bottom = max(start.y, end.y) + padding;
    return rect;
}

static RECT App_ShapeDirtyRect(POINT start, POINT end) {
    return App_LineDirtyRect(start, end, 2);
}

static RECT App_UnionDirtyRects(RECT left, RECT right) {
    RECT rect;

    rect.left = min(left.left, right.left);
    rect.top = min(left.top, right.top);
    rect.right = max(left.right, right.right);
    rect.bottom = max(left.bottom, right.bottom);
    return rect;
}

RECT App_GetToolRect(const APP_STATE* app, TOOL_ID tool) {
    RECT rect;
    int row;
    int column;
    int width;
    int height;

    row = ((int)tool) / TOOLBOX_COLUMNS;
    column = ((int)tool) % TOOLBOX_COLUMNS;
    width = App_RectWidth(&app->toolbox_art_rect);
    height = App_RectHeight(&app->toolbox_art_rect);

    SetRect(
        &rect,
        app->toolbox_art_rect.left + (column * width) / TOOLBOX_COLUMNS,
        app->toolbox_art_rect.top + (row * height) / TOOLBOX_ROWS,
        app->toolbox_art_rect.left + ((column + 1) * width) / TOOLBOX_COLUMNS,
        app->toolbox_art_rect.top + ((row + 1) * height) / TOOLBOX_ROWS
    );

    return rect;
}

TOOL_ID App_HitTestToolbox(const APP_STATE* app, POINT point) {
    int index;
    int relative_x;
    int relative_y;
    int column;
    int row;

    if (!PtInRect(&app->toolbox_art_rect, point)) {
        return TOOL_COUNT;
    }

    relative_x = point.x - app->toolbox_art_rect.left;
    relative_y = point.y - app->toolbox_art_rect.top;
    column = (relative_x * TOOLBOX_COLUMNS) / max(1, App_RectWidth(&app->toolbox_art_rect));
    row = (relative_y * TOOLBOX_ROWS) / max(1, App_RectHeight(&app->toolbox_art_rect));

    if (column < 0 || column >= TOOLBOX_COLUMNS || row < 0 || row >= TOOLBOX_ROWS) {
        return TOOL_COUNT;
    }

    index = (row * TOOLBOX_COLUMNS) + column;
    return index >= 0 && index < TOOL_COUNT ? (TOOL_ID)index : TOOL_COUNT;
}

int App_HitTestPalette(const APP_STATE* app, POINT point) {
    int index;

    for (index = 0; index < PALETTE_COLOR_COUNT; ++index) {
        if (PtInRect(&app->palette_cells[index], point)) {
            return index;
        }
    }

    return -1;
}

void App_DoImmediateTool(APP_STATE* app, POINT canvas_point, int button) {
    COLORREF color;
    TCHAR status[128];

    color = App_GetDrawColor(app, button);

    switch (app->active_tool) {
    case TOOL_FILL:
        App_CopyCanvasToUndo(app);
        App_FillAt(app, canvas_point, color);
        App_MarkDirty(app);
        App_SetStatus(app, TEXT("Filled region"));
        InvalidateRect(app->window, &app->canvas_area_rect, FALSE);
        break;
    case TOOL_TEXT:
        App_BeginTextEdit(app, canvas_point);
        break;
    case TOOL_FREE_SELECT:
    case TOOL_SELECT:
        _sntprintf(status, ARRAYSIZE(status), TEXT("%s is not interactive yet in this CE port"), g_tools[app->active_tool].name);
        status[ARRAYSIZE(status) - 1] = 0;
        App_SetStatus(app, status);
        break;
    default:
        break;
    }
}

int App_ToolUsesDrag(TOOL_ID tool) {
    switch (tool) {
    case TOOL_COLOR_ERASER:
    case TOOL_ERASER:
    case TOOL_BRUSH:
    case TOOL_AIRBRUSH:
    case TOOL_LINE:
    case TOOL_CURVE:
    case TOOL_RECT:
    case TOOL_RECT_FILL:
    case TOOL_POLYGON:
    case TOOL_POLYGON_FILL:
    case TOOL_ELLIPSE:
    case TOOL_ELLIPSE_FILL:
    case TOOL_ROUNDRECT:
    case TOOL_ROUNDRECT_FILL:
        return 1;
    default:
        return 0;
    }
}

int App_ToolUsesShapePreview(TOOL_ID tool) {
    switch (tool) {
    case TOOL_LINE:
    case TOOL_CURVE:
    case TOOL_POLYGON:
    case TOOL_POLYGON_FILL:
    case TOOL_RECT:
    case TOOL_RECT_FILL:
    case TOOL_ELLIPSE:
    case TOOL_ELLIPSE_FILL:
    case TOOL_ROUNDRECT:
    case TOOL_ROUNDRECT_FILL:
        return 1;
    default:
        return 0;
    }
}

void App_BeginDrag(APP_STATE* app, POINT canvas_point, int button) {
    RECT dirty_rect;

    app->drawing = 1;
    app->active_button = button;
    app->drag_start = canvas_point;
    app->drag_last = canvas_point;
    app->drag_current = canvas_point;
    App_CopyCanvasToUndo(app);
    SetCapture(app->window);

    if (app->active_tool == TOOL_BRUSH || app->active_tool == TOOL_ERASER || app->active_tool == TOOL_COLOR_ERASER) {
        App_DrawLine(app, canvas_point, canvas_point, App_GetDrawColor(app, button), App_GetStrokeWidth(app->active_tool));
        App_MarkDirty(app);
        dirty_rect = App_LineDirtyRect(canvas_point, canvas_point, App_GetStrokeWidth(app->active_tool) + 2);
        App_InvalidateCanvasRect(app, &dirty_rect);
    } else if (app->active_tool == TOOL_AIRBRUSH) {
        App_DrawSpray(app, canvas_point, App_GetDrawColor(app, button));
        App_MarkDirty(app);
        dirty_rect = App_LineDirtyRect(canvas_point, canvas_point, 8);
        App_InvalidateCanvasRect(app, &dirty_rect);
    }
}

void App_ContinueDrag(APP_STATE* app, POINT canvas_point) {
    COLORREF color;
    POINT previous_point;
    RECT dirty_rect;

    if (!app->drawing) {
        return;
    }
    if (canvas_point.x == app->drag_current.x && canvas_point.y == app->drag_current.y) {
        return;
    }

    color = App_GetDrawColor(app, app->active_button);
    previous_point = app->drag_current;
    app->drag_current = canvas_point;

    if (App_ToolUsesShapePreview(app->active_tool)) {
        App_RestoreUndo(app);
        App_DrawShape(app, app->active_tool, app->drag_start, canvas_point, color);
        App_MarkDirty(app);
        dirty_rect = App_UnionDirtyRects(
            App_ShapeDirtyRect(app->drag_start, previous_point),
            App_ShapeDirtyRect(app->drag_start, canvas_point)
        );
        App_InvalidateCanvasRect(app, &dirty_rect);
    } else if (app->active_tool == TOOL_BRUSH || app->active_tool == TOOL_ERASER || app->active_tool == TOOL_COLOR_ERASER) {
        App_DrawLine(app, app->drag_last, canvas_point, color, App_GetStrokeWidth(app->active_tool));
        App_MarkDirty(app);
        dirty_rect = App_LineDirtyRect(app->drag_last, canvas_point, App_GetStrokeWidth(app->active_tool) + 2);
        App_InvalidateCanvasRect(app, &dirty_rect);
    } else if (app->active_tool == TOOL_AIRBRUSH) {
        App_DrawSpray(app, canvas_point, color);
        App_MarkDirty(app);
        dirty_rect = App_LineDirtyRect(canvas_point, canvas_point, 8);
        App_InvalidateCanvasRect(app, &dirty_rect);
    }

    app->drag_last = canvas_point;
}

void App_EndDrag(APP_STATE* app, POINT canvas_point) {
    if (!app->drawing) {
        return;
    }

    App_ContinueDrag(app, canvas_point);
    app->drawing = 0;
    app->active_button = 0;
    ReleaseCapture();
}

HCURSOR App_GetToolCursor(const APP_STATE* app) {
    return app->tool_cursors[app->active_tool];
}
