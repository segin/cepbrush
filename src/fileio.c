#include <stdlib.h>
#include <string.h>

#include "app.h"

typedef struct CANVAS_SIZE_DIALOG_DATA_TAG {
    APP_STATE* app;
    int width;
    int height;
    int accepted;
    int done;
    HWND window;
    HWND width_edit;
    HWND height_edit;
} CANVAS_SIZE_DIALOG_DATA;

static WORD App_ReadLe16(const BYTE* bytes) {
    return (WORD)(bytes[0] | ((WORD)bytes[1] << 8));
}

static DWORD App_ReadLe32(const BYTE* bytes) {
    return (DWORD)(
        ((DWORD)bytes[0])
        | ((DWORD)bytes[1] << 8)
        | ((DWORD)bytes[2] << 16)
        | ((DWORD)bytes[3] << 24)
    );
}

static void App_WriteLe16(BYTE* bytes, WORD value) {
    bytes[0] = (BYTE)(value & 0xFF);
    bytes[1] = (BYTE)((value >> 8) & 0xFF);
}

static void App_WriteLe32(BYTE* bytes, DWORD value) {
    bytes[0] = (BYTE)(value & 0xFF);
    bytes[1] = (BYTE)((value >> 8) & 0xFF);
    bytes[2] = (BYTE)((value >> 16) & 0xFF);
    bytes[3] = (BYTE)((value >> 24) & 0xFF);
}

static int App_ShowFileDialog(HWND window, TCHAR* path, DWORD flags, int save_dialog) {
    OPENFILENAME dialog;
    static const TCHAR filter[] = TEXT("Bitmap Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0\0");

    memset(&dialog, 0, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = flags;
    dialog.lpstrDefExt = TEXT("bmp");

    if (save_dialog) {
        return GetSaveFileName(&dialog);
    }
    return GetOpenFileName(&dialog);
}

static int App_WriteFileChecked(HANDLE file, const void* buffer, DWORD size) {
    DWORD bytes_written;

    if (!WriteFile(file, buffer, size, &bytes_written, NULL)) {
        return 0;
    }

    return bytes_written == size;
}

static int App_ReadFileChecked(HANDLE file, void* buffer, DWORD size) {
    DWORD bytes_read;

    if (!ReadFile(file, buffer, size, &bytes_read, NULL)) {
        return 0;
    }

    return bytes_read == size;
}

static int App_SeekFileAbsolute(HANDLE file, DWORD offset) {
    DWORD result;

    SetLastError(NO_ERROR);
    result = SetFilePointer(file, (LONG)offset, NULL, FILE_BEGIN);
    if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        return 0;
    }
    return 1;
}

static int App_GetFileSizeChecked(HANDLE file, DWORD* size_out) {
    DWORD size;

    SetLastError(NO_ERROR);
    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
        return 0;
    }

    *size_out = size;
    return 1;
}

static int App_SaveBitmapFile(APP_STATE* app, LPCTSTR path) {
    HANDLE file;
    BYTE file_header[14];
    BYTE info_header[40];
    DWORD row_size;
    DWORD image_size;
    BYTE* row_buffer;
    int y;
    int success;

    row_size = (DWORD)(((app->canvas_width * 3) + 3) & ~3);
    image_size = row_size * (DWORD)app->canvas_height;

    file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBox(app->window, TEXT("Unable to create the bitmap file."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    memset(file_header, 0, sizeof(file_header));
    memset(info_header, 0, sizeof(info_header));

    App_WriteLe16(file_header, 0x4D42);
    App_WriteLe32(file_header + 2, 14u + 40u + image_size);
    App_WriteLe32(file_header + 10, 14u + 40u);

    App_WriteLe32(info_header, 40u);
    App_WriteLe32(info_header + 4, (DWORD)app->canvas_width);
    App_WriteLe32(info_header + 8, (DWORD)app->canvas_height);
    App_WriteLe16(info_header + 12, 1);
    App_WriteLe16(info_header + 14, 24);
    App_WriteLe32(info_header + 16, BI_RGB);
    App_WriteLe32(info_header + 20, image_size);

    row_buffer = (BYTE*)malloc(row_size);
    if (!row_buffer) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("Not enough memory to save the bitmap."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    success = App_WriteFileChecked(file, file_header, sizeof(file_header))
        && App_WriteFileChecked(file, info_header, sizeof(info_header));

    for (y = app->canvas_height - 1; success && y >= 0; --y) {
        int x;

        memset(row_buffer, 0, row_size);
        for (x = 0; x < app->canvas_width; ++x) {
            COLORREF color;

            color = app->canvas_bits[(y * app->canvas_width) + x];
            row_buffer[x * 3] = GetBValue(color);
            row_buffer[(x * 3) + 1] = GetGValue(color);
            row_buffer[(x * 3) + 2] = GetRValue(color);
        }
        success = App_WriteFileChecked(file, row_buffer, row_size);
    }

    free(row_buffer);
    CloseHandle(file);
    if (!success) {
        DeleteFile(path);
        MessageBox(app->window, TEXT("Unable to save the full bitmap file."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    App_MarkSaved(app, path);
    App_SetStatus(app, TEXT("Bitmap saved"));
    return 1;
}

static DWORD App_BitmapRowSize(int width, WORD bits_per_pixel) {
    DWORD bits_per_row;

    if (width <= 0 || bits_per_pixel == 0) {
        return 0;
    }
    bits_per_row = (DWORD)width * (DWORD)bits_per_pixel;
    if (bits_per_row / (DWORD)bits_per_pixel != (DWORD)width) {
        return 0;
    }
    return ((bits_per_row + 31u) / 32u) * 4u;
}

static BYTE App_ScaleMaskedComponent(DWORD pixel, DWORD mask) {
    DWORD component;
    DWORD max_value;
    int shift;

    if (mask == 0) {
        return 0;
    }

    shift = 0;
    while (((mask >> shift) & 1u) == 0u) {
        ++shift;
    }

    component = (pixel & mask) >> shift;
    max_value = mask >> shift;
    if (max_value == 0) {
        return 0;
    }

    return (BYTE)((component * 255u + (max_value / 2u)) / max_value);
}

static COLORREF App_DecodeBitfields(DWORD pixel, const DWORD masks[3]) {
    return RGB(
        App_ScaleMaskedComponent(pixel, masks[0]),
        App_ScaleMaskedComponent(pixel, masks[1]),
        App_ScaleMaskedComponent(pixel, masks[2])
    );
}

static int App_LoadBitmapPixels(
    HANDLE file,
    COLORREF* pixels,
    int width,
    int height,
    int top_down,
    WORD bits_per_pixel,
    DWORD compression,
    const COLORREF* palette,
    int palette_count,
    const DWORD bitfields[3]
) {
    BYTE* row_buffer;
    DWORD row_size;
    int row;

    row_size = App_BitmapRowSize(width, bits_per_pixel);
    row_buffer = (BYTE*)malloc(row_size);
    if (!row_buffer) {
        return 0;
    }

    for (row = 0; row < height; ++row) {
        int target_y;
        int x;

        if (!App_ReadFileChecked(file, row_buffer, row_size)) {
            free(row_buffer);
            return 0;
        }

        target_y = top_down ? row : (height - 1 - row);
        switch (bits_per_pixel) {
        case 32:
            if (compression == BI_BITFIELDS) {
                for (x = 0; x < width; ++x) {
                    DWORD pixel;

                    pixel = App_ReadLe32(row_buffer + (x * 4));
                    pixels[(target_y * width) + x] = App_DecodeBitfields(pixel, bitfields);
                }
            } else {
                for (x = 0; x < width; ++x) {
                    BYTE* pixel;

                    pixel = row_buffer + (x * 4);
                    pixels[(target_y * width) + x] = RGB(pixel[2], pixel[1], pixel[0]);
                }
            }
            break;
        case 16:
            {
                static const DWORD rgb555_masks[3] = { 0x7C00u, 0x03E0u, 0x001Fu };
                const DWORD* masks;

                masks = compression == BI_BITFIELDS ? bitfields : rgb555_masks;
                for (x = 0; x < width; ++x) {
                    DWORD pixel;

                    pixel = (DWORD)App_ReadLe16(row_buffer + (x * 2));
                    pixels[(target_y * width) + x] = App_DecodeBitfields(pixel, masks);
                }
            }
            break;
        case 24:
            for (x = 0; x < width; ++x) {
                BYTE* pixel;

                pixel = row_buffer + (x * 3);
                pixels[(target_y * width) + x] = RGB(pixel[2], pixel[1], pixel[0]);
            }
            break;
        case 8:
            for (x = 0; x < width; ++x) {
                BYTE index;

                index = row_buffer[x];
                if ((int)index >= palette_count) {
                    free(row_buffer);
                    return 0;
                }
                pixels[(target_y * width) + x] = palette[index];
            }
            break;
        case 4:
            for (x = 0; x < width; ++x) {
                BYTE packed;
                BYTE index;

                packed = row_buffer[x / 2];
                index = (BYTE)((x & 1) == 0 ? (packed >> 4) : (packed & 0x0F));
                if ((int)index >= palette_count) {
                    free(row_buffer);
                    return 0;
                }
                pixels[(target_y * width) + x] = palette[index];
            }
            break;
        case 1:
            for (x = 0; x < width; ++x) {
                BYTE packed;
                BYTE index;

                packed = row_buffer[x / 8];
                index = (BYTE)((packed >> (7 - (x & 7))) & 0x01);
                if ((int)index >= palette_count) {
                    free(row_buffer);
                    return 0;
                }
                pixels[(target_y * width) + x] = palette[index];
            }
            break;
        default:
            free(row_buffer);
            return 0;
        }
    }

    free(row_buffer);
    return 1;
}

static int App_LoadBitmapFile(APP_STATE* app, LPCTSTR path) {
    HANDLE file;
    BYTE file_header[14];
    BYTE* info_bytes;
    DWORD file_size;
    BYTE info_size_bytes[4];
    DWORD info_size;
    DWORD current_offset;
    DWORD bitfields[3];
    COLORREF palette[256];
    int palette_count;
    int palette_entry_size;
    LONG header_width;
    LONG header_height;
    WORD planes;
    WORD bits_per_pixel;
    DWORD compression;
    DWORD colors_used;
    int width;
    int height;
    int top_down;
    unsigned long pixel_count;
    COLORREF* pixels;
    int success;

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBox(app->window, TEXT("Unable to open this bitmap file."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    if (!App_GetFileSizeChecked(file, &file_size) || file_size < sizeof(file_header) + sizeof(DWORD)) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("This bitmap file is too small."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    memset(bitfields, 0, sizeof(bitfields));
    memset(palette, 0, sizeof(palette));
    success = App_ReadFileChecked(file, file_header, sizeof(file_header))
        && App_ReadFileChecked(file, info_size_bytes, sizeof(info_size_bytes));
    info_size = App_ReadLe32(info_size_bytes);
    if (!success || App_ReadLe16(file_header) != 0x4D42 || info_size < 12u) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("This bitmap header is not supported."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    if (info_size > file_size - (DWORD)sizeof(file_header)) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("The bitmap header is invalid."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }
    if (info_size != 12u && info_size < 40u) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("This bitmap header is not supported."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    info_bytes = (BYTE*)malloc(info_size - sizeof(info_size));
    if (!info_bytes) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("Not enough memory to read the bitmap header."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    if (!App_ReadFileChecked(file, info_bytes, info_size - sizeof(info_size))) {
        free(info_bytes);
        CloseHandle(file);
        MessageBox(app->window, TEXT("The bitmap file is truncated or unreadable."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }
    current_offset = sizeof(file_header) + info_size;

    if (info_size == 12u) {
        header_width = (LONG)App_ReadLe16(info_bytes);
        header_height = (LONG)App_ReadLe16(info_bytes + 2);
        planes = App_ReadLe16(info_bytes + 4);
        bits_per_pixel = App_ReadLe16(info_bytes + 6);
        compression = BI_RGB;
        colors_used = 0;
    } else {
        header_width = (LONG)App_ReadLe32(info_bytes);
        header_height = (LONG)App_ReadLe32(info_bytes + 4);
        planes = App_ReadLe16(info_bytes + 8);
        bits_per_pixel = App_ReadLe16(info_bytes + 10);
        compression = App_ReadLe32(info_bytes + 12);
        colors_used = App_ReadLe32(info_bytes + 28);

        if (info_size >= 52u && compression == BI_BITFIELDS) {
            bitfields[0] = App_ReadLe32(info_bytes + 36);
            bitfields[1] = App_ReadLe32(info_bytes + 40);
            bitfields[2] = App_ReadLe32(info_bytes + 44);
        } else if (info_size == 40u && compression == BI_BITFIELDS) {
            BYTE mask_bytes[12];

            if (!App_ReadFileChecked(file, mask_bytes, sizeof(mask_bytes))) {
                free(info_bytes);
                CloseHandle(file);
                MessageBox(app->window, TEXT("The bitmap color masks are truncated."), APP_TITLE, MB_OK | MB_ICONERROR);
                return 0;
            }
            bitfields[0] = App_ReadLe32(mask_bytes);
            bitfields[1] = App_ReadLe32(mask_bytes + 4);
            bitfields[2] = App_ReadLe32(mask_bytes + 8);
            current_offset += sizeof(mask_bytes);
        }
    }
    free(info_bytes);

    if (planes != 1) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("This bitmap header is not supported."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }
    if (compression != BI_RGB && compression != BI_BITFIELDS) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("Only uncompressed and bitfield BMP files are supported."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }
    if (bits_per_pixel != 1
        && bits_per_pixel != 4
        && bits_per_pixel != 8
        && bits_per_pixel != 16
        && bits_per_pixel != 24
        && bits_per_pixel != 32) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("Only 1, 4, 8, 16, 24, and 32-bit BMP files are supported."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }
    if (compression == BI_BITFIELDS && bits_per_pixel != 16 && bits_per_pixel != 32) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("This bitmap format is not supported."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    width = (int)header_width;
    top_down = header_height < 0;
    if (header_height == (LONG)0x80000000L) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("Bitmap dimensions are not supported on this device."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }
    height = (int)(top_down ? -header_height : header_height);
    if (width <= 0
        || height <= 0
        || width > APP_MAX_BITMAP_WIDTH
        || height > APP_MAX_BITMAP_HEIGHT) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("Bitmap dimensions are not supported on this device."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;
    if (pixel_count > APP_MAX_BITMAP_PIXELS) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("Bitmap dimensions are not supported on this device."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    palette_count = 0;
    palette_entry_size = info_size == 12u ? 3 : (int)sizeof(RGBQUAD);
    if (bits_per_pixel <= 8) {
        palette_count = colors_used ? (int)colors_used : (1 << bits_per_pixel);
        if (palette_count < 0 || palette_count > (int)ARRAYSIZE(palette)) {
            CloseHandle(file);
            MessageBox(app->window, TEXT("The bitmap palette is too large."), APP_TITLE, MB_OK | MB_ICONERROR);
            return 0;
        }
        if ((DWORD)palette_count > (file_size - current_offset) / (DWORD)palette_entry_size) {
            CloseHandle(file);
            MessageBox(app->window, TEXT("The bitmap palette is truncated or unreadable."), APP_TITLE, MB_OK | MB_ICONERROR);
            return 0;
        }
        if (palette_count > 0) {
            int palette_index;

            for (palette_index = 0; palette_index < palette_count; ++palette_index) {
                BYTE entry[4];

                memset(entry, 0, sizeof(entry));
                if (!App_ReadFileChecked(file, entry, (DWORD)palette_entry_size)) {
                    CloseHandle(file);
                    MessageBox(app->window, TEXT("The bitmap palette is truncated or unreadable."), APP_TITLE, MB_OK | MB_ICONERROR);
                    return 0;
                }
                palette[palette_index] = RGB(entry[2], entry[1], entry[0]);
            }
            current_offset += (DWORD)palette_count * (DWORD)palette_entry_size;
        }
    }

    if (App_ReadLe32(file_header + 10) < current_offset || App_ReadLe32(file_header + 10) >= file_size) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("The bitmap data offset is invalid."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }
    {
        DWORD data_offset;
        DWORD expected_data_size;

        data_offset = App_ReadLe32(file_header + 10);
        expected_data_size = App_BitmapRowSize(width, bits_per_pixel) * (DWORD)height;
        if (expected_data_size / (DWORD)height != App_BitmapRowSize(width, bits_per_pixel)) {
            CloseHandle(file);
            MessageBox(app->window, TEXT("The bitmap pixel data size overflows."), APP_TITLE, MB_OK | MB_ICONERROR);
            return 0;
        }
        if (file_size - data_offset < expected_data_size) {
            CloseHandle(file);
            MessageBox(app->window, TEXT("The bitmap file is truncated."), APP_TITLE, MB_OK | MB_ICONERROR);
            return 0;
        }
    }
    if (!App_SeekFileAbsolute(file, App_ReadLe32(file_header + 10))) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("The bitmap data offset is invalid."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    pixels = (COLORREF*)malloc((size_t)pixel_count * sizeof(COLORREF));
    if (!pixels) {
        CloseHandle(file);
        MessageBox(app->window, TEXT("Not enough memory to load the bitmap."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    success = App_LoadBitmapPixels(
        file,
        pixels,
        width,
        height,
        top_down,
        bits_per_pixel,
        compression,
        palette,
        palette_count,
        bitfields
    );
    CloseHandle(file);
    if (!success) {
        free(pixels);
        MessageBox(app->window, TEXT("The bitmap file is unsupported, truncated, or has invalid colors."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }

    if (!App_CreateCanvas(app, width, height)) {
        free(pixels);
        MessageBox(app->window, TEXT("Unable to create a canvas for this bitmap."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 0;
    }
    memcpy(app->canvas_bits, pixels, (size_t)pixel_count * sizeof(COLORREF));
    free(pixels);

    app->scroll_x = 0;
    app->scroll_y = 0;
    App_MarkSaved(app, path);
    App_Layout(app);
    App_SetStatus(app, TEXT("Bitmap loaded"));
    InvalidateRect(app->window, NULL, TRUE);
    return 1;
}

static LPCTSTR App_CanvasSizeClassName(void) {
    return TEXT("CEPaintbrushCanvasSizeDialog");
}

static LRESULT CALLBACK App_CanvasSizeWndProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    CANVAS_SIZE_DIALOG_DATA* data;

    data = (CANVAS_SIZE_DIALOG_DATA*)GetWindowLong(window, GWL_USERDATA);
    switch (message) {
    case WM_CREATE:
        {
            CREATESTRUCT* create_struct;
            HFONT font;

            create_struct = (CREATESTRUCT*)l_param;
            data = (CANVAS_SIZE_DIALOG_DATA*)create_struct->lpCreateParams;
            SetWindowLong(window, GWL_USERDATA, (LONG)data);
            data->window = window;
            font = (HFONT)GetStockObject(SYSTEM_FONT);

            SendMessage(
                CreateWindow(TEXT("STATIC"), TEXT("Width:"), WS_CHILD | WS_VISIBLE, 8, 10, 32, 14, window, NULL, data->app->instance, NULL),
                WM_SETFONT,
                (WPARAM)font,
                TRUE
            );
            data->width_edit = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
                46,
                8,
                54,
                16,
                window,
                (HMENU)IDC_CANVAS_WIDTH,
                data->app->instance,
                NULL
            );
            SendMessage(data->width_edit, WM_SETFONT, (WPARAM)font, TRUE);
            SetDlgItemInt(window, IDC_CANVAS_WIDTH, (UINT)data->width, FALSE);

            SendMessage(
                CreateWindow(TEXT("STATIC"), TEXT("Height:"), WS_CHILD | WS_VISIBLE, 8, 30, 32, 14, window, NULL, data->app->instance, NULL),
                WM_SETFONT,
                (WPARAM)font,
                TRUE
            );
            data->height_edit = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
                46,
                28,
                54,
                16,
                window,
                (HMENU)IDC_CANVAS_HEIGHT,
                data->app->instance,
                NULL
            );
            SendMessage(data->height_edit, WM_SETFONT, (WPARAM)font, TRUE);
            SetDlgItemInt(window, IDC_CANVAS_HEIGHT, (UINT)data->height, FALSE);

            SendMessage(
                CreateWindow(TEXT("BUTTON"), TEXT("OK"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 18, 50, 34, 16, window, (HMENU)IDOK, data->app->instance, NULL),
                WM_SETFONT,
                (WPARAM)font,
                TRUE
            );
            SendMessage(
                CreateWindow(TEXT("BUTTON"), TEXT("Cancel"), WS_CHILD | WS_VISIBLE | WS_TABSTOP, 58, 50, 42, 16, window, (HMENU)IDCANCEL, data->app->instance, NULL),
                WM_SETFONT,
                (WPARAM)font,
                TRUE
            );

            SetFocus(data->width_edit);
        }
        return 0;
    case WM_COMMAND:
        if (!data) {
            break;
        }
        switch (LOWORD(w_param)) {
        case IDOK:
            {
                BOOL width_ok;
                BOOL height_ok;
                UINT width_value;
                UINT height_value;

                width_value = GetDlgItemInt(window, IDC_CANVAS_WIDTH, &width_ok, FALSE);
                height_value = GetDlgItemInt(window, IDC_CANVAS_HEIGHT, &height_ok, FALSE);
                if (!width_ok || !height_ok
                    || width_value < 1
                    || height_value < 1
                    || width_value > APP_MAX_BITMAP_WIDTH
                    || height_value > APP_MAX_BITMAP_HEIGHT
                    || ((unsigned long)width_value * (unsigned long)height_value) > APP_MAX_BITMAP_PIXELS) {
                    MessageBox(window, TEXT("Enter a valid canvas size for this device."), APP_TITLE, MB_OK | MB_ICONERROR);
                    return 0;
                }

                data->width = (int)width_value;
                data->height = (int)height_value;
                data->accepted = 1;
                data->done = 1;
                DestroyWindow(window);
            }
            return 0;
        case IDCANCEL:
            data->done = 1;
            DestroyWindow(window);
            return 0;
        default:
            break;
        }
        break;
    case WM_CLOSE:
        if (data) {
            data->done = 1;
        }
        DestroyWindow(window);
        return 0;
    default:
        break;
    }

    return DefWindowProc(window, message, w_param, l_param);
}

static int App_EnsureCanvasSizeClass(APP_STATE* app) {
    static int class_registered = 0;

    if (!class_registered) {
        WNDCLASS window_class;

        memset(&window_class, 0, sizeof(window_class));
        window_class.lpfnWndProc = App_CanvasSizeWndProc;
        window_class.hInstance = app->instance;
        window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
        window_class.hIcon = app->app_icon_small;
        window_class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        window_class.lpszClassName = App_CanvasSizeClassName();

        if (!RegisterClass(&window_class)) {
            return 0;
        }
        class_registered = 1;
    }

    return 1;
}

static int App_ShowCanvasSizeDialog(APP_STATE* app, int* width, int* height) {
    CANVAS_SIZE_DIALOG_DATA data;
    RECT owner_rect;
    MSG message;

    if (!App_EnsureCanvasSizeClass(app)) {
        return 0;
    }

    memset(&data, 0, sizeof(data));
    data.app = app;
    data.width = *width;
    data.height = *height;

    GetWindowRect(app->window, &owner_rect);
    data.window = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        App_CanvasSizeClassName(),
        TEXT("Canvas Size"),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        owner_rect.left + 20,
        owner_rect.top + 20,
        116,
        94,
        app->window,
        NULL,
        app->instance,
        &data
    );
    if (!data.window) {
        return 0;
    }

    EnableWindow(app->window, FALSE);
    ShowWindow(data.window, SW_SHOW);
    UpdateWindow(data.window);

    while (!data.done && GetMessage(&message, NULL, 0, 0)) {
        if (!IsDialogMessage(data.window, &message)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }

    EnableWindow(app->window, TRUE);
    SetActiveWindow(app->window);
    if (!data.accepted) {
        return 0;
    }

    *width = data.width;
    *height = data.height;
    return 1;
}

int App_DoSave(APP_STATE* app, int force_prompt) {
    TCHAR path[MAX_PATH];

    App_CopyString(path, ARRAYSIZE(path), app->document_path);
    if (force_prompt || !path[0]) {
        path[0] = 0;
        if (!App_ShowFileDialog(app->window, path, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY, 1)) {
            return 0;
        }
    }

    return App_SaveBitmapFile(app, path);
}

int App_ConfirmDiscardChanges(APP_STATE* app) {
    int result;

    if (!app->dirty) {
        return 1;
    }

    result = MessageBox(
        app->window,
        TEXT("The image has changed.\n\nDo you want to save the current changes?"),
        APP_TITLE,
        MB_YESNOCANCEL | MB_ICONQUESTION
    );

    if (result == IDCANCEL) {
        return 0;
    }
    if (result == IDYES) {
        return App_DoSave(app, 0);
    }

    return 1;
}

void App_HandleCommand(APP_STATE* app, int command_id) {
    TCHAR path[MAX_PATH];

    if (app->text_edit_active) {
        if (!App_CommitTextEdit(app)) {
            return;
        }
    }

    switch (command_id) {
    case ID_FILE_NEW:
        if (App_ConfirmDiscardChanges(app)) {
            App_ResetDocument(app, DEFAULT_CANVAS_WIDTH, DEFAULT_CANVAS_HEIGHT);
        }
        break;
    case ID_FILE_OPEN:
        if (!App_ConfirmDiscardChanges(app)) {
            break;
        }
        path[0] = 0;
        if (App_ShowFileDialog(app->window, path, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY, 0)) {
            App_LoadBitmapFile(app, path);
        }
        break;
    case ID_FILE_SAVE:
        App_DoSave(app, 0);
        break;
    case ID_FILE_SAVE_AS:
        App_DoSave(app, 1);
        break;
    case ID_FILE_ATTRIBUTES:
        {
            int width;
            int height;

            width = app->canvas_width;
            height = app->canvas_height;
            if (!App_ShowCanvasSizeDialog(app, &width, &height)) {
                break;
            }
            if (width == app->canvas_width && height == app->canvas_height) {
                break;
            }
            if (!App_ResizeCanvas(app, width, height)) {
                MessageBox(app->window, TEXT("Unable to resize the canvas."), APP_TITLE, MB_OK | MB_ICONERROR);
                break;
            }
            app->scroll_x = 0;
            app->scroll_y = 0;
            App_MarkDirty(app);
            App_Layout(app);
            App_SetStatus(app, TEXT("Canvas resized"));
            InvalidateRect(app->window, NULL, TRUE);
        }
        break;
    case ID_FILE_EXIT:
        SendMessage(app->window, WM_CLOSE, 0, 0);
        break;
    case ID_EDIT_UNDO:
        if (app->has_undo) {
            App_RestoreUndo(app);
            App_ConsumeUndo(app);
            App_MarkDirty(app);
            App_SetStatus(app, TEXT("Undo"));
            InvalidateRect(app->window, &app->canvas_frame_rect, FALSE);
        }
        break;
    case ID_EDIT_CLEAR:
        App_CopyCanvasToUndo(app);
        {
            RECT fill_rect;
            HBRUSH white_brush;

            SetRect(&fill_rect, 0, 0, app->canvas_width, app->canvas_height);
            white_brush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(app->canvas_dc, &fill_rect, white_brush);
            DeleteObject(white_brush);
        }
        App_MarkDirty(app);
        App_SetStatus(app, TEXT("Image cleared"));
        InvalidateRect(app->window, &app->canvas_frame_rect, FALSE);
        break;
    case ID_VIEW_COLOR_TOOLBOX:
        app->use_bw_toolbox = 0;
        App_UpdateMenuState(app);
        InvalidateRect(app->window, &app->toolbox_rect, FALSE);
        break;
    case ID_VIEW_BW_TOOLBOX:
        app->use_bw_toolbox = 1;
        App_UpdateMenuState(app);
        InvalidateRect(app->window, &app->toolbox_rect, FALSE);
        break;
    case ID_VIEW_ZOOM_1X:
        App_SetZoom(app, 1);
        break;
    case ID_VIEW_ZOOM_2X:
        App_SetZoom(app, 2);
        break;
    case ID_VIEW_ZOOM_4X:
        App_SetZoom(app, 4);
        break;
    case ID_VIEW_ZOOM_8X:
        App_SetZoom(app, 8);
        break;
    case ID_VIEW_PIXEL_GRID:
        app->show_pixel_grid = !app->show_pixel_grid;
        App_UpdateMenuState(app);
        InvalidateRect(app->window, &app->canvas_area_rect, FALSE);
        break;
    case ID_HELP_ABOUT:
        MessageBox(
            app->window,
            TEXT("CE Paintbrush\n\nDesktop-style Windows CE paint app using the original Paintbrush icon, toolbox bitmaps, and cursor set."),
            APP_TITLE,
            MB_OK | MB_ICONINFORMATION
        );
        break;
    default:
        break;
    }

    App_UpdateMenuState(app);
}
