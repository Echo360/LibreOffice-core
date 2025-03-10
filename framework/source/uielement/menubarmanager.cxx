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

#include <uielement/menubarmanager.hxx>
#include <framework/menuconfiguration.hxx>
#include <framework/bmkmenu.hxx>
#include <framework/addonmenu.hxx>
#include <framework/imageproducer.hxx>
#include <framework/addonsoptions.hxx>
#include <classes/fwkresid.hxx>
#include <classes/menumanager.hxx>
#include <helper/mischelper.hxx>
#include <framework/menuextensionsupplier.hxx>
#include <classes/resource.hrc>
#include <services.h>

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/frame/XDispatch.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/frame/XFramesSupplier.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/container/XEnumeration.hpp>
#include <com/sun/star/util/XStringWidth.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/uno/XCurrentContext.hpp>
#include <com/sun/star/lang/XMultiComponentFactory.hpp>
#include <com/sun/star/frame/XPopupMenuController.hpp>
#include <com/sun/star/frame/thePopupMenuControllerFactory.hpp>
#include <com/sun/star/lang/SystemDependent.hpp>
#include <com/sun/star/ui/GlobalAcceleratorConfiguration.hpp>
#include <com/sun/star/ui/ItemType.hpp>
#include <com/sun/star/ui/ImageType.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/frame/ModuleManager.hpp>
#include <com/sun/star/ui/theModuleUIConfigurationManagerSupplier.hpp>
#include <com/sun/star/ui/XUIConfigurationManagerSupplier.hpp>
#include <com/sun/star/ui/ItemStyle.hpp>
#include <com/sun/star/frame/status/Visibility.hpp>
#include <com/sun/star/util/URLTransformer.hpp>

#include <comphelper/processfactory.hxx>
#include <comphelper/extract.hxx>
#include <svtools/menuoptions.hxx>
#include <svtools/javainteractionhandler.hxx>
#include <uno/current_context.hxx>
#include <unotools/historyoptions.hxx>
#include <unotools/pathoptions.hxx>
#include <unotools/cmdoptions.hxx>
#include <unotools/localfilehelper.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <vcl/svapp.hxx>
#include <vcl/window.hxx>
#include <vcl/menu.hxx>
#include <vcl/settings.hxx>
#include <osl/mutex.hxx>
#include <osl/file.hxx>
#include <cppuhelper/implbase1.hxx>
#include <svtools/acceleratorexecute.hxx>
#include <svtools/miscopt.hxx>
#include <uielement/menubarmerger.hxx>

// Be careful removing this "bad" construct. There are serious problems
// with #define STRICT and including windows.h. Changing this needs some
// redesign on other projects, too. Especially sal/main.h which defines
// HINSTANCE depending on STRICT!!!!!!!!!!!!!!!
struct SystemMenuData
{
    unsigned long nSize;
    long          hMenu;
};

using namespace ::cppu;
using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::util;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::ui;

static const char ITEM_DESCRIPTOR_COMMANDURL[]        = "CommandURL";
static const char ITEM_DESCRIPTOR_HELPURL[]           = "HelpURL";
static const char ITEM_DESCRIPTOR_CONTAINER[]         = "ItemDescriptorContainer";
static const char ITEM_DESCRIPTOR_LABEL[]             = "Label";
static const char ITEM_DESCRIPTOR_TYPE[]              = "Type";
static const char ITEM_DESCRIPTOR_MODULEIDENTIFIER[]  = "ModuleIdentifier";
static const char ITEM_DESCRIPTOR_DISPATCHPROVIDER[]  = "DispatchProvider";
static const char ITEM_DESCRIPTOR_STYLE[]             = "Style";
static const char ITEM_DESCRIPTOR_ISVISIBLE[]         = "IsVisible";
static const char ITEM_DESCRIPTOR_ENABLED[]           = "Enabled";

static const sal_Int32 LEN_DESCRIPTOR_COMMANDURL       = 10;
static const sal_Int32 LEN_DESCRIPTOR_HELPURL          = 7;
static const sal_Int32 LEN_DESCRIPTOR_CONTAINER        = 23;
static const sal_Int32 LEN_DESCRIPTOR_LABEL            = 5;
static const sal_Int32 LEN_DESCRIPTOR_TYPE             = 4;
static const sal_Int32 LEN_DESCRIPTOR_MODULEIDENTIFIER = 16;
static const sal_Int32 LEN_DESCRIPTOR_DISPATCHPROVIDER = 16;
static const sal_Int32 LEN_DESCRIPTOR_STYLE            = 5;
static const sal_Int32 LEN_DESCRIPTOR_ISVISIBLE        = 9;
static const sal_Int32 LEN_DESCRIPTOR_ENABLED          = 7;

const sal_uInt16 ADDONMENU_MERGE_ITEMID_START = 1500;

namespace framework
{

// special menu ids/command ids for dynamic popup menus
#define SID_SFX_START           5000
#define SID_MDIWINDOWLIST       (SID_SFX_START + 610)
#define SID_ADDONLIST           (SID_SFX_START + 1677)
#define SID_HELPMENU            (SID_SFX_START + 410)

#define aCmdHelpIndex ".uno:HelpIndex"
#define aCmdToolsMenu ".uno:ToolsMenu"
#define aCmdHelpMenu ".uno:HelpMenu"
#define aSlotHelpMenu "slot:5410"

#define aSpecialWindowMenu "window"
#define aSlotSpecialWindowMenu "slot:5610"
#define aSlotSpecialToolsMenu "slot:6677"

// special uno commands for window list
#define aSpecialWindowCommand ".uno:WindowList"

static sal_Int16 getImageTypeFromBools( bool bBig )
{
    sal_Int16 n( 0 );
    if ( bBig )
        n |= ::com::sun::star::ui::ImageType::SIZE_LARGE;
    return n;
}

MenuBarManager::MenuBarManager(
    const Reference< XComponentContext >& rxContext,
    const Reference< XFrame >& rFrame,
    const Reference< XURLTransformer >& _xURLTransformer,
    const Reference< XDispatchProvider >& rDispatchProvider,
    const OUString& rModuleIdentifier,
    Menu* pMenu, bool bDelete, bool bDeleteChildren ):
    OWeakObject()
    , m_bDisposed( false )
    , m_bRetrieveImages( false )
    , m_bAcceleratorCfg( false )
    , m_bModuleIdentified( false )
    , m_aListenerContainer( m_mutex )
    , m_xContext(rxContext)
    , m_xURLTransformer(_xURLTransformer)
    , m_sIconTheme( SvtMiscOptions().GetIconTheme() )
{
    m_xPopupMenuControllerFactory = frame::thePopupMenuControllerFactory::get(m_xContext);
    FillMenuManager( pMenu, rFrame, rDispatchProvider, rModuleIdentifier, bDelete, bDeleteChildren );
}

MenuBarManager::MenuBarManager(
    const Reference< XComponentContext >& rxContext,
    const Reference< XFrame >& rFrame,
    const Reference< XURLTransformer >& _xURLTransformer,
    AddonMenu* pAddonMenu,
    bool bDelete,
    bool bDeleteChildren ):
    OWeakObject()
    , m_bDisposed( false )
    , m_bRetrieveImages( true )
    , m_bAcceleratorCfg( false )
    , m_bModuleIdentified( false )
    , m_aListenerContainer( m_mutex )
    , m_xContext(rxContext)
    , m_xURLTransformer(_xURLTransformer)
    , m_sIconTheme( SvtMiscOptions().GetIconTheme() )
{
    Init(rFrame,pAddonMenu,bDelete,bDeleteChildren);
}

MenuBarManager::MenuBarManager(
    const Reference< XComponentContext >& rxContext,
    const Reference< XFrame >& rFrame,
    const Reference< XURLTransformer >& _xURLTransformer,
    AddonPopupMenu* pAddonPopupMenu,
    bool bDelete,
    bool bDeleteChildren ):
    OWeakObject()
    , m_bDisposed( false )
    , m_bRetrieveImages( true )
    , m_bAcceleratorCfg( false )
    , m_bModuleIdentified( false )
    , m_aListenerContainer( m_mutex )
    , m_xContext(rxContext)
    , m_xURLTransformer(_xURLTransformer)
    , m_sIconTheme( SvtMiscOptions().GetIconTheme() )
{
    Init(rFrame,pAddonPopupMenu,bDelete,bDeleteChildren,true);
}

Any SAL_CALL MenuBarManager::queryInterface( const Type & rType ) throw ( RuntimeException, std::exception )
{
    Any a = ::cppu::queryInterface(
                rType ,
                (static_cast< ::com::sun::star::frame::XStatusListener* >(this)),
                (static_cast< ::com::sun::star::frame::XFrameActionListener* >(this)),
                (static_cast< ::com::sun::star::ui::XUIConfigurationListener* >(this)),
                (static_cast< XEventListener* >((XStatusListener *)this)),
                (static_cast< XComponent* >(this)),
                (static_cast< ::com::sun::star::awt::XSystemDependentMenuPeer* >(this)));

    if ( a.hasValue() )
        return a;

    return OWeakObject::queryInterface( rType );
}

void SAL_CALL MenuBarManager::acquire() throw()
{
    OWeakObject::acquire();
}

void SAL_CALL MenuBarManager::release() throw()
{
    OWeakObject::release();
}

Any SAL_CALL MenuBarManager::getMenuHandle( const Sequence< sal_Int8 >& /*ProcessId*/, sal_Int16 SystemType ) throw (RuntimeException, std::exception)
{
    SolarMutexGuard aSolarGuard;

    if ( m_bDisposed )
        throw com::sun::star::lang::DisposedException();

    Any a;

    if ( m_pVCLMenu )
    {
        SystemMenuData aSystemMenuData;
        aSystemMenuData.nSize = sizeof( SystemMenuData );

        m_pVCLMenu->GetSystemMenuData( &aSystemMenuData );
#ifdef _WIN32
        if( SystemType == SystemDependent::SYSTEM_WIN32 )
        {
            a <<= (long) aSystemMenuData.hMenu;
        }
#else
        (void) SystemType;
#endif
    }

    return a;
}

MenuBarManager::~MenuBarManager()
{
    // stop asynchronous settings timer
    m_xDeferedItemContainer.clear();
    m_aAsyncSettingsTimer.Stop();

    DBG_ASSERT( OWeakObject::m_refCount == 0, "Who wants to delete an object with refcount > 0!" );
}

void MenuBarManager::Destroy()
{
    SolarMutexGuard aGuard;

    if ( !m_bDisposed )
    {
        // stop asynchronous settings timer and
        // release defered item container reference
        m_aAsyncSettingsTimer.Stop();
        m_xDeferedItemContainer.clear();
        RemoveListener();

        std::vector< MenuItemHandler* >::iterator p;
        for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
        {
            MenuItemHandler* pItemHandler = *p;
            pItemHandler->xMenuItemDispatch.clear();
            pItemHandler->xSubMenuManager.clear();
            pItemHandler->xPopupMenu.clear();
            delete pItemHandler;
        }
        m_aMenuItemHandlerVector.clear();

        if ( m_bDeleteMenu )
        {
            delete m_pVCLMenu;
            m_pVCLMenu = 0;
        }
    }
}

// XComponent
void SAL_CALL MenuBarManager::dispose() throw( RuntimeException, std::exception )
{
    Reference< XComponent > xThis( static_cast< OWeakObject* >(this), UNO_QUERY );

    EventObject aEvent( xThis );
    m_aListenerContainer.disposeAndClear( aEvent );

    {
        SolarMutexGuard g;
        Destroy();
        m_bDisposed = true;

        if ( m_xDocImageManager.is() )
        {
            try
            {
                m_xDocImageManager->removeConfigurationListener(
                    Reference< XUIConfigurationListener >(
                        static_cast< OWeakObject* >( this ), UNO_QUERY ));
            }
            catch ( const Exception& )
            {
            }
        }
        if ( m_xModuleImageManager.is() )
        {
            try
            {
                m_xModuleImageManager->removeConfigurationListener(
                    Reference< XUIConfigurationListener >(
                        static_cast< OWeakObject* >( this ), UNO_QUERY ));
            }
            catch ( const Exception& )
            {
            }
        }
        m_xDocImageManager.clear();
        m_xModuleImageManager.clear();
        Reference< XComponent > xCompGAM( m_xGlobalAcceleratorManager, UNO_QUERY );
        if ( xCompGAM.is() )
            xCompGAM->dispose();
        m_xGlobalAcceleratorManager.clear();
        m_xModuleAcceleratorManager.clear();
        m_xDocAcceleratorManager.clear();
        m_xUICommandLabels.clear();
        m_xPopupMenuControllerFactory.clear();
        m_xContext.clear();
    }
}

void SAL_CALL MenuBarManager::addEventListener( const Reference< XEventListener >& xListener ) throw( RuntimeException, std::exception )
{
    SolarMutexGuard g;

    /* SAFE AREA ----------------------------------------------------------------------------------------------- */
    if ( m_bDisposed )
        throw DisposedException();

    m_aListenerContainer.addInterface( cppu::UnoType<XEventListener>::get(), xListener );
}

void SAL_CALL MenuBarManager::removeEventListener( const Reference< XEventListener >& xListener ) throw( RuntimeException, std::exception )
{
    SolarMutexGuard g;
    /* SAFE AREA ----------------------------------------------------------------------------------------------- */
    m_aListenerContainer.removeInterface( cppu::UnoType<XEventListener>::get(), xListener );
}

void SAL_CALL MenuBarManager::elementInserted( const ::com::sun::star::ui::ConfigurationEvent& Event )
throw (RuntimeException, std::exception)
{
    SolarMutexGuard g;

    /* SAFE AREA ----------------------------------------------------------------------------------------------- */
    if ( m_bDisposed )
        return;

    sal_Int16 nImageType = sal_Int16();
    sal_Int16 nCurrentImageType = getImageTypeFromBools( false );
    if (( Event.aInfo >>= nImageType ) &&
        ( nImageType == nCurrentImageType ))
        RequestImages();
}

void SAL_CALL MenuBarManager::elementRemoved( const ::com::sun::star::ui::ConfigurationEvent& Event )
throw (RuntimeException, std::exception)
{
    elementInserted(Event);
}

void SAL_CALL MenuBarManager::elementReplaced( const ::com::sun::star::ui::ConfigurationEvent& Event )
throw (RuntimeException, std::exception)
{
    elementInserted(Event);
}

// XFrameActionListener
void SAL_CALL MenuBarManager::frameAction( const FrameActionEvent& Action )
throw ( RuntimeException, std::exception )
{
    SolarMutexGuard g;

    if ( m_bDisposed )
        throw com::sun::star::lang::DisposedException();

    if ( Action.Action == FrameAction_CONTEXT_CHANGED )
    {
        std::vector< MenuItemHandler* >::iterator p;
        for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
        {
            // Clear dispatch reference as we will requery it later o
            MenuItemHandler* pItemHandler = *p;
            pItemHandler->xMenuItemDispatch.clear();
        }
    }
}

// XStatusListener
void SAL_CALL MenuBarManager::statusChanged( const FeatureStateEvent& Event )
throw ( RuntimeException, std::exception )
{
    OUString aFeatureURL = Event.FeatureURL.Complete;

    SolarMutexGuard aSolarGuard;
    {
        vcl::MenuInvalidator aInvalidator;
        aInvalidator.Invalidated();
    }
    {
        if ( m_bDisposed )
            return;

        // We have to check all menu entries as there can be identical entries in a popup menu.
        std::vector< MenuItemHandler* >::iterator p;
        for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
        {
            MenuItemHandler* pMenuItemHandler = *p;
            if ( pMenuItemHandler->aMenuItemURL == aFeatureURL )
            {
                bool            bCheckmark( false );
                bool            bMenuItemEnabled( m_pVCLMenu->IsItemEnabled( pMenuItemHandler->nItemId ));
                bool            bEnabledItem( Event.IsEnabled );
                OUString       aItemText;
                status::Visibility  aVisibilityStatus;

                #ifdef UNIX
                //enable some slots hardly, because UNIX clipboard does not notify all changes
                // Can be removed if follow up task will be fixed directly within applications.
                // Note: PasteSpecial is handled specifically by calc
                if ( pMenuItemHandler->aMenuItemURL == ".uno:Paste"
                    || pMenuItemHandler->aMenuItemURL == ".uno:PasteClipboard" )      // special for draw/impress
                    bEnabledItem = true;
                #endif

                // Enable/disable item
                if ( bEnabledItem != bMenuItemEnabled )
                    m_pVCLMenu->EnableItem( pMenuItemHandler->nItemId, bEnabledItem );

                if ( Event.State >>= bCheckmark )
                {
                    // Checkmark or RadioButton
                    m_pVCLMenu->ShowItem( pMenuItemHandler->nItemId, true );
                    m_pVCLMenu->CheckItem( pMenuItemHandler->nItemId, bCheckmark );

                    MenuItemBits nBits = m_pVCLMenu->GetItemBits( pMenuItemHandler->nItemId );
                    //If not already designated RadioButton set as CheckMark
                    if (!(nBits & MIB_RADIOCHECK))
                        m_pVCLMenu->SetItemBits( pMenuItemHandler->nItemId, nBits | MIB_CHECKABLE );
                }
                else if ( Event.State >>= aItemText )
                {
                    // Replacement for place holders
                    if ( aItemText.matchAsciiL( "($1)", 4 ))
                    {
                        OUString aTmp(FWK_RESSTR(STR_UPDATEDOC));
                        aTmp += " ";
                        aTmp += aItemText.copy( 4 );
                        aItemText = aTmp;
                    }
                    else if ( aItemText.matchAsciiL( "($2)", 4 ))
                    {
                        OUString aTmp(FWK_RESSTR(STR_CLOSEDOC_ANDRETURN));
                        aTmp += aItemText.copy( 4 );
                        aItemText = aTmp;
                    }
                    else if ( aItemText.matchAsciiL( "($3)", 4 ))
                    {
                        OUString aTmp(FWK_RESSTR(STR_SAVECOPYDOC));
                        aTmp += aItemText.copy( 4 );
                        aItemText = aTmp;
                    }

                    m_pVCLMenu->ShowItem( pMenuItemHandler->nItemId, true );
                    m_pVCLMenu->SetItemText( pMenuItemHandler->nItemId, aItemText );
                }
                else if ( Event.State >>= aVisibilityStatus )
                {
                    // Visibility
                    m_pVCLMenu->ShowItem( pMenuItemHandler->nItemId, aVisibilityStatus.bVisible );
                }
                else
                    m_pVCLMenu->ShowItem( pMenuItemHandler->nItemId, true );
            }

            if ( Event.Requery )
            {
                // Release dispatch object - will be requeried on the next activate!
                pMenuItemHandler->xMenuItemDispatch.clear();
            }
        }
    }
}

// Helper to retrieve own structure from item ID
MenuBarManager::MenuItemHandler* MenuBarManager::GetMenuItemHandler( sal_uInt16 nItemId )
{
    SolarMutexGuard g;

    std::vector< MenuItemHandler* >::iterator p;
    for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
    {
        MenuItemHandler* pItemHandler = *p;
        if ( pItemHandler->nItemId == nItemId )
            return pItemHandler;
    }

    return 0;
}

// Helper to set request images flag
void MenuBarManager::RequestImages()
{

    m_bRetrieveImages = true;
    const sal_uInt32 nCount = m_aMenuItemHandlerVector.size();
    for ( sal_uInt32 i = 0; i < nCount; ++i )
    {
        MenuItemHandler* pItemHandler = m_aMenuItemHandlerVector[i];
        if ( pItemHandler->xSubMenuManager.is() )
        {
            MenuBarManager* pMenuBarManager = (MenuBarManager*)(pItemHandler->xSubMenuManager.get());
            pMenuBarManager->RequestImages();
        }
    }
}

// Helper to reset objects to prepare shutdown
void MenuBarManager::RemoveListener()
{
    SolarMutexGuard g;

    // Check service manager reference. Remove listener can be called due
    // to a disposing call from the frame and therefore we already removed
    // our listeners and release the service manager reference!
    if ( m_xContext.is() )
    {
        std::vector< MenuItemHandler* >::iterator p;
        for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
        {
            MenuItemHandler* pItemHandler = *p;
            if ( pItemHandler->xMenuItemDispatch.is() )
            {
                URL aTargetURL;
                aTargetURL.Complete = pItemHandler->aMenuItemURL;
                m_xURLTransformer->parseStrict( aTargetURL );

                pItemHandler->xMenuItemDispatch->removeStatusListener(
                    static_cast< XStatusListener* >( this ), aTargetURL );
            }

            pItemHandler->xMenuItemDispatch.clear();
            if ( pItemHandler->xPopupMenu.is() )
            {
                {
                    // Remove popup menu from menu structure
                    m_pVCLMenu->SetPopupMenu( pItemHandler->nItemId, 0 );
                }

                Reference< com::sun::star::lang::XEventListener > xEventListener( pItemHandler->xPopupMenuController, UNO_QUERY );
                if ( xEventListener.is() )
                {
                    EventObject aEventObject;
                    aEventObject.Source = (OWeakObject *)this;
                    xEventListener->disposing( aEventObject );
                }

                // We now provide a popup menu controller to external code.
                // Therefore the life-time must be explicitly handled via
                // dispose!!
                try
                {
                    Reference< XComponent > xComponent( pItemHandler->xPopupMenuController, UNO_QUERY );
                    if ( xComponent.is() )
                        xComponent->dispose();
                }
                catch ( const RuntimeException& )
                {
                    throw;
                }
                catch ( const Exception& )
                {
                }

                // Release references to controller and popup menu
                pItemHandler->xPopupMenuController.clear();
                pItemHandler->xPopupMenu.clear();
            }

            Reference< XComponent > xComponent( pItemHandler->xSubMenuManager, UNO_QUERY );
            if ( xComponent.is() )
                xComponent->dispose();
        }
    }

    try
    {
        if ( m_xFrame.is() )
            m_xFrame->removeFrameActionListener( Reference< XFrameActionListener >(
                                                    static_cast< OWeakObject* >( this ), UNO_QUERY ));
    }
    catch ( const Exception& )
    {
    }

    m_xFrame = 0;
}

void SAL_CALL MenuBarManager::disposing( const EventObject& Source ) throw ( RuntimeException, std::exception )
{
    MenuItemHandler* pMenuItemDisposing = NULL;

    SolarMutexGuard g;

    std::vector< MenuItemHandler* >::iterator p;
    for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
    {
        MenuItemHandler* pMenuItemHandler = *p;
        if ( pMenuItemHandler->xMenuItemDispatch.is() &&
             pMenuItemHandler->xMenuItemDispatch == Source.Source )
        {
            // disposing called from menu item dispatcher, remove listener
            pMenuItemDisposing = pMenuItemHandler;
            break;
        }
    }

    if ( pMenuItemDisposing )
    {
        // Release references to the dispatch object
        URL aTargetURL;
        aTargetURL.Complete = pMenuItemDisposing->aMenuItemURL;

        // Check reference of service manager before we use it. Reference could
        // be cleared due to RemoveListener call!
        if ( m_xContext.is() )
        {
            m_xURLTransformer->parseStrict( aTargetURL );

            pMenuItemDisposing->xMenuItemDispatch->removeStatusListener(
                static_cast< XStatusListener* >( this ), aTargetURL );
            pMenuItemDisposing->xMenuItemDispatch.clear();
            if ( pMenuItemDisposing->xPopupMenu.is() )
            {
                Reference< com::sun::star::lang::XEventListener > xEventListener( pMenuItemDisposing->xPopupMenuController, UNO_QUERY );
                if ( xEventListener.is() )
                    xEventListener->disposing( Source );

                {
                    // Remove popup menu from menu structure as we release our reference to
                    // the controller.
                    m_pVCLMenu->SetPopupMenu( pMenuItemDisposing->nItemId, 0 );
                }

                pMenuItemDisposing->xPopupMenuController.clear();
                pMenuItemDisposing->xPopupMenu.clear();
            }
        }
        return;
    }
    else if ( Source.Source == m_xFrame )
    {
        // Our frame gets disposed. We have to remove all our listeners
        RemoveListener();
    }
    else if ( Source.Source == Reference< XInterface >( m_xDocImageManager, UNO_QUERY ))
        m_xDocImageManager.clear();
    else if ( Source.Source == Reference< XInterface >( m_xModuleImageManager, UNO_QUERY ))
        m_xModuleImageManager.clear();
}

void MenuBarManager::CheckAndAddMenuExtension( Menu* pMenu )
{

    // retrieve menu extension item
    MenuExtensionItem aMenuItem( GetMenuExtension() );
    if (( !aMenuItem.aURL.isEmpty() ) &&
        ( !aMenuItem.aLabel.isEmpty() ))
    {
        // remove all old window list entries from menu
        sal_uInt16 nNewItemId( 0 );
        sal_uInt16 nInsertPos( MENU_APPEND );
        sal_uInt16 nBeforePos( MENU_APPEND );
        OUString aCommandBefore( ".uno:About" );
        for ( sal_uInt16 n = 0; n < pMenu->GetItemCount(); n++ )
        {
            sal_uInt16 nItemId = pMenu->GetItemId( n );
            nNewItemId = std::max( nItemId, nNewItemId );
            if ( pMenu->GetItemCommand( nItemId ) == aCommandBefore )
                nBeforePos = n;
        }
        ++nNewItemId;

        if ( nBeforePos != MENU_APPEND )
            nInsertPos = nBeforePos;

        pMenu->InsertItem(nNewItemId, aMenuItem.aLabel, 0, OString(), nInsertPos);
        pMenu->SetItemCommand( nNewItemId, aMenuItem.aURL );
    }
}

static void lcl_CheckForChildren(Menu* pMenu, sal_uInt16 nItemId)
{
    if (PopupMenu* pThisPopup = pMenu->GetPopupMenu( nItemId ))
        pMenu->EnableItem( nItemId, pThisPopup->GetItemCount() ? true : false );
}

// vcl handler

namespace {

class QuietInteractionContext:
    public cppu::WeakImplHelper1< com::sun::star::uno::XCurrentContext >,
    private boost::noncopyable
{
public:
    QuietInteractionContext(
        com::sun::star::uno::Reference< com::sun::star::uno::XCurrentContext >
            const & context):
        context_(context) {}

private:
    virtual ~QuietInteractionContext() {}

    virtual com::sun::star::uno::Any SAL_CALL getValueByName(
        OUString const & Name)
        throw (com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE
    {
        return Name != JAVA_INTERACTION_HANDLER_NAME && context_.is()
            ? context_->getValueByName(Name)
            : com::sun::star::uno::Any();
    }

    com::sun::star::uno::Reference< com::sun::star::uno::XCurrentContext >
        context_;
};

}

IMPL_LINK( MenuBarManager, Activate, Menu *, pMenu )
{
    if ( pMenu == m_pVCLMenu )
    {
        com::sun::star::uno::ContextLayer layer(
            new QuietInteractionContext(
                com::sun::star::uno::getCurrentContext()));

        // set/unset hiding disabled menu entries
        bool bDontHide           = SvtMenuOptions().IsEntryHidingEnabled();
        const StyleSettings& rSettings = Application::GetSettings().GetStyleSettings();
        bool bShowMenuImages     = rSettings.GetUseImagesInMenus();
        bool bHasDisabledEntries = SvtCommandOptions().HasEntries( SvtCommandOptions::CMDOPTION_DISABLED );

        SolarMutexGuard g;

        sal_uInt16 nFlag = pMenu->GetMenuFlags();
        if ( bDontHide )
            nFlag &= ~MENU_FLAG_HIDEDISABLEDENTRIES;
        else
            nFlag |= MENU_FLAG_HIDEDISABLEDENTRIES;
        pMenu->SetMenuFlags( nFlag );

        if ( m_bActive )
            return 0;

        m_bActive = true;

        OUString aMenuCommand( m_aMenuItemCommand );
        if ( m_aMenuItemCommand == aSpecialWindowMenu || m_aMenuItemCommand == aSlotSpecialWindowMenu || aMenuCommand == aSpecialWindowCommand )
             MenuManager::UpdateSpecialWindowMenu( pMenu, m_xContext );

        // Check if some modes have changed so we have to update our menu images
        OUString sIconTheme = SvtMiscOptions().GetIconTheme();

        if ( m_bRetrieveImages ||
             bShowMenuImages != m_bShowMenuImages ||
             sIconTheme != m_sIconTheme )
        {
            m_bShowMenuImages   = bShowMenuImages;
            m_bRetrieveImages   = false;
            m_sIconTheme     = sIconTheme;
            MenuManager::FillMenuImages( m_xFrame, pMenu, bShowMenuImages );
        }

        // Try to map commands to labels
        for ( sal_uInt16 nPos = 0; nPos < pMenu->GetItemCount(); nPos++ )
        {
            sal_uInt16 nItemId = pMenu->GetItemId( nPos );
            if (( pMenu->GetItemType( nPos ) != MENUITEM_SEPARATOR ) &&
                ( pMenu->GetItemText( nItemId ).isEmpty() ))
            {
                OUString aCommand = pMenu->GetItemCommand( nItemId );
                if ( !aCommand.isEmpty() ) {
                    pMenu->SetItemText( nItemId, RetrieveLabelFromCommand( aCommand ));
                }
            }
        }

        // Try to set accelerator keys
        {
            RetrieveShortcuts( m_aMenuItemHandlerVector );
            std::vector< MenuItemHandler* >::iterator p;
            for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
            {
                MenuItemHandler* pMenuItemHandler = *p;

                // Set key code, workaround for hard-coded shortcut F1 mapped to .uno:HelpIndex
                // Only non-popup menu items can have a short-cut
                if ( pMenuItemHandler->aMenuItemURL == aCmdHelpIndex )
                {
                    KeyCode aKeyCode( KEY_F1 );
                    pMenu->SetAccelKey( pMenuItemHandler->nItemId, aKeyCode );
                }
                else if ( pMenu->GetPopupMenu( pMenuItemHandler->nItemId ) == 0 )
                    pMenu->SetAccelKey( pMenuItemHandler->nItemId, pMenuItemHandler->aKeyCode );
            }
        }

        URL aTargetURL;

        // Use provided dispatch provider => fallback to frame as dispatch provider
        Reference< XDispatchProvider > xDispatchProvider;
        if ( m_xDispatchProvider.is() )
            xDispatchProvider = m_xDispatchProvider;
        else
            xDispatchProvider = Reference< XDispatchProvider >( m_xFrame, UNO_QUERY );

        if ( xDispatchProvider.is() )
        {
            KeyCode             aEmptyKeyCode;
            SvtCommandOptions   aCmdOptions;
            std::vector< MenuItemHandler* >::iterator p;
            for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
            {
                MenuItemHandler* pMenuItemHandler = *p;
                if ( pMenuItemHandler )
                {
                    if ( !pMenuItemHandler->xMenuItemDispatch.is() &&
                         !pMenuItemHandler->xSubMenuManager.is()      )
                    {
                        // There is no dispatch mechanism for the special window list menu items,
                        // because they are handled directly through XFrame->activate!!!
                        // Don't update dispatches for special file menu items.
                        if ( !(( pMenuItemHandler->nItemId >= START_ITEMID_WINDOWLIST &&
                                 pMenuItemHandler->nItemId < END_ITEMID_WINDOWLIST )))
                        {
                            Reference< XDispatch > xMenuItemDispatch;

                            OUString aItemCommand = pMenu->GetItemCommand( pMenuItemHandler->nItemId );
                            if ( aItemCommand.isEmpty() )
                            {
                                aItemCommand = "slot:" + OUString::number( pMenuItemHandler->nItemId );
                                pMenu->SetItemCommand( pMenuItemHandler->nItemId, aItemCommand );
                            }

                            aTargetURL.Complete = aItemCommand;

                            m_xURLTransformer->parseStrict( aTargetURL );

                            if ( bHasDisabledEntries )
                            {
                                if ( aCmdOptions.Lookup( SvtCommandOptions::CMDOPTION_DISABLED, aTargetURL.Path ))
                                    pMenu->HideItem( pMenuItemHandler->nItemId );
                            }

                            if ( m_bIsBookmarkMenu )
                                xMenuItemDispatch = xDispatchProvider->queryDispatch( aTargetURL, pMenuItemHandler->aTargetFrame, 0 );
                            else
                                xMenuItemDispatch = xDispatchProvider->queryDispatch( aTargetURL, OUString(), 0 );

                            bool bPopupMenu( false );
                            if ( !pMenuItemHandler->xPopupMenuController.is() &&
                                 m_xPopupMenuControllerFactory->hasController( aItemCommand, OUString() ))
                            {
                                bPopupMenu = CreatePopupMenuController( pMenuItemHandler );
                            }
                            else if ( pMenuItemHandler->xPopupMenuController.is() )
                            {
                                // Force update of popup menu
                                pMenuItemHandler->xPopupMenuController->updatePopupMenu();
                                bPopupMenu = true;
                                if (PopupMenu*  pThisPopup = pMenu->GetPopupMenu( pMenuItemHandler->nItemId ))
                                    pMenu->EnableItem( pMenuItemHandler->nItemId, pThisPopup->GetItemCount() ? true : false );
                            }

                            lcl_CheckForChildren(pMenu, pMenuItemHandler->nItemId);

                            if ( xMenuItemDispatch.is() )
                            {
                                pMenuItemHandler->xMenuItemDispatch = xMenuItemDispatch;
                                pMenuItemHandler->aMenuItemURL      = aTargetURL.Complete;

                                if ( !bPopupMenu )
                                {
                                    xMenuItemDispatch->addStatusListener( static_cast< XStatusListener* >( this ), aTargetURL );
                                    xMenuItemDispatch->removeStatusListener( static_cast< XStatusListener* >( this ), aTargetURL );
                                    xMenuItemDispatch->addStatusListener( static_cast< XStatusListener* >( this ), aTargetURL );
                                }
                            }
                            else if ( !bPopupMenu )
                                pMenu->EnableItem( pMenuItemHandler->nItemId, false );
                        }
                    }
                    else if ( pMenuItemHandler->xPopupMenuController.is() )
                    {
                        // Force update of popup menu
                        pMenuItemHandler->xPopupMenuController->updatePopupMenu();
                        lcl_CheckForChildren(pMenu, pMenuItemHandler->nItemId);
                    }
                    else if ( pMenuItemHandler->xMenuItemDispatch.is() )
                    {
                        // We need an update to reflect the current state
                        try
                        {
                            aTargetURL.Complete = pMenuItemHandler->aMenuItemURL;
                            m_xURLTransformer->parseStrict( aTargetURL );

                            pMenuItemHandler->xMenuItemDispatch->addStatusListener(
                                                                    static_cast< XStatusListener* >( this ), aTargetURL );
                            pMenuItemHandler->xMenuItemDispatch->removeStatusListener(
                                                                    static_cast< XStatusListener* >( this ), aTargetURL );
                        }
                        catch ( const Exception& )
                        {
                        }
                    }
                    else if ( pMenuItemHandler->xSubMenuManager.is() )
                        lcl_CheckForChildren(pMenu, pMenuItemHandler->nItemId);
                }
            }
        }
    }

    return 1;
}

IMPL_LINK( MenuBarManager, Deactivate, Menu *, pMenu )
{
    if ( pMenu == m_pVCLMenu )
    {
        m_bActive = false;
        if ( pMenu->IsMenuBar() && m_xDeferedItemContainer.is() )
        {
            // Start timer to handle settings asynchronous
            // Changing the menu inside this handler leads to
            // a crash under X!
            m_aAsyncSettingsTimer.SetTimeoutHdl(LINK(this, MenuBarManager, AsyncSettingsHdl));
            m_aAsyncSettingsTimer.SetTimeout(10);
            m_aAsyncSettingsTimer.Start();
        }
    }

    return 1;
}

IMPL_LINK( MenuBarManager, AsyncSettingsHdl, Timer*,)
{
    SolarMutexGuard g;
    Reference< XInterface > xSelfHold(
        static_cast< ::cppu::OWeakObject* >( this ), UNO_QUERY_THROW );

    m_aAsyncSettingsTimer.Stop();
    if ( !m_bActive && m_xDeferedItemContainer.is() )
    {
        SetItemContainer( m_xDeferedItemContainer );
        m_xDeferedItemContainer.clear();
    }

    return 0;
}

IMPL_LINK( MenuBarManager, Select, Menu *, pMenu )
{
    URL                     aTargetURL;
    Sequence<PropertyValue> aArgs;
    Reference< XDispatch >  xDispatch;

    {
        SolarMutexGuard g;

        sal_uInt16 nCurItemId = pMenu->GetCurItemId();
        sal_uInt16 nCurPos    = pMenu->GetItemPos( nCurItemId );
        if ( pMenu == m_pVCLMenu &&
             pMenu->GetItemType( nCurPos ) != MENUITEM_SEPARATOR )
        {
            if ( nCurItemId >= START_ITEMID_WINDOWLIST &&
                 nCurItemId <= END_ITEMID_WINDOWLIST )
            {
                // window list menu item selected

                Reference< XDesktop2 > xDesktop = Desktop::create( m_xContext );

                sal_uInt16 nTaskId = START_ITEMID_WINDOWLIST;
                Reference< XIndexAccess > xList( xDesktop->getFrames(), UNO_QUERY );
                sal_Int32 nCount = xList->getCount();
                for ( sal_Int32 i=0; i<nCount; ++i )
                {
                    Reference< XFrame > xFrame;
                    xList->getByIndex(i) >>= xFrame;
                    if ( xFrame.is() && nTaskId == nCurItemId )
                    {
                        Window* pWin = VCLUnoHelper::GetWindow( xFrame->getContainerWindow() );
                        pWin->GrabFocus();
                        pWin->ToTop( TOTOP_RESTOREWHENMIN );
                        break;
                    }

                    nTaskId++;
                }
            }
            else
            {
                MenuItemHandler* pMenuItemHandler = GetMenuItemHandler( nCurItemId );
                if ( pMenuItemHandler && pMenuItemHandler->xMenuItemDispatch.is() )
                {
                    aTargetURL.Complete = pMenuItemHandler->aMenuItemURL;
                    m_xURLTransformer->parseStrict( aTargetURL );

                    if ( m_bIsBookmarkMenu )
                    {
                        // bookmark menu item selected
                        aArgs.realloc( 1 );
                        aArgs[0].Name = "Referer";
                        aArgs[0].Value <<= OUString( "private:user" );
                    }

                    xDispatch = pMenuItemHandler->xMenuItemDispatch;
                }
            }
        }
    }

    if ( xDispatch.is() )
    {
        const sal_uInt32 nRef = Application::ReleaseSolarMutex();
        xDispatch->dispatch( aTargetURL, aArgs );
        Application::AcquireSolarMutex( nRef );
    }

    return 1;
}

IMPL_LINK_NOARG(MenuBarManager, Highlight)
{
    return 0;
}

bool MenuBarManager::MustBeHidden( PopupMenu* pPopupMenu, const Reference< XURLTransformer >& rTransformer )
{
    if ( pPopupMenu )
    {
        URL               aTargetURL;
        SvtCommandOptions aCmdOptions;

        sal_uInt16 nCount = pPopupMenu->GetItemCount();
        sal_uInt16 nHideCount( 0 );

        for ( sal_uInt16 i = 0; i < nCount; i++ )
        {
            sal_uInt16 nId = pPopupMenu->GetItemId( i );
            if ( nId > 0 )
            {
                PopupMenu* pSubPopupMenu = pPopupMenu->GetPopupMenu( nId );
                if ( pSubPopupMenu )
                {
                    if ( MustBeHidden( pSubPopupMenu, rTransformer ))
                    {
                        pPopupMenu->HideItem( nId );
                        ++nHideCount;
                    }
                }
                else
                {
                    aTargetURL.Complete = pPopupMenu->GetItemCommand( nId );
                    rTransformer->parseStrict( aTargetURL );

                    if ( aCmdOptions.Lookup( SvtCommandOptions::CMDOPTION_DISABLED, aTargetURL.Path ))
                        ++nHideCount;
                }
            }
            else
                ++nHideCount;
        }

        return ( nCount == nHideCount );
    }

    return true;
}

OUString MenuBarManager::RetrieveLabelFromCommand(const OUString& rCmdURL)
{
    return framework::RetrieveLabelFromCommand(rCmdURL, m_xContext, m_xUICommandLabels,m_xFrame,m_aModuleIdentifier,m_bModuleIdentified,"Label");
}

bool MenuBarManager::CreatePopupMenuController( MenuItemHandler* pMenuItemHandler )
{
    OUString aItemCommand( pMenuItemHandler->aMenuItemURL );

    // Try instanciate a popup menu controller. It is stored in the menu item handler.
    if ( !m_xPopupMenuControllerFactory.is() )
        return false;

    Sequence< Any > aSeq( 2 );
    PropertyValue aPropValue;

    aPropValue.Name         = "ModuleIdentifier";
    aPropValue.Value      <<= m_aModuleIdentifier;
    aSeq[0] <<= aPropValue;
    aPropValue.Name         = "Frame";
    aPropValue.Value      <<= m_xFrame;
    aSeq[1] <<= aPropValue;

    Reference< XPopupMenuController > xPopupMenuController(
                                            m_xPopupMenuControllerFactory->createInstanceWithArgumentsAndContext(
                                                aItemCommand,
                                                aSeq,
                                                m_xContext ),
                                            UNO_QUERY );

    if ( xPopupMenuController.is() )
    {
        // Provide our awt popup menu to the popup menu controller
        pMenuItemHandler->xPopupMenuController = xPopupMenuController;
        xPopupMenuController->setPopupMenu( pMenuItemHandler->xPopupMenu );
        return true;
    }

    return false;
}

void MenuBarManager::FillMenuManager( Menu* pMenu, const Reference< XFrame >& rFrame, const Reference< XDispatchProvider >& rDispatchProvider, const OUString& rModuleIdentifier, bool bDelete, bool bDeleteChildren )
{
    m_xFrame            = rFrame;
    m_bActive           = false;
    m_bDeleteMenu       = bDelete;
    m_bDeleteChildren   = bDeleteChildren;
    m_pVCLMenu          = pMenu;
    m_bInitialized      = false;
    m_bIsBookmarkMenu   = false;
    m_xDispatchProvider = rDispatchProvider;

    const StyleSettings& rSettings = Application::GetSettings().GetStyleSettings();
    m_bShowMenuImages   = rSettings.GetUseImagesInMenus();
    m_bRetrieveImages   = false;

    sal_Int32 nAddonsURLPrefixLength = ADDONSPOPUPMENU_URL_PREFIX.getLength();

    // Add root as ui configuration listener
    RetrieveImageManagers();

    if ( pMenu->IsMenuBar() && rFrame.is() )
    {
        // First merge all addon popup menus into our structure
        sal_uInt16 nPos = 0;
        for ( nPos = 0; nPos < pMenu->GetItemCount(); nPos++ )
        {
            sal_uInt16          nItemId  = pMenu->GetItemId( nPos );
            OUString aCommand = pMenu->GetItemCommand( nItemId );
            if ( nItemId == SID_MDIWINDOWLIST || aCommand == aSpecialWindowCommand)
            {
                // Retrieve addon popup menus and add them to our menu bar
                framework::AddonMenuManager::MergeAddonPopupMenus( rFrame, nPos, (MenuBar *)pMenu, m_xContext );
                break;
            }
        }

        // Merge the Add-Ons help menu items into the Office help menu
        framework::AddonMenuManager::MergeAddonHelpMenu( rFrame, (MenuBar *)pMenu, m_xContext );
    }

    OUString    aEmpty;
    bool    bAccessibilityEnabled( Application::GetSettings().GetMiscSettings().GetEnableATToolSupport() );
    sal_uInt16 nItemCount = pMenu->GetItemCount();
    OUString aItemCommand;
    m_aMenuItemHandlerVector.reserve(nItemCount);
    for ( sal_uInt16 i = 0; i < nItemCount; i++ )
    {
        sal_uInt16 nItemId = FillItemCommand(aItemCommand,pMenu, i );

        // Set module identifier when provided from outside
        if ( !rModuleIdentifier.isEmpty() )
        {
            m_aModuleIdentifier = rModuleIdentifier;
            m_bModuleIdentified = true;
        }

        if (( pMenu->IsMenuBar() || bAccessibilityEnabled ) &&
            ( pMenu->GetItemText( nItemId ).isEmpty() ))
        {
            if ( !aItemCommand.isEmpty() )
                pMenu->SetItemText( nItemId, RetrieveLabelFromCommand( aItemCommand ));
        }

        Reference< XDispatch > xDispatch;
        Reference< XStatusListener > xStatusListener;
        PopupMenu* pPopup = pMenu->GetPopupMenu( nItemId );
        bool bItemShowMenuImages = m_bShowMenuImages;
        // overwrite the show icons on menu option?
        if (!bItemShowMenuImages)
        {
            MenuItemBits nBits =  pMenu->GetItemBits( nItemId );
            bItemShowMenuImages = ( ( nBits & MIB_ICON ) == MIB_ICON );
        }
        if ( pPopup )
        {
            // Retrieve module identifier from Help Command entry
            OUString aModuleIdentifier( rModuleIdentifier );
            if (!pMenu->GetHelpCommand(nItemId).isEmpty())
            {
                aModuleIdentifier = pMenu->GetHelpCommand( nItemId );
                pMenu->SetHelpCommand( nItemId, aEmpty );
            }

            if ( m_xPopupMenuControllerFactory.is() &&
                 pPopup->GetItemCount() == 0 &&
                 m_xPopupMenuControllerFactory->hasController( aItemCommand, OUString() )
                  )
            {
                // Check if we have to create a popup menu for a uno based popup menu controller.
                // We have to set an empty popup menu into our menu structure so the controller also
                // works with inplace OLE. Remove old dummy popup menu!
                MenuItemHandler* pItemHandler = new MenuItemHandler( nItemId, xStatusListener, xDispatch );
                VCLXPopupMenu* pVCLXPopupMenu = new VCLXPopupMenu;
                PopupMenu* pNewPopupMenu = (PopupMenu *)pVCLXPopupMenu->GetMenu();
                pMenu->SetPopupMenu( nItemId, pNewPopupMenu );
                pItemHandler->xPopupMenu = Reference< com::sun::star::awt::XPopupMenu >( (OWeakObject *)pVCLXPopupMenu, UNO_QUERY );
                pItemHandler->aMenuItemURL = aItemCommand;
                m_aMenuItemHandlerVector.push_back( pItemHandler );
                delete pPopup;

                if ( bAccessibilityEnabled )
                {
                    if ( CreatePopupMenuController( pItemHandler ))
                        pItemHandler->xPopupMenuController->updatePopupMenu();
                }
                lcl_CheckForChildren(pMenu, nItemId);
            }
            else if (( aItemCommand.getLength() > nAddonsURLPrefixLength ) &&
                     ( aItemCommand.startsWith( ADDONSPOPUPMENU_URL_PREFIX ) ))
            {
                // A special addon popup menu, must be created with a different ctor
                MenuBarManager* pSubMenuManager = new MenuBarManager( m_xContext, m_xFrame, m_xURLTransformer,(AddonPopupMenu *)pPopup, bDeleteChildren, bDeleteChildren );
                AddMenu(pSubMenuManager,aItemCommand,nItemId);
            }
            else
            {
                Reference< XDispatchProvider > xPopupMenuDispatchProvider( rDispatchProvider );

                // Retrieve possible attributes struct
                MenuConfiguration::Attributes* pAttributes = (MenuConfiguration::Attributes *)(pMenu->GetUserValue( nItemId ));
                if ( pAttributes )
                    xPopupMenuDispatchProvider = pAttributes->xDispatchProvider;

                // Check if this is the help menu. Add menu item if needed
                if ( nItemId == SID_HELPMENU || aItemCommand == aSlotHelpMenu || aItemCommand == aCmdHelpMenu )
                {
                    // Check if this is the help menu. Add menu item if needed
                    CheckAndAddMenuExtension( pPopup );
                }
                else if (( nItemId == SID_ADDONLIST || aItemCommand == aSlotSpecialToolsMenu || aItemCommand == aCmdToolsMenu ) &&
                        AddonMenuManager::HasAddonMenuElements() )
                {
                    // Create addon popup menu if there exist elements and this is the tools popup menu
                    AddonMenu* pSubMenu = AddonMenuManager::CreateAddonMenu(rFrame, m_xContext);
                    if ( pSubMenu && ( pSubMenu->GetItemCount() > 0 ))
                    {
                        sal_uInt16 nCount = 0;
                        if ( pPopup->GetItemType( nCount-1 ) != MENUITEM_SEPARATOR )
                            pPopup->InsertSeparator();

                        // Use resource to load popup menu title
                        OUString aAddonsStrRes(FWK_RESSTR(STR_MENU_ADDONS));
                        pPopup->InsertItem( ITEMID_ADDONLIST, aAddonsStrRes );
                        pPopup->SetPopupMenu( ITEMID_ADDONLIST, pSubMenu );

                        // Set item command for popup menu to enable it for GetImageFromURL
                        OUString aNewItemCommand = "slot:" + OUString::number( ITEMID_ADDONLIST );
                        pPopup->SetItemCommand( ITEMID_ADDONLIST, aNewItemCommand );
                    }
                    else
                        delete pSubMenu;
                }

                if ( nItemId == ITEMID_ADDONLIST )
                {
                    AddonMenu* pSubMenu = dynamic_cast< AddonMenu* >( pPopup );
                    if ( pSubMenu )
                    {
                        MenuBarManager* pSubMenuManager = new MenuBarManager( m_xContext, m_xFrame, m_xURLTransformer,pSubMenu, true, false );
                        AddMenu(pSubMenuManager,aItemCommand,nItemId);
                        pSubMenuManager->m_aMenuItemCommand = OUString();

                        // Set image for the addon popup menu item
                        if ( bItemShowMenuImages && !pPopup->GetItemImage( ITEMID_ADDONLIST ))
                        {
                            Reference< XFrame > xTemp( rFrame );
                            Image aImage = GetImageFromURL( xTemp, aItemCommand, false );
                            if ( !!aImage )
                                   pPopup->SetItemImage( ITEMID_ADDONLIST, aImage );
                        }
                    }
                }
                else
                {
                    MenuBarManager* pSubMenuMgr = new MenuBarManager( m_xContext, rFrame, m_xURLTransformer,rDispatchProvider, aModuleIdentifier, pPopup, bDeleteChildren, bDeleteChildren );
                    AddMenu(pSubMenuMgr,aItemCommand,nItemId);
                }
            }
        }
        else if ( pMenu->GetItemType( i ) != MENUITEM_SEPARATOR )
        {
            if ( bItemShowMenuImages )
            {
                if ( AddonMenuManager::IsAddonMenuId( nItemId ))
                {
                    // Add-Ons uses images from different places
                    Image           aImage;
                    OUString   aImageId;

                    MenuConfiguration::Attributes* pMenuAttributes =
                        (MenuConfiguration::Attributes*)pMenu->GetUserValue( nItemId );

                    if ( pMenuAttributes && !pMenuAttributes->aImageId.isEmpty() )
                    {
                        // Retrieve image id from menu attributes
                        aImage = GetImageFromURL( m_xFrame, aImageId, false );
                    }

                    if ( !aImage )
                    {
                        aImage = GetImageFromURL( m_xFrame, aItemCommand, false );
                        if ( !aImage )
                            aImage = AddonsOptions().GetImageFromURL( aItemCommand, false );
                    }

                    if ( !!aImage )
                        pMenu->SetItemImage( nItemId, aImage );
                    else
                        m_bRetrieveImages = true;
                }
                m_bRetrieveImages = true;
            }

            MenuItemHandler* pItemHandler = new MenuItemHandler( nItemId, xStatusListener, xDispatch );
            pItemHandler->aMenuItemURL = aItemCommand;

            if ( m_xPopupMenuControllerFactory.is() &&
                 m_xPopupMenuControllerFactory->hasController( aItemCommand, OUString() ))
            {
                // Check if we have to create a popup menu for a uno based popup menu controller.
                // We have to set an empty popup menu into our menu structure so the controller also
                // works with inplace OLE.
                VCLXPopupMenu* pVCLXPopupMenu = new VCLXPopupMenu;
                PopupMenu* pPopupMenu = (PopupMenu *)pVCLXPopupMenu->GetMenu();
                pMenu->SetPopupMenu( pItemHandler->nItemId, pPopupMenu );
                pItemHandler->xPopupMenu = Reference< com::sun::star::awt::XPopupMenu >( (OWeakObject *)pVCLXPopupMenu, UNO_QUERY );

                if ( bAccessibilityEnabled && CreatePopupMenuController( pItemHandler ) )
                {
                    pItemHandler->xPopupMenuController->updatePopupMenu();
                }

                lcl_CheckForChildren(pMenu, pItemHandler->nItemId);
            }

            m_aMenuItemHandlerVector.push_back( pItemHandler );
        }
    }

    if ( bAccessibilityEnabled )
    {
        RetrieveShortcuts( m_aMenuItemHandlerVector );
        std::vector< MenuItemHandler* >::iterator p;
        for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
        {
            MenuItemHandler* pMenuItemHandler = *p;

            // Set key code, workaround for hard-coded shortcut F1 mapped to .uno:HelpIndex
            // Only non-popup menu items can have a short-cut
            if ( pMenuItemHandler->aMenuItemURL == aCmdHelpIndex )
            {
                KeyCode aKeyCode( KEY_F1 );
                pMenu->SetAccelKey( pMenuItemHandler->nItemId, aKeyCode );
            }
            else if ( pMenu->GetPopupMenu( pMenuItemHandler->nItemId ) == 0 )
                pMenu->SetAccelKey( pMenuItemHandler->nItemId, pMenuItemHandler->aKeyCode );
        }
    }

    SetHdl();
}

void MenuBarManager::impl_RetrieveShortcutsFromConfiguration(
    const Reference< XAcceleratorConfiguration >& rAccelCfg,
    const Sequence< OUString >& rCommands,
    std::vector< MenuItemHandler* >& aMenuShortCuts )
{
    if ( rAccelCfg.is() )
    {
        try
        {
            com::sun::star::awt::KeyEvent aKeyEvent;
            Sequence< Any > aSeqKeyCode = rAccelCfg->getPreferredKeyEventsForCommandList( rCommands );
            for ( sal_Int32 i = 0; i < aSeqKeyCode.getLength(); i++ )
            {
                if ( aSeqKeyCode[i] >>= aKeyEvent )
                    aMenuShortCuts[i]->aKeyCode = svt::AcceleratorExecute::st_AWTKey2VCLKey( aKeyEvent );
            }
        }
        catch ( const IllegalArgumentException& )
        {
        }
    }
}

void MenuBarManager::RetrieveShortcuts( std::vector< MenuItemHandler* >& aMenuShortCuts )
{
    if ( !m_bModuleIdentified )
    {
        m_bModuleIdentified = true;
        Reference< XModuleManager2 > xModuleManager = ModuleManager::create( m_xContext );

        try
        {
            m_aModuleIdentifier = xModuleManager->identify( m_xFrame );
        }
        catch( const Exception& )
        {
        }
    }

    if ( m_bModuleIdentified )
    {
        Reference< XAcceleratorConfiguration > xDocAccelCfg( m_xDocAcceleratorManager );
        Reference< XAcceleratorConfiguration > xModuleAccelCfg( m_xModuleAcceleratorManager );
        Reference< XAcceleratorConfiguration > xGlobalAccelCfg( m_xGlobalAcceleratorManager );

        if ( !m_bAcceleratorCfg )
        {
            // Retrieve references on demand
            m_bAcceleratorCfg = true;
            if ( !xDocAccelCfg.is() )
            {
                Reference< XController > xController = m_xFrame->getController();
                Reference< XModel > xModel;
                if ( xController.is() )
                {
                    xModel = xController->getModel();
                    if ( xModel.is() )
                    {
                        Reference< XUIConfigurationManagerSupplier > xSupplier( xModel, UNO_QUERY );
                        if ( xSupplier.is() )
                        {
                            Reference< XUIConfigurationManager > xDocUICfgMgr( xSupplier->getUIConfigurationManager(), UNO_QUERY );
                            if ( xDocUICfgMgr.is() )
                            {
                                xDocAccelCfg = xDocUICfgMgr->getShortCutManager();
                                m_xDocAcceleratorManager = xDocAccelCfg;
                            }
                        }
                    }
                }
            }

            if ( !xModuleAccelCfg.is() )
            {
                Reference< XModuleUIConfigurationManagerSupplier > xModuleCfgMgrSupplier =
                    theModuleUIConfigurationManagerSupplier::get( m_xContext );
                try
                {
                    Reference< XUIConfigurationManager > xUICfgMgr = xModuleCfgMgrSupplier->getUIConfigurationManager( m_aModuleIdentifier );
                    if ( xUICfgMgr.is() )
                    {
                        xModuleAccelCfg = xUICfgMgr->getShortCutManager();
                        m_xModuleAcceleratorManager = xModuleAccelCfg;
                    }
                }
                catch ( const RuntimeException& )
                {
                    throw;
                }
                catch ( const Exception& )
                {
                }
            }

            if ( !xGlobalAccelCfg.is() ) try
            {
                xGlobalAccelCfg = GlobalAcceleratorConfiguration::create( m_xContext );
                m_xGlobalAcceleratorManager = xGlobalAccelCfg;
            }
            catch ( const css::uno::DeploymentException& )
            {
                SAL_WARN("fwk.uielement", "GlobalAcceleratorConfiguration"
                        " not available. This should happen only on mobile platforms.");
            }
        }

        KeyCode aEmptyKeyCode;
        Sequence< OUString > aSeq( aMenuShortCuts.size() );
        const sal_uInt32 nCount = aMenuShortCuts.size();
        for ( sal_uInt32 i = 0; i < nCount; ++i )
        {
            aSeq[i] = aMenuShortCuts[i]->aMenuItemURL;
            aMenuShortCuts[i]->aKeyCode = aEmptyKeyCode;
        }

        if ( m_xGlobalAcceleratorManager.is() )
            impl_RetrieveShortcutsFromConfiguration( xGlobalAccelCfg, aSeq, aMenuShortCuts );
        if ( m_xModuleAcceleratorManager.is() )
            impl_RetrieveShortcutsFromConfiguration( xModuleAccelCfg, aSeq, aMenuShortCuts );
        if ( m_xDocAcceleratorManager.is() )
            impl_RetrieveShortcutsFromConfiguration( xDocAccelCfg, aSeq, aMenuShortCuts );
    }
}

void MenuBarManager::RetrieveImageManagers()
{
    if ( !m_xDocImageManager.is() )
    {
        Reference< XController > xController = m_xFrame->getController();
        Reference< XModel > xModel;
        if ( xController.is() )
        {
            xModel = xController->getModel();
            if ( xModel.is() )
            {
                Reference< XUIConfigurationManagerSupplier > xSupplier( xModel, UNO_QUERY );
                if ( xSupplier.is() )
                {
                    Reference< XUIConfigurationManager > xDocUICfgMgr( xSupplier->getUIConfigurationManager(), UNO_QUERY );
                    m_xDocImageManager = Reference< XImageManager >( xDocUICfgMgr->getImageManager(), UNO_QUERY );
                    m_xDocImageManager->addConfigurationListener(
                                            Reference< XUIConfigurationListener >(
                                                static_cast< OWeakObject* >( this ), UNO_QUERY ));
                }
            }
        }
    }

    Reference< XModuleManager2 > xModuleManager;
    if ( m_aModuleIdentifier.isEmpty() )
        xModuleManager.set( ModuleManager::create( m_xContext ) );

    try
    {
        if ( xModuleManager.is() )
            m_aModuleIdentifier = xModuleManager->identify( Reference< XInterface >( m_xFrame, UNO_QUERY ) );
    }
    catch( const Exception& )
    {
    }

    if ( !m_xModuleImageManager.is() )
    {
        Reference< XModuleUIConfigurationManagerSupplier > xModuleCfgMgrSupplier =
            theModuleUIConfigurationManagerSupplier::get( m_xContext );
        Reference< XUIConfigurationManager > xUICfgMgr = xModuleCfgMgrSupplier->getUIConfigurationManager( m_aModuleIdentifier );
        m_xModuleImageManager.set( xUICfgMgr->getImageManager(), UNO_QUERY );
        m_xModuleImageManager->addConfigurationListener( Reference< XUIConfigurationListener >(
                                                            static_cast< OWeakObject* >( this ), UNO_QUERY ));
    }
}

void MenuBarManager::FillMenuWithConfiguration(
    sal_uInt16&                         nId,
    Menu*                               pMenu,
    const OUString&              rModuleIdentifier,
    const Reference< XIndexAccess >&    rItemContainer,
    const Reference< XURLTransformer >& rTransformer )
{
    Reference< XDispatchProvider > xEmptyDispatchProvider;
    MenuBarManager::FillMenu( nId, pMenu, rModuleIdentifier, rItemContainer, xEmptyDispatchProvider );

    // Merge add-on menu entries into the menu bar
    MenuBarManager::MergeAddonMenus( static_cast< Menu* >( pMenu ),
                                     AddonsOptions().GetMergeMenuInstructions(),
                                     rModuleIdentifier );

    bool bHasDisabledEntries = SvtCommandOptions().HasEntries( SvtCommandOptions::CMDOPTION_DISABLED );
    if ( bHasDisabledEntries )
    {
        sal_uInt16 nCount = pMenu->GetItemCount();
        for ( sal_uInt16 i = 0; i < nCount; i++ )
        {
            sal_uInt16 nID = pMenu->GetItemId( i );
            if ( nID > 0 )
            {
                PopupMenu* pPopupMenu = pMenu->GetPopupMenu( nID );
                if ( pPopupMenu )
                {
                    if ( MustBeHidden( pPopupMenu, rTransformer ))
                        pMenu->HideItem( nId );
                }
            }
        }
    }
}

void MenuBarManager::FillMenu(
    sal_uInt16&                           nId,
    Menu*                                 pMenu,
    const OUString&                  rModuleIdentifier,
    const Reference< XIndexAccess >&      rItemContainer,
    const Reference< XDispatchProvider >& rDispatchProvider )
{
    // Fill menu bar with container contents
     for ( sal_Int32 n = 0; n < rItemContainer->getCount(); n++ )
    {
        Sequence< PropertyValue >       aProp;
        OUString                   aCommandURL;
        OUString                   aLabel;
        OUString                   aHelpURL;
        OUString                   aModuleIdentifier( rModuleIdentifier );
        bool                        bShow(true);
        bool                        bEnabled(true);
        sal_uInt16                      nType = 0;
        Reference< XIndexAccess >       xIndexContainer;
        Reference< XDispatchProvider >  xDispatchProvider( rDispatchProvider );
        sal_Int16 nStyle = 0;
        try
        {
            if ( rItemContainer->getByIndex( n ) >>= aProp )
            {
                for ( int i = 0; i < aProp.getLength(); i++ )
                {
                    OUString aPropName = aProp[i].Name;
                    if ( aPropName.equalsAsciiL( ITEM_DESCRIPTOR_COMMANDURL, LEN_DESCRIPTOR_COMMANDURL ))
                        aProp[i].Value >>= aCommandURL;
                    else if ( aPropName.equalsAsciiL( ITEM_DESCRIPTOR_HELPURL, LEN_DESCRIPTOR_HELPURL ))
                        aProp[i].Value >>= aHelpURL;
                    else if ( aPropName.equalsAsciiL( ITEM_DESCRIPTOR_CONTAINER, LEN_DESCRIPTOR_CONTAINER ))
                        aProp[i].Value >>= xIndexContainer;
                    else if ( aPropName.equalsAsciiL( ITEM_DESCRIPTOR_LABEL, LEN_DESCRIPTOR_LABEL ))
                        aProp[i].Value >>= aLabel;
                    else if ( aPropName.equalsAsciiL( ITEM_DESCRIPTOR_TYPE, LEN_DESCRIPTOR_TYPE ))
                        aProp[i].Value >>= nType;
                    else if ( aPropName.equalsAsciiL( ITEM_DESCRIPTOR_MODULEIDENTIFIER, LEN_DESCRIPTOR_MODULEIDENTIFIER ))
                        aProp[i].Value >>= aModuleIdentifier;
                    else if ( aPropName.equalsAsciiL( ITEM_DESCRIPTOR_DISPATCHPROVIDER, LEN_DESCRIPTOR_DISPATCHPROVIDER ))
                        aProp[i].Value >>= xDispatchProvider;
                    else if ( aProp[i].Name.equalsAsciiL( ITEM_DESCRIPTOR_STYLE, LEN_DESCRIPTOR_STYLE ))
                        aProp[i].Value >>= nStyle;
                    else if ( aProp[i].Name.equalsAsciiL( ITEM_DESCRIPTOR_ISVISIBLE, LEN_DESCRIPTOR_ISVISIBLE ))
                        aProp[i].Value >>= bShow;
                    else if ( aProp[i].Name.equalsAsciiL( ITEM_DESCRIPTOR_ENABLED, LEN_DESCRIPTOR_ENABLED ))
                        aProp[i].Value >>= bEnabled;
                }

                if ( nType == ::com::sun::star::ui::ItemType::DEFAULT )
                {
                    pMenu->InsertItem( nId, aLabel );
                    pMenu->SetItemCommand( nId, aCommandURL );

                    if ( nStyle )
                    {
                        MenuItemBits nBits = pMenu->GetItemBits( nId );
                        if ( nStyle & ::com::sun::star::ui::ItemStyle::ICON )
                           nBits |= MIB_ICON;
                        if ( nStyle & ::com::sun::star::ui::ItemStyle::TEXT )
                           nBits |= MIB_TEXT;
                        if ( nStyle & ::com::sun::star::ui::ItemStyle::RADIO_CHECK )
                           nBits |= MIB_RADIOCHECK;
                        pMenu->SetItemBits( nId, nBits );
                    }

                    if ( !bShow )
                        pMenu->HideItem( nId );

                    if ( !bEnabled)
                        pMenu->EnableItem( nId, false );

                    if ( xIndexContainer.is() )
                    {
                        PopupMenu* pNewPopupMenu = new PopupMenu;
                        pMenu->SetPopupMenu( nId, pNewPopupMenu );

                        if ( xDispatchProvider.is() )
                        {
                            // Use attributes struct to transport special dispatch provider
                            MenuConfiguration::Attributes* pAttributes = new MenuConfiguration::Attributes;
                            pAttributes->xDispatchProvider = xDispatchProvider;
                            pMenu->SetUserValue( nId, (sal_uIntPtr)( pAttributes ));
                        }

                        // Use help command to transport module identifier
                        if ( !aModuleIdentifier.isEmpty() )
                            pMenu->SetHelpCommand( nId, aModuleIdentifier );

                        ++nId;
                        FillMenu( nId, pNewPopupMenu, aModuleIdentifier, xIndexContainer, xDispatchProvider );
                    }
                    else
                        ++nId;
                }
                else
                {
                    pMenu->InsertSeparator();
                    ++nId;
                }
            }
        }
        catch ( const IndexOutOfBoundsException& )
        {
            break;
        }
    }
}

void MenuBarManager::MergeAddonMenus(
    Menu* pMenuBar,
    const MergeMenuInstructionContainer& aMergeInstructionContainer,
    const OUString& rModuleIdentifier )
{
    // set start value for the item ID for the new addon menu items
    sal_uInt16 nItemId = ADDONMENU_MERGE_ITEMID_START;

    const sal_uInt32 nCount = aMergeInstructionContainer.size();
    for ( sal_uInt32 i = 0; i < nCount; i++ )
    {
        const MergeMenuInstruction& rMergeInstruction = aMergeInstructionContainer[i];

        if ( MenuBarMerger::IsCorrectContext( rMergeInstruction.aMergeContext, rModuleIdentifier ))
        {
            ::std::vector< OUString > aMergePath;

            // retrieve the merge path from the merge point string
            MenuBarMerger::RetrieveReferencePath( rMergeInstruction.aMergePoint, aMergePath );

            // convert the sequence/sequence property value to a more convenient vector<>
            AddonMenuContainer aMergeMenuItems;
            MenuBarMerger::GetSubMenu( rMergeInstruction.aMergeMenu, aMergeMenuItems );

            // try to find the reference point for our merge operation
            Menu* pMenu = pMenuBar;
            ReferencePathInfo aResult = MenuBarMerger::FindReferencePath( aMergePath, pMenu );

            if ( aResult.eResult == RP_OK )
            {
                // normal merge operation
                MenuBarMerger::ProcessMergeOperation( aResult.pPopupMenu,
                                                      aResult.nPos,
                                                      nItemId,
                                                      rMergeInstruction.aMergeCommand,
                                                      rMergeInstruction.aMergeCommandParameter,
                                                      rModuleIdentifier,
                                                      aMergeMenuItems );
            }
            else
            {
                // fallback
                MenuBarMerger::ProcessFallbackOperation( aResult,
                                                         nItemId,
                                                         rMergeInstruction.aMergeCommand,
                                                         rMergeInstruction.aMergeFallback,
                                                         aMergePath,
                                                         rModuleIdentifier,
                                                         aMergeMenuItems );
            }
        }
    }
}

void MenuBarManager::SetItemContainer( const Reference< XIndexAccess >& rItemContainer )
{
    SolarMutexGuard aSolarMutexGuard;

    Reference< XFrame > xFrame = m_xFrame;

    if ( !m_bModuleIdentified )
    {
        m_bModuleIdentified = true;
        Reference< XModuleManager2 > xModuleManager = ModuleManager::create( m_xContext );

        try
        {
            m_aModuleIdentifier = xModuleManager->identify( xFrame );
        }
        catch( const Exception& )
        {
        }
    }

    // Clear MenuBarManager structures
    {
        // Check active state as we cannot change our VCL menu during activation by the user
        if ( m_bActive )
        {
            m_xDeferedItemContainer = rItemContainer;
            return;
        }

        RemoveListener();
        std::vector< MenuItemHandler* >::iterator p;
        for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
        {
            MenuItemHandler* pItemHandler = *p;
            pItemHandler->xMenuItemDispatch.clear();
            pItemHandler->xSubMenuManager.clear();
            delete pItemHandler;
        }
        m_aMenuItemHandlerVector.clear();

        // Remove top-level parts
        m_pVCLMenu->Clear();

        sal_uInt16          nId = 1;

        // Fill menu bar with container contents
        FillMenuWithConfiguration( nId, (Menu *)m_pVCLMenu, m_aModuleIdentifier, rItemContainer, m_xURLTransformer );

        // Refill menu manager again
        Reference< XDispatchProvider > xDispatchProvider;
        FillMenuManager( m_pVCLMenu, xFrame, xDispatchProvider, m_aModuleIdentifier, false, true );

        // add itself as frame action listener
        m_xFrame->addFrameActionListener( Reference< XFrameActionListener >( static_cast< OWeakObject* >( this ), UNO_QUERY ));
    }
}

void MenuBarManager::GetPopupController( PopupControllerCache& rPopupController )
{

    SolarMutexGuard aSolarMutexGuard;

    std::vector< MenuItemHandler* >::iterator p;
    for ( p = m_aMenuItemHandlerVector.begin(); p != m_aMenuItemHandlerVector.end(); ++p )
    {
        MenuItemHandler* pItemHandler = *p;
        if ( pItemHandler->xPopupMenuController.is() )
        {
            Reference< XDispatchProvider > xDispatchProvider( pItemHandler->xPopupMenuController, UNO_QUERY );

            PopupControllerEntry aPopupControllerEntry;
            aPopupControllerEntry.m_xDispatchProvider = xDispatchProvider;

            // Just use the main part of the URL for popup menu controllers
            sal_Int32     nQueryPart( 0 );
            sal_Int32     nSchemePart( 0 );
            OUString aMainURL( "vnd.sun.star.popup:" );
            OUString aMenuURL( pItemHandler->aMenuItemURL );

            nSchemePart = aMenuURL.indexOf( ':' );
            if (( nSchemePart > 0 ) &&
                ( aMenuURL.getLength() > ( nSchemePart+1 )))
            {
                nQueryPart  = aMenuURL.indexOf( '?', nSchemePart );
                if ( nQueryPart > 0 )
                    aMainURL += aMenuURL.copy( nSchemePart, nQueryPart-nSchemePart );
                else if ( nQueryPart == -1 )
                    aMainURL += aMenuURL.copy( nSchemePart+1 );

                rPopupController.insert( PopupControllerCache::value_type(
                                           aMainURL, aPopupControllerEntry ));
            }
        }
        if ( pItemHandler->xSubMenuManager.is() )
        {
            MenuBarManager* pMenuBarManager = (MenuBarManager*)(pItemHandler->xSubMenuManager.get());
            if ( pMenuBarManager )
                pMenuBarManager->GetPopupController( rPopupController );
        }
    }
}

void MenuBarManager::AddMenu(MenuBarManager* pSubMenuManager,const OUString& _sItemCommand,sal_uInt16 _nItemId)
{
    Reference< XStatusListener > xSubMenuManager( static_cast< OWeakObject *>( pSubMenuManager ), UNO_QUERY );
    m_xFrame->addFrameActionListener( Reference< XFrameActionListener >( xSubMenuManager, UNO_QUERY ));

    // store menu item command as we later have to know which menu is active (see Activate handler)
    pSubMenuManager->m_aMenuItemCommand = _sItemCommand;
    Reference< XDispatch > xDispatch;
    MenuItemHandler* pMenuItemHandler = new MenuItemHandler(
                                                _nItemId,
                                                xSubMenuManager,
                                                xDispatch );
    pMenuItemHandler->aMenuItemURL = _sItemCommand;
    m_aMenuItemHandlerVector.push_back( pMenuItemHandler );
}

sal_uInt16 MenuBarManager::FillItemCommand(OUString& _rItemCommand, Menu* _pMenu,sal_uInt16 _nIndex) const
{
    sal_uInt16 nItemId = _pMenu->GetItemId( _nIndex );

    _rItemCommand = _pMenu->GetItemCommand( nItemId );
    if ( _rItemCommand.isEmpty() )
    {
        _rItemCommand = "slot:" + OUString::number( nItemId );
        _pMenu->SetItemCommand( nItemId, _rItemCommand );
    }
    return nItemId;
}
void MenuBarManager::Init(const Reference< XFrame >& rFrame,AddonMenu* pAddonMenu,bool bDelete,bool bDeleteChildren,bool _bHandlePopUp)
{
    m_bActive           = false;
    m_bDeleteMenu       = bDelete;
    m_bDeleteChildren   = bDeleteChildren;
    m_pVCLMenu          = pAddonMenu;
    m_xFrame            = rFrame;
    m_bInitialized      = false;
    m_bIsBookmarkMenu   = true;
    m_bShowMenuImages   = true;

    OUString aModuleIdentifier;
    m_xPopupMenuControllerFactory = frame::thePopupMenuControllerFactory::get(
        ::comphelper::getProcessComponentContext());

    Reference< XStatusListener > xStatusListener;
    Reference< XDispatch > xDispatch;
    sal_uInt16 nItemCount = pAddonMenu->GetItemCount();
    OUString aItemCommand;
    m_aMenuItemHandlerVector.reserve(nItemCount);
    for ( sal_uInt16 i = 0; i < nItemCount; i++ )
    {
        sal_uInt16 nItemId = FillItemCommand(aItemCommand,pAddonMenu, i );

        PopupMenu* pPopupMenu = pAddonMenu->GetPopupMenu( nItemId );
        if ( pPopupMenu )
        {
            Reference< XDispatchProvider > xDispatchProvider;
            MenuBarManager* pSubMenuManager = new MenuBarManager( m_xContext, rFrame, m_xURLTransformer,xDispatchProvider, aModuleIdentifier, pPopupMenu, _bHandlePopUp ? sal_False : bDeleteChildren, _bHandlePopUp ? sal_False : bDeleteChildren );

            Reference< XStatusListener > xSubMenuManager( static_cast< OWeakObject *>( pSubMenuManager ), UNO_QUERY );

            // store menu item command as we later have to know which menu is active (see Acivate handler)
            pSubMenuManager->m_aMenuItemCommand = aItemCommand;

            MenuItemHandler* pMenuItemHandler = new MenuItemHandler(
                                                        nItemId,
                                                        xSubMenuManager,
                                                        xDispatch );
            m_aMenuItemHandlerVector.push_back( pMenuItemHandler );
        }
        else
        {
            if ( pAddonMenu->GetItemType( i ) != MENUITEM_SEPARATOR )
            {
                MenuConfiguration::Attributes* pAddonAttributes = (MenuConfiguration::Attributes *)(pAddonMenu->GetUserValue( nItemId ));
                MenuItemHandler* pMenuItemHandler = new MenuItemHandler( nItemId, xStatusListener, xDispatch );

                if ( pAddonAttributes )
                {
                    // read additional attributes from attributes struct and AddonMenu implementation will delete all attributes itself!!
                    pMenuItemHandler->aTargetFrame = pAddonAttributes->aTargetFrame;
                }

                pMenuItemHandler->aMenuItemURL = aItemCommand;
                if ( _bHandlePopUp )
                {
                    // Check if we have to create a popup menu for a uno based popup menu controller.
                    // We have to set an empty popup menu into our menu structure so the controller also
                    // works with inplace OLE.
                    if ( m_xPopupMenuControllerFactory.is() &&
                        m_xPopupMenuControllerFactory->hasController( aItemCommand, OUString() ))
                    {
                        VCLXPopupMenu* pVCLXPopupMenu = new VCLXPopupMenu;
                        PopupMenu* pCtlPopupMenu = (PopupMenu *)pVCLXPopupMenu->GetMenu();
                        pAddonMenu->SetPopupMenu( pMenuItemHandler->nItemId, pCtlPopupMenu );
                        pMenuItemHandler->xPopupMenu = Reference< com::sun::star::awt::XPopupMenu >( (OWeakObject *)pVCLXPopupMenu, UNO_QUERY );

                    }
                }
                m_aMenuItemHandlerVector.push_back( pMenuItemHandler );
            }
        }
    }

    SetHdl();
}

void MenuBarManager::SetHdl()
{
    m_pVCLMenu->SetHighlightHdl( LINK( this, MenuBarManager, Highlight ));
    m_pVCLMenu->SetActivateHdl( LINK( this, MenuBarManager, Activate ));
    m_pVCLMenu->SetDeactivateHdl( LINK( this, MenuBarManager, Deactivate ));
    m_pVCLMenu->SetSelectHdl( LINK( this, MenuBarManager, Select ));

    if ( !m_xURLTransformer.is() && m_xContext.is() )
        m_xURLTransformer.set( URLTransformer::create( m_xContext) );
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
