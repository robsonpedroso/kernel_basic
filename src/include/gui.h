#ifndef _GUI_H
#define _GUI_H

// Shared EGA/VGA 16-color palette indices for the Win3.11-style chrome
// (window.c, widget.c, kernel.c). Values match the standard VGA DAC
// default palette, so no custom palette programming is needed.
#define GUI_COLOR_BG          7   // light gray (window/button face)
#define GUI_COLOR_SHADOW      8   // dark gray (bevel shadow edge)
#define GUI_COLOR_LIGHT       15  // white (bevel highlight edge)
#define GUI_COLOR_BLACK       0
#define GUI_COLOR_TITLE       1   // blue (title bar face)
#define GUI_COLOR_TITLE_TEXT  15  // white
#define GUI_COLOR_DESKTOP     3   // cyan (desktop background)

#endif
