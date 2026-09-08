//========================================================================================
//
//  AssetCollectorSuites.cpp - Suite acquisition tables
//
//========================================================================================
#include "IllustratorSDK.h"
#include "AssetCollectorSuites.h"
#include "Suites.hpp"

extern "C"
{
    AIArtSetSuite            *sAIArtSet;
    AITextFrameSuite         *sAITextFrame;
    AIFontSuite              *sAIFont;
    AIArtSuite               *sAIArt;
    AIPlacedSuite            *sAIPlaced;
    AIDocumentListSuite      *sAIDocumentList;
    AIDocumentSuite          *sAIDocument;
    AIPanelSuite             *sAIPanel;
    AIPanelFlyoutMenuSuite   *sAIPanelFlyoutMenu;
    AIMenuSuite              *sAIMenu;
    AIUIThemeSuite           *sAIUITheme;
    AIPreferenceSuite        *sAIPreference;
    AIStringFormatUtilsSuite *sAIStringFormatUtils;
    AIUnicodeStringSuite     *sAIUnicodeString;
    SPBlocksSuite            *sSPBlocks;
    EXTERN_TEXT_SUITES
}

ImportSuite gImportSuites[] =
{
    // ---- Required suites: these must be available in every supported host.
    // If any of these cannot be acquired the plug-in refuses to load.
    kAIArtSetSuite, kAIArtSetSuiteVersion, &sAIArtSet,
    kAIArtSuite, kAIArtSuiteVersion, &sAIArt,
    kAIDocumentListSuite, kAIDocumentListSuiteVersion, &sAIDocumentList,
    kAIDocumentSuite, kAIDocumentVersion, &sAIDocument,
    kAIMenuSuite, kAIMenuSuiteVersion, &sAIMenu,
    kAIStringFormatUtilsSuite, kAIStringFormatUtilsSuiteVersion, &sAIStringFormatUtils,
    kAIUnicodeStringSuite, kAIUnicodeStringSuiteVersion, &sAIUnicodeString,

    // ---- Optional suites: these may be missing or have a different version
    // in older hosts. Acquisition failures here must not prevent the plug-in
    // from loading. Guard every use with a null check.
    { nullptr, kStartOptionalSuites, nullptr },

    kAITextFrameSuite, kAITextFrameSuiteVersion, &sAITextFrame,
    kAIFontSuite, kAIFontSuiteVersion, &sAIFont,
    kAIPlacedSuite, kAIPlacedSuiteVersion, &sAIPlaced,
    kAIPanelSuite, kAIPanelSuiteVersion, &sAIPanel,
    kAIPanelFlyoutMenuSuite, kAIPanelFlyoutMenuSuiteVersion, &sAIPanelFlyoutMenu,
    kAIUIThemeSuite, kAIUIThemeSuiteVersion, &sAIUITheme,
    kAIPreferenceSuite, kAIPreferenceSuiteVersion, &sAIPreference,
    kSPBlocksSuite, kSPBlocksSuiteVersion, &sSPBlocks,
    IMPORT_TEXT_SUITES

    nullptr, 0, nullptr
};