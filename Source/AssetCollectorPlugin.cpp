//========================================================================================
//
//  AssetCollectorPlugin.cpp - Main plug-in implementation
//
//  Merged font + link collector:
//    - Lists every font (family / style / file) and every linked file used by the
//      current document in one list with a type column
//    - Copies font files into "<dest>\<document name>\fonts" and linked files into
//      "<dest>\<document name>\links"; "pack all" also copies the document itself
//    - Actions: refresh resources, pack all fonts, pack all links, pack selected,
//      pack all (fonts + links + document)
//    - Native SDK panel (AIPanelSuite) that follows the Illustrator UI theme and
//      adapts its layout to the panel width
//
//========================================================================================

#include "IllustratorSDK.h"

#ifdef WIN_ENV
#include <WindowsX.h>
#include <CommCtrl.h>
#include <objbase.h>
#include <ShlObj.h>
#include <shobjidl.h>
#include <wctype.h>
#include <wincrypt.h>
#include "IAIUnicodeString.h"
#endif

#pragma comment(lib, "advapi32.lib")

#include "AssetCollectorPlugin.h"
#include "AssetCollectorSuites.h"
#include "AssetCollectorID.h"
#include "ThemeHelper.h"

using namespace std;

#define AC_PANEL_PROP L"ACPlugin"
#define AC_REFRESH_NOTIFIER_TYPE "AssetCollector Refresh"

//========================================================================================
// Entry points (the common Main.cpp supplies PluginMain and calls these)
//========================================================================================
Plugin* AllocatePlugin(SPPluginRef pluginRef)
{
    return new AssetCollectorPlugin(pluginRef);
}

void FixupReload(Plugin* plugin)
{
    AssetCollectorPlugin::FixupVTable(static_cast<AssetCollectorPlugin*>(plugin));
}

//========================================================================================
// Constructor / destructor
//========================================================================================
AssetCollectorPlugin::AssetCollectorPlugin(SPPluginRef pluginRef)
    : Plugin(pluginRef)
{
    strncpy(fPluginName, kAssetCollectorPluginName, kMaxStringLength);

    fPanel = nullptr;
    fFlyoutMenu = nullptr;
    fPanelMenuItem = nullptr;
    fAboutPluginMenu = nullptr;
    fBrightnessNotifier = nullptr;
    fRefreshNotifier = nullptr;

    hDlg = nullptr;
    fDefaultWindProc = nullptr;
    hLabelDest = nullptr;
    hEditDest = nullptr;
    hBtnBrowse = nullptr;
    hBtnRefresh = nullptr;
    hBtnCopyFonts = nullptr;
    hBtnCopyLinks = nullptr;
    hBtnPackAll = nullptr;
    hBtnPackSel = nullptr;
    hList = nullptr;
    hStatus = nullptr;
    hHdrName = nullptr;
    hHdrType = nullptr;
    hHdrFile = nullptr;
    hHdrLoc = nullptr;
    hHdrChk = nullptr;
    fHoverButton = nullptr;
    fUIFont = nullptr;
    fRowFont = nullptr;
    fLastListWidth = 0;
}

AssetCollectorPlugin::~AssetCollectorPlugin()
{
}

//========================================================================================
// Plugin lifecycle
//========================================================================================
ASErr AssetCollectorPlugin::StartupPlugin(SPInterfaceMessage* message)
{
    AIErr error = Plugin::StartupPlugin(message);
    if (error != kNoErr)
        return error;

    SDKAboutPluginsHelper aboutPluginsHelper;
    error = aboutPluginsHelper.AddAboutPluginsMenuItem(
        message,
        kSDKDefAboutSDKCompanyPluginsGroupName,
        ai::UnicodeString(kSDKDefAboutSDKCompanyPluginsGroupNameString),
        "AssetCollector...",
        &fAboutPluginMenu);
    if (error != kNoErr)
        return error;

    // Menu item to show/hide the panel
    error = sAIMenu->AddMenuItemZString(
        fPluginRef,
        "AssetCollector...",
        kOtherPalettesMenuGroup,
        ZREF("AssetCollector..."),
        kMenuItemNoOptions,
        &fPanelMenuItem);
    if (error != kNoErr)
        return error;

    // Panel flyout menu (optional suite: guard against older hosts)
    if (sAIPanelFlyoutMenu)
    {
        error = sAIPanelFlyoutMenu->Create(fFlyoutMenu);
        if (error != kNoErr)
            return error;
        error = sAIPanelFlyoutMenu->AppendItem(fFlyoutMenu, kACFlyoutRefresh, ai::UnicodeString(L"刷新资源列表"));
        if (error != kNoErr)
            return error;
        error = sAIPanelFlyoutMenu->AppendItem(fFlyoutMenu, kACFlyoutPackAll, ai::UnicodeString(L"打包所有"));
        if (error != kNoErr)
            return error;
        error = sAIPanelFlyoutMenu->AppendItem(fFlyoutMenu, kACFlyoutPackSel, ai::UnicodeString(L"打包已选"));
        if (error != kNoErr)
            return error;
        error = sAIPanelFlyoutMenu->AppendItem(fFlyoutMenu, kACFlyoutAbout, ai::UnicodeString(L"关于 AssetCollector"));
        if (error != kNoErr)
            return error;
    }

    // Create the panel (dockable). The panel suite is optional: on hosts that
    // do not provide it we still load the plug-in with the menu items intact.
    if (sAIPanel)
    {
        AISize pnSize = { 360, 520 };
        error = sAIPanel->Create(
            fPluginRef,
            ai::UnicodeString("AssetCollector"),
            ai::UnicodeString("AssetCollector"),
            1,
            pnSize,
            true,
            fFlyoutMenu,
            this,
            fPanel);
        if (error != kNoErr)
            return error;

        AISize minSize = { 320, 400 };
        AISize prefConstSize = { 360, 500 };
        AISize prefUnconstSize = { 380, 560 };
        AISize maxSize = { 1400, 1400 };
        AIErr sizeErr = sAIPanel->SetSizes(fPanel, minSize, prefUnconstSize, prefConstSize, maxSize);
        if (sizeErr != kNoErr)
        {
            sAIPanel->SetMinimumSize(fPanel, minSize);
            sAIPanel->SetMaximumSize(fPanel, maxSize);
            sAIPanel->SetPreferredSizes(fPanel, prefConstSize);
        }

        sAIPanel->SetFlyoutMenuProc(fPanel, PanelFlyoutMenuProc);
        sAIPanel->SetVisibilityChangedNotifyProc(fPanel, PanelVisibilityChangedNotifyProc);
        sAIPanel->SetSizeChangedNotifyProc(fPanel, PanelSizeChangedNotifyProc);
        sAIPanel->SetClosedNotifyProc(fPanel, PanelClosedNotifyProc);
        sAIPanel->SetSVGIconResourceID(fPanel, 16200, 16200);

        error = sAIPanel->GetPlatformWindow(fPanel, hDlg);
        if (error != kNoErr)
            return error;

        // Subclass the panel window so we can handle WM_SIZE / theme painting etc.
        ::SetPropW(hDlg, AC_PANEL_PROP, this);
        fDefaultWindProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hDlg, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(AssetCollectorPlugin::NewWindowProc)));

        ThemeHelper::Refresh();
        CreateControls();
        LoadDestFolderPref();

        error = sAINotifier->AddNotifier(
            fPluginRef,
            kAIUIBrightnessChangedNotifier,
            kAIUIBrightnessChangedNotifier,
            &fBrightnessNotifier);
        if (error != kNoErr)
            return error;

        // Custom notifier: refresh requests coming from the panel's Windows
        // message loop are re-dispatched through AI, so the refresh runs in a
        // proper document context (MatchingArtSet needs the current document).
        error = sAINotifier->AddNotifier(
            fPluginRef,
            AC_REFRESH_NOTIFIER_TYPE,
            AC_REFRESH_NOTIFIER_TYPE,
            &fRefreshNotifier);
        if (error != kNoErr)
            return error;

        error = sAIPanel->Show(fPanel, true);
        return error;
    }

    return kNoErr;
}

ASErr AssetCollectorPlugin::PreShutdownPlugin()
{
    if (fBrightnessNotifier)
    {
        sAINotifier->SetNotifierActive(fBrightnessNotifier, false);
        fBrightnessNotifier = nullptr;
    }

    if (fRefreshNotifier)
    {
        sAINotifier->SetNotifierActive(fRefreshNotifier, false);
        fRefreshNotifier = nullptr;
    }

    if (hDlg)
    {
        if (fDefaultWindProc)
            SetWindowLongPtrW(hDlg, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(fDefaultWindProc));
        RemovePropW(hDlg, AC_PANEL_PROP);
    }

    if (fPanel)
    {
        sAIPanel->Destroy(fPanel);
        fPanel = nullptr;
    }

    if (hDlg)
    {
        DestroyWindow(hDlg);
        hDlg = nullptr;
    }

    if (fFlyoutMenu)
    {
        sAIPanelFlyoutMenu->Destroy(fFlyoutMenu);
        fFlyoutMenu = nullptr;
    }

    if (fUIFont)
    {
        DeleteObject(fUIFont);
        fUIFont = nullptr;
    }

    if (fRowFont)
    {
        DeleteObject(fRowFont);
        fRowFont = nullptr;
    }

    return kNoErr;
}

ASErr AssetCollectorPlugin::ShutdownPlugin(SPInterfaceMessage* message)
{
    return kNoErr;
}

ASErr AssetCollectorPlugin::GoMenuItem(AIMenuMessage* message)
{
    if (message->menuItem == fAboutPluginMenu)
    {
        SDKAboutPluginsHelper aboutPluginsHelper;
        aboutPluginsHelper.PopAboutBox(
            message,
            "About AssetCollector",
            kSDKDefAboutSDKCompanyPluginsAlertString);
    }
    else if (message->menuItem == fPanelMenuItem && fPanel)
    {
        AIBoolean shown = false;
        sAIPanel->IsShown(fPanel, shown);
        sAIPanel->Show(fPanel, !shown);
    }
    return kNoErr;
}

ASErr AssetCollectorPlugin::Notify(AINotifierMessage* message)
{
    if (!message)
        return kNoErr;

    if (message->notifier == fBrightnessNotifier)
    {
        ThemeHelper::Refresh();
        ApplyTheme();
    }
    else if (message->notifier == fRefreshNotifier)
    {
        RefreshAssets();
    }
    return kNoErr;
}

//========================================================================================
// Panel callbacks
//========================================================================================
void AssetCollectorPlugin::PanelFlyoutMenuProc(AIPanelRef inPanel, ai::uint32 itemID)
{
    if (!sAIPanel)
        return;

    AIPanelUserData userData = nullptr;
    sAIPanel->GetUserData(inPanel, userData);
    AssetCollectorPlugin* plugin = reinterpret_cast<AssetCollectorPlugin*>(userData);
    if (!plugin)
        return;

    switch (itemID)
    {
        case kACFlyoutRefresh:
            if (sAINotifier)
                sAINotifier->Notify(AC_REFRESH_NOTIFIER_TYPE, nullptr);
            break;
        case kACFlyoutPackAll:
            plugin->PackAll();
            break;
        case kACFlyoutPackSel:
            plugin->PackSelected();
            break;
        case kACFlyoutAbout:
            if (sAIUser)
                sAIUser->MessageAlert(ai::UnicodeString(
                    L"AssetCollector 1.0 Illustrator 原生资源收集插件，"
                    L"列出当前文档使用的字体与外链文件，并复制到目标文件夹的文档名下。BY MAGK"));
            break;
    }
}

void AssetCollectorPlugin::PanelVisibilityChangedNotifyProc(AIPanelRef inPanel, AIBoolean isVisible)
{
    if (!sAIPanel)
        return;

    AIPanelUserData userData = nullptr;
    sAIPanel->GetUserData(inPanel, userData);
    AssetCollectorPlugin* plugin = reinterpret_cast<AssetCollectorPlugin*>(userData);
    if (plugin && isVisible)
        plugin->RefreshAssets();
}

void AssetCollectorPlugin::PanelSizeChangedNotifyProc(AIPanelRef inPanel)
{
    // The WM_SIZE message on the subclassed window handles the layout.
}

void AssetCollectorPlugin::PanelClosedNotifyProc(AIPanelRef inPanel)
{
}

//========================================================================================
// Window message handling
//========================================================================================
LRESULT CALLBACK AssetCollectorPlugin::NewWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AssetCollectorPlugin* plugin =
        reinterpret_cast<AssetCollectorPlugin*>(GetPropW(hWnd, AC_PANEL_PROP));
    LRESULT result = 0;
    if (plugin)
    {
        bool handled = plugin->PanelWindowProc(result, hWnd, msg, wParam, lParam);
        if (!handled)
            result = plugin->CallDefaultWindowProc(hWnd, msg, wParam, lParam);
    }
    else
    {
        result = DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return result;
}

LRESULT AssetCollectorPlugin::CallDefaultWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return ::CallWindowProcW(fDefaultWindProc, hWnd, msg, wParam, lParam);
}

bool AssetCollectorPlugin::PanelWindowProc(LRESULT& result, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_SIZE:
            LayoutControls();
            break;

        case WM_ERASEBKGND:
            result = 1;
            return true;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            HBRUSH br = CreateSolidBrush(ThemeHelper::Background());
            FillRect(hdc, &rc, br);
            DeleteObject(br);
            EndPaint(hWnd, &ps);
            result = 0;
            return true;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND hwndCtl = reinterpret_cast<HWND>(lParam);
            if (hwndCtl == hEditDest)
            {
                SetBkColor(hdc, ThemeHelper::EditBackground());
                SetTextColor(hdc, ThemeHelper::EditText());
                result = reinterpret_cast<LRESULT>(ThemeHelper::EditBackgroundBrush());
            }
            else
            {
                SetBkColor(hdc, ThemeHelper::Background());
                SetTextColor(hdc, ThemeHelper::Text());
                result = reinterpret_cast<LRESULT>(ThemeHelper::BackgroundBrush());
            }
            return true;
        }

        case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (dis && dis->CtlType == ODT_BUTTON)
            {
                DrawThemeButton(dis->hDC, dis->rcItem, dis->hwndItem, dis->itemState);
                result = TRUE;
                return true;
            }
            break;
        }

        case WM_NOTIFY:
        {
            LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
            if (nmhdr && nmhdr->hwndFrom == hList && nmhdr->code == NM_CLICK)
            {
                NMITEMACTIVATE* nma = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                LVHITTESTINFO ht;
                ZeroMemory(&ht, sizeof(ht));
                ht.pt = nma->ptAction;
                ListView_SubItemHitTest(hList, &ht);
                if (ht.iItem >= 0 && ht.iSubItem == 4 &&
                    ht.iItem < static_cast<int>(fChecked.size()))
                {
                    fChecked[ht.iItem] = !fChecked[ht.iItem];
                    InvalidateRect(hList, nullptr, TRUE);
                    result = 0;
                    return true;
                }
            }
            if (nmhdr && nmhdr->hwndFrom == hList && nmhdr->code == NM_CUSTOMDRAW)
            {
                LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
                switch (cd->nmcd.dwDrawStage)
                {
                    case CDDS_PREPAINT:
                        result = CDRF_NOTIFYITEMDRAW;
                        return true;

                    case CDDS_ITEMPREPAINT:
                    {
                        COLORREF textColor = ThemeHelper::Text();
                        COLORREF bgColor = ThemeHelper::Background();
                        if (cd->nmcd.dwItemSpec == (DWORD)-1)
                        {
                            // Header (LVS_NOCOLUMNHEADER header row) - paint as header
                            bgColor = ThemeHelper::Blend(ThemeHelper::Background(), ThemeHelper::Text(), 10);
                            textColor = ThemeHelper::Text();
                        }
                        else if (cd->iSubItem == 0 && (cd->nmcd.uItemState & CDIS_SELECTED))
                        {
                            bgColor = ThemeHelper::SelectionBackground();
                            textColor = ThemeHelper::Text();
                        }
                        ListView_SetBkColor(hList, ThemeHelper::Background());
                        ListView_SetTextBkColor(hList, ThemeHelper::Background());
                        cd->clrText = textColor;
                        cd->clrTextBk = bgColor;
                        // Draw with the normal UI font even though the control's
                        // (row-height) font is taller.
                        SelectObject(cd->nmcd.hdc, fUIFont);
                        result = CDRF_NOTIFYSUBITEMDRAW | CDRF_NEWFONT;
                        return true;
                    }

                    case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
                    {
                        if (cd->iSubItem == 4)
                        {
                            // Selection checkbox column: paint it ourselves and
                            // skip the default text drawing.
                            COLORREF bgColor = ThemeHelper::Background();
                            if (cd->nmcd.uItemState & CDIS_SELECTED)
                                bgColor = ThemeHelper::SelectionBackground();
                            HBRUSH br = CreateSolidBrush(bgColor);
                            RECT rc = cd->nmcd.rc;
                            FillRect(cd->nmcd.hdc, &rc, br);
                            DeleteObject(br);
                            bool checked = cd->nmcd.dwItemSpec < fChecked.size() &&
                                fChecked[cd->nmcd.dwItemSpec];
                            RECT cb = rc;
                            int size = 13;
                            cb.left = rc.left + (rc.right - rc.left - size) / 2;
                            cb.top = rc.top + (rc.bottom - rc.top - size) / 2;
                            cb.right = cb.left + size;
                            cb.bottom = cb.top + size;
                            DrawFrameControl(cd->nmcd.hdc, &cb, DFC_BUTTON,
                                DFCS_BUTTONCHECK | (checked ? DFCS_CHECKED : 0));
                            result = CDRF_SKIPDEFAULT;
                            return true;
                        }
                        COLORREF bgColor = ThemeHelper::Background();
                        COLORREF textColor = ThemeHelper::Text();
                        if (cd->nmcd.uItemState & CDIS_SELECTED)
                        {
                            bgColor = ThemeHelper::SelectionBackground();
                            textColor = ThemeHelper::Text();
                        }
                        cd->clrText = textColor;
                        cd->clrTextBk = bgColor;
                        result = CDRF_DODEFAULT;
                        return true;
                    }
                }
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_BTN_BROWSE:
                    BrowseFolder();
                    result = 0;
                    return true;
                case IDC_BTN_REFRESH:
                    // Re-dispatch through AI so the refresh runs with a valid
                    // current-document context; MatchingArtSet fails when
                    // invoked from the panel's Windows message loop.
                    if (sAINotifier)
                        sAINotifier->Notify(AC_REFRESH_NOTIFIER_TYPE, nullptr);
                    result = 0;
                    return true;
                case IDC_BTN_COPY_FONTS:
                    CopyFontEntries();
                    result = 0;
                    return true;
                case IDC_BTN_COPY_LINKS:
                    CopyLinkEntries();
                    result = 0;
                    return true;
                case IDC_BTN_PACK_ALL:
                    PackAll();
                    result = 0;
                    return true;
                case IDC_BTN_PACK_SEL:
                    PackSelected();
                    result = 0;
                    return true;
            }
            break;
        }

        case WM_MOUSEMOVE:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            HWND hwndUnder = ChildWindowFromPoint(hWnd, pt);
            HWND hwndHover = nullptr;
            if (hwndUnder && hwndUnder != hWnd &&
                (hwndUnder == hBtnBrowse || hwndUnder == hBtnRefresh ||
                 hwndUnder == hBtnCopyFonts || hwndUnder == hBtnCopyLinks ||
                 hwndUnder == hBtnPackAll || hwndUnder == hBtnPackSel))
                hwndHover = hwndUnder;

            if (hwndHover != fHoverButton)
            {
                if (fHoverButton)
                    InvalidateRect(fHoverButton, nullptr, TRUE);
                fHoverButton = hwndHover;
                if (fHoverButton)
                    InvalidateRect(fHoverButton, nullptr, TRUE);
            }

            if (!fHoverButton)
            {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                tme.dwHoverTime = 0;
                TrackMouseEvent(&tme);
            }
            result = 0;
            return true;
        }

        case WM_MOUSELEAVE:
            if (fHoverButton)
            {
                InvalidateRect(fHoverButton, nullptr, TRUE);
                fHoverButton = nullptr;
            }
            result = 0;
            return true;
    }

    result = 0;
    return false;
}

//========================================================================================
// UI construction
//========================================================================================
void AssetCollectorPlugin::CreateControls()
{
    HINSTANCE hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hDlg, GWLP_HINSTANCE));

    // UI font
    HDC hdcScreen = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdcScreen, LOGPIXELSY);
    ReleaseDC(nullptr, hdcScreen);
    int fontSize = -MulDiv(9, dpi, 72);
    fUIFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Row-height font for the list: taller than the UI font so rows don't clip
    // the text's descenders; the actual drawing switches back to fUIFont via
    // CDRF_NEWFONT in NM_CUSTOMDRAW.
    fRowFont = CreateFontW(-MulDiv(13, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    CreateThemedStatic(hDlg, IDC_LABEL_DEST, L"目标文件夹:", 0, 0, 0, 0);
    hLabelDest = GetDlgItem(hDlg, IDC_LABEL_DEST);

    hEditDest = CreateWindowExW(0, WC_EDITW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL,
        0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_EDIT_DEST), hInst, nullptr);

    CreateThemedButton(hDlg, IDC_BTN_BROWSE, L"浏览…", 0, 0, 0, 0);
    hBtnBrowse = GetDlgItem(hDlg, IDC_BTN_BROWSE);
    CreateThemedButton(hDlg, IDC_BTN_REFRESH, L"刷新资源", 0, 0, 0, 0);
    hBtnRefresh = GetDlgItem(hDlg, IDC_BTN_REFRESH);
    CreateThemedButton(hDlg, IDC_BTN_COPY_FONTS, L"打包所有字体", 0, 0, 0, 0);
    hBtnCopyFonts = GetDlgItem(hDlg, IDC_BTN_COPY_FONTS);
    CreateThemedButton(hDlg, IDC_BTN_COPY_LINKS, L"打包所有链接", 0, 0, 0, 0);
    hBtnCopyLinks = GetDlgItem(hDlg, IDC_BTN_COPY_LINKS);
    CreateThemedButton(hDlg, IDC_BTN_PACK_ALL, L"打包所有", 0, 0, 0, 0);
    hBtnPackAll = GetDlgItem(hDlg, IDC_BTN_PACK_ALL);
    CreateThemedButton(hDlg, IDC_BTN_PACK_SEL, L"打包已选", 0, 0, 0, 0);
    hBtnPackSel = GetDlgItem(hDlg, IDC_BTN_PACK_SEL);

    // Column header statics (the list itself has no header)
    CreateThemedStatic(hDlg, IDC_HDR_NAME, L"资源名称", 0, 0, 0, 0);
    hHdrName = GetDlgItem(hDlg, IDC_HDR_NAME);
    CreateThemedStatic(hDlg, IDC_HDR_TYPE, L"类型", 0, 0, 0, 0);
    hHdrType = GetDlgItem(hDlg, IDC_HDR_TYPE);
    CreateThemedStatic(hDlg, IDC_HDR_FILE, L"文件", 0, 0, 0, 0);
    hHdrFile = GetDlgItem(hDlg, IDC_HDR_FILE);
    CreateThemedStatic(hDlg, IDC_HDR_LOC, L"位置", 0, 0, 0, 0);
    hHdrLoc = GetDlgItem(hDlg, IDC_HDR_LOC);
    CreateThemedStatic(hDlg, IDC_HDR_CHK, L"选择", 0, 0, 0, 0);
    hHdrChk = GetDlgItem(hDlg, IDC_HDR_CHK);

    // Asset list (report view, header hidden - we draw our own)
    hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
        LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_LIST_ASSETS), hInst, nullptr);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    for (int i = 0; i < 5; ++i)
    {
        LVCOLUMNW col;
        col.mask = LVCF_WIDTH | LVCF_FMT;
        col.fmt = LVCFMT_LEFT;
        col.cx = 100;
        ListView_InsertColumn(hList, i, &col);
    }

    hStatus = CreateWindowExW(0, WC_STATICW, L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_STATIC_STATUS), hInst, nullptr);

    // Apply UI font to all child controls
    for (HWND child = GetWindow(hDlg, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(fUIFont), TRUE);

    // The list uses the taller font so its row height fits the text
    SendMessageW(hList, WM_SETFONT, reinterpret_cast<WPARAM>(fRowFont), TRUE);

    LayoutControls();
    ApplyTheme();
}

void AssetCollectorPlugin::CreateThemedButton(HWND parent, int ctrlID, const wchar_t* label,
                                              int x, int y, int w, int h)
{
    CreateWindowExW(0, WC_BUTTONW, label,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, parent, reinterpret_cast<HMENU>(ctrlID),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)), nullptr);
}

void AssetCollectorPlugin::CreateThemedStatic(HWND parent, int ctrlID, const wchar_t* label,
                                              int x, int y, int w, int h)
{
    CreateWindowExW(0, WC_STATICW, label,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h, parent, reinterpret_cast<HMENU>(ctrlID),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)), nullptr);
}

void AssetCollectorPlugin::LayoutControls()
{
    if (!hDlg || !hList || !hEditDest)
        return;
    RECT rc;
    GetClientRect(hDlg, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    const int m = 8;
    const int gap = 6;
    const int ctrlH = 20;

    int y = m;

    // Row 1: destination folder
    int labelW = 64;
    MoveWindow(hLabelDest, m, y, labelW, ctrlH, TRUE);
    int editW = W - m - labelW - gap - 56 - gap;
    if (editW < 60) editW = 60;
    MoveWindow(hEditDest, m + labelW + gap, y, editW, ctrlH, TRUE);
    MoveWindow(hBtnBrowse, W - m - 56, y, 56, ctrlH, TRUE);

    // Row 2: column headers
    y += ctrlH + gap;
    int hdrH = 20;
    int listW = W - 2 * m;
    int colChk = 40;
    int colName = listW * 26 / 100;
    int colType = listW * 12 / 100;
    int colFile = listW * 26 / 100;
    int colLoc = listW - colName - colType - colFile - colChk;
    if (colLoc < 60) colLoc = 60;
    MoveWindow(hHdrName, m, y, colName, hdrH, TRUE);
    MoveWindow(hHdrType, m + colName, y, colType, hdrH, TRUE);
    MoveWindow(hHdrFile, m + colName + colType, y, colFile, hdrH, TRUE);
    MoveWindow(hHdrLoc, m + colName + colType + colFile, y, colLoc, hdrH, TRUE);
    MoveWindow(hHdrChk, m + colName + colType + colFile + colLoc, y, colChk, hdrH, TRUE);

    // Row 3: list
    y += hdrH + gap;
    int listH = H - y - m;
    listH -= 3 * (ctrlH + gap);   // three control rows below
    if (listH < 40) listH = 40;
    MoveWindow(hList, m, y, listW, listH, TRUE);

    // Button width: 7 full-width (Chinese) characters of the UI font
    HDC hdc = GetDC(hDlg);
    HFONT oldFont = nullptr;
    if (fUIFont)
        oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, fUIFont));
    SIZE sz;
    GetTextExtentPoint32W(hdc, L"刷", 1, &sz);
    if (oldFont)
        SelectObject(hdc, oldFont);
    ReleaseDC(hDlg, hdc);
    int btnW = sz.cx * 7;
    if (btnW < 60) btnW = 60;

    // Row 4: refresh + font + link actions
    y += listH + gap;
    MoveWindow(hBtnRefresh, m, y, btnW, ctrlH, TRUE);
    MoveWindow(hBtnCopyFonts, m + btnW + gap, y, btnW, ctrlH, TRUE);
    MoveWindow(hBtnCopyLinks, m + 2 * (btnW + gap), y, btnW, ctrlH, TRUE);

    // Row 5: pack selected + pack all
    y += ctrlH + gap;
    MoveWindow(hBtnPackSel, m, y, btnW, ctrlH, TRUE);
    MoveWindow(hBtnPackAll, m + btnW + gap, y, btnW, ctrlH, TRUE);

    // Row 6: status line (scanning progress -> resource summary)
    y += ctrlH + gap;
    MoveWindow(hStatus, m, y, W - 2 * m, ctrlH, TRUE);

    // Column widths follow the list width; re-ellipsize when it changed
    RECT lrc;
    GetClientRect(hList, &lrc);
    int lw = lrc.right - lrc.left;
    ListView_SetColumnWidth(hList, 0, lw * 26 / 100);
    ListView_SetColumnWidth(hList, 1, lw * 12 / 100);
    ListView_SetColumnWidth(hList, 2, lw * 26 / 100);
    ListView_SetColumnWidth(hList, 3, lw - lw * 26 / 100 - lw * 12 / 100 - lw * 26 / 100 - 40);
    ListView_SetColumnWidth(hList, 4, 40);
    if (lw != fLastListWidth)
    {
        fLastListWidth = lw;
        RepopulateListView();
    }
}

void AssetCollectorPlugin::ApplyTheme()
{
    if (!hDlg)
        return;
    InvalidateRect(hDlg, nullptr, TRUE);
    for (HWND child = GetWindow(hDlg, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        InvalidateRect(child, nullptr, TRUE);
    if (hList)
    {
        ListView_SetBkColor(hList, ThemeHelper::Background());
        ListView_SetTextBkColor(hList, ThemeHelper::Background());
        ListView_SetTextColor(hList, ThemeHelper::Text());
    }
    UpdateWindow(hDlg);
}

void AssetCollectorPlugin::DrawThemeButton(HDC hdc, const RECT& rc, HWND hwndBtn, UINT itemState)
{
    bool hover = (hwndBtn == fHoverButton);
    bool pressed = (itemState & ODS_SELECTED) != 0;
    bool disabled = (itemState & ODS_DISABLED) != 0;

    COLORREF bg;
    if (pressed)
        bg = ThemeHelper::Blend(ThemeHelper::Background(), ThemeHelper::Text(), 22);
    else if (hover)
        bg = ThemeHelper::Blend(ThemeHelper::Background(), ThemeHelper::Text(), 13);
    else
        bg = ThemeHelper::Blend(ThemeHelper::Background(), ThemeHelper::Text(), 5);

    HBRUSH br = CreateSolidBrush(bg);
    HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, 8, 8);
    FillRgn(hdc, rgn, br);
    DeleteObject(br);

    // Rounded outline
    HPEN pen = CreatePen(PS_SOLID, 1, ThemeHelper::Border());
    HGDIOBJ oldPen = reinterpret_cast<HGDIOBJ>(SelectObject(hdc, pen));
    HGDIOBJ oldBrush = reinterpret_cast<HGDIOBJ>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(rgn);

    wchar_t label[128];
    GetWindowTextW(hwndBtn, label, 128);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? ThemeHelper::Blend(ThemeHelper::Background(), ThemeHelper::Text(), 45)
                               : ThemeHelper::Text());

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwndBtn, WM_GETFONT, 0, 0));
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, font));
    RECT tr = rc;
    tr.top += 1;
    tr.bottom -= 1;
    DrawTextW(hdc, label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    if (itemState & ODS_FOCUS)
    {
        RECT fr = rc;
        InflateRect(&fr, -3, -3);
        DrawFocusRect(hdc, &fr);
    }
}

//========================================================================================
// Status / destination folder helpers
//========================================================================================
void AssetCollectorPlugin::UpdateStatus(const ai::UnicodeString& text)
{
    if (hStatus)
    {
        ai::WCHARStr ws(text);
        SetWindowTextW(hStatus, ws);
    }
}

void AssetCollectorPlugin::LoadDestFolderPref()
{
    if (!sAIPreference)
        return;

    ai::FilePath fp;
    if (sAIPreference->GetFilePathSpecificationPreference(
            kACPrefPrefix, kACDestFolderSuffix, fp) == kNoErr)
    {
        fDestFolder = fp.GetFullPath();
        SetDestFolderEdit(fDestFolder);
    }
}

void AssetCollectorPlugin::SaveDestFolderPref()
{
    if (!sAIPreference || fDestFolder.as_ASUnicode().empty())
        return;
    ai::FilePath fp(fDestFolder);
    sAIPreference->PutFilePathSpecificationPreference(
        kACPrefPrefix, kACDestFolderSuffix, fp);
}

ai::UnicodeString AssetCollectorPlugin::GetDestFolderFromEdit()
{
    if (!hEditDest)
        return fDestFolder;
    int len = GetWindowTextLengthW(hEditDest);
    if (len <= 0)
        return ai::UnicodeString();
    std::wstring buf(len + 1, L'\0');
    GetWindowTextW(hEditDest, &buf[0], len + 1);
    buf.resize(len);
    return ai::UnicodeString(buf.c_str());
}

void AssetCollectorPlugin::SetDestFolderEdit(const ai::UnicodeString& folder)
{
    if (hEditDest)
    {
        ai::WCHARStr ws(folder);
        SetWindowTextW(hEditDest, ws);
    }
}

//========================================================================================
// Document asset discovery
//========================================================================================
ai::UnicodeString AssetCollectorPlugin::GetDocumentFilePath()
{
    if (!sAIDocumentList || !sAIDocument)
        return ai::UnicodeString();

    AIDocumentHandle doc = nullptr;
    if (sAIDocumentList->GetNthDocument(&doc, 0) != kNoErr || !doc)
        return ai::UnicodeString();
    ai::FilePath fp;
    if (sAIDocument->GetDocumentFileSpecificationFromHandle(doc, fp) != kNoErr)
        return ai::UnicodeString();
    return fp.GetFullPath();
}

ai::UnicodeString AssetCollectorPlugin::GetDocumentName()
{
    ai::UnicodeString path = GetDocumentFilePath();
    if (path.as_ASUnicode().empty())
        return ai::UnicodeString();
    return ai::FilePath(path).GetFileNameNoExt();
}

AIErr AssetCollectorPlugin::CollectDocumentAssets()
{
    fAssets.clear();

    if (!sAIDocument || !sAIDocumentList || !sAIArtSet || !sAIArt || !sAIPlaced)
        return kNoErr;

    // Prefer the current document; when called from a panel window message
    // there may be no current-document context, so fall back to the first
    // open document and activate it (the SDK pattern used by SnpChooser).
    AIDocumentHandle doc = nullptr;
    AIErr error = sAIDocument->GetDocument(&doc);
    if (error != kNoErr || !doc)
    {
        error = sAIDocumentList->GetNthDocument(&doc, 0);
        if (error != kNoErr || !doc)
            return error;
        error = sAIDocumentList->Activate(doc, false);
        if (error != kNoErr)
            return error;
    }

    // One traversal for both text frames (fonts) and placed art (links).
    AIArtSet artSet = nullptr;
    error = sAIArtSet->NewArtSet(&artSet);
    if (error != kNoErr)
        return error;

    AIArtSpec specs[2];
    specs[0].type = kTextFrameArt;
    specs[0].whichAttr = 0;
    specs[0].attr = 0;
    specs[1].type = kPlacedArt;
    specs[1].whichAttr = 0;
    specs[1].attr = 0;

    error = sAIArtSet->MatchingArtSet(specs, 2, artSet);
    if (error != kNoErr)
    {
        sAIArtSet->DisposeArtSet(&artSet);
        return error;
    }

    size_t count = 0;
    if (sAIArtSet->CountArtSet(artSet, &count) == kNoErr)
    {
        for (size_t i = 0; i < count; ++i)
        {
            AIArtHandle art = nullptr;
            if (sAIArtSet->IndexArtSet(artSet, i, &art) != kNoErr || !art)
                continue;
            ai::int16 type = 0;
            if (sAIArt->GetArtType(art, &type) != kNoErr)
                continue;
            if (type == kTextFrameArt)
                AddFontFromFrame(art);
            else if (type == kPlacedArt)
            {
                ai::FilePath fp;
                if (sAIPlaced->GetPlacedFileSpecification(art, fp) == kNoErr)
                    AddLinkEntry(fp.GetFullPath());
            }
        }
    }

    sAIArtSet->DisposeArtSet(&artSet);
    return kNoErr;
}

void AssetCollectorPlugin::AddFontFromFrame(AIArtHandle frame)
{
    try
    {
        // All suites used below (AI + ATE) are optional on older hosts.
        if (!sAITextFrame || !sAIFont ||
            !sTextFrame || !sStory || !sStories ||
            !sTextRanges || !sCharInspector || !sArrayFontRef || !sFont)
            return;

        // Reach every story of the document through this frame and collect all
        // font references at once (pattern from FontCollector / SnpText.cpp).
        TextFrameRef textFrameRef = nullptr;
        if (sAITextFrame->GetATETextFrame(frame, &textFrameRef) != kNoErr || !textFrameRef)
            return;

        ATE::ITextFrame textFrame(textFrameRef);
        ATE::IStory story = textFrame.GetStory();
        ATE::IStories stories = story.GetStories();
        ATE::ITextRanges textRanges = stories.GetTextRanges();
        ATE::ICharInspector inspector = textRanges.GetCharInspector();
        ATE::IArrayFontRef fonts = inspector.GetFont();
        ATETextDOM::Int32 fontCount = fonts.GetSize();
        for (ATETextDOM::Int32 f = 0; f < fontCount; ++f)
        {
            ATE::IFont font = fonts.Item(f);
            if (font.IsNull())
                continue;
            FontRef fontRef = font.GetRef();
            AIFontKey fontKey = nullptr;
            if (sAIFont->FontKeyFromFont(fontRef, &fontKey) == kNoErr && fontKey)
                AddFontEntry(fontKey);
        }
    }
    catch (ATE::Exception&)
    {
    }
    catch (...)
    {
    }
}

void AssetCollectorPlugin::AddFontEntry(AIFontKey fontKey)
{
    for (const AssetEntry& existing : fAssets)
        if (existing.kind == AssetEntry::Font && existing.fontKey == fontKey)
            return;

    if (!sAIFont)
        return;

    AssetEntry entry;
    entry.kind = AssetEntry::Font;
    entry.fontKey = fontKey;

    ASUnicode nameBuf[1024];
    if (sAIFont->GetFontFamilyUINameUnicode(fontKey, nameBuf, 1024) == kNoErr)
        entry.name = ai::UnicodeString(nameBuf);
    if (sAIFont->GetFontStyleUINameUnicode(fontKey, nameBuf, 1024) == kNoErr)
        entry.sub = ai::UnicodeString(nameBuf);

    ai::uint32 fileCount = 0;
    if (sAIFont->GetFontFilePathCount(fontKey, fileCount) == kNoErr)
    {
        for (ai::uint32 n = 0; n < fileCount; ++n)
        {
            ai::FilePath fp;
            if (sAIFont->GetNthFilePath(fontKey, n, fp) == kNoErr)
                entry.filePaths.push_back(fp.GetFullPath());
        }
    }

    if (!entry.filePaths.empty())
        entry.location = ai::FilePath(entry.filePaths[0]).GetParent().GetFullPath();

    fAssets.push_back(entry);
}

void AssetCollectorPlugin::AddLinkEntry(const ai::UnicodeString& filePath)
{
    if (filePath.as_ASUnicode().empty())
        return;

    for (const AssetEntry& existing : fAssets)
        if (existing.kind == AssetEntry::Link &&
            !existing.filePaths.empty() &&
            existing.filePaths[0].as_ASUnicode() == filePath.as_ASUnicode())
            return;

    AssetEntry entry;
    entry.kind = AssetEntry::Link;
    entry.filePaths.push_back(filePath);

    ai::FilePath fp(filePath);
    entry.name = fp.GetFileName();
    entry.location = fp.GetParent().GetFullPath();

    ai::WCHARStr wsName(fp.GetFileName());
    std::wstring s = wsName.as_LPCWSTR();
    size_t dot = s.find_last_of(L'.');
    ai::UnicodeString ext;
    if (dot != std::wstring::npos && dot + 1 < s.size())
    {
        std::wstring e = s.substr(dot + 1);
        for (wchar_t& c : e)
            c = towupper(c);
        ext = ai::UnicodeString(e.c_str());
    }
    entry.sub = ext;

    fAssets.push_back(entry);
}

void AssetCollectorPlugin::RefreshAssets()
{
    fAssets.clear();
    fChecked.clear();
    ClearListView();
    UpdateStatus(ai::UnicodeString(L"正在扫描文档资源…"));

    AIErr error = CollectDocumentAssets();
    if (error != kNoErr)
    {
        UpdateStatus(ai::UnicodeString(L"没有打开的文档"));
        return;
    }

    if (fAssets.empty())
    {
        UpdateStatus(ai::UnicodeString(L"当前文档没有字体或链接"));
        return;
    }

    sort(fAssets.begin(), fAssets.end(),
        [](const AssetEntry& a, const AssetEntry& b)
        {
            if (a.kind != b.kind)
                return a.kind < b.kind;
            return _wcsicmp(
                reinterpret_cast<const wchar_t*>(a.name.as_ASUnicode().c_str()),
                reinterpret_cast<const wchar_t*>(b.name.as_ASUnicode().c_str())) < 0;
        });

    PopulateListView();

    size_t fontCount = 0;
    size_t linkCount = 0;
    for (const AssetEntry& e : fAssets)
    {
        if (e.kind == AssetEntry::Font)
            ++fontCount;
        else
            ++linkCount;
    }

    std::wstring status = L"共" + std::to_wstring(fAssets.size()) + L"个资源(字体"
        + std::to_wstring(fontCount) + L"，链接" + std::to_wstring(linkCount) + L")";
    UpdateStatus(ai::UnicodeString(status.c_str()));
}

//========================================================================================
// List view population
//========================================================================================
void AssetCollectorPlugin::ClearListView()
{
    if (hList)
        ListView_DeleteAllItems(hList);
}

void AssetCollectorPlugin::PopulateListView()
{
    ClearListView();
    if (fAssets.empty() || !hList)
        return;

    fChecked.resize(fAssets.size(), false);

    RECT rc;
    GetClientRect(hList, &rc);
    int lw = rc.right - rc.left;
    HDC hdc = GetDC(hList);

    for (size_t i = 0; i < fAssets.size(); ++i)
    {
        const AssetEntry& entry = fAssets[i];

        ai::UnicodeString typeName = entry.kind == AssetEntry::Font
            ? ai::UnicodeString(L"字体") : ai::UnicodeString(L"链接");
        ai::UnicodeString file = entry.filePaths.empty()
            ? ai::UnicodeString(L"—")
            : entry.filePaths[0];

        ai::UnicodeString nameE = Ellipsize(entry.name, lw * 26 / 100 - 8, hdc);
        ai::UnicodeString subE = Ellipsize(entry.sub, lw * 12 / 100 - 8, hdc);
        ai::UnicodeString fileE = Ellipsize(file, lw * 26 / 100 - 8, hdc);
        ai::UnicodeString locE = Ellipsize(entry.location, lw - lw * 26 / 100 - lw * 12 / 100 - lw * 26 / 100 - 40 - 8, hdc);

        ai::WCHARStr wsName(nameE), wsType(typeName), wsSub(subE), wsFile(fileE), wsLoc(locE);

        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(wsName.as_LPCWSTR());
        ListView_InsertItem(hList, &item);
        ListView_SetItemText(hList, static_cast<int>(i), 1,
            const_cast<LPWSTR>(wsType.as_LPCWSTR()));
        ListView_SetItemText(hList, static_cast<int>(i), 2,
            const_cast<LPWSTR>(wsSub.as_LPCWSTR()));
        ListView_SetItemText(hList, static_cast<int>(i), 3,
            const_cast<LPWSTR>(wsLoc.as_LPCWSTR()));
        ListView_SetItemText(hList, static_cast<int>(i), 4, L"");
    }

    ReleaseDC(hList, hdc);
}

void AssetCollectorPlugin::RepopulateListView()
{
    // Called on list resize; re-run so that truncated strings use new column widths.
    // Also re-paints the visible data (keeps theme colors fresh).
    if (hList)
    {
        PopulateListView();
        InvalidateRect(hList, nullptr, TRUE);
    }
}

ai::UnicodeString AssetCollectorPlugin::Ellipsize(const ai::UnicodeString& text, int maxWidth, HDC hdc)
{
    ai::WCHARStr wsText(text);
    std::wstring s = wsText.as_LPCWSTR();
    if (maxWidth <= 8 || s.empty())
        return text;

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(hList, WM_GETFONT, 0, 0));
    if (!font)
        return text;
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, font));

    SIZE sz;
    GetTextExtentPoint32W(hdc, s.c_str(), static_cast<int>(s.size()), &sz);
    if (sz.cx <= maxWidth)
    {
        SelectObject(hdc, oldFont);
        return text;
    }

    std::wstring ell = L"…";
    while (s.size() > 1)
    {
        s.pop_back();
        std::wstring cand = s + ell;
        GetTextExtentPoint32W(hdc, cand.c_str(), static_cast<int>(cand.size()), &sz);
        if (sz.cx <= maxWidth)
        {
            SelectObject(hdc, oldFont);
            return ai::UnicodeString(cand.c_str());
        }
    }

    SelectObject(hdc, oldFont);
    return ai::UnicodeString(ell.c_str());
}

ai::UnicodeString AssetCollectorPlugin::SanitizeFileName(const ai::UnicodeString& name)
{
    ai::WCHARStr wsName(name);
    std::wstring s = wsName.as_LPCWSTR();
    const wchar_t bad[] = L"\\/:*?\"<>|";
    for (wchar_t& c : s)
        if (wcschr(bad, c))
            c = L'_';
    while (!s.empty() && (s.back() == L'.' || s.back() == L' '))
        s.pop_back();
    if (s.empty())
        s = L"AssetCollector";
    return ai::UnicodeString(s.c_str());
}

//========================================================================================
// Folder browsing / copying
//========================================================================================
void AssetCollectorPlugin::BrowseFolder()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IFileOpenDialog* pfd = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&pfd))))
        return;

    DWORD options = 0;
    pfd->GetOptions(&options);
    pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

    ai::UnicodeString current = GetDestFolderFromEdit();
    if (!current.as_ASUnicode().empty())
    {
        ai::WCHARStr ws(current);
        IShellItem* shellItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(ws.as_LPCWSTR(),
                                                  nullptr, IID_PPV_ARGS(&shellItem))))
        {
            pfd->SetFolder(shellItem);
            shellItem->Release();
        }
    }

    if (SUCCEEDED(pfd->Show(hDlg)))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(pfd->GetResult(&item)))
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            {
                fDestFolder = ai::UnicodeString(path);
                CoTaskMemFree(path);
                SetDestFolderEdit(fDestFolder);
                SaveDestFolderPref();
            }
            item->Release();
        }
    }

    pfd->Release();
}

//========================================================================================
// Copy / pack actions
//========================================================================================
// MD5 of a file's content (used to tell whether two files with the same name
// are actually the same content or not).
static std::wstring FileMD5(const std::wstring& path)
{
    std::wstring digestHex;
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return digestHex;

    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    if (CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(prov, CALG_MD5, 0, 0, &hash))
    {
        BYTE buffer[65536];
        DWORD read = 0;
        while (ReadFile(hFile, buffer, sizeof(buffer), &read, nullptr) && read > 0)
        {
            CryptHashData(hash, buffer, read, 0);
            if (read < sizeof(buffer))
                break;
        }
        BYTE digest[16];
        DWORD digestLen = sizeof(digest);
        if (CryptGetHashParam(hash, HP_HASHVAL, digest, &digestLen, 0))
        {
            wchar_t hex[3];
            for (DWORD i = 0; i < digestLen; ++i)
            {
                swprintf(hex, 3, L"%02x", digest[i]);
                digestHex += hex;
            }
        }
    }
    if (hash)
        CryptDestroyHash(hash);
    if (prov)
        CryptReleaseContext(prov, 0);
    CloseHandle(hFile);
    return digestHex;
}

// Do two files contain identical content?
static bool FilesIdentical(const std::wstring& a, const std::wstring& b)
{
    return FileMD5(a) == FileMD5(b);
}

// True when a file with this path already exists on disk.
static bool PathExists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Build the next free destination for a colliding file name.
// "logo.png" -> "logo (2).png", "logo (3).png", ... until one is free.
static std::wstring FindFreeDest(const std::wstring& dir, const std::wstring& fileName)
{
    std::wstring dst = dir + L"\\" + fileName;
    if (!PathExists(dst))
        return dst;

    // Split "name.ext" into "name" and ".ext" so the counter goes before the extension.
    size_t dot = fileName.find_last_of(L'.');
    std::wstring stem = (dot == std::wstring::npos) ? fileName : fileName.substr(0, dot);
    std::wstring ext = (dot == std::wstring::npos) ? std::wstring() : fileName.substr(dot);

    for (int n = 2;; ++n)
    {
        std::wstring candidate = dir + L"\\" + stem + L" (" + std::to_wstring(n) + L")" + ext;
        if (!PathExists(candidate))
            return candidate;
    }
}

std::wstring AssetCollectorPlugin::GetTargetBase()
{
    ai::UnicodeString dest = GetDestFolderFromEdit();
    ai::WCHARStr wsDest(dest);
    std::wstring base = wsDest.as_LPCWSTR();
    if (base.empty())
        return std::wstring();
    if (base.back() != L'\\')
        base += L'\\';
    ai::UnicodeString docName = GetDocumentName();
    if (docName.as_ASUnicode().empty())
        docName = ai::UnicodeString(L"AssetCollector");
    ai::WCHARStr wsDocName(SanitizeFileName(docName));
    base += wsDocName.as_LPCWSTR();
    return base;
}

CopyResult AssetCollectorPlugin::CopyToSubdir(AssetEntry::Kind kind,
                                              const std::wstring& subdir,
                                              bool selectedOnly)
{
    CopyResult res;
    std::vector<std::wstring> collectedDst;  // source paths already copied this run

    int match = 0;
    for (size_t i = 0; i < fAssets.size(); ++i)
        if (fAssets[i].kind == kind && (!selectedOnly || (i < fChecked.size() && fChecked[i])))
            ++match;
    if (match == 0)
    {
        if (selectedOnly)
            res.needSelect = true;
        else
            res.nothing = true;
        return res;
    }

    std::wstring base = GetTargetBase();
    if (base.empty())
    {
        if (sAIUser)
            sAIUser->MessageAlert(ai::UnicodeString(L"请先选择目标文件夹。"));
        res.fatal = true;
        return res;
    }

    // <dest>\<document name>\fonts  /  \links
    if (!CreateDirectoryW(base.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        std::wstring errMsg = L"无法创建目标文件夹：\n" + base;
        MessageBoxW(hDlg, errMsg.c_str(), L"AssetCollector", MB_OK | MB_ICONERROR | MB_APPLMODAL);
        res.fatal = true;
        return res;
    }
    std::wstring sub = base + L"\\" + subdir;
    if (!CreateDirectoryW(sub.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        std::wstring errMsg = L"无法创建目标文件夹：\n" + sub;
        MessageBoxW(hDlg, errMsg.c_str(), L"AssetCollector", MB_OK | MB_ICONERROR | MB_APPLMODAL);
        res.fatal = true;
        return res;
    }

    for (size_t i = 0; i < fAssets.size(); ++i)
    {
        const AssetEntry& entry = fAssets[i];
        if (entry.kind != kind)
            continue;
        if (selectedOnly && (i >= fChecked.size() || !fChecked[i]))
            continue;
        if (entry.filePaths.empty())
        {
            ++res.skipped;
            continue;
        }
        for (const ai::UnicodeString& src : entry.filePaths)
        {
            ai::FilePath srcFp(src);
            ai::WCHARStr wsFileName(srcFp.GetFileName());
            std::wstring fileName = wsFileName.as_LPCWSTR();
            ai::WCHARStr wsSrc(src);

            // Skip source files we have already collected this run (identical
            // content), regardless of the target file name.
            bool alreadyCollected = false;
            for (const std::wstring& existing : collectedDst)
            {
                if (existing == wsSrc.as_LPCWSTR())
                {
                    alreadyCollected = true;
                    break;
                }
            }
            if (alreadyCollected)
            {
                ++res.deduped;
                res.logLines.push_back(L"[去重] " + fileName + L" 内容与已收集文件相同，跳过："
                    + wsSrc.as_LPCWSTR());
                continue;
            }

            // Case A: a file with the plain name already exists and is identical
            // content -> keep the existing one, do not copy (deduplicate).
            std::wstring plainDst = sub + L"\\" + fileName;
            if (PathExists(plainDst) && FilesIdentical(wsSrc.as_LPCWSTR(), plainDst))
            {
                ++res.deduped;
                collectedDst.push_back(wsSrc.as_LPCWSTR());
                res.logLines.push_back(L"[去重] " + fileName + L" 目标已存在相同内容："
                    + plainDst);
                continue;
            }

            // Case B: plain name is free, or occupied by different content.
            // In the latter case FindFreeDest moves to "name (n).ext".
            std::wstring dst = FindFreeDest(sub, fileName);
            std::wstring finalName = dst.substr(sub.size() + 1);
            bool renamed = (finalName != fileName);

            if (CopyFileW(wsSrc.as_LPCWSTR(), dst.c_str(), FALSE))
            {
                ++res.ok;
                collectedDst.push_back(wsSrc.as_LPCWSTR());
                if (renamed)
                {
                    ++res.renamed;
                    res.renameNotes.push_back(fileName + L" -> " + finalName
                        + L"  （" + wsSrc.as_LPCWSTR() + L"）");
                    res.logLines.push_back(L"[改名] " + fileName + L" 与已收集文件不同，保存为 "
                        + finalName + L"：源 " + wsSrc.as_LPCWSTR());
                }
                else
                {
                    res.logLines.push_back(L"[复制] " + fileName + L" -> " + dst);
                }
            }
            else
            {
                ++res.failed;
                res.logLines.push_back(L"[失败] " + fileName + L" 复制失败："
                    + wsSrc.as_LPCWSTR() + L" -> " + dst);
            }
        }
    }
    return res;
}

void AssetCollectorPlugin::ShowCopyResult(const ai::UnicodeString& kindName,
                                          const CopyResult& r,
                                          const std::wstring& subdir)
{
    ai::WCHARStr wsKind(kindName);
    std::wstring msg = std::wstring(wsKind.as_LPCWSTR()) + L"打包完成：成功 "
        + std::to_wstring(r.ok) + L" 个文件";
    if (r.renamed > 0)
        msg += L"，改名 " + std::to_wstring(r.renamed);
    if (r.deduped > 0)
        msg += L"，去重 " + std::to_wstring(r.deduped);
    if (r.skipped > 0)
        msg += L"，跳过 " + std::to_wstring(r.skipped);
    if (r.failed > 0)
        msg += L"，失败 " + std::to_wstring(r.failed);

    if (!r.renameNotes.empty())
    {
        msg += L"\n\n以下文件因重名且内容不同而被改名：\n";
        for (size_t i = 0; i < r.renameNotes.size(); ++i)
            msg += L"  " + r.renameNotes[i] + L"\n";
    }

    std::wstring base = GetTargetBase();
    msg += L"\n目标文件夹：\n" + base + L"\\" + subdir;

    MessageBoxW(hDlg, msg.c_str(), L"AssetCollector", MB_OK | MB_ICONINFORMATION | MB_APPLMODAL);

    std::wstring status = std::wstring(wsKind.as_LPCWSTR()) + L"：成功 "
        + std::to_wstring(r.ok) + L" 个";
    if (r.renamed > 0)
        status += L"，改名 " + std::to_wstring(r.renamed);
    if (r.deduped > 0)
        status += L"，去重 " + std::to_wstring(r.deduped);
    if (r.failed > 0)
        status += L"，失败 " + std::to_wstring(r.failed);
    UpdateStatus(ai::UnicodeString(status.c_str()));
}

void AssetCollectorPlugin::ShowPackResult(const CopyResult& rf, const CopyResult& rl, const CopyResult& rd)
{
    std::wstring msg = L"打包完成：\n\n";
    msg += L"字体：成功 " + std::to_wstring(rf.ok) + L" 个文件";
    if (rf.renamed > 0)
        msg += L"，改名 " + std::to_wstring(rf.renamed);
    if (rf.deduped > 0)
        msg += L"，去重 " + std::to_wstring(rf.deduped);
    if (rf.skipped > 0)
        msg += L"，跳过 " + std::to_wstring(rf.skipped);
    if (rf.failed > 0)
        msg += L"，失败 " + std::to_wstring(rf.failed);
    if (rf.needSelect)
        msg += L"（未选择）";
    else if (rf.nothing)
        msg += L"（文档没有字体）";
    msg += L"\n链接：成功 " + std::to_wstring(rl.ok) + L" 个文件";
    if (rl.renamed > 0)
        msg += L"，改名 " + std::to_wstring(rl.renamed);
    if (rl.deduped > 0)
        msg += L"，去重 " + std::to_wstring(rl.deduped);
    if (rl.skipped > 0)
        msg += L"，跳过 " + std::to_wstring(rl.skipped);
    if (rl.failed > 0)
        msg += L"，失败 " + std::to_wstring(rl.failed);
    if (rl.needSelect)
        msg += L"（未选择）";
    else if (rl.nothing)
        msg += L"（文档没有链接）";
    if (rd.ok > 0 || rd.skipped > 0 || rd.failed > 0)
    {
        msg += L"\n文档：成功 " + std::to_wstring(rd.ok) + L" 个文件";
        if (rd.skipped > 0)
            msg += L"，跳过 " + std::to_wstring(rd.skipped);
        if (rd.failed > 0)
            msg += L"，失败 " + std::to_wstring(rd.failed);
    }
    else
    {
        msg += L"\n文档：未保存，未包含文档文件";
    }

    // List every renamed file so the user can map old -> new names.
    std::wstring renameBlock;
    if (!rf.renameNotes.empty())
    {
        renameBlock += L"\n\n字体改名：\n";
        for (size_t i = 0; i < rf.renameNotes.size(); ++i)
            renameBlock += L"  " + rf.renameNotes[i] + L"\n";
    }
    if (!rl.renameNotes.empty())
    {
        renameBlock += L"\n链接改名：\n";
        for (size_t i = 0; i < rl.renameNotes.size(); ++i)
            renameBlock += L"  " + rl.renameNotes[i] + L"\n";
    }
    msg += renameBlock;

    std::wstring base = GetTargetBase();
    msg += L"\n目标文件夹：\n" + base;

    MessageBoxW(hDlg, msg.c_str(), L"AssetCollector", MB_OK | MB_ICONINFORMATION | MB_APPLMODAL);

    std::wstring status = L"字体 " + std::to_wstring(rf.ok) + L" 个，链接 "
        + std::to_wstring(rl.ok) + L" 个";
    if (rd.ok > 0)
        status += L"，文档 " + std::to_wstring(rd.ok);
    if (rf.renamed > 0 || rl.renamed > 0)
        status += L"，有改名";
    if (rf.deduped > 0 || rl.deduped > 0)
        status += L"，有去重";
    if (rf.failed > 0 || rl.failed > 0 || rd.failed > 0)
        status += L"，有失败";
    UpdateStatus(ai::UnicodeString(status.c_str()));
}

// Convert a wide string to UTF-8 (used by the TXT log).
static std::string ToUtf8(const std::wstring& w)
{
    if (w.empty())
        return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                  nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        &s[0], len, nullptr, nullptr);
    return s;
}

// Write the collection log as a UTF-8 TXT file next to the collected assets.
// The log records every asset found in the document (fonts / links), where it
// was copied, which files were renamed / deduplicated and why.
void AssetCollectorPlugin::WriteCollectLog(const std::wstring& base,
                                           const CopyResult& rf,
                                           const CopyResult& rl,
                                           const CopyResult& rd)
{
    if (base.empty())
        return;

    std::wstring log;
    log += L"====================================================\n";
    log += L" AssetCollector 收集日志\n";
    log += L"====================================================\n";

    ai::UnicodeString docPath = GetDocumentFilePath();
    ai::UnicodeString docName = GetDocumentName();
    log += L"文档文件：";
    log += (docPath.as_ASUnicode().empty())
        ? std::wstring(L"（未保存）\n")
        : (ai::WCHARStr(docPath).as_LPCWSTR() + std::wstring(L"\n"));
    log += L"文档名称：";
    log += (docName.as_ASUnicode().empty())
        ? std::wstring(L"（未知）\n")
        : (ai::WCHARStr(docName).as_LPCWSTR() + std::wstring(L"\n"));
    log += L"目标文件夹：\n  " + base + L"\n\n";

    // 1. What the document referenced (all fonts / links found).
    log += L"----------------------------------------------------\n";
    log += L" 一、文档中查找到的资源\n";
    log += L"----------------------------------------------------\n";
    int fontIdx = 0;
    int linkIdx = 0;
    for (size_t i = 0; i < fAssets.size(); ++i)
    {
        const AssetEntry& entry = fAssets[i];
        if (entry.kind == AssetEntry::Font)
        {
            ++fontIdx;
            log += L"\n[字体 " + std::to_wstring(fontIdx) + L"] " + std::wstring(ai::WCHARStr(entry.name).as_LPCWSTR());
            if (!entry.sub.as_ASUnicode().empty())
                log += std::wstring(L"  ") + ai::WCHARStr(entry.sub).as_LPCWSTR();
            log += L"\n";
            for (size_t k = 0; k < entry.filePaths.size(); ++k)
                log += L"    文件：" + std::wstring(ai::WCHARStr(entry.filePaths[k]).as_LPCWSTR()) + L"\n";
        }
        else if (entry.kind == AssetEntry::Link)
        {
            ++linkIdx;
            log += L"\n[链接 " + std::to_wstring(linkIdx) + L"] " + std::wstring(ai::WCHARStr(entry.name).as_LPCWSTR());
            if (!entry.sub.as_ASUnicode().empty())
                log += std::wstring(L"  （") + ai::WCHARStr(entry.sub).as_LPCWSTR() + L"）";
            log += L"\n";
            if (!entry.location.as_ASUnicode().empty())
                log += L"    所在目录：" + std::wstring(ai::WCHARStr(entry.location).as_LPCWSTR()) + L"\n";
            for (size_t k = 0; k < entry.filePaths.size(); ++k)
                log += L"    文件：" + std::wstring(ai::WCHARStr(entry.filePaths[k]).as_LPCWSTR()) + L"\n";
        }
    }
    if (fontIdx == 0 && linkIdx == 0)
        log += L"\n（未查找到字体或链接资源）\n";

    // 2. Copy results per category.
    log += L"\n----------------------------------------------------\n";
    log += L" 二、收集结果\n";
    log += L"----------------------------------------------------\n";
    auto summary = [](const CopyResult& r) {
        std::wstring s = L"成功 " + std::to_wstring(r.ok);
        if (r.renamed > 0)
            s += L"，改名 " + std::to_wstring(r.renamed);
        if (r.deduped > 0)
            s += L"，去重 " + std::to_wstring(r.deduped);
        if (r.skipped > 0)
            s += L"，跳过 " + std::to_wstring(r.skipped);
        if (r.failed > 0)
            s += L"，失败 " + std::to_wstring(r.failed);
        return s;
    };
    log += L"\n[字体] " + summary(rf) + L"\n";
    log += L"[链接] " + summary(rl) + L"\n";
    log += L"[文档] " + summary(rd) + L"\n";

    // 3. Operation log (one line per file).
    log += L"\n----------------------------------------------------\n";
    log += L" 三、操作明细\n";
    log += L"----------------------------------------------------\n";
    auto detail = [](const std::vector<std::wstring>& lines, const std::wstring& label) {
        std::wstring s;
        for (size_t i = 0; i < lines.size(); ++i)
            s += L"\n[" + label + L"] " + lines[i];
        return s;
    };
    log += detail(rf.logLines, L"字体");
    log += detail(rl.logLines, L"链接");
    if (rd.ok > 0 || rd.failed > 0)
        log += L"\n[文档] " + std::wstring(ai::WCHARStr(GetDocumentFilePath()).as_LPCWSTR())
            + L" 已复制到 " + base;

    // 4. Renamed files (old -> new) for easy tracking.
    if (!rf.renameNotes.empty() || !rl.renameNotes.empty())
    {
        log += L"\n\n----------------------------------------------------\n";
        log += L" 四、改名记录\n";
        log += L"----------------------------------------------------\n";
        for (size_t i = 0; i < rf.renameNotes.size(); ++i)
            log += L"\n[字体] " + rf.renameNotes[i];
        for (size_t i = 0; i < rl.renameNotes.size(); ++i)
            log += L"\n[链接] " + rl.renameNotes[i];
    }

    log += L"\n\n====================================================\n";
    log += L" 结束\n";
    log += L"====================================================\n";

    std::wstring logPath = base + L"\\AssetCollector_收集日志.txt";
    HANDLE hFile = CreateFileW(logPath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return;
    std::string utf8 = ToUtf8(log);
    DWORD written = 0;
    WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    CloseHandle(hFile);
}

void AssetCollectorPlugin::CopyFontEntries()
{
    CopyResult r = CopyToSubdir(AssetEntry::Font, L"fonts", false);
    if (r.fatal)
        return;
    if (r.nothing)
    {
        if (sAIUser)
            sAIUser->MessageAlert(ai::UnicodeString(L"当前文档没有字体，请先点击“刷新资源”。"));
        return;
    }
    std::wstring base = GetTargetBase();
    if (!base.empty())
        WriteCollectLog(base, r, CopyResult(), CopyResult());
    ShowCopyResult(ai::UnicodeString(L"字体"), r, L"fonts");
}

void AssetCollectorPlugin::CopyLinkEntries()
{
    CopyResult r = CopyToSubdir(AssetEntry::Link, L"links", false);
    if (r.fatal)
        return;
    if (r.nothing)
    {
        if (sAIUser)
            sAIUser->MessageAlert(ai::UnicodeString(L"当前文档没有链接，请先点击“刷新资源”。"));
        return;
    }
    std::wstring base = GetTargetBase();
    if (!base.empty())
        WriteCollectLog(base, CopyResult(), r, CopyResult());
    ShowCopyResult(ai::UnicodeString(L"链接"), r, L"links");
}

void AssetCollectorPlugin::PackAll()
{
    CopyResult rf = CopyToSubdir(AssetEntry::Font, L"fonts", false);
    CopyResult rl = CopyToSubdir(AssetEntry::Link, L"links", false);
    if (rf.fatal || rl.fatal)
        return;

    // Include the document file itself (only when it has been saved).
    CopyResult rd;
    ai::UnicodeString docPath = GetDocumentFilePath();
    if (!docPath.as_ASUnicode().empty())
    {
        std::wstring base = GetTargetBase();
        if (!base.empty())
        {
            ai::FilePath srcFp(docPath);
            ai::WCHARStr wsFileName(srcFp.GetFileName());
            ai::WCHARStr wsSrc(docPath);
            std::wstring dst = base + L"\\" + wsFileName.as_LPCWSTR();
            if (CopyFileW(wsSrc.as_LPCWSTR(), dst.c_str(), FALSE))
                ++rd.ok;
            else if (GetLastError() == ERROR_FILE_EXISTS)
                ++rd.skipped;
            else
                ++rd.failed;
        }
    }
    ShowPackResult(rf, rl, rd);
    std::wstring base = GetTargetBase();
    if (!base.empty())
        WriteCollectLog(base, rf, rl, rd);
}

void AssetCollectorPlugin::PackSelected()
{
    CopyResult rf = CopyToSubdir(AssetEntry::Font, L"fonts", true);
    CopyResult rl = CopyToSubdir(AssetEntry::Link, L"links", true);
    if (rf.fatal || rl.fatal)
        return;
    if (rf.needSelect && rl.needSelect)
    {
        if (sAIUser)
            sAIUser->MessageAlert(ai::UnicodeString(L"请先在“选择”列勾选要打包的资源。"));
        return;
    }
    CopyResult rd;
    ShowPackResult(rf, rl, rd);
    std::wstring base = GetTargetBase();
    if (!base.empty())
        WriteCollectLog(base, rf, rl, rd);
}