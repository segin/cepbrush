#include <string.h>

#include "app.h"

static LRESULT CALLBACK App_WndProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    APP_STATE* app;

    app = &g_app;

    switch (message) {
    case WM_CREATE:
        app->window = window;
        app->zoom = 1;
        app->active_menu_index = -1;
        app->foreground_color = RGB(0, 0, 0);
        app->background_color = RGB(255, 255, 255);
        app->spray_seed = 1;
        App_LoadResources(app);
        if (!App_CreateShellMenuBar(app)) {
            return -1;
        }
        if (app->app_icon_small) {
            SendMessage(window, WM_SETICON, ICON_SMALL, (LPARAM)app->app_icon_small);
        }
        if (app->app_icon_big) {
            SendMessage(window, WM_SETICON, ICON_BIG, (LPARAM)app->app_icon_big);
        }
        if (!App_CreateCanvas(app, DEFAULT_CANVAS_WIDTH, DEFAULT_CANVAS_HEIGHT)) {
            return -1;
        }
        App_CopyString(app->document_path, ARRAYSIZE(app->document_path), TEXT(""));
        App_CopyString(app->status_text, ARRAYSIZE(app->status_text), TEXT("Tool: Brush"));
        app->active_tool = TOOL_BRUSH;
        App_Layout(app);
        App_UpdateTitle(app);
        App_UpdateMenuState(app);
        return 0;
    case WM_SIZE:
        App_Layout(app);
        InvalidateRect(window, NULL, TRUE);
        return 0;
    case WM_HSCROLL:
        App_HandleScroll(app, SB_HORZ, LOWORD(w_param));
        return 0;
    case WM_VSCROLL:
        App_HandleScroll(app, SB_VERT, LOWORD(w_param));
        return 0;
    case WM_COMMAND:
        if (LOWORD(w_param) == ID_TEXT_EDIT_CONTROL) {
            return 0;
        }
        App_HandleCommand(app, LOWORD(w_param));
        return 0;
    case WM_INITMENUPOPUP:
        App_UpdateMenuState(app);
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        {
            LRESULT color_result;

            color_result = App_HandleTextCtlColor(app, (HDC)w_param, (HWND)l_param);
            if (color_result != 0) {
                return color_result;
            }
        }
        break;
    case WM_SYSKEYDOWN:
        if (w_param == VK_F4) {
            App_HandleCommand(app, ID_FILE_EXIT);
            return 0;
        }
        break;
    case WM_SETFOCUS:
        if (app->text_edit_window) {
            SetFocus(app->text_edit_window);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        {
            POINT point;

            point = App_PointFromLParam(l_param);
            App_HandlePointerDown(app, point, message == WM_RBUTTONDOWN ? MK_RBUTTON : MK_LBUTTON);
        }
        return 0;
    case WM_MOUSEMOVE:
        {
            POINT point;

            point = App_PointFromLParam(l_param);
            App_HandlePointerMove(app, point);
        }
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        {
            POINT point;

            point = App_PointFromLParam(l_param);
            App_HandlePointerUp(app, point);
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(l_param) == HTCLIENT) {
            POINT point;
            HCURSOR cursor;

            GetCursorPos(&point);
            ScreenToClient(window, &point);

            if (PtInRect(&app->canvas_rect, point)) {
                cursor = App_GetToolCursor(app);
                if (cursor) {
                    SetCursor(cursor);
                    return TRUE;
                }
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT paint_struct;
            HDC hdc;

            hdc = BeginPaint(window, &paint_struct);
            App_DrawUi(hdc, app, &paint_struct.rcPaint);
            EndPaint(window, &paint_struct);
        }
        return 0;
    case WM_CLOSE:
        if (app->text_edit_active && !App_CommitTextEdit(app)) {
            return 0;
        }
        if (!App_ConfirmDiscardChanges(app)) {
            return 0;
        }
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        App_DestroyCanvas(app);
        App_DestroyShellMenuBar(app);
        App_DestroyResources(app);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProc(window, message, w_param, l_param);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPWSTR command_line, int show_command) {
    WNDCLASS window_class;
    HWND window;
    MSG message;

    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    memset(&g_app, 0, sizeof(g_app));
    g_app.instance = instance;

    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = App_WndProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APP));
    window_class.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    window_class.lpszClassName = APP_CLASS_NAME;

    if (!RegisterClass(&window_class)) {
        return 1;
    }

    window = CreateWindow(
        APP_CLASS_NAME,
        APP_TITLE,
        WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX | WS_HSCROLL | WS_VSCROLL,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        680,
        NULL,
        NULL,
        instance,
        NULL
    );

    if (!window) {
        return 1;
    }

    ShowWindow(window, SW_SHOWMAXIMIZED);
    UpdateWindow(window);

    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return (int)message.wParam;
}
