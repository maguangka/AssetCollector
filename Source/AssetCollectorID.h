//========================================================================================
//
//  AssetCollectorID.h - Control and plugin identifiers
//
//========================================================================================
#ifndef __AssetCollectorID_H__
#define __AssetCollectorID_H__

// Plugin name (also used for the .aip file name and pipl registration)
#define kAssetCollectorPluginName "AssetCollector"

// Panel controls
#define IDC_LABEL_DEST          1011
#define IDC_EDIT_DEST           1001
#define IDC_BTN_BROWSE          1002
#define IDC_BTN_REFRESH         1003
#define IDC_BTN_COPY_FONTS      1004
#define IDC_LIST_ASSETS         1005
#define IDC_STATIC_STATUS       1006
#define IDC_HDR_NAME            1007
#define IDC_HDR_TYPE            1008
#define IDC_HDR_FILE            1009
#define IDC_HDR_LOC             1010
#define IDC_BTN_COPY_FONTS_SEL  1012
#define IDC_HDR_CHK             1013
#define IDC_BTN_COPY_LINKS      1014
#define IDC_BTN_COPY_LINKS_SEL  1015
#define IDC_BTN_PACK_ALL        1016
#define IDC_BTN_PACK_SEL        1017

// Flyout menu items
#define kACFlyoutRefresh        1
#define kACFlyoutPackAll        2
#define kACFlyoutPackSel        3
#define kACFlyoutAbout          4

// Preference keys (persisted in the Illustrator preferences)
#define kACPrefPrefix           "AssetCollector"
#define kACDestFolderSuffix     "DestFolder"

#endif // __AssetCollectorID_H__