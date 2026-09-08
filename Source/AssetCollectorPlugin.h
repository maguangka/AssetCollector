//========================================================================================
//
//  AssetCollectorPlugin.h - Main plug-in class
//
//========================================================================================
#ifndef __AssetCollectorPlugin_H__
#define __AssetCollectorPlugin_H__

#include "IllustratorSDK.h"
#include "Plugin.hpp"
#include "AIFont.h"
#include "AIPlaced.h"
#include "AIPanel.h"
#include "AIDocumentList.h"
#include "AIUITheme.h"
#include "SDKDef.h"
#include "SDKAboutPluginsHelper.h"

#ifdef WIN_ENV
#include <Windows.h>
#include <string>
#include <vector>
#endif

// Result of a copy / pack operation.
struct CopyResult
{
    int  ok = 0;        // files copied successfully
    int  skipped = 0;   // files skipped (identical content already present)
    int  failed = 0;    // files that failed to copy
    int  renamed = 0;   // files copied under a different (renamed) file name
    int  deduped = 0;   // files identical to one already collected, not copied
    bool needSelect = false;   // selected mode, nothing of this kind checked
    bool nothing = false;      // all mode, document has nothing of this kind
    bool fatal = false;        // destination problem, already reported

    // Human-readable notes about renamed files, shown in the result dialog.
    std::vector<std::wstring> renameNotes;
    // One text line per copied / skipped / renamed / deduped file for the log.
    std::vector<std::wstring> logLines;
};

//========================================================================================
// Plugin class
//========================================================================================
class AssetCollectorPlugin : public Plugin
{
public:
    AssetCollectorPlugin(SPPluginRef pluginRef);
    ~AssetCollectorPlugin();

    FIXUP_VTABLE_EX(AssetCollectorPlugin, Plugin);

    ASErr StartupPlugin(SPInterfaceMessage* message) override;
    ASErr PreShutdownPlugin() override;
    ASErr ShutdownPlugin(SPInterfaceMessage* message) override;
    ASErr GoMenuItem(AIMenuMessage* message) override;
    ASErr Notify(AINotifierMessage* message) override;

#ifdef WIN_ENV

private:
    // One asset used by the document: a font or a linked file.
    struct AssetEntry
    {
        enum Kind { Font, Link };
        Kind            kind;
        AIFontKey       fontKey;                    // Font only
        ai::UnicodeString name;                     // font family name / linked file name
        ai::UnicodeString sub;                      // font style name / link format
        std::vector<ai::UnicodeString> filePaths;   // font files / single link path
        ai::UnicodeString location;                 // parent directory
    };

    AIPanelRef           fPanel;
    AIPanelFlyoutMenuRef fFlyoutMenu;
    AIMenuItemHandle     fPanelMenuItem;
    AIMenuItemHandle     fAboutPluginMenu;
    AINotifierHandle     fBrightnessNotifier;
    AINotifierHandle     fRefreshNotifier;

    HWND                 hDlg;
    WNDPROC              fDefaultWindProc;

    HWND                 hLabelDest;
    HWND                 hEditDest;
    HWND                 hBtnBrowse;
    HWND                 hBtnRefresh;
    HWND                 hBtnCopyFonts;
    HWND                 hBtnCopyLinks;
    HWND                 hBtnPackAll;
    HWND                 hBtnPackSel;
    HWND                 hList;
    HWND                 hStatus;
    HWND                 hHdrName;
    HWND                 hHdrType;
    HWND                 hHdrFile;
    HWND                 hHdrLoc;
    HWND                 hHdrChk;

    HWND                 fHoverButton;
    HFONT                fUIFont;
    HFONT                fRowFont;
    int                  fLastListWidth;

    std::vector<AssetEntry> fAssets;
    std::vector<bool>    fChecked;
    ai::UnicodeString    fDestFolder;

    // Window procedure and panel callbacks
    static LRESULT CALLBACK NewWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool PanelWindowProc(LRESULT& result, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT CallDefaultWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void PanelFlyoutMenuProc(AIPanelRef inPanel, ai::uint32 itemID);
    static void PanelVisibilityChangedNotifyProc(AIPanelRef inPanel, AIBoolean isVisible);
    static void PanelSizeChangedNotifyProc(AIPanelRef inPanel);
    static void PanelClosedNotifyProc(AIPanelRef inPanel);

    // UI construction
    void CreateControls();
    void CreateThemedButton(HWND parent, int ctrlID, const wchar_t* label,
                            int x, int y, int w, int h);
    void CreateThemedStatic(HWND parent, int ctrlID, const wchar_t* label,
                            int x, int y, int w, int h);
    void LayoutControls();
    void ApplyTheme();
    void DrawThemeButton(HDC hdc, const RECT& rc, HWND hwndBtn, UINT itemState);

    // Status / destination folder helpers
    void UpdateStatus(const ai::UnicodeString& text);
    void LoadDestFolderPref();
    void SaveDestFolderPref();
    ai::UnicodeString GetDestFolderFromEdit();
    void SetDestFolderEdit(const ai::UnicodeString& folder);

    // Asset discovery
    ai::UnicodeString GetDocumentName();
    ai::UnicodeString GetDocumentFilePath();
    AIErr CollectDocumentAssets();
    void AddFontFromFrame(AIArtHandle frame);
    void AddFontEntry(AIFontKey fontKey);
    void AddLinkEntry(const ai::UnicodeString& filePath);

    // Refresh / list population
    void RefreshAssets();
    void ClearListView();
    void PopulateListView();
    void RepopulateListView();
    ai::UnicodeString Ellipsize(const ai::UnicodeString& text, int maxWidth, HDC hdc);
    ai::UnicodeString SanitizeFileName(const ai::UnicodeString& name);

    // Folder browsing / copying
    void BrowseFolder();
    void CopyFontEntries();
    void CopyLinkEntries();
    void PackAll();
    void PackSelected();
    std::wstring GetTargetBase();
    CopyResult CopyToSubdir(AssetEntry::Kind kind, const std::wstring& subdir, bool selectedOnly);
    void ShowCopyResult(const ai::UnicodeString& kindName, const CopyResult& r, const std::wstring& subdir);
    void ShowPackResult(const CopyResult& rf, const CopyResult& rl, const CopyResult& rd);
    void WriteCollectLog(const std::wstring& base, const CopyResult& rf,
                         const CopyResult& rl, const CopyResult& rd);
#endif
};

#endif // __AssetCollectorPlugin_H__