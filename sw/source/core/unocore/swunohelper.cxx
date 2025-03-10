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

#include <com/sun/star/uno/Sequence.h>
#include <com/sun/star/uno/Exception.hpp>
#include <com/sun/star/ucb/UniversalContentBroker.hpp>
#include <com/sun/star/ucb/XContentIdentifier.hpp>
#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <com/sun/star/ucb/TransferInfo.hpp>
#include <com/sun/star/ucb/NameClash.hpp>
#include <com/sun/star/sdbc/XResultSet.hpp>
#include <com/sun/star/sdbc/XRow.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <comphelper/processfactory.hxx>
#include <comphelper/types.hxx>
#include <tools/urlobj.hxx>
#include <tools/datetime.hxx>
#include <rtl/ustring.hxx>
#include <ucbhelper/contentidentifier.hxx>
#include <ucbhelper/content.hxx>
#include <swunohelper.hxx>

//UUUU
#include <svx/xfillit0.hxx>
#include <svl/itemset.hxx>

using namespace com::sun::star;

namespace SWUnoHelper
{

sal_Int32 GetEnumAsInt32( const ::com::sun::star::uno::Any& rVal )
{
    sal_Int32 eVal;
    try
    {
        eVal = comphelper::getEnumAsINT32( rVal );
    }
    catch( ::com::sun::star::uno::Exception & )
    {
        eVal = 0;
        OSL_FAIL( "can't get EnumAsInt32" );
    }
    return eVal;
}

// methods for UCB actions
bool UCB_DeleteFile( const OUString& rURL )
{
    bool bRemoved;
    try
    {
        ucbhelper::Content aTempContent( rURL,
                                ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XCommandEnvironment >(),
                                comphelper::getProcessComponentContext() );
        aTempContent.executeCommand("delete",
                        ::com::sun::star::uno::makeAny( sal_True ) );
        bRemoved = true;
    }
    catch( ::com::sun::star::uno::Exception& )
    {
        bRemoved = false;
        OSL_FAIL( "Exeception from executeCommand( delete )" );
    }
    return bRemoved;
}

bool UCB_CopyFile( const OUString& rURL, const OUString& rNewURL, bool bCopyIsMove )
{
    bool bCopyCompleted = true;
    try
    {
        INetURLObject aURL( rNewURL );
        const OUString sName( aURL.GetName() );
        aURL.removeSegment();
        const OUString sMainURL( aURL.GetMainURL(INetURLObject::NO_DECODE) );

        ucbhelper::Content aTempContent( sMainURL,
                                ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XCommandEnvironment >(),
                                comphelper::getProcessComponentContext() );

        ::com::sun::star::uno::Any aAny;
        ::com::sun::star::ucb::TransferInfo aInfo;
        aInfo.NameClash = ::com::sun::star::ucb::NameClash::ERROR;
        aInfo.NewTitle = sName;
        aInfo.SourceURL = rURL;
        aInfo.MoveData = bCopyIsMove;
        aAny <<= aInfo;
        aTempContent.executeCommand( "transfer", aAny );
    }
    catch( ::com::sun::star::uno::Exception& )
    {
        OSL_FAIL( "Exeception from executeCommand( transfer )" );
        bCopyCompleted = false;
    }
    return bCopyCompleted;
}

bool UCB_IsCaseSensitiveFileName( const OUString& rURL )
{
    bool bCaseSensitive;
    try
    {
        INetURLObject aTempObj( rURL );
        aTempObj.SetBase( aTempObj.GetBase().toAsciiLowerCase() );
        ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XContentIdentifier > xRef1 = new
                ucbhelper::ContentIdentifier( aTempObj.GetMainURL( INetURLObject::NO_DECODE ));

        aTempObj.SetBase(aTempObj.GetBase().toAsciiUpperCase());
        ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XContentIdentifier > xRef2 = new
                ucbhelper::ContentIdentifier( aTempObj.GetMainURL( INetURLObject::NO_DECODE ));

        ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XUniversalContentBroker > xUcb =
              com::sun::star::ucb::UniversalContentBroker::create(comphelper::getProcessComponentContext());

        sal_Int32 nCompare = xUcb->compareContentIds( xRef1, xRef2 );
        bCaseSensitive = 0 != nCompare;
    }
    catch( ::com::sun::star::uno::Exception& )
    {
        bCaseSensitive = false;
        OSL_FAIL( "Exeception from compareContentIds()" );
    }
    return bCaseSensitive;
}

bool UCB_IsReadOnlyFileName( const OUString& rURL )
{
    bool bIsReadOnly = false;
    try
    {
        ucbhelper::Content aCnt( rURL, ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XCommandEnvironment >(), comphelper::getProcessComponentContext() );
        ::com::sun::star::uno::Any aAny = aCnt.getPropertyValue("IsReadOnly");
        if(aAny.hasValue())
            bIsReadOnly = *(sal_Bool*)aAny.getValue();
    }
    catch( ::com::sun::star::uno::Exception& )
    {
        bIsReadOnly = false;
    }
    return bIsReadOnly;
}

bool UCB_IsFile( const OUString& rURL )
{
    bool bExists = false;
    try
    {
        ::ucbhelper::Content aContent( rURL, ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XCommandEnvironment >(), comphelper::getProcessComponentContext() );
        bExists = aContent.isDocument();
    }
    catch (::com::sun::star::uno::Exception &)
    {
    }
    return bExists;
}

bool UCB_IsDirectory( const OUString& rURL )
{
    bool bExists = false;
    try
    {
        ::ucbhelper::Content aContent( rURL, ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XCommandEnvironment >(), comphelper::getProcessComponentContext() );
        bExists = aContent.isFolder();
    }
    catch (::com::sun::star::uno::Exception &)
    {
    }
    return bExists;
}

    // get a list of files from the folder of the URL
    // options: pExtension = 0 -> all, else this specific extension
    //          pDateTime != 0 -> returns also the modified date/time of
    //                       the files in a std::vector<OUString> -->
    //                       !! objects must be deleted from the caller!!
bool UCB_GetFileListOfFolder( const OUString& rURL,
                                std::vector<OUString>& rList,
                                const OUString* pExtension,
                                std::vector< ::DateTime* >* pDateTimeList )
{
    bool bOk = false;
    try
    {
        ucbhelper::Content aCnt( rURL, ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XCommandEnvironment >(), comphelper::getProcessComponentContext() );
        ::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XResultSet > xResultSet;

        const sal_Int32 nSeqSize = pDateTimeList ? 2 : 1;
        ::com::sun::star::uno::Sequence < OUString > aProps( nSeqSize );
        OUString* pProps = aProps.getArray();
        pProps[ 0 ] = "Title";
        if( pDateTimeList )
            pProps[ 1 ] = "DateModified";

        try
        {
            xResultSet = aCnt.createCursor( aProps, ::ucbhelper::INCLUDE_DOCUMENTS_ONLY );
        }
        catch( ::com::sun::star::uno::Exception& )
        {
            OSL_FAIL( "create cursor failed!" );
        }

        if( xResultSet.is() )
        {
            ::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XRow > xRow( xResultSet, ::com::sun::star::uno::UNO_QUERY );
            const sal_Int32 nExtLen = pExtension ? pExtension->getLength() : 0;
            try
            {
                if( xResultSet->first() )
                {
                    do {
                        const OUString sTitle( xRow->getString( 1 ) );
                        if( !nExtLen ||
                            ( sTitle.getLength() > nExtLen &&
                              sTitle.endsWith( *pExtension )) )
                        {
                            rList.push_back( sTitle );

                            if( pDateTimeList )
                            {
                                ::com::sun::star::util::DateTime aStamp = xRow->getTimestamp(2);
                                ::DateTime* pDateTime = new ::DateTime(
                                        ::Date( aStamp.Day,
                                                aStamp.Month,
                                                aStamp.Year ),
                                        ::Time( aStamp.Hours,
                                                aStamp.Minutes,
                                                aStamp.Seconds,
                                                aStamp.NanoSeconds ));
                                pDateTimeList->push_back( pDateTime );
                            }
                        }

                    } while( xResultSet->next() );
                }
                bOk = true;
            }
            catch( ::com::sun::star::uno::Exception& )
            {
                OSL_FAIL( "Exception caught!" );
            }
        }
    }
    catch( ::com::sun::star::uno::Exception& )
    {
        OSL_FAIL( "Exception caught!" );
        bOk = false;
    }
    return bOk;
}

//UUUU
bool needToMapFillItemsToSvxBrushItemTypes(const SfxItemSet& rSet)
{
    const XFillStyleItem* pXFillStyleItem(static_cast< const XFillStyleItem*  >(rSet.GetItem(XATTR_FILLSTYLE, false)));

    if(!pXFillStyleItem)
    {
        return false;
    }

    // here different FillStyles can be excluded for export; it will depend on the
    // quality these fallbacks can reach. That again is done in getSvxBrushItemFromSourceSet,
    // take a look there how the superset of DrawObject FillStyles is mapped to SvxBrushItem.
    // For now, take them all - except drawing::FillStyle_NONE

    if(drawing::FillStyle_NONE != pXFillStyleItem->GetValue())
    {
        return true;
    }

    // if(XFILL_SOLID == pXFillStyleItem->GetValue() || XFILL_BITMAP == pXFillStyleItem->GetValue())
    // {
    //     return true;
    // }

    return false;
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
