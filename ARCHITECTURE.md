# Architecture Overview

This document describes the current architecture of `cepbrush`, a native Windows CE Paintbrush-style raster editor. It is based on the live code in this repository, not the upstream NT Paintbrush sources the assets came from.

## 1. Project Structure

```text
cepbrush/
├── ARCHITECTURE.md      # This document
├── README.md            # Project overview and feature summary
├── Makefile             # WinCE cross-build entry point
├── assets/
│   └── original/        # Original Paintbrush icon, toolbox bitmaps, and cursors
├── src/
│   ├── main.c           # Entire application implementation and event loop
│   ├── resource.h       # Resource identifiers for commands, bitmaps, and cursors
│   └── resources.rc     # Win32 resource script embedding menus and original assets
└── build outputs
    ├── cepbrush.exe     # Windows CE ARM executable
    ├── src/main.o
    └── src/resources.o
```

Notes:

- The codebase is intentionally small and currently centered around a single translation unit, [`src/main.c`](/home/segin/ce/cepbrush/src/main.c:1).
- `assets/original/` is treated as source material for resource embedding, not runtime-loaded loose assets.
- The checked-in executable and object files are build artifacts, not architectural components.

## 2. High-Level System Diagram

```text
[User]
  |
  v
[WinCE Desktop Window]
  |
  +--> [Menu Strip + Popup Menus]
  +--> [Toolbox Panel]
  +--> [Color Palette Panel]
  +--> [Scrollable Canvas Viewport]
            |
            +--> [32-bit in-memory canvas bitmap]
            +--> [single-level undo bitmap]
            +--> [BMP open/save to filesystem]
  |
  +--> [Embedded Win32 Resources]
            |
            +--> original icon
            +--> toolbox bitmaps
            +--> original cursor set
```

The program is a single-process desktop application. All state is held in memory inside one `APP_STATE` instance, and persistence is limited to reading and writing bitmap files through the local filesystem.

## 3. Core Components

### 3.1. Frontend / UI Shell

Name: CE Paintbrush desktop window

Description: The top-level WinCE window owns the layout, input dispatch, menu behavior, scrollbars, and painting lifecycle. It presents a desktop-style shell rather than a mobile command-bar layout.

Technologies: Win32/WinCE C API, GDI, standard window message loop

Deployment: Native ARM executable for Windows CE

Key implementation points:

- `WinMain` boots the app, registers the window class, loads the menu resource, and creates the main window.
- `App_WndProc` is the central message dispatcher handling create, paint, pointer, command, and scroll messages.
- `App_Layout` computes the menu strip, toolbox, palette, status bar, and canvas viewport rectangles.

### 3.2. Document / Canvas Engine

Name: In-memory raster document model

Description: The current image is stored as a 32-bit DIB section selected into an off-screen DC. All drawing tools mutate this bitmap directly. A second bitmap/DC pair stores one undo snapshot.

Technologies: `CreateDIBSection`, memory DCs, GDI drawing primitives, direct pixel access through `COLORREF*`

Deployment: In-process memory only

Key implementation points:

- `App_CreateCanvas` allocates the primary and undo DIBs.
- `App_CopyCanvasToUndo` and `App_RestoreUndo` implement single-level undo.
- `App_WindowToCanvas` maps viewport-relative pointer coordinates into scrolled canvas coordinates.
- `App_UpdateScrollBars`, `App_SetScrollPosition`, and `App_HandleScroll` keep the viewport aligned to the full image size.

### 3.3. Tool System

Name: Paint tool dispatcher

Description: Tool behavior is encoded as a compact enum-driven dispatch model. Hit-testing selects tools from the original toolbox art, and pointer input is routed either to immediate actions or drag-based tools.

Technologies: enums, static lookup tables, GDI drawing, flood-fill via heap-backed stack

Deployment: In-process

Current tool families:

- Immediate tools: fill, color picker, magnifier, text stamp
- Drag tools: pencil, brush, airbrush, eraser, line, curve placeholder, polygon placeholder, rectangle, ellipse, rounded rectangle
- Selection tools exist visually but are currently placeholders in behavior

Key implementation points:

- `g_tools` maps tool ids to display names and cursor resources.
- `App_HitTestToolbox` converts pointer position into a selected tool.
- `App_DoImmediateTool`, `App_BeginDrag`, `App_ContinueDrag`, and `App_EndDrag` drive tool execution.
- `App_DrawLine`, `App_DrawSpray`, `App_DrawShape`, `App_DrawTextStamp`, and `App_FillAt` implement drawing primitives.

### 3.4. Resource Layer

Name: Embedded legacy Paintbrush asset bundle

Description: The UI uses original Paintbrush visual resources embedded into the executable. This keeps the CE port visually tied to the legacy app while avoiding dependency on the old codebase.

Technologies: Win32 `.rc` resources, `LoadBitmap`, `LoadCursor`, `LoadIcon`

Deployment: Compiled into `cepbrush.exe`

Key implementation points:

- [`src/resources.rc`](/home/segin/ce/cepbrush/src/resources.rc:1) defines the menu resource and embeds the icon, toolbox bitmaps, arrow bitmap, and the full cursor set.
- [`src/resource.h`](/home/segin/ce/cepbrush/src/resource.h:1) defines command IDs and resource IDs.
- `App_LoadResources` loads the embedded UI assets into process memory at startup.

## 4. Data Stores

### 4.1. Primary Working Image

Name: Canvas DIB

Type: In-memory 32-bit bitmap

Purpose: Stores the active raster image being edited.

Key Structures:

- `APP_STATE.canvas_bitmap`
- `APP_STATE.canvas_dc`
- `APP_STATE.canvas_bits`

### 4.2. Undo Snapshot

Name: Undo DIB

Type: In-memory 32-bit bitmap

Purpose: Stores the previous image state for single-level undo.

Key Structures:

- `APP_STATE.undo_bitmap`
- `APP_STATE.undo_dc`
- `APP_STATE.undo_bits`

### 4.3. Persistent Storage

Name: BMP file on local filesystem

Type: Windows bitmap file

Purpose: Source and sink for `Open`, `Save`, and `Save As`.

Key implementation points:

- `App_LoadBitmapFile` imports bitmap files into the current canvas.
- `App_SaveBitmapFile` writes a 24-bit BMP using manual header and row serialization.

## 5. External Integrations / APIs

This project does not currently depend on network services or third-party online APIs.

External interfaces are limited to local platform APIs:

- Win32/WinCE windowing and GDI APIs for UI, input, resources, scrolling, and drawing
- Common dialog APIs for file picker and save dialog support
- Local filesystem APIs for BMP persistence

## 6. Deployment & Infrastructure

Cloud Provider: None

Target Platform: Windows CE on ARM

Build Environment:

- Cross-compiler: `/opt/arm-mingw32ce/bin/arm-mingw32ce-gcc`
- Resource compiler: `arm-mingw32ce-windres`
- Build entry point: `make`

Artifact:

- `cepbrush.exe` as a WinCE ARM executable

CI/CD Pipeline: None currently defined in this repository

Monitoring & Logging: None beyond user-visible dialogs and status text

## 7. Security Considerations

Authentication: None

Authorization: None

Data Encryption: None

Key Security Considerations:

- File operations are entirely local and user-driven through common dialogs.
- The app trusts selected BMP input files and performs only minimal format validation through Windows image loading.
- There is no sandboxing, privilege separation, or multi-user model inside the application.

## 8. Development & Testing Environment

Local Setup Instructions:

1. Ensure the ARM Windows CE cross-toolchain exists under `/opt/arm-mingw32ce/bin/`.
2. Run `make` from the repository root.
3. Deploy `cepbrush.exe` to the target CE device or emulator.

Testing Frameworks:

- No automated test suite yet
- Current validation is compile-time plus manual runtime verification on CE hardware or emulator

Code Quality Tools:

- Compiler warnings via `-Wall -Wextra -pedantic`

## 9. Future Considerations / Roadmap

- Split [`src/main.c`](/home/segin/ce/cepbrush/src/main.c:1) into focused modules such as `ui`, `canvas`, `tools`, `fileio`, and `resources`.
- Add true text editing, marquee/select workflows, polygon/curve completion, and richer brush configuration.
- Improve bitmap loading validation and error handling for malformed or oversized files.
- Introduce tests for BMP serialization/deserialization and pure drawing helpers that can be host-tested.
- Consider removing checked-in build artifacts from the source tree if this becomes a shared repository.

## 10. Project Identification

Project Name: CE Paintbrush

Repository URL: Local workspace only (`/home/segin/ce/cepbrush`)

Primary Contact/Team: Not specified in this repository

Date of Last Update: 2026-04-19

## 11. Glossary / Acronyms

CE: Windows CE

DIB: Device Independent Bitmap

DC: Device Context

GDI: Windows Graphics Device Interface

Viewport: The visible subsection of the larger canvas exposed through the scrollable canvas area
