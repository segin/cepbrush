# CE Paintbrush

`cepbrush` is a desktop-style Windows CE paint application that preserves the original Microsoft Paintbrush graphics from the legacy NT source tree while using a fresh CE-native implementation.

The original icon, toolbox bitmaps, and cursor set live in [`assets/original`](./assets/original). They are embedded directly into the CE binary through [`src/resources.rc`](./src/resources.rc).

Current port features:

- desktop-oriented layout with a fixed toolbox, color palette, status bar, and central canvas
- classic Paintbrush visual assets for the toolbox and cursors
- drawing tools for pencil, brush, airbrush, line, rectangle, rounded rectangle, ellipse, eraser, fill, color picker, magnifier, and text stamp
- `New`, `Open`, `Save`, `Save As`, and single-level `Undo`

This is a CE port inspired by the original app, not a direct build of the NT-era codebase.
