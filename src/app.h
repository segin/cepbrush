#ifndef APP_H
#define APP_H

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <aygshell.h>
#include <tchar.h>
#include <stddef.h>
#include <stdio.h>

#ifndef _sntprintf
#ifdef _UNICODE
#define _sntprintf _snwprintf
#else
#define _sntprintf _snprintf
#endif
#endif

#include "resource.h"

#define APP_CLASS_NAME TEXT("CEPaintbrushWindow")
#define APP_TITLE TEXT("CE Paintbrush")

#define DEFAULT_CANVAS_WIDTH 640
#define DEFAULT_CANVAS_HEIGHT 400

#define APP_MAX_BITMAP_WIDTH 2048
#define APP_MAX_BITMAP_HEIGHT 2048
#define APP_MAX_BITMAP_PIXELS 4194304UL

#define TOOLBOX_ART_WIDTH 58
#define TOOLBOX_ART_HEIGHT 279
#define TOOLBOX_COLUMNS 2
#define TOOLBOX_ROWS 9
#define TOOL_COUNT (TOOLBOX_COLUMNS * TOOLBOX_ROWS)

#define MENU_ITEM_COUNT 4
#define PALETTE_COLOR_COUNT 16

#define LAYOUT_MARGIN 6
#define LAYOUT_TOOLBOX_WIDTH 92
#define LAYOUT_MENU_HEIGHT 24
#define LAYOUT_PALETTE_HEIGHT 74
#define LAYOUT_STATUS_HEIGHT 22
#define LAYOUT_MENU_ITEM_WIDTH 48
#define LAYOUT_MENU_ITEM_STEP 50
#define LAYOUT_MENU_ITEM_TOP 2
#define LAYOUT_MENU_ITEM_BOTTOM 2
#define LAYOUT_TOOLBOX_ART_TOP 26
#define LAYOUT_MIN_CANVAS_FRAME 10
#define LAYOUT_CANVAS_FRAME_INSET 3
#define LAYOUT_SWATCH_SIZE 30
#define LAYOUT_FOREGROUND_LEFT 12
#define LAYOUT_FOREGROUND_TOP 10
#define LAYOUT_BACKGROUND_LEFT 28
#define LAYOUT_BACKGROUND_TOP 26
#define LAYOUT_PALETTE_GRID_LEFT 84
#define LAYOUT_PALETTE_GRID_TOP 12
#define LAYOUT_PALETTE_CELL_STEP 24
#define LAYOUT_PALETTE_CELL_SIZE 20
#define LAYOUT_TEXT_PANEL_HEIGHT 38
#define LAYOUT_TEXT_BUTTON_HEIGHT 24
#define LAYOUT_TEXT_FONT_BUTTON_WIDTH 68
#define LAYOUT_TEXT_STYLE_BUTTON_WIDTH 24
#define LAYOUT_TEXT_BG_BUTTON_WIDTH 76
#define LAYOUT_TEXT_ACTION_BUTTON_WIDTH 56

#define DEFAULT_TEXT_BOX_WIDTH 240
#define DEFAULT_TEXT_BOX_HEIGHT 96

#ifndef ARRAYSIZE
#define ARRAYSIZE(values) (sizeof(values) / sizeof((values)[0]))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

typedef enum TOOL_ID_TAG {
    TOOL_FREE_SELECT = 0,
    TOOL_SELECT = 1,
    TOOL_AIRBRUSH = 2,
    TOOL_TEXT = 3,
    TOOL_COLOR_ERASER = 4,
    TOOL_ERASER = 5,
    TOOL_FILL = 6,
    TOOL_BRUSH = 7,
    TOOL_CURVE = 8,
    TOOL_LINE = 9,
    TOOL_RECT = 10,
    TOOL_RECT_FILL = 11,
    TOOL_ROUNDRECT = 12,
    TOOL_ROUNDRECT_FILL = 13,
    TOOL_ELLIPSE = 14,
    TOOL_ELLIPSE_FILL = 15,
    TOOL_POLYGON = 16,
    TOOL_POLYGON_FILL = 17
} TOOL_ID;

typedef struct TOOL_DEF_TAG {
    LPCTSTR name;
    int cursor_id;
} TOOL_DEF;

typedef struct APP_STATE_TAG {
    HINSTANCE instance;
    HWND window;
    HMENU menu;
    HWND command_bar;
    HBITMAP toolbox_color_bitmap;
    HBITMAP toolbox_bw_bitmap;
    HBITMAP arrow_bitmap;
    HCURSOR tool_cursors[TOOL_COUNT];
    HBRUSH palette_brushes[PALETTE_COLOR_COUNT];
    HICON app_icon_small;
    HICON app_icon_big;
    HBITMAP canvas_bitmap;
    HBITMAP undo_bitmap;
    HDC canvas_dc;
    HDC undo_dc;
    COLORREF* canvas_bits;
    COLORREF* undo_bits;
    int canvas_width;
    int canvas_height;
    int command_bar_height;
    RECT menu_bar_rect;
    RECT menu_item_rects[MENU_ITEM_COUNT];
    RECT toolbox_rect;
    RECT toolbox_art_rect;
    RECT text_panel_rect;
    RECT text_font_button_rect;
    RECT text_bold_rect;
    RECT text_italic_rect;
    RECT text_underline_rect;
    RECT text_opaque_rect;
    RECT text_commit_rect;
    RECT text_cancel_rect;
    RECT text_sample_rect;
    RECT palette_rect;
    RECT status_rect;
    RECT canvas_area_rect;
    RECT canvas_frame_rect;
    RECT canvas_rect;
    RECT foreground_rect;
    RECT background_rect;
    RECT palette_cells[PALETTE_COLOR_COUNT];
    HWND text_edit_window;
    HFONT text_font;
    HBRUSH text_preview_brush;
    LOGFONT text_logfont;
    RECT text_canvas_rect;
    int scroll_x;
    int scroll_y;
    int zoom;
    int use_bw_toolbox;
    int show_pixel_grid;
    int active_menu_index;
    TOOL_ID active_tool;
    COLORREF foreground_color;
    COLORREF background_color;
    TCHAR document_path[MAX_PATH];
    TCHAR status_text[160];
    DWORD spray_seed;
    int dirty;
    int has_undo;
    int text_edit_active;
    int text_background_opaque;
    int drawing;
    int active_button;
    POINT drag_start;
    POINT drag_last;
    POINT drag_current;
} APP_STATE;

extern APP_STATE g_app;
extern const TOOL_DEF g_tools[TOOL_COUNT];
extern const COLORREF g_palette_colors[PALETTE_COLOR_COUNT];
extern const TCHAR* g_menu_labels[MENU_ITEM_COUNT];

int App_RectWidth(const RECT* rect);
int App_RectHeight(const RECT* rect);
void App_CopyString(TCHAR* destination, size_t destination_count, LPCTSTR source);
void App_UpdateTitle(APP_STATE* app);
void App_SetStatus(APP_STATE* app, LPCTSTR text);
void App_DrawBevel(HDC hdc, const RECT* rect, COLORREF light, COLORREF dark);
void App_DrawSunkenPanel(HDC hdc, const RECT* rect);
void App_DrawRaisedPanel(HDC hdc, const RECT* rect);
void App_Layout(APP_STATE* app);
int App_CreateShellMenuBar(APP_STATE* app);
void App_DestroyShellMenuBar(APP_STATE* app);
void App_LoadResources(APP_STATE* app);
void App_DestroyResources(APP_STATE* app);
void App_UpdateMenuState(APP_STATE* app);
void App_SetZoom(APP_STATE* app, int zoom);
void App_SetActiveTool(APP_STATE* app, TOOL_ID tool);
void App_ResetDocument(APP_STATE* app, int width, int height);

void App_DestroyCanvas(APP_STATE* app);
int App_CreateCanvas(APP_STATE* app, int width, int height);
int App_ResizeCanvas(APP_STATE* app, int width, int height);
void App_CopyCanvasToUndo(APP_STATE* app);
void App_RestoreUndo(APP_STATE* app);
void App_ConsumeUndo(APP_STATE* app);
void App_MarkDirty(APP_STATE* app);
void App_MarkSaved(APP_STATE* app, LPCTSTR path);
int App_GetCanvasPageWidth(const APP_STATE* app);
int App_GetCanvasPageHeight(const APP_STATE* app);
void App_UpdateScrollBars(APP_STATE* app);
void App_SetScrollPosition(APP_STATE* app, int bar, int position);
void App_HandleScroll(APP_STATE* app, int bar, int request_code);
int App_WindowToCanvas(const APP_STATE* app, POINT point, POINT* canvas_point);
COLORREF App_GetDrawColor(const APP_STATE* app, int button);
POINT App_PointFromLParam(LPARAM l_param);
int App_GetStrokeWidth(TOOL_ID tool);
int App_IsFilledShapeTool(TOOL_ID tool);
int App_IsPixelInside(const APP_STATE* app, int x, int y);
COLORREF App_GetCanvasPixel(APP_STATE* app, int x, int y);
void App_SetCanvasPixel(APP_STATE* app, int x, int y, COLORREF color);
void App_DrawLine(APP_STATE* app, POINT start, POINT end, COLORREF color, int width);
void App_DrawSpray(APP_STATE* app, POINT center, COLORREF color);
void App_DrawShape(APP_STATE* app, TOOL_ID tool, POINT start, POINT end, COLORREF color);
void App_DrawTextBlock(APP_STATE* app, LPCTSTR text, const RECT* canvas_rect);
void App_FillAt(APP_STATE* app, POINT point, COLORREF color);
void App_InvalidateCanvasPixels(APP_STATE* app, int left, int top, int right, int bottom);
void App_InvalidateCanvasRect(APP_STATE* app, const RECT* canvas_rect);

RECT App_GetToolRect(const APP_STATE* app, TOOL_ID tool);
TOOL_ID App_HitTestToolbox(const APP_STATE* app, POINT point);
int App_HitTestPalette(const APP_STATE* app, POINT point);
void App_DoImmediateTool(APP_STATE* app, POINT canvas_point, int button);
int App_ToolUsesDrag(TOOL_ID tool);
int App_ToolUsesShapePreview(TOOL_ID tool);
void App_BeginDrag(APP_STATE* app, POINT canvas_point, int button);
void App_ContinueDrag(APP_STATE* app, POINT canvas_point);
void App_EndDrag(APP_STATE* app, POINT canvas_point);
HCURSOR App_GetToolCursor(const APP_STATE* app);

int App_TextUiVisible(const APP_STATE* app);
void App_InitTextState(APP_STATE* app);
void App_DestroyTextState(APP_STATE* app);
void App_ApplyTextFont(APP_STATE* app);
void App_UpdateTextPreviewBrush(APP_STATE* app);
void App_PositionTextEdit(APP_STATE* app);
int App_BeginTextEdit(APP_STATE* app, POINT canvas_point);
int App_CommitTextEdit(APP_STATE* app);
void App_CancelTextEdit(APP_STATE* app);
int App_ChooseTextFont(APP_STATE* app);
LRESULT App_HandleTextCtlColor(APP_STATE* app, HDC hdc, HWND child_window);

void App_DrawUi(HDC hdc, APP_STATE* app, const RECT* paint_rect);
void App_HandlePointerDown(APP_STATE* app, POINT point, int button);
void App_HandlePointerMove(APP_STATE* app, POINT point);
void App_HandlePointerUp(APP_STATE* app, POINT point);

int App_DoSave(APP_STATE* app, int force_prompt);
int App_ConfirmDiscardChanges(APP_STATE* app);
void App_HandleCommand(APP_STATE* app, int command_id);

#endif
