/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include "tools/debug.hxx"
#include "tools/diagnose_ex.h"
#include "tools/rc.h"
#include "tools/stream.hxx"

#include "vcl/svapp.hxx"
#include "vcl/mnemonic.hxx"
#include "vcl/image.hxx"
#include "vcl/event.hxx"
#include "vcl/help.hxx"
#include "vcl/floatwin.hxx"
#include "vcl/wrkwin.hxx"
#include "vcl/timer.hxx"
#include "vcl/decoview.hxx"
#include "vcl/bitmap.hxx"
#include "vcl/menu.hxx"
#include "vcl/button.hxx"
#include "vcl/gradient.hxx"
#include "vcl/i18nhelp.hxx"
#include "vcl/taskpanelist.hxx"
#include "vcl/controllayout.hxx"
#include "vcl/toolbox.hxx"
#include "vcl/dockingarea.hxx"
#include "vcl/settings.hxx"

#include "salinst.hxx"
#include "svdata.hxx"
#include "svids.hrc"
#include "window.h"
#include "salmenu.hxx"
#include "salframe.hxx"

#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/i18n/XCharacterClassification.hpp>
#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <vcl/unowrap.hxx>

#include <vcl/unohelp.hxx>
#include <vcl/configsettings.hxx>

#include "vcl/lazydelete.hxx"

#include <map>
#include <vector>

namespace vcl
{

struct MenuLayoutData : public ControlLayoutData
{
    std::vector< sal_uInt16 >               m_aLineItemIds;
    std::vector< sal_uInt16 >               m_aLineItemPositions;
    std::map< sal_uInt16, Rectangle >       m_aVisibleItemBoundRects;
};

}

using namespace ::com::sun::star;
using namespace vcl;

#define ITEMPOS_INVALID     0xFFFF

#define EXTRASPACEY         2
#define EXTRAITEMHEIGHT     4
#define GUTTERBORDER        8

// document closer
#define IID_DOCUMENTCLOSE 1

static bool ImplAccelDisabled()
{
    // display of accelerator strings may be suppressed via configuration
    static int nAccelDisabled = -1;

    if( nAccelDisabled == -1 )
    {
        OUString aStr =
            vcl::SettingsConfigItem::get()->
            getValue( "Menu", "SuppressAccelerators" );
        nAccelDisabled = aStr.equalsIgnoreAsciiCase("true") ? 1 : 0;
    }
    return nAccelDisabled == 1;
}

struct MenuItemData
{
    sal_uInt16      nId;                    // SV Id
    MenuItemType    eType;                  // MenuItem-Type
    MenuItemBits    nBits;                  // MenuItem-Bits
    Menu*           pSubMenu;               // Pointer to SubMenu
    Menu*           pAutoSubMenu;           // Pointer to SubMenu from Resource
    OUString        aText;                  // Menu-Text
    OUString        aHelpText;              // Help-String
    OUString        aTipHelpText;           // TipHelp-String (eg, expanded filenames)
    OUString        aCommandStr;            // CommandString
    OUString        aHelpCommandStr;        // Help command string (to reference external help)
    OString         sIdent;
    OString         aHelpId;                // Help-Id
    sal_uLong           nUserValue;             // User value
    Image           aImage;                 // Image
    KeyCode         aAccelKey;              // Accelerator-Key
    bool            bChecked;               // Checked
    bool            bEnabled;               // Enabled
    bool            bVisible;               // Visible (note: this flag will not override MENU_FLAG_HIDEDISABLEDENTRIES when true)
    bool            bIsTemporary;           // Temporary inserted ('No selection possible')
    bool            bMirrorMode;
    long            nItemImageAngle;
    Size            aSz;                    // only temporarily valid
    OUString        aAccessibleName;        // accessible name
    OUString        aAccessibleDescription; // accessible description

    SalMenuItem*    pSalMenuItem;           // access to native menu

    MenuItemData()
        : nId(0)
        , eType(MENUITEM_DONTKNOW)
        , nBits(0)
        , pSubMenu(NULL)
        , pAutoSubMenu(NULL)
        , nUserValue(0)
        , bChecked(false)
        , bEnabled(false)
        , bVisible(false)
        , bIsTemporary(false)
        , bMirrorMode(false)
        , nItemImageAngle(0)
        , pSalMenuItem(NULL)
    {
    }
    MenuItemData( const OUString& rStr, const Image& rImage )
        : nId(0)
        , eType(MENUITEM_DONTKNOW)
        , nBits(0)
        , pSubMenu(NULL)
        , pAutoSubMenu(NULL)
        , aText(rStr)
        , nUserValue(0)
        , aImage(rImage)
        , bChecked(false)
        , bEnabled(false)
        , bVisible(false)
        , bIsTemporary(false)
        , bMirrorMode(false)
        , nItemImageAngle(0)
        , pSalMenuItem(NULL)
    {
    }
    ~MenuItemData();
    bool HasCheck() const
    {
        return bChecked || ( nBits & ( MIB_RADIOCHECK | MIB_CHECKABLE | MIB_AUTOCHECK ) );
    }
};

MenuItemData::~MenuItemData()
{
    if( pAutoSubMenu )
    {
        ((PopupMenu*)pAutoSubMenu)->pRefAutoSubMenu = NULL;
        delete pAutoSubMenu;
        pAutoSubMenu = NULL;
    }
    if( pSalMenuItem )
        ImplGetSVData()->mpDefInst->DestroyMenuItem( pSalMenuItem );
}

class MenuItemList
{
private:
    typedef ::std::vector< MenuItemData* > MenuItemDataList_impl;
    MenuItemDataList_impl maItemList;

    uno::Reference< i18n::XCharacterClassification > xCharClass;

public:
                    MenuItemList() {}
                    ~MenuItemList();

    MenuItemData*   Insert(
                        sal_uInt16 nId,
                        MenuItemType eType,
                        MenuItemBits nBits,
                        const OUString& rStr,
                        const Image& rImage,
                        Menu* pMenu,
                        size_t nPos,
                        const OString &rIdent
                    );
    void            InsertSeparator(const OString &rIdent, size_t nPos);
    void            Remove( size_t nPos );

    MenuItemData*   GetData( sal_uInt16 nSVId, size_t& rPos ) const;
    MenuItemData*   GetData( sal_uInt16 nSVId ) const
                    {
                        size_t nTemp;
                        return GetData( nSVId, nTemp );
                    }
    MenuItemData*   GetDataFromPos( size_t nPos ) const
                    {
                        return ( nPos < maItemList.size() ) ? maItemList[ nPos ] : NULL;
                    }

    MenuItemData*   SearchItem(
                        sal_Unicode cSelectChar,
                        KeyCode aKeyCode,
                        sal_uInt16& rPos,
                        sal_uInt16& nDuplicates,
                        sal_uInt16 nCurrentPos
                    ) const;
    size_t          GetItemCount( sal_Unicode cSelectChar ) const;
    size_t          GetItemCount( KeyCode aKeyCode ) const;
    size_t          size()
                    {
                        return maItemList.size();
                    }
};

MenuItemList::~MenuItemList()
{
    for( size_t i = 0, n = maItemList.size(); i < n; ++i )
        delete maItemList[ i ];
}

MenuItemData* MenuItemList::Insert(
    sal_uInt16 nId,
    MenuItemType eType,
    MenuItemBits nBits,
    const OUString& rStr,
    const Image& rImage,
    Menu* pMenu,
    size_t nPos,
    const OString &rIdent
)
{
    MenuItemData* pData     = new MenuItemData( rStr, rImage );
    pData->nId              = nId;
    pData->sIdent           = rIdent;
    pData->eType            = eType;
    pData->nBits            = nBits;
    pData->pSubMenu         = NULL;
    pData->pAutoSubMenu     = NULL;
    pData->nUserValue       = 0;
    pData->bChecked         = false;
    pData->bEnabled         = true;
    pData->bVisible         = true;
    pData->bIsTemporary     = false;
    pData->bMirrorMode      = false;
    pData->nItemImageAngle  = 0;

    SalItemParams aSalMIData;
    aSalMIData.nId = nId;
    aSalMIData.eType = eType;
    aSalMIData.nBits = nBits;
    aSalMIData.pMenu = pMenu;
    aSalMIData.aText = rStr;
    aSalMIData.aImage = rImage;

    // Native-support: returns NULL if not supported
    pData->pSalMenuItem = ImplGetSVData()->mpDefInst->CreateMenuItem( &aSalMIData );

    if( nPos < maItemList.size() ) {
        maItemList.insert( maItemList.begin() + nPos, pData );
    } else {
        maItemList.push_back( pData );
    }
    return pData;
}

void MenuItemList::InsertSeparator(const OString &rIdent, size_t nPos)
{
    MenuItemData* pData     = new MenuItemData;
    pData->nId              = 0;
    pData->sIdent           = rIdent;
    pData->eType            = MENUITEM_SEPARATOR;
    pData->nBits            = 0;
    pData->pSubMenu         = NULL;
    pData->pAutoSubMenu     = NULL;
    pData->nUserValue       = 0;
    pData->bChecked         = false;
    pData->bEnabled         = true;
    pData->bVisible         = true;
    pData->bIsTemporary     = false;
    pData->bMirrorMode      = false;
    pData->nItemImageAngle  = 0;

    SalItemParams aSalMIData;
    aSalMIData.nId = 0;
    aSalMIData.eType = MENUITEM_SEPARATOR;
    aSalMIData.nBits = 0;
    aSalMIData.pMenu = NULL;
    aSalMIData.aText = OUString();
    aSalMIData.aImage = Image();

    // Native-support: returns NULL if not supported
    pData->pSalMenuItem = ImplGetSVData()->mpDefInst->CreateMenuItem( &aSalMIData );

    if( nPos < maItemList.size() ) {
        maItemList.insert( maItemList.begin() + nPos, pData );
    } else {
        maItemList.push_back( pData );
    }
}

void MenuItemList::Remove( size_t nPos )
{
    if( nPos < maItemList.size() )
    {
        delete maItemList[ nPos ];
        maItemList.erase( maItemList.begin() + nPos );
    }
}

MenuItemData* MenuItemList::GetData( sal_uInt16 nSVId, size_t& rPos ) const
{
    for( size_t i = 0, n = maItemList.size(); i < n; ++i )
    {
        if ( maItemList[ i ]->nId == nSVId )
        {
            rPos = i;
            return maItemList[ i ];
        }
    }
    return NULL;
}

MenuItemData* MenuItemList::SearchItem(
    sal_Unicode cSelectChar,
    KeyCode aKeyCode,
    sal_uInt16& rPos,
    sal_uInt16& nDuplicates,
    sal_uInt16 nCurrentPos
) const
{
    const vcl::I18nHelper& rI18nHelper = Application::GetSettings().GetUILocaleI18nHelper();

    size_t nListCount = maItemList.size();

    // try character code first
    nDuplicates = GetItemCount( cSelectChar );  // return number of duplicates
    if( nDuplicates )
    {
        for ( rPos = 0; rPos < nListCount; rPos++)
        {
            MenuItemData* pData = maItemList[ rPos ];
            if ( pData->bEnabled && rI18nHelper.MatchMnemonic( pData->aText, cSelectChar ) )
            {
                if( nDuplicates > 1 && rPos == nCurrentPos )
                    continue;   // select next entry with the same mnemonic
                else
                    return pData;
            }
        }
    }

    // nothing found, try keycode instead
    nDuplicates = GetItemCount( aKeyCode ); // return number of duplicates

    if( nDuplicates )
    {
        char ascii = 0;
        if( aKeyCode.GetCode() >= KEY_A && aKeyCode.GetCode() <= KEY_Z )
            ascii = sal::static_int_cast<char>('A' + (aKeyCode.GetCode() - KEY_A));

        for ( rPos = 0; rPos < nListCount; rPos++)
        {
            MenuItemData* pData = maItemList[ rPos ];
            if ( pData->bEnabled )
            {
                sal_Int32 n = pData->aText.indexOf('~');
                if ( n != -1 )
                {
                    KeyCode mnKeyCode;
                    sal_Unicode mnUnicode = pData->aText[n+1];
                    Window* pDefWindow = ImplGetDefaultWindow();
                    if(  (  pDefWindow
                         && pDefWindow->ImplGetFrame()->MapUnicodeToKeyCode( mnUnicode,
                             Application::GetSettings().GetUILanguageTag().getLanguageType(), mnKeyCode )
                         && aKeyCode.GetCode() == mnKeyCode.GetCode()
                         )
                      || (  ascii
                         && rI18nHelper.MatchMnemonic( pData->aText, ascii )
                         )
                      )
                    {
                        if( nDuplicates > 1 && rPos == nCurrentPos )
                            continue;   // select next entry with the same mnemonic
                        else
                            return pData;
                    }
                }
            }
        }
    }

    return NULL;
}

size_t MenuItemList::GetItemCount( sal_Unicode cSelectChar ) const
{
    // returns number of entries with same mnemonic
    const vcl::I18nHelper& rI18nHelper = Application::GetSettings().GetUILocaleI18nHelper();

    size_t nItems = 0;
    for ( size_t nPos = maItemList.size(); nPos; )
    {
        MenuItemData* pData = maItemList[ --nPos ];
        if ( pData->bEnabled && rI18nHelper.MatchMnemonic( pData->aText, cSelectChar ) )
            nItems++;
    }

    return nItems;
}

size_t MenuItemList::GetItemCount( KeyCode aKeyCode ) const
{
    // returns number of entries with same mnemonic
    // uses key codes instead of character codes
    const vcl::I18nHelper& rI18nHelper = Application::GetSettings().GetUILocaleI18nHelper();
    char ascii = 0;
    if( aKeyCode.GetCode() >= KEY_A && aKeyCode.GetCode() <= KEY_Z )
        ascii = sal::static_int_cast<char>('A' + (aKeyCode.GetCode() - KEY_A));

    size_t nItems = 0;
    for ( size_t nPos = maItemList.size(); nPos; )
    {
        MenuItemData* pData = maItemList[ --nPos ];
        if ( pData->bEnabled )
        {
            sal_Int32 n = pData->aText.indexOf('~');
            if (n != -1)
            {
                KeyCode mnKeyCode;
                // if MapUnicodeToKeyCode fails or is unsupported we try the pure ascii mapping of the keycodes
                // so we have working shortcuts when ascii mnemonics are used
                Window* pDefWindow = ImplGetDefaultWindow();
                if(  (  pDefWindow
                     && pDefWindow->ImplGetFrame()->MapUnicodeToKeyCode( pData->aText[n+1],
                         Application::GetSettings().GetUILanguageTag().getLanguageType(), mnKeyCode )
                     && aKeyCode.GetCode() == mnKeyCode.GetCode()
                     )
                  || (  ascii
                     && rI18nHelper.MatchMnemonic( pData->aText, ascii )
                     )
                  )
                    nItems++;
            }
        }
    }

    return nItems;
}

// - MenuFloatingWindow -

class MenuFloatingWindow : public FloatingWindow
{
    friend void Menu::ImplFillLayoutData() const;
    friend Menu::~Menu();

private:
    Menu*           pMenu;
    PopupMenu*      pActivePopup;
    Timer           aHighlightChangedTimer;
    Timer           aSubmenuCloseTimer;
    Timer           aScrollTimer;
    sal_uLong           nSaveFocusId;
    sal_uInt16          nHighlightedItem;       // highlighted/selected Item
    sal_uInt16          nMBDownPos;
    sal_uInt16          nScrollerHeight;
    sal_uInt16          nFirstEntry;
    sal_uInt16          nBorder;
    sal_uInt16          nPosInParent;
    bool            bInExecute;

    bool            bScrollMenu;
    bool            bScrollUp;
    bool            bScrollDown;
    bool            bIgnoreFirstMove;
    bool            bKeyInput;

                    DECL_LINK(PopupEnd, void *);
                    DECL_LINK( HighlightChanged, Timer* );
                    DECL_LINK(SubmenuClose, void *);
                    DECL_LINK(AutoScroll, void *);
                    DECL_LINK( ShowHideListener, VclWindowEvent* );

    virtual void    StateChanged( StateChangedType nType ) SAL_OVERRIDE;
    virtual void    DataChanged( const DataChangedEvent& rDCEvt ) SAL_OVERRIDE;

    void            InitMenuClipRegion();

protected:
    Region          ImplCalcClipRegion( bool bIncludeLogo = true ) const;
    void            ImplDrawScroller( bool bUp );
    using Window::ImplScroll;
    void            ImplScroll( const Point& rMousePos );
    void            ImplScroll( bool bUp );
    void            ImplCursorUpDown( bool bUp, bool bHomeEnd = false );
    void            ImplHighlightItem( const MouseEvent& rMEvt, bool bMBDown );
    long            ImplGetStartY() const;
    Rectangle       ImplGetItemRect( sal_uInt16 nPos );

public:
                    MenuFloatingWindow( Menu* pMenu, Window* pParent, WinBits nStyle );
                    virtual ~MenuFloatingWindow();

            void    doShutdown();

    virtual void    MouseMove( const MouseEvent& rMEvt ) SAL_OVERRIDE;
    virtual void    MouseButtonDown( const MouseEvent& rMEvt ) SAL_OVERRIDE;
    virtual void    MouseButtonUp( const MouseEvent& rMEvt ) SAL_OVERRIDE;
    virtual void    KeyInput( const KeyEvent& rKEvent ) SAL_OVERRIDE;
    virtual void    Command( const CommandEvent& rCEvt ) SAL_OVERRIDE;
    virtual void    Paint( const Rectangle& rRect ) SAL_OVERRIDE;
    virtual void    RequestHelp( const HelpEvent& rHEvt ) SAL_OVERRIDE;
    virtual void    Resize() SAL_OVERRIDE;

    void            SetFocusId( sal_uLong nId ) { nSaveFocusId = nId; }
    sal_uLong           GetFocusId() const      { return nSaveFocusId; }

    void            EnableScrollMenu( bool b );
    bool            IsScrollMenu() const        { return bScrollMenu; }
    sal_uInt16          GetScrollerHeight() const   { return nScrollerHeight; }

    void            Execute();
    void            StopExecute( sal_uLong nFocusId = 0 );
    void            EndExecute();
    void            EndExecute( sal_uInt16 nSelectId );

    PopupMenu*      GetActivePopup() const  { return pActivePopup; }
    void            KillActivePopup( PopupMenu* pThisOnly = NULL );

    void            HighlightItem( sal_uInt16 nPos, bool bHighlight );
    void            ChangeHighlightItem( sal_uInt16 n, bool bStartPopupTimer );
    sal_uInt16          GetHighlightedItem() const { return nHighlightedItem; }

    void            SetPosInParent( sal_uInt16 nPos ) { nPosInParent = nPos; }

    virtual ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > CreateAccessible() SAL_OVERRIDE;
};

// To get the transparent mouse-over look, the closer is actually a toolbox
// overload DataChange to handle style changes correctly
class DecoToolBox : public ToolBox
{
    long lastSize;
    Size maMinSize;

    using Window::ImplInit;
public:
            DecoToolBox( Window* pParent, WinBits nStyle = 0 );
    void    ImplInit();

    void    DataChanged( const DataChangedEvent& rDCEvt ) SAL_OVERRIDE;

    void    SetImages( long nMaxHeight = 0, bool bForce = false );

    void    calcMinSize();
    const Size& getMinSize() { return maMinSize;}

    Image   maImage;
};

DecoToolBox::DecoToolBox( Window* pParent, WinBits nStyle ) :
    ToolBox( pParent, nStyle )
{
    ImplInit();
}

void DecoToolBox::ImplInit()
{
    lastSize = -1;
    calcMinSize();
}

void DecoToolBox::DataChanged( const DataChangedEvent& rDCEvt )
{
    Window::DataChanged( rDCEvt );

    if ( rDCEvt.GetFlags() & SETTINGS_STYLE )
    {
        calcMinSize();
        SetBackground();
        SetImages( 0, true);
    }
}

void DecoToolBox::calcMinSize()
{
    ToolBox aTbx( GetParent() );
    if( GetItemCount() == 0 )
    {
        ResMgr* pResMgr = ImplGetResMgr();

        Bitmap aBitmap;
        if( pResMgr )
            aBitmap = Bitmap( ResId( SV_RESID_BITMAP_CLOSEDOC, *pResMgr ) );
        aTbx.InsertItem( IID_DOCUMENTCLOSE, Image( aBitmap ) );
    }
    else
    {
        sal_uInt16 nItems = GetItemCount();
        for( sal_uInt16 i = 0; i < nItems; i++ )
        {
            sal_uInt16 nId = GetItemId( i );
            aTbx.InsertItem( nId, GetItemImage( nId ) );
        }
    }
    aTbx.SetOutStyle( TOOLBOX_STYLE_FLAT );
    maMinSize = aTbx.CalcWindowSizePixel();
}


void DecoToolBox::SetImages( long nMaxHeight, bool bForce )
{
    long border = getMinSize().Height() - maImage.GetSizePixel().Height();

    if( !nMaxHeight && lastSize != -1 )
        nMaxHeight = lastSize + border; // don't change anything if called with 0

    if( nMaxHeight < getMinSize().Height() )
        nMaxHeight = getMinSize().Height();

    if( (lastSize != nMaxHeight - border) || bForce )
    {
        lastSize = nMaxHeight - border;

        Color       aEraseColor( 255, 255, 255, 255 );
        BitmapEx    aBmpExDst( maImage.GetBitmapEx() );
        BitmapEx    aBmpExSrc( aBmpExDst );

        aEraseColor.SetTransparency( 255 );
        aBmpExDst.Erase( aEraseColor );
        aBmpExDst.SetSizePixel( Size( lastSize, lastSize ) );

        Rectangle aSrcRect( Point(0,0), maImage.GetSizePixel() );
        Rectangle aDestRect( Point((lastSize - maImage.GetSizePixel().Width())/2,
                                (lastSize - maImage.GetSizePixel().Height())/2 ),
                            maImage.GetSizePixel() );

        aBmpExDst.CopyPixel( aDestRect, aSrcRect, &aBmpExSrc );
        SetItemImage( IID_DOCUMENTCLOSE, Image( aBmpExDst ) );
    }
}

// a basic class for both (due to pActivePopup, Timer,...) would be nice,
// but a container class should have been created then, as they
// would be derived from different windows
// In most functions we would have to create exceptions for
// menubar, popupmenu, hence we made two classes

class MenuBarWindow : public Window
{
    friend class MenuBar;
    friend class Menu;

private:
    struct AddButtonEntry
    {
        sal_uInt16      m_nId;
        Link        m_aSelectLink;
        Link        m_aHighlightLink;

        AddButtonEntry() : m_nId( 0 ) {}
    };

    Menu*           pMenu;
    PopupMenu*      pActivePopup;
    sal_uInt16          nHighlightedItem;
    sal_uInt16          nRolloveredItem;
    sal_uLong           nSaveFocusId;
    bool            mbAutoPopup;
    bool            bIgnoreFirstMove;
    bool            bStayActive;

    DecoToolBox     aCloser;
    PushButton      aFloatBtn;
    PushButton      aHideBtn;

    std::map< sal_uInt16, AddButtonEntry > m_aAddButtons;

    void            HighlightItem( sal_uInt16 nPos, bool bHighlight );
    void            ChangeHighlightItem( sal_uInt16 n, bool bSelectPopupEntry, bool bAllowRestoreFocus = true, bool bDefaultToDocument = true );

    sal_uInt16          ImplFindEntry( const Point& rMousePos ) const;
    void            ImplCreatePopup( bool bPreSelectFirst );
    bool            ImplHandleKeyEvent( const KeyEvent& rKEvent, bool bFromMenu = true );
    Rectangle       ImplGetItemRect( sal_uInt16 nPos );

    void            ImplInitStyleSettings();

                    DECL_LINK(CloserHdl, void *);
                    DECL_LINK(FloatHdl, void *);
                    DECL_LINK(HideHdl, void *);
                    DECL_LINK( ToolboxEventHdl, VclWindowEvent* );
                    DECL_LINK( ShowHideListener, VclWindowEvent* );

    void            StateChanged( StateChangedType nType ) SAL_OVERRIDE;
    void            DataChanged( const DataChangedEvent& rDCEvt ) SAL_OVERRIDE;
    void            LoseFocus() SAL_OVERRIDE;
    void            GetFocus() SAL_OVERRIDE;

public:
                    MenuBarWindow( Window* pParent );
                    virtual ~MenuBarWindow();

    void            ShowButtons( bool bClose, bool bFloat, bool bHide );

    virtual void    MouseMove( const MouseEvent& rMEvt ) SAL_OVERRIDE;
    virtual void    MouseButtonDown( const MouseEvent& rMEvt ) SAL_OVERRIDE;
    virtual void    MouseButtonUp( const MouseEvent& rMEvt ) SAL_OVERRIDE;
    virtual void    KeyInput( const KeyEvent& rKEvent ) SAL_OVERRIDE;
    virtual void    Paint( const Rectangle& rRect ) SAL_OVERRIDE;
    virtual void    Resize() SAL_OVERRIDE;
    virtual void    RequestHelp( const HelpEvent& rHEvt ) SAL_OVERRIDE;

    void            SetFocusId( sal_uLong nId ) { nSaveFocusId = nId; }
    sal_uLong           GetFocusId() const { return nSaveFocusId; }

    void            SetMenu( MenuBar* pMenu );
    void            KillActivePopup();
    void            PopupClosed( Menu* pMenu );
    sal_uInt16          GetHighlightedItem() const { return nHighlightedItem; }
    virtual ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > CreateAccessible() SAL_OVERRIDE;

    void SetAutoPopup( bool bAuto ) { mbAutoPopup = bAuto; }
    void            ImplLayoutChanged();
    Size            MinCloseButtonSize();

    // add an arbitrary button to the menubar (will appear next to closer)
    sal_uInt16              AddMenuBarButton( const Image&, const Link&, const OUString&, sal_uInt16 nPos );
    void                SetMenuBarButtonHighlightHdl( sal_uInt16 nId, const Link& );
    Rectangle           GetMenuBarButtonRectPixel( sal_uInt16 nId );
    void                RemoveMenuBarButton( sal_uInt16 nId );
    bool                HandleMenuButtonEvent( sal_uInt16 i_nButtonId );
};

static void ImplAddNWFSeparator( Window *pThis, const MenubarValue& rMenubarValue )
{
    // add a separator if
    // - we have an adjacent docking area
    // - and if toolbars would draw them as well (mbDockingAreaSeparateTB must not be set, see dockingarea.cxx)
    if( rMenubarValue.maTopDockingAreaHeight && !ImplGetSVData()->maNWFData.mbDockingAreaSeparateTB && !ImplGetSVData()->maNWFData.mbDockingAreaAvoidTBFrames )
    {
        // note: the menubar only provides the upper (dark) half of it, the rest (bright part) is drawn by the docking area

        pThis->SetLineColor( pThis->GetSettings().GetStyleSettings().GetSeparatorColor() );
        Point aPt;
        Rectangle aRect( aPt, pThis->GetOutputSizePixel() );
        pThis->DrawLine( aRect.BottomLeft(), aRect.BottomRight() );
    }
}

static void ImplSetMenuItemData( MenuItemData* pData )
{
    // convert data
    if ( !pData->aImage )
        pData->eType = MENUITEM_STRING;
    else if ( pData->aText.isEmpty() )
        pData->eType = MENUITEM_IMAGE;
    else
        pData->eType = MENUITEM_STRINGIMAGE;
}

static sal_uLong ImplChangeTipTimeout( sal_uLong nTimeout, Window *pWindow )
{
       AllSettings aAllSettings( pWindow->GetSettings() );
       HelpSettings aHelpSettings( aAllSettings.GetHelpSettings() );
       sal_uLong nRet = aHelpSettings.GetTipTimeout();
       aHelpSettings.SetTipTimeout( nTimeout );
       aAllSettings.SetHelpSettings( aHelpSettings );
       pWindow->SetSettings( aAllSettings );
       return nRet;
}

static bool ImplHandleHelpEvent( Window* pMenuWindow, Menu* pMenu, sal_uInt16 nHighlightedItem, const HelpEvent& rHEvt, const Rectangle &rHighlightRect )
{
    if( ! pMenu )
        return false;

    bool bDone = false;
    sal_uInt16 nId = 0;

    if ( nHighlightedItem != ITEMPOS_INVALID )
    {
        MenuItemData* pItemData = pMenu->GetItemList()->GetDataFromPos( nHighlightedItem );
        if ( pItemData )
            nId = pItemData->nId;
    }

    if ( ( rHEvt.GetMode() & HELPMODE_BALLOON ) && pMenuWindow )
    {
        Point aPos;
        if( rHEvt.KeyboardActivated() )
            aPos = rHighlightRect.Center();
        else
            aPos = rHEvt.GetMousePosPixel();

        Rectangle aRect( aPos, Size() );
        if (!pMenu->GetHelpText(nId).isEmpty())
            Help::ShowBalloon( pMenuWindow, aPos, pMenu->GetHelpText( nId ) );
        else
        {
            // give user a chance to read the full filename
            sal_uLong oldTimeout=ImplChangeTipTimeout( 60000, pMenuWindow );
            // call always, even when strlen==0 to correctly remove tip
            Help::ShowQuickHelp( pMenuWindow, aRect, pMenu->GetTipHelpText( nId ) );
            ImplChangeTipTimeout( oldTimeout, pMenuWindow );
        }
        bDone = true;
    }
    else if ( ( rHEvt.GetMode() & HELPMODE_QUICK ) && pMenuWindow )
    {
        Point aPos = rHEvt.GetMousePosPixel();
        Rectangle aRect( aPos, Size() );
        // give user a chance to read the full filename
        sal_uLong oldTimeout=ImplChangeTipTimeout( 60000, pMenuWindow );
        // call always, even when strlen==0 to correctly remove tip
        Help::ShowQuickHelp( pMenuWindow, aRect, pMenu->GetTipHelpText( nId ) );
        ImplChangeTipTimeout( oldTimeout, pMenuWindow );
        bDone = true;
    }
    else if ( rHEvt.GetMode() & (HELPMODE_CONTEXT | HELPMODE_EXTENDED) )
    {
        // is help in the application selected
        Help* pHelp = Application::GetHelp();
        if ( pHelp )
        {
            // is an id available, then call help with the id, otherwise
            // use help-index
            OUString aCommand = pMenu->GetItemCommand( nId );
            OString aHelpId(  pMenu->GetHelpId( nId ) );
            if( aHelpId.isEmpty() )
                aHelpId = OOO_HELP_INDEX;

            if ( !aCommand.isEmpty() )
                pHelp->Start( aCommand, NULL );
            else
                pHelp->Start( OStringToOUString( aHelpId, RTL_TEXTENCODING_UTF8 ), NULL );
        }
        bDone = true;
    }
    return bDone;
}

static int ImplGetTopDockingAreaHeight( Window *pWindow )
{
    // find docking area that is top aligned and return its height
    // note: dockingareas are direct children of the SystemWindow
    if( pWindow->ImplGetFrameWindow() )
    {
        Window *pWin = pWindow->ImplGetFrameWindow()->GetWindow( WINDOW_FIRSTCHILD ); //mpWindowImpl->mpFirstChild;
        while( pWin )
        {
            if( pWin->IsSystemWindow() )
            {
                Window *pChildWin = pWin->GetWindow( WINDOW_FIRSTCHILD ); //mpWindowImpl->mpFirstChild;
                while( pChildWin )
                {
                    DockingAreaWindow *pDockingArea = NULL;
                    if ( pChildWin->GetType() == WINDOW_DOCKINGAREA )
                        pDockingArea = static_cast< DockingAreaWindow* >( pChildWin );

                    if( pDockingArea && pDockingArea->GetAlign() == WINDOWALIGN_TOP &&
                        pDockingArea->IsVisible() && pDockingArea->GetOutputSizePixel().Height() != 0 )
                    {
                        return pDockingArea->GetOutputSizePixel().Height();
                    }

                    pChildWin = pChildWin->GetWindow( WINDOW_NEXT ); //mpWindowImpl->mpNext;
                }

            }

            pWin = pWin->GetWindow( WINDOW_NEXT ); //mpWindowImpl->mpNext;
        }
    }
    return 0;
}

Menu::Menu()
{
    bIsMenuBar = false;
    ImplInit();
}

// this constructor makes sure we're creating the native menu
// with the correct type (ie, MenuBar vs. PopupMenu)
Menu::Menu( bool bMenubar )
{
    bIsMenuBar = bMenubar;
    ImplInit();
}

Menu::~Menu()
{

    vcl::LazyDeletor<Menu>::Undelete( this );

    ImplCallEventListeners( VCLEVENT_OBJECT_DYING, ITEMPOS_INVALID );

    // at the window free the reference to the accessible component
    // and make sure the MenuFloatingWindow knows about our destruction
    if ( pWindow )
    {
        MenuFloatingWindow* pFloat = (MenuFloatingWindow*)pWindow;
        if( pFloat->pMenu == this )
            pFloat->pMenu = NULL;
        pWindow->SetAccessible( ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible >() );
    }

    // dispose accessible components
    if ( mxAccessible.is() )
    {
        ::com::sun::star::uno::Reference< ::com::sun::star::lang::XComponent> xComponent( mxAccessible, ::com::sun::star::uno::UNO_QUERY );
        if ( xComponent.is() )
            xComponent->dispose();
    }

    if ( nEventId )
        Application::RemoveUserEvent( nEventId );

    // Notify deletion of this menu
    ImplMenuDelData* pDelData = mpFirstDel;
    while ( pDelData )
    {
        pDelData->mpMenu = NULL;
        pDelData = pDelData->mpNext;
    }

    bKilled = true;

    delete pItemList;
    delete pLogo;
    delete mpLayoutData;

    // Native-support: destroy SalMenu
    ImplSetSalMenu( NULL );
}

void Menu::ImplInit()
{
    mnHighlightedItemPos = ITEMPOS_INVALID;
    mpSalMenu       = NULL;
    nMenuFlags      = 0;
    nDefaultItem    = 0;
    //bIsMenuBar      = false;  // this is now set in the ctor, must not be changed here!!!
    nSelectedId     = 0;
    pItemList       = new MenuItemList;
    pLogo           = NULL;
    pStartedFrom    = NULL;
    pWindow         = NULL;
    nEventId        = 0;
    bCanceled       = false;
    bInCallback     = false;
    bKilled         = false;
    mpLayoutData    = NULL;
    mpFirstDel      = NULL;         // Dtor notification list
    // Native-support: returns NULL if not supported
    mpSalMenu = ImplGetSVData()->mpDefInst->CreateMenu( bIsMenuBar, this );
}

void Menu::ImplLoadRes( const ResId& rResId )
{
    ResMgr* pMgr = rResId.GetResMgr();
    if( ! pMgr )
        return;

    rResId.SetRT( RSC_MENU );
    GetRes( rResId );

    sal_uLong nObjMask = ReadLongRes();

    if( nObjMask & RSC_MENU_ITEMS )
    {
        sal_uLong nObjFollows = ReadLongRes();
        // insert menu items
        for( sal_uLong i = 0; i < nObjFollows; i++ )
        {
            InsertItem( ResId( (RSHEADER_TYPE*)GetClassRes(), *pMgr ) );
            IncrementRes( GetObjSizeRes( (RSHEADER_TYPE*)GetClassRes() ) );
        }
    }

    if( nObjMask & RSC_MENU_TEXT )
    {
        if( bIsMenuBar ) // no title in menubar
            ReadStringRes();
        else
            aTitleText = ReadStringRes();
    }
    if( nObjMask & RSC_MENU_DEFAULTITEMID )
        SetDefaultItem( sal::static_int_cast<sal_uInt16>(ReadLongRes()) );
}

void Menu::CreateAutoMnemonics()
{
    MnemonicGenerator aMnemonicGenerator;
    size_t n;
    for ( n = 0; n < pItemList->size(); n++ )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( n );
        if ( ! (pData->nBits & MIB_NOSELECT ) )
            aMnemonicGenerator.RegisterMnemonic( pData->aText );
    }
    for ( n = 0; n < pItemList->size(); n++ )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( n );
        if ( ! (pData->nBits & MIB_NOSELECT ) )
            pData->aText = aMnemonicGenerator.CreateMnemonic( pData->aText );
    }
}

void Menu::Activate()
{
    bInCallback = true;

    ImplMenuDelData aDelData( this );

    ImplCallEventListeners( VCLEVENT_MENU_ACTIVATE, ITEMPOS_INVALID );

    if( !aDelData.isDeleted() )
    {
        if ( !aActivateHdl.Call( this ) )
        {
            if( !aDelData.isDeleted() )
            {
                Menu* pStartMenu = ImplGetStartMenu();
                if ( pStartMenu && ( pStartMenu != this ) )
                {
                    pStartMenu->bInCallback = true;
                    // MT 11/01: Call EventListener here? I don't know...
                    pStartMenu->aActivateHdl.Call( this );
                    pStartMenu->bInCallback = false;
                }
            }
        }
        bInCallback = false;
    }
}

void Menu::Deactivate()
{
    for ( size_t n = pItemList->size(); n; )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( --n );
        if ( pData->bIsTemporary )
            pItemList->Remove( n );
    }

    bInCallback = true;

    ImplMenuDelData aDelData( this );

    Menu* pStartMenu = ImplGetStartMenu();
    ImplCallEventListeners( VCLEVENT_MENU_DEACTIVATE, ITEMPOS_INVALID );

    if( !aDelData.isDeleted() )
    {
        if ( !aDeactivateHdl.Call( this ) )
        {
            if( !aDelData.isDeleted() )
            {
                if ( pStartMenu && ( pStartMenu != this ) )
                {
                    pStartMenu->bInCallback = true;
                    pStartMenu->aDeactivateHdl.Call( this );
                    pStartMenu->bInCallback = false;
                }
            }
        }
    }

    if( !aDelData.isDeleted() )
    {
        bInCallback = false;
    }
}

void Menu::Highlight()
{
    ImplMenuDelData aDelData( this );

    Menu* pStartMenu = ImplGetStartMenu();
    if ( !aHighlightHdl.Call( this ) && !aDelData.isDeleted() )
    {
        if ( pStartMenu && ( pStartMenu != this ) )
            pStartMenu->aHighlightHdl.Call( this );
    }
}

void Menu::ImplSelect()
{
    MenuItemData* pData = GetItemList()->GetData( nSelectedId );
    if ( pData && (pData->nBits & MIB_AUTOCHECK) )
    {
        bool bChecked = IsItemChecked( nSelectedId );
        if ( pData->nBits & MIB_RADIOCHECK )
        {
            if ( !bChecked )
                CheckItem( nSelectedId, true );
        }
        else
            CheckItem( nSelectedId, !bChecked );
    }

    // call select
    ImplSVData* pSVData = ImplGetSVData();
    pSVData->maAppData.mpActivePopupMenu = NULL;        // if new execute in select()
    nEventId = Application::PostUserEvent( LINK( this, Menu, ImplCallSelect ) );
}

void Menu::Select()
{
    ImplMenuDelData aDelData( this );

    ImplCallEventListeners( VCLEVENT_MENU_SELECT, GetItemPos( GetCurItemId() ) );
    if ( !aDelData.isDeleted() && !aSelectHdl.Call( this ) )
    {
        if( !aDelData.isDeleted() )
        {
            Menu* pStartMenu = ImplGetStartMenu();
            if ( pStartMenu && ( pStartMenu != this ) )
            {
                pStartMenu->nSelectedId = nSelectedId;
                pStartMenu->aSelectHdl.Call( this );
            }
        }
    }
}

#if defined(MACOSX)
void Menu::ImplSelectWithStart( Menu* pSMenu )
{
    Menu* pOldStartedFrom = pStartedFrom;
    pStartedFrom = pSMenu;
    Menu* pOldStartedStarted = pOldStartedFrom ? pOldStartedFrom->pStartedFrom : NULL;
    Select();
    if( pOldStartedFrom )
        pOldStartedFrom->pStartedFrom = pOldStartedStarted;
    pStartedFrom = pOldStartedFrom;
}
#endif

void Menu::RequestHelp( const HelpEvent& )
{
}

void Menu::ImplCallEventListeners( sal_uLong nEvent, sal_uInt16 nPos )
{
    ImplMenuDelData aDelData( this );

    VclMenuEvent aEvent( this, nEvent, nPos );

    // This is needed by atk accessibility bridge
    if ( nEvent == VCLEVENT_MENU_HIGHLIGHT )
    {
        Application::ImplCallEventListeners( &aEvent );
    }

    if ( !aDelData.isDeleted() )
        maEventListeners.Call( &aEvent );

    if( !aDelData.isDeleted() )
    {
        Menu* pMenu = this;
        while ( pMenu )
        {
            maChildEventListeners.Call( &aEvent );

            if( aDelData.isDeleted() )
                break;

            pMenu = ( pMenu->pStartedFrom != pMenu ) ? pMenu->pStartedFrom : NULL;
        }
    }
}

void Menu::AddEventListener( const Link& rEventListener )
{
    maEventListeners.addListener( rEventListener );
}

void Menu::RemoveEventListener( const Link& rEventListener )
{
    maEventListeners.removeListener( rEventListener );
}

void Menu::InsertItem(sal_uInt16 nItemId, const OUString& rStr, MenuItemBits nItemBits,
    const OString &rIdent, sal_uInt16 nPos)
{
    DBG_ASSERT( nItemId, "Menu::InsertItem(): ItemId == 0" );
    DBG_ASSERT( GetItemPos( nItemId ) == MENU_ITEM_NOTFOUND,
                "Menu::InsertItem(): ItemId already exists" );

    // if Position > ItemCount, append
    if ( nPos >= pItemList->size() )
        nPos = MENU_APPEND;

    // put Item in MenuItemList
    MenuItemData* pData = pItemList->Insert(nItemId, MENUITEM_STRING,
                             nItemBits, rStr, Image(), this, nPos, rIdent);

    // update native menu
    if( ImplGetSalMenu() && pData->pSalMenuItem )
        ImplGetSalMenu()->InsertItem( pData->pSalMenuItem, nPos );

    Window* pWin = ImplGetWindow();
    delete mpLayoutData, mpLayoutData = NULL;
    if ( pWin )
    {
        ImplCalcSize( pWin );
        if ( pWin->IsVisible() )
            pWin->Invalidate();
    }
    ImplCallEventListeners( VCLEVENT_MENU_INSERTITEM, nPos );
}

void Menu::InsertItem(sal_uInt16 nItemId, const Image& rImage,
    MenuItemBits nItemBits, const OString &rIdent, sal_uInt16 nPos)
{
    InsertItem(nItemId, OUString(), nItemBits, rIdent, nPos);
    SetItemImage( nItemId, rImage );
}

void Menu::InsertItem(sal_uInt16 nItemId, const OUString& rStr,
    const Image& rImage, MenuItemBits nItemBits,
    const OString &rIdent, sal_uInt16 nPos)
{
    InsertItem(nItemId, rStr, nItemBits, rIdent, nPos);
    SetItemImage( nItemId, rImage );
}

void Menu::InsertItem( const ResId& rResId, sal_uInt16 nPos )
{
    ResMgr* pMgr = rResId.GetResMgr();
    if( ! pMgr )
        return;

    sal_uLong              nObjMask;

    GetRes( rResId.SetRT( RSC_MENUITEM ) );
    nObjMask    = ReadLongRes();

    bool bSep = false;
    if ( nObjMask & RSC_MENUITEM_SEPARATOR )
        bSep = ReadShortRes() != 0;

    sal_uInt16 nItemId = 1;
    if ( nObjMask & RSC_MENUITEM_ID )
        nItemId = sal::static_int_cast<sal_uInt16>(ReadLongRes());

    MenuItemBits nStatus = 0;
    if ( nObjMask & RSC_MENUITEM_STATUS )
        nStatus = sal::static_int_cast<MenuItemBits>(ReadLongRes());

    OUString aText;
    if ( nObjMask & RSC_MENUITEM_TEXT )
        aText = ReadStringRes();

    // create item
    if ( nObjMask & RSC_MENUITEM_BITMAP )
    {
        if ( !bSep )
        {
            Bitmap aBmp( ResId( (RSHEADER_TYPE*)GetClassRes(), *pMgr ) );
            Image const aImg(aBmp);
            if ( !aText.isEmpty() )
                InsertItem( nItemId, aText, aImg, nStatus, OString(), nPos );
            else
                InsertItem( nItemId, aImg, nStatus, OString(), nPos );
        }
        IncrementRes( GetObjSizeRes( (RSHEADER_TYPE*)GetClassRes() ) );
    }
    else if ( !bSep )
        InsertItem(nItemId, aText, nStatus, OString(), nPos);
    if ( bSep )
        InsertSeparator(OString(), nPos);

    OUString aHelpText;
    if ( nObjMask & RSC_MENUITEM_HELPTEXT )
    {
        aHelpText = ReadStringRes();
        if( !bSep )
            SetHelpText( nItemId, aHelpText );
    }

    if ( nObjMask & RSC_MENUITEM_HELPID )
    {
        OString aHelpId( ReadByteStringRes() );
        if ( !bSep )
            SetHelpId( nItemId, aHelpId );
    }

    if( !bSep )
        SetHelpText( nItemId, aHelpText );

    if ( nObjMask & RSC_MENUITEM_KEYCODE )
    {
        if ( !bSep )
            SetAccelKey( nItemId, KeyCode( ResId( (RSHEADER_TYPE*)GetClassRes(), *pMgr ) ) );
        IncrementRes( GetObjSizeRes( (RSHEADER_TYPE*)GetClassRes() ) );
    }
    if( nObjMask & RSC_MENUITEM_CHECKED )
    {
        if ( !bSep )
            CheckItem( nItemId, ReadShortRes() != 0 );
    }
    if ( nObjMask & RSC_MENUITEM_DISABLE )
    {
        if ( !bSep )
            EnableItem( nItemId, ReadShortRes() == 0 );
    }
    if ( nObjMask & RSC_MENUITEM_COMMAND )
    {
        OUString aCommandStr = ReadStringRes();
        if ( !bSep )
            SetItemCommand( nItemId, aCommandStr );
    }
    if ( nObjMask & RSC_MENUITEM_MENU )
    {
        if ( !bSep )
        {
            MenuItemData* pData = GetItemList()->GetData( nItemId );
            if ( pData )
            {
                PopupMenu* pSubMenu = new PopupMenu( ResId( (RSHEADER_TYPE*)GetClassRes(), *pMgr ) );
                pData->pAutoSubMenu = pSubMenu;
                // #111060# keep track of this pointer, may be it will be deleted from outside
                pSubMenu->pRefAutoSubMenu = &pData->pAutoSubMenu;
                SetPopupMenu( nItemId, pSubMenu );
            }
        }
        IncrementRes( GetObjSizeRes( (RSHEADER_TYPE*)GetClassRes() ) );
    }
    delete mpLayoutData, mpLayoutData = NULL;
}

void Menu::InsertSeparator(const OString &rIdent, sal_uInt16 nPos)
{
    // do nothing if it's a menu bar
    if ( bIsMenuBar )
        return;

    // if position > ItemCount, append
    if ( nPos >= pItemList->size() )
        nPos = MENU_APPEND;

    // put separator in item list
    pItemList->InsertSeparator(rIdent, nPos);

    // update native menu
    size_t itemPos = ( nPos != MENU_APPEND ) ? nPos : pItemList->size() - 1;
    MenuItemData *pData = pItemList->GetDataFromPos( itemPos );
    if( ImplGetSalMenu() && pData && pData->pSalMenuItem )
        ImplGetSalMenu()->InsertItem( pData->pSalMenuItem, nPos );

    delete mpLayoutData, mpLayoutData = NULL;

    ImplCallEventListeners( VCLEVENT_MENU_INSERTITEM, nPos );
}

void Menu::RemoveItem( sal_uInt16 nPos )
{
    bool bRemove = false;

    if ( nPos < GetItemCount() )
    {
        // update native menu
        if( ImplGetSalMenu() )
            ImplGetSalMenu()->RemoveItem( nPos );

        pItemList->Remove( nPos );
        bRemove = true;
    }

    Window* pWin = ImplGetWindow();
    if ( pWin )
    {
        ImplCalcSize( pWin );
        if ( pWin->IsVisible() )
            pWin->Invalidate();
    }
    delete mpLayoutData, mpLayoutData = NULL;

    if ( bRemove )
        ImplCallEventListeners( VCLEVENT_MENU_REMOVEITEM, nPos );
}

void ImplCopyItem( Menu* pThis, const Menu& rMenu, sal_uInt16 nPos, sal_uInt16 nNewPos,
                  sal_uInt16 nMode = 0 )
{
    MenuItemType eType = rMenu.GetItemType( nPos );

    if ( eType == MENUITEM_DONTKNOW )
        return;

    if ( eType == MENUITEM_SEPARATOR )
        pThis->InsertSeparator( OString(), nNewPos );
    else
    {
        sal_uInt16 nId = rMenu.GetItemId( nPos );

        DBG_ASSERT( pThis->GetItemPos( nId ) == MENU_ITEM_NOTFOUND,
                    "Menu::CopyItem(): ItemId already exists" );

        MenuItemData* pData = rMenu.GetItemList()->GetData( nId );

        if (!pData)
            return;

        if ( eType == MENUITEM_STRINGIMAGE )
            pThis->InsertItem( nId, pData->aText, pData->aImage, pData->nBits, pData->sIdent, nNewPos );
        else if ( eType == MENUITEM_STRING )
            pThis->InsertItem( nId, pData->aText, pData->nBits, pData->sIdent, nNewPos );
        else
            pThis->InsertItem( nId, pData->aImage, pData->nBits, pData->sIdent, nNewPos );

        if ( rMenu.IsItemChecked( nId ) )
            pThis->CheckItem( nId, true );
        if ( !rMenu.IsItemEnabled( nId ) )
            pThis->EnableItem( nId, false );
        pThis->SetHelpId( nId, pData->aHelpId );
        pThis->SetHelpText( nId, pData->aHelpText );
        pThis->SetAccelKey( nId, pData->aAccelKey );
        pThis->SetItemCommand( nId, pData->aCommandStr );
        pThis->SetHelpCommand( nId, pData->aHelpCommandStr );

        PopupMenu* pSubMenu = rMenu.GetPopupMenu( nId );
        if ( pSubMenu )
        {
            // create auto-copy
            if ( nMode == 1 )
            {
                PopupMenu* pNewMenu = new PopupMenu( *pSubMenu );
                pThis->SetPopupMenu( nId, pNewMenu );
            }
            else
                pThis->SetPopupMenu( nId, pSubMenu );
        }
    }
}

void Menu::CopyItem( const Menu& rMenu, sal_uInt16 nPos, sal_uInt16 nNewPos )
{
    ImplCopyItem( this, rMenu, nPos, nNewPos );
}

void Menu::Clear()
{
    for ( sal_uInt16 i = GetItemCount(); i; i-- )
        RemoveItem( 0 );
}

sal_uInt16 Menu::GetItemCount() const
{
    return (sal_uInt16)pItemList->size();
}

sal_uInt16 Menu::ImplGetVisibleItemCount() const
{
    sal_uInt16 nItems = 0;
    for ( size_t n = pItemList->size(); n; )
    {
        if ( ImplIsVisible( --n ) )
            nItems++;
    }
    return nItems;
}

sal_uInt16 Menu::ImplGetFirstVisible() const
{
    for ( size_t n = 0; n < pItemList->size(); n++ )
    {
        if ( ImplIsVisible( n ) )
            return n;
    }
    return ITEMPOS_INVALID;
}

sal_uInt16 Menu::ImplGetPrevVisible( sal_uInt16 nPos ) const
{
    for ( size_t n = nPos; n; )
    {
        if ( n && ImplIsVisible( --n ) )
            return n;
    }
    return ITEMPOS_INVALID;
}

sal_uInt16 Menu::ImplGetNextVisible( sal_uInt16 nPos ) const
{
    for ( size_t n = nPos+1; n < pItemList->size(); n++ )
    {
        if ( ImplIsVisible( n ) )
            return n;
    }
    return ITEMPOS_INVALID;
}

sal_uInt16 Menu::GetItemId(sal_uInt16 nPos) const
{
    MenuItemData* pData = pItemList->GetDataFromPos( nPos );

    if ( pData )
        return pData->nId;
    else
        return 0;
}

sal_uInt16 Menu::GetItemId(const OString &rIdent) const
{
    for (size_t n = 0; n < pItemList->size(); ++n)
    {
        MenuItemData* pData = pItemList->GetDataFromPos(n);
        if (pData && pData->sIdent == rIdent)
            return pData->nId;
    }
    return MENU_ITEM_NOTFOUND;
}

sal_uInt16 Menu::GetItemPos( sal_uInt16 nItemId ) const
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( pData )
        return (sal_uInt16)nPos;
    else
        return MENU_ITEM_NOTFOUND;
}

MenuItemType Menu::GetItemType( sal_uInt16 nPos ) const
{
    MenuItemData* pData = pItemList->GetDataFromPos( nPos );

    if ( pData )
        return pData->eType;
    else
        return MENUITEM_DONTKNOW;
}

void Menu::SetHighlightItem( sal_uInt16 nItem )
{
    nHighlightedItem = nItem;
}


OString Menu::GetCurItemIdent() const
{
    const MenuItemData* pData = pItemList->GetData(nSelectedId);
    return pData ? pData->sIdent : OString();
}

OString Menu::GetItemIdent(sal_uInt16 nId) const
{
    const MenuItemData* pData = pItemList->GetData(nId);
    return pData ? pData->sIdent : OString();
}

void Menu::SetItemBits( sal_uInt16 nItemId, MenuItemBits nBits )
{
    MenuItemData* pData = pItemList->GetData( nItemId );
    if ( pData )
        pData->nBits = nBits;
}

MenuItemBits Menu::GetItemBits( sal_uInt16 nItemId ) const
{
    MenuItemBits nBits = 0;
    MenuItemData* pData = pItemList->GetData( nItemId );
    if ( pData )
        nBits = pData->nBits;
    return nBits;
}

void Menu::SetUserValue( sal_uInt16 nItemId, sal_uLong nValue )
{
    MenuItemData* pData = pItemList->GetData( nItemId );
    if ( pData )
        pData->nUserValue = nValue;
}

sal_uLong Menu::GetUserValue( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );
    return pData ? pData->nUserValue : 0;
}

void Menu::SetPopupMenu( sal_uInt16 nItemId, PopupMenu* pMenu )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    // Item does not exist -> return NULL
    if ( !pData )
        return;

    // same menu, nothing to do
    if ( (PopupMenu*)pData->pSubMenu == pMenu )
        return;

    // data exchange
    pData->pSubMenu = pMenu;

    // #112023# Make sure pStartedFrom does not point to invalid (old) data
    if ( pData->pSubMenu )
        pData->pSubMenu->pStartedFrom = 0;

    // set native submenu
    if( ImplGetSalMenu() && pData->pSalMenuItem )
    {
        if( pMenu )
            ImplGetSalMenu()->SetSubMenu( pData->pSalMenuItem, pMenu->ImplGetSalMenu(), nPos );
        else
            ImplGetSalMenu()->SetSubMenu( pData->pSalMenuItem, NULL, nPos );
    }

    ImplCallEventListeners( VCLEVENT_MENU_SUBMENUCHANGED, nPos );
}

PopupMenu* Menu::GetPopupMenu( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return (PopupMenu*)(pData->pSubMenu);
    else
        return NULL;
}

void Menu::SetAccelKey( sal_uInt16 nItemId, const KeyCode& rKeyCode )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return;

    if ( pData->aAccelKey == rKeyCode )
        return;

    pData->aAccelKey = rKeyCode;

    // update native menu
    if( ImplGetSalMenu() && pData->pSalMenuItem )
        ImplGetSalMenu()->SetAccelerator( nPos, pData->pSalMenuItem, rKeyCode, rKeyCode.GetName() );
}

KeyCode Menu::GetAccelKey( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aAccelKey;
    else
        return KeyCode();
}

KeyEvent Menu::GetActivationKey( sal_uInt16 nItemId ) const
{
    KeyEvent aRet;
    MenuItemData* pData = pItemList->GetData( nItemId );
    if( pData )
    {
        sal_Int32 nPos = pData->aText.indexOf( '~' );
        if( nPos != -1 && nPos < pData->aText.getLength()-1 )
        {
            sal_uInt16 nCode = 0;
            sal_Unicode cAccel = pData->aText[nPos+1];
            if( cAccel >= 'a' && cAccel <= 'z' )
                nCode = KEY_A + (cAccel-'a');
            else if( cAccel >= 'A' && cAccel <= 'Z' )
                nCode = KEY_A + (cAccel-'A');
            else if( cAccel >= '0' && cAccel <= '9' )
                nCode = KEY_0 + (cAccel-'0');
            if(nCode )
                aRet = KeyEvent( cAccel, KeyCode( nCode, KEY_MOD2 ) );
        }

    }
    return aRet;
}

void Menu::CheckItem( sal_uInt16 nItemId, bool bCheck )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData || pData->bChecked == bCheck )
        return;

    // if radio-check, then uncheck previous
    if ( bCheck && (pData->nBits & MIB_AUTOCHECK) &&
         (pData->nBits & MIB_RADIOCHECK) )
    {
        MenuItemData*   pGroupData;
        sal_uInt16          nGroupPos;
        sal_uInt16          nItemCount = GetItemCount();
        bool            bFound = false;

        nGroupPos = nPos;
        while ( nGroupPos )
        {
            pGroupData = pItemList->GetDataFromPos( nGroupPos-1 );
            if ( pGroupData->nBits & MIB_RADIOCHECK )
            {
                if ( IsItemChecked( pGroupData->nId ) )
                {
                    CheckItem( pGroupData->nId, false );
                    bFound = true;
                    break;
                }
            }
            else
                break;
            nGroupPos--;
        }

        if ( !bFound )
        {
            nGroupPos = nPos+1;
            while ( nGroupPos < nItemCount )
            {
                pGroupData = pItemList->GetDataFromPos( nGroupPos );
                if ( pGroupData->nBits & MIB_RADIOCHECK )
                {
                    if ( IsItemChecked( pGroupData->nId ) )
                    {
                        CheckItem( pGroupData->nId, false );
                        break;
                    }
                }
                else
                    break;
                nGroupPos++;
            }
        }
    }

    pData->bChecked = bCheck;

    // update native menu
    if( ImplGetSalMenu() )
        ImplGetSalMenu()->CheckItem( nPos, bCheck );

    ImplCallEventListeners( bCheck ? VCLEVENT_MENU_ITEMCHECKED : VCLEVENT_MENU_ITEMUNCHECKED, nPos );
}

bool Menu::IsItemChecked( sal_uInt16 nItemId ) const
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return false;

    return pData->bChecked;
}

void Menu::EnableItem( sal_uInt16 nItemId, bool bEnable )
{
    size_t          nPos;
    MenuItemData*   pItemData = pItemList->GetData( nItemId, nPos );

    if ( pItemData && ( pItemData->bEnabled != bEnable ) )
    {
        pItemData->bEnabled = bEnable;

        Window* pWin = ImplGetWindow();
        if ( pWin && pWin->IsVisible() )
        {
            DBG_ASSERT( bIsMenuBar, "Menu::EnableItem - Popup visible!" );
            long nX = 0;
            size_t nCount = pItemList->size();
            for ( size_t n = 0; n < nCount; n++ )
            {
                MenuItemData* pData = pItemList->GetDataFromPos( n );
                if ( n == nPos )
                {
                    pWin->Invalidate( Rectangle( Point( nX, 0 ), Size( pData->aSz.Width(), pData->aSz.Height() ) ) );
                    break;
                }
                nX += pData->aSz.Width();
            }
        }
        // update native menu
        if( ImplGetSalMenu() )
            ImplGetSalMenu()->EnableItem( nPos, bEnable );

        ImplCallEventListeners( bEnable ? VCLEVENT_MENU_ENABLE : VCLEVENT_MENU_DISABLE, nPos );
    }
}

bool Menu::IsItemEnabled( sal_uInt16 nItemId ) const
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return false;

    return pData->bEnabled;
}

void Menu::ShowItem( sal_uInt16 nItemId, bool bVisible )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    DBG_ASSERT( !bIsMenuBar, "Menu::ShowItem - ignored for menu bar entries!" );
    if ( !bIsMenuBar && pData && ( pData->bVisible != bVisible ) )
    {
        Window* pWin = ImplGetWindow();
        if ( pWin && pWin->IsVisible() )
        {
            DBG_ASSERT( false, "Menu::ShowItem - ignored for visible popups!" );
            return;
        }
        pData->bVisible = bVisible;

        // update native menu
        if( ImplGetSalMenu() )
            ImplGetSalMenu()->ShowItem( nPos, bVisible );
    }
}

void Menu::SetItemText( sal_uInt16 nItemId, const OUString& rStr )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return;

    if ( !rStr.equals( pData->aText ) )
    {
        pData->aText = rStr;
        ImplSetMenuItemData( pData );
        // update native menu
        if( ImplGetSalMenu() && pData->pSalMenuItem )
            ImplGetSalMenu()->SetItemText( nPos, pData->pSalMenuItem, rStr );

        Window* pWin = ImplGetWindow();
        delete mpLayoutData, mpLayoutData = NULL;
        if ( pWin && IsMenuBar() )
        {
            ImplCalcSize( pWin );
            if ( pWin->IsVisible() )
                pWin->Invalidate();
        }

        ImplCallEventListeners( VCLEVENT_MENU_ITEMTEXTCHANGED, nPos );
    }
}

OUString Menu::GetItemText( sal_uInt16 nItemId ) const
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( pData )
        return pData->aText;

    return OUString();
}

void Menu::SetItemImage( sal_uInt16 nItemId, const Image& rImage )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( !pData )
        return;

    pData->aImage = rImage;
    ImplSetMenuItemData( pData );

    // update native menu
    if( ImplGetSalMenu() && pData->pSalMenuItem )
        ImplGetSalMenu()->SetItemImage( nPos, pData->pSalMenuItem, rImage );
}

static inline Image ImplRotImage( const Image& rImage, long nAngle10 )
{
    Image       aRet;
    BitmapEx    aBmpEx( rImage.GetBitmapEx() );

    aBmpEx.Rotate( nAngle10, COL_WHITE );

    return Image( aBmpEx );
}

void Menu::SetItemImageAngle( sal_uInt16 nItemId, long nAngle10 )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( pData )
    {
        long nDeltaAngle = (nAngle10 - pData->nItemImageAngle) % 3600;
        while( nDeltaAngle < 0 )
            nDeltaAngle += 3600;

        pData->nItemImageAngle = nAngle10;
        if( nDeltaAngle && !!pData->aImage )
            pData->aImage = ImplRotImage( pData->aImage, nDeltaAngle );
    }
}

static inline Image ImplMirrorImage( const Image& rImage )
{
    Image       aRet;
    BitmapEx    aBmpEx( rImage.GetBitmapEx() );

    aBmpEx.Mirror( BMP_MIRROR_HORZ );

    return Image( aBmpEx );
}

void Menu::SetItemImageMirrorMode( sal_uInt16 nItemId, bool bMirror )
{
    size_t          nPos;
    MenuItemData*   pData = pItemList->GetData( nItemId, nPos );

    if ( pData )
    {
        if( ( pData->bMirrorMode && ! bMirror ) ||
            ( ! pData->bMirrorMode && bMirror )
            )
        {
            pData->bMirrorMode = bMirror;
            if( !!pData->aImage )
                pData->aImage = ImplMirrorImage( pData->aImage );
        }
    }
}

Image Menu::GetItemImage( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aImage;
    else
        return Image();
}

void Menu::SetItemCommand( sal_uInt16 nItemId, const OUString& rCommand )
{
    size_t        nPos;
    MenuItemData* pData = pItemList->GetData( nItemId, nPos );

    if ( pData )
        pData->aCommandStr = rCommand;
}

OUString Menu::GetItemCommand( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if (pData)
        return pData->aCommandStr;

    return OUString();
}

void Menu::SetHelpCommand( sal_uInt16 nItemId, const OUString& rStr )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        pData->aHelpCommandStr = rStr;
}

OUString Menu::GetHelpCommand( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aHelpCommandStr;

    return OUString();
}

void Menu::SetHelpText( sal_uInt16 nItemId, const OUString& rStr )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        pData->aHelpText = rStr;
}

OUString Menu::ImplGetHelpText( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData && pData->aHelpText.isEmpty() &&
         (( !pData->aHelpId.isEmpty()  ) || ( !pData->aCommandStr.isEmpty() )))
    {
        Help* pHelp = Application::GetHelp();
        if ( pHelp )
        {
            if (!pData->aCommandStr.isEmpty())
                pData->aHelpText = pHelp->GetHelpText( pData->aCommandStr, NULL );
            if (pData->aHelpText.isEmpty() && !pData->aHelpId.isEmpty())
                pData->aHelpText = pHelp->GetHelpText( OStringToOUString( pData->aHelpId, RTL_TEXTENCODING_UTF8 ), NULL );
        }
    }

    return OUString();
}

OUString Menu::GetHelpText( sal_uInt16 nItemId ) const
{
    return ImplGetHelpText( nItemId );
}

void Menu::SetTipHelpText( sal_uInt16 nItemId, const OUString& rStr )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        pData->aTipHelpText = rStr;
}

OUString Menu::GetTipHelpText( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aTipHelpText;

    return OUString();
}

void Menu::SetHelpId( sal_uInt16 nItemId, const OString& rHelpId )
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        pData->aHelpId = rHelpId;
}

OString Menu::GetHelpId( sal_uInt16 nItemId ) const
{
    OString aRet;

    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
    {
        if ( !pData->aHelpId.isEmpty() )
            aRet = pData->aHelpId;
        else
            aRet = OUStringToOString( pData->aCommandStr, RTL_TEXTENCODING_UTF8 );
    }

    return aRet;
}

Menu& Menu::operator=( const Menu& rMenu )
{
    // clean up
    Clear();

    // copy items
    sal_uInt16 nCount = rMenu.GetItemCount();
    for ( sal_uInt16 i = 0; i < nCount; i++ )
        ImplCopyItem( this, rMenu, i, MENU_APPEND, 1 );

    nDefaultItem = rMenu.nDefaultItem;
    aActivateHdl = rMenu.aActivateHdl;
    aDeactivateHdl = rMenu.aDeactivateHdl;
    aHighlightHdl = rMenu.aHighlightHdl;
    aSelectHdl = rMenu.aSelectHdl;
    aTitleText = rMenu.aTitleText;
    bIsMenuBar = rMenu.bIsMenuBar;

    return *this;
}

bool Menu::ImplIsVisible( sal_uInt16 nPos ) const
{
    bool bVisible = true;

    MenuItemData* pData = pItemList->GetDataFromPos( nPos );
    // check general visibility first
    if( pData && !pData->bVisible )
        bVisible = false;

    if ( bVisible && pData && pData->eType == MENUITEM_SEPARATOR )
    {
        if( nPos == 0 ) // no separator should be shown at the very beginning
            bVisible = false;
        else
        {
            // always avoid adjacent separators
            size_t nCount = pItemList->size();
            size_t n;
            MenuItemData* pNextData = NULL;
            // search next visible item
            for( n = nPos + 1; n < nCount; n++ )
            {
                pNextData = pItemList->GetDataFromPos( n );
                if( pNextData && pNextData->bVisible )
                {
                    if( pNextData->eType == MENUITEM_SEPARATOR || ImplIsVisible(n) )
                        break;
                }
            }
            if( n == nCount ) // no next visible item
                bVisible = false;
            // check for separator
            if( pNextData && pNextData->bVisible && pNextData->eType == MENUITEM_SEPARATOR )
                bVisible = false;

            if( bVisible )
            {
                for( n = nPos; n > 0; n-- )
                {
                    pNextData = pItemList->GetDataFromPos( n-1 );
                    if( pNextData && pNextData->bVisible )
                    {
                        if( pNextData->eType != MENUITEM_SEPARATOR && ImplIsVisible(n-1) )
                            break;
                    }
                }
                if( n == 0 ) // no previous visible item
                    bVisible = false;
            }
        }
    }

    // not allowed for menubar, as I do not know
    // whether a menu-entry will disappear or will appear
    if ( bVisible && !bIsMenuBar && ( nMenuFlags & MENU_FLAG_HIDEDISABLEDENTRIES ) &&
        !( nMenuFlags & MENU_FLAG_ALWAYSSHOWDISABLEDENTRIES ) )
    {
        if( !pData ) // e.g. nPos == ITEMPOS_INVALID
            bVisible = false;
        else if ( pData->eType != MENUITEM_SEPARATOR ) // separators handled above
        {
            // bVisible = pData->bEnabled && ( !pData->pSubMenu || pData->pSubMenu->HasValidEntries( true ) );
            bVisible = pData->bEnabled; // do not check submenus as they might be filled at Activate().
        }
    }

    return bVisible;
}

bool Menu::IsItemPosVisible( sal_uInt16 nItemPos ) const
{
    return IsMenuVisible() && ImplIsVisible( nItemPos );
}

bool Menu::IsMenuVisible() const
{
    return pWindow && pWindow->IsReallyVisible();
}

bool Menu::ImplIsSelectable( sal_uInt16 nPos ) const
{
    bool bSelectable = true;

    MenuItemData* pData = pItemList->GetDataFromPos( nPos );
    // check general visibility first
    if ( pData && ( pData->nBits & MIB_NOSELECT ) )
        bSelectable = false;

    return bSelectable;
}

void Menu::SelectItem( sal_uInt16 nItemId )
{
    if( bIsMenuBar )
        static_cast<MenuBar*>(this)->SelectEntry( nItemId );
    else
        static_cast<PopupMenu*>(this)->SelectEntry( nItemId );
}

::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > Menu::GetAccessible()
{
    // Since PopupMenu are sometimes shared by different instances of MenuBar, the mxAccessible member gets
    // overwritten and may contain a disposed object when the initial menubar gets set again. So use the
    // mxAccessible member only for sub menus.
    if ( pStartedFrom )
    {
        for ( sal_uInt16 i = 0, nCount = pStartedFrom->GetItemCount(); i < nCount; ++i )
        {
            sal_uInt16 nItemId = pStartedFrom->GetItemId( i );
            if ( static_cast< Menu* >( pStartedFrom->GetPopupMenu( nItemId ) ) == this )
            {
                ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > xParent = pStartedFrom->GetAccessible();
                if ( xParent.is() )
                {
                    ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessibleContext > xParentContext( xParent->getAccessibleContext() );
                    if ( xParentContext.is() )
                        return xParentContext->getAccessibleChild( i );
                }
            }
        }
    }
    else if ( !mxAccessible.is() )
    {
        UnoWrapperBase* pWrapper = Application::GetUnoWrapper();
        if ( pWrapper )
            mxAccessible = pWrapper->CreateAccessible( this, bIsMenuBar );
    }

    return mxAccessible;
}

void Menu::SetAccessible( const ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible >& rxAccessible )
{
    mxAccessible = rxAccessible;
}

Size Menu::ImplGetNativeCheckAndRadioSize( const Window* pWin, long& rCheckHeight, long& rRadioHeight ) const
{
    long nCheckWidth = 0, nRadioWidth = 0;
    rCheckHeight = rRadioHeight = 0;

    if( ! bIsMenuBar )
    {
        ImplControlValue aVal;
        Rectangle aNativeBounds;
        Rectangle aNativeContent;
        Point tmp( 0, 0 );
        Rectangle aCtrlRegion( Rectangle( tmp, Size( 100, 15 ) ) );
        if( pWin->IsNativeControlSupported( CTRL_MENU_POPUP, PART_MENU_ITEM_CHECK_MARK ) )
        {
            if( pWin->GetNativeControlRegion( ControlType(CTRL_MENU_POPUP),
                                              ControlPart(PART_MENU_ITEM_CHECK_MARK),
                                              aCtrlRegion,
                                              ControlState(CTRL_STATE_ENABLED),
                                              aVal,
                                              OUString(),
                                              aNativeBounds,
                                              aNativeContent )
            )
            {
                rCheckHeight = aNativeBounds.GetHeight();
                nCheckWidth = aNativeContent.GetWidth();
            }
        }
        if( pWin->IsNativeControlSupported( CTRL_MENU_POPUP, PART_MENU_ITEM_RADIO_MARK ) )
        {
            if( pWin->GetNativeControlRegion( ControlType(CTRL_MENU_POPUP),
                                              ControlPart(PART_MENU_ITEM_RADIO_MARK),
                                              aCtrlRegion,
                                              ControlState(CTRL_STATE_ENABLED),
                                              aVal,
                                              OUString(),
                                              aNativeBounds,
                                              aNativeContent )
            )
            {
                rRadioHeight = aNativeBounds.GetHeight();
                nRadioWidth = aNativeContent.GetWidth();
            }
        }
    }
    return Size(std::max(nCheckWidth, nRadioWidth), std::max(rCheckHeight, rRadioHeight));
}

bool Menu::ImplGetNativeSubmenuArrowSize( Window* pWin, Size& rArrowSize, long& rArrowSpacing ) const
{
    ImplControlValue aVal;
    Rectangle aNativeBounds;
    Rectangle aNativeContent;
    Point tmp( 0, 0 );
    Rectangle aCtrlRegion( Rectangle( tmp, Size( 100, 15 ) ) );
    if( pWin->IsNativeControlSupported( CTRL_MENU_POPUP,
                                        PART_MENU_SUBMENU_ARROW ) )
        {
            if( pWin->GetNativeControlRegion( ControlType(CTRL_MENU_POPUP),
                                              ControlPart(PART_MENU_SUBMENU_ARROW),
                                              aCtrlRegion,
                                              ControlState(CTRL_STATE_ENABLED),
                                              aVal,
                                              OUString(),
                                              aNativeBounds,
                                              aNativeContent )
            )
            {
                Size aSize( Size ( aNativeContent.GetWidth(),
                                   aNativeContent.GetHeight() ) );
                rArrowSize = aSize;
                rArrowSpacing = aNativeBounds.GetWidth() - aNativeContent.GetWidth();

                return true;
            }
        }
    return false;
}

void Menu::ImplAddDel( ImplMenuDelData& rDel )
{
    DBG_ASSERT( !rDel.mpMenu, "Menu::ImplAddDel(): cannot add ImplMenuDelData twice !" );
    if( !rDel.mpMenu )
    {
        rDel.mpMenu = this;
        rDel.mpNext = mpFirstDel;
        mpFirstDel = &rDel;
    }
}

void Menu::ImplRemoveDel( ImplMenuDelData& rDel )
{
    rDel.mpMenu = NULL;
    if ( mpFirstDel == &rDel )
    {
        mpFirstDel = rDel.mpNext;
    }
    else
    {
        ImplMenuDelData* pData = mpFirstDel;
        while ( pData && (pData->mpNext != &rDel) )
            pData = pData->mpNext;

        DBG_ASSERT( pData, "Menu::ImplRemoveDel(): ImplMenuDelData not registered !" );
        if( pData )
            pData->mpNext = rDel.mpNext;
    }
}

Size Menu::ImplCalcSize( const Window* pWin )
{
    // | Check/Radio/Image| Text| Accel/Popup|

    // for symbols: nFontHeight x nFontHeight
    long nFontHeight = pWin->GetTextHeight();
    long nExtra = nFontHeight/4;

    long nMinMenuItemHeight = nFontHeight;
    long nCheckHeight = 0, nRadioHeight = 0;
    Size aMaxSize = ImplGetNativeCheckAndRadioSize(pWin, nCheckHeight, nRadioHeight);
    if( aMaxSize.Height() > nMinMenuItemHeight )
        nMinMenuItemHeight = aMaxSize.Height();

    Size aMaxImgSz;

    const StyleSettings& rSettings = pWin->GetSettings().GetStyleSettings();
    if ( rSettings.GetUseImagesInMenus() )
    {
        if ( 16 > nMinMenuItemHeight )
            nMinMenuItemHeight = 16;
        for ( size_t i = pItemList->size(); i; )
        {
            MenuItemData* pData = pItemList->GetDataFromPos( --i );
            if ( ImplIsVisible( i )
               && (  ( pData->eType == MENUITEM_IMAGE )
                  || ( pData->eType == MENUITEM_STRINGIMAGE )
                  )
               )
            {
                Size aImgSz = pData->aImage.GetSizePixel();
                if ( aImgSz.Height() > aMaxImgSz.Height() )
                    aMaxImgSz.Height() = aImgSz.Height();
                if ( aImgSz.Height() > nMinMenuItemHeight )
                    nMinMenuItemHeight = aImgSz.Height();
                break;
            }
        }
    }

    Size aSz;
    long nCheckWidth = 0;
    long nMaxWidth = 0;

    for ( size_t n = pItemList->size(); n; )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( --n );

        pData->aSz.Height() = 0;
        pData->aSz.Width() = 0;

        if ( ImplIsVisible( n ) )
        {
            long nWidth = 0;

            // Separator
            if ( !bIsMenuBar && ( pData->eType == MENUITEM_SEPARATOR ) )
            {
                DBG_ASSERT( !bIsMenuBar, "Separator in MenuBar ?! " );
                pData->aSz.Height() = 4;
            }

            // Image:
            if ( !bIsMenuBar && ( ( pData->eType == MENUITEM_IMAGE ) || ( pData->eType == MENUITEM_STRINGIMAGE ) ) )
            {
                Size aImgSz = pData->aImage.GetSizePixel();
                aImgSz.Height() += 4; // add a border for native marks
                aImgSz.Width() += 4; // add a border for native marks
                if ( aImgSz.Width() > aMaxImgSz.Width() )
                    aMaxImgSz.Width() = aImgSz.Width();
                if ( aImgSz.Height() > aMaxImgSz.Height() )
                    aMaxImgSz.Height() = aImgSz.Height();
                if ( aImgSz.Height() > pData->aSz.Height() )
                    pData->aSz.Height() = aImgSz.Height();
            }

            // Check Buttons:
            if ( !bIsMenuBar && pData->HasCheck() )
            {
                nCheckWidth = aMaxSize.Width();
                // checks / images take the same place
                if( ! ( ( pData->eType == MENUITEM_IMAGE ) || ( pData->eType == MENUITEM_STRINGIMAGE ) ) )
                    nWidth += nCheckWidth + nExtra * 2;
            }

            // Text:
            if ( (pData->eType == MENUITEM_STRING) || (pData->eType == MENUITEM_STRINGIMAGE) )
            {
                long nTextWidth = pWin->GetCtrlTextWidth( pData->aText );
                long nTextHeight = pWin->GetTextHeight();

                if ( bIsMenuBar )
                {
                    if ( nTextHeight > pData->aSz.Height() )
                        pData->aSz.Height() = nTextHeight;

                    pData->aSz.Width() = nTextWidth + 4*nExtra;
                    aSz.Width() += pData->aSz.Width();
                }
                else
                    pData->aSz.Height() = std::max( std::max( nTextHeight, pData->aSz.Height() ), nMinMenuItemHeight );

                nWidth += nTextWidth;
            }

            // Accel
            if ( !bIsMenuBar && pData->aAccelKey.GetCode() && !ImplAccelDisabled() )
            {
                OUString aName = pData->aAccelKey.GetName();
                long nAccWidth = pWin->GetTextWidth( aName );
                nAccWidth += nExtra;
                nWidth += nAccWidth;
            }

            // SubMenu?
            if ( !bIsMenuBar && pData->pSubMenu )
            {
                    if ( nFontHeight > nWidth )
                        nWidth += nFontHeight;

                pData->aSz.Height() = std::max( std::max( nFontHeight, pData->aSz.Height() ), nMinMenuItemHeight );
            }

            pData->aSz.Height() += EXTRAITEMHEIGHT; // little bit more distance

            if ( !bIsMenuBar )
                aSz.Height() += (long)pData->aSz.Height();

            if ( nWidth > nMaxWidth )
                nMaxWidth = nWidth;

        }
    }

    if ( !bIsMenuBar )
    {
        // popup menus should not be wider than half the screen
        // except on rather small screens
        // TODO: move GetScreenNumber from SystemWindow to Window ?
        // currently we rely on internal privileges
        unsigned int nDisplayScreen = pWin->ImplGetWindowImpl()->mpFrame->maGeometry.nDisplayScreenNumber;
        Rectangle aDispRect( Application::GetScreenPosSizePixel( nDisplayScreen ) );
        long nScreenWidth = aDispRect.GetWidth() >= 800 ? aDispRect.GetWidth() : 800;
        if( nMaxWidth > nScreenWidth/2 )
            nMaxWidth = nScreenWidth/2;

        sal_uInt16 gfxExtra = (sal_uInt16) std::max( nExtra, 7L ); // #107710# increase space between checkmarks/images/text
        nImgOrChkPos = (sal_uInt16)nExtra;
        long nImgOrChkWidth = 0;
        if( aMaxSize.Height() > 0 ) // NWF case
            nImgOrChkWidth = aMaxSize.Height() + nExtra;
        else // non NWF case
            nImgOrChkWidth = nFontHeight/2 + gfxExtra;
        nImgOrChkWidth = std::max( nImgOrChkWidth, aMaxImgSz.Width() + gfxExtra );
        nTextPos = (sal_uInt16)(nImgOrChkPos + nImgOrChkWidth);
        nTextPos = nTextPos + gfxExtra;

        aSz.Width() = nTextPos + nMaxWidth + nExtra;
        aSz.Width() += 4*nExtra;   // a _little_ more ...

        aSz.Width() += 2*ImplGetSVData()->maNWFData.mnMenuFormatBorderX;
        aSz.Height() += 2*ImplGetSVData()->maNWFData.mnMenuFormatBorderY;
    }
    else
    {
        nTextPos = (sal_uInt16)(2*nExtra);
        aSz.Height() = nFontHeight+6;

        // get menubar height from native methods if supported
        if( pWindow->IsNativeControlSupported( CTRL_MENUBAR, PART_ENTIRE_CONTROL ) )
        {
            ImplControlValue aVal;
            Rectangle aNativeBounds;
            Rectangle aNativeContent;
            Point tmp( 0, 0 );
            Rectangle aCtrlRegion( tmp, Size( 100, 15 ) );
            if( pWindow->GetNativeControlRegion( ControlType(CTRL_MENUBAR),
                                                 ControlPart(PART_ENTIRE_CONTROL),
                                                 aCtrlRegion,
                                                 ControlState(CTRL_STATE_ENABLED),
                                                 aVal,
                                                 OUString(),
                                                 aNativeBounds,
                                                 aNativeContent )
            )
            {
                int nNativeHeight = aNativeBounds.GetHeight();
                if( nNativeHeight > aSz.Height() )
                    aSz.Height() = nNativeHeight;
            }
        }

        // account for the size of the close button, which actually is a toolbox
        // due to NWF this is variable
        long nCloserHeight = ((MenuBarWindow*) pWindow)->MinCloseButtonSize().Height();
        if( aSz.Height() < nCloserHeight )
            aSz.Height() = nCloserHeight;
    }

    if ( pLogo )
        aSz.Width() += pLogo->aBitmap.GetSizePixel().Width();

    return aSz;
}

static void ImplPaintCheckBackground( Window* i_pWindow, const Rectangle& i_rRect, bool i_bHighlight )
{
    bool bNativeOk = false;
    if( i_pWindow->IsNativeControlSupported( CTRL_TOOLBAR, PART_BUTTON ) )
    {
        ImplControlValue    aControlValue;
        Rectangle           aCtrlRegion( i_rRect );
        ControlState        nState = CTRL_STATE_PRESSED | CTRL_STATE_ENABLED;

        aControlValue.setTristateVal( BUTTONVALUE_ON );

        bNativeOk = i_pWindow->DrawNativeControl( CTRL_TOOLBAR, PART_BUTTON,
                                                  aCtrlRegion, nState, aControlValue,
                                                  OUString() );
    }

    if( ! bNativeOk )
    {
        const StyleSettings& rSettings = i_pWindow->GetSettings().GetStyleSettings();
        Color aColor( i_bHighlight ? rSettings.GetMenuHighlightTextColor() : rSettings.GetHighlightColor() );
        i_pWindow->DrawSelectionBackground( i_rRect, 0, i_bHighlight, true, false, 2, NULL, &aColor );
    }
}

static OUString getShortenedString( const OUString& i_rLong, Window* i_pWin, long i_nMaxWidth )
{
    sal_Int32 nPos = -1;
    OUString aNonMnem( OutputDevice::GetNonMnemonicString( i_rLong, nPos ) );
    aNonMnem = i_pWin->GetEllipsisString( aNonMnem, i_nMaxWidth, TEXT_DRAW_CENTERELLIPSIS );
    // re-insert mnemonic
    if( nPos != -1 )
    {
        if( nPos < aNonMnem.getLength() && i_rLong[nPos+1] == aNonMnem[nPos] )
        {
            OUStringBuffer aBuf( i_rLong.getLength() );
            aBuf.append( aNonMnem.copy( 0, nPos) );
            aBuf.append( '~' );
            aBuf.append( aNonMnem.copy(nPos) );
            aNonMnem = aBuf.makeStringAndClear();
        }
    }
    return aNonMnem;
}

void Menu::ImplPaint( Window* pWin, sal_uInt16 nBorder, long nStartY, MenuItemData* pThisItemOnly, bool bHighlighted, bool bLayout, bool bRollover ) const
{
    // for symbols: nFontHeight x nFontHeight
    long nFontHeight = pWin->GetTextHeight();
    long nExtra = nFontHeight/4;

    long nCheckHeight = 0, nRadioHeight = 0;
    ImplGetNativeCheckAndRadioSize( pWin, nCheckHeight, nRadioHeight );

    DecorationView aDecoView( pWin );
    const StyleSettings& rSettings = pWin->GetSettings().GetStyleSettings();

    Point aTopLeft, aTmpPos;

    if ( pLogo )
        aTopLeft.X() = pLogo->aBitmap.GetSizePixel().Width();

    int nOuterSpaceX = 0;
    if( !bIsMenuBar )
    {
        nOuterSpaceX = ImplGetSVData()->maNWFData.mnMenuFormatBorderX;
        aTopLeft.X() += nOuterSpaceX;
        aTopLeft.Y() += ImplGetSVData()->maNWFData.mnMenuFormatBorderY;
    }

    Size aOutSz = pWin->GetOutputSizePixel();
    size_t nCount = pItemList->size();
    if( bLayout )
        mpLayoutData->m_aVisibleItemBoundRects.clear();

    for ( size_t n = 0; n < nCount; n++ )
    {
        MenuItemData* pData = pItemList->GetDataFromPos( n );
        if ( ImplIsVisible( n ) && ( !pThisItemOnly || ( pData == pThisItemOnly ) ) )
        {
            if ( pThisItemOnly )
            {
                if ( bIsMenuBar && bRollover )
                    pWin->SetTextColor( rSettings.GetMenuBarRolloverTextColor() );
                else if ( bHighlighted )
                    pWin->SetTextColor( rSettings.GetMenuHighlightTextColor() );
            }

            Point aPos( aTopLeft );
            aPos.Y() += nBorder;
            aPos.Y() += nStartY;

            if ( aPos.Y() >= 0 )
            {
                long    nTextOffsetY = ((pData->aSz.Height()-nFontHeight)/2);
                if( bIsMenuBar )
                    nTextOffsetY += (aOutSz.Height()-pData->aSz.Height()) / 2;
                sal_uInt16  nTextStyle   = 0;
                sal_uInt16  nSymbolStyle = 0;
                sal_uInt16  nImageStyle  = 0;

                // submenus without items are not disabled when no items are
                // contained. The application itself should check for this!
                // Otherwise it could happen entries are disabled due to
                // asynchronous loading
                if ( !pData->bEnabled )
                {
                    nTextStyle   |= TEXT_DRAW_DISABLE;
                    nSymbolStyle |= SYMBOL_DRAW_DISABLE;
                    nImageStyle  |= IMAGE_DRAW_DISABLE;
                }

                // Separator
                if ( !bLayout && !bIsMenuBar && ( pData->eType == MENUITEM_SEPARATOR ) )
                {
                    bool bNativeOk = false;
                    if( pWin->IsNativeControlSupported( CTRL_MENU_POPUP,
                                                        PART_MENU_SEPARATOR ) )
                    {
                        ControlState nState = 0;
                        if ( pData->bEnabled )
                            nState |= CTRL_STATE_ENABLED;
                        if ( bHighlighted )
                            nState |= CTRL_STATE_SELECTED;
                        Size aSz( pData->aSz );
                        aSz.Width() = aOutSz.Width() - 2*nOuterSpaceX;
                        Rectangle aItemRect( aPos, aSz );
                        MenupopupValue aVal( nTextPos-GUTTERBORDER, aItemRect );
                        bNativeOk = pWin->DrawNativeControl( CTRL_MENU_POPUP, PART_MENU_SEPARATOR,
                                                             aItemRect,
                                                             nState,
                                                             aVal,
                                                             OUString() );
                    }
                    if( ! bNativeOk )
                    {
                        aTmpPos.Y() = aPos.Y() + ((pData->aSz.Height()-2)/2);
                        aTmpPos.X() = aPos.X() + 2 + nOuterSpaceX;
                        pWin->SetLineColor( rSettings.GetShadowColor() );
                        pWin->DrawLine( aTmpPos, Point( aOutSz.Width() - 3 - 2*nOuterSpaceX, aTmpPos.Y() ) );
                        aTmpPos.Y()++;
                        pWin->SetLineColor( rSettings.GetLightColor() );
                        pWin->DrawLine( aTmpPos, Point( aOutSz.Width() - 3 - 2*nOuterSpaceX, aTmpPos.Y() ) );
                        pWin->SetLineColor();
                    }
                }

                Rectangle aOuterCheckRect( Point( aPos.X()+nImgOrChkPos, aPos.Y() ), Size( pData->aSz.Height(), pData->aSz.Height() ) );
                aOuterCheckRect.Left()      += 1;
                aOuterCheckRect.Right()     -= 1;
                aOuterCheckRect.Top()       += 1;
                aOuterCheckRect.Bottom()    -= 1;

                // CheckMark
                if ( !bLayout && !bIsMenuBar && pData->HasCheck() )
                {
                    // draw selection transparent marker if checked
                    // onto that either a checkmark or the item image
                    // will be painted
                    // however do not do this if native checks will be painted since
                    // the selection color too often does not fit the theme's check and/or radio

                    if( ! ( ( pData->eType == MENUITEM_IMAGE ) || ( pData->eType == MENUITEM_STRINGIMAGE ) ) )
                    {
                        if ( pWin->IsNativeControlSupported( CTRL_MENU_POPUP,
                                                             (pData->nBits & MIB_RADIOCHECK)
                                                             ? PART_MENU_ITEM_CHECK_MARK
                                                             : PART_MENU_ITEM_RADIO_MARK ) )
                        {
                            ControlPart nPart = ((pData->nBits & MIB_RADIOCHECK)
                                                 ? PART_MENU_ITEM_RADIO_MARK
                                                 : PART_MENU_ITEM_CHECK_MARK);

                            ControlState nState = 0;

                            if ( pData->bChecked )
                                nState |= CTRL_STATE_PRESSED;

                            if ( pData->bEnabled )
                                nState |= CTRL_STATE_ENABLED;

                            if ( bHighlighted )
                                nState |= CTRL_STATE_SELECTED;

                            long nCtrlHeight = (pData->nBits & MIB_RADIOCHECK) ? nCheckHeight : nRadioHeight;
                            aTmpPos.X() = aOuterCheckRect.Left() + (aOuterCheckRect.GetWidth() - nCtrlHeight)/2;
                            aTmpPos.Y() = aOuterCheckRect.Top() + (aOuterCheckRect.GetHeight() - nCtrlHeight)/2;

                            Rectangle aCheckRect( aTmpPos, Size( nCtrlHeight, nCtrlHeight ) );
                            Size aSz( pData->aSz );
                            aSz.Width() = aOutSz.Width() - 2*nOuterSpaceX;
                            Rectangle aItemRect( aPos, aSz );
                            MenupopupValue aVal( nTextPos-GUTTERBORDER, aItemRect );
                            pWin->DrawNativeControl( CTRL_MENU_POPUP, nPart,
                                                     aCheckRect,
                                                     nState,
                                                     aVal,
                                                     OUString() );
                        }
                        else if ( pData->bChecked ) // by default do nothing for unchecked items
                        {
                            ImplPaintCheckBackground( pWin, aOuterCheckRect, pThisItemOnly && bHighlighted );

                            SymbolType eSymbol;
                            Size aSymbolSize;
                            if ( pData->nBits & MIB_RADIOCHECK )
                            {
                                eSymbol = SYMBOL_RADIOCHECKMARK;
                                aSymbolSize = Size( nFontHeight/2, nFontHeight/2 );
                            }
                            else
                            {
                                eSymbol = SYMBOL_CHECKMARK;
                                aSymbolSize = Size( (nFontHeight*25)/40, nFontHeight/2 );
                            }
                            aTmpPos.X() = aOuterCheckRect.Left() + (aOuterCheckRect.GetWidth() - aSymbolSize.Width())/2;
                            aTmpPos.Y() = aOuterCheckRect.Top() + (aOuterCheckRect.GetHeight() - aSymbolSize.Height())/2;
                            Rectangle aRect( aTmpPos, aSymbolSize );
                            aDecoView.DrawSymbol( aRect, eSymbol, pWin->GetTextColor(), nSymbolStyle );
                        }
                    }
                }

                // Image:
                if ( !bLayout && !bIsMenuBar && ( ( pData->eType == MENUITEM_IMAGE ) || ( pData->eType == MENUITEM_STRINGIMAGE ) ) )
                {
                    // Don't render an image for a check thing
                    if( pData->bChecked )
                        ImplPaintCheckBackground( pWin, aOuterCheckRect, pThisItemOnly && bHighlighted );
                    aTmpPos = aOuterCheckRect.TopLeft();
                    aTmpPos.X() += (aOuterCheckRect.GetWidth()-pData->aImage.GetSizePixel().Width())/2;
                    aTmpPos.Y() += (aOuterCheckRect.GetHeight()-pData->aImage.GetSizePixel().Height())/2;
                    pWin->DrawImage( aTmpPos, pData->aImage, nImageStyle );
                }

                // Text:
                if ( ( pData->eType == MENUITEM_STRING ) || ( pData->eType == MENUITEM_STRINGIMAGE ) )
                {
                    aTmpPos.X() = aPos.X() + nTextPos;
                    aTmpPos.Y() = aPos.Y();
                    aTmpPos.Y() += nTextOffsetY;
                    sal_uInt16 nStyle = nTextStyle|TEXT_DRAW_MNEMONIC;
                    if ( pData->bIsTemporary )
                        nStyle |= TEXT_DRAW_DISABLE;
                    MetricVector* pVector = bLayout ? &mpLayoutData->m_aUnicodeBoundRects : NULL;
                    OUString* pDisplayText = bLayout ? &mpLayoutData->m_aDisplayText : NULL;
                    if( bLayout )
                    {
                        mpLayoutData->m_aLineIndices.push_back( mpLayoutData->m_aDisplayText.getLength() );
                        mpLayoutData->m_aLineItemIds.push_back( pData->nId );
                        mpLayoutData->m_aLineItemPositions.push_back( n );
                    }
                    // #i47946# with NWF painted menus the background is transparent
                    // since DrawCtrlText can depend on the background (e.g. for
                    // TEXT_DRAW_DISABLE), temporarily set a background which
                    // hopefully matches the NWF background since it is read
                    // from the system style settings
                    bool bSetTmpBackground = !pWin->IsBackground() && pWin->IsNativeControlSupported( CTRL_MENU_POPUP, PART_ENTIRE_CONTROL );
                    if( bSetTmpBackground )
                    {
                        Color aBg = bIsMenuBar ?
                            pWin->GetSettings().GetStyleSettings().GetMenuBarColor() :
                            pWin->GetSettings().GetStyleSettings().GetMenuColor();
                        pWin->SetBackground( Wallpaper( aBg ) );
                    }
                    // how much space is there for the text ?
                    long nMaxItemTextWidth = aOutSz.Width() - aTmpPos.X() - nExtra - nOuterSpaceX;
                    if( !bIsMenuBar && pData->aAccelKey.GetCode() && !ImplAccelDisabled() )
                    {
                        OUString aAccText = pData->aAccelKey.GetName();
                        nMaxItemTextWidth -= pWin->GetTextWidth( aAccText ) + 3*nExtra;
                    }
                    if( !bIsMenuBar && pData->pSubMenu )
                    {
                        nMaxItemTextWidth -= nFontHeight - nExtra;
                    }
                    OUString aItemText( getShortenedString( pData->aText, pWin, nMaxItemTextWidth ) );
                    pWin->DrawCtrlText( aTmpPos, aItemText, 0, aItemText.getLength(), nStyle, pVector, pDisplayText );
                    if( bSetTmpBackground )
                        pWin->SetBackground();
                }

                // Accel
                if ( !bLayout && !bIsMenuBar && pData->aAccelKey.GetCode() && !ImplAccelDisabled() )
                {
                    OUString aAccText = pData->aAccelKey.GetName();
                    aTmpPos.X() = aOutSz.Width() - pWin->GetTextWidth( aAccText );
                    aTmpPos.X() -= 4*nExtra;

                    aTmpPos.X() -= nOuterSpaceX;
                    aTmpPos.Y() = aPos.Y();
                    aTmpPos.Y() += nTextOffsetY;
                    pWin->DrawCtrlText( aTmpPos, aAccText, 0, aAccText.getLength(), nTextStyle );
                }

                // SubMenu?
                if ( !bLayout && !bIsMenuBar && pData->pSubMenu )
                {
                    bool bNativeOk = false;
                    if( pWin->IsNativeControlSupported( CTRL_MENU_POPUP,
                                                        PART_MENU_SUBMENU_ARROW ) )
                    {
                        ControlState nState = 0;
                        Size aTmpSz( 0, 0 );
                        long aSpacing = 0;

                        if( !ImplGetNativeSubmenuArrowSize( pWin,
                                                            aTmpSz, aSpacing ) )
                        {
                            aTmpSz = Size( nFontHeight, nFontHeight );
                            aSpacing = nOuterSpaceX;
                        }

                        if ( pData->bEnabled )
                            nState |= CTRL_STATE_ENABLED;
                        if ( bHighlighted )
                            nState |= CTRL_STATE_SELECTED;

                        aTmpPos.X() = aOutSz.Width() - aTmpSz.Width() - aSpacing - nOuterSpaceX;
                        aTmpPos.Y() = aPos.Y() + ( pData->aSz.Height() - aTmpSz.Height() ) / 2;
                        aTmpPos.Y() += nExtra/2;

                        Rectangle aItemRect( aTmpPos, aTmpSz );
                        MenupopupValue aVal( nTextPos-GUTTERBORDER, aItemRect );
                        bNativeOk = pWin->DrawNativeControl( CTRL_MENU_POPUP,
                                                             PART_MENU_SUBMENU_ARROW,
                                                             aItemRect,
                                                             nState,
                                                             aVal,
                                                             OUString() );
                    }
                    if( ! bNativeOk )
                    {
                        aTmpPos.X() = aOutSz.Width() - nFontHeight + nExtra - nOuterSpaceX;
                        aTmpPos.Y() = aPos.Y();
                        aTmpPos.Y() += nExtra/2;
                        aTmpPos.Y() += ( pData->aSz.Height() / 2 ) - ( nFontHeight/4 );
                        if ( pData->nBits & MIB_POPUPSELECT )
                        {
                            pWin->SetTextColor( rSettings.GetMenuTextColor() );
                            Point aTmpPos2( aPos );
                            aTmpPos2.X() = aOutSz.Width() - nFontHeight - nFontHeight/4;
                            aDecoView.DrawFrame(
                                Rectangle( aTmpPos2, Size( nFontHeight+nFontHeight/4, pData->aSz.Height() ) ), FRAME_DRAW_GROUP );
                        }
                        aDecoView.DrawSymbol(
                            Rectangle( aTmpPos, Size( nFontHeight/2, nFontHeight/2 ) ),
                            SYMBOL_SPIN_RIGHT, pWin->GetTextColor(), nSymbolStyle );
                    }
                }

                if ( pThisItemOnly && bHighlighted )
                {
                    // This restores the normal menu or menu bar text
                    // color for when it is no longer highlighted.
                    if ( bIsMenuBar )
                        pWin->SetTextColor( rSettings.GetMenuBarTextColor() );
                    else
                        pWin->SetTextColor( rSettings.GetMenuTextColor() );
                }
            }
            if( bLayout )
            {
                if ( !bIsMenuBar )
                    mpLayoutData->m_aVisibleItemBoundRects[ n ] = Rectangle( aTopLeft, Size( aOutSz.Width(), pData->aSz.Height() ) );
                else
                    mpLayoutData->m_aVisibleItemBoundRects[ n ] = Rectangle( aTopLeft, pData->aSz );
            }
        }

        if ( !bIsMenuBar )
        {
            aTopLeft.Y() += pData->aSz.Height();
        }
        else
        {
            aTopLeft.X() += pData->aSz.Width();
        }
    }

    if ( !bLayout && !pThisItemOnly && pLogo )
    {
        Size aLogoSz = pLogo->aBitmap.GetSizePixel();

        Rectangle aRect( Point( 0, 0 ), Point( aLogoSz.Width()-1, aOutSz.Height() ) );
        if ( pWin->GetColorCount() >= 256 )
        {
            Gradient aGrad( GradientStyle_LINEAR, pLogo->aStartColor, pLogo->aEndColor );
            aGrad.SetAngle( 1800 );
            aGrad.SetBorder( 15 );
            pWin->DrawGradient( aRect, aGrad );
        }
        else
        {
            pWin->SetFillColor( pLogo->aStartColor );
            pWin->DrawRect( aRect );
        }

        Point aLogoPos( 0, aOutSz.Height() - aLogoSz.Height() );
        pLogo->aBitmap.Draw( pWin, aLogoPos );
    }
}

Menu* Menu::ImplGetStartMenu()
{
    Menu* pStart = this;
    while ( pStart && pStart->pStartedFrom && ( pStart->pStartedFrom != pStart ) )
        pStart = pStart->pStartedFrom;
    return pStart;
}

void Menu::ImplCallHighlight(sal_uInt16 nItem)
{
    ImplMenuDelData aDelData( this );

    nSelectedId = 0;
    MenuItemData* pData = pItemList->GetDataFromPos(nItem);
    if ( pData )
        nSelectedId = pData->nId;
    ImplCallEventListeners( VCLEVENT_MENU_HIGHLIGHT, GetItemPos( GetCurItemId() ) );

    if( !aDelData.isDeleted() )
    {
        Highlight();
        nSelectedId = 0;
    }
}

IMPL_LINK_NOARG(Menu, ImplCallSelect)
{
    nEventId = 0;
    Select();
    return 0;
}

Menu* Menu::ImplFindSelectMenu()
{
    Menu* pSelMenu = nEventId ? this : NULL;

    for ( size_t n = GetItemList()->size(); n && !pSelMenu; )
    {
        MenuItemData* pData = GetItemList()->GetDataFromPos( --n );

        if ( pData->pSubMenu )
            pSelMenu = pData->pSubMenu->ImplFindSelectMenu();
    }

    return pSelMenu;
}

Menu* Menu::ImplFindMenu( sal_uInt16 nItemId )
{
    Menu* pSelMenu = NULL;

    for ( size_t n = GetItemList()->size(); n && !pSelMenu; )
    {
        MenuItemData* pData = GetItemList()->GetDataFromPos( --n );

        if( pData->nId == nItemId )
            pSelMenu = this;
        else if ( pData->pSubMenu )
            pSelMenu = pData->pSubMenu->ImplFindMenu( nItemId );
    }

    return pSelMenu;
}

void Menu::RemoveDisabledEntries( bool bCheckPopups, bool bRemoveEmptyPopups )
{
    for ( sal_uInt16 n = 0; n < GetItemCount(); n++ )
    {
        bool bRemove = false;
        MenuItemData* pItem = pItemList->GetDataFromPos( n );
        if ( pItem->eType == MENUITEM_SEPARATOR )
        {
            if ( !n || ( GetItemType( n-1 ) == MENUITEM_SEPARATOR ) )
                bRemove = true;
        }
        else
            bRemove = !pItem->bEnabled;

        if ( bCheckPopups && pItem->pSubMenu )
        {
            pItem->pSubMenu->RemoveDisabledEntries( true );
            if ( bRemoveEmptyPopups && !pItem->pSubMenu->GetItemCount() )
                bRemove = true;
        }

        if ( bRemove )
            RemoveItem( n-- );
    }

    if ( GetItemCount() )
    {
        sal_uInt16 nLast = GetItemCount() - 1;
        MenuItemData* pItem = pItemList->GetDataFromPos( nLast );
        if ( pItem->eType == MENUITEM_SEPARATOR )
            RemoveItem( nLast );
    }
    delete mpLayoutData, mpLayoutData = NULL;
}

bool Menu::HasValidEntries( bool bCheckPopups )
{
    bool bValidEntries = false;
    sal_uInt16 nCount = GetItemCount();
    for ( sal_uInt16 n = 0; !bValidEntries && ( n < nCount ); n++ )
    {
        MenuItemData* pItem = pItemList->GetDataFromPos( n );
        if ( pItem->bEnabled && ( pItem->eType != MENUITEM_SEPARATOR ) )
        {
            if ( bCheckPopups && pItem->pSubMenu )
                bValidEntries = pItem->pSubMenu->HasValidEntries( true );
            else
                bValidEntries = true;
        }
    }
    return bValidEntries;
}

void Menu::ImplKillLayoutData() const
{
    delete mpLayoutData, mpLayoutData = NULL;
}

void Menu::ImplFillLayoutData() const
{
    if( pWindow && pWindow->IsReallyVisible() )
    {
        mpLayoutData = new MenuLayoutData();
        if( bIsMenuBar )
        {
            ImplPaint( pWindow, 0, 0, 0, false, true );
        }
        else
        {
            MenuFloatingWindow* pFloat = (MenuFloatingWindow*)pWindow;
            ImplPaint( pWindow, pFloat->nScrollerHeight, pFloat->ImplGetStartY(), 0, false, true );
        }
    }
}

Rectangle Menu::GetCharacterBounds( sal_uInt16 nItemID, long nIndex ) const
{
    long nItemIndex = -1;
    if( ! mpLayoutData )
        ImplFillLayoutData();
    if( mpLayoutData )
    {
        for( size_t i = 0; i < mpLayoutData->m_aLineItemIds.size(); i++ )
        {
            if( mpLayoutData->m_aLineItemIds[i] == nItemID )
            {
                nItemIndex = mpLayoutData->m_aLineIndices[i];
                break;
            }
        }
    }
    return (mpLayoutData && nItemIndex != -1) ? mpLayoutData->GetCharacterBounds( nItemIndex+nIndex ) : Rectangle();
}

long Menu::GetIndexForPoint( const Point& rPoint, sal_uInt16& rItemID ) const
{
    long nIndex = -1;
    rItemID = 0;
    if( ! mpLayoutData )
        ImplFillLayoutData();
    if( mpLayoutData )
    {
        nIndex = mpLayoutData->GetIndexForPoint( rPoint );
        for( size_t i = 0; i < mpLayoutData->m_aLineIndices.size(); i++ )
        {
            if( mpLayoutData->m_aLineIndices[i] <= nIndex &&
                (i == mpLayoutData->m_aLineIndices.size()-1 || mpLayoutData->m_aLineIndices[i+1] > nIndex) )
            {
                // make index relative to item
                nIndex -= mpLayoutData->m_aLineIndices[i];
                rItemID = mpLayoutData->m_aLineItemIds[i];
                break;
            }
        }
    }
    return nIndex;
}

Rectangle Menu::GetBoundingRectangle( sal_uInt16 nPos ) const
{
    Rectangle aRet;

    if( ! mpLayoutData )
        ImplFillLayoutData();
    if( mpLayoutData )
    {
        std::map< sal_uInt16, Rectangle >::const_iterator it = mpLayoutData->m_aVisibleItemBoundRects.find( nPos );
        if( it != mpLayoutData->m_aVisibleItemBoundRects.end() )
            aRet = it->second;
    }
    return aRet;
}

void Menu::SetAccessibleName( sal_uInt16 nItemId, const OUString& rStr )
{
    size_t        nPos;
    MenuItemData* pData = pItemList->GetData( nItemId, nPos );

    if ( pData && !rStr.equals( pData->aAccessibleName ) )
    {
        pData->aAccessibleName = rStr;
        ImplCallEventListeners( VCLEVENT_MENU_ACCESSIBLENAMECHANGED, nPos );
    }
}

OUString Menu::GetAccessibleName( sal_uInt16 nItemId ) const
{
    MenuItemData* pData = pItemList->GetData( nItemId );

    if ( pData )
        return pData->aAccessibleName;

    return OUString();
}

void Menu::ImplSetSalMenu( SalMenu *pSalMenu )
{
    if( mpSalMenu )
        ImplGetSVData()->mpDefInst->DestroyMenu( mpSalMenu );
    mpSalMenu = pSalMenu;
}

bool Menu::GetSystemMenuData( SystemMenuData* pData ) const
{
    Menu* pMenu = (Menu*)this;
    if( pData && pMenu->ImplGetSalMenu() )
    {
        pMenu->ImplGetSalMenu()->GetSystemMenuData( pData );
        return true;
    }
    else
        return false;
}

bool Menu::IsHighlighted( sal_uInt16 nItemPos ) const
{
    bool bRet = false;

    if( pWindow )
    {
        if( bIsMenuBar )
            bRet = ( nItemPos == static_cast< MenuBarWindow * > (pWindow)->GetHighlightedItem() );
        else
            bRet = ( nItemPos == static_cast< MenuFloatingWindow * > (pWindow)->GetHighlightedItem() );
    }

    return bRet;
}

void Menu::HighlightItem( sal_uInt16 nItemPos )
{
    if ( pWindow )
    {
        if ( bIsMenuBar )
        {
            MenuBarWindow* pMenuWin = static_cast< MenuBarWindow* >( pWindow );
            pMenuWin->SetAutoPopup( false );
            pMenuWin->ChangeHighlightItem( nItemPos, false );
        }
        else
        {
            static_cast< MenuFloatingWindow* >( pWindow )->ChangeHighlightItem( nItemPos, false );
        }
    }
}

// - MenuBar -

MenuBar::MenuBar() : Menu( true )
{
    mbDisplayable       = true;
    mbCloserVisible     = false;
    mbFloatBtnVisible   = false;
    mbHideBtnVisible    = false;
}

MenuBar::MenuBar( const MenuBar& rMenu ) : Menu( true )
{
    mbDisplayable       = true;
    mbCloserVisible     = false;
    mbFloatBtnVisible   = false;
    mbHideBtnVisible    = false;
    *this               = rMenu;
    bIsMenuBar          = true;
}

MenuBar::~MenuBar()
{
    ImplDestroy( this, true );
}

void MenuBar::ShowCloser( bool bShow )
{
    ShowButtons( bShow, mbFloatBtnVisible, mbHideBtnVisible );
}

void MenuBar::ShowButtons( bool bClose, bool bFloat, bool bHide )
{
    if ( (bClose != mbCloserVisible)    ||
         (bFloat != mbFloatBtnVisible)  ||
         (bHide  != mbHideBtnVisible) )
    {
        mbCloserVisible     = bClose;
        mbFloatBtnVisible   = bFloat;
        mbHideBtnVisible    = bHide;
        if ( ImplGetWindow() )
            ((MenuBarWindow*)ImplGetWindow())->ShowButtons( bClose, bFloat, bHide );
    }
}

void MenuBar::SetDisplayable( bool bDisplayable )
{
    if( bDisplayable != mbDisplayable )
    {
        mbDisplayable = bDisplayable;
        MenuBarWindow* pMenuWin = (MenuBarWindow*) ImplGetWindow();
        if( pMenuWin )
            pMenuWin->ImplLayoutChanged();
    }
}

Window* MenuBar::ImplCreate( Window* pParent, Window* pWindow, MenuBar* pMenu )
{
    if ( !pWindow )
        pWindow = new MenuBarWindow( pParent );

    pMenu->pStartedFrom = 0;
    pMenu->pWindow = pWindow;
    ((MenuBarWindow*)pWindow)->SetMenu( pMenu );
    long nHeight = pMenu->ImplCalcSize( pWindow ).Height();

    // depending on the native implementation or the displayable flag
    // the menubar windows is suppressed (ie, height=0)
    if( !((MenuBar*) pMenu)->IsDisplayable() ||
        ( pMenu->ImplGetSalMenu() && pMenu->ImplGetSalMenu()->VisibleMenuBar() ) )
        nHeight = 0;

    pWindow->setPosSizePixel( 0, 0, 0, nHeight, WINDOW_POSSIZE_HEIGHT );
    return pWindow;
}

void MenuBar::ImplDestroy( MenuBar* pMenu, bool bDelete )
{
    MenuBarWindow* pWindow = (MenuBarWindow*) pMenu->ImplGetWindow();
    if ( pWindow && bDelete )
    {
        pWindow->KillActivePopup();
        delete pWindow;
    }
    pMenu->pWindow = NULL;
}

bool MenuBar::ImplHandleKeyEvent( const KeyEvent& rKEvent, bool bFromMenu )
{
    bool bDone = false;

    // No keyboard processing when system handles the menu or our menubar is invisible
    if( !IsDisplayable() ||
        ( ImplGetSalMenu() && ImplGetSalMenu()->VisibleMenuBar() ) )
        return bDone;

    // check for enabled, if this method is called from another window...
    Window* pWin = ImplGetWindow();
    if ( pWin && pWin->IsEnabled() && pWin->IsInputEnabled()  && ! pWin->IsInModalMode() )
        bDone = ((MenuBarWindow*)pWin)->ImplHandleKeyEvent( rKEvent, bFromMenu );
    return bDone;
}

void MenuBar::SelectEntry( sal_uInt16 nId )
{
    MenuBarWindow* pMenuWin = (MenuBarWindow*) ImplGetWindow();

    if( pMenuWin )
    {
        pMenuWin->GrabFocus();
        nId = GetItemPos( nId );

        // #99705# popup the selected menu
        pMenuWin->SetAutoPopup( true );
        if( ITEMPOS_INVALID != pMenuWin->nHighlightedItem )
        {
            pMenuWin->KillActivePopup();
            pMenuWin->ChangeHighlightItem( ITEMPOS_INVALID, false );
        }
        if( nId != ITEMPOS_INVALID )
            pMenuWin->ChangeHighlightItem( nId, false );
    }
}

// handler for native menu selection and command events

bool MenuBar::HandleMenuActivateEvent( Menu *pMenu ) const
{
    if( pMenu )
    {
        ImplMenuDelData aDelData( this );

        pMenu->pStartedFrom = (Menu*)this;
        pMenu->bInCallback = true;
        pMenu->Activate();

        if( !aDelData.isDeleted() )
            pMenu->bInCallback = false;
    }
    return true;
}

bool MenuBar::HandleMenuDeActivateEvent( Menu *pMenu ) const
{
    if( pMenu )
    {
        ImplMenuDelData aDelData( this );

        pMenu->pStartedFrom = (Menu*)this;
        pMenu->bInCallback = true;
        pMenu->Deactivate();
        if( !aDelData.isDeleted() )
            pMenu->bInCallback = false;
    }
    return true;
}

bool MenuBar::HandleMenuHighlightEvent( Menu *pMenu, sal_uInt16 nHighlightEventId ) const
{
    if( !pMenu )
        pMenu = ((Menu*) this)->ImplFindMenu( nHighlightEventId );
    if( pMenu )
    {
        ImplMenuDelData aDelData( pMenu );

        if( mnHighlightedItemPos != ITEMPOS_INVALID )
            pMenu->ImplCallEventListeners( VCLEVENT_MENU_DEHIGHLIGHT, mnHighlightedItemPos );

        if( !aDelData.isDeleted() )
        {
            pMenu->mnHighlightedItemPos = pMenu->GetItemPos( nHighlightEventId );
            pMenu->nSelectedId = nHighlightEventId;
            pMenu->pStartedFrom = (Menu*)this;
            pMenu->ImplCallHighlight( pMenu->mnHighlightedItemPos );
        }
        return true;
    }
    else
        return false;
}

bool MenuBar::HandleMenuCommandEvent( Menu *pMenu, sal_uInt16 nCommandEventId ) const
{
    if( !pMenu )
        pMenu = ((Menu*) this)->ImplFindMenu( nCommandEventId );
    if( pMenu )
    {
        pMenu->nSelectedId = nCommandEventId;
        pMenu->pStartedFrom = (Menu*)this;
        pMenu->ImplSelect();
        return true;
    }
    else
        return false;
}

sal_uInt16 MenuBar::AddMenuBarButton( const Image& i_rImage, const Link& i_rLink, const OUString& i_rToolTip, sal_uInt16 i_nPos )
{
    return pWindow ? static_cast<MenuBarWindow*>(pWindow)->AddMenuBarButton( i_rImage, i_rLink, i_rToolTip, i_nPos ) : 0;
}

void MenuBar::SetMenuBarButtonHighlightHdl( sal_uInt16 nId, const Link& rLink )
{
    if( pWindow )
        static_cast<MenuBarWindow*>(pWindow)->SetMenuBarButtonHighlightHdl( nId, rLink );
}

Rectangle MenuBar::GetMenuBarButtonRectPixel( sal_uInt16 nId )
{
    return pWindow ? static_cast<MenuBarWindow*>(pWindow)->GetMenuBarButtonRectPixel( nId ) : Rectangle();
}

void MenuBar::RemoveMenuBarButton( sal_uInt16 nId )
{
    if( pWindow )
        static_cast<MenuBarWindow*>(pWindow)->RemoveMenuBarButton( nId );
}

bool MenuBar::HandleMenuButtonEvent( Menu *, sal_uInt16 i_nButtonId ) const
{
    return static_cast<MenuBarWindow*>(pWindow)->HandleMenuButtonEvent( i_nButtonId );
}

// bool PopupMenu::bAnyPopupInExecute = false;

PopupMenu::PopupMenu()
{
    pRefAutoSubMenu = NULL;
}

PopupMenu::PopupMenu( const ResId& rResId )
{
    pRefAutoSubMenu = NULL;
    ImplLoadRes( rResId );
}

PopupMenu::PopupMenu( const PopupMenu& rMenu ) : Menu()
{
    pRefAutoSubMenu = NULL;
    *this = rMenu;
}

PopupMenu::~PopupMenu()
{
    if( pRefAutoSubMenu && *pRefAutoSubMenu == this )
        *pRefAutoSubMenu = NULL;    // #111060# avoid second delete in ~MenuItemData
}

bool PopupMenu::IsInExecute()
{
    return GetActivePopupMenu() ? true : false;
}

PopupMenu* PopupMenu::GetActivePopupMenu()
{
    ImplSVData* pSVData = ImplGetSVData();
    return pSVData->maAppData.mpActivePopupMenu;
}

void PopupMenu::EndExecute( sal_uInt16 nSelectId )
{
    if ( ImplGetWindow() )
        ImplGetFloatingWindow()->EndExecute( nSelectId );
}

void PopupMenu::SelectEntry( sal_uInt16 nId )
{
    if ( ImplGetWindow() )
    {
        if( nId != ITEMPOS_INVALID )
        {
            size_t nPos = 0;
            MenuItemData* pData = GetItemList()->GetData( nId, nPos );
            if (pData && pData->pSubMenu)
                ImplGetFloatingWindow()->ChangeHighlightItem( nPos, true );
            else
                ImplGetFloatingWindow()->EndExecute( nId );
        }
        else
        {
            MenuFloatingWindow* pFloat = ImplGetFloatingWindow();
            pFloat->GrabFocus();

            for( size_t nPos = 0; nPos < GetItemList()->size(); nPos++ )
            {
                MenuItemData* pData = GetItemList()->GetDataFromPos( nPos );
                if( pData->pSubMenu )
                {
                    pFloat->KillActivePopup();
                }
            }
            pFloat->ChangeHighlightItem( ITEMPOS_INVALID, false );
        }
    }
}

void PopupMenu::SetSelectedEntry( sal_uInt16 nId )
{
    nSelectedId = nId;
}

sal_uInt16 PopupMenu::Execute( Window* pExecWindow, const Point& rPopupPos )
{
    return Execute( pExecWindow, Rectangle( rPopupPos, rPopupPos ), POPUPMENU_EXECUTE_DOWN );
}

sal_uInt16 PopupMenu::Execute( Window* pExecWindow, const Rectangle& rRect, sal_uInt16 nFlags )
{
    ENSURE_OR_RETURN( pExecWindow, "PopupMenu::Execute: need a non-NULL window!", 0 );

    sal_uLong nPopupModeFlags = 0;
    if ( nFlags & POPUPMENU_EXECUTE_DOWN )
        nPopupModeFlags = FLOATWIN_POPUPMODE_DOWN;
    else if ( nFlags & POPUPMENU_EXECUTE_UP )
        nPopupModeFlags = FLOATWIN_POPUPMODE_UP;
    else if ( nFlags & POPUPMENU_EXECUTE_LEFT )
        nPopupModeFlags = FLOATWIN_POPUPMODE_LEFT;
    else if ( nFlags & POPUPMENU_EXECUTE_RIGHT )
        nPopupModeFlags = FLOATWIN_POPUPMODE_RIGHT;
    else
        nPopupModeFlags = FLOATWIN_POPUPMODE_DOWN;

    if (nFlags & POPUPMENU_NOMOUSEUPCLOSE )                      // allow popup menus to stay open on mouse button up
        nPopupModeFlags |= FLOATWIN_POPUPMODE_NOMOUSEUPCLOSE;    // useful if the menu was opened on mousebutton down (eg toolbox configuration)

    if (nFlags & POPUPMENU_NOHORZ_PLACEMENT)
        nPopupModeFlags |= FLOATWIN_POPUPMODE_NOHORZPLACEMENT;

    return ImplExecute( pExecWindow, rRect, nPopupModeFlags, 0, false );
}

sal_uInt16 PopupMenu::ImplExecute( Window* pW, const Rectangle& rRect, sal_uLong nPopupModeFlags, Menu* pSFrom, bool bPreSelectFirst )
{
    if ( !pSFrom && ( PopupMenu::IsInExecute() || !GetItemCount() ) )
        return 0;

    delete mpLayoutData, mpLayoutData = NULL;

    ImplSVData* pSVData = ImplGetSVData();

    pStartedFrom = pSFrom;
    nSelectedId = 0;
    bCanceled = false;

    sal_uLong nFocusId = 0;
    bool bRealExecute = false;
    if ( !pStartedFrom )
    {
        pSVData->maWinData.mbNoDeactivate = true;
        nFocusId = Window::SaveFocus();
        bRealExecute = true;
    }
    else
    {
        // assure that only one menu is open at a time
        if( pStartedFrom->bIsMenuBar && pSVData->maWinData.mpFirstFloat )
            pSVData->maWinData.mpFirstFloat->EndPopupMode( FLOATWIN_POPUPMODEEND_CANCEL | FLOATWIN_POPUPMODEEND_CLOSEALL );
    }

    DBG_ASSERT( !ImplGetWindow(), "Win?!" );
    Rectangle aRect( rRect );
    aRect.SetPos( pW->OutputToScreenPixel( aRect.TopLeft() ) );

    WinBits nStyle = WB_BORDER;
    if ( bRealExecute )
        nPopupModeFlags |= FLOATWIN_POPUPMODE_NEWLEVEL;
    if ( !pStartedFrom || !pStartedFrom->bIsMenuBar )
        nPopupModeFlags |= FLOATWIN_POPUPMODE_PATHMOUSECANCELCLICK | FLOATWIN_POPUPMODE_ALLMOUSEBUTTONCLOSE;

    nPopupModeFlags |= FLOATWIN_POPUPMODE_NOKEYCLOSE;

    // could be useful during debugging.
    // nPopupModeFlags |= FLOATWIN_POPUPMODE_NOFOCUSCLOSE;

    ImplDelData aDelData;
    pW->ImplAddDel( &aDelData );

    bInCallback = true; // set it here, if Activate overloaded
    Activate();
    bInCallback = false;

    if ( aDelData.IsDead() )
        return 0;   // Error

    pW->ImplRemoveDel( &aDelData );

    if ( bCanceled || bKilled )
        return 0;

    if ( !GetItemCount() )
        return 0;

    // The flag MENU_FLAG_HIDEDISABLEDENTRIES is inherited.
    if ( pSFrom )
    {
        if ( pSFrom->nMenuFlags & MENU_FLAG_HIDEDISABLEDENTRIES )
            nMenuFlags |= MENU_FLAG_HIDEDISABLEDENTRIES;
        else
            nMenuFlags &= ~MENU_FLAG_HIDEDISABLEDENTRIES;
    }
    else
        // #102790# context menus shall never show disabled entries
        nMenuFlags |= MENU_FLAG_HIDEDISABLEDENTRIES;

    sal_uInt16 nVisibleEntries = ImplGetVisibleItemCount();
    if ( !nVisibleEntries )
    {
        ResMgr* pResMgr = ImplGetResMgr();
        if( pResMgr )
        {
            OUString aTmpEntryText( ResId( SV_RESID_STRING_NOSELECTIONPOSSIBLE, *pResMgr ) );
            MenuItemData* pData = pItemList->Insert(
                0xFFFF, MENUITEM_STRING, 0, aTmpEntryText, Image(), NULL, 0xFFFF, OString() );
            size_t nPos = 0;
            pData = pItemList->GetData( pData->nId, nPos );
            assert(pData);
            if (pData)
            {
                pData->bIsTemporary = true;
            }
            ImplCallEventListeners(VCLEVENT_MENU_SUBMENUCHANGED, nPos);
        }
    }
    else if ( Application::GetSettings().GetStyleSettings().GetAutoMnemonic() && !( nMenuFlags & MENU_FLAG_NOAUTOMNEMONICS ) )
    {
        CreateAutoMnemonics();
    }

    MenuFloatingWindow* pWin = new MenuFloatingWindow( this, pW, nStyle | WB_SYSTEMWINDOW );
    if( pSVData->maNWFData.mbFlatMenu )
        pWin->SetBorderStyle( WINDOW_BORDER_NOBORDER );
    else
        pWin->SetBorderStyle( pWin->GetBorderStyle() | WINDOW_BORDER_MENU );
    pWindow = pWin;

    Size aSz = ImplCalcSize( pWin );

    Rectangle aDesktopRect(pWin->GetDesktopRectPixel());
    if( Application::GetScreenCount() > 1 && Application::IsUnifiedDisplay() )
    {
        Window* pDeskW = pWindow->GetWindow( WINDOW_REALPARENT );
        if( ! pDeskW )
            pDeskW = pWindow;
        Point aDesktopTL( pDeskW->OutputToAbsoluteScreenPixel( aRect.TopLeft() ) );
        aDesktopRect = Application::GetScreenPosSizePixel(
            Application::GetBestScreen( Rectangle( aDesktopTL, aRect.GetSize() ) ));
    }

    long nMaxHeight = aDesktopRect.GetHeight();

    //rhbz#1021915. If a menu won't fit in the desired location the default
    //mode is to place it somewhere it will fit.  e.g. above, left, right. For
    //some cases, e.g. menubars, it's desirable to limit the options to
    //above/below and force the menu to scroll if it won't fit
    if (nPopupModeFlags & FLOATWIN_POPUPMODE_NOHORZPLACEMENT)
    {
        Window* pRef = pWin;
        if ( pRef->GetParent() )
            pRef = pRef->GetParent();

        Rectangle devRect(  pRef->OutputToAbsoluteScreenPixel( aRect.TopLeft() ),
                            pRef->OutputToAbsoluteScreenPixel( aRect.BottomRight() ) );

        long nHeightAbove = devRect.Top() - aDesktopRect.Top();
        long nHeightBelow = aDesktopRect.Bottom() - devRect.Bottom();
        nMaxHeight = std::min(nMaxHeight, std::max(nHeightAbove, nHeightBelow));
    }

    if ( pStartedFrom && pStartedFrom->bIsMenuBar )
        nMaxHeight -= pW->GetSizePixel().Height();
    sal_Int32 nLeft, nTop, nRight, nBottom;
    pWindow->GetBorder( nLeft, nTop, nRight, nBottom );
    nMaxHeight -= nTop+nBottom;
    if ( aSz.Height() > nMaxHeight )
    {
        pWin->EnableScrollMenu( true );
        sal_uInt16 nStart = ImplGetFirstVisible();
        sal_uInt16 nEntries = ImplCalcVisEntries( nMaxHeight, nStart );
        aSz.Height() = ImplCalcHeight( nEntries );
    }

    pWin->SetFocusId( nFocusId );
    pWin->SetOutputSizePixel( aSz );
    // #102158# menus must never grab the focus, otherwise
    // they will be closed immediately
    // from now on focus grabbing is only prohibited automatically if
    // FLOATWIN_POPUPMODE_GRABFOCUS was set (which is done below), because some
    // floaters (like floating toolboxes) may grab the focus
    // pWin->GrabFocus();
    if ( GetItemCount() )
    {
        SalMenu* pMenu = ImplGetSalMenu();
        if( pMenu && bRealExecute && pMenu->ShowNativePopupMenu( pWin, aRect, nPopupModeFlags | FLOATWIN_POPUPMODE_GRABFOCUS ) )
        {
            pWin->StopExecute(0);
            pWin->doShutdown();
            pWindow->doLazyDelete();
            pWindow = NULL;
            return nSelectedId;
        }
        else
        {
            pWin->StartPopupMode( aRect, nPopupModeFlags | FLOATWIN_POPUPMODE_GRABFOCUS );
        }
        if( pSFrom )
        {
            sal_uInt16 aPos;
            if( pSFrom->bIsMenuBar )
                aPos = ((MenuBarWindow *) pSFrom->pWindow)->GetHighlightedItem();
            else
                aPos = ((MenuFloatingWindow *) pSFrom->pWindow)->GetHighlightedItem();

            pWin->SetPosInParent( aPos );  // store position to be sent in SUBMENUDEACTIVATE
            pSFrom->ImplCallEventListeners( VCLEVENT_MENU_SUBMENUACTIVATE, aPos );
        }
    }
    if ( bPreSelectFirst )
    {
        size_t nCount = pItemList->size();
        for ( size_t n = 0; n < nCount; n++ )
        {
            MenuItemData* pData = pItemList->GetDataFromPos( n );
            if (  (  pData->bEnabled
                  || !Application::GetSettings().GetStyleSettings().GetSkipDisabledInMenus()
                  )
               && ( pData->eType != MENUITEM_SEPARATOR )
               && ImplIsVisible( n )
               && ImplIsSelectable( n )
               )
            {
                pWin->ChangeHighlightItem( n, false );
                break;
            }
        }
    }
    if ( bRealExecute )
    {
        pWin->ImplAddDel( &aDelData );

        ImplDelData aModalWinDel;
        pW->ImplAddDel( &aModalWinDel );
        pW->ImplIncModalCount();

        pWin->Execute();

        DBG_ASSERT( ! aModalWinDel.IsDead(), "window for popup died, modal count incorrect !" );
        if( ! aModalWinDel.IsDead() )
            pW->ImplDecModalCount();

        if ( !aDelData.IsDead() )
            pWin->ImplRemoveDel( &aDelData );
        else
            return 0;

        // Restore focus (could already have been
        // restored in Select)
        nFocusId = pWin->GetFocusId();
        if ( nFocusId )
        {
            pWin->SetFocusId( 0 );
            pSVData->maWinData.mbNoDeactivate = false;
        }
        pWin->ImplEndPopupMode( 0, nFocusId );

        if ( nSelectedId )  // then clean up .. ( otherwise done by TH )
        {
            PopupMenu* pSub = pWin->GetActivePopup();
            while ( pSub )
            {
                pSub->ImplGetFloatingWindow()->EndPopupMode();
                pSub = pSub->ImplGetFloatingWindow()->GetActivePopup();
            }
        }
        pWin->doShutdown();
        pWindow->doLazyDelete();
        pWindow = NULL;

        // is there still Select?
        Menu* pSelect = ImplFindSelectMenu();
        if ( pSelect )
        {
            // Select should be called prior to leaving execute in a popup menu!
            Application::RemoveUserEvent( pSelect->nEventId );
            pSelect->nEventId = 0;
            pSelect->Select();
        }
    }

    return bRealExecute ? nSelectedId : 0;
}

sal_uInt16 PopupMenu::ImplCalcVisEntries( long nMaxHeight, sal_uInt16 nStartEntry, sal_uInt16* pLastVisible ) const
{
    nMaxHeight -= 2 * ImplGetFloatingWindow()->GetScrollerHeight();

    long nHeight = 0;
    size_t nEntries = pItemList->size();
    sal_uInt16 nVisEntries = 0;

    if ( pLastVisible )
        *pLastVisible = 0;

    for ( size_t n = nStartEntry; n < nEntries; n++ )
    {
        if ( ImplIsVisible( n ) )
        {
            MenuItemData* pData = pItemList->GetDataFromPos( n );
            nHeight += pData->aSz.Height();
            if ( nHeight > nMaxHeight )
                break;

            if ( pLastVisible )
                *pLastVisible = n;
            nVisEntries++;
        }
    }
    return nVisEntries;
}

long PopupMenu::ImplCalcHeight( sal_uInt16 nEntries ) const
{
    long nHeight = 0;

    sal_uInt16 nFound = 0;
    for ( size_t n = 0; ( nFound < nEntries ) && ( n < pItemList->size() ); n++ )
    {
        if ( ImplIsVisible( (sal_uInt16) n ) )
        {
            MenuItemData* pData = pItemList->GetDataFromPos( n );
            nHeight += pData->aSz.Height();
            nFound++;
        }
    }

    nHeight += 2*ImplGetFloatingWindow()->GetScrollerHeight();

    return nHeight;
}

static void ImplInitMenuWindow( Window* pWin, bool bFont, bool bMenuBar )
{
    const StyleSettings& rStyleSettings = pWin->GetSettings().GetStyleSettings();

    if ( bFont )
        pWin->SetPointFont( rStyleSettings.GetMenuFont() );
    if( bMenuBar )
    {
        const BitmapEx& rPersonaBitmap = Application::GetSettings().GetStyleSettings().GetPersonaHeader();
        if ( !rPersonaBitmap.IsEmpty() )
        {
            Wallpaper aWallpaper( rPersonaBitmap );
            aWallpaper.SetStyle( WALLPAPER_TOPRIGHT );
            aWallpaper.SetColor( Application::GetSettings().GetStyleSettings().GetWorkspaceColor() );

            pWin->SetBackground( aWallpaper );
            pWin->SetPaintTransparent( false );
            pWin->SetParentClipMode( 0 );
        }
        else if ( pWin->IsNativeControlSupported( CTRL_MENUBAR, PART_ENTIRE_CONTROL ) )
        {
            pWin->SetBackground();  // background will be drawn by NWF
        }
        else
        {
            Wallpaper aWallpaper;
            aWallpaper.SetStyle( WALLPAPER_APPLICATIONGRADIENT );
            pWin->SetBackground( aWallpaper );
            pWin->SetPaintTransparent( false );
            pWin->SetParentClipMode( 0 );
        }
    }
    else
    {
        if( pWin->IsNativeControlSupported( CTRL_MENU_POPUP, PART_ENTIRE_CONTROL ) )
        {
            pWin->SetBackground();  // background will be drawn by NWF
        }
        else
            pWin->SetBackground( Wallpaper( rStyleSettings.GetMenuColor() ) );
    }

    if ( bMenuBar )
        pWin->SetTextColor( rStyleSettings.GetMenuBarTextColor() );
    else
        pWin->SetTextColor( rStyleSettings.GetMenuTextColor() );
    pWin->SetTextFillColor();
    pWin->SetLineColor();
}

MenuFloatingWindow::MenuFloatingWindow( Menu* pMen, Window* pParent, WinBits nStyle ) :
    FloatingWindow( pParent, nStyle )
{
    mpWindowImpl->mbMenuFloatingWindow= true;
    pMenu               = pMen;
    pActivePopup        = 0;
    nSaveFocusId        = 0;
    bInExecute          = false;
    bScrollMenu         = false;
    nHighlightedItem    = ITEMPOS_INVALID;
    nMBDownPos          = ITEMPOS_INVALID;
    nPosInParent        = ITEMPOS_INVALID;
    nScrollerHeight     = 0;
    nBorder             = EXTRASPACEY;
    nFirstEntry         = 0;
    bScrollUp           = false;
    bScrollDown         = false;
    bIgnoreFirstMove    = true;
    bKeyInput           = false;

    EnableSaveBackground();
    ImplInitMenuWindow( this, true, false );

    SetPopupModeEndHdl( LINK( this, MenuFloatingWindow, PopupEnd ) );

    aHighlightChangedTimer.SetTimeoutHdl( LINK( this, MenuFloatingWindow, HighlightChanged ) );
    aHighlightChangedTimer.SetTimeout( GetSettings().GetMouseSettings().GetMenuDelay() );
    aSubmenuCloseTimer.SetTimeout( GetSettings().GetMouseSettings().GetMenuDelay() );
    aSubmenuCloseTimer.SetTimeoutHdl( LINK( this, MenuFloatingWindow, SubmenuClose ) );
    aScrollTimer.SetTimeoutHdl( LINK( this, MenuFloatingWindow, AutoScroll ) );

    AddEventListener( LINK( this, MenuFloatingWindow, ShowHideListener ) );
}

void MenuFloatingWindow::doShutdown()
{
    if( pMenu )
    {
        // #105373# notify toolkit that highlight was removed
        // otherwise the entry will not be read when the menu is opened again
        if( nHighlightedItem != ITEMPOS_INVALID )
            pMenu->ImplCallEventListeners( VCLEVENT_MENU_DEHIGHLIGHT, nHighlightedItem );
        pMenu->SetHighlightItem(ITEMPOS_INVALID);
        if( !bKeyInput && pMenu && pMenu->pStartedFrom && !pMenu->pStartedFrom->bIsMenuBar )
        {
            // #102461# remove highlight in parent
            MenuItemData* pData;
            size_t i, nCount = pMenu->pStartedFrom->pItemList->size();
            for(i = 0; i < nCount; i++)
            {
                pData = pMenu->pStartedFrom->pItemList->GetDataFromPos( i );
                if( pData && ( pData->pSubMenu == pMenu ) )
                    break;
            }
            if( i < nCount )
            {
                MenuFloatingWindow* pPWin = (MenuFloatingWindow*)pMenu->pStartedFrom->ImplGetWindow();
                if( pPWin )
                    pPWin->HighlightItem( i, false );
            }
        }

        // free the reference to the accessible component
        SetAccessible( ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible >() );

        aHighlightChangedTimer.Stop();

        // #95056# invalidate screen area covered by system window
        // so this can be taken into account if the commandhandler performs a scroll operation
        if( GetParent() )
        {
            Rectangle aInvRect( GetWindowExtentsRelative( GetParent() ) );
            GetParent()->Invalidate( aInvRect );
        }
        pMenu = NULL;
        RemoveEventListener( LINK( this, MenuFloatingWindow, ShowHideListener ) );
    }
}

MenuFloatingWindow::~MenuFloatingWindow()
{
    doShutdown();
}

void MenuFloatingWindow::Resize()
{
    InitMenuClipRegion();
}

long MenuFloatingWindow::ImplGetStartY() const
{
    long nY = 0;
    if( pMenu )
    {
        for ( sal_uInt16 n = 0; n < nFirstEntry; n++ )
            nY += pMenu->GetItemList()->GetDataFromPos( n )->aSz.Height();
    }
    return -nY;
}

Region MenuFloatingWindow::ImplCalcClipRegion( bool bIncludeLogo ) const
{
    Size aOutSz = GetOutputSizePixel();
    Point aPos;
    Rectangle aRect( aPos, aOutSz );
    aRect.Top() += nScrollerHeight;
    aRect.Bottom() -= nScrollerHeight;

    if ( pMenu && pMenu->pLogo && !bIncludeLogo )
        aRect.Left() += pMenu->pLogo->aBitmap.GetSizePixel().Width();

    Region aRegion(aRect);
    if ( pMenu && pMenu->pLogo && bIncludeLogo && nScrollerHeight )
        aRegion.Union( Rectangle( Point(), Size( pMenu->pLogo->aBitmap.GetSizePixel().Width(), aOutSz.Height() ) ) );

    return aRegion;
}

void MenuFloatingWindow::InitMenuClipRegion()
{
    if ( IsScrollMenu() )
    {
        SetClipRegion( ImplCalcClipRegion() );
    }
    else
    {
        SetClipRegion();
    }
}

void MenuFloatingWindow::ImplHighlightItem( const MouseEvent& rMEvt, bool bMBDown )
{
    if( ! pMenu )
        return;

    long nY = nScrollerHeight + ImplGetSVData()->maNWFData.mnMenuFormatBorderY;
    long nMouseY = rMEvt.GetPosPixel().Y();
    Size aOutSz = GetOutputSizePixel();
    if ( ( nMouseY >= nY ) && ( nMouseY < ( aOutSz.Height() - nY ) ) )
    {
        bool bHighlighted = false;
        size_t nCount = pMenu->pItemList->size();
        nY += ImplGetStartY();  // ggf. gescrollt.
        for ( size_t n = 0; !bHighlighted && ( n < nCount ); n++ )
        {
            if ( pMenu->ImplIsVisible( n ) )
            {
                MenuItemData* pItemData = pMenu->pItemList->GetDataFromPos( n );
                long nOldY = nY;
                nY += pItemData->aSz.Height();
                if ( ( nOldY <= nMouseY ) && ( nY > nMouseY ) && pMenu->ImplIsSelectable( n ) )
                {
                    bool bPopupArea = true;
                    if ( pItemData->nBits & MIB_POPUPSELECT )
                    {
                        // only when clicked over the arrow...
                        Size aSz = GetOutputSizePixel();
                        long nFontHeight = GetTextHeight();
                        bPopupArea = ( rMEvt.GetPosPixel().X() >= ( aSz.Width() - nFontHeight - nFontHeight/4 ) );
                    }

                    if ( bMBDown )
                    {
                        if ( n != nHighlightedItem )
                        {
                            ChangeHighlightItem( (sal_uInt16)n, false );
                        }

                        bool bAllowNewPopup = true;
                        if ( pActivePopup )
                        {
                            MenuItemData* pData = pMenu->pItemList->GetDataFromPos( n );
                            bAllowNewPopup = pData && ( pData->pSubMenu != pActivePopup );
                            if ( bAllowNewPopup )
                                KillActivePopup();
                        }

                        if ( bPopupArea && bAllowNewPopup )
                        {
                            HighlightChanged( NULL );
                        }
                    }
                    else
                    {
                        if ( n != nHighlightedItem )
                        {
                            ChangeHighlightItem( (sal_uInt16)n, true );
                        }
                        else if ( pItemData->nBits & MIB_POPUPSELECT )
                        {
                            if ( bPopupArea && ( pActivePopup != pItemData->pSubMenu ) )
                                HighlightChanged( NULL );
                        }
                    }
                    bHighlighted = true;
                }
            }
        }
        if ( !bHighlighted )
            ChangeHighlightItem( ITEMPOS_INVALID, true );
    }
    else
    {
        ImplScroll( rMEvt.GetPosPixel() );
        ChangeHighlightItem( ITEMPOS_INVALID, true );
    }
}

IMPL_LINK_NOARG(MenuFloatingWindow, PopupEnd)
{
    // "this" will be deleted before the end of this method!
    Menu* pM = pMenu;
    if ( bInExecute )
    {
        if ( pActivePopup )
        {
            //DBG_ASSERT( !pActivePopup->ImplGetWindow(), "PopupEnd, obwohl pActivePopup MIT Window!" );
            KillActivePopup(); // should be ok to just remove it
            //pActivePopup->bCanceled = true;
        }
        bInExecute = false;
        pMenu->bInCallback = true;
        pMenu->Deactivate();
        pMenu->bInCallback = false;
    }
    else
    {
        if( pMenu )
        {
            // if the window was closed by TH, there is another menu
            // which has this window as pActivePopup
            if ( pMenu->pStartedFrom )
            {
                // pWin from parent could be 0, if the list is
                // cleaned from the start, now clean up the endpopup-events
                if ( pMenu->pStartedFrom->bIsMenuBar )
                {
                    MenuBarWindow* p = (MenuBarWindow*) pMenu->pStartedFrom->ImplGetWindow();
                    if ( p )
                        p->PopupClosed( pMenu );
                }
                else
                {
                    MenuFloatingWindow* p = (MenuFloatingWindow*) pMenu->pStartedFrom->ImplGetWindow();
                    if ( p )
                        p->KillActivePopup( (PopupMenu*)pMenu );
                }
            }
        }
    }

    if ( pM )
        pM->pStartedFrom = 0;

    return 0;
}

IMPL_LINK_NOARG(MenuFloatingWindow, AutoScroll)
{
    ImplScroll( GetPointerPosPixel() );
    return 1;
}

IMPL_LINK( MenuFloatingWindow, HighlightChanged, Timer*, pTimer )
{
    if( ! pMenu )
        return 0;

    MenuItemData* pItemData = pMenu->pItemList->GetDataFromPos( nHighlightedItem );
    if ( pItemData )
    {
        if ( pActivePopup && ( pActivePopup != pItemData->pSubMenu ) )
        {
            sal_uLong nOldFlags = GetPopupModeFlags();
            SetPopupModeFlags( GetPopupModeFlags() | FLOATWIN_POPUPMODE_NOAPPFOCUSCLOSE );
            KillActivePopup();
            SetPopupModeFlags( nOldFlags );
        }
        if ( pItemData->bEnabled && pItemData->pSubMenu && pItemData->pSubMenu->GetItemCount() && ( pItemData->pSubMenu != pActivePopup ) )
        {
            pActivePopup = (PopupMenu*)pItemData->pSubMenu;
            long nY = nScrollerHeight+ImplGetStartY();
            MenuItemData* pData = 0;
            for ( sal_uLong n = 0; n < nHighlightedItem; n++ )
            {
                pData = pMenu->pItemList->GetDataFromPos( n );
                nY += pData->aSz.Height();
            }
            pData = pMenu->pItemList->GetDataFromPos( nHighlightedItem );
            Size MySize = GetOutputSizePixel();
            Point aItemTopLeft( 0, nY );
            Point aItemBottomRight( aItemTopLeft );
            aItemBottomRight.X() += MySize.Width();
            aItemBottomRight.Y() += pData->aSz.Height();

            // shift the popups a little
            aItemTopLeft.X() += 2;
            aItemBottomRight.X() -= 2;
            if ( nHighlightedItem )
                aItemTopLeft.Y() -= 2;
            else
            {
                sal_Int32 nL, nT, nR, nB;
                GetBorder( nL, nT, nR, nB );
                aItemTopLeft.Y() -= nT;
            }

            // pTest: crash due to Reschedule() in call of Activate()
            // Also it is prevented that submenus are displayed which
            // were for long in Activate Rescheduled and which should not be
            // displayed now.
            Menu* pTest = pActivePopup;
            sal_uLong nOldFlags = GetPopupModeFlags();
            SetPopupModeFlags( GetPopupModeFlags() | FLOATWIN_POPUPMODE_NOAPPFOCUSCLOSE );
            sal_uInt16 nRet = pActivePopup->ImplExecute( this, Rectangle( aItemTopLeft, aItemBottomRight ), FLOATWIN_POPUPMODE_RIGHT, pMenu, pTimer ? false : true  );
            SetPopupModeFlags( nOldFlags );

            // nRet != 0, wenn es waerend Activate() abgeschossen wurde...
            if ( !nRet && ( pActivePopup == pTest ) && pActivePopup->ImplGetWindow() )
                pActivePopup->ImplGetFloatingWindow()->AddPopupModeWindow( this );
        }
    }

    return 0;
}

IMPL_LINK_NOARG(MenuFloatingWindow, SubmenuClose)
{
    if( pMenu && pMenu->pStartedFrom )
    {
        MenuFloatingWindow* pWin = (MenuFloatingWindow*) pMenu->pStartedFrom->GetWindow();
        if( pWin )
            pWin->KillActivePopup();
    }
    return 0;
}

IMPL_LINK( MenuFloatingWindow, ShowHideListener, VclWindowEvent*, pEvent )
{
    if( ! pMenu )
        return 0;

    if( pEvent->GetId() == VCLEVENT_WINDOW_SHOW )
        pMenu->ImplCallEventListeners( VCLEVENT_MENU_SHOW, ITEMPOS_INVALID );
    else if( pEvent->GetId() == VCLEVENT_WINDOW_HIDE )
        pMenu->ImplCallEventListeners( VCLEVENT_MENU_HIDE, ITEMPOS_INVALID );
    return 0;
}

void MenuFloatingWindow::EnableScrollMenu( bool b )
{
    bScrollMenu = b;
    nScrollerHeight = b ? (sal_uInt16) GetSettings().GetStyleSettings().GetScrollBarSize() /2 : 0;
    bScrollDown = true;
    InitMenuClipRegion();
}

void MenuFloatingWindow::Execute()
{
    ImplSVData* pSVData = ImplGetSVData();

    pSVData->maAppData.mpActivePopupMenu = (PopupMenu*)pMenu;

    bInExecute = true;
//  bCallingSelect = false;

    while ( bInExecute )
        Application::Yield();

    pSVData->maAppData.mpActivePopupMenu = NULL;
}

void MenuFloatingWindow::StopExecute( sal_uLong nFocusId )
{
    // restore focus
    // (could have been restored in Select)
    if ( nSaveFocusId )
    {
        Window::EndSaveFocus( nFocusId, false );
        nFocusId = nSaveFocusId;
        if ( nFocusId )
        {
            nSaveFocusId = 0;
            ImplGetSVData()->maWinData.mbNoDeactivate = false;
        }
    }
    ImplEndPopupMode( 0, nFocusId );

    aHighlightChangedTimer.Stop();
    bInExecute = false;
    if ( pActivePopup )
    {
        KillActivePopup();
    }
    // notify parent, needed for accessibility
    if( pMenu && pMenu->pStartedFrom )
        pMenu->pStartedFrom->ImplCallEventListeners( VCLEVENT_MENU_SUBMENUDEACTIVATE, nPosInParent );
}

void MenuFloatingWindow::KillActivePopup( PopupMenu* pThisOnly )
{
    if ( pActivePopup && ( !pThisOnly || ( pThisOnly == pActivePopup ) ) )
    {
        if( pActivePopup->pWindow != NULL )
            if( ((FloatingWindow *) pActivePopup->pWindow)->IsInCleanUp() )
                return; // kill it later
        if ( pActivePopup->bInCallback )
            pActivePopup->bCanceled = true;

        // For all actions pActivePopup = 0, if e.g.
        // PopupModeEndHdl the popups to destroy were called synchronous
        PopupMenu* pPopup = pActivePopup;
        pActivePopup = NULL;
        pPopup->bInCallback = true;
        pPopup->Deactivate();
        pPopup->bInCallback = false;
        if ( pPopup->ImplGetWindow() )
        {
            pPopup->ImplGetFloatingWindow()->StopExecute();
            pPopup->ImplGetFloatingWindow()->doShutdown();
            pPopup->pWindow->doLazyDelete();
            pPopup->pWindow = NULL;

            Update();
        }
    }
}

void MenuFloatingWindow::EndExecute()
{
    Menu* pStart = pMenu ? pMenu->ImplGetStartMenu() : NULL;
    sal_uLong nFocusId = 0;
    if ( pStart && pStart->bIsMenuBar )
    {
        nFocusId = ((MenuBarWindow*)((MenuBar*)pStart)->ImplGetWindow())->GetFocusId();
        if ( nFocusId )
        {
            ((MenuBarWindow*)((MenuBar*)pStart)->ImplGetWindow())->SetFocusId( 0 );
            ImplGetSVData()->maWinData.mbNoDeactivate = false;
        }
    }

    // if started else where, cleanup there as well
    MenuFloatingWindow* pCleanUpFrom = this;
    MenuFloatingWindow* pWin = this;
    while ( pWin && !pWin->bInExecute &&
        pWin->pMenu->pStartedFrom && !pWin->pMenu->pStartedFrom->bIsMenuBar )
    {
        pWin = ((PopupMenu*)pWin->pMenu->pStartedFrom)->ImplGetFloatingWindow();
    }
    if ( pWin )
        pCleanUpFrom = pWin;

    // this window will be destroyed => store date locally...
    Menu* pM = pMenu;
    sal_uInt16 nItem = nHighlightedItem;

    pCleanUpFrom->StopExecute( nFocusId );

    if ( nItem != ITEMPOS_INVALID && pM )
    {
        MenuItemData* pItemData = pM->GetItemList()->GetDataFromPos( nItem );
        if ( pItemData && !pItemData->bIsTemporary )
        {
            pM->nSelectedId = pItemData->nId;
            if ( pStart )
                pStart->nSelectedId = pItemData->nId;

            pM->ImplSelect();
        }
    }
}

void MenuFloatingWindow::EndExecute( sal_uInt16 nId )
{
    size_t nPos;
    if ( pMenu && pMenu->GetItemList()->GetData( nId, nPos ) )
        nHighlightedItem = nPos;
    else
        nHighlightedItem = ITEMPOS_INVALID;

    EndExecute();
}

void MenuFloatingWindow::MouseButtonDown( const MouseEvent& rMEvt )
{
    // TH creates a ToTop on this window, but the active popup
    // should stay on top...
    // due to focus change this would close all menus -> don't do it (#94123)
    //if ( pActivePopup && pActivePopup->ImplGetWindow() && !pActivePopup->ImplGetFloatingWindow()->pActivePopup )
    //    pActivePopup->ImplGetFloatingWindow()->ToTop( TOTOP_NOGRABFOCUS );

    ImplHighlightItem( rMEvt, true );

    nMBDownPos = nHighlightedItem;
}

void MenuFloatingWindow::MouseButtonUp( const MouseEvent& rMEvt )
{
    MenuItemData* pData = pMenu ? pMenu->GetItemList()->GetDataFromPos( nHighlightedItem ) : NULL;
    // nMBDownPos store in local variable and reset immediately,
    // as it will be too late after EndExecute
    sal_uInt16 _nMBDownPos = nMBDownPos;
    nMBDownPos = ITEMPOS_INVALID;
    if ( pData && pData->bEnabled && ( pData->eType != MENUITEM_SEPARATOR ) )
    {
        if ( !pData->pSubMenu )
        {
            EndExecute();
        }
        else if ( ( pData->nBits & MIB_POPUPSELECT ) && ( nHighlightedItem == _nMBDownPos ) && ( rMEvt.GetClicks() == 2 ) )
        {
            // not when clicked over the arrow...
            Size aSz = GetOutputSizePixel();
            long nFontHeight = GetTextHeight();
            if ( rMEvt.GetPosPixel().X() < ( aSz.Width() - nFontHeight - nFontHeight/4 ) )
                EndExecute();
        }
    }

}

void MenuFloatingWindow::MouseMove( const MouseEvent& rMEvt )
{
    if ( !IsVisible() || rMEvt.IsSynthetic() || rMEvt.IsEnterWindow() )
        return;

    if ( rMEvt.IsLeaveWindow() )
    {
        // #102461# do not remove highlight if a popup menu is open at this position
        MenuItemData* pData = pMenu ? pMenu->pItemList->GetDataFromPos( nHighlightedItem ) : NULL;
        // close popup with some delayed if we leave somewhere else
        if( pActivePopup && pData && pData->pSubMenu != pActivePopup )
            pActivePopup->ImplGetFloatingWindow()->aSubmenuCloseTimer.Start();

        if( !pActivePopup || (pData && pData->pSubMenu != pActivePopup ) )
            ChangeHighlightItem( ITEMPOS_INVALID, false );

        if ( IsScrollMenu() )
            ImplScroll( rMEvt.GetPosPixel() );
    }
    else
    {
        aSubmenuCloseTimer.Stop();
        if( bIgnoreFirstMove )
            bIgnoreFirstMove = false;
        else
            ImplHighlightItem( rMEvt, false );
    }
}

void MenuFloatingWindow::ImplScroll( bool bUp )
{
    KillActivePopup();
    Update();

    if( ! pMenu )
        return;

    HighlightItem( nHighlightedItem, false );

    pMenu->ImplKillLayoutData();

    if ( bScrollUp && bUp )
    {
        nFirstEntry = pMenu->ImplGetPrevVisible( nFirstEntry );
        DBG_ASSERT( nFirstEntry != ITEMPOS_INVALID, "Scroll?!" );

        long nScrollEntryHeight = pMenu->GetItemList()->GetDataFromPos( nFirstEntry )->aSz.Height();

        if ( !bScrollDown )
        {
            bScrollDown = true;
            ImplDrawScroller( false );
        }

        if ( pMenu->ImplGetPrevVisible( nFirstEntry ) == ITEMPOS_INVALID )
        {
            bScrollUp = false;
            ImplDrawScroller( true );
        }

        Scroll( 0, nScrollEntryHeight, ImplCalcClipRegion( false ).GetBoundRect(), SCROLL_CLIP );
    }
    else if ( bScrollDown && !bUp )
    {
        long nScrollEntryHeight = pMenu->GetItemList()->GetDataFromPos( nFirstEntry )->aSz.Height();

        nFirstEntry = pMenu->ImplGetNextVisible( nFirstEntry );
        DBG_ASSERT( nFirstEntry != ITEMPOS_INVALID, "Scroll?!" );

        if ( !bScrollUp )
        {
            bScrollUp = true;
            ImplDrawScroller( true );
        }

        long nHeight = GetOutputSizePixel().Height();
        sal_uInt16 nLastVisible;
        ((PopupMenu*)pMenu)->ImplCalcVisEntries( nHeight, nFirstEntry, &nLastVisible );
        if ( pMenu->ImplGetNextVisible( nLastVisible ) == ITEMPOS_INVALID )
        {
            bScrollDown = false;
            ImplDrawScroller( false );
        }

        Scroll( 0, -nScrollEntryHeight, ImplCalcClipRegion( false ).GetBoundRect(), SCROLL_CLIP );
    }

    HighlightItem( nHighlightedItem, true );
}

void MenuFloatingWindow::ImplScroll( const Point& rMousePos )
{
    Size aOutSz = GetOutputSizePixel();

    long nY = nScrollerHeight;
    long nMouseY = rMousePos.Y();
    long nDelta = 0;

    if ( bScrollUp && ( nMouseY < nY ) )
    {
        ImplScroll( true );
        nDelta = nY - nMouseY;
    }
    else if ( bScrollDown && ( nMouseY > ( aOutSz.Height() - nY ) ) )
    {
        ImplScroll( false );
        nDelta = nMouseY - ( aOutSz.Height() - nY );
    }

    if ( nDelta )
    {
        aScrollTimer.Stop();    // if scrolled through MouseMove.
        long nTimeout;
        if ( nDelta < 3 )
            nTimeout = 200;
        else if ( nDelta < 5 )
            nTimeout = 100;
        else if ( nDelta < 8 )
            nTimeout = 70;
        else if ( nDelta < 12 )
            nTimeout = 40;
        else
            nTimeout = 20;
        aScrollTimer.SetTimeout( nTimeout );
        aScrollTimer.Start();
    }
}
void MenuFloatingWindow::ChangeHighlightItem( sal_uInt16 n, bool bStartPopupTimer )
{
    // #57934# ggf. immediately close the active, as TH's backgroundstorage works.
    // #65750# we prefer to refrain from the background storage of small lines.
    //         otherwise the menus are difficult to operate.
    //  MenuItemData* pNextData = pMenu->pItemList->GetDataFromPos( n );
    //  if ( pActivePopup && pNextData && ( pActivePopup != pNextData->pSubMenu ) )
    //      KillActivePopup();

    aSubmenuCloseTimer.Stop();
    if( ! pMenu )
        return;

    if ( nHighlightedItem != ITEMPOS_INVALID )
    {
        HighlightItem( nHighlightedItem, false );
        pMenu->ImplCallEventListeners( VCLEVENT_MENU_DEHIGHLIGHT, nHighlightedItem );
    }

    nHighlightedItem = (sal_uInt16)n;
    DBG_ASSERT( pMenu->ImplIsVisible( nHighlightedItem ) || nHighlightedItem == ITEMPOS_INVALID, "ChangeHighlightItem: Not visible!" );
    if( nHighlightedItem != ITEMPOS_INVALID )
    {
        if( pMenu->pStartedFrom && !pMenu->pStartedFrom->bIsMenuBar )
        {
            // #102461# make sure parent entry is highlighted as well
            MenuItemData* pData;
            size_t i, nCount = pMenu->pStartedFrom->pItemList->size();
            for(i = 0; i < nCount; i++)
            {
                pData = pMenu->pStartedFrom->pItemList->GetDataFromPos( i );
                if( pData && ( pData->pSubMenu == pMenu ) )
                    break;
            }
            if( i < nCount )
            {
                MenuFloatingWindow* pPWin = (MenuFloatingWindow*)pMenu->pStartedFrom->ImplGetWindow();
                if( pPWin && pPWin->nHighlightedItem != i )
                {
                    pPWin->HighlightItem( i, true );
                    pPWin->nHighlightedItem = i;
                }
            }
        }
        HighlightItem( nHighlightedItem, true );
        pMenu->SetHighlightItem(nHighlightedItem);
        pMenu->ImplCallHighlight( nHighlightedItem );
    }
    else
        pMenu->nSelectedId = 0;

    if ( bStartPopupTimer )
    {
        // #102438# Menu items are not selectable
        // If a menu item is selected by an AT-tool via the XAccessibleAction, XAccessibleValue
        // or XAccessibleSelection interface, and the parent popup menus are not executed yet,
        // the parent popup menus must be executed SYNCHRONOUSLY, before the menu item is selected.
        if ( GetSettings().GetMouseSettings().GetMenuDelay() )
            aHighlightChangedTimer.Start();
        else
            HighlightChanged( &aHighlightChangedTimer );
    }
}

void MenuFloatingWindow::HighlightItem( sal_uInt16 nPos, bool bHighlight )
{
    if( ! pMenu )
        return;

    Size    aSz = GetOutputSizePixel();
    long    nStartY = ImplGetStartY();
    long    nY = nScrollerHeight + nStartY + ImplGetSVData()->maNWFData.mnMenuFormatBorderY;
    long    nX = 0;

    if ( pMenu->pLogo )
        nX = pMenu->pLogo->aBitmap.GetSizePixel().Width();

    int nOuterSpaceX = ImplGetSVData()->maNWFData.mnMenuFormatBorderX;

    size_t nCount = pMenu->pItemList->size();
    for ( size_t n = 0; n < nCount; n++ )
    {
        MenuItemData* pData = pMenu->pItemList->GetDataFromPos( n );
        if ( n == nPos )
        {
            DBG_ASSERT( pMenu->ImplIsVisible( n ), "Highlight: Item not visible!" );
            if ( pData->eType != MENUITEM_SEPARATOR )
            {
                bool bRestoreLineColor = false;
                Color oldLineColor;
                bool bDrawItemRect = true;

                Rectangle aItemRect( Point( nX+nOuterSpaceX, nY ), Size( aSz.Width()-2*nOuterSpaceX, pData->aSz.Height() ) );
                if ( pData->nBits & MIB_POPUPSELECT )
                {
                    long nFontHeight = GetTextHeight();
                    aItemRect.Right() -= nFontHeight + nFontHeight/4;
                }

                if( IsNativeControlSupported( CTRL_MENU_POPUP, PART_ENTIRE_CONTROL ) )
                {
                    Size aPxSize( GetOutputSizePixel() );
                    Push( PUSH_CLIPREGION );
                    IntersectClipRegion( Rectangle( Point( nX, nY ), Size( aSz.Width(), pData->aSz.Height() ) ) );
                    Rectangle aCtrlRect( Point( nX, 0 ), Size( aPxSize.Width()-nX, aPxSize.Height() ) );
                    MenupopupValue aVal( pMenu->nTextPos-GUTTERBORDER, aItemRect );
                    DrawNativeControl( CTRL_MENU_POPUP, PART_ENTIRE_CONTROL,
                                       aCtrlRect,
                                       CTRL_STATE_ENABLED,
                                       aVal,
                                       OUString() );
                    if( bHighlight &&
                        IsNativeControlSupported( CTRL_MENU_POPUP, PART_MENU_ITEM ) )
                    {
                        bDrawItemRect = false;
                        if( !DrawNativeControl( CTRL_MENU_POPUP, PART_MENU_ITEM,
                                                        aItemRect,
                                                        CTRL_STATE_SELECTED | ( pData->bEnabled? CTRL_STATE_ENABLED: 0 ),
                                                        aVal,
                                                        OUString() ) )
                        {
                            bDrawItemRect = bHighlight;
                        }
                    }
                    else
                        bDrawItemRect = bHighlight;
                    Pop();
                }
                if( bDrawItemRect )
                {
                    if ( bHighlight )
                    {
                        if( pData->bEnabled )
                            SetFillColor( GetSettings().GetStyleSettings().GetMenuHighlightColor() );
                        else
                        {
                            SetFillColor();
                            oldLineColor = GetLineColor();
                            SetLineColor( GetSettings().GetStyleSettings().GetMenuHighlightColor() );
                            bRestoreLineColor = true;
                        }
                    }
                    else
                        SetFillColor( GetSettings().GetStyleSettings().GetMenuColor() );

                    DrawRect( aItemRect );
                }
                pMenu->ImplPaint( this, nScrollerHeight, nStartY, pData, bHighlight );
                if( bRestoreLineColor )
                    SetLineColor( oldLineColor );
            }
            return;
        }

        nY += pData->aSz.Height();
    }
}

Rectangle MenuFloatingWindow::ImplGetItemRect( sal_uInt16 nPos )
{
    if( ! pMenu )
        return Rectangle();

    Rectangle aRect;
    Size    aSz = GetOutputSizePixel();
    long    nStartY = ImplGetStartY();
    long    nY = nScrollerHeight+nStartY;
    long    nX = 0;

    if ( pMenu->pLogo )
        nX = pMenu->pLogo->aBitmap.GetSizePixel().Width();

    size_t nCount = pMenu->pItemList->size();
    for ( size_t n = 0; n < nCount; n++ )
    {
        MenuItemData* pData = pMenu->pItemList->GetDataFromPos( n );
        if ( n == nPos )
        {
            DBG_ASSERT( pMenu->ImplIsVisible( n ), "ImplGetItemRect: Item not visible!" );
            if ( pData->eType != MENUITEM_SEPARATOR )
            {
                aRect = Rectangle( Point( nX, nY ), Size( aSz.Width(), pData->aSz.Height() ) );
                if ( pData->nBits & MIB_POPUPSELECT )
                {
                    long nFontHeight = GetTextHeight();
                    aRect.Right() -= nFontHeight + nFontHeight/4;
                }
            }
            break;
        }
        nY += pData->aSz.Height();
    }
    return aRect;
}

void MenuFloatingWindow::ImplCursorUpDown( bool bUp, bool bHomeEnd )
{
    if( ! pMenu )
        return;

    const StyleSettings& rSettings = GetSettings().GetStyleSettings();

    sal_uInt16 n = nHighlightedItem;
    if ( n == ITEMPOS_INVALID )
    {
        if ( bUp )
            n = 0;
        else
            n = pMenu->GetItemCount()-1;
    }

    sal_uInt16 nLoop = n;

    if( bHomeEnd )
    {
        // absolute positioning
        if( bUp )
        {
            n = pMenu->GetItemCount();
            nLoop = n-1;
        }
        else
        {
            n = (sal_uInt16)-1;
            nLoop = n+1;
        }
    }

    do
    {
        if ( bUp )
        {
            if ( n )
                n--;
            else
                if ( !IsScrollMenu() || ( nHighlightedItem == ITEMPOS_INVALID ) )
                    n = pMenu->GetItemCount()-1;
                else
                    break;
        }
        else
        {
            n++;
            if ( n >= pMenu->GetItemCount() )
            {
                if ( !IsScrollMenu() || ( nHighlightedItem == ITEMPOS_INVALID ) )
                    n = 0;
                else
                    break;
            }
        }

        MenuItemData* pData = (MenuItemData*)pMenu->GetItemList()->GetDataFromPos( n );
        if ( ( pData->bEnabled || !rSettings.GetSkipDisabledInMenus() )
              && ( pData->eType != MENUITEM_SEPARATOR ) && pMenu->ImplIsVisible( n ) && pMenu->ImplIsSelectable( n ) )
        {
            // Is selection in visible area?
            if ( IsScrollMenu() )
            {
                ChangeHighlightItem( ITEMPOS_INVALID, false );

                while ( n < nFirstEntry )
                    ImplScroll( true );

                Size aOutSz = GetOutputSizePixel();
                sal_uInt16 nLastVisible;
                ((PopupMenu*)pMenu)->ImplCalcVisEntries( aOutSz.Height(), nFirstEntry, &nLastVisible );
                while ( n > nLastVisible )
                {
                    ImplScroll( false );
                    ((PopupMenu*)pMenu)->ImplCalcVisEntries( aOutSz.Height(), nFirstEntry, &nLastVisible );
                }
            }
            ChangeHighlightItem( n, false );
            break;
        }
    } while ( n != nLoop );
}

void MenuFloatingWindow::KeyInput( const KeyEvent& rKEvent )
{
    ImplDelData aDelData;
    ImplAddDel( &aDelData );

    sal_uInt16 nCode = rKEvent.GetKeyCode().GetCode();
    bKeyInput = true;
    switch ( nCode )
    {
        case KEY_UP:
        case KEY_DOWN:
        {
            ImplCursorUpDown( nCode == KEY_UP );
        }
        break;
        case KEY_END:
        case KEY_HOME:
        {
            ImplCursorUpDown( nCode == KEY_END, true );
        }
        break;
        case KEY_F6:
        case KEY_ESCAPE:
        {
            // Ctrl-F6 acts like ESC here, the menu bar however will then put the focus in the document
            if( nCode == KEY_F6 && !rKEvent.GetKeyCode().IsMod1() )
                break;
            if( pMenu )
            {
                if ( !pMenu->pStartedFrom )
                {
                    StopExecute();
                    KillActivePopup();
                }
                else if ( pMenu->pStartedFrom->bIsMenuBar )
                {
                    // Forward...
                    ((MenuBarWindow*)((MenuBar*)pMenu->pStartedFrom)->ImplGetWindow())->KeyInput( rKEvent );
                }
                else
                {
                    StopExecute();
                    PopupMenu* pPopupMenu = (PopupMenu*)pMenu->pStartedFrom;
                    MenuFloatingWindow* pFloat = pPopupMenu->ImplGetFloatingWindow();
                    pFloat->GrabFocus();
                    pFloat->KillActivePopup();
                    pPopupMenu->ImplCallHighlight(pFloat->nHighlightedItem);
                }
            }
        }
        break;
        case KEY_LEFT:
        {
            if ( pMenu && pMenu->pStartedFrom )
            {
                StopExecute();
                if ( pMenu->pStartedFrom->bIsMenuBar )
                {
                    // Forward...
                    ((MenuBarWindow*)((MenuBar*)pMenu->pStartedFrom)->ImplGetWindow())->KeyInput( rKEvent );
                }
                else
                {
                    MenuFloatingWindow* pFloat = ((PopupMenu*)pMenu->pStartedFrom)->ImplGetFloatingWindow();
                    pFloat->GrabFocus();
                    pFloat->KillActivePopup();
                    sal_uInt16 highlightItem = pFloat->GetHighlightedItem();
                    pFloat->ChangeHighlightItem(highlightItem, false);
                }
            }
        }
        break;
        case KEY_RIGHT:
        {
            if( pMenu )
            {
                bool bDone = false;
                if ( nHighlightedItem != ITEMPOS_INVALID )
                {
                    MenuItemData* pData = pMenu->GetItemList()->GetDataFromPos( nHighlightedItem );
                    if ( pData && pData->pSubMenu )
                    {
                        HighlightChanged( 0 );
                        bDone = true;
                    }
                }
                if ( !bDone )
                {
                    Menu* pStart = pMenu->ImplGetStartMenu();
                    if ( pStart && pStart->bIsMenuBar )
                    {
                        // Forward...
                        pStart->ImplGetWindow()->KeyInput( rKEvent );
                    }
                }
            }
        }
        break;
        case KEY_RETURN:
        {
            if( pMenu )
            {
                MenuItemData* pData = pMenu->GetItemList()->GetDataFromPos( nHighlightedItem );
                if ( pData && pData->bEnabled )
                {
                    if ( pData->pSubMenu )
                        HighlightChanged( 0 );
                    else
                        EndExecute();
                }
                else
                    StopExecute();
            }
        }
        break;
        case KEY_MENU:
        {
            if( pMenu )
            {
                Menu* pStart = pMenu->ImplGetStartMenu();
                if ( pStart && pStart->bIsMenuBar )
                {
                    // Forward...
                    pStart->ImplGetWindow()->KeyInput( rKEvent );
                }
            }
        }
        break;
        default:
        {
            sal_Unicode nCharCode = rKEvent.GetCharCode();
            sal_uInt16 nPos = 0;
            sal_uInt16 nDuplicates = 0;
            MenuItemData* pData = (nCharCode && pMenu) ? pMenu->GetItemList()->SearchItem( nCharCode, rKEvent.GetKeyCode(), nPos, nDuplicates, nHighlightedItem ) : NULL;
            if ( pData )
            {
                if ( pData->pSubMenu || nDuplicates > 1 )
                {
                    ChangeHighlightItem( nPos, false );
                    HighlightChanged( 0 );
                }
                else
                {
                    nHighlightedItem = nPos;
                    EndExecute();
                }
            }
            else
                FloatingWindow::KeyInput( rKEvent );
        }
    }
    // #105474# check if menu window was not destroyed
    if ( !aDelData.IsDead() )
    {
        ImplRemoveDel( &aDelData );
        bKeyInput = false;
    }
}

void MenuFloatingWindow::Paint( const Rectangle& )
{
    if( ! pMenu )
        return;

    if( IsNativeControlSupported( CTRL_MENU_POPUP, PART_ENTIRE_CONTROL ) )
    {
        SetClipRegion();
        long nX = pMenu->pLogo ? pMenu->pLogo->aBitmap.GetSizePixel().Width() : 0;
        Size aPxSize( GetOutputSizePixel() );
        aPxSize.Width() -= nX;
        ImplControlValue aVal( pMenu->nTextPos-GUTTERBORDER );
        DrawNativeControl( CTRL_MENU_POPUP, PART_ENTIRE_CONTROL,
                           Rectangle( Point( nX, 0 ), aPxSize ),
                           CTRL_STATE_ENABLED,
                           aVal,
                           OUString() );
        InitMenuClipRegion();
    }
    if ( IsScrollMenu() )
    {
        ImplDrawScroller( true );
        ImplDrawScroller( false );
    }
    SetFillColor( GetSettings().GetStyleSettings().GetMenuColor() );
    pMenu->ImplPaint( this, nScrollerHeight, ImplGetStartY() );
    if ( nHighlightedItem != ITEMPOS_INVALID )
        HighlightItem( nHighlightedItem, true );
}

void MenuFloatingWindow::ImplDrawScroller( bool bUp )
{
    if( ! pMenu )
        return;

    SetClipRegion();

    Size aOutSz = GetOutputSizePixel();
    long nY = bUp ? 0 : ( aOutSz.Height() - nScrollerHeight );
    long nX = pMenu->pLogo ? pMenu->pLogo->aBitmap.GetSizePixel().Width() : 0;
    Rectangle aRect( Point( nX, nY ), Size( aOutSz.Width()-nX, nScrollerHeight ) );

    DecorationView aDecoView( this );
    SymbolType eSymbol = bUp ? SYMBOL_SPIN_UP : SYMBOL_SPIN_DOWN;

    sal_uInt16 nStyle = 0;
    if ( ( bUp && !bScrollUp ) || ( !bUp && !bScrollDown ) )
        nStyle |= SYMBOL_DRAW_DISABLE;

    aDecoView.DrawSymbol( aRect, eSymbol, GetSettings().GetStyleSettings().GetButtonTextColor(), nStyle );

    InitMenuClipRegion();
}

void MenuFloatingWindow::RequestHelp( const HelpEvent& rHEvt )
{
    sal_uInt16 nId = nHighlightedItem;
    Menu* pM = pMenu;
    Window* pW = this;

    // #102618# Get item rect before destroying the window in EndExecute() call
    Rectangle aHighlightRect( ImplGetItemRect( nHighlightedItem ) );

    if ( rHEvt.GetMode() & (HELPMODE_CONTEXT | HELPMODE_EXTENDED) )
    {
        nHighlightedItem = ITEMPOS_INVALID;
        EndExecute();
        pW = NULL;
    }

    if( !ImplHandleHelpEvent( pW, pM, nId, rHEvt, aHighlightRect ) )
        Window::RequestHelp( rHEvt );
}

void MenuFloatingWindow::StateChanged( StateChangedType nType )
{
    FloatingWindow::StateChanged( nType );

    if ( ( nType == STATE_CHANGE_CONTROLFOREGROUND ) || ( nType == STATE_CHANGE_CONTROLBACKGROUND ) )
    {
        ImplInitMenuWindow( this, false, false );
        Invalidate();
    }
}

void MenuFloatingWindow::DataChanged( const DataChangedEvent& rDCEvt )
{
    FloatingWindow::DataChanged( rDCEvt );

    if ( (rDCEvt.GetType() == DATACHANGED_FONTS) ||
         (rDCEvt.GetType() == DATACHANGED_FONTSUBSTITUTION) ||
         ((rDCEvt.GetType() == DATACHANGED_SETTINGS) &&
          (rDCEvt.GetFlags() & SETTINGS_STYLE)) )
    {
        ImplInitMenuWindow( this, false, false );
        Invalidate();
    }
}

void MenuFloatingWindow::Command( const CommandEvent& rCEvt )
{
    if ( rCEvt.GetCommand() == COMMAND_WHEEL )
    {
        const CommandWheelData* pData = rCEvt.GetWheelData();
        if( !pData->GetModifier() && ( pData->GetMode() == COMMAND_WHEEL_SCROLL ) )
        {
//          ImplCursorUpDown( pData->GetDelta() > 0L );
            ImplScroll( pData->GetDelta() > 0L );
            MouseMove( MouseEvent( GetPointerPosPixel(), 0 ) );
        }
    }
}

::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > MenuFloatingWindow::CreateAccessible()
{
    ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > xAcc;

    if ( pMenu && !pMenu->pStartedFrom )
        xAcc = pMenu->GetAccessible();

    return xAcc;
}

MenuBarWindow::MenuBarWindow( Window* pParent ) :
    Window( pParent, 0 ),
    aCloser( this ),
    aFloatBtn( this, WB_NOPOINTERFOCUS | WB_SMALLSTYLE | WB_RECTSTYLE ),
    aHideBtn( this, WB_NOPOINTERFOCUS | WB_SMALLSTYLE | WB_RECTSTYLE )
{
    SetType( WINDOW_MENUBARWINDOW );
    pMenu = NULL;
    pActivePopup = NULL;
    nSaveFocusId = 0;
    nHighlightedItem = ITEMPOS_INVALID;
    nRolloveredItem = ITEMPOS_INVALID;
    mbAutoPopup = true;
    nSaveFocusId = 0;
    bIgnoreFirstMove = true;
    bStayActive = false;

    ResMgr* pResMgr = ImplGetResMgr();

    if( pResMgr )
    {
        BitmapEx aBitmap( ResId( SV_RESID_BITMAP_CLOSEDOC, *pResMgr ) );
        aCloser.maImage = Image( aBitmap );

        aCloser.SetOutStyle( TOOLBOX_STYLE_FLAT );
        aCloser.SetBackground();
        aCloser.SetPaintTransparent( true );
        aCloser.SetParentClipMode( PARENTCLIPMODE_NOCLIP );

        aCloser.InsertItem( IID_DOCUMENTCLOSE, aCloser.maImage, 0 );
        aCloser.SetSelectHdl( LINK( this, MenuBarWindow, CloserHdl ) );
        aCloser.AddEventListener( LINK( this, MenuBarWindow, ToolboxEventHdl ) );
        aCloser.SetQuickHelpText( IID_DOCUMENTCLOSE, ResId(SV_HELPTEXT_CLOSEDOCUMENT, *pResMgr).toString() );

        aFloatBtn.SetClickHdl( LINK( this, MenuBarWindow, FloatHdl ) );
        aFloatBtn.SetSymbol( SYMBOL_FLOAT );
        aFloatBtn.SetQuickHelpText( ResId(SV_HELPTEXT_RESTORE, *pResMgr).toString() );

        aHideBtn.SetClickHdl( LINK( this, MenuBarWindow, HideHdl ) );
        aHideBtn.SetSymbol( SYMBOL_HIDE );
        aHideBtn.SetQuickHelpText( ResId(SV_HELPTEXT_MINIMIZE, *pResMgr).toString() );
    }

    ImplInitStyleSettings();

    AddEventListener( LINK( this, MenuBarWindow, ShowHideListener ) );
}

MenuBarWindow::~MenuBarWindow()
{
    aCloser.RemoveEventListener( LINK( this, MenuBarWindow, ToolboxEventHdl ) );
    RemoveEventListener( LINK( this, MenuBarWindow, ShowHideListener ) );
}

void MenuBarWindow::SetMenu( MenuBar* pMen )
{
    pMenu = pMen;
    KillActivePopup();
    nHighlightedItem = ITEMPOS_INVALID;
    ImplInitMenuWindow( this, true, true );
    if ( pMen )
    {
        aCloser.ShowItem( IID_DOCUMENTCLOSE, pMen->HasCloser() );
        aCloser.Show( pMen->HasCloser() || !m_aAddButtons.empty() );
        aFloatBtn.Show( pMen->HasFloatButton() );
        aHideBtn.Show( pMen->HasHideButton() );
    }
    Invalidate();

    // show and connect native menubar
    if( pMenu && pMenu->ImplGetSalMenu() )
    {
        if( pMenu->ImplGetSalMenu()->VisibleMenuBar() )
            ImplGetFrame()->SetMenu( pMenu->ImplGetSalMenu() );

        pMenu->ImplGetSalMenu()->SetFrame( ImplGetFrame() );
    }
}

void MenuBarWindow::ShowButtons( bool bClose, bool bFloat, bool bHide )
{
    aCloser.ShowItem( IID_DOCUMENTCLOSE, bClose );
    aCloser.Show( bClose || ! m_aAddButtons.empty() );
    aFloatBtn.Show( bFloat );
    aHideBtn.Show( bHide );
    Resize();
}

Size MenuBarWindow::MinCloseButtonSize()
{
    return aCloser.getMinSize();
}

IMPL_LINK_NOARG(MenuBarWindow, CloserHdl)
{
    if( ! pMenu )
        return 0;

    if( aCloser.GetCurItemId() == IID_DOCUMENTCLOSE )
    {
        // #i106052# call close hdl asynchronously to ease handler implementation
        // this avoids still being in the handler while the DecoToolBox already
        // gets destroyed
        Application::PostUserEvent( ((MenuBar*)pMenu)->GetCloserHdl(), pMenu );
    }
    else
    {
        std::map<sal_uInt16,AddButtonEntry>::iterator it = m_aAddButtons.find( aCloser.GetCurItemId() );
        if( it != m_aAddButtons.end() )
        {
            MenuBar::MenuBarButtonCallbackArg aArg;
            aArg.nId = it->first;
            aArg.bHighlight = (aCloser.GetHighlightItemId() == it->first);
            aArg.pMenuBar = dynamic_cast<MenuBar*>(pMenu);
            return it->second.m_aSelectLink.Call( &aArg );
        }
    }
    return 0;
}

IMPL_LINK( MenuBarWindow, ToolboxEventHdl, VclWindowEvent*, pEvent )
{
    if( ! pMenu )
        return 0;

    MenuBar::MenuBarButtonCallbackArg aArg;
    aArg.nId = 0xffff;
    aArg.bHighlight = (pEvent->GetId() == VCLEVENT_TOOLBOX_HIGHLIGHT);
    aArg.pMenuBar = dynamic_cast<MenuBar*>(pMenu);
    if( pEvent->GetId() == VCLEVENT_TOOLBOX_HIGHLIGHT )
        aArg.nId = aCloser.GetHighlightItemId();
    else if( pEvent->GetId() == VCLEVENT_TOOLBOX_HIGHLIGHTOFF )
    {
        sal_uInt16 nPos = static_cast< sal_uInt16 >(reinterpret_cast<sal_IntPtr>(pEvent->GetData()));
        aArg.nId = aCloser.GetItemId( nPos );
    }
    std::map< sal_uInt16, AddButtonEntry >::iterator it = m_aAddButtons.find( aArg.nId );
    if( it != m_aAddButtons.end() )
    {
        it->second.m_aHighlightLink.Call( &aArg );
    }
    return 0;
}

IMPL_LINK( MenuBarWindow, ShowHideListener, VclWindowEvent*, pEvent )
{
    if( ! pMenu )
        return 0;

    if( pEvent->GetId() == VCLEVENT_WINDOW_SHOW )
        pMenu->ImplCallEventListeners( VCLEVENT_MENU_SHOW, ITEMPOS_INVALID );
    else if( pEvent->GetId() == VCLEVENT_WINDOW_HIDE )
        pMenu->ImplCallEventListeners( VCLEVENT_MENU_HIDE, ITEMPOS_INVALID );
    return 0;
}

IMPL_LINK_NOARG(MenuBarWindow, FloatHdl)
{
    return pMenu ? ((MenuBar*)pMenu)->GetFloatButtonClickHdl().Call( pMenu ) : 0;
}

IMPL_LINK_NOARG(MenuBarWindow, HideHdl)
{
    return pMenu ? ((MenuBar*)pMenu)->GetHideButtonClickHdl().Call( pMenu ) : 0;
}

void MenuBarWindow::ImplCreatePopup( bool bPreSelectFirst )
{
    MenuItemData* pItemData = pMenu ? pMenu->GetItemList()->GetDataFromPos( nHighlightedItem ) : NULL;
    if ( pItemData )
    {
        bIgnoreFirstMove = true;
        if ( pActivePopup && ( pActivePopup != pItemData->pSubMenu ) )
        {
            KillActivePopup();
        }
        if ( pItemData->bEnabled && pItemData->pSubMenu && ( nHighlightedItem != ITEMPOS_INVALID ) && ( pItemData->pSubMenu != pActivePopup ) )
        {
            pActivePopup = (PopupMenu*)pItemData->pSubMenu;
            long nX = 0;
            MenuItemData* pData = 0;
            for ( sal_uLong n = 0; n < nHighlightedItem; n++ )
            {
                pData = pMenu->GetItemList()->GetDataFromPos( n );
                nX += pData->aSz.Width();
            }
            pData = pMenu->pItemList->GetDataFromPos( nHighlightedItem );
            Point aItemTopLeft( nX, 0 );
            Point aItemBottomRight( aItemTopLeft );
            aItemBottomRight.X() += pData->aSz.Width();

            // the menu bar could have height 0 in fullscreen mode:
            // so do not use always WindowHeight, as ItemHeight < WindowHeight.
            if ( GetSizePixel().Height() )
            {
                // #107747# give menuitems the height of the menubar
                aItemBottomRight.Y() += GetOutputSizePixel().Height()-1;
            }

            // ImplExecute is not modal...
            // #99071# do not grab the focus, otherwise it will be restored to the menubar
            // when the frame is reactivated later
            //GrabFocus();
            pActivePopup->ImplExecute( this, Rectangle( aItemTopLeft, aItemBottomRight ), FLOATWIN_POPUPMODE_DOWN | FLOATWIN_POPUPMODE_NOHORZPLACEMENT, pMenu, bPreSelectFirst );
            if ( pActivePopup )
            {
                // does not have a window, if aborted before or if there are no entries
                if ( pActivePopup->ImplGetFloatingWindow() )
                    pActivePopup->ImplGetFloatingWindow()->AddPopupModeWindow( this );
                else
                    pActivePopup = NULL;
            }
        }
    }
}

void MenuBarWindow::KillActivePopup()
{
    if ( pActivePopup )
    {
        if( pActivePopup->pWindow != NULL )
            if( ((FloatingWindow *) pActivePopup->pWindow)->IsInCleanUp() )
                return; // kill it later

        if ( pActivePopup->bInCallback )
            pActivePopup->bCanceled = true;

        pActivePopup->bInCallback = true;
        pActivePopup->Deactivate();
        pActivePopup->bInCallback = false;
        // check for pActivePopup, if stopped by deactivate...
        if ( pActivePopup->ImplGetWindow() )
        {
            pActivePopup->ImplGetFloatingWindow()->StopExecute();
            pActivePopup->ImplGetFloatingWindow()->doShutdown();
            pActivePopup->pWindow->doLazyDelete();
            pActivePopup->pWindow = NULL;
        }
        pActivePopup = 0;
    }
}

void MenuBarWindow::PopupClosed( Menu* pPopup )
{
    if ( pPopup == pActivePopup )
    {
        KillActivePopup();
        ChangeHighlightItem( ITEMPOS_INVALID, false, ImplGetFrameWindow()->ImplGetFrameData()->mbHasFocus, false );
    }
}

void MenuBarWindow::MouseButtonDown( const MouseEvent& rMEvt )
{
    mbAutoPopup = true;
    sal_uInt16 nEntry = ImplFindEntry( rMEvt.GetPosPixel() );
    if ( ( nEntry != ITEMPOS_INVALID ) && !pActivePopup )
    {
        ChangeHighlightItem( nEntry, false );
    }
    else
    {
        KillActivePopup();
        ChangeHighlightItem( ITEMPOS_INVALID, false );
    }
}

void MenuBarWindow::MouseButtonUp( const MouseEvent& )
{
}

void MenuBarWindow::MouseMove( const MouseEvent& rMEvt )
{
    if ( rMEvt.IsSynthetic() || rMEvt.IsEnterWindow() )
        return;

    if ( rMEvt.IsLeaveWindow() )
    {
        if ( nRolloveredItem != ITEMPOS_INVALID && nRolloveredItem != nHighlightedItem )
            HighlightItem( nRolloveredItem, false );

        nRolloveredItem = ITEMPOS_INVALID;
        return;
    }

    sal_uInt16 nEntry = ImplFindEntry( rMEvt.GetPosPixel() );
    if ( nHighlightedItem == ITEMPOS_INVALID )
    {
        if ( nRolloveredItem != nEntry  )
        {
            if ( nRolloveredItem != ITEMPOS_INVALID )
                HighlightItem( nRolloveredItem, false );

            nRolloveredItem = nEntry;
            HighlightItem( nRolloveredItem, true );
        }
        return;
    }
    nRolloveredItem = nEntry;

    if( bIgnoreFirstMove )
    {
        bIgnoreFirstMove = false;
        return;
    }

    if ( ( nEntry != ITEMPOS_INVALID )
       && ( nEntry != nHighlightedItem ) )
        ChangeHighlightItem( nEntry, false );
}

void MenuBarWindow::ChangeHighlightItem( sal_uInt16 n, bool bSelectEntry, bool bAllowRestoreFocus, bool bDefaultToDocument)
{
    if( ! pMenu )
        return;

    // #57934# close active popup if applicable, as TH's background storage works.
    MenuItemData* pNextData = pMenu->pItemList->GetDataFromPos( n );
    if ( pActivePopup && pActivePopup->ImplGetWindow() && ( !pNextData || ( pActivePopup != pNextData->pSubMenu ) ) )
        KillActivePopup(); // pActivePopup when applicable without pWin, if Rescheduled in  Activate()

    // activate menubar only ones per cycle...
    bool bJustActivated = false;
    if ( ( nHighlightedItem == ITEMPOS_INVALID ) && ( n != ITEMPOS_INVALID ) )
    {
        ImplGetSVData()->maWinData.mbNoDeactivate = true;
        if( !bStayActive )
        {
            // #105406# avoid saving the focus when we already have the focus
            bool bNoSaveFocus = (this == ImplGetSVData()->maWinData.mpFocusWin );

            if( nSaveFocusId )
            {
                if( !ImplGetSVData()->maWinData.mbNoSaveFocus )
                {
                    // we didn't clean up last time
                    Window::EndSaveFocus( nSaveFocusId, false );    // clean up
                    nSaveFocusId = 0;
                    if( !bNoSaveFocus )
                        nSaveFocusId = Window::SaveFocus(); // only save focus when initially activated
                }
                else {
                    ; // do nothing: we 're activated again from taskpanelist, focus was already saved
                }
            }
            else
            {
                if( !bNoSaveFocus )
                    nSaveFocusId = Window::SaveFocus(); // only save focus when initially activated
            }
        }
        else
            bStayActive = false;
        pMenu->bInCallback = true;  // set here if Activate overloaded
        pMenu->Activate();
        pMenu->bInCallback = false;
        bJustActivated = true;
    }
    else if ( ( nHighlightedItem != ITEMPOS_INVALID ) && ( n == ITEMPOS_INVALID ) )
    {
        pMenu->bInCallback = true;
        pMenu->Deactivate();
        pMenu->bInCallback = false;
        ImplGetSVData()->maWinData.mbNoDeactivate = false;
        if( !ImplGetSVData()->maWinData.mbNoSaveFocus )
        {
            sal_uLong nTempFocusId = nSaveFocusId;
            nSaveFocusId = 0;
            Window::EndSaveFocus( nTempFocusId, bAllowRestoreFocus );
            // #105406# restore focus to document if we could not save focus before
            if( bDefaultToDocument && !nTempFocusId && bAllowRestoreFocus )
                GrabFocusToDocument();
        }
    }

    if ( nHighlightedItem != ITEMPOS_INVALID )
    {
        if ( nHighlightedItem != nRolloveredItem )
            HighlightItem( nHighlightedItem, false );

        pMenu->ImplCallEventListeners( VCLEVENT_MENU_DEHIGHLIGHT, nHighlightedItem );
    }

    nHighlightedItem = (sal_uInt16)n;
    DBG_ASSERT( ( nHighlightedItem == ITEMPOS_INVALID ) || pMenu->ImplIsVisible( nHighlightedItem ), "ChangeHighlightItem: Not visible!" );
    if ( nHighlightedItem != ITEMPOS_INVALID )
        HighlightItem( nHighlightedItem, true );
    else if ( nRolloveredItem != ITEMPOS_INVALID )
        HighlightItem( nRolloveredItem, true );
    pMenu->SetHighlightItem(nHighlightedItem);
    pMenu->ImplCallHighlight(nHighlightedItem);

    if( mbAutoPopup )
        ImplCreatePopup( bSelectEntry );

    // #58935# #73659# Focus, if no popup underneath...
    if ( bJustActivated && !pActivePopup )
        GrabFocus();
}

void MenuBarWindow::HighlightItem( sal_uInt16 nPos, bool bHighlight )
{
    if( ! pMenu )
        return;

    long nX = 0;
    size_t nCount = pMenu->pItemList->size();
    for ( size_t n = 0; n < nCount; n++ )
    {
        MenuItemData* pData = pMenu->pItemList->GetDataFromPos( n );
        if ( n == nPos )
        {
            if ( pData->eType != MENUITEM_SEPARATOR )
            {
                // #107747# give menuitems the height of the menubar
                Rectangle aRect = Rectangle( Point( nX, 1 ), Size( pData->aSz.Width(), GetOutputSizePixel().Height()-2 ) );
                Push( PUSH_CLIPREGION );
                IntersectClipRegion( aRect );
                bool bRollover = bHighlight && nPos != nHighlightedItem;
                if ( bHighlight )
                {
                    if( IsNativeControlSupported( CTRL_MENUBAR, PART_MENU_ITEM ) &&
                        IsNativeControlSupported( CTRL_MENUBAR, PART_ENTIRE_CONTROL ) )
                    {
                        // draw background (transparency)
                        MenubarValue aControlValue;
                        aControlValue.maTopDockingAreaHeight = ImplGetTopDockingAreaHeight( this );

                        if ( !Application::GetSettings().GetStyleSettings().GetPersonaHeader().IsEmpty() )
                            Erase();
                        else
                        {
                            Point tmp(0,0);
                            Rectangle aBgRegion( tmp, GetOutputSizePixel() );
                            DrawNativeControl( CTRL_MENUBAR, PART_ENTIRE_CONTROL,
                                    aBgRegion,
                                    CTRL_STATE_ENABLED,
                                    aControlValue,
                                    OUString() );
                        }

                        ImplAddNWFSeparator( this, aControlValue );

                        // draw selected item
                        ControlState nState = CTRL_STATE_ENABLED;
                        if ( bRollover )
                            nState |= CTRL_STATE_ROLLOVER;
                        else
                            nState |= CTRL_STATE_SELECTED;
                        DrawNativeControl( CTRL_MENUBAR, PART_MENU_ITEM,
                                           aRect,
                                           nState,
                                           aControlValue,
                                           OUString() );
                    }
                    else
                    {
                        if ( bRollover )
                            SetFillColor( GetSettings().GetStyleSettings().GetMenuBarRolloverColor() );
                        else
                            SetFillColor( GetSettings().GetStyleSettings().GetMenuHighlightColor() );
                        SetLineColor();
                        DrawRect( aRect );
                    }
                }
                else
                {
                    if( IsNativeControlSupported( CTRL_MENUBAR, PART_ENTIRE_CONTROL) )
                    {
                        MenubarValue aMenubarValue;
                        aMenubarValue.maTopDockingAreaHeight = ImplGetTopDockingAreaHeight( this );

                        if ( !Application::GetSettings().GetStyleSettings().GetPersonaHeader().IsEmpty() )
                            Erase( aRect );
                        else
                        {
                            // use full window size to get proper gradient
                            // but clip accordingly
                            Point aPt;
                            Rectangle aCtrlRect( aPt, GetOutputSizePixel() );

                            DrawNativeControl( CTRL_MENUBAR, PART_ENTIRE_CONTROL, aCtrlRect, CTRL_STATE_ENABLED, aMenubarValue, OUString() );
                        }

                        ImplAddNWFSeparator( this, aMenubarValue );
                    }
                    else
                        Erase( aRect );
                }
                Pop();
                pMenu->ImplPaint( this, 0, 0, pData, bHighlight, false, bRollover );
            }
            return;
        }

        nX += pData->aSz.Width();
    }
}

Rectangle MenuBarWindow::ImplGetItemRect( sal_uInt16 nPos )
{
    Rectangle aRect;
    if( pMenu )
    {
        long nX = 0;
        size_t nCount = pMenu->pItemList->size();
        for ( size_t n = 0; n < nCount; n++ )
        {
            MenuItemData* pData = pMenu->pItemList->GetDataFromPos( n );
            if ( n == nPos )
            {
                if ( pData->eType != MENUITEM_SEPARATOR )
                    // #107747# give menuitems the height of the menubar
                    aRect = Rectangle( Point( nX, 1 ), Size( pData->aSz.Width(), GetOutputSizePixel().Height()-2 ) );
                break;
            }

            nX += pData->aSz.Width();
        }
    }
    return aRect;
}

void MenuBarWindow::KeyInput( const KeyEvent& rKEvent )
{
    if ( !ImplHandleKeyEvent( rKEvent ) )
        Window::KeyInput( rKEvent );
}

bool MenuBarWindow::ImplHandleKeyEvent( const KeyEvent& rKEvent, bool bFromMenu )
{
    if( ! pMenu )
        return false;

    if ( pMenu->bInCallback )
        return true;    // swallow

    bool bDone = false;
    sal_uInt16 nCode = rKEvent.GetKeyCode().GetCode();

    if( GetParent() )
    {
        if( GetParent()->GetWindow( WINDOW_CLIENT )->IsSystemWindow() )
        {
            SystemWindow *pSysWin = (SystemWindow*)GetParent()->GetWindow( WINDOW_CLIENT );
            if( pSysWin->GetTaskPaneList() )
                if( pSysWin->GetTaskPaneList()->HandleKeyEvent( rKEvent ) )
                    return true;
        }
    }

    if ( nCode == KEY_MENU && !rKEvent.GetKeyCode().IsShift() ) // only F10, not Shift-F10
    {
        mbAutoPopup = ImplGetSVData()->maNWFData.mbOpenMenuOnF10;
        if ( nHighlightedItem == ITEMPOS_INVALID )
        {
            ChangeHighlightItem( 0, false );
            GrabFocus();
        }
        else
        {
            ChangeHighlightItem( ITEMPOS_INVALID, false );
            nSaveFocusId = 0;
        }
        bDone = true;
    }
    else if ( bFromMenu )
    {
        if ( ( nCode == KEY_LEFT ) || ( nCode == KEY_RIGHT ) ||
            ( nCode == KEY_HOME ) || ( nCode == KEY_END ) )
        {
            sal_uInt16 n = nHighlightedItem;
            if ( n == ITEMPOS_INVALID )
            {
                if ( nCode == KEY_LEFT)
                    n = 0;
                else
                    n = pMenu->GetItemCount()-1;
            }

            // handling gtk like (aka mbOpenMenuOnF10)
            // do not highlight an item when opening a sub menu
            // unless there already was a higlighted sub menu item
            bool bWasHighlight = false;
            if( pActivePopup )
            {
                MenuFloatingWindow* pSubWindow = dynamic_cast<MenuFloatingWindow*>(pActivePopup->ImplGetWindow());
                if( pSubWindow )
                    bWasHighlight = (pSubWindow->GetHighlightedItem() != ITEMPOS_INVALID);
            }

            sal_uInt16 nLoop = n;

            if( nCode == KEY_HOME )
                { n = (sal_uInt16)-1; nLoop = n+1; }
            if( nCode == KEY_END )
                { n = pMenu->GetItemCount(); nLoop = n-1; }

            do
            {
                if ( nCode == KEY_LEFT || nCode == KEY_END )
                {
                    if ( n )
                        n--;
                    else
                        n = pMenu->GetItemCount()-1;
                }
                if ( nCode == KEY_RIGHT || nCode == KEY_HOME )
                {
                    n++;
                    if ( n >= pMenu->GetItemCount() )
                        n = 0;
                }

                MenuItemData* pData = (MenuItemData*)pMenu->GetItemList()->GetDataFromPos( n );
                if ( ( pData->eType != MENUITEM_SEPARATOR ) && pMenu->ImplIsVisible( n ) )
                {
                    bool bDoSelect = true;
                    if( ImplGetSVData()->maNWFData.mbOpenMenuOnF10 )
                        bDoSelect = bWasHighlight;
                    ChangeHighlightItem( n, bDoSelect );
                    break;
                }
            } while ( n != nLoop );
            bDone = true;
        }
        else if ( nCode == KEY_RETURN )
        {
            if( pActivePopup ) KillActivePopup();
            else
                if ( !mbAutoPopup )
                {
                    ImplCreatePopup( true );
                    mbAutoPopup = true;
                }
            bDone = true;
        }
        else if ( ( nCode == KEY_UP ) || ( nCode == KEY_DOWN ) )
        {
            if ( !mbAutoPopup )
            {
                ImplCreatePopup( true );
                mbAutoPopup = true;
            }
            bDone = true;
        }
        else if ( nCode == KEY_ESCAPE || ( nCode == KEY_F6 && rKEvent.GetKeyCode().IsMod1() ) )
        {
            if( pActivePopup )
            {
                // bring focus to menu bar without any open popup
                mbAutoPopup = false;
                sal_uInt16 n = nHighlightedItem;
                nHighlightedItem = ITEMPOS_INVALID;
                bStayActive = true;
                ChangeHighlightItem( n, false );
                bStayActive = false;
                KillActivePopup();
                GrabFocus();
            }
            else
                ChangeHighlightItem( ITEMPOS_INVALID, false );

            if( nCode == KEY_F6 && rKEvent.GetKeyCode().IsMod1() )
            {
                // put focus into document
                GrabFocusToDocument();
            }

            bDone = true;
        }
    }

    if ( !bDone && ( bFromMenu || rKEvent.GetKeyCode().IsMod2() ) )
    {
        sal_Unicode nCharCode = rKEvent.GetCharCode();
        if ( nCharCode )
        {
            sal_uInt16 nEntry, nDuplicates;
            MenuItemData* pData = pMenu->GetItemList()->SearchItem( nCharCode, rKEvent.GetKeyCode(), nEntry, nDuplicates, nHighlightedItem );
            if ( pData && (nEntry != ITEMPOS_INVALID) )
            {
                mbAutoPopup = true;
                ChangeHighlightItem( nEntry, true );
                bDone = true;
            }
        }
    }
    return bDone;
}

void MenuBarWindow::Paint( const Rectangle& )
{
    if( ! pMenu )
        return;

    // no VCL paint if native menus
    if( pMenu->ImplGetSalMenu() && pMenu->ImplGetSalMenu()->VisibleMenuBar() )
    {
        ImplGetFrame()->DrawMenuBar();
        return;
    }

    if( IsNativeControlSupported( CTRL_MENUBAR, PART_ENTIRE_CONTROL) )
    {
        MenubarValue aMenubarValue;
        aMenubarValue.maTopDockingAreaHeight = ImplGetTopDockingAreaHeight( this );

        if ( !Application::GetSettings().GetStyleSettings().GetPersonaHeader().IsEmpty() )
            Erase();
        else
        {
            Point aPt;
            Rectangle aCtrlRegion( aPt, GetOutputSizePixel() );

            DrawNativeControl( CTRL_MENUBAR, PART_ENTIRE_CONTROL, aCtrlRegion, CTRL_STATE_ENABLED, aMenubarValue, OUString() );
        }

        ImplAddNWFSeparator( this, aMenubarValue );
    }
    SetFillColor( GetSettings().GetStyleSettings().GetMenuColor() );
    pMenu->ImplPaint( this, 0 );
    if ( nHighlightedItem != ITEMPOS_INVALID )
        HighlightItem( nHighlightedItem, true );

    // in high contrast mode draw a separating line on the lower edge
    if( ! IsNativeControlSupported( CTRL_MENUBAR, PART_ENTIRE_CONTROL) &&
        GetSettings().GetStyleSettings().GetHighContrastMode() )
    {
        Push( PUSH_LINECOLOR | PUSH_MAPMODE );
        SetLineColor( Color( COL_WHITE ) );
        SetMapMode( MapMode( MAP_PIXEL ) );
        Size aSize = GetSizePixel();
        DrawLine( Point( 0, aSize.Height()-1 ), Point( aSize.Width()-1, aSize.Height()-1 ) );
        Pop();
    }

}

void MenuBarWindow::Resize()
{
    Size aOutSz = GetOutputSizePixel();
    long n      = aOutSz.Height()-4;
    long nX     = aOutSz.Width()-3;
    long nY     = 2;

    if ( aCloser.IsVisible() )
    {
        aCloser.Hide();
        aCloser.SetImages( n );
        Size aTbxSize( aCloser.CalcWindowSizePixel() );
        nX -= aTbxSize.Width();
        long nTbxY = (aOutSz.Height() - aTbxSize.Height())/2;
        aCloser.setPosSizePixel( nX, nTbxY, aTbxSize.Width(), aTbxSize.Height() );
        nX -= 3;
        aCloser.Show();
    }
    if ( aFloatBtn.IsVisible() )
    {
        nX -= n;
        aFloatBtn.setPosSizePixel( nX, nY, n, n );
    }
    if ( aHideBtn.IsVisible() )
    {
        nX -= n;
        aHideBtn.setPosSizePixel( nX, nY, n, n );
    }

    aFloatBtn.SetSymbol( SYMBOL_FLOAT );
    aHideBtn.SetSymbol( SYMBOL_HIDE );
    //aCloser.SetSymbol( SYMBOL_CLOSE ); //is a toolbox now

    Invalidate();
}

sal_uInt16 MenuBarWindow::ImplFindEntry( const Point& rMousePos ) const
{
    if( pMenu )
    {
        long nX = 0;
        size_t nCount = pMenu->pItemList->size();
        for ( size_t n = 0; n < nCount; n++ )
        {
            MenuItemData* pData = pMenu->pItemList->GetDataFromPos( n );
            if ( pMenu->ImplIsVisible( n ) )
            {
                nX += pData->aSz.Width();
                if ( nX > rMousePos.X() )
                    return (sal_uInt16)n;
            }
        }
    }
    return ITEMPOS_INVALID;
}

void MenuBarWindow::RequestHelp( const HelpEvent& rHEvt )
{
    sal_uInt16 nId = nHighlightedItem;
    if ( rHEvt.GetMode() & (HELPMODE_CONTEXT | HELPMODE_EXTENDED) )
        ChangeHighlightItem( ITEMPOS_INVALID, true );

    Rectangle aHighlightRect( ImplGetItemRect( nHighlightedItem ) );
    if( !ImplHandleHelpEvent( this, pMenu, nId, rHEvt, aHighlightRect ) )
        Window::RequestHelp( rHEvt );
}

void MenuBarWindow::StateChanged( StateChangedType nType )
{
    Window::StateChanged( nType );

    if ( ( nType == STATE_CHANGE_CONTROLFOREGROUND ) ||
         ( nType == STATE_CHANGE_CONTROLBACKGROUND ) )
    {
        ImplInitMenuWindow( this, false, true );
        Invalidate();
    }
    else if( pMenu )
        pMenu->ImplKillLayoutData();

}

void MenuBarWindow::ImplLayoutChanged()
{
    if( pMenu )
    {
        ImplInitMenuWindow( this, true, true );
        // if the font was changed.
        long nHeight = pMenu->ImplCalcSize( this ).Height();

        // depending on the native implementation or the displayable flag
        // the menubar windows is suppressed (ie, height=0)
        if( !((MenuBar*) pMenu)->IsDisplayable() ||
            ( pMenu->ImplGetSalMenu() && pMenu->ImplGetSalMenu()->VisibleMenuBar() ) )
            nHeight = 0;

        setPosSizePixel( 0, 0, 0, nHeight, WINDOW_POSSIZE_HEIGHT );
        GetParent()->Resize();
        Invalidate();
        Resize();
        if( pMenu )
            pMenu->ImplKillLayoutData();
    }
}

void MenuBarWindow::ImplInitStyleSettings()
{
    if( IsNativeControlSupported( CTRL_MENUBAR, PART_MENU_ITEM ) &&
        IsNativeControlSupported( CTRL_MENUBAR, PART_ENTIRE_CONTROL ) )
    {
        AllSettings aSettings( GetSettings() );
        ImplGetFrame()->UpdateSettings( aSettings ); // to update persona
        StyleSettings aStyle( aSettings.GetStyleSettings() );
        Color aHighlightTextColor = ImplGetSVData()->maNWFData.maMenuBarHighlightTextColor;
        if( aHighlightTextColor != Color( COL_TRANSPARENT ) )
        {
            aStyle.SetMenuHighlightTextColor( aHighlightTextColor );
        }
        aSettings.SetStyleSettings( aStyle );
        OutputDevice::SetSettings( aSettings );
    }
}

void MenuBarWindow::DataChanged( const DataChangedEvent& rDCEvt )
{
    Window::DataChanged( rDCEvt );

    if ( (rDCEvt.GetType() == DATACHANGED_FONTS) ||
         (rDCEvt.GetType() == DATACHANGED_FONTSUBSTITUTION) ||
         ((rDCEvt.GetType() == DATACHANGED_SETTINGS) &&
          (rDCEvt.GetFlags() & SETTINGS_STYLE)) )
    {
        ImplInitStyleSettings();
        ImplLayoutChanged();
    }
}

void MenuBarWindow::LoseFocus()
{
    if ( !HasChildPathFocus( true ) )
        ChangeHighlightItem( ITEMPOS_INVALID, false, false );
}

void MenuBarWindow::GetFocus()
{
    if ( nHighlightedItem == ITEMPOS_INVALID )
    {
        mbAutoPopup = false;    // do not open menu when activated by focus handling like taskpane cycling
        ChangeHighlightItem( 0, false );
    }
}

::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > MenuBarWindow::CreateAccessible()
{
    ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > xAcc;

    if ( pMenu )
        xAcc = pMenu->GetAccessible();

    return xAcc;
}

sal_uInt16 MenuBarWindow::AddMenuBarButton( const Image& i_rImage, const Link& i_rLink, const OUString& i_rToolTip, sal_uInt16 i_nPos )
{
    // find first free button id
    sal_uInt16 nId = IID_DOCUMENTCLOSE;
    std::map< sal_uInt16, AddButtonEntry >::const_iterator it;
    if( i_nPos > m_aAddButtons.size() )
        i_nPos = static_cast<sal_uInt16>(m_aAddButtons.size());
    do
    {
        nId++;
        it = m_aAddButtons.find( nId );
    } while( it != m_aAddButtons.end() && nId < 128 );
    DBG_ASSERT( nId < 128, "too many addbuttons in menubar" );
    AddButtonEntry& rNewEntry = m_aAddButtons[nId];
    rNewEntry.m_nId = nId;
    rNewEntry.m_aSelectLink = i_rLink;
    aCloser.InsertItem( nId, i_rImage, 0, 0 );
    aCloser.calcMinSize();
    ShowButtons( aCloser.IsItemVisible( IID_DOCUMENTCLOSE ),
                 aFloatBtn.IsVisible(),
                 aHideBtn.IsVisible() );
    ImplLayoutChanged();

    if( pMenu->mpSalMenu )
        pMenu->mpSalMenu->AddMenuBarButton( SalMenuButtonItem( nId, i_rImage, i_rToolTip ) );

    return nId;
}

void MenuBarWindow::SetMenuBarButtonHighlightHdl( sal_uInt16 nId, const Link& rLink )
{
    std::map< sal_uInt16, AddButtonEntry >::iterator it = m_aAddButtons.find( nId );
    if( it != m_aAddButtons.end() )
        it->second.m_aHighlightLink = rLink;
}

Rectangle MenuBarWindow::GetMenuBarButtonRectPixel( sal_uInt16 nId )
{
    Rectangle aRect;
    if( m_aAddButtons.find( nId ) != m_aAddButtons.end() )
    {
        if( pMenu->mpSalMenu )
        {
            aRect = pMenu->mpSalMenu->GetMenuBarButtonRectPixel( nId, ImplGetWindowImpl()->mpFrame );
            if( aRect == Rectangle( Point( -1, -1 ), Size( 1, 1 ) ) )
            {
                // system menu button is somewhere but location cannot be determined
                return Rectangle();
            }
        }

        if( aRect.IsEmpty() )
        {
            aRect = aCloser.GetItemRect( nId );
            Point aOffset = aCloser.OutputToScreenPixel( Point() );
            aRect.Move( aOffset.X(), aOffset.Y() );
        }
    }
    return aRect;
}

void MenuBarWindow::RemoveMenuBarButton( sal_uInt16 nId )
{
    sal_uInt16 nPos = aCloser.GetItemPos( nId );
    aCloser.RemoveItem( nPos );
    m_aAddButtons.erase( nId );
    aCloser.calcMinSize();
    ImplLayoutChanged();

    if( pMenu->mpSalMenu )
        pMenu->mpSalMenu->RemoveMenuBarButton( nId );
}

bool MenuBarWindow::HandleMenuButtonEvent( sal_uInt16 i_nButtonId )
{
    std::map< sal_uInt16, AddButtonEntry >::iterator it = m_aAddButtons.find( i_nButtonId );
    if( it != m_aAddButtons.end() )
    {
        MenuBar::MenuBarButtonCallbackArg aArg;
        aArg.nId = it->first;
        aArg.bHighlight = true;
        aArg.pMenuBar = dynamic_cast<MenuBar*>(pMenu);
        return it->second.m_aSelectLink.Call( &aArg );
    }
    return false;
}

ImplMenuDelData::ImplMenuDelData( const Menu* pMenu )
: mpNext( 0 )
, mpMenu( 0 )
{
    if( pMenu )
        const_cast< Menu* >( pMenu )->ImplAddDel( *this );
}

ImplMenuDelData::~ImplMenuDelData()
{
    if( mpMenu )
        const_cast< Menu* >( mpMenu )->ImplRemoveDel( *this );
}

namespace vcl
{
    MenuInvalidator::MenuInvalidator() {};

    static VclEventListeners2* pMenuInvalidateListeners = NULL;
    VclEventListeners2* MenuInvalidator::GetMenuInvalidateListeners()
    {
        if(!pMenuInvalidateListeners)
            pMenuInvalidateListeners = new VclEventListeners2();
        return pMenuInvalidateListeners;
    }
    void MenuInvalidator::Invalidated()
    {
        VclSimpleEvent aEvent(0);
        GetMenuInvalidateListeners()->callListeners(&aEvent);
    };
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
