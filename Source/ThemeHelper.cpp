//========================================================================================
//
//  ThemeHelper.cpp
//
//========================================================================================
#include "IllustratorSDK.h"
#include "AssetCollectorSuites.h"
#include "ThemeHelper.h"

namespace
{
COLORREF gBackground = RGB(60, 60, 60);
COLORREF gText = RGB(220, 220, 220);
COLORREF gEditText = RGB(220, 220, 220);
COLORREF gEditBackground = RGB(45, 45, 45);
COLORREF gBorder = RGB(90, 90, 90);
COLORREF gSelection = RGB(80, 80, 80);
bool gIsDark = true;

HBRUSH gBackgroundBrush = nullptr;
HBRUSH gEditBackgroundBrush = nullptr;
HBRUSH gBorderBrush = nullptr;

inline int Clamp01(AIReal v)
{
    if (v < 0.0) return 0;
    if (v > 1.0) return 1;
    return (int)(v * 255.0 + 0.5);
}

void DeleteBrushes()
{
    if (gBackgroundBrush) { DeleteObject(gBackgroundBrush); gBackgroundBrush = nullptr; }
    if (gEditBackgroundBrush) { DeleteObject(gEditBackgroundBrush); gEditBackgroundBrush = nullptr; }
    if (gBorderBrush) { DeleteObject(gBorderBrush); gBorderBrush = nullptr; }
}
}

COLORREF ThemeHelper::GetComponentColor(int component)
{
    AIUIThemeColor color;
    if (sAIUITheme && sAIUITheme->GetUIThemeColor(kAIUIThemeSelectorPanel, component, color) == kNoErr)
        return RGB(Clamp01(color.red), Clamp01(color.green), Clamp01(color.blue));

    // Fallback: map unknown components to sensible defaults
    if (component == kAIUIComponentColorText)
        return gText;
    if (component == kAIUIComponentColorEditText)
        return gText;
    if (component == kAIUIComponentColorEditTextBackground)
        return gEditBackground;
    if (component == kAIUIComponentColorBorder)
        return gBorder;
    return gBackground;
}

void ThemeHelper::Refresh()
{
    gIsDark = sAIUITheme && sAIUITheme->IsUIThemeDark();

    gBackground = GetComponentColor(kAIUIComponentColorBackground);
    gText = GetComponentColor(kAIUIComponentColorText);
    gEditText = GetComponentColor(kAIUIComponentColorEditText);
    gEditBackground = GetComponentColor(kAIUIComponentColorEditTextBackground);
    gBorder = GetComponentColor(kAIUIComponentColorBorder);

    // Selection highlight: nudge the background toward white (dark theme) or
    // toward black (light theme).
    gSelection = gIsDark ? Blend(gBackground, RGB(255, 255, 255), 16)
                         : Blend(gBackground, RGB(0, 0, 0), 14);

    DeleteBrushes();
    gBackgroundBrush = CreateSolidBrush(gBackground);
    gEditBackgroundBrush = CreateSolidBrush(gEditBackground);
    gBorderBrush = CreateSolidBrush(gBorder);
}

COLORREF ThemeHelper::Background()
{
    return gBackground;
}

COLORREF ThemeHelper::Text()
{
    return gText;
}

COLORREF ThemeHelper::EditText()
{
    return gEditText;
}

COLORREF ThemeHelper::EditBackground()
{
    return gEditBackground;
}

COLORREF ThemeHelper::Border()
{
    return gBorder;
}

COLORREF ThemeHelper::SelectionBackground()
{
    return gSelection;
}

HBRUSH ThemeHelper::BackgroundBrush()
{
    return gBackgroundBrush;
}

HBRUSH ThemeHelper::EditBackgroundBrush()
{
    return gEditBackgroundBrush;
}

HBRUSH ThemeHelper::BorderBrush()
{
    return gBorderBrush;
}

COLORREF ThemeHelper::Blend(COLORREF base, COLORREF overlay, int percent)
{
    if (percent <= 0) return base;
    if (percent >= 100) return overlay;
    int r = (GetRValue(base) * (100 - percent) + GetRValue(overlay) * percent) / 100;
    int g = (GetGValue(base) * (100 - percent) + GetGValue(overlay) * percent) / 100;
    int b = (GetBValue(base) * (100 - percent) + GetBValue(overlay) * percent) / 100;
    return RGB(r, g, b);
}