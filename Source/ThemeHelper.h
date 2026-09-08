//========================================================================================
//
//  ThemeHelper.h - Reads the current Adobe UI theme colors (light/dark) and exposes
//  them as GDI colors and brushes so that the panel controls adapt to the
//  Illustrator theme automatically.
//
//========================================================================================
#ifndef __ThemeHelper_H__
#define __ThemeHelper_H__

#ifdef WIN_ENV
#include <Windows.h>
#endif

class ThemeHelper
{
public:
    // Re-queries the theme colors and rebuilds cached brushes.
    static void Refresh();

    static COLORREF Background();
    static COLORREF Text();
    static COLORREF EditText();
    static COLORREF EditBackground();
    static COLORREF Border();
    // Background color used for the selected row of the font list.
    static COLORREF SelectionBackground();

    static HBRUSH BackgroundBrush();
    static HBRUSH EditBackgroundBrush();
    static HBRUSH BorderBrush();

    // Blends two colors: percent 0..100, 0 = fully base, 100 = fully overlay.
    static COLORREF Blend(COLORREF base, COLORREF overlay, int percent);

private:
    static COLORREF GetComponentColor(int component);
};

#endif // __ThemeHelper_H__