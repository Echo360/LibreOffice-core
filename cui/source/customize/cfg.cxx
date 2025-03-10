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

#include "sal/config.h"

#include <cassert>
#include <stdlib.h>
#include <time.h>

#include <vcl/help.hxx>
#include <vcl/msgbox.hxx>
#include <vcl/decoview.hxx>
#include <vcl/toolbox.hxx>
#include <vcl/scrbar.hxx>
#include <vcl/virdev.hxx>
#include <vcl/settings.hxx>

#include <sfx2/app.hxx>
#include <sfx2/sfxdlg.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/viewsh.hxx>
#include <sfx2/msg.hxx>
#include <sfx2/msgpool.hxx>
#include <sfx2/mnumgr.hxx>
#include <sfx2/minfitem.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/request.hxx>
#include <sfx2/filedlghelper.hxx>
#include <svl/stritem.hxx>
#include <svtools/miscopt.hxx>
#include <svtools/svlbitm.hxx>
#include "svtools/treelistentry.hxx"
#include "svtools/viewdataentry.hxx"
#include <tools/diagnose_ex.h>
#include <toolkit/helper/vclunohelper.hxx>

#include <algorithm>
#include <cuires.hrc>
#include "cfg.hrc"
#include "helpid.hrc"

#include "acccfg.hxx"
#include "cfg.hxx"
#include "eventdlg.hxx"
#include <dialmgr.hxx>

#include <comphelper/documentinfo.hxx>
#include <comphelper/processfactory.hxx>
#include <unotools/configmgr.hxx>
#include <com/sun/star/embed/ElementModes.hpp>
#include <com/sun/star/embed/FileSystemStorageFactory.hpp>
#include <com/sun/star/frame/XFramesSupplier.hpp>
#include <com/sun/star/frame/XFrames.hpp>
#include <com/sun/star/frame/XLayoutManager.hpp>
#include <com/sun/star/frame/FrameSearchFlag.hpp>
#include <com/sun/star/frame/ModuleManager.hpp>
#include <com/sun/star/frame/XController.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/theUICommandDescription.hpp>
#include <com/sun/star/graphic/GraphicProvider.hpp>
#include <com/sun/star/ui/ItemType.hpp>
#include <com/sun/star/ui/ItemStyle.hpp>
#include <com/sun/star/ui/ImageManager.hpp>
#include <com/sun/star/ui/theModuleUIConfigurationManagerSupplier.hpp>
#include <com/sun/star/ui/XUIConfiguration.hpp>
#include <com/sun/star/ui/XUIConfigurationListener.hpp>
#include <com/sun/star/ui/XUIConfigurationManagerSupplier.hpp>
#include <com/sun/star/ui/XUIConfigurationPersistence.hpp>
#include <com/sun/star/ui/XUIConfigurationStorage.hpp>
#include <com/sun/star/ui/XModuleUIConfigurationManager.hpp>
#include <com/sun/star/ui/XUIElement.hpp>
#include <com/sun/star/ui/UIElementType.hpp>
#include <com/sun/star/ui/ImageType.hpp>
#include <com/sun/star/ui/theWindowStateConfiguration.hpp>
#include <com/sun/star/ui/dialogs/ExtendedFilePickerElementIds.hpp>
#include "com/sun/star/ui/dialogs/TemplateDescription.hpp"
#include <com/sun/star/ui/dialogs/XFilePickerControlAccess.hpp>
#include <com/sun/star/util/thePathSettings.hpp>

#include "dlgname.hxx"
#include <boost/scoped_ptr.hpp>

#define PRTSTR(x) OUStringToOString(x, RTL_TEXTENCODING_ASCII_US).pData->buffer

#define ENTRY_HEIGHT 16

static const char ITEM_DESCRIPTOR_COMMANDURL[]  = "CommandURL";
static const char ITEM_DESCRIPTOR_CONTAINER[]   = "ItemDescriptorContainer";
static const char ITEM_DESCRIPTOR_LABEL[]       = "Label";
static const char ITEM_DESCRIPTOR_TYPE[]        = "Type";
static const char ITEM_DESCRIPTOR_STYLE[]       = "Style";
static const char ITEM_DESCRIPTOR_ISVISIBLE[]   = "IsVisible";
static const char ITEM_DESCRIPTOR_RESOURCEURL[] = "ResourceURL";
static const char ITEM_DESCRIPTOR_UINAME[]      = "UIName";

static const char ITEM_MENUBAR_URL[] = "private:resource/menubar/menubar";
static const char ITEM_TOOLBAR_URL[] = "private:resource/toolbar/";

static const char CUSTOM_TOOLBAR_STR[] = "custom_toolbar_";
static const char CUSTOM_MENU_STR[] = "vnd.openoffice.org:CustomMenu";

static const char aSeparatorStr[] = "----------------------------------";
static const char aMenuSeparatorStr[] = " | ";

namespace uno = com::sun::star::uno;
namespace frame = com::sun::star::frame;
namespace lang = com::sun::star::lang;
namespace container = com::sun::star::container;
namespace beans = com::sun::star::beans;
namespace graphic = com::sun::star::graphic;

#if OSL_DEBUG_LEVEL > 1

void printPropertySet(
    const OUString& prefix,
    const uno::Reference< beans::XPropertySet >& xPropSet )
{
    uno::Reference< beans::XPropertySetInfo > xPropSetInfo =
        xPropSet->getPropertySetInfo();

    uno::Sequence< beans::Property > aPropDetails =
        xPropSetInfo->getProperties();

    OSL_TRACE("printPropertySet: %d properties", aPropDetails.getLength());

    for ( sal_Int32 i = 0; i < aPropDetails.getLength(); ++i )
    {
        OUString tmp;
        sal_Int32 ival;

        uno::Any a = xPropSet->getPropertyValue( aPropDetails[i].Name );

        if ( a >>= tmp )
        {
            OSL_TRACE("%s: Got property: %s = %s",
                PRTSTR(prefix), PRTSTR(aPropDetails[i].Name), PRTSTR(tmp));
        }
        else if ( ( a >>= ival ) )
        {
            OSL_TRACE("%s: Got property: %s = %d",
                PRTSTR(prefix), PRTSTR(aPropDetails[i].Name), PRTSTR(tmp));
        }
        else
        {
            OSL_TRACE("%s: Got property: %s of type %s",
                PRTSTR(prefix), PRTSTR(aPropDetails[i].Name), PRTSTR(a.getValueTypeName()));
        }
    }
}

void printProperties(
    const OUString& prefix,
    const uno::Sequence< beans::PropertyValue >& aProp )
{
    for ( sal_Int32 i = 0; i < aProp.getLength(); ++i )
    {
        OUString tmp;

        aProp[i].Value >>= tmp;

        OSL_TRACE("%s: Got property: %s = %s",
            PRTSTR(prefix), PRTSTR(aProp[i].Name), PRTSTR(tmp));
    }
}

void printEntries(SvxEntries* entries)
{
    SvxEntries::const_iterator iter = entries->begin();

    for ( ; iter != entries->end(); ++iter )
    {
        SvxConfigEntry* entry = *iter;

        OSL_TRACE("printEntries: %s", PRTSTR(entry->GetName()));
    }
}

#endif

OUString
stripHotKey( const OUString& str )
{
    sal_Int32 index = str.indexOf( '~' );
    if ( index == -1 )
    {
        return str;
    }
    else
    {
        return str.replaceAt( index, 1, OUString() );
    }
}

OUString replaceSaveInName(
    const OUString& rMessage,
    const OUString& rSaveInName )
{
    OUString name;
    OUString placeholder("%SAVE IN SELECTION%" );

    sal_Int32 pos = rMessage.indexOf( placeholder );

    if ( pos != -1 )
    {
        name = rMessage.replaceAt(
            pos, placeholder.getLength(), rSaveInName );
    }

    return name;
}

OUString
replaceSixteen( const OUString& str, sal_Int32 nReplacement )
{
    OUString result( str );
    OUString sixteen = OUString::number( 16 );
    OUString expected = OUString::number( nReplacement );

    sal_Int32 len = sixteen.getLength();
    sal_Int32 index = result.indexOf( sixteen );

    while ( index != -1 )
    {
        result = result.replaceAt( index, len, expected );
        index = result.indexOf( sixteen, index );
    }

    return result;
}

OUString
generateCustomName(
    const OUString& prefix,
    SvxEntries* entries,
    sal_Int32 suffix = 1 )
{
    // find and replace the %n placeholder in the prefix string
    OUString name;
    OUString placeholder("%n" );

    sal_Int32 pos = prefix.indexOf( placeholder );

    if ( pos != -1 )
    {
        name = prefix.replaceAt(
            pos, placeholder.getLength(), OUString::number( suffix ) );
    }
    else
    {
        // no placeholder found so just append the suffix
        name = prefix + OUString::number( suffix );
    }

    if (!entries)
        return name;

    // now check is there is an already existing entry with this name
    SvxEntries::const_iterator iter = entries->begin();

    SvxConfigEntry* pEntry;
    while ( iter != entries->end() )
    {
        pEntry = *iter;

        if ( name.equals( pEntry->GetName() ) )
        {
            break;
        }
        ++iter;
    }

    if ( iter != entries->end() )
    {
        // name already exists so try the next number up
        return generateCustomName( prefix, entries, ++suffix );
    }

    return name;
}

sal_uInt32 generateRandomValue()
{
    srand( unsigned( time( NULL ) ));
    return sal_uInt32( rand() );
}

OUString
generateCustomURL(
    SvxEntries* entries )
{
    OUString url = OUString(ITEM_TOOLBAR_URL );
    url += OUString(CUSTOM_TOOLBAR_STR );

    // use a random number to minimize possible clash with existing custom toolbars
    url += OUString::number( generateRandomValue(), 16 );

    // now check is there is an already existing entry with this url
    SvxEntries::const_iterator iter = entries->begin();

    SvxConfigEntry* pEntry;
    while ( iter != entries->end() )
    {
        pEntry = *iter;

        if ( url.equals( pEntry->GetCommand() ) )
        {
            break;
        }
        ++iter;
    }

    if ( iter != entries->end() )
    {
        // url already exists so try the next number up
        return generateCustomURL( entries );
    }

    return url;
}

OUString
generateCustomMenuURL(
    SvxEntries* entries,
    sal_Int32 suffix = 1 )
{
    OUString url(CUSTOM_MENU_STR );
    url += OUString::number( suffix );

    // now check is there is an already existing entry with this url
    SvxEntries::const_iterator iter = entries->begin();

    SvxConfigEntry* pEntry;
    while ( iter != entries->end() )
    {
        pEntry = *iter;

        if ( url.equals( pEntry->GetCommand() ) )
        {
            break;
        }
        ++iter;
    }

    if ( iter != entries->end() )
    {
        // url already exists so try the next number up
        return generateCustomMenuURL( entries, ++suffix );
    }

    return url;
}

static sal_Int16 theImageType =
    css::ui::ImageType::COLOR_NORMAL |
    css::ui::ImageType::SIZE_DEFAULT;

void InitImageType()
{
    theImageType =
        css::ui::ImageType::COLOR_NORMAL |
        css::ui::ImageType::SIZE_DEFAULT;

    if ( SvtMiscOptions().AreCurrentSymbolsLarge() )
    {
        theImageType |= css::ui::ImageType::SIZE_LARGE;
    }
}

sal_Int16 GetImageType()
{
    return theImageType;
}

void RemoveEntry( SvxEntries* pEntries, SvxConfigEntry* pChildEntry )
{
    SvxEntries::iterator iter = pEntries->begin();

    while ( iter != pEntries->end() )
    {
        if ( pChildEntry == *iter )
        {
            pEntries->erase( iter );
            break;
        }
        ++iter;
    }
}

bool
SvxConfigPage::CanConfig( const OUString& aModuleId )
{
    OSL_TRACE("SupportsDocumentConfig: %s", PRTSTR(aModuleId));

    if  ( aModuleId == "com.sun.star.script.BasicIDE" || aModuleId == "com.sun.star.frame.Bibliography" )
    {
        return false;
    }
    return true;
}

OUString GetModuleName( const OUString& aModuleId )
{
    if ( aModuleId == "com.sun.star.text.TextDocument" ||
         aModuleId == "com.sun.star.text.GlobalDocument" )
        return OUString("Writer");
    else if ( aModuleId == "com.sun.star.text.WebDocument" )
        return OUString("Writer/Web");
    else if ( aModuleId == "com.sun.star.drawing.DrawingDocument" )
        return OUString("Draw");
    else if ( aModuleId == "com.sun.star.presentation.PresentationDocument" )
        return OUString("Impress");
    else if ( aModuleId == "com.sun.star.sheet.SpreadsheetDocument" )
        return OUString("Calc");
    else if ( aModuleId == "com.sun.star.script.BasicIDE" )
        return OUString("Basic");
    else if ( aModuleId == "com.sun.star.formula.FormulaProperties" )
        return OUString("Math");
    else if ( aModuleId == "com.sun.star.sdb.RelationDesign" )
        return OUString("Relation Design");
    else if ( aModuleId == "com.sun.star.sdb.QueryDesign" )
        return OUString("Query Design");
    else if ( aModuleId == "com.sun.star.sdb.TableDesign" )
        return OUString("Table Design");
    else if ( aModuleId == "com.sun.star.sdb.DataSourceBrowser" )
        return OUString("Data Source Browser" );
    else if ( aModuleId == "com.sun.star.sdb.DatabaseDocument" )
        return OUString("Database" );

    return OUString();
}

OUString GetUIModuleName( const OUString& aModuleId, const uno::Reference< css::frame::XModuleManager2 >& rModuleManager )
{
    assert(rModuleManager.is());

    OUString aModuleUIName;

    try
    {
        uno::Any a = rModuleManager->getByName( aModuleId );
        uno::Sequence< beans::PropertyValue > aSeq;

        if ( a >>= aSeq )
        {
            for ( sal_Int32 i = 0; i < aSeq.getLength(); ++i )
            {
                if ( aSeq[i].Name == "ooSetupFactoryUIName" )
                {
                    aSeq[i].Value >>= aModuleUIName;
                    break;
                }
            }
        }
    }
    catch ( uno::RuntimeException& )
    {
        throw;
    }
    catch ( uno::Exception& )
    {
    }

    if ( aModuleUIName.isEmpty() )
        aModuleUIName = GetModuleName( aModuleId );

    return aModuleUIName;
}

bool GetMenuItemData(
    const uno::Reference< container::XIndexAccess >& rItemContainer,
    sal_Int32 nIndex,
    OUString& rCommandURL,
    OUString& rLabel,
    sal_uInt16& rType,
    uno::Reference< container::XIndexAccess >& rSubMenu )
{
    try
    {
        uno::Sequence< beans::PropertyValue > aProp;
        if ( rItemContainer->getByIndex( nIndex ) >>= aProp )
        {
            for ( sal_Int32 i = 0; i < aProp.getLength(); ++i )
            {
                if ( aProp[i].Name.equalsAscii( ITEM_DESCRIPTOR_COMMANDURL ))
                {
                    aProp[i].Value >>= rCommandURL;
                }
                else if ( aProp[i].Name.equalsAscii( ITEM_DESCRIPTOR_CONTAINER ))
                {
                    aProp[i].Value >>= rSubMenu;
                }
                else if ( aProp[i].Name.equalsAscii( ITEM_DESCRIPTOR_LABEL ))
                {
                    aProp[i].Value >>= rLabel;
                }
                else if ( aProp[i].Name.equalsAscii( ITEM_DESCRIPTOR_TYPE ))
                {
                    aProp[i].Value >>= rType;
                }
            }

            return true;
        }
    }
    catch ( ::com::sun::star::lang::IndexOutOfBoundsException& )
    {
    }

    return false;
}

bool GetToolbarItemData(
    const uno::Reference< container::XIndexAccess >& rItemContainer,
    sal_Int32 nIndex,
    OUString& rCommandURL,
    OUString& rLabel,
    sal_uInt16& rType,
    bool& rIsVisible,
    sal_Int32& rStyle,
    uno::Reference< container::XIndexAccess >& rSubMenu )
{
    try
    {
        uno::Sequence< beans::PropertyValue > aProp;
        if ( rItemContainer->getByIndex( nIndex ) >>= aProp )
        {
            for ( sal_Int32 i = 0; i < aProp.getLength(); ++i )
            {
                if ( aProp[i].Name.equalsAscii( ITEM_DESCRIPTOR_COMMANDURL ))
                {
                    aProp[i].Value >>= rCommandURL;
                }
                if ( aProp[i].Name.equalsAscii( ITEM_DESCRIPTOR_STYLE ))
                {
                    aProp[i].Value >>= rStyle;
                }
                else if (aProp[i].Name.equalsAscii(ITEM_DESCRIPTOR_CONTAINER))
                {
                    aProp[i].Value >>= rSubMenu;
                }
                else if ( aProp[i].Name.equalsAscii( ITEM_DESCRIPTOR_LABEL ))
                {
                    aProp[i].Value >>= rLabel;
                }
                else if ( aProp[i].Name.equalsAscii( ITEM_DESCRIPTOR_TYPE ))
                {
                    aProp[i].Value >>= rType;
                }
                else if (aProp[i].Name.equalsAscii(ITEM_DESCRIPTOR_ISVISIBLE))
                {
                    aProp[i].Value >>= rIsVisible;
                }
            }

            return true;
        }
    }
    catch ( ::com::sun::star::lang::IndexOutOfBoundsException& )
    {
    }

    return false;
}

uno::Sequence< beans::PropertyValue >
ConvertSvxConfigEntry(
    const uno::Reference< container::XNameAccess >& xCommandToLabelMap,
    const SvxConfigEntry* pEntry )
{
    static const OUString aDescriptorCommandURL (
        ITEM_DESCRIPTOR_COMMANDURL  );

    static const OUString aDescriptorType(
            ITEM_DESCRIPTOR_TYPE  );

    static const OUString aDescriptorLabel(
            ITEM_DESCRIPTOR_LABEL  );

    uno::Sequence< beans::PropertyValue > aPropSeq( 3 );

    aPropSeq[0].Name = aDescriptorCommandURL;
    aPropSeq[0].Value <<= OUString( pEntry->GetCommand() );

    aPropSeq[1].Name = aDescriptorType;
    aPropSeq[1].Value <<= css::ui::ItemType::DEFAULT;

    // If the name has not been changed and the name is the same as
    // in the default command to label map then the label can be stored
    // as an empty string.
    // It will be initialised again later using the command to label map.
    aPropSeq[2].Name = aDescriptorLabel;
    if ( !pEntry->HasChangedName() && !pEntry->GetCommand().isEmpty() )
    {
        bool isDefaultName = false;
        try
        {
            uno::Any a( xCommandToLabelMap->getByName( pEntry->GetCommand() ) );
            uno::Sequence< beans::PropertyValue > tmpPropSeq;
            if ( a >>= tmpPropSeq )
            {
                for ( sal_Int32 i = 0; i < tmpPropSeq.getLength(); ++i )
                {
                    if ( tmpPropSeq[i].Name.equals( aDescriptorLabel ) )
                    {
                        OUString tmpLabel;
                        tmpPropSeq[i].Value >>= tmpLabel;

                        if ( tmpLabel.equals( pEntry->GetName() ) )
                        {
                            isDefaultName = true;
                        }

                        break;
                    }
                }
            }
        }
        catch ( container::NoSuchElementException& )
        {
            // isDefaultName is left as FALSE
        }

        if ( isDefaultName )
        {
            aPropSeq[2].Value <<= OUString();
        }
        else
        {
            aPropSeq[2].Value <<= OUString( pEntry->GetName() );
        }
    }
    else
    {
        aPropSeq[2].Value <<= OUString( pEntry->GetName() );
    }

    return aPropSeq;
}

uno::Sequence< beans::PropertyValue >
ConvertToolbarEntry(
    const uno::Reference< container::XNameAccess >& xCommandToLabelMap,
    const SvxConfigEntry* pEntry )
{
    static const OUString aDescriptorCommandURL (
        ITEM_DESCRIPTOR_COMMANDURL  );

    static const OUString aDescriptorType(
            ITEM_DESCRIPTOR_TYPE  );

    static const OUString aDescriptorLabel(
            ITEM_DESCRIPTOR_LABEL  );

    static const OUString aIsVisible(
            ITEM_DESCRIPTOR_ISVISIBLE  );

    uno::Sequence< beans::PropertyValue > aPropSeq( 4 );

    aPropSeq[0].Name = aDescriptorCommandURL;
    aPropSeq[0].Value <<= OUString( pEntry->GetCommand() );

    aPropSeq[1].Name = aDescriptorType;
    aPropSeq[1].Value <<= css::ui::ItemType::DEFAULT;

    // If the name has not been changed and the name is the same as
    // in the default command to label map then the label can be stored
    // as an empty string.
    // It will be initialised again later using the command to label map.
    aPropSeq[2].Name = aDescriptorLabel;
    if ( !pEntry->HasChangedName() && !pEntry->GetCommand().isEmpty() )
    {
        bool isDefaultName = false;
        try
        {
            uno::Any a( xCommandToLabelMap->getByName( pEntry->GetCommand() ) );
            uno::Sequence< beans::PropertyValue > tmpPropSeq;
            if ( a >>= tmpPropSeq )
            {
                for ( sal_Int32 i = 0; i < tmpPropSeq.getLength(); ++i )
                {
                    if ( tmpPropSeq[i].Name.equals( aDescriptorLabel ) )
                    {
                        OUString tmpLabel;
                        tmpPropSeq[i].Value >>= tmpLabel;

                        if ( tmpLabel.equals( pEntry->GetName() ) )
                        {
                            isDefaultName = true;
                        }

                        break;
                    }
                }
            }
        }
        catch ( container::NoSuchElementException& )
        {
            // isDefaultName is left as FALSE
        }

        if ( isDefaultName )
        {
            aPropSeq[2].Value <<= OUString();
        }
        else
        {
            aPropSeq[2].Value <<= OUString( pEntry->GetName() );
        }
    }
    else
    {
        aPropSeq[2].Value <<= OUString( pEntry->GetName() );
    }

    aPropSeq[3].Name = aIsVisible;
    aPropSeq[3].Value <<= pEntry->IsVisible();

    return aPropSeq;
}

SfxTabPage *CreateSvxMenuConfigPage( Window *pParent, const SfxItemSet* rSet )
{
    return new SvxMenuConfigPage( pParent, *rSet );
}

SfxTabPage *CreateKeyboardConfigPage( Window *pParent, const SfxItemSet* rSet )
{
       return new SfxAcceleratorConfigPage( pParent, *rSet );
}

SfxTabPage *CreateSvxToolbarConfigPage( Window *pParent, const SfxItemSet* rSet )
{
    return new SvxToolbarConfigPage( pParent, *rSet );
}

SfxTabPage *CreateSvxEventConfigPage( Window *pParent, const SfxItemSet* rSet )
{
    return new SvxEventConfigPage( pParent, *rSet, SvxEventConfigPage::EarlyInit() );
}

namespace {

bool showKeyConfigTabPage( const css::uno::Reference< css::frame::XFrame >& xFrame )
{
    if (!xFrame.is())
    {
        return false;
    }
    OUString sModuleId(
        css::frame::ModuleManager::create(
            comphelper::getProcessComponentContext())
        ->identify(xFrame));
    return !sModuleId.isEmpty()
        && sModuleId != "com.sun.star.frame.StartModule";
}

}

/******************************************************************************
 *
 * SvxConfigDialog is the configuration dialog which is brought up from the
 * Tools menu. It includes tabs for customizing menus, toolbars, events and
 * key bindings.
 *
 *****************************************************************************/
SvxConfigDialog::SvxConfigDialog(Window * pParent, const SfxItemSet* pInSet)
    : SfxTabDialog(pParent, "CustomizeDialog",
        "cui/ui/customizedialog.ui", pInSet)
    , m_nMenusPageId(0)
    , m_nKeyboardPageId(0)
    , m_nToolbarsPageId(0)
    , m_nEventsPageId(0)
{
    InitImageType();

    m_nMenusPageId = AddTabPage("menus", CreateSvxMenuConfigPage, NULL);
    m_nKeyboardPageId = AddTabPage("keyboard", CreateKeyboardConfigPage, NULL);
    m_nToolbarsPageId = AddTabPage("toolbars", CreateSvxToolbarConfigPage, NULL);
    m_nEventsPageId = AddTabPage("events", CreateSvxEventConfigPage, NULL);

    const SfxPoolItem* pItem =
        pInSet->GetItem( pInSet->GetPool()->GetWhich( SID_CONFIG ) );

    if ( pItem )
    {
        OUString text = ((const SfxStringItem*)pItem)->GetValue();

        if (text.startsWith( ITEM_TOOLBAR_URL ) )
        {
            SetCurPageId(m_nToolbarsPageId);
        }
    }
}

void SvxConfigDialog::SetFrame(const ::com::sun::star::uno::Reference< ::com::sun::star::frame::XFrame >& xFrame)
{
    m_xFrame = xFrame;

    if (!showKeyConfigTabPage( xFrame ))
        RemoveTabPage(m_nKeyboardPageId);
}

void SvxConfigDialog::PageCreated( sal_uInt16 nId, SfxTabPage& rPage )
{
    if (nId == m_nMenusPageId || nId == m_nKeyboardPageId ||
        nId == m_nToolbarsPageId)
    {
        rPage.SetFrame(m_xFrame);
    }
    else if (nId == m_nEventsPageId)
    {
        dynamic_cast< SvxEventConfigPage& >( rPage ).LateInit( m_xFrame );
    }
}

/******************************************************************************
 *
 * The SaveInData class is used to hold data for entries in the Save In
 * ListBox controls in the menu and toolbar tabs
 *
 ******************************************************************************/

// Initialize static variable which holds default XImageManager
uno::Reference< css::ui::XImageManager>* SaveInData::xDefaultImgMgr = NULL;

SaveInData::SaveInData(
    const uno::Reference< css::ui::XUIConfigurationManager >& xCfgMgr,
    const uno::Reference< css::ui::XUIConfigurationManager >& xParentCfgMgr,
    const OUString& aModuleId,
    bool isDocConfig )
        :
            bModified( false ),
            bDocConfig( isDocConfig ),
            bReadOnly( false ),
            m_xCfgMgr( xCfgMgr ),
            m_xParentCfgMgr( xParentCfgMgr )
{
    m_aSeparatorSeq.realloc( 1 );
    m_aSeparatorSeq[0].Name  = OUString( ITEM_DESCRIPTOR_TYPE  );
    m_aSeparatorSeq[0].Value <<= css::ui::ItemType::SEPARATOR_LINE;

    if ( bDocConfig )
    {
        uno::Reference< css::ui::XUIConfigurationPersistence >
            xDocPersistence( GetConfigManager(), uno::UNO_QUERY );

        bReadOnly = xDocPersistence->isReadOnly();
    }

    uno::Reference<uno::XComponentContext> xContext = ::comphelper::getProcessComponentContext();

    uno::Reference< container::XNameAccess > xNameAccess(
        css::frame::theUICommandDescription::get(xContext) );

    xNameAccess->getByName( aModuleId ) >>= m_xCommandToLabelMap;

    if ( !m_xImgMgr.is() )
    {
        m_xImgMgr = uno::Reference< css::ui::XImageManager >(
            GetConfigManager()->getImageManager(), uno::UNO_QUERY );
    }

    if ( !IsDocConfig() )
    {
        // If this is not a document configuration then it is the settings
        // for the module (writer, calc, impress etc.) Use this as the default
        // XImageManager instance
        xDefaultImgMgr = &m_xImgMgr;
    }
    else
    {
        // If this is a document configuration then use the module image manager
        // as default.
        if ( m_xParentCfgMgr.is() )
        {
            m_xParentImgMgr = uno::Reference< css::ui::XImageManager >(
                m_xParentCfgMgr->getImageManager(), uno::UNO_QUERY );
            xDefaultImgMgr = &m_xParentImgMgr;
        }
    }
}

uno::Reference< graphic::XGraphic > GetGraphic(
    const uno::Reference< css::ui::XImageManager >& xImageManager,
    const OUString& rCommandURL )
{
    uno::Reference< graphic::XGraphic > result;

    if ( xImageManager.is() )
    {
        // TODO handle large graphics
        uno::Sequence< uno::Reference< graphic::XGraphic > > aGraphicSeq;

        uno::Sequence< OUString > aImageCmdSeq( 1 );
        aImageCmdSeq[0] = rCommandURL;

        try
        {
            aGraphicSeq =
                xImageManager->getImages( GetImageType(), aImageCmdSeq );

            if ( aGraphicSeq.getLength() > 0 )
            {
                result =  aGraphicSeq[0];
            }
        }
        catch ( uno::Exception& )
        {
            // will return empty XGraphic
        }
    }

    return result;
}

Image SaveInData::GetImage( const OUString& rCommandURL )
{
    Image aImage;

    uno::Reference< graphic::XGraphic > xGraphic =
        GetGraphic( m_xImgMgr, rCommandURL );

    if ( xGraphic.is() )
    {
        aImage = Image( xGraphic );
    }
    else if ( xDefaultImgMgr != NULL && (*xDefaultImgMgr).is() )
    {
        xGraphic = GetGraphic( (*xDefaultImgMgr), rCommandURL );

        if ( xGraphic.is() )
        {
            aImage = Image( xGraphic );
        }
    }

    return aImage;
}

bool SaveInData::PersistChanges(
    const uno::Reference< uno::XInterface >& xManager )
{
    bool result = true;

    try
    {
        if ( xManager.is() && !IsReadOnly() )
        {
            uno::Reference< css::ui::XUIConfigurationPersistence >
                xConfigPersistence( xManager, uno::UNO_QUERY );

            if ( xConfigPersistence->isModified() )
            {
                xConfigPersistence->store();
            }
        }
    }
    catch ( com::sun::star::io::IOException& )
    {
        result = false;
    }

    return result;
}

/******************************************************************************
 *
 * The MenuSaveInData class extends SaveInData and provides menu specific
 * load and store functionality.
 *
 ******************************************************************************/

// Initialize static variable which holds default Menu data
MenuSaveInData* MenuSaveInData::pDefaultData = NULL;

MenuSaveInData::MenuSaveInData(
    const uno::Reference< css::ui::XUIConfigurationManager >& cfgmgr,
    const uno::Reference< css::ui::XUIConfigurationManager >& xParentCfgMgr,
    const OUString& aModuleId,
    bool isDocConfig )
    :
        SaveInData( cfgmgr, xParentCfgMgr, aModuleId, isDocConfig ),
        m_aMenuResourceURL(
            ITEM_MENUBAR_URL  ),
        m_aDescriptorContainer(
            ITEM_DESCRIPTOR_CONTAINER  ),
        pRootEntry( 0 )
{
    try
    {
        OUString url( ITEM_MENUBAR_URL  );
        m_xMenuSettings = GetConfigManager()->getSettings( url, sal_False );
    }
    catch ( container::NoSuchElementException& )
    {
        // will use menu settings for the module
    }

    // If this is not a document configuration then it is the settings
    // for the module (writer, calc, impress etc.). These settings should
    // be set as the default to be used for SaveIn locations that do not
    // have custom settings
    if ( !IsDocConfig() )
    {
        SetDefaultData( this );
    }
}

MenuSaveInData::~MenuSaveInData()
{
    if ( pRootEntry != NULL )
    {
        delete pRootEntry;
    }
}

SvxEntries*
MenuSaveInData::GetEntries()
{
    if ( pRootEntry == NULL )
    {
        pRootEntry = new SvxConfigEntry(
            OUString("MainMenus"),
            OUString(), true);

        if ( m_xMenuSettings.is() )
        {
            LoadSubMenus( m_xMenuSettings, OUString(), pRootEntry );
        }
        else if ( GetDefaultData() != NULL )
        {
            // If the doc has no config settings use module config settings
            LoadSubMenus( GetDefaultData()->m_xMenuSettings, OUString(), pRootEntry );
        }
    }

    return pRootEntry->GetEntries();
}

void
MenuSaveInData::SetEntries( SvxEntries* pNewEntries )
{
    // delete old menu hierarchy first
    delete pRootEntry->GetEntries();

    // now set new menu hierarchy
    pRootEntry->SetEntries( pNewEntries );
}

bool MenuSaveInData::LoadSubMenus(
    const uno::Reference< container::XIndexAccess >& xMenuSettings,
    const OUString& rBaseTitle,
    SvxConfigEntry* pParentData )
{
    SvxEntries* pEntries = pParentData->GetEntries();

    // Don't access non existing menu configuration!
    if ( !xMenuSettings.is() )
        return true;

    for ( sal_Int32 nIndex = 0; nIndex < xMenuSettings->getCount(); ++nIndex )
    {
        uno::Reference< container::XIndexAccess >   xSubMenu;
        OUString                aCommandURL;
        OUString                aLabel;
        bool                    bIsUserDefined = true;

        sal_uInt16 nType( css::ui::ItemType::DEFAULT );

        bool bItem = GetMenuItemData( xMenuSettings, nIndex,
            aCommandURL, aLabel, nType, xSubMenu );

        if ( bItem )
        {
            if ( nType == css::ui::ItemType::DEFAULT )
            {
                uno::Any a;
                try
                {
                    a = m_xCommandToLabelMap->getByName( aCommandURL );
                    bIsUserDefined = false;
                }
                catch ( container::NoSuchElementException& )
                {
                    bIsUserDefined = true;
                }

                // If custom label not set retrieve it from the command
                // to info service
                if ( aLabel.equals( OUString() ) )
                {
                    uno::Sequence< beans::PropertyValue > aPropSeq;
                    if ( a >>= aPropSeq )
                    {
                        for ( sal_Int32 i = 0; i < aPropSeq.getLength(); ++i )
                        {
                            if ( aPropSeq[i].Name.equalsAscii( ITEM_DESCRIPTOR_LABEL ) )
                            {
                                aPropSeq[i].Value >>= aLabel;
                                break;
                            }
                        }
                    }
                }

                if ( xSubMenu.is() )
                {
                    // popup menu
                    SvxConfigEntry* pEntry = new SvxConfigEntry(
                        aLabel, aCommandURL, true );

                    pEntry->SetUserDefined( bIsUserDefined );

                    pEntries->push_back( pEntry );

                    OUString subMenuTitle( rBaseTitle );

                    if ( !subMenuTitle.isEmpty() )
                    {
                        subMenuTitle += OUString( aMenuSeparatorStr);
                    }
                    else
                    {
                        pEntry->SetMain( true );
                    }

                    subMenuTitle += stripHotKey( aLabel );

                    LoadSubMenus( xSubMenu, subMenuTitle, pEntry );
                }
                else
                {
                    SvxConfigEntry* pEntry = new SvxConfigEntry(
                        aLabel, aCommandURL, false );
                    pEntry->SetUserDefined( bIsUserDefined );
                    pEntries->push_back( pEntry );
                }
            }
            else
            {
                SvxConfigEntry* pEntry = new SvxConfigEntry;
                pEntry->SetUserDefined( bIsUserDefined );
                pEntries->push_back( pEntry );
            }
        }
    }
    return true;
}

bool MenuSaveInData::Apply()
{
    bool result = false;

    if ( IsModified() )
    {
        // Apply new menu bar structure to our settings container
        m_xMenuSettings = uno::Reference< container::XIndexAccess >(
            GetConfigManager()->createSettings(), uno::UNO_QUERY );

        uno::Reference< container::XIndexContainer > xIndexContainer (
            m_xMenuSettings, uno::UNO_QUERY );

        uno::Reference< lang::XSingleComponentFactory > xFactory (
            m_xMenuSettings, uno::UNO_QUERY );

        Apply( pRootEntry, xIndexContainer, xFactory, NULL );

        try
        {
            if ( GetConfigManager()->hasSettings( m_aMenuResourceURL ) )
            {
                GetConfigManager()->replaceSettings(
                    m_aMenuResourceURL, m_xMenuSettings );
            }
            else
            {
                GetConfigManager()->insertSettings(
                    m_aMenuResourceURL, m_xMenuSettings );
            }
        }
        catch ( container::NoSuchElementException& )
        {
            OSL_TRACE("caught container::NoSuchElementException saving settings");
        }
        catch ( com::sun::star::io::IOException& )
        {
            OSL_TRACE("caught IOException saving settings");
        }
        catch ( com::sun::star::uno::Exception& )
        {
            OSL_TRACE("caught some other exception saving settings");
        }

        SetModified( false );

        result = PersistChanges( GetConfigManager() );
    }

    return result;
}

void MenuSaveInData::Apply(
    SvxConfigEntry* pRootEntry_,
    uno::Reference< container::XIndexContainer >& rMenuBar,
    uno::Reference< lang::XSingleComponentFactory >& rFactory,
    SvTreeListEntry *pParentEntry )
{
    (void)pRootEntry_;
    (void)pParentEntry;

    SvxEntries::const_iterator iter = GetEntries()->begin();
    SvxEntries::const_iterator end = GetEntries()->end();

    uno::Reference<uno::XComponentContext> xContext = ::comphelper::getProcessComponentContext();

    for ( ; iter != end; ++iter )
    {
        SvxConfigEntry* pEntryData = *iter;

        uno::Sequence< beans::PropertyValue > aPropValueSeq =
            ConvertSvxConfigEntry( m_xCommandToLabelMap, pEntryData );

        uno::Reference< container::XIndexContainer > xSubMenuBar(
            rFactory->createInstanceWithContext( xContext ),
            uno::UNO_QUERY );

        sal_Int32 nIndex = aPropValueSeq.getLength();
        aPropValueSeq.realloc( nIndex + 1 );
        aPropValueSeq[nIndex].Name = m_aDescriptorContainer;
        aPropValueSeq[nIndex].Value <<= xSubMenuBar;
        rMenuBar->insertByIndex(
            rMenuBar->getCount(), uno::makeAny( aPropValueSeq ));
        ApplyMenu( xSubMenuBar, rFactory, pEntryData );
    }
}

void MenuSaveInData::ApplyMenu(
    uno::Reference< container::XIndexContainer >& rMenuBar,
    uno::Reference< lang::XSingleComponentFactory >& rFactory,
    SvxConfigEntry* pMenuData )
{
    uno::Reference<uno::XComponentContext> xContext = ::comphelper::getProcessComponentContext();

    SvxEntries::const_iterator iter = pMenuData->GetEntries()->begin();
    SvxEntries::const_iterator end = pMenuData->GetEntries()->end();

    for ( ; iter != end; ++iter )
    {
        SvxConfigEntry* pEntry = *iter;

        if ( pEntry->IsPopup() )
        {
            uno::Sequence< beans::PropertyValue > aPropValueSeq =
                ConvertSvxConfigEntry( m_xCommandToLabelMap, pEntry );

            uno::Reference< container::XIndexContainer > xSubMenuBar(
                rFactory->createInstanceWithContext( xContext ),
                    uno::UNO_QUERY );

            sal_Int32 nIndex = aPropValueSeq.getLength();
            aPropValueSeq.realloc( nIndex + 1 );
            aPropValueSeq[nIndex].Name = m_aDescriptorContainer;
            aPropValueSeq[nIndex].Value <<= xSubMenuBar;

            rMenuBar->insertByIndex(
                rMenuBar->getCount(), uno::makeAny( aPropValueSeq ));

            ApplyMenu( xSubMenuBar, rFactory, pEntry );
        }
        else if ( pEntry->IsSeparator() )
        {
            rMenuBar->insertByIndex(
                rMenuBar->getCount(), uno::makeAny( m_aSeparatorSeq ));
        }
        else
        {
            uno::Sequence< beans::PropertyValue > aPropValueSeq =
                ConvertSvxConfigEntry( m_xCommandToLabelMap, pEntry );
            rMenuBar->insertByIndex(
                rMenuBar->getCount(), uno::makeAny( aPropValueSeq ));
        }
    }
}

void
MenuSaveInData::Reset()
{
    GetConfigManager()->reset();

    delete pRootEntry;
    pRootEntry = NULL;

    try
    {
        m_xMenuSettings = GetConfigManager()->getSettings(
            m_aMenuResourceURL, sal_False );
    }
    catch ( container::NoSuchElementException& )
    {
        // will use default settings
    }
}

class PopupPainter : public SvLBoxString
{
public:
    PopupPainter( SvTreeListEntry* pEntry, const OUString& rStr )
        : SvLBoxString( pEntry, 0, rStr )
    { }

    virtual ~PopupPainter() { }

    virtual void Paint( const Point& rPos, SvTreeListBox& rOutDev,
        const SvViewDataEntry* pView, const SvTreeListEntry* pEntry) SAL_OVERRIDE
    {
        SvLBoxString::Paint(rPos, rOutDev, pView, pEntry);

        Color aOldFillColor = rOutDev.GetFillColor();

        SvTreeListBox* pTreeBox = static_cast< SvTreeListBox* >( &rOutDev );
        long nX = pTreeBox->GetSizePixel().Width();

        ScrollBar* pVScroll = pTreeBox->GetVScroll();
        if ( pVScroll->IsVisible() )
        {
            nX -= pVScroll->GetSizePixel().Width();
        }

        const SvViewDataItem* pItem = rOutDev.GetViewDataItem( pEntry, this );
        nX -= pItem->maSize.Height();

        long nSize = pItem->maSize.Height() / 2;
        long nHalfSize = nSize / 2;
        long nY = rPos.Y() + nHalfSize;

        if ( aOldFillColor == COL_WHITE )
        {
            rOutDev.SetFillColor( Color( COL_BLACK ) );
        }
        else
        {
            rOutDev.SetFillColor( Color( COL_WHITE ) );
        }

        long n = 0;
        while ( n <= nHalfSize )
        {
            rOutDev.DrawRect( Rectangle( nX+n, nY+n, nX+n, nY+nSize-n ) );
            ++n;
        }

        rOutDev.SetFillColor( aOldFillColor );
    }
};

/******************************************************************************
 *
 * SvxMenuEntriesListBox is the listbox in which the menu items for a
 * particular menu are shown. We have a custom listbox because we need
 * to add drag'n'drop support from the Macro Selector and within the
 * listbox
 *
 *****************************************************************************/
SvxMenuEntriesListBox::SvxMenuEntriesListBox(Window* pParent, SvxConfigPage* pPg)
    : SvTreeListBox(pParent, WB_TABSTOP|WB_CLIPCHILDREN|WB_HIDESELECTION|WB_BORDER)
    , pPage(pPg)
    , m_bIsInternalDrag( false )
{
    SetSpaceBetweenEntries( 3 );
    SetEntryHeight( ENTRY_HEIGHT );

    SetHighlightRange();
    SetSelectionMode(SINGLE_SELECTION);

    SetDragDropMode( SV_DRAGDROP_CTRL_MOVE  |
                     SV_DRAGDROP_APP_COPY   |
                     SV_DRAGDROP_ENABLE_TOP |
                     SV_DRAGDROP_APP_DROP);
}

SvxMenuEntriesListBox::~SvxMenuEntriesListBox()
{
    // do nothing
}

// drag and drop support
DragDropMode SvxMenuEntriesListBox::NotifyStartDrag(
    TransferDataContainer& aTransferDataContainer, SvTreeListEntry* pEntry )
{
    (void)aTransferDataContainer;
    (void)pEntry;

    m_bIsInternalDrag = true;
    return GetDragDropMode();
}

void SvxMenuEntriesListBox::DragFinished( sal_Int8 nDropAction )
{
    (void)nDropAction;
    m_bIsInternalDrag = false;
}

sal_Int8 SvxMenuEntriesListBox::AcceptDrop( const AcceptDropEvent& rEvt )
{
    if ( m_bIsInternalDrag )
    {
        // internal copy isn't allowed!
        if ( rEvt.mnAction == DND_ACTION_COPY )
            return DND_ACTION_NONE;
        else
            return SvTreeListBox::AcceptDrop( rEvt );
    }

    // Always do COPY instead of MOVE if D&D comes from outside!
    AcceptDropEvent aNewAcceptDropEvent( rEvt );
    aNewAcceptDropEvent.mnAction = DND_ACTION_COPY;
    return SvTreeListBox::AcceptDrop( aNewAcceptDropEvent );
}

bool SvxMenuEntriesListBox::NotifyAcceptDrop( SvTreeListEntry* )
{
    return true;
}

TriState SvxMenuEntriesListBox::NotifyMoving(
    SvTreeListEntry* pTarget, SvTreeListEntry* pSource,
    SvTreeListEntry*& rpNewParent, sal_uLong& rNewChildPos)
{
    // only try to do a move if we are dragging within the list box
    if ( m_bIsInternalDrag )
    {
        if ( pPage->MoveEntryData( pSource, pTarget ) )
        {
            SvTreeListBox::NotifyMoving(
                pTarget, pSource, rpNewParent, rNewChildPos );
            return TRISTATE_TRUE;
        }
        else
        {
            return TRISTATE_FALSE;
        }
    }
    else
    {
        return NotifyCopying( pTarget, pSource, rpNewParent, rNewChildPos );
    }
}

TriState SvxMenuEntriesListBox::NotifyCopying(
    SvTreeListEntry* pTarget, SvTreeListEntry* pSource,
    SvTreeListEntry*& rpNewParent, sal_uLong& rNewChildPos)
{
    (void)pSource;
    (void)rpNewParent;
    (void)rNewChildPos;

    if ( !m_bIsInternalDrag )
    {
        // if the target is NULL then add function to the start of the list
        pPage->AddFunction( pTarget, pTarget == NULL );

        // AddFunction already adds the listbox entry so return TRISTATE_FALSE
        // to stop another listbox entry being added
        return TRISTATE_FALSE;
    }

    // Copying is only allowed from external controls, not within the listbox
    return TRISTATE_FALSE;
}

void SvxMenuEntriesListBox::KeyInput( const KeyEvent& rKeyEvent )
{
    KeyCode keycode = rKeyEvent.GetKeyCode();

    // support DELETE for removing the current entry
    if ( keycode == KEY_DELETE )
    {
        pPage->DeleteSelectedContent();
    }
    // support CTRL+UP and CTRL+DOWN for moving selected entries
    else if ( keycode.GetCode() == KEY_UP && keycode.IsMod1() )
    {
        pPage->MoveEntry( true );
    }
    else if ( keycode.GetCode() == KEY_DOWN && keycode.IsMod1() )
    {
        pPage->MoveEntry( false );
    }
    else
    {
        // pass on to superclass
        SvTreeListBox::KeyInput( rKeyEvent );
    }
}

/******************************************************************************
 *
 * SvxConfigPage is the abstract base class on which the Menu and Toolbar
 * configuration tabpages are based. It includes methods which are common to
 * both tabpages to add, delete, move and rename items etc.
 *
 *****************************************************************************/
SvxConfigPage::SvxConfigPage(Window *pParent, const SfxItemSet& rSet)
    : SfxTabPage(pParent, "MenuAssignPage", "cui/ui/menuassignpage.ui", &rSet)
    , bInitialised(false)
    , pCurrentSaveInData(0)
    , m_pContentsListBox(0)
    , m_pSelectorDlg(0)
{
    get(m_pTopLevel, "toplevel");
    get(m_pTopLevelLabel, "toplevelft");
    get(m_pTopLevelListBox, "toplevellist");
    get(m_pNewTopLevelButton, "toplevelbutton");
    get(m_pModifyTopLevelButton, "menuedit");
    get(m_pContents, "contents");
    get(m_pContentsLabel, "contentslabel");
    get(m_pAddCommandsButton, "add");
    get(m_pModifyCommandButton, "modify");
    get(m_pMoveUpButton, "up");
    get(m_pMoveDownButton, "down");
    get(m_pSaveInListBox, "savein");
    get(m_pDescriptionField, "desc");
    m_pDescriptionField->set_height_request(m_pDescriptionField->GetTextHeight()*4);
    get(m_pEntries, "entries");
    Size aSize(LogicToPixel(Size(108, 115), MAP_APPFONT));
    m_pEntries->set_height_request(aSize.Height());
    m_pEntries->set_width_request(aSize.Width());

    m_pDescriptionField->SetControlBackground( GetSettings().GetStyleSettings().GetDialogColor() );
    m_pDescriptionField->EnableCursor( false );
}

void SvxConfigPage::Reset( const SfxItemSet* )
{
    // If we haven't initialised our XMultiServiceFactory reference
    // then Reset is being called at the opening of the dialog.

    // Load menu configuration data for the module of the currently
    // selected document, for the currently selected document, and for
    // all other open documents of the same module type
    if ( !bInitialised )
    {
        sal_uInt16 nPos = 0;
        uno::Reference < css::ui::XUIConfigurationManager > xCfgMgr;
        uno::Reference < css::ui::XUIConfigurationManager > xDocCfgMgr;

        uno::Reference< uno::XComponentContext > xContext(
            ::comphelper::getProcessComponentContext(), uno::UNO_QUERY_THROW );

        m_xFrame = GetFrame();
        OUString aModuleId = GetFrameWithDefaultAndIdentify( m_xFrame );

        // replace %MODULENAME in the label with the correct module name
        uno::Reference< css::frame::XModuleManager2 > xModuleManager(
            css::frame::ModuleManager::create( xContext ));
        OUString aModuleName = GetUIModuleName( aModuleId, xModuleManager );

        OUString title = m_pTopLevel->get_label();
        OUString aSearchString("%MODULENAME" );
        sal_Int32 index = title.indexOf( aSearchString );

        if ( index != -1 )
        {
            title = title.replaceAt(
                index, aSearchString.getLength(), aModuleName );
            m_pTopLevel->set_label(title);
        }

        uno::Reference< css::ui::XModuleUIConfigurationManagerSupplier >
            xModuleCfgSupplier( css::ui::theModuleUIConfigurationManagerSupplier::get(xContext) );

        // Set up data for module specific menus
        SaveInData* pModuleData = NULL;

        try
        {
            xCfgMgr =
                xModuleCfgSupplier->getUIConfigurationManager( aModuleId );

            pModuleData = CreateSaveInData( xCfgMgr,
                                            uno::Reference< css::ui::XUIConfigurationManager >(),
                                            aModuleId,
                                            false );
        }
        catch ( container::NoSuchElementException& )
        {
        }

        if ( pModuleData != NULL )
        {
            nPos = m_pSaveInListBox->InsertEntry(
                utl::ConfigManager::getProductName() +
                OUString( " "  ) + aModuleName );
            m_pSaveInListBox->SetEntryData( nPos, pModuleData );
        }

        // try to retrieve the document based ui configuration manager
        OUString aTitle;
        uno::Reference< frame::XController > xController =
            m_xFrame->getController();
        if ( CanConfig( aModuleId ) && xController.is() )
        {
            uno::Reference< frame::XModel > xModel( xController->getModel() );
            if ( xModel.is() )
            {
                uno::Reference< css::ui::XUIConfigurationManagerSupplier >
                    xCfgSupplier( xModel, uno::UNO_QUERY );

                if ( xCfgSupplier.is() )
                {
                    xDocCfgMgr = xCfgSupplier->getUIConfigurationManager();
                }
                aTitle = ::comphelper::DocumentInfo::getDocumentTitle( xModel );
            }
        }

        SaveInData* pDocData = NULL;
        if ( xDocCfgMgr.is() )
        {
            pDocData = CreateSaveInData( xDocCfgMgr, xCfgMgr, aModuleId, true );

            if ( !pDocData->IsReadOnly() )
            {
                nPos = m_pSaveInListBox->InsertEntry( aTitle );
                m_pSaveInListBox->SetEntryData( nPos, pDocData );
            }
        }

        // if an item to select has been passed in (eg. the ResourceURL for a
        // toolbar) then try to select the SaveInData entry that has that item
        bool bURLToSelectFound = false;
        if ( !m_aURLToSelect.isEmpty() )
        {
            if ( pDocData && pDocData->HasURL( m_aURLToSelect ) )
            {
                m_pSaveInListBox->SelectEntryPos( nPos, true );
                pCurrentSaveInData = pDocData;
                bURLToSelectFound = true;
            }
            else if ( pModuleData && pModuleData->HasURL( m_aURLToSelect ) )
            {
                m_pSaveInListBox->SelectEntryPos( 0, true );
                pCurrentSaveInData = pModuleData;
                bURLToSelectFound = true;
            }
        }

        if ( !bURLToSelectFound )
        {
            // if the document has menu configuration settings select it
            // it the SaveIn listbox, otherwise select the module data
            if ( pDocData != NULL && pDocData->HasSettings() )
            {
                m_pSaveInListBox->SelectEntryPos( nPos, true );
                pCurrentSaveInData = pDocData;
            }
            else
            {
                m_pSaveInListBox->SelectEntryPos( 0, true );
                pCurrentSaveInData = pModuleData;
            }
        }

#ifdef DBG_UTIL
        DBG_ASSERT( pCurrentSaveInData, "SvxConfigPage::Reset(): no SaveInData" );
#endif

        if ( CanConfig( aModuleId ) )
        {
            // Load configuration for other open documents which have
            // same module type
            uno::Sequence< uno::Reference< frame::XFrame > > aFrameList;
            try
            {
                uno::Reference< frame::XDesktop2 > xFramesSupplier = frame::Desktop::create(
                    xContext );

                uno::Reference< frame::XFrames > xFrames =
                    xFramesSupplier->getFrames();

                aFrameList = xFrames->queryFrames(
                    frame::FrameSearchFlag::ALL & ~frame::FrameSearchFlag::SELF );

            }
            catch( const uno::Exception& )
            {
                DBG_UNHANDLED_EXCEPTION();
            }

            for ( sal_Int32 i = 0; i < aFrameList.getLength(); ++i )
            {
                uno::Reference < frame::XFrame > xf = aFrameList[i];

                if ( xf.is() && xf != m_xFrame )
                {
                    OUString aCheckId;
                    try{
                        aCheckId = xModuleManager->identify( xf );
                    } catch(const uno::Exception&)
                        { aCheckId = ""; }

                    if ( aModuleId.equals( aCheckId ) )
                    {
                        // try to get the document based ui configuration manager
                        OUString aTitle2;
                        uno::Reference< frame::XController > xController_ =
                            xf->getController();

                        if ( xController_.is() )
                        {
                            uno::Reference< frame::XModel > xModel(
                                xController_->getModel() );

                            if ( xModel.is() )
                            {
                                uno::Reference<
                                    css::ui::XUIConfigurationManagerSupplier >
                                        xCfgSupplier( xModel, uno::UNO_QUERY );

                                if ( xCfgSupplier.is() )
                                {
                                    xDocCfgMgr =
                                        xCfgSupplier->getUIConfigurationManager();
                                }
                                aTitle2 = ::comphelper::DocumentInfo::getDocumentTitle( xModel );
                            }
                        }

                        if ( xDocCfgMgr.is() )
                        {
                            SaveInData* pData = CreateSaveInData( xDocCfgMgr, xCfgMgr, aModuleId, true );

                            if ( pData && !pData->IsReadOnly() )
                            {
                                nPos = m_pSaveInListBox->InsertEntry( aTitle2 );
                                m_pSaveInListBox->SetEntryData( nPos, pData );
                            }
                        }
                    }
                }
            }
        }

        m_pSaveInListBox->SetSelectHdl(
            LINK( this, SvxConfigPage, SelectSaveInLocation ) );

        bInitialised = true;

        Init();
    }
    else
    {
        if ( QueryReset() == RET_YES )
        {
            // Reset menu configuration for currently selected SaveInData
            GetSaveInData()->Reset();

            Init();
        }
    }
}

OUString SvxConfigPage::GetFrameWithDefaultAndIdentify( uno::Reference< frame::XFrame >& _inout_rxFrame )
{
    OUString sModuleID;
    try
    {
        uno::Reference< uno::XComponentContext > xContext(
            ::comphelper::getProcessComponentContext() );

        uno::Reference< frame::XDesktop2 > xDesktop = frame::Desktop::create(
            xContext );

        if ( !_inout_rxFrame.is() )
            _inout_rxFrame = xDesktop->getActiveFrame();

        if ( !_inout_rxFrame.is() )
        {
            _inout_rxFrame = xDesktop->getCurrentFrame();
        }

        if ( !_inout_rxFrame.is() && SfxViewFrame::Current() )
            _inout_rxFrame = SfxViewFrame::Current()->GetFrame().GetFrameInterface();

        if ( !_inout_rxFrame.is() )
        {
            SAL_WARN( "cui.customize", "SvxConfigPage::GetFrameWithDefaultAndIdentify(): no frame found!" );
            return sModuleID;
        }

        uno::Reference< css::frame::XModuleManager2 > xModuleManager(
                css::frame::ModuleManager::create( xContext ) );

        try
        {
            sModuleID = xModuleManager->identify( _inout_rxFrame );
        }
        catch ( const frame::UnknownModuleException& )
        {
        }

    }
    catch( const uno::Exception& )
    {
        DBG_UNHANDLED_EXCEPTION();
    }

    return sModuleID;
}

bool SvxConfigPage::FillItemSet( SfxItemSet* )
{
    bool result = false;

    for ( sal_uInt16 i = 0 ; i < m_pSaveInListBox->GetEntryCount(); ++i )
    {
        SaveInData* pData =
            (SaveInData*) m_pSaveInListBox->GetEntryData( i );

        result = pData->Apply();
    }
    return result;
}

IMPL_LINK( SvxConfigPage, SelectSaveInLocation, ListBox *, pBox )
{
    (void)pBox;

    pCurrentSaveInData = (SaveInData*) m_pSaveInListBox->GetEntryData(
            m_pSaveInListBox->GetSelectEntryPos());

    Init();
    return 1;
}

void SvxConfigPage::ReloadTopLevelListBox( SvxConfigEntry* pToSelect )
{
    sal_uInt16 nSelectionPos = m_pTopLevelListBox->GetSelectEntryPos();
    m_pTopLevelListBox->Clear();

    if ( GetSaveInData() && GetSaveInData()->GetEntries() )
    {
        SvxEntries::const_iterator iter = GetSaveInData()->GetEntries()->begin();
        SvxEntries::const_iterator end = GetSaveInData()->GetEntries()->end();

        for ( ; iter != end; ++iter )
        {
            SvxConfigEntry* pEntryData = *iter;
            sal_uInt16 nPos = m_pTopLevelListBox->InsertEntry( stripHotKey( pEntryData->GetName() ) );
            m_pTopLevelListBox->SetEntryData( nPos, pEntryData );

            if ( pEntryData == pToSelect )
                nSelectionPos = nPos;

            AddSubMenusToUI( stripHotKey( pEntryData->GetName() ), pEntryData );
        }
    }
#ifdef DBG_UTIL
    else
    {
        DBG_ASSERT( GetSaveInData(), "SvxConfigPage::ReloadTopLevelListBox(): no SaveInData" );
        DBG_ASSERT( GetSaveInData()->GetEntries() ,
            "SvxConfigPage::ReloadTopLevelListBox(): no SaveInData entries" );
    }
#endif

    nSelectionPos = nSelectionPos < m_pTopLevelListBox->GetEntryCount() ?
        nSelectionPos : m_pTopLevelListBox->GetEntryCount() - 1;

    m_pTopLevelListBox->SelectEntryPos( nSelectionPos, true );
    m_pTopLevelListBox->GetSelectHdl().Call( this );
}

void SvxConfigPage::AddSubMenusToUI(
    const OUString& rBaseTitle, SvxConfigEntry* pParentData )
{
    SvxEntries::const_iterator iter = pParentData->GetEntries()->begin();
    SvxEntries::const_iterator end = pParentData->GetEntries()->end();

    for ( ; iter != end; ++iter )
    {
        SvxConfigEntry* pEntryData = *iter;

        if ( pEntryData->IsPopup() )
        {
            OUString subMenuTitle( rBaseTitle );
            subMenuTitle += OUString(aMenuSeparatorStr);
            subMenuTitle += stripHotKey( pEntryData->GetName() );

            sal_uInt16 nPos = m_pTopLevelListBox->InsertEntry( subMenuTitle );
            m_pTopLevelListBox->SetEntryData( nPos, pEntryData );

            AddSubMenusToUI( subMenuTitle, pEntryData );
        }
    }
}

SvxEntries* SvxConfigPage::FindParentForChild(
    SvxEntries* pRootEntries, SvxConfigEntry* pChildData )
{
    SvxEntries::const_iterator iter = pRootEntries->begin();
    SvxEntries::const_iterator end = pRootEntries->end();

    for ( ; iter != end; ++iter )
    {
        SvxConfigEntry* pEntryData = *iter;

        if ( pEntryData == pChildData )
        {
            return pRootEntries;
        }
        else if ( pEntryData->IsPopup() )
        {
            SvxEntries* result =
                FindParentForChild( pEntryData->GetEntries(), pChildData );

            if ( result != NULL )
            {
                return result;
            }
        }
    }
    return NULL;
}

SvTreeListEntry* SvxConfigPage::AddFunction(
    SvTreeListEntry* pTarget, bool bFront, bool bAllowDuplicates )
{
    OUString aDisplayName = m_pSelectorDlg->GetSelectedDisplayName();
    OUString aURL = m_pSelectorDlg->GetScriptURL();

    if ( aURL.isEmpty() )
    {
        return NULL;
    }

    SvxConfigEntry* pNewEntryData =
        new SvxConfigEntry( aDisplayName, aURL, false );
    pNewEntryData->SetUserDefined( true );

    // check that this function is not already in the menu
    SvxConfigEntry* pParent = GetTopLevelSelection();

    if ( !bAllowDuplicates )
    {
        for (SvxEntries::const_iterator iter(pParent->GetEntries()->begin()), end(pParent->GetEntries()->end());
             iter != end ; ++iter)
        {
            SvxConfigEntry *pCurEntry = *iter;

            if ( pCurEntry->GetCommand() == pNewEntryData->GetCommand() )
            {
                // asynchronous error message, because of MsgBoxes
                PostUserEvent(
                    LINK( this, SvxConfigPage, AsyncInfoMsg ) );
                delete pNewEntryData;
                return NULL;
            }
        }
    }

    return InsertEntry( pNewEntryData, pTarget, bFront );
}

SvTreeListEntry* SvxConfigPage::InsertEntry(
    SvxConfigEntry* pNewEntryData,
    SvTreeListEntry* pTarget,
    bool bFront )
{
    // Grab the entries list for the currently selected menu
    SvxEntries* pEntries = GetTopLevelSelection()->GetEntries();

    SvTreeListEntry* pNewEntry = NULL;
    SvTreeListEntry* pCurEntry =
        pTarget != NULL ? pTarget : m_pContentsListBox->GetCurEntry();

    if ( bFront )
    {
        pEntries->insert( pEntries->begin(), pNewEntryData );
        pNewEntry = InsertEntryIntoUI( pNewEntryData, 0 );
    }
    else if ( pCurEntry == NULL || pCurEntry == m_pContentsListBox->Last() )
    {
        pEntries->push_back( pNewEntryData );
        pNewEntry = InsertEntryIntoUI( pNewEntryData );
    }
    else
    {
        SvxConfigEntry* pEntryData =
            (SvxConfigEntry*) pCurEntry->GetUserData();

        SvxEntries::iterator iter = pEntries->begin();
        SvxEntries::const_iterator end = pEntries->end();

        // Advance the iterator to the data for currently selected entry
        sal_uInt16 nPos = 0;
        while (*iter != pEntryData && ++iter != end)
        {
            ++nPos;
        }

        // Now step past it to the entry after the currently selected one
        ++iter;
        ++nPos;

        // Now add the new entry to the UI and to the parent's list
        if ( iter != end )
        {
            pEntries->insert( iter, pNewEntryData );
            pNewEntry = InsertEntryIntoUI( pNewEntryData, nPos );
        }
    }

    if ( pNewEntry != NULL )
    {
        m_pContentsListBox->Select( pNewEntry );
        m_pContentsListBox->MakeVisible( pNewEntry );

        GetSaveInData()->SetModified( true );
    }

    return pNewEntry;
}

SvTreeListEntry* SvxConfigPage::InsertEntryIntoUI(
    SvxConfigEntry* pNewEntryData, sal_uLong nPos )
{
    SvTreeListEntry* pNewEntry = NULL;

    if (pNewEntryData->IsSeparator())
    {
        pNewEntry = m_pContentsListBox->InsertEntry(
            OUString(aSeparatorStr),
            0, false, nPos, pNewEntryData);
    }
    else
    {
        OUString aName = stripHotKey( pNewEntryData->GetName() );

        Image aImage = GetSaveInData()->GetImage(
            pNewEntryData->GetCommand());

        if ( !!aImage )
        {
            pNewEntry = m_pContentsListBox->InsertEntry(
                aName, aImage, aImage, 0, false, nPos, pNewEntryData );
        }
        else
        {
            pNewEntry = m_pContentsListBox->InsertEntry(
                aName, 0, false, nPos, pNewEntryData );
        }

        if ( pNewEntryData->IsPopup() ||
             pNewEntryData->GetStyle() & css::ui::ItemStyle::DROP_DOWN )
        {
            // add new popup painter, it gets destructed by the entry
            pNewEntry->ReplaceItem(
                new PopupPainter( pNewEntry, aName ),
                pNewEntry->ItemCount() - 1 );
        }
    }

    return pNewEntry;
}

IMPL_LINK( SvxConfigPage, AsyncInfoMsg, OUString*, pMsg )
{
    (void)pMsg;

    // Asynchronous msg because of D&D
    InfoBox( this, CUI_RES(
        IBX_MNUCFG_ALREADY_INCLUDED ) ).Execute();

    return 0;
}

IMPL_LINK( SvxConfigPage, MoveHdl, Button *, pButton )
{
    MoveEntry(pButton == m_pMoveUpButton);
    return 0;
}

void SvxConfigPage::MoveEntry( bool bMoveUp )
{
    SvTreeListEntry *pSourceEntry = m_pContentsListBox->FirstSelected();
    SvTreeListEntry *pTargetEntry = NULL;
    SvTreeListEntry *pToSelect = NULL;

    if ( !pSourceEntry )
    {
        return;
    }

    if ( bMoveUp )
    {
        // Move Up is just a Move Down with the source and target reversed
        pTargetEntry = pSourceEntry;
        pSourceEntry = m_pContentsListBox->PrevSibling( pTargetEntry );
        pToSelect = pTargetEntry;
    }
    else
    {
        pTargetEntry = m_pContentsListBox->NextSibling( pSourceEntry );
        pToSelect = pSourceEntry;
    }

    if ( MoveEntryData( pSourceEntry, pTargetEntry ) )
    {
        m_pContentsListBox->GetModel()->Move( pSourceEntry, pTargetEntry );
        m_pContentsListBox->Select( pToSelect );
        m_pContentsListBox->MakeVisible( pToSelect );

        UpdateButtonStates();
    }
}

bool SvxConfigPage::MoveEntryData(
    SvTreeListEntry* pSourceEntry, SvTreeListEntry* pTargetEntry )
{
    //#i53677#
    if (NULL == pSourceEntry || NULL == pTargetEntry)
    {
        return false;
    }

    // Grab the entries list for the currently selected menu
    SvxEntries* pEntries = GetTopLevelSelection()->GetEntries();

    SvxConfigEntry* pSourceData =
        (SvxConfigEntry*) pSourceEntry->GetUserData();

    SvxConfigEntry* pTargetData =
        (SvxConfigEntry*) pTargetEntry->GetUserData();

    if ( pSourceData != NULL && pTargetData != NULL )
    {
        // remove the source entry from our list
        RemoveEntry( pEntries, pSourceData );

        SvxEntries::iterator iter = pEntries->begin();
        SvxEntries::const_iterator end = pEntries->end();

        // advance the iterator to the position of the target entry
        while (*iter != pTargetData && ++iter != end) ;

        // insert the source entry at the position after the target
        pEntries->insert( ++iter, pSourceData );

        GetSaveInData()->SetModified( true );

        return true;
    }

    return false;
}

SvxMenuConfigPage::SvxMenuConfigPage(Window *pParent, const SfxItemSet& rSet)
    : SvxConfigPage(pParent, rSet)
{
    m_pContentsListBox = new SvxMenuEntriesListBox(m_pEntries, this);
    m_pContentsListBox->set_grid_left_attach(0);
    m_pContentsListBox->set_grid_top_attach(0);
    m_pContentsListBox->set_hexpand(true);
    m_pContentsListBox->set_vexpand(true);
    m_pContentsListBox->Show();

    m_pTopLevelListBox->SetSelectHdl(
        LINK( this, SvxMenuConfigPage, SelectMenu ) );

    m_pContentsListBox->SetSelectHdl(
        LINK( this, SvxMenuConfigPage, SelectMenuEntry ) );

    m_pMoveUpButton->SetClickHdl ( LINK( this, SvxConfigPage, MoveHdl) );
    m_pMoveDownButton->SetClickHdl ( LINK( this, SvxConfigPage, MoveHdl) );

    m_pNewTopLevelButton->SetClickHdl  (
        LINK( this, SvxMenuConfigPage, NewMenuHdl ) );

    m_pAddCommandsButton->SetClickHdl  (
        LINK( this, SvxMenuConfigPage, AddCommandsHdl ) );

    PopupMenu* pMenu = m_pModifyTopLevelButton->GetPopupMenu();
    pMenu->SetMenuFlags(
        pMenu->GetMenuFlags() | MENU_FLAG_ALWAYSSHOWDISABLEDENTRIES );

    m_pModifyTopLevelButton->SetSelectHdl(
        LINK( this, SvxMenuConfigPage, MenuSelectHdl ) );

    PopupMenu* pEntry = m_pModifyCommandButton->GetPopupMenu();
    pEntry->SetMenuFlags(
        pEntry->GetMenuFlags() | MENU_FLAG_ALWAYSSHOWDISABLEDENTRIES );

    m_pModifyCommandButton->SetSelectHdl(
        LINK( this, SvxMenuConfigPage, EntrySelectHdl ) );
}

// Populates the Menu combo box
void SvxMenuConfigPage::Init()
{
    // ensure that the UI is cleared before populating it
    m_pTopLevelListBox->Clear();
    m_pContentsListBox->Clear();

    ReloadTopLevelListBox();

    m_pTopLevelListBox->SelectEntryPos(0, true);
    m_pTopLevelListBox->GetSelectHdl().Call(this);
}

SvxMenuConfigPage::~SvxMenuConfigPage()
{
    for ( sal_uInt16 i = 0 ; i < m_pSaveInListBox->GetEntryCount(); ++i )
    {
        MenuSaveInData* pData =
            (MenuSaveInData*) m_pSaveInListBox->GetEntryData( i );

        delete pData;
    }

    delete m_pSelectorDlg;
    delete m_pContentsListBox;
}

IMPL_LINK( SvxMenuConfigPage, SelectMenuEntry, Control *, pBox )
{
    (void)pBox;

    UpdateButtonStates();

    return 1;
}

void SvxMenuConfigPage::UpdateButtonStates()
{
    PopupMenu* pPopup = m_pModifyCommandButton->GetPopupMenu();

    // Disable Up and Down buttons depending on current selection
    SvTreeListEntry* selection = m_pContentsListBox->GetCurEntry();

    if ( m_pContentsListBox->GetEntryCount() == 0 || selection == NULL )
    {
        m_pMoveUpButton->Enable( false );
        m_pMoveDownButton->Enable( false );

        pPopup->EnableItem( ID_BEGIN_GROUP, true );
        pPopup->EnableItem( ID_RENAME, false );
        pPopup->EnableItem( ID_DELETE, false );

        m_pDescriptionField->SetText("");

        return;
    }

    SvTreeListEntry* first = m_pContentsListBox->First();
    SvTreeListEntry* last = m_pContentsListBox->Last();

    m_pMoveUpButton->Enable( selection != first );
    m_pMoveDownButton->Enable( selection != last );

    SvxConfigEntry* pEntryData =
        (SvxConfigEntry*) selection->GetUserData();

    if ( pEntryData->IsSeparator() )
    {
        pPopup->EnableItem( ID_DELETE, true );
        pPopup->EnableItem( ID_BEGIN_GROUP, false );
        pPopup->EnableItem( ID_RENAME, false );

        m_pDescriptionField->SetText("");
    }
    else
    {
        pPopup->EnableItem( ID_BEGIN_GROUP, true );
        pPopup->EnableItem( ID_DELETE, true );
        pPopup->EnableItem( ID_RENAME, true );

        m_pDescriptionField->SetText(pEntryData->GetHelpText());
    }
}

void SvxMenuConfigPage::DeleteSelectedTopLevel()
{
    SvxConfigEntry* pMenuData = GetTopLevelSelection();

    SvxEntries* pParentEntries =
        FindParentForChild( GetSaveInData()->GetEntries(), pMenuData );

    RemoveEntry( pParentEntries, pMenuData );
    delete pMenuData;

    ReloadTopLevelListBox();

    GetSaveInData()->SetModified( true );
}

bool SvxMenuConfigPage::DeleteSelectedContent()
{
    SvTreeListEntry *pActEntry = m_pContentsListBox->FirstSelected();

    if ( pActEntry != NULL )
    {
        // get currently selected menu entry
        SvxConfigEntry* pMenuEntry =
            (SvxConfigEntry*) pActEntry->GetUserData();

        // get currently selected menu
        SvxConfigEntry* pMenu = GetTopLevelSelection();

        // remove menu entry from the list for this menu
        RemoveEntry( pMenu->GetEntries(), pMenuEntry );

        // remove menu entry from UI
        m_pContentsListBox->GetModel()->Remove( pActEntry );

        // if this is a submenu entry, redraw the menus list box
        if ( pMenuEntry->IsPopup() )
        {
            ReloadTopLevelListBox();
        }

        // delete data for menu entry
        delete pMenuEntry;

        GetSaveInData()->SetModified( true );

        return true;
    }
    return false;
}

short SvxMenuConfigPage::QueryReset()
{
    OUString msg = CUI_RES( RID_SVXSTR_CONFIRM_MENU_RESET );

    OUString saveInName = m_pSaveInListBox->GetEntry(
        m_pSaveInListBox->GetSelectEntryPos() );

    OUString label = replaceSaveInName( msg, saveInName );

    QueryBox qbox( this, WB_YES_NO, label );

    return qbox.Execute();
}

IMPL_LINK( SvxMenuConfigPage, SelectMenu, ListBox *, pBox )
{
    (void)pBox;

    m_pContentsListBox->Clear();

    SvxConfigEntry* pMenuData = GetTopLevelSelection();

    PopupMenu* pPopup = m_pModifyTopLevelButton->GetPopupMenu();
    if ( pMenuData )
    {
        pPopup->EnableItem( ID_DELETE, pMenuData->IsDeletable() );
        pPopup->EnableItem( ID_RENAME, pMenuData->IsRenamable() );
        pPopup->EnableItem( ID_MOVE, pMenuData->IsMovable() );

        SvxEntries* pEntries = pMenuData->GetEntries();
        SvxEntries::const_iterator iter = pEntries->begin();

        for ( ; iter != pEntries->end(); ++iter )
        {
            SvxConfigEntry* pEntry = *iter;
            InsertEntryIntoUI( pEntry );
        }
    }

    UpdateButtonStates();

    return 0;
}

IMPL_LINK( SvxMenuConfigPage, MenuSelectHdl, MenuButton *, pButton )
{
    OString sIdent = pButton->GetCurItemIdent();

    if (sIdent == "delete")
    {
        DeleteSelectedTopLevel();
    }
    else if (sIdent == "rename")
    {
        SvxConfigEntry* pMenuData = GetTopLevelSelection();

        OUString aNewName( stripHotKey( pMenuData->GetName() ) );
        OUString aDesc = CUI_RESSTR( RID_SVXSTR_LABEL_NEW_NAME );

        boost::scoped_ptr<SvxNameDialog> pNameDialog(new SvxNameDialog( this, aNewName, aDesc ));
        pNameDialog->SetHelpId( HID_SVX_CONFIG_RENAME_MENU );
        pNameDialog->SetText( CUI_RESSTR( RID_SVXSTR_RENAME_MENU ) );

        if ( pNameDialog->Execute() == RET_OK ) {
            pNameDialog->GetName( aNewName );
            pMenuData->SetName( aNewName );

            ReloadTopLevelListBox();

            GetSaveInData()->SetModified( true );
        }
    }
    else if (sIdent == "move")
    {
        SvxConfigEntry* pMenuData = GetTopLevelSelection();

        boost::scoped_ptr<SvxMainMenuOrganizerDialog> pDialog(
            new SvxMainMenuOrganizerDialog( this,
                GetSaveInData()->GetEntries(), pMenuData ));

        if ( pDialog->Execute() == RET_OK )
        {
            GetSaveInData()->SetEntries( pDialog->GetEntries() );

            ReloadTopLevelListBox( pDialog->GetSelectedEntry() );

            GetSaveInData()->SetModified( true );
        }
    }
    else
        return sal_False;
    return sal_True;
}

IMPL_LINK( SvxMenuConfigPage, EntrySelectHdl, MenuButton *, pButton )
{
    OString sIdent = pButton->GetCurItemIdent();
    if (sIdent == "addsubmenu")
    {
        OUString aNewName;
        OUString aDesc = CUI_RESSTR( RID_SVXSTR_SUBMENU_NAME );

        boost::scoped_ptr<SvxNameDialog> pNameDialog(new SvxNameDialog( this, aNewName, aDesc ));
        pNameDialog->SetHelpId( HID_SVX_CONFIG_NAME_SUBMENU );
        pNameDialog->SetText( CUI_RESSTR( RID_SVXSTR_ADD_SUBMENU ) );

        if ( pNameDialog->Execute() == RET_OK ) {
            pNameDialog->GetName(aNewName);

            SvxConfigEntry* pNewEntryData =
                new SvxConfigEntry( aNewName, aNewName, true );
            pNewEntryData->SetUserDefined( true );

            InsertEntry( pNewEntryData );

            ReloadTopLevelListBox();

            GetSaveInData()->SetModified( true );
        }
    }
    else if (sIdent == "addseparator")
    {
        SvxConfigEntry* pNewEntryData = new SvxConfigEntry;
        pNewEntryData->SetUserDefined( true );
        InsertEntry( pNewEntryData );
    }
    else if (sIdent == "moddelete")
    {
        DeleteSelectedContent();
    }
    else if (sIdent == "modrename")
    {
        SvTreeListEntry* pActEntry = m_pContentsListBox->GetCurEntry();
        SvxConfigEntry* pEntry =
            (SvxConfigEntry*) pActEntry->GetUserData();

        OUString aNewName( stripHotKey( pEntry->GetName() ) );
        OUString aDesc = CUI_RESSTR( RID_SVXSTR_LABEL_NEW_NAME );

        boost::scoped_ptr<SvxNameDialog> pNameDialog(new SvxNameDialog( this, aNewName, aDesc ));
        pNameDialog->SetHelpId( HID_SVX_CONFIG_RENAME_MENU_ITEM );
        pNameDialog->SetText( CUI_RESSTR( RID_SVXSTR_RENAME_MENU ) );

        if ( pNameDialog->Execute() == RET_OK ) {
            pNameDialog->GetName(aNewName);

            pEntry->SetName( aNewName );
            m_pContentsListBox->SetEntryText( pActEntry, aNewName );

            GetSaveInData()->SetModified( true );
        }
    }
    else
    {
        return sal_False;
    }

    if ( GetSaveInData()->IsModified() )
    {
        UpdateButtonStates();
    }

    return sal_True;
}

IMPL_LINK( SvxMenuConfigPage, AddFunctionHdl,
    SvxScriptSelectorDialog *, pDialog )
{
    (void)pDialog;

    AddFunction();

    return 0;
}

IMPL_LINK( SvxMenuConfigPage, NewMenuHdl, Button *, pButton )
{
    (void)pButton;

    boost::scoped_ptr<SvxMainMenuOrganizerDialog> pDialog(
        new SvxMainMenuOrganizerDialog( 0,
            GetSaveInData()->GetEntries(), NULL, true ));

    if ( pDialog->Execute() == RET_OK )
    {
        GetSaveInData()->SetEntries( pDialog->GetEntries() );
        ReloadTopLevelListBox( pDialog->GetSelectedEntry() );
        GetSaveInData()->SetModified( true );
    }

    return 0;
}

IMPL_LINK( SvxMenuConfigPage, AddCommandsHdl, Button *, pButton )
{
    (void)pButton;

    if ( m_pSelectorDlg == NULL )
    {
        // Create Script Selector which also shows builtin commands
        m_pSelectorDlg = new SvxScriptSelectorDialog( this, true, m_xFrame );

        m_pSelectorDlg->SetAddHdl(
            LINK( this, SvxMenuConfigPage, AddFunctionHdl ) );

        m_pSelectorDlg->SetDialogDescription( CUI_RES( RID_SVXSTR_MENU_ADDCOMMANDS_DESCRIPTION ) );
    }

    // Position the Script Selector over the Add button so it is
    // beside the menu contents list and does not obscure it
    m_pSelectorDlg->SetPosPixel( m_pAddCommandsButton->GetPosPixel() );

    m_pSelectorDlg->SetImageProvider(
        static_cast< ImageProvider* >( GetSaveInData() ) );

    m_pSelectorDlg->Show();
    return 1;
}

SaveInData* SvxMenuConfigPage::CreateSaveInData(
    const uno::Reference< css::ui::XUIConfigurationManager >& xCfgMgr,
    const uno::Reference< css::ui::XUIConfigurationManager >& xParentCfgMgr,
    const OUString& aModuleId,
    bool bDocConfig )
{
    return static_cast< SaveInData* >(
        new MenuSaveInData( xCfgMgr, xParentCfgMgr, aModuleId, bDocConfig ));
}

SvxMainMenuOrganizerDialog::SvxMainMenuOrganizerDialog(
    Window* pParent, SvxEntries* entries,
    SvxConfigEntry* selection, bool bCreateMenu )
    : ModalDialog(pParent, "MoveMenuDialog", "cui/ui/movemenu.ui")
    , bModified(false)
{
    get(m_pMenuBox, "namebox");
    get(m_pMenuNameEdit, "menuname");
    get(m_pMoveUpButton, "up");
    get(m_pMoveDownButton, "down");
    get(m_pMenuListBox, "menulist");
    m_pMenuListBox->set_height_request(m_pMenuListBox->GetTextHeight() * 12);

    // Copy the entries list passed in
    if ( entries != NULL )
    {
        SvxConfigEntry* pEntry;
        SvTreeListEntry* pLBEntry;

        pEntries = new SvxEntries();
        SvxEntries::const_iterator iter = entries->begin();

        while ( iter != entries->end() )
        {
            pEntry = *iter;
            pLBEntry =
                m_pMenuListBox->InsertEntry( stripHotKey( pEntry->GetName() ) );
            pLBEntry->SetUserData( pEntry );
            pEntries->push_back( pEntry );

            if ( pEntry == selection )
            {
                m_pMenuListBox->Select( pLBEntry );
            }
            ++iter;
        }
    }

    if ( bCreateMenu )
    {
        // Generate custom name for new menu
        OUString prefix = CUI_RES( RID_SVXSTR_NEW_MENU );

        OUString newname = generateCustomName( prefix, entries );
        OUString newurl = generateCustomMenuURL( pEntries );

        SvxConfigEntry* pNewEntryData =
            new SvxConfigEntry( newname, newurl, true );
        pNewEntryData->SetUserDefined( true );
        pNewEntryData->SetMain( true );

        pNewMenuEntry =
            m_pMenuListBox->InsertEntry( stripHotKey( pNewEntryData->GetName() ) );
        m_pMenuListBox->Select( pNewMenuEntry );

        pNewMenuEntry->SetUserData( pNewEntryData );

        pEntries->push_back( pNewEntryData );

        m_pMenuNameEdit->SetText( newname );
        m_pMenuNameEdit->SetModifyHdl(
            LINK( this, SvxMainMenuOrganizerDialog, ModifyHdl ) );
    }
    else
    {
        pNewMenuEntry = NULL;

        // hide name label and textfield
        m_pMenuBox->Hide();
        // change the title
        SetText( CUI_RES( RID_SVXSTR_MOVE_MENU ) );
    }

    m_pMenuListBox->SetSelectHdl(
        LINK( this, SvxMainMenuOrganizerDialog, SelectHdl ) );

    m_pMoveUpButton->SetClickHdl (
        LINK( this, SvxMainMenuOrganizerDialog, MoveHdl) );
    m_pMoveDownButton->SetClickHdl (
        LINK( this, SvxMainMenuOrganizerDialog, MoveHdl) );
}

IMPL_LINK(SvxMainMenuOrganizerDialog, ModifyHdl, Edit*, pEdit)
{
    (void)pEdit;

    // if the Edit control is empty do not change the name
    if (m_pMenuNameEdit->GetText().isEmpty())
    {
        return 0;
    }

    SvxConfigEntry* pNewEntryData =
        (SvxConfigEntry*) pNewMenuEntry->GetUserData();

    pNewEntryData->SetName(m_pMenuNameEdit->GetText());

    m_pMenuListBox->SetEntryText( pNewMenuEntry, pNewEntryData->GetName() );

    return 0;
}

SvxMainMenuOrganizerDialog::~SvxMainMenuOrganizerDialog()
{
}

IMPL_LINK( SvxMainMenuOrganizerDialog, SelectHdl, Control*, pCtrl )
{
    (void)pCtrl;
    UpdateButtonStates();
    return 1;
}

void SvxMainMenuOrganizerDialog::UpdateButtonStates()
{
    // Disable Up and Down buttons depending on current selection
    SvTreeListEntry* selection = m_pMenuListBox->GetCurEntry();
    SvTreeListEntry* first = m_pMenuListBox->First();
    SvTreeListEntry* last = m_pMenuListBox->Last();

    m_pMoveUpButton->Enable( selection != first );
    m_pMoveDownButton->Enable( selection != last );
}

IMPL_LINK( SvxMainMenuOrganizerDialog, MoveHdl, Button *, pButton )
{
    SvTreeListEntry *pSourceEntry = m_pMenuListBox->FirstSelected();
    SvTreeListEntry *pTargetEntry = NULL;

    if ( !pSourceEntry )
    {
        return 0;
    }

    if (pButton == m_pMoveDownButton)
    {
        pTargetEntry = m_pMenuListBox->NextSibling( pSourceEntry );
    }
    else if (pButton == m_pMoveUpButton)
    {
        // Move Up is just a Move Down with the source and target reversed
        pTargetEntry = pSourceEntry;
        pSourceEntry = m_pMenuListBox->PrevSibling( pTargetEntry );
    }

    if ( pSourceEntry != NULL && pTargetEntry != NULL )
    {
        SvxConfigEntry* pSourceData =
            (SvxConfigEntry*) pSourceEntry->GetUserData();
        SvxConfigEntry* pTargetData =
            (SvxConfigEntry*) pTargetEntry->GetUserData();

        SvxEntries::iterator iter1 = GetEntries()->begin();
        SvxEntries::iterator iter2 = GetEntries()->begin();
        SvxEntries::const_iterator end = GetEntries()->end();

        // Advance the iterators to the positions of the source and target
        while (*iter1 != pSourceData && ++iter1 != end) ;
        while (*iter2 != pTargetData && ++iter2 != end) ;

        // Now swap the entries in the menu list and in the UI
        if ( iter1 != end && iter2 != end )
        {
            std::swap( *iter1, *iter2 );
            m_pMenuListBox->GetModel()->Move( pSourceEntry, pTargetEntry );
            m_pMenuListBox->MakeVisible( pSourceEntry );

            bModified = true;
        }
    }

    if ( bModified )
    {
        UpdateButtonStates();
    }

    return 0;
}


SvxConfigEntry* SvxMainMenuOrganizerDialog::GetSelectedEntry()
{
    return (SvxConfigEntry*)m_pMenuListBox->FirstSelected()->GetUserData();
}

const OUString&
SvxConfigEntry::GetHelpText()
{
    if ( aHelpText.isEmpty() )
    {
        if ( !aCommand.isEmpty() )
        {
            aHelpText = Application::GetHelp()->GetHelpText( aCommand, NULL );
        }
    }

    return aHelpText;
}

SvxConfigEntry::SvxConfigEntry( const OUString& rDisplayName,
                                const OUString& rCommandURL, bool bPopup, bool bParentData )
    : nId( 1 )
    , aLabel(rDisplayName)
    , aCommand(rCommandURL)
    , bPopUp(bPopup)
    , bStrEdited( false )
    , bIsUserDefined( false )
    , bIsMain( false )
    , bIsParentData( bParentData )
    , bIsVisible( true )
    , nStyle( 0 )
    , pEntries( 0 )
{
    if (bPopUp)
    {
        pEntries = new SvxEntries();
    }
}

SvxConfigEntry::~SvxConfigEntry()
{
    if ( pEntries != NULL )
    {
        SvxEntries::const_iterator iter = pEntries->begin();

        for ( ; iter != pEntries->end(); ++iter )
        {
            delete *iter;
        }
        delete pEntries;
    }
}

bool SvxConfigEntry::IsMovable()
{
    if ( IsPopup() && !IsMain() )
    {
        return false;
    }
    return true;
}

bool SvxConfigEntry::IsDeletable()
{
    if ( IsMain() && !IsUserDefined() )
    {
        return false;
    }
    return true;
}

bool SvxConfigEntry::IsRenamable()
{
    if ( IsMain() && !IsUserDefined() )
    {
        return false;
    }
    return true;
}

SvxToolbarConfigPage::SvxToolbarConfigPage(Window *pParent, const SfxItemSet& rSet)
    : SvxConfigPage(pParent, rSet)
{
    SetHelpId( HID_SVX_CONFIG_TOOLBAR );

    m_pContentsListBox = new SvxToolbarEntriesListBox(m_pEntries, this);
    m_pContentsListBox->set_grid_left_attach(0);
    m_pContentsListBox->set_grid_top_attach(0);
    m_pContentsListBox->set_hexpand(true);
    m_pContentsListBox->set_vexpand(true);
    m_pContentsListBox->Show();

    m_pContentsListBox->SetHelpId( HID_SVX_CONFIG_TOOLBAR_CONTENTS );
    m_pNewTopLevelButton->SetHelpId( HID_SVX_NEW_TOOLBAR );
    m_pModifyTopLevelButton->SetHelpId( HID_SVX_MODIFY_TOOLBAR );
    m_pAddCommandsButton->SetHelpId( HID_SVX_NEW_TOOLBAR_ITEM );
    m_pModifyCommandButton->SetHelpId( HID_SVX_MODIFY_TOOLBAR_ITEM );
    m_pSaveInListBox->SetHelpId( HID_SVX_SAVE_IN );
    m_pMoveUpButton->SetHelpId( HID_SVX_UP_TOOLBAR_ITEM );
    m_pMoveDownButton->SetHelpId( HID_SVX_DOWN_TOOLBAR_ITEM );

    m_pTopLevel->set_label(CUI_RES(RID_SVXSTR_PRODUCTNAME_TOOLBARS));

    m_pTopLevelLabel->SetText( CUI_RES( RID_SVXSTR_TOOLBAR ) );
    m_pModifyTopLevelButton->SetText( CUI_RES( RID_SVXSTR_TOOLBAR ) );
    m_pContents->set_label(CUI_RES(RID_SVXSTR_TOOLBAR_CONTENT));
    m_pContentsLabel->SetText( CUI_RES( RID_SVXSTR_COMMANDS ) );

    m_pTopLevelListBox->SetSelectHdl(
        LINK( this, SvxToolbarConfigPage, SelectToolbar ) );
    m_pContentsListBox->SetSelectHdl(
        LINK( this, SvxToolbarConfigPage, SelectToolbarEntry ) );

    m_pNewTopLevelButton->SetClickHdl  (
        LINK( this, SvxToolbarConfigPage, NewToolbarHdl ) );

    m_pAddCommandsButton->SetClickHdl  (
        LINK( this, SvxToolbarConfigPage, AddCommandsHdl ) );

    m_pMoveUpButton->SetClickHdl ( LINK( this, SvxToolbarConfigPage, MoveHdl) );
    m_pMoveDownButton->SetClickHdl ( LINK( this, SvxToolbarConfigPage, MoveHdl) );
    // Always enable Up and Down buttons
    // added for issue i53677 by shizhoubo
    m_pMoveDownButton->Enable( true );
    m_pMoveUpButton->Enable( true );

    PopupMenu* pMenu = new PopupMenu( CUI_RES( MODIFY_TOOLBAR ) );
    pMenu->SetMenuFlags(
        pMenu->GetMenuFlags() | MENU_FLAG_ALWAYSSHOWDISABLEDENTRIES );

    m_pModifyTopLevelButton->SetPopupMenu( pMenu );
    m_pModifyTopLevelButton->SetSelectHdl(
        LINK( this, SvxToolbarConfigPage, ToolbarSelectHdl ) );

    PopupMenu* pEntry = new PopupMenu(
        CUI_RES( MODIFY_TOOLBAR_CONTENT ) );
    pEntry->SetMenuFlags(
        pEntry->GetMenuFlags() | MENU_FLAG_ALWAYSSHOWDISABLEDENTRIES );

    m_pModifyCommandButton->SetPopupMenu( pEntry );
    m_pModifyCommandButton->SetSelectHdl(
        LINK( this, SvxToolbarConfigPage, EntrySelectHdl ) );

    // default toolbar to select is standardbar unless a different one
    // has been passed in
    m_aURLToSelect = OUString(ITEM_TOOLBAR_URL );
    m_aURLToSelect += "standardbar";

    const SfxPoolItem* pItem =
        rSet.GetItem( rSet.GetPool()->GetWhich( SID_CONFIG ) );

    if ( pItem )
    {
        OUString text = ((const SfxStringItem*)pItem)->GetValue();
        if (text.startsWith( ITEM_TOOLBAR_URL ))
        {
            m_aURLToSelect = text.copy( 0 );
        }
    }
}

SvxToolbarConfigPage::~SvxToolbarConfigPage()
{
    for ( sal_uInt16 i = 0 ; i < m_pSaveInListBox->GetEntryCount(); ++i )
    {
        ToolbarSaveInData* pData =
            (ToolbarSaveInData*) m_pSaveInListBox->GetEntryData( i );

        delete pData;
    }

    if ( m_pSelectorDlg != NULL )
    {
        delete m_pSelectorDlg;
    }


    delete m_pContentsListBox;
}

void SvxToolbarConfigPage::DeleteSelectedTopLevel()
{
    sal_uInt16 nSelectionPos = m_pTopLevelListBox->GetSelectEntryPos();
    ToolbarSaveInData* pSaveInData = (ToolbarSaveInData*) GetSaveInData();
    pSaveInData->RemoveToolbar( GetTopLevelSelection() );

    if ( m_pTopLevelListBox->GetEntryCount() > 1 )
    {
        // select next entry after the one being deleted
        // selection position is indexed from 0 so need to
        // subtract one from the entry count
        if ( nSelectionPos != m_pTopLevelListBox->GetEntryCount() - 1 )
        {
            m_pTopLevelListBox->SelectEntryPos( nSelectionPos + 1, true );
        }
        else
        {
            m_pTopLevelListBox->SelectEntryPos( nSelectionPos - 1, true );
        }
        m_pTopLevelListBox->GetSelectHdl().Call( this );

        // and now remove the entry
        m_pTopLevelListBox->RemoveEntry( nSelectionPos );
    }
    else
    {
        ReloadTopLevelListBox();
    }
}

bool SvxToolbarConfigPage::DeleteSelectedContent()
{
    SvTreeListEntry *pActEntry = m_pContentsListBox->FirstSelected();

    if ( pActEntry != NULL )
    {
        // get currently selected entry
        SvxConfigEntry* pEntry =
            (SvxConfigEntry*) pActEntry->GetUserData();

        SvxConfigEntry* pToolbar = GetTopLevelSelection();

        // remove entry from the list for this toolbar
        RemoveEntry( pToolbar->GetEntries(), pEntry );

        // remove toolbar entry from UI
        m_pContentsListBox->GetModel()->Remove( pActEntry );

        // delete data for toolbar entry
        delete pEntry;

        (( ToolbarSaveInData* ) GetSaveInData())->ApplyToolbar( pToolbar );
        UpdateButtonStates();

        // if this is the last entry in the toolbar and it is a user
        // defined toolbar pop up a dialog asking the user if they
        // want to delete the toolbar
        if ( m_pContentsListBox->GetEntryCount() == 0 &&
             GetTopLevelSelection()->IsDeletable() )
        {
            QueryBox qbox( this,
                CUI_RES( QBX_CONFIRM_DELETE_TOOLBAR ) );

            if ( qbox.Execute() == RET_YES )
            {
                DeleteSelectedTopLevel();
            }
        }

        return true;
    }

    return false;
}

IMPL_LINK( SvxToolbarConfigPage, MoveHdl, Button *, pButton )
{
    MoveEntry(pButton == m_pMoveUpButton);
    return 0;
}

void SvxToolbarConfigPage::MoveEntry( bool bMoveUp )
{
    SvxConfigPage::MoveEntry( bMoveUp );

    // Apply change to currently selected toolbar
    SvxConfigEntry* pToolbar = GetTopLevelSelection();
    if ( pToolbar )
        ((ToolbarSaveInData*)GetSaveInData())->ApplyToolbar( pToolbar );
    else
    {
        SAL_WARN( "cui.customize", "SvxToolbarConfigPage::MoveEntry(): no entry" );
        UpdateButtonStates();
    }
}

IMPL_LINK( SvxToolbarConfigPage, ToolbarSelectHdl, MenuButton *, pButton )
{
    sal_uInt16 nSelectionPos = m_pTopLevelListBox->GetSelectEntryPos();

    SvxConfigEntry* pToolbar =
        (SvxConfigEntry*)m_pTopLevelListBox->GetEntryData( nSelectionPos );

    ToolbarSaveInData* pSaveInData = (ToolbarSaveInData*) GetSaveInData();

    switch( pButton->GetCurItemId() )
    {
        case ID_DELETE:
        {
            DeleteSelectedTopLevel();
            UpdateButtonStates();
            break;
        }
        case ID_RENAME:
        {
            OUString aNewName( stripHotKey( pToolbar->GetName() ) );
            OUString aDesc = CUI_RESSTR( RID_SVXSTR_LABEL_NEW_NAME );

            boost::scoped_ptr<SvxNameDialog> pNameDialog(new SvxNameDialog( this, aNewName, aDesc ));
            pNameDialog->SetHelpId( HID_SVX_CONFIG_RENAME_TOOLBAR );
            pNameDialog->SetText( CUI_RESSTR( RID_SVXSTR_RENAME_TOOLBAR ) );

            if ( pNameDialog->Execute() == RET_OK )
            {
                pNameDialog->GetName(aNewName);

                pToolbar->SetName( aNewName );
                pSaveInData->ApplyToolbar( pToolbar );

                // have to use remove and insert to change the name
                m_pTopLevelListBox->RemoveEntry( nSelectionPos );
                nSelectionPos =
                    m_pTopLevelListBox->InsertEntry( aNewName, nSelectionPos );
                m_pTopLevelListBox->SetEntryData( nSelectionPos, pToolbar );
                m_pTopLevelListBox->SelectEntryPos( nSelectionPos );
            }
            break;
        }
        case ID_DEFAULT_STYLE:
        {
            QueryBox qbox( this,
                CUI_RES( QBX_CONFIRM_RESTORE_DEFAULT ) );

            if ( qbox.Execute() == RET_YES )
            {
                ToolbarSaveInData* pSaveInData_ =
                    (ToolbarSaveInData*) GetSaveInData();

                pSaveInData_->RestoreToolbar( pToolbar );

                m_pTopLevelListBox->GetSelectHdl().Call( this );
            }

            break;
        }
        case ID_ICONS_ONLY:
        {
            pToolbar->SetStyle( 0 );
            pSaveInData->SetSystemStyle( m_xFrame, pToolbar->GetCommand(), 0 );

            m_pTopLevelListBox->GetSelectHdl().Call( this );

            break;
        }
        case ID_TEXT_ONLY:
        {
            pToolbar->SetStyle( 1 );
            pSaveInData->SetSystemStyle( m_xFrame, pToolbar->GetCommand(), 1 );

            m_pTopLevelListBox->GetSelectHdl().Call( this );

            break;
        }
        case ID_ICONS_AND_TEXT:
        {
            pToolbar->SetStyle( 2 );
            pSaveInData->SetSystemStyle( m_xFrame, pToolbar->GetCommand(), 2 );

            m_pTopLevelListBox->GetSelectHdl().Call( this );

            break;
        }
    }
    return 1;
}

IMPL_LINK( SvxToolbarConfigPage, EntrySelectHdl, MenuButton *, pButton )
{
    bool bNeedsApply = false;

    // get currently selected toolbar
    SvxConfigEntry* pToolbar = GetTopLevelSelection();

    switch( pButton->GetCurItemId() )
    {
        case ID_RENAME:
        {
            SvTreeListEntry* pActEntry = m_pContentsListBox->GetCurEntry();
            SvxConfigEntry* pEntry =
                (SvxConfigEntry*) pActEntry->GetUserData();

            OUString aNewName( stripHotKey( pEntry->GetName() ) );
            OUString aDesc = CUI_RESSTR( RID_SVXSTR_LABEL_NEW_NAME );

            boost::scoped_ptr<SvxNameDialog> pNameDialog(new SvxNameDialog( this, aNewName, aDesc ));
            pNameDialog->SetHelpId( HID_SVX_CONFIG_RENAME_TOOLBAR_ITEM );
            pNameDialog->SetText( CUI_RESSTR( RID_SVXSTR_RENAME_TOOLBAR ) );

            if ( pNameDialog->Execute() == RET_OK ) {
                pNameDialog->GetName(aNewName);

                pEntry->SetName( aNewName );
                m_pContentsListBox->SetEntryText( pActEntry, aNewName );

                bNeedsApply = true;
            }
            break;
        }
        case ID_DEFAULT_COMMAND:
        {
            SvTreeListEntry* pActEntry = m_pContentsListBox->GetCurEntry();
            SvxConfigEntry* pEntry =
                (SvxConfigEntry*) pActEntry->GetUserData();

            sal_uInt16 nSelectionPos = 0;

            // find position of entry within the list
            for ( sal_uInt16 i = 0; i < m_pContentsListBox->GetEntryCount(); ++i )
            {
                if ( m_pContentsListBox->GetEntry( 0, i ) == pActEntry )
                {
                    nSelectionPos = i;
                    break;
                }
            }

            ToolbarSaveInData* pSaveInData =
                (ToolbarSaveInData*) GetSaveInData();

            OUString aSystemName =
                pSaveInData->GetSystemUIName( pEntry->GetCommand() );

            if ( !pEntry->GetName().equals( aSystemName ) )
            {
                pEntry->SetName( aSystemName );
                m_pContentsListBox->SetEntryText(
                    pActEntry, stripHotKey( aSystemName ) );
                bNeedsApply = true;
            }

            uno::Sequence< OUString > aURLSeq( 1 );
            aURLSeq[ 0 ] = pEntry->GetCommand();

            try
            {
                GetSaveInData()->GetImageManager()->removeImages(
                    GetImageType(), aURLSeq );

                // reset backup in entry
                pEntry->SetBackupGraphic(
                    uno::Reference< graphic::XGraphic >() );

                GetSaveInData()->PersistChanges(
                    GetSaveInData()->GetImageManager() );

                m_pContentsListBox->GetModel()->Remove( pActEntry );

                SvTreeListEntry* pNewLBEntry =
                    InsertEntryIntoUI( pEntry, nSelectionPos );

                m_pContentsListBox->SetCheckButtonState( pNewLBEntry,
                    pEntry->IsVisible() ?
                        SV_BUTTON_CHECKED : SV_BUTTON_UNCHECKED );

                m_pContentsListBox->Select( pNewLBEntry );
                m_pContentsListBox->MakeVisible( pNewLBEntry );

                bNeedsApply = true;
            }
               catch ( uno::Exception& )
               {
                OSL_TRACE("Error restoring image");
               }
            break;
        }
        case ID_BEGIN_GROUP:
        {
            SvxConfigEntry* pNewEntryData = new SvxConfigEntry;
            pNewEntryData->SetUserDefined( true );

            SvTreeListEntry* pNewLBEntry = InsertEntry( pNewEntryData );

            m_pContentsListBox->SetCheckButtonInvisible( pNewLBEntry );
            m_pContentsListBox->SetCheckButtonState(
                pNewLBEntry, SV_BUTTON_TRISTATE );

            bNeedsApply = true;
            break;
        }
        case ID_DELETE:
        {
            DeleteSelectedContent();
            break;
        }
        case ID_ICON_ONLY:
        {
            break;
        }
        case ID_TEXT_ONLY:
        {
            break;
        }
        case ID_ICON_AND_TEXT:
        {
            break;
        }
        case ID_CHANGE_SYMBOL:
        {
            SvTreeListEntry* pActEntry = m_pContentsListBox->GetCurEntry();
            SvxConfigEntry* pEntry =
                (SvxConfigEntry*) pActEntry->GetUserData();

            sal_uInt16 nSelectionPos = 0;

            // find position of entry within the list
            for ( sal_uInt16 i = 0; i < m_pContentsListBox->GetEntryCount(); ++i )
            {
                if ( m_pContentsListBox->GetEntry( 0, i ) == pActEntry )
                {
                    nSelectionPos = i;
                    break;
                }
            }

            boost::scoped_ptr<SvxIconSelectorDialog> pIconDialog(
                new SvxIconSelectorDialog( 0,
                    GetSaveInData()->GetImageManager(),
                    GetSaveInData()->GetParentImageManager() ));

            if ( pIconDialog->Execute() == RET_OK )
            {
                uno::Reference< graphic::XGraphic > newgraphic =
                    pIconDialog->GetSelectedIcon();

                if ( newgraphic.is() )
                {
                    uno::Sequence< uno::Reference< graphic::XGraphic > >
                        aGraphicSeq( 1 );

                    uno::Sequence< OUString > aURLSeq( 1 );
                    aURLSeq[ 0 ] = pEntry->GetCommand();

                    if ( !pEntry->GetBackupGraphic().is() )
                    {
                        uno::Reference< graphic::XGraphic > backup;
                        backup = GetGraphic(
                            GetSaveInData()->GetImageManager(), aURLSeq[ 0 ] );

                        if ( backup.is() )
                        {
                            pEntry->SetBackupGraphic( backup );
                        }
                    }

                    aGraphicSeq[ 0 ] = newgraphic;
                    try
                    {
                        GetSaveInData()->GetImageManager()->replaceImages(
                            GetImageType(), aURLSeq, aGraphicSeq );

                        Image aImage( newgraphic );

                        m_pContentsListBox->GetModel()->Remove( pActEntry );
                        SvTreeListEntry* pNewLBEntry =
                            InsertEntryIntoUI( pEntry, nSelectionPos );

                        m_pContentsListBox->SetCheckButtonState( pNewLBEntry,
                            pEntry->IsVisible() ?
                                SV_BUTTON_CHECKED : SV_BUTTON_UNCHECKED );

                        m_pContentsListBox->Select( pNewLBEntry );
                        m_pContentsListBox->MakeVisible( pNewLBEntry );

                        GetSaveInData()->PersistChanges(
                            GetSaveInData()->GetImageManager() );
                    }
                    catch ( uno::Exception& )
                    {
                        OSL_TRACE("Error replacing image");
                    }
                }
            }
            break;
        }
        case ID_RESET_SYMBOL:
        {
            SvTreeListEntry* pActEntry = m_pContentsListBox->GetCurEntry();
            SvxConfigEntry* pEntry =
                (SvxConfigEntry*) pActEntry->GetUserData();

            sal_uInt16 nSelectionPos = 0;

            // find position of entry within the list
            for ( sal_uInt16 i = 0; i < m_pContentsListBox->GetEntryCount(); ++i )
            {
                if ( m_pContentsListBox->GetEntry( 0, i ) == pActEntry )
                {
                    nSelectionPos = i;
                    break;
                }
            }

            uno::Reference< graphic::XGraphic > backup =
                pEntry->GetBackupGraphic();

            uno::Sequence< uno::Reference< graphic::XGraphic > >
                aGraphicSeq( 1 );
            aGraphicSeq[ 0 ] = backup;

            uno::Sequence< OUString > aURLSeq( 1 );
            aURLSeq[ 0 ] = pEntry->GetCommand();

            try
            {
                GetSaveInData()->GetImageManager()->replaceImages(
                    GetImageType(), aURLSeq, aGraphicSeq );

                Image aImage( backup );
                m_pContentsListBox->GetModel()->Remove( pActEntry );

                SvTreeListEntry* pNewLBEntry =
                    InsertEntryIntoUI( pEntry, nSelectionPos );

                m_pContentsListBox->SetCheckButtonState( pNewLBEntry,
                    pEntry->IsVisible() ?
                        SV_BUTTON_CHECKED : SV_BUTTON_UNCHECKED );

                m_pContentsListBox->Select( pNewLBEntry );
                m_pContentsListBox->MakeVisible( pNewLBEntry );

                // reset backup in entry
                pEntry->SetBackupGraphic(
                    uno::Reference< graphic::XGraphic >() );

                GetSaveInData()->PersistChanges(
                    GetSaveInData()->GetImageManager() );
            }
            catch ( uno::Exception& )
            {
                OSL_TRACE("Error resetting image");
            }
            break;
        }
    }

    if ( bNeedsApply )
    {
        (( ToolbarSaveInData* ) GetSaveInData())->ApplyToolbar( pToolbar );
        UpdateButtonStates();
    }

    return 1;
}

void SvxToolbarConfigPage::Init()
{
    // ensure that the UI is cleared before populating it
    m_pTopLevelListBox->Clear();
    m_pContentsListBox->Clear();

    ReloadTopLevelListBox();

    sal_uInt16 nPos = 0;
    if ( !m_aURLToSelect.isEmpty() )
    {
        for ( sal_uInt16 i = 0 ; i < m_pTopLevelListBox->GetEntryCount(); ++i )
        {
            SvxConfigEntry* pData =
                (SvxConfigEntry*) m_pTopLevelListBox->GetEntryData( i );

            if ( pData->GetCommand().equals( m_aURLToSelect ) )
            {
                nPos = i;
                break;
            }
        }

        // in future select the default toolbar: Standard
        m_aURLToSelect = OUString(ITEM_TOOLBAR_URL );
        m_aURLToSelect += "standardbar";
    }

    m_pTopLevelListBox->SelectEntryPos(nPos, true);
    m_pTopLevelListBox->GetSelectHdl().Call(this);
}

SaveInData* SvxToolbarConfigPage::CreateSaveInData(
    const uno::Reference< css::ui::XUIConfigurationManager >& xCfgMgr,
    const uno::Reference< css::ui::XUIConfigurationManager >& xParentCfgMgr,
    const OUString& aModuleId,
    bool bDocConfig )
{
    return static_cast< SaveInData* >(
        new ToolbarSaveInData( xCfgMgr, xParentCfgMgr, aModuleId, bDocConfig ));
}

ToolbarSaveInData::ToolbarSaveInData(
    const uno::Reference < css::ui::XUIConfigurationManager >& xCfgMgr,
    const uno::Reference < css::ui::XUIConfigurationManager >& xParentCfgMgr,
    const OUString& aModuleId,
    bool docConfig ) :

    SaveInData              ( xCfgMgr, xParentCfgMgr, aModuleId, docConfig ),
    pRootEntry              ( NULL ),
    m_aDescriptorContainer  ( ITEM_DESCRIPTOR_CONTAINER  )

{
    uno::Reference<uno::XComponentContext> xContext = ::comphelper::getProcessComponentContext();
    // Initialize the m_xPersistentWindowState variable which is used
    // to get the default properties of system toolbars such as name
    uno::Reference< container::XNameAccess > xPWSS = css::ui::theWindowStateConfiguration::get( xContext );

    xPWSS->getByName( aModuleId ) >>= m_xPersistentWindowState;
}

ToolbarSaveInData::~ToolbarSaveInData()
{
    delete pRootEntry;
}

void ToolbarSaveInData::SetSystemStyle(
    uno::Reference< frame::XFrame > xFrame,
    const OUString& rResourceURL,
    sal_Int32 nStyle )
{
    // change the style using the API
    SetSystemStyle( rResourceURL, nStyle );

    // this code is a temporary hack as the UI is not updating after
    // changing the toolbar style via the API
    uno::Reference< css::frame::XLayoutManager > xLayoutManager;
    Window *window = NULL;

    uno::Reference< beans::XPropertySet > xPropSet( xFrame, uno::UNO_QUERY );
    if ( xPropSet.is() )
    {
        uno::Any a = xPropSet->getPropertyValue(
            OUString( "LayoutManager"  ) );
        a >>= xLayoutManager;
    }

    if ( xLayoutManager.is() )
    {
        uno::Reference< css::ui::XUIElement > xUIElement =
            xLayoutManager->getElement( rResourceURL );

        // check reference before we call getRealInterface. The layout manager
        // can only provide references for elements that have been created
        // before. It's possible that the current element is not available.
        uno::Reference< com::sun::star::awt::XWindow > xWindow;
        if ( xUIElement.is() )
            xWindow = uno::Reference< com::sun::star::awt::XWindow >(
                        xUIElement->getRealInterface(), uno::UNO_QUERY );

        window = VCLUnoHelper::GetWindow( xWindow );
    }

    if ( window != NULL && window->GetType() == WINDOW_TOOLBOX )
    {
        ToolBox* toolbox = (ToolBox*)window;

        if ( nStyle == 0 )
        {
            toolbox->SetButtonType( BUTTON_SYMBOL );
        }
        else if ( nStyle == 1 )
        {
            toolbox->SetButtonType( BUTTON_TEXT );
        }
        if ( nStyle == 2 )
        {
            toolbox->SetButtonType( BUTTON_SYMBOLTEXT );
        }
    }
}

void ToolbarSaveInData::SetSystemStyle(
    const OUString& rResourceURL,
    sal_Int32 nStyle )
{
    if ( rResourceURL.startsWith( "private" ) &&
         m_xPersistentWindowState.is() &&
         m_xPersistentWindowState->hasByName( rResourceURL ) )
    {
        try
        {
            uno::Sequence< beans::PropertyValue > aProps;

            uno::Any a( m_xPersistentWindowState->getByName( rResourceURL ) );

            if ( a >>= aProps )
            {
                for ( sal_Int32 i = 0; i < aProps.getLength(); ++i )
                {
                    if ( aProps[ i ].Name.equalsAscii( ITEM_DESCRIPTOR_STYLE) )
                    {
                        aProps[ i ].Value = uno::makeAny( nStyle );
                        break;
                    }
                }
            }

            uno::Reference< container::XNameReplace >
                xNameReplace( m_xPersistentWindowState, uno::UNO_QUERY );

            xNameReplace->replaceByName( rResourceURL, uno::makeAny( aProps ) );
        }
        catch ( uno::Exception& )
        {
            // do nothing, a default value is returned
            OSL_TRACE("Exception setting toolbar style");
        }
    }
}

sal_Int32 ToolbarSaveInData::GetSystemStyle( const OUString& rResourceURL )
{
    sal_Int32 result = 0;

    if ( rResourceURL.startsWith( "private" ) &&
         m_xPersistentWindowState.is() &&
         m_xPersistentWindowState->hasByName( rResourceURL ) )
    {
        try
        {
            uno::Sequence< beans::PropertyValue > aProps;
            uno::Any a( m_xPersistentWindowState->getByName( rResourceURL ) );

            if ( a >>= aProps )
            {
                for ( sal_Int32 i = 0; i < aProps.getLength(); ++i )
                {
                    if ( aProps[ i ].Name.equalsAscii( ITEM_DESCRIPTOR_STYLE) )
                    {
                        aProps[i].Value >>= result;
                        break;
                    }
                }
            }
        }
        catch ( uno::Exception& )
        {
            // do nothing, a default value is returned
        }
    }

    return result;
}

OUString ToolbarSaveInData::GetSystemUIName( const OUString& rResourceURL )
{
    OUString result;

    if ( rResourceURL.startsWith( "private" ) &&
         m_xPersistentWindowState.is() &&
         m_xPersistentWindowState->hasByName( rResourceURL ) )
    {
        try
        {
            uno::Sequence< beans::PropertyValue > aProps;
            uno::Any a( m_xPersistentWindowState->getByName( rResourceURL ) );

            if ( a >>= aProps )
            {
                for ( sal_Int32 i = 0; i < aProps.getLength(); ++i )
                {
                    if ( aProps[ i ].Name.equalsAscii( ITEM_DESCRIPTOR_UINAME) )
                    {
                        aProps[ i ].Value >>= result;
                    }
                }
            }
        }
        catch ( uno::Exception& )
        {
            // do nothing, an empty UIName will be returned
        }
    }

    if ( rResourceURL.startsWith( ".uno" ) &&
         m_xCommandToLabelMap.is() &&
         m_xCommandToLabelMap->hasByName( rResourceURL ) )
    {
        uno::Any a;
        try
        {
            a = m_xCommandToLabelMap->getByName( rResourceURL );

            uno::Sequence< beans::PropertyValue > aPropSeq;
            if ( a >>= aPropSeq )
            {
                for ( sal_Int32 i = 0; i < aPropSeq.getLength(); ++i )
                {
                    if ( aPropSeq[i].Name.equalsAscii( ITEM_DESCRIPTOR_LABEL ) )
                    {
                        aPropSeq[i].Value >>= result;
                    }
                }
            }
        }
        catch ( uno::Exception& )
        {
            // not a system command name
        }
    }

    return result;
}

bool EntrySort( SvxConfigEntry* a, SvxConfigEntry* b )
{
    return a->GetName().compareTo( b->GetName() ) < 0;
}

SvxEntries* ToolbarSaveInData::GetEntries()
{
    typedef ::boost::unordered_map< OUString,
                             bool,
                             OUStringHash,
                             ::std::equal_to< OUString > > ToolbarInfo;

    ToolbarInfo aToolbarInfo;

    if ( pRootEntry == NULL )
    {

        pRootEntry = new SvxConfigEntry(
            OUString("MainToolbars"),
            OUString(), true);

        uno::Sequence< uno::Sequence < beans::PropertyValue > > info =
            GetConfigManager()->getUIElementsInfo(
                css::ui::UIElementType::TOOLBAR );

        for ( sal_Int32 i = 0; i < info.getLength(); ++i )
        {
            uno::Sequence< beans::PropertyValue > props = info[ i ];

            OUString url;
            OUString systemname;
            OUString uiname;

            for ( sal_Int32 j = 0; j < props.getLength(); ++j )
            {
                if ( props[ j ].Name.equalsAscii( ITEM_DESCRIPTOR_RESOURCEURL) )
                {
                    props[ j ].Value >>= url;
                    systemname = url.copy( url.lastIndexOf( '/' ) + 1 );
                }
                else if ( props[ j ].Name.equalsAscii( ITEM_DESCRIPTOR_UINAME) )
                {
                    props[ j ].Value >>= uiname;
                }
            }

            try
            {
                uno::Reference< container::XIndexAccess > xToolbarSettings =
                    GetConfigManager()->getSettings( url, sal_False );

                if ( uiname.isEmpty() )
                {
                    // try to get the name from m_xPersistentWindowState
                    uiname = GetSystemUIName( url );

                    if ( uiname.isEmpty() )
                    {
                        uiname = systemname;
                    }
                }

                SvxConfigEntry* pEntry = new SvxConfigEntry(
                    uiname, url, true );

                pEntry->SetMain( true );
                pEntry->SetStyle( GetSystemStyle( url ) );


                // insert into boost::unordered_map to filter duplicates from the parent
                aToolbarInfo.insert( ToolbarInfo::value_type( systemname, true ));

                OUString custom(CUSTOM_TOOLBAR_STR);
                if ( systemname.startsWith( custom ) )
                {
                    pEntry->SetUserDefined( true );
                }
                else
                {
                    pEntry->SetUserDefined( false );
                }

                pRootEntry->GetEntries()->push_back( pEntry );

                LoadToolbar( xToolbarSettings, pEntry );
            }
            catch ( container::NoSuchElementException& )
            {
                // TODO, handle resourceURL with no settings
            }
        }

        uno::Reference< css::ui::XUIConfigurationManager > xParentCfgMgr = GetParentConfigManager();
        if ( xParentCfgMgr.is() )
        {
            // Retrieve also the parent toolbars to make it possible
            // to configure module toolbars and save them into the document
            // config manager.
            uno::Sequence< uno::Sequence < beans::PropertyValue > > info_ =
                xParentCfgMgr->getUIElementsInfo(
                    css::ui::UIElementType::TOOLBAR );

            for ( sal_Int32 i = 0; i < info_.getLength(); ++i )
            {
                uno::Sequence< beans::PropertyValue > props = info_[ i ];

                OUString url;
                OUString systemname;
                OUString uiname;

                for ( sal_Int32 j = 0; j < props.getLength(); ++j )
                {
                    if ( props[ j ].Name.equalsAscii( ITEM_DESCRIPTOR_RESOURCEURL) )
                    {
                        props[ j ].Value >>= url;
                        systemname = url.copy( url.lastIndexOf( '/' ) + 1 );
                    }
                    else if ( props[ j ].Name.equalsAscii( ITEM_DESCRIPTOR_UINAME) )
                    {
                        props[ j ].Value >>= uiname;
                    }
                }

                // custom toolbars of the parent are not visible in the document layer
                OUString custom(CUSTOM_TOOLBAR_STR);
                if ( systemname.startsWith( custom ) )
                    continue;

                // check if toolbar is already in the document layer
                ToolbarInfo::const_iterator pIter = aToolbarInfo.find( systemname );
                if ( pIter == aToolbarInfo.end() )
                {
                    aToolbarInfo.insert( ToolbarInfo::value_type( systemname, true ));

                    try
                    {
                        uno::Reference< container::XIndexAccess > xToolbarSettings =
                            xParentCfgMgr->getSettings( url, sal_False );

                        if ( uiname.isEmpty() )
                        {
                            // try to get the name from m_xPersistentWindowState
                            uiname = GetSystemUIName( url );

                            if ( uiname.isEmpty() )
                            {
                                uiname = systemname;
                            }
                        }

                        SvxConfigEntry* pEntry = new SvxConfigEntry(
                            uiname, url, true, true );

                        pEntry->SetMain( true );
                        pEntry->SetStyle( GetSystemStyle( url ) );

                        if ( systemname.startsWith( custom ) )
                        {
                            pEntry->SetUserDefined( true );
                        }
                        else
                        {
                            pEntry->SetUserDefined( false );
                        }

                        pRootEntry->GetEntries()->push_back( pEntry );

                        LoadToolbar( xToolbarSettings, pEntry );
                    }
                    catch ( container::NoSuchElementException& )
                    {
                        // TODO, handle resourceURL with no settings
                    }
                }
            }
        }

        std::sort( GetEntries()->begin(), GetEntries()->end(), EntrySort );
    }

    return pRootEntry->GetEntries();
}

void
ToolbarSaveInData::SetEntries( SvxEntries* pNewEntries )
{
    // delete old menu hierarchy first
    delete pRootEntry->GetEntries();

    // now set new menu hierarchy
    pRootEntry->SetEntries( pNewEntries );
}

bool
ToolbarSaveInData::HasURL( const OUString& rURL )
{
    SvxEntries::const_iterator iter = GetEntries()->begin();
    SvxEntries::const_iterator end = GetEntries()->end();

    while ( iter != end )
    {
        SvxConfigEntry* pEntry = *iter;

        if ( pEntry->GetCommand().equals( rURL ) )
        {
            if ( pEntry->IsParentData() )
                return false;
            else
                return true;
        }

        ++iter;
    }
    return false;
}

bool ToolbarSaveInData::HasSettings()
{
    // return true if there is at least one toolbar entry
    if ( GetEntries()->size() > 0 )
    {
        return true;
    }
    return false;
}

void ToolbarSaveInData::Reset()
{
    SvxEntries::const_iterator toolbars = GetEntries()->begin();
    SvxEntries::const_iterator end = GetEntries()->end();

    // reset each toolbar by calling removeSettings for its toolbar URL
    for ( ; toolbars != end; ++toolbars )
    {
        SvxConfigEntry* pToolbar = *toolbars;

        try
        {
            OUString url = pToolbar->GetCommand();
            GetConfigManager()->removeSettings( url );
        }
        catch ( uno::Exception& )
        {
            // error occurred removing the settings
            // TODO - add error dialog in future?
        }
    }

    // persist changes to toolbar storage
    PersistChanges( GetConfigManager() );

    // now delete the root SvxConfigEntry the next call to GetEntries()
    // causes it to be reinitialised
    delete pRootEntry;
    pRootEntry = NULL;

    // reset all icons to default
    try
    {
        GetImageManager()->reset();
        PersistChanges( GetImageManager() );
    }
    catch ( uno::Exception& )
    {
        OSL_TRACE("Error resetting all icons when resetting toolbars");
    }
}

bool ToolbarSaveInData::Apply()
{
    // toolbar changes are instantly applied
    return false;
}

void ToolbarSaveInData::ApplyToolbar(
    uno::Reference< container::XIndexContainer >& rToolbarBar,
    uno::Reference< lang::XSingleComponentFactory >& rFactory,
    SvxConfigEntry* pToolbarData )
{
    uno::Reference<uno::XComponentContext> xContext = ::comphelper::getProcessComponentContext();

    SvxEntries::const_iterator iter = pToolbarData->GetEntries()->begin();
    SvxEntries::const_iterator end = pToolbarData->GetEntries()->end();

    for ( ; iter != end; ++iter )
    {
        SvxConfigEntry* pEntry = *iter;

        if ( pEntry->IsPopup() )
        {
            uno::Sequence< beans::PropertyValue > aPropValueSeq =
                ConvertToolbarEntry( m_xCommandToLabelMap, pEntry );

            uno::Reference< container::XIndexContainer > xSubMenuBar(
                rFactory->createInstanceWithContext( xContext ),
                    uno::UNO_QUERY );

            sal_Int32 nIndex = aPropValueSeq.getLength();
            aPropValueSeq.realloc( nIndex + 1 );
            aPropValueSeq[nIndex].Name = m_aDescriptorContainer;
            aPropValueSeq[nIndex].Value <<= xSubMenuBar;
            rToolbarBar->insertByIndex(
                rToolbarBar->getCount(), uno::makeAny( aPropValueSeq ));

            ApplyToolbar( xSubMenuBar, rFactory, pEntry );
        }
        else if ( pEntry->IsSeparator() )
        {
            rToolbarBar->insertByIndex(
                rToolbarBar->getCount(), uno::makeAny( m_aSeparatorSeq ));
        }
        else
        {
            uno::Sequence< beans::PropertyValue > aPropValueSeq =
                ConvertToolbarEntry( m_xCommandToLabelMap, pEntry );

            rToolbarBar->insertByIndex(
                rToolbarBar->getCount(), uno::makeAny( aPropValueSeq ));
        }
    }
}

void ToolbarSaveInData::ApplyToolbar( SvxConfigEntry* pToolbar )
{
    // Apply new toolbar structure to our settings container
    uno::Reference< container::XIndexAccess > xSettings(
        GetConfigManager()->createSettings(), uno::UNO_QUERY );

    uno::Reference< container::XIndexContainer > xIndexContainer (
        xSettings, uno::UNO_QUERY );

    uno::Reference< lang::XSingleComponentFactory > xFactory (
        xSettings, uno::UNO_QUERY );

    ApplyToolbar( xIndexContainer, xFactory, pToolbar );

    uno::Reference< beans::XPropertySet > xProps(
        xSettings, uno::UNO_QUERY );

    if ( pToolbar->IsUserDefined() )
    {
        xProps->setPropertyValue(
            OUString(ITEM_DESCRIPTOR_UINAME ),
            uno::makeAny( OUString( pToolbar->GetName() ) ) );
    }

    try
    {
        if ( GetConfigManager()->hasSettings( pToolbar->GetCommand() ) )
        {
            GetConfigManager()->replaceSettings(
                pToolbar->GetCommand(), xSettings );
        }
        else
        {
            GetConfigManager()->insertSettings(
                pToolbar->GetCommand(), xSettings );
            if ( pToolbar->IsParentData() )
                pToolbar->SetParentData( false );
        }
    }
    catch ( container::NoSuchElementException& )
    {
        OSL_TRACE("caught container::NoSuchElementException saving settings");
    }
    catch ( com::sun::star::io::IOException& )
    {
        OSL_TRACE("caught IOException saving settings");
    }
    catch ( com::sun::star::uno::Exception& )
    {
        OSL_TRACE("caught some other exception saving settings");
    }

    PersistChanges( GetConfigManager() );
}

void ToolbarSaveInData::CreateToolbar( SvxConfigEntry* pToolbar )
{
    // show the new toolbar in the UI also
    uno::Reference< container::XIndexAccess >
        xSettings( GetConfigManager()->createSettings(), uno::UNO_QUERY );

    uno::Reference< container::XIndexContainer >
        xIndexContainer ( xSettings, uno::UNO_QUERY );

    uno::Reference< beans::XPropertySet >
        xPropertySet( xSettings, uno::UNO_QUERY );

    xPropertySet->setPropertyValue(
        OUString(ITEM_DESCRIPTOR_UINAME ),
            uno::makeAny( pToolbar->GetName() ) );

    try
    {
        GetConfigManager()->insertSettings( pToolbar->GetCommand(), xSettings );
    }
    catch ( container::ElementExistException& )
    {
        OSL_TRACE("caught ElementExistsException saving settings");
    }
    catch ( com::sun::star::lang::IllegalArgumentException& )
    {
        OSL_TRACE("caught IOException saving settings");
    }
    catch ( com::sun::star::lang::IllegalAccessException& )
    {
        OSL_TRACE("caught IOException saving settings");
    }
    catch ( com::sun::star::uno::Exception& )
    {
        OSL_TRACE("caught some other exception saving settings");
    }

    GetEntries()->push_back( pToolbar );

    PersistChanges( GetConfigManager() );
}

void ToolbarSaveInData::RemoveToolbar( SvxConfigEntry* pToolbar )
{
    try
    {
        OUString url = pToolbar->GetCommand();
        GetConfigManager()->removeSettings( url );
        RemoveEntry( GetEntries(), pToolbar );
        delete pToolbar;

        PersistChanges( GetConfigManager() );

        // remove the persistent window state data
        css::uno::Reference< css::container::XNameContainer > xNameContainer(
            m_xPersistentWindowState, css::uno::UNO_QUERY_THROW );

        xNameContainer->removeByName( url );
    }
    catch ( uno::Exception& )
    {
        // error occurred removing the settings
    }
}

void ToolbarSaveInData::RestoreToolbar( SvxConfigEntry* pToolbar )
{
    OUString url = pToolbar->GetCommand();

    // Restore of toolbar is done by removing it from
    // it's configuration manager and then getting it again
    bool bParentToolbar = pToolbar->IsParentData();

    // Cannot restore parent toolbar
    if ( bParentToolbar )
        return;

    try
    {
        GetConfigManager()->removeSettings( url );
        pToolbar->GetEntries()->clear();
        PersistChanges( GetConfigManager() );
    }
    catch ( uno::Exception& )
    {
        // if an error occurs removing the settings then just return
        return;
    }

    // Now reload the toolbar settings
    try
    {
        uno::Reference< container::XIndexAccess > xToolbarSettings;
        if ( IsDocConfig() )
        {
            xToolbarSettings = GetParentConfigManager()->getSettings( url, sal_False );
            pToolbar->SetParentData( true );
        }
        else
            xToolbarSettings = GetConfigManager()->getSettings( url, sal_False );

        LoadToolbar( xToolbarSettings, pToolbar );

        // After reloading, ensure that the icon is reset of each entry
        // in the toolbar
        SvxEntries::const_iterator iter = pToolbar->GetEntries()->begin();
        uno::Sequence< OUString > aURLSeq( 1 );
        for ( ; iter != pToolbar->GetEntries()->end(); ++iter )
        {
            SvxConfigEntry* pEntry = *iter;
            aURLSeq[ 0 ] = pEntry->GetCommand();

            try
            {
                GetImageManager()->removeImages( GetImageType(), aURLSeq );
            }
               catch ( uno::Exception& )
               {
                OSL_TRACE("Error restoring icon when resetting toolbar");
               }
        }
        PersistChanges( GetImageManager() );
    }
    catch ( container::NoSuchElementException& )
    {
        // cannot find the resource URL after removing it
        // so no entry will appear in the toolbar list
    }
}

bool ToolbarSaveInData::LoadToolbar(
    const uno::Reference< container::XIndexAccess >& xToolbarSettings,
    SvxConfigEntry* pParentData )
{
    SvxEntries*         pEntries            = pParentData->GetEntries();

    for ( sal_Int32 nIndex = 0; nIndex < xToolbarSettings->getCount(); ++nIndex )
    {
        uno::Reference< container::XIndexAccess >   xSubMenu;
        OUString                aCommandURL;
        OUString                aLabel;
        bool                    bIsUserDefined = true;
        bool                bIsVisible;
        sal_Int32               nStyle;

        sal_uInt16 nType( css::ui::ItemType::DEFAULT );

        bool bItem = GetToolbarItemData( xToolbarSettings, nIndex, aCommandURL,
            aLabel, nType, bIsVisible, nStyle, xSubMenu );

        if ( bItem )
        {
            if ( nType == css::ui::ItemType::DEFAULT )
            {
                uno::Any a;
                try
                {
                    a = m_xCommandToLabelMap->getByName( aCommandURL );
                    bIsUserDefined = false;
                }
                catch ( container::NoSuchElementException& )
                {
                    bIsUserDefined = true;
                }

                // If custom label not set retrieve it from the command
                // to info service
                if ( aLabel.equals( OUString() ) )
                {
                    uno::Sequence< beans::PropertyValue > aPropSeq;
                    if ( a >>= aPropSeq )
                    {
                        for ( sal_Int32 i = 0; i < aPropSeq.getLength(); ++i )
                        {
                            if ( aPropSeq[i].Name.equalsAscii( ITEM_DESCRIPTOR_LABEL ) )
                            {
                                aPropSeq[i].Value >>= aLabel;
                                break;
                            }
                        }
                    }
                }

                if ( xSubMenu.is() )
                {
                    SvxConfigEntry* pEntry = new SvxConfigEntry(
                        aLabel, aCommandURL, true );

                    pEntry->SetUserDefined( bIsUserDefined );
                    pEntry->SetVisible( bIsVisible );

                    pEntries->push_back( pEntry );

                    LoadToolbar( xSubMenu, pEntry );
                }
                else
                {
                    SvxConfigEntry* pEntry = new SvxConfigEntry(
                        aLabel, aCommandURL, false );
                    pEntry->SetUserDefined( bIsUserDefined );
                    pEntry->SetVisible( bIsVisible );
                    pEntry->SetStyle( nStyle );
                    pEntries->push_back( pEntry );
                }
            }
            else
            {
                SvxConfigEntry* pEntry = new SvxConfigEntry;
                pEntry->SetUserDefined( bIsUserDefined );
                pEntries->push_back( pEntry );
            }
        }
    }

    return true;
}

IMPL_LINK( SvxToolbarConfigPage, SelectToolbarEntry, Control *, pBox )
{
    (void)pBox;
    UpdateButtonStates();
    return 1;
}

void SvxToolbarConfigPage::UpdateButtonStates()
{
    PopupMenu* pPopup = m_pModifyCommandButton->GetPopupMenu();
    pPopup->EnableItem( ID_RENAME, false );
    pPopup->EnableItem( ID_DELETE, false );
    pPopup->EnableItem( ID_BEGIN_GROUP, false );
    pPopup->EnableItem( ID_DEFAULT_COMMAND, false );
    pPopup->EnableItem( ID_ICON_ONLY, false );
    pPopup->EnableItem( ID_ICON_AND_TEXT, false );
    pPopup->EnableItem( ID_TEXT_ONLY, false );
    pPopup->EnableItem( ID_CHANGE_SYMBOL, false );
    pPopup->EnableItem( ID_RESET_SYMBOL, false );

    m_pDescriptionField->SetText("");

    SvTreeListEntry* selection = m_pContentsListBox->GetCurEntry();
    if ( m_pContentsListBox->GetEntryCount() == 0 || selection == NULL )
    {
        return;
    }

    SvxConfigEntry* pEntryData = (SvxConfigEntry*) selection->GetUserData();
    if ( pEntryData->IsSeparator() )
        pPopup->EnableItem( ID_DELETE, true );
    else
    {
        pPopup->EnableItem( ID_BEGIN_GROUP, true );
        pPopup->EnableItem( ID_DELETE, true );
        pPopup->EnableItem( ID_RENAME, true );
        pPopup->EnableItem( ID_ICON_ONLY, true );
        pPopup->EnableItem( ID_ICON_AND_TEXT, true );
        pPopup->EnableItem( ID_TEXT_ONLY, true );
        pPopup->EnableItem( ID_CHANGE_SYMBOL, true );

        if ( !pEntryData->IsUserDefined() )
            pPopup->EnableItem( ID_DEFAULT_COMMAND, true );

        if ( pEntryData->IsIconModified() )
            pPopup->EnableItem( ID_RESET_SYMBOL, true );

        m_pDescriptionField->SetText(pEntryData->GetHelpText());
    }
}

short SvxToolbarConfigPage::QueryReset()
{
    OUString msg = CUI_RES( RID_SVXSTR_CONFIRM_TOOLBAR_RESET );

    OUString saveInName = m_pSaveInListBox->GetEntry(
        m_pSaveInListBox->GetSelectEntryPos() );

    OUString label = replaceSaveInName( msg, saveInName );

    QueryBox qbox( this, WB_YES_NO, label );

    return qbox.Execute();
}

IMPL_LINK( SvxToolbarConfigPage, SelectToolbar, ListBox *, pBox )
{
    (void)pBox;

    m_pContentsListBox->Clear();

    SvxConfigEntry* pToolbar = GetTopLevelSelection();
    if ( pToolbar == NULL )
    {
        m_pModifyTopLevelButton->Enable( false );
        m_pModifyCommandButton->Enable( false );
        m_pAddCommandsButton->Enable( false );

        return 0;
    }

    m_pModifyTopLevelButton->Enable( true );
    m_pModifyCommandButton->Enable( true );
    m_pAddCommandsButton->Enable( true );

    PopupMenu* pPopup = m_pModifyTopLevelButton->GetPopupMenu();

    pPopup->EnableItem( ID_DELETE, pToolbar->IsDeletable() );
    pPopup->EnableItem( ID_RENAME, pToolbar->IsRenamable() );
    pPopup->EnableItem( ID_DEFAULT_STYLE, !pToolbar->IsRenamable() );

    switch( pToolbar->GetStyle() )
    {
        case 0:
        {
            pPopup->CheckItem( ID_ICONS_ONLY );
            break;
        }
        case 1:
        {
            pPopup->CheckItem( ID_TEXT_ONLY );
            break;
        }
        case 2:
        {
            pPopup->CheckItem( ID_ICONS_AND_TEXT );
            break;
        }
    }

    SvxEntries* pEntries = pToolbar->GetEntries();
    SvxEntries::const_iterator iter = pEntries->begin();

    for ( ; iter != pEntries->end(); ++iter )
    {
        SvxConfigEntry* pEntry = *iter;

        SvTreeListEntry* pNewLBEntry = InsertEntryIntoUI( pEntry );

        if(pEntry->IsSeparator())
            m_pContentsListBox->SetCheckButtonInvisible( pNewLBEntry );

        if (pEntry->IsBinding())
        {
            m_pContentsListBox->SetCheckButtonState( pNewLBEntry,
                pEntry->IsVisible() ? SV_BUTTON_CHECKED : SV_BUTTON_UNCHECKED );
        }
        else
        {
            m_pContentsListBox->SetCheckButtonState(
                pNewLBEntry, SV_BUTTON_TRISTATE );
        }
    }

    UpdateButtonStates();

    return 0;
}

IMPL_LINK( SvxToolbarConfigPage, NewToolbarHdl, Button *, pButton )
{
    (void)pButton;

    OUString prefix = CUI_RES( RID_SVXSTR_NEW_TOOLBAR );

    OUString aNewName =
        generateCustomName( prefix, GetSaveInData()->GetEntries() );

    OUString aNewURL =
        generateCustomURL( GetSaveInData()->GetEntries() );

    boost::scoped_ptr<SvxNewToolbarDialog> pNameDialog(new SvxNewToolbarDialog( 0, aNewName ));

    sal_uInt16 nInsertPos;
    for ( sal_uInt16 i = 0 ; i < m_pSaveInListBox->GetEntryCount(); ++i )
    {
        SaveInData* pData =
            (SaveInData*) m_pSaveInListBox->GetEntryData( i );

        nInsertPos = pNameDialog->m_pSaveInListBox->InsertEntry(
            m_pSaveInListBox->GetEntry( i ) );

        pNameDialog->m_pSaveInListBox->SetEntryData( nInsertPos, pData );
    }

    pNameDialog->m_pSaveInListBox->SelectEntryPos(
        m_pSaveInListBox->GetSelectEntryPos(), true );

    if ( pNameDialog->Execute() == RET_OK )
    {
        aNewName = pNameDialog->GetName();

        nInsertPos = pNameDialog->m_pSaveInListBox->GetSelectEntryPos();

        ToolbarSaveInData* pData = (ToolbarSaveInData*)
            pNameDialog->m_pSaveInListBox->GetEntryData( nInsertPos );

        if ( GetSaveInData() != pData )
        {
            m_pSaveInListBox->SelectEntryPos( nInsertPos, true );
            m_pSaveInListBox->GetSelectHdl().Call(this);
        }

        SvxConfigEntry* pToolbar =
            new SvxConfigEntry( aNewName, aNewURL, true );

        pToolbar->SetUserDefined( true );
        pToolbar->SetMain( true );

        pData->CreateToolbar( pToolbar );

        nInsertPos = m_pTopLevelListBox->InsertEntry( pToolbar->GetName() );
        m_pTopLevelListBox->SetEntryData( nInsertPos, pToolbar );
        m_pTopLevelListBox->SelectEntryPos( nInsertPos, true );
        m_pTopLevelListBox->GetSelectHdl().Call(this);

        pData->SetModified( true );
    }

    return 0;
}

IMPL_LINK( SvxToolbarConfigPage, AddCommandsHdl, Button *, pButton )
{
    (void)pButton;

    if ( m_pSelectorDlg == NULL )
    {
        // Create Script Selector which shows slot commands
        m_pSelectorDlg = new SvxScriptSelectorDialog( this, true, m_xFrame );

        // Position the Script Selector over the Add button so it is
        // beside the menu contents list and does not obscure it
        m_pSelectorDlg->SetPosPixel( m_pAddCommandsButton->GetPosPixel() );

        m_pSelectorDlg->SetAddHdl(
            LINK( this, SvxToolbarConfigPage, AddFunctionHdl ) );
    }

    m_pSelectorDlg->SetImageProvider(
        static_cast< ImageProvider* >( GetSaveInData() ) );

    m_pSelectorDlg->Show();
    return 1;
}

IMPL_LINK( SvxToolbarConfigPage, AddFunctionHdl,
    SvxScriptSelectorDialog *, pDialog )
{
    (void)pDialog;

    AddFunction();

    return 0;
}

SvTreeListEntry* SvxToolbarConfigPage::AddFunction(
    SvTreeListEntry* pTarget, bool bFront, bool bAllowDuplicates )
{
    SvTreeListEntry* pNewLBEntry =
        SvxConfigPage::AddFunction( pTarget, bFront, bAllowDuplicates );

    SvxConfigEntry* pEntry = (SvxConfigEntry*) pNewLBEntry->GetUserData();

    if ( pEntry->IsBinding() )
    {
        pEntry->SetVisible( true );
        m_pContentsListBox->SetCheckButtonState(
            pNewLBEntry, SV_BUTTON_CHECKED );
    }
    else
    {
        m_pContentsListBox->SetCheckButtonState(
            pNewLBEntry, SV_BUTTON_TRISTATE );
    }

    // get currently selected toolbar and apply change
    SvxConfigEntry* pToolbar = GetTopLevelSelection();

    if ( pToolbar != NULL )
    {
        ( ( ToolbarSaveInData* ) GetSaveInData() )->ApplyToolbar( pToolbar );
    }

    return pNewLBEntry;
}

SvxToolbarEntriesListBox::SvxToolbarEntriesListBox(Window* pParent, SvxToolbarConfigPage* pPg)
    : SvxMenuEntriesListBox(pParent, pPg)
    , pPage(pPg)
{
    m_pButtonData = new SvLBoxButtonData( this );
    BuildCheckBoxButtonImages( m_pButtonData );
    EnableCheckButton( m_pButtonData );
}

SvxToolbarEntriesListBox::~SvxToolbarEntriesListBox()
{
    delete m_pButtonData;
}

void SvxToolbarEntriesListBox::BuildCheckBoxButtonImages( SvLBoxButtonData* pData )
{
    // Build checkbox images according to the current application
    // settings. This is necessary to be able to have correct colors
    // in all color modes, like high contrast.
    const AllSettings& rSettings = Application::GetSettings();

    VirtualDevice   aDev;
    Size            aSize( 26, 20 );

    aDev.SetOutputSizePixel( aSize );

    Image aImage = GetSizedImage( aDev, aSize,
        CheckBox::GetCheckImage( rSettings, BUTTON_DRAW_DEFAULT ));

    // Fill button data struct with new images
    pData->aBmps[SV_BMP_UNCHECKED]      = aImage;
    pData->aBmps[SV_BMP_CHECKED]        = GetSizedImage( aDev, aSize, CheckBox::GetCheckImage( rSettings, BUTTON_DRAW_CHECKED ));
    pData->aBmps[SV_BMP_HICHECKED]      = GetSizedImage( aDev, aSize, CheckBox::GetCheckImage( rSettings, BUTTON_DRAW_CHECKED | BUTTON_DRAW_PRESSED ));
    pData->aBmps[SV_BMP_HIUNCHECKED]    = GetSizedImage( aDev, aSize, CheckBox::GetCheckImage( rSettings, BUTTON_DRAW_DEFAULT | BUTTON_DRAW_PRESSED));
    pData->aBmps[SV_BMP_TRISTATE]       = GetSizedImage( aDev, aSize, Image() ); // Use tristate bitmaps to have no checkbox for separator entries
    pData->aBmps[SV_BMP_HITRISTATE]     = GetSizedImage( aDev, aSize, Image() );

    // Get image size
    m_aCheckBoxImageSizePixel = aImage.GetSizePixel();
}

Image SvxToolbarEntriesListBox::GetSizedImage(
    VirtualDevice& aDev, const Size& aNewSize, const Image& aImage )
{
    // Create new checkbox images for treelistbox. They must have a
    // decent width to have a clear column for the visibility checkbox.

    // Standard transparent color is light magenta as is won't be
    // used for other things
    Color   aFillColor( COL_LIGHTMAGENTA );

    // Position image at the center of (width-2),(height) rectangle.
    // We need 2 pixels to have a bigger border to the next button image
    sal_uInt16  nPosX = std::max( (sal_uInt16) (((( aNewSize.Width() - 2 ) - aImage.GetSizePixel().Width() ) / 2 ) - 1), (sal_uInt16) 0 );
    sal_uInt16  nPosY = std::max( (sal_uInt16) (((( aNewSize.Height() - 2 ) - aImage.GetSizePixel().Height() ) / 2 ) + 1), (sal_uInt16) 0 );
    Point   aPos( nPosX > 0 ? nPosX : 0, nPosY > 0 ? nPosY : 0 );
    aDev.SetFillColor( aFillColor );
    aDev.SetLineColor( aFillColor );
    aDev.DrawRect( Rectangle( Point(), aNewSize ));
    aDev.DrawImage( aPos, aImage );

    // Draw separator line 2 pixels left from the right border
    Color aLineColor = GetDisplayBackground().GetColor().IsDark() ? Color( COL_WHITE ) : Color( COL_BLACK );
    aDev.SetLineColor( aLineColor );
    aDev.DrawLine( Point( aNewSize.Width()-3, 0 ), Point( aNewSize.Width()-3, aNewSize.Height()-1 ));

    // Create new image that uses the fillcolor as transparent
    return Image( aDev.GetBitmap( Point(), aNewSize ), aFillColor );
}

void SvxToolbarEntriesListBox::DataChanged( const DataChangedEvent& rDCEvt )
{
    SvTreeListBox::DataChanged( rDCEvt );

    if (( rDCEvt.GetType() == DATACHANGED_SETTINGS ) &&
        ( rDCEvt.GetFlags() & SETTINGS_STYLE ))
    {
        BuildCheckBoxButtonImages( m_pButtonData );
        Invalidate();
    }
}



void SvxToolbarEntriesListBox::ChangeVisibility( SvTreeListEntry* pEntry )
{
    if ( pEntry != NULL )
    {
        SvxConfigEntry* pEntryData =
            (SvxConfigEntry*) pEntry->GetUserData();

        if ( pEntryData->IsBinding() )
        {
            pEntryData->SetVisible( !pEntryData->IsVisible() );

            SvxConfigEntry* pToolbar = pPage->GetTopLevelSelection();

            ToolbarSaveInData* pToolbarSaveInData = ( ToolbarSaveInData* )
                pPage->GetSaveInData();

               pToolbarSaveInData->ApplyToolbar( pToolbar );

            SetCheckButtonState( pEntry, pEntryData->IsVisible() ?
                SV_BUTTON_CHECKED : SV_BUTTON_UNCHECKED );
        }
    }
}

void SvxToolbarEntriesListBox::CheckButtonHdl()
{
    ChangeVisibility( GetHdlEntry() );
}

void SvxToolbarEntriesListBox::KeyInput( const KeyEvent& rKeyEvent )
{
    // space key will change visibility of toolbar items
    if ( rKeyEvent.GetKeyCode() == KEY_SPACE )
    {
        ChangeVisibility( GetCurEntry() );
    }
    else
    {
        // pass on to superclass
        SvxMenuEntriesListBox::KeyInput( rKeyEvent );
    }
}

TriState SvxToolbarEntriesListBox::NotifyMoving(
    SvTreeListEntry* pTarget, SvTreeListEntry* pSource,
    SvTreeListEntry*& rpNewParent, sal_uLong& rNewChildPos)
{
    TriState result = SvxMenuEntriesListBox::NotifyMoving(
        pTarget, pSource, rpNewParent, rNewChildPos );

    if ( result )
    {
        // Instant Apply changes to UI
        SvxConfigEntry* pToolbar = pPage->GetTopLevelSelection();
        if ( pToolbar != NULL )
        {
            ToolbarSaveInData* pSaveInData =
                ( ToolbarSaveInData*) pPage->GetSaveInData();
            pSaveInData->ApplyToolbar( pToolbar );
        }
    }

    return result;
}

TriState SvxToolbarEntriesListBox::NotifyCopying(
    SvTreeListEntry*  pTarget,
    SvTreeListEntry*  pSource,
    SvTreeListEntry*& rpNewParent,
    sal_uLong&      rNewChildPos)
{
    (void)pSource;
    (void)rpNewParent;
    (void)rNewChildPos;

    if ( !m_bIsInternalDrag )
    {
        // if the target is NULL then add function to the start of the list
        ((SvxToolbarConfigPage*)pPage)->AddFunction( pTarget, pTarget == NULL );

        // Instant Apply changes to UI
        SvxConfigEntry* pToolbar = pPage->GetTopLevelSelection();
        if ( pToolbar != NULL )
        {
            ToolbarSaveInData* pSaveInData =
                ( ToolbarSaveInData*) pPage->GetSaveInData();
            pSaveInData->ApplyToolbar( pToolbar );
        }

        // AddFunction already adds the listbox entry so return TRISTATE_FALSE
        // to stop another listbox entry being added
        return TRISTATE_FALSE;
    }

    // Copying is only allowed from external controls, not within the listbox
    return TRISTATE_FALSE;
}

SvxNewToolbarDialog::SvxNewToolbarDialog(Window* pWindow, const OUString& rName)
    : ModalDialog(pWindow, "NewToolbarDialog", "cui/ui/newtoolbardialog.ui")
{
    get(m_pEdtName, "edit");
    get(m_pBtnOK, "ok");
    get(m_pSaveInListBox, "savein");
    m_pEdtName->SetText( rName );
    m_pEdtName->SetSelection(Selection(SELECTION_MIN, SELECTION_MAX));
    ModifyHdl(m_pEdtName);
    m_pEdtName->SetModifyHdl(LINK(this, SvxNewToolbarDialog, ModifyHdl));
}

IMPL_LINK(SvxNewToolbarDialog, ModifyHdl, Edit*, pEdit)
{
    (void)pEdit;

    if(aCheckNameHdl.IsSet())
        m_pBtnOK->Enable(aCheckNameHdl.Call(this) > 0);

    return 0;
}

/*******************************************************************************
*
* The SvxIconSelectorDialog class
*
*******************************************************************************/
SvxIconSelectorDialog::SvxIconSelectorDialog( Window *pWindow,
    const uno::Reference< css::ui::XImageManager >& rXImageManager,
    const uno::Reference< css::ui::XImageManager >& rXParentImageManager )
    :
    ModalDialog          ( pWindow, "IconSelector", "cui/ui/iconselectordialog.ui" ),
    m_nNextId            ( 0 ),
    m_xImageManager      ( rXImageManager ),
    m_xParentImageManager( rXParentImageManager )
{
    get(pTbSymbol, "symbolsToolbar");
    get(pFtNote, "noteLabel");
    get(pBtnImport, "importButton");
    get(pBtnDelete, "deleteButton");

    aTbSize = pTbSymbol->LogicToPixel(Size(160, 80), MapMode(MAP_APPFONT));
    pTbSymbol->set_width_request(aTbSize.Width());
    pTbSymbol->set_height_request(aTbSize.Height());
    pTbSymbol->SetStyle(pTbSymbol->GetStyle() | WB_SCROLL | WB_LINESPACING);

    typedef ::boost::unordered_map< OUString,
                             bool,
                             OUStringHash,
                             ::std::equal_to< OUString > > ImageInfo;

    pTbSymbol->SetPageScroll( true );

    bool bLargeIcons = GetImageType() & css::ui::ImageType::SIZE_LARGE;
    m_nExpectedSize = bLargeIcons ? 26 : 16;

    if ( m_nExpectedSize != 16 )
    {
        pFtNote->SetText( replaceSixteen( pFtNote->GetText(), m_nExpectedSize ) );
    }

    uno::Reference< uno::XComponentContext > xComponentContext =
        ::comphelper::getProcessComponentContext();

    m_xGraphProvider = uno::Reference< graphic::XGraphicProvider >(
        graphic::GraphicProvider::create( xComponentContext ) );

    uno::Reference< css::util::XPathSettings > xPathSettings =
        css::util::thePathSettings::get( xComponentContext );


    OUString aDirectory = xPathSettings->getUserConfig();

    sal_Int32 aCount = aDirectory.getLength();

    if ( aCount > 0 )
    {
        sal_Unicode aChar = aDirectory[ aCount-1 ];
        if ( aChar != '/')
        {
            aDirectory += "/";
        }
    }
    else
    {
        pBtnImport->Enable( false );
    }

    aDirectory += "soffice.cfg/import";

    uno::Reference< lang::XSingleServiceFactory > xStorageFactory(
          ::com::sun::star::embed::FileSystemStorageFactory::create( xComponentContext ) );

    uno::Sequence< uno::Any > aArgs( 2 );
    aArgs[ 0 ] <<= aDirectory;
    aArgs[ 1 ] <<= com::sun::star::embed::ElementModes::READWRITE;

    uno::Reference< com::sun::star::embed::XStorage > xStorage(
        xStorageFactory->createInstanceWithArguments( aArgs ), uno::UNO_QUERY );

    uno::Sequence< uno::Any > aProp( 2 );
    beans::PropertyValue aPropValue;

    aPropValue.Name = "UserConfigStorage";
    aPropValue.Value <<= xStorage;
    aProp[ 0 ] <<= aPropValue;

    aPropValue.Name = "OpenMode";
    aPropValue.Value <<= com::sun::star::embed::ElementModes::READWRITE;
    aProp[ 1 ] <<= aPropValue;

    m_xImportedImageManager = css::ui::ImageManager::create( xComponentContext );

    ImageInfo mImageInfo;
    uno::Sequence< OUString > names;
    if ( m_xImportedImageManager.is() )
    {
        names = m_xImportedImageManager->getAllImageNames( GetImageType() );
        for ( sal_Int32 n = 0; n < names.getLength(); ++n )
            mImageInfo.insert( ImageInfo::value_type( names[n], false ));
    }
    sal_uInt16 nId = 1;
    ImageInfo::const_iterator pConstIter = mImageInfo.begin();
    uno::Sequence< OUString > name( 1 );
    while ( pConstIter != mImageInfo.end() )
    {
        name[ 0 ] = pConstIter->first;
        uno::Sequence< uno::Reference< graphic::XGraphic> > graphics = m_xImportedImageManager->getImages( GetImageType(), name );
        if ( graphics.getLength() > 0 )
        {
            Image img = Image( graphics[ 0 ] );
            pTbSymbol->InsertItem( nId, img, pConstIter->first );

            graphics[ 0 ]->acquire();

            pTbSymbol->SetItemData(
                nId, static_cast< void * > ( graphics[ 0 ].get() ) );

            ++nId;
        }
        ++pConstIter;
    }

    ImageInfo                 aImageInfo;

    if ( m_xParentImageManager.is() )
    {
        names = m_xParentImageManager->getAllImageNames( GetImageType() );
        for ( sal_Int32 n = 0; n < names.getLength(); ++n )
            aImageInfo.insert( ImageInfo::value_type( names[n], false ));
    }

    names = m_xImageManager->getAllImageNames( GetImageType() );
    for ( sal_Int32 n = 0; n < names.getLength(); ++n )
    {
        ImageInfo::iterator pIter = aImageInfo.find( names[n] );
        if ( pIter != aImageInfo.end() )
            pIter->second = true;
        else
            aImageInfo.insert( ImageInfo::value_type( names[n], true ));
    }

    // large growth factor, expecting many entries
    pConstIter = aImageInfo.begin();
    while ( pConstIter != aImageInfo.end() )
    {
        name[ 0 ] = pConstIter->first;

        uno::Sequence< uno::Reference< graphic::XGraphic> > graphics;
        try
        {
            if ( pConstIter->second )
                graphics = m_xImageManager->getImages( GetImageType(), name );
            else
                graphics = m_xParentImageManager->getImages( GetImageType(), name );
        }
        catch ( uno::Exception& )
        {
            // can't get sequence for this name so it will not be
            // added to the list
        }

        if ( graphics.getLength() > 0 )
        {
            Image img = Image( graphics[ 0 ] );
            pTbSymbol->InsertItem( nId, img, pConstIter->first );

            uno::Reference< graphic::XGraphic > xGraphic = graphics[ 0 ];

            if ( xGraphic.is() )
                xGraphic->acquire();

            pTbSymbol->SetItemData(
                nId, static_cast< void * > ( xGraphic.get() ) );

            ++nId;
        }

        ++pConstIter;
    }

    pBtnDelete->Enable( false );
    pTbSymbol->SetSelectHdl( LINK(this, SvxIconSelectorDialog, SelectHdl) );
    pBtnImport->SetClickHdl( LINK(this, SvxIconSelectorDialog, ImportHdl) );
    pBtnDelete->SetClickHdl( LINK(this, SvxIconSelectorDialog, DeleteHdl) );

    m_nNextId = pTbSymbol->GetItemCount()+1;
}

SvxIconSelectorDialog::~SvxIconSelectorDialog()
{
    sal_uInt16 nCount = pTbSymbol->GetItemCount();

    for (sal_uInt16 n = 0; n < nCount; ++n )
    {
        sal_uInt16 nId = pTbSymbol->GetItemId(n);

        uno::XInterface* xi = static_cast< uno::XInterface* >(
            pTbSymbol->GetItemData( nId ) );

        if ( xi != NULL )
        {
            xi->release();
        }
    }
}

uno::Reference< graphic::XGraphic> SvxIconSelectorDialog::GetSelectedIcon()
{
    uno::Reference< graphic::XGraphic > result;

    sal_uInt16 nId;
    for ( sal_uInt16 n = 0; n < pTbSymbol->GetItemCount(); ++n )
    {
        nId = pTbSymbol->GetItemId( n );
        if ( pTbSymbol->IsItemChecked( nId ) )
        {
            result = uno::Reference< graphic::XGraphic >(
                reinterpret_cast< graphic::XGraphic* >(
                    pTbSymbol->GetItemData( nId ) ) );
        }
    }

    return result;
}

IMPL_LINK( SvxIconSelectorDialog, SelectHdl, ToolBox *, pToolBox )
{
    (void)pToolBox;

    sal_uInt16 nCount = pTbSymbol->GetItemCount();

    for (sal_uInt16 n = 0; n < nCount; ++n )
    {
        sal_uInt16 nId = pTbSymbol->GetItemId( n );

        if ( pTbSymbol->IsItemChecked( nId ) )
        {
            pTbSymbol->CheckItem( nId, false );
        }
    }

    sal_uInt16 nId = pTbSymbol->GetCurItemId();
    pTbSymbol->CheckItem( nId );

    OUString aSelImageText = pTbSymbol->GetItemText( nId );
    if ( m_xImportedImageManager->hasImage( GetImageType(), aSelImageText ) )
    {
        pBtnDelete->Enable( true );
    }
    else
    {
        pBtnDelete->Enable( false );
    }

    return 0;
}

IMPL_LINK( SvxIconSelectorDialog, ImportHdl, PushButton *, pButton )
{
    (void)pButton;

    sfx2::FileDialogHelper aImportDialog(
        css::ui::dialogs::TemplateDescription::FILEOPEN_LINK_PREVIEW,
        SFXWB_GRAPHIC | SFXWB_MULTISELECTION );

    // disable the link checkbox in the dialog
    uno::Reference< css::ui::dialogs::XFilePickerControlAccess >
        xController( aImportDialog.GetFilePicker(), uno::UNO_QUERY);
    if ( xController.is() )
    {
        xController->enableControl(
            css::ui::dialogs::ExtendedFilePickerElementIds::CHECKBOX_LINK,
            sal_False);
    }

    aImportDialog.SetCurrentFilter(
        OUString("PNG - Portable Network Graphic"));

    if ( ERRCODE_NONE == aImportDialog.Execute() )
    {
        uno::Sequence< OUString > paths = aImportDialog.GetMPath();
        ImportGraphics ( paths );
    }

    return 0;
}

IMPL_LINK( SvxIconSelectorDialog, DeleteHdl, PushButton *, pButton )
{
    (void)pButton;

    OUString message = CUI_RES( RID_SVXSTR_DELETE_ICON_CONFIRM );
    if ( WarningBox( this, WinBits(WB_OK_CANCEL), message ).Execute() == RET_OK )
    {
        sal_uInt16 nCount = pTbSymbol->GetItemCount();

        for (sal_uInt16 n = 0; n < nCount; ++n )
        {
            sal_uInt16 nId = pTbSymbol->GetItemId( n );

            if ( pTbSymbol->IsItemChecked( nId ) )
            {
                OUString aSelImageText = pTbSymbol->GetItemText( nId );
                uno::Sequence< OUString > URLs(1);
                URLs[0] = aSelImageText;
                pTbSymbol->RemoveItem( pTbSymbol->GetItemPos( nId ) );
                m_xImportedImageManager->removeImages( GetImageType(), URLs );
                uno::Reference< css::ui::XUIConfigurationPersistence >
                    xConfigPersistence( m_xImportedImageManager, uno::UNO_QUERY );
                if ( xConfigPersistence.is() && xConfigPersistence->isModified() )
                {
                    xConfigPersistence->store();
                }
                break;
            }
        }
    }
    return 0;
}

bool SvxIconSelectorDialog::ReplaceGraphicItem(
    const OUString& aURL )
{
    uno::Sequence< OUString > URLs(1);
    uno::Sequence< uno::Reference<graphic::XGraphic > > aImportGraph( 1 );
    uno::Reference< css::ui::XUIConfigurationPersistence >
        xConfigPer( m_xImportedImageManager, uno::UNO_QUERY );

    uno::Reference< graphic::XGraphic > xGraphic;
    uno::Sequence< beans::PropertyValue > aMediaProps( 1 );
    aMediaProps[0].Name = "URL";
    aMediaProps[0].Value <<= aURL;

    com::sun::star::awt::Size aSize;
    bool bOK = false;
    try
    {
        xGraphic = m_xGraphProvider->queryGraphic( aMediaProps );

        uno::Reference< beans::XPropertySet > props =
            m_xGraphProvider->queryGraphicDescriptor( aMediaProps );
        uno::Any a = props->getPropertyValue(
            OUString("SizePixel") );
        a >>= aSize;
        if (0 == aSize.Width || 0 == aSize.Height)
            return false;
        else
            bOK = true;
    }
    catch ( uno::Exception& )
    {
        return false;
    }

    bool   bResult( false );
    sal_uInt16 nCount = pTbSymbol->GetItemCount();
    for (sal_uInt16 n = 0; n < nCount; ++n )
    {
        sal_uInt16 nId = pTbSymbol->GetItemId( n );

        if ( OUString( pTbSymbol->GetItemText( nId ) ) == aURL )
        {
            try
            {
                // replace/insert image with provided URL
                pTbSymbol->RemoveItem( pTbSymbol->GetItemPos( nId ) );
                aMediaProps[0].Value <<= aURL;

                Image aImage( xGraphic );
                if ( bOK && ((aSize.Width != m_nExpectedSize) || (aSize.Height != m_nExpectedSize)) )
                {
                    BitmapEx aBitmap = aImage.GetBitmapEx();
                    BitmapEx aBitmapex = BitmapEx::AutoScaleBitmap(aBitmap, m_nExpectedSize);
                    aImage = Image( aBitmapex);
                }
                pTbSymbol->InsertItem( nId,aImage, aURL, 0, 0 ); //modify

                xGraphic = aImage.GetXGraphic();

                URLs[0] = aURL;
                aImportGraph[ 0 ] = xGraphic;
                m_xImportedImageManager->replaceImages( GetImageType(), URLs, aImportGraph );
                xConfigPer->store();

                bResult = true;
                break;
            }
            catch ( ::com::sun::star::uno::Exception& )
            {
                break;
            }
        }
    }

    return bResult;
}

void SvxIconSelectorDialog::ImportGraphics(
    const uno::Sequence< OUString >& rPaths )
{
    uno::Sequence< OUString > rejected( rPaths.getLength() );
    sal_Int32 rejectedCount = 0;

    sal_uInt16 ret = 0;
    sal_Int32 aIndex;
    OUString aIconName;
    uno::Sequence< beans::PropertyValue > aMediaProps( 1 );
    aMediaProps[0].Name = "URL";
    uno::Reference< css::ui::XUIConfigurationPersistence >
        xConfigPer( m_xImportedImageManager, uno::UNO_QUERY );

    if ( rPaths.getLength() == 1 )
    {
        if ( m_xImportedImageManager->hasImage( GetImageType(), rPaths[0] ) )
        {
            aIndex = rPaths[0].lastIndexOf( '/' );
            aIconName = rPaths[0].copy( aIndex+1 );
            ret = SvxIconReplacementDialog( this, aIconName ).ShowDialog();
            if ( ret == 2 )
            {
                ReplaceGraphicItem( rPaths[0] );
            }
        }
        else
        {
            if ( !ImportGraphic( rPaths[0] ) )
            {
                rejected[0] = rPaths[0];
                rejectedCount = 1;
            }
        }
    }
    else
    {
        OUString aSourcePath( rPaths[0] );
        if ( rPaths[0].lastIndexOf( '/' ) != rPaths[0].getLength() -1 )
            aSourcePath = rPaths[0] + "/";

        for ( sal_Int32 i = 1; i < rPaths.getLength(); ++i )
        {
            OUString aPath = aSourcePath + rPaths[i];
            if ( m_xImportedImageManager->hasImage( GetImageType(), aPath ) )
            {
                aIndex = rPaths[i].lastIndexOf( '/' );
                aIconName = rPaths[i].copy( aIndex+1 );
                ret = SvxIconReplacementDialog( this, aIconName, true ).ShowDialog();
                if ( ret == 2 )
                {
                    ReplaceGraphicItem( aPath );
                }
                else if ( ret == 5 )
                {
                    for ( sal_Int32 k = i; k < rPaths.getLength(); ++k )
                    {
                        aPath = aSourcePath + rPaths[k];
                        bool bHasReplaced = ReplaceGraphicItem( aPath );

                        if ( !bHasReplaced )
                        {
                            bool result = ImportGraphic( aPath );
                            if ( !result )
                            {
                                rejected[ rejectedCount ] = rPaths[i];
                                ++rejectedCount;
                            }
                        }
                    }
                    break;
                }
            }
            else
            {
                bool result = ImportGraphic( aSourcePath + rPaths[i] );
                if ( !result )
                {
                    rejected[ rejectedCount ] = rPaths[i];
                    ++rejectedCount;
                }
            }
        }
    }

    if ( rejectedCount != 0 )
    {
        OUString message;
        OUString newLine("\n");
        OUString fPath;
        if (rejectedCount > 1)
              fPath = rPaths[0].copy(8) + "/";
        for ( sal_Int32 i = 0; i < rejectedCount; ++i )
        {
            message += fPath + rejected[i];
            message += newLine;
        }

        SvxIconChangeDialog aDialog(this, message);
        aDialog.Execute();
    }
}

bool SvxIconSelectorDialog::ImportGraphic( const OUString& aURL )
{
    bool result = false;

    sal_uInt16 nId = m_nNextId;
    ++m_nNextId;

    uno::Sequence< beans::PropertyValue > aMediaProps( 1 );
    aMediaProps[0].Name = "URL";

    uno::Reference< graphic::XGraphic > xGraphic;
    com::sun::star::awt::Size aSize;
    bool bOK = true;
    aMediaProps[0].Value <<= aURL;
    try
    {
        uno::Reference< beans::XPropertySet > props =
            m_xGraphProvider->queryGraphicDescriptor( aMediaProps );

        uno::Any a = props->getPropertyValue(
            OUString("SizePixel") );

            xGraphic = m_xGraphProvider->queryGraphic( aMediaProps );
            if ( xGraphic.is() )
            {
                a >>= aSize;
                if ( 0 == aSize.Width || 0 == aSize.Height )
                    bOK = false;

                Image aImage( xGraphic );

                if ( bOK && ((aSize.Width != m_nExpectedSize) || (aSize.Height != m_nExpectedSize)) )
                {
                    BitmapEx aBitmap = aImage.GetBitmapEx();
                    BitmapEx aBitmapex = BitmapEx::AutoScaleBitmap(aBitmap, m_nExpectedSize);
                    aImage = Image( aBitmapex);
                }
                if ( bOK && !!aImage )
                {
                    pTbSymbol->InsertItem( nId, aImage, aURL, 0, 0 );

                    xGraphic = aImage.GetXGraphic();
                    xGraphic->acquire();

                    pTbSymbol->SetItemData(
                        nId, static_cast< void * > ( xGraphic.get() ) );
                    uno::Sequence< OUString > aImportURL( 1 );
                    aImportURL[ 0 ] = aURL;
                    uno::Sequence< uno::Reference<graphic::XGraphic > > aImportGraph( 1 );
                    aImportGraph[ 0 ] = xGraphic;
                    m_xImportedImageManager->insertImages( GetImageType(), aImportURL, aImportGraph );
                    uno::Reference< css::ui::XUIConfigurationPersistence >
                    xConfigPersistence( m_xImportedImageManager, uno::UNO_QUERY );

                    if ( xConfigPersistence.is() && xConfigPersistence->isModified() )
                    {
                        xConfigPersistence->store();
                    }

                    result = true;
                }
                else
                {
                    OSL_TRACE("could not create Image from XGraphic");
                }
            }
            else
            {
                OSL_TRACE("could not get query XGraphic");
            }
    }
    catch( uno::Exception& e )
    {
        OSL_TRACE("Caught exception importing XGraphic: %s", PRTSTR(e.Message));
    }
    return result;
}

/*******************************************************************************
*
* The SvxIconReplacementDialog class
*
*******************************************************************************/
SvxIconReplacementDialog :: SvxIconReplacementDialog(
    Window *pWindow, const OUString& aMessage, bool /*bYestoAll*/ )
    :
MessBox( pWindow, WB_DEF_YES, CUI_RES( RID_SVXSTR_REPLACE_ICON_CONFIRM ),  CUI_RES( RID_SVXSTR_REPLACE_ICON_WARNING ) )

{
    SetImage( WarningBox::GetStandardImage() );
    SetMessText( ReplaceIconName( aMessage ) );
    RemoveButton( 1 );
    AddButton( BUTTON_YES, 2, 0 );
    AddButton( CUI_RES( RID_SVXSTR_YESTOALL ), 5, 0 );
    AddButton( BUTTON_NO, 3, 0 );
    AddButton( BUTTON_CANCEL, 4, 0 );
}

SvxIconReplacementDialog :: SvxIconReplacementDialog(
    Window *pWindow, const OUString& aMessage )
    :
MessBox( pWindow, WB_YES_NO_CANCEL, CUI_RES( RID_SVXSTR_REPLACE_ICON_CONFIRM ),  CUI_RES( RID_SVXSTR_REPLACE_ICON_WARNING ) )
{
    SetImage( WarningBox::GetStandardImage() );
    SetMessText( ReplaceIconName( aMessage ));
}

OUString SvxIconReplacementDialog :: ReplaceIconName( const OUString& rMessage )
{
    OUString name;
    OUString message = CUI_RES( RID_SVXSTR_REPLACE_ICON_WARNING );
    OUString placeholder("%ICONNAME" );
    sal_Int32 pos = message.indexOf( placeholder );
    if ( pos != -1 )
    {
        name = message.replaceAt(
            pos, placeholder.getLength(), rMessage );
    }
    return name;
}

sal_uInt16 SvxIconReplacementDialog :: ShowDialog()
{
    this->Execute();
    return ( this->GetCurButtonId() );
}
/*******************************************************************************
*
* The SvxIconChangeDialog class added for issue83555
*
*******************************************************************************/
SvxIconChangeDialog::SvxIconChangeDialog(
    Window *pWindow, const OUString& aMessage)
    :ModalDialog(pWindow, "IconChange", "cui/ui/iconchangedialog.ui")
{
    get(pFImageInfo, "infoImage");
    get(pLineEditDescription, "addrTextview");

    Size aSize(LogicToPixel(Size(140, 83), MapMode(MAP_APPFONT)));
    pLineEditDescription->set_width_request(aSize.Width());
    pLineEditDescription->set_height_request(aSize.Height());

    pFImageInfo->SetImage(InfoBox::GetStandardImage());
    pLineEditDescription->SetControlBackground( GetSettings().GetStyleSettings().GetDialogColor() );
    pLineEditDescription->SetText(aMessage);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
