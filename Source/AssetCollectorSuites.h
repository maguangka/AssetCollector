//========================================================================================
//
//  AssetCollectorSuites.h - Suite pointers shared across the plugin
//
//========================================================================================
#ifndef __AssetCollectorSuites_H__
#define __AssetCollectorSuites_H__

#include "ATETextSuitesImportHelper.h"
#include "AIFont.h"
#include "AIPlaced.h"
#include "AIPanel.h"
#include "AIUITheme.h"
#include "AIPreference.h"
#include "AIStringFormatUtils.h"
#include "AIUnicodeString.h"
#include "SPBlocks.h"
#include "AIDocumentList.h"

extern "C"
{
    extern AIArtSetSuite            *sAIArtSet;
    extern AITextFrameSuite         *sAITextFrame;
    extern AIFontSuite              *sAIFont;
    extern AIArtSuite               *sAIArt;
    extern AIPlacedSuite            *sAIPlaced;
    extern AIDocumentListSuite      *sAIDocumentList;
    extern AIDocumentSuite          *sAIDocument;
    extern AIPanelSuite             *sAIPanel;
    extern AIPanelFlyoutMenuSuite   *sAIPanelFlyoutMenu;
    extern AIMenuSuite              *sAIMenu;
    extern AIUIThemeSuite           *sAIUITheme;
    extern AIPreferenceSuite        *sAIPreference;
    extern AIStringFormatUtilsSuite *sAIStringFormatUtils;
    extern AIUnicodeStringSuite     *sAIUnicodeString;
    extern SPBlocksSuite            *sSPBlocks;
    extern ATE::ApplicationPaintSuite* sApplicationPaint;
    extern ATE::CompFontSuite* sCompFont;
    extern ATE::CompFontClassSuite* sCompFontClass;
    extern ATE::CompFontClassSetSuite* sCompFontClassSet;
    extern ATE::CompFontComponentSuite* sCompFontComponent;
    extern ATE::CompFontSetSuite* sCompFontSet;
    extern ATE::GlyphRunSuite* sGlyphRun;
    extern ATE::GlyphRunsIteratorSuite* sGlyphRunsIterator;
    extern ATE::ListStyleSuite* sListStyle;
    extern ATE::ListStyleSetSuite* sListStyleSet;
    extern ATE::MojiKumiSuite* sMojiKumi;
    extern ATE::MojiKumiSetSuite* sMojiKumiSet;
    extern ATE::TextFrameSuite* sTextFrame;
    extern ATE::TextFramesIteratorSuite* sTextFramesIterator;
    extern ATE::TextLineSuite* sTextLine;
    extern ATE::TextLinesIteratorSuite* sTextLinesIterator;
    extern ATE::TextResourcesSuite* sTextResources;
    extern ATE::ApplicationTextResourcesSuite* sApplicationTextResources;
    extern ATE::DocumentTextResourcesSuite* sDocumentTextResources;
    extern ATE::VersionInfoSuite* sVersionInfo;
    extern ATE::ArrayApplicationPaintRefSuite* sArrayApplicationPaintRef;
    extern ATE::ArrayRealSuite* sArrayReal;
    extern ATE::ArrayBoolSuite* sArrayBool;
    extern ATE::ArrayIntegerSuite* sArrayInteger;
    extern ATE::ArrayLineCapTypeSuite* sArrayLineCapType;
    extern ATE::ArrayFigureStyleSuite* sArrayFigureStyle;
    extern ATE::ArrayLineJoinTypeSuite* sArrayLineJoinType;
    extern ATE::ArrayWariChuJustificationSuite* sArrayWariChuJustification;
    extern ATE::ArrayStyleRunAlignmentSuite* sArrayStyleRunAlignment;
    extern ATE::ArrayAutoKernTypeSuite* sArrayAutoKernType;
    extern ATE::ArrayBaselineDirectionSuite* sArrayBaselineDirection;
    extern ATE::ArrayLanguageSuite* sArrayLanguage;
    extern ATE::ArrayFontCapsOptionSuite* sArrayFontCapsOption;
    extern ATE::ArrayFontBaselineOptionSuite* sArrayFontBaselineOption;
    extern ATE::ArrayFontOpenTypePositionOptionSuite* sArrayFontOpenTypePositionOption;
    extern ATE::ArrayUnderlinePositionSuite* sArrayUnderlinePosition;
    extern ATE::ArrayStrikethroughPositionSuite* sArrayStrikethroughPosition;
    extern ATE::ArrayParagraphJustificationSuite* sArrayParagraphJustification;
    extern ATE::ArrayArrayRealSuite* sArrayArrayReal;
    extern ATE::ArrayBurasagariTypeSuite* sArrayBurasagariType;
    extern ATE::ArrayPreferredKinsokuOrderSuite* sArrayPreferredKinsokuOrder;
    extern ATE::ArrayKinsokuRefSuite* sArrayKinsokuRef;
    extern ATE::ArrayListStyleRefSuite* sArrayListStyleRef;
    extern ATE::ArrayListStyleSetRefSuite* sArrayListStyleSetRef;
    extern ATE::ArrayMojiKumiRefSuite* sArrayMojiKumiRef;
    extern ATE::ArrayMojiKumiSetRefSuite* sArrayMojiKumiSetRef;
    extern ATE::ArrayTabStopsRefSuite* sArrayTabStopsRef;
    extern ATE::ArrayLeadingTypeSuite* sArrayLeadingType;
    extern ATE::ArrayFontRefSuite* sArrayFontRef;
    extern ATE::ArrayGlyphIDSuite* sArrayGlyphID;
    extern ATE::ArrayRealPointSuite* sArrayRealPoint;
    extern ATE::ArrayRealMatrixSuite* sArrayRealMatrix;
    extern ATE::CharFeaturesSuite* sCharFeatures;
    extern ATE::CharInspectorSuite* sCharInspector;
    extern ATE::CharStyleSuite* sCharStyle;
    extern ATE::CharStylesSuite* sCharStyles;
    extern ATE::CharStylesIteratorSuite* sCharStylesIterator;
    extern ATE::FindSuite* sFind;
    extern ATE::FontSuite* sFont;
    extern ATE::GlyphSuite* sGlyph;
    extern ATE::GlyphsSuite* sGlyphs;
    extern ATE::GlyphsIteratorSuite* sGlyphsIterator;
    extern ATE::KinsokuSuite* sKinsoku;
    extern ATE::KinsokuSetSuite* sKinsokuSet;
    extern ATE::ParaFeaturesSuite* sParaFeatures;
    extern ATE::ParagraphSuite* sParagraph;
    extern ATE::ParagraphsIteratorSuite* sParagraphsIterator;
    extern ATE::ParaInspectorSuite* sParaInspector;
    extern ATE::ParaStyleSuite* sParaStyle;
    extern ATE::ParaStylesSuite* sParaStyles;
    extern ATE::ParaStylesIteratorSuite* sParaStylesIterator;
    extern ATE::SpellSuite* sSpell;
    extern ATE::StoriesSuite* sStories;
    extern ATE::StorySuite* sStory;
    extern ATE::TabStopSuite* sTabStop;
    extern ATE::TabStopsSuite* sTabStops;
    extern ATE::TabStopsIteratorSuite* sTabStopsIterator;
    extern ATE::TextRangeSuite* sTextRange;
    extern ATE::TextRangesSuite* sTextRanges;
    extern ATE::TextRangesIteratorSuite* sTextRangesIterator;
    extern ATE::TextRunsIteratorSuite* sTextRunsIterator;
    extern ATE::WordsIteratorSuite* sWordsIterator;
    extern ATE::ArrayLineSuite* sArrayLine;
    extern ATE::ArrayComposerEngineSuite* sArrayComposerEngine;
}

#endif // __AssetCollectorSuites_H__