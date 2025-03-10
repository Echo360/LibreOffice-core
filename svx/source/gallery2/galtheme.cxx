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

#include <config_features.h>

#include "sal/config.h"

#include <algorithm>

#include <comphelper/processfactory.hxx>
#include <tools/urlobj.hxx>
#include <tools/vcompat.hxx>
#include <unotools/streamwrap.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <unotools/tempfile.hxx>
#include <unotools/localfilehelper.hxx>
#include <ucbhelper/content.hxx>
#include <sot/storage.hxx>
#include <sot/formats.hxx>
#include <sot/filelist.hxx>
#include <vcl/virdev.hxx>
#include <vcl/cvtgrf.hxx>
#include <svl/itempool.hxx>
#include <sfx2/docfile.hxx>
#include <avmedia/mediawindow.hxx>
#include <svx/svdograf.hxx>
#include <svx/fmpage.hxx>
#include "codec.hxx"
#include <svx/unomodel.hxx>
#include <svx/fmmodel.hxx>
#include <svx/fmview.hxx>
#include "svx/galmisc.hxx"
#include "svx/galtheme.hxx"
#include <com/sun/star/sdbc/XResultSet.hpp>
#include <com/sun/star/ucb/XContentAccess.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include "galobj.hxx"
#include <svx/gallery1.hxx>
#include "galtheme.hrc"
#include <vcl/lstbox.hxx>
#include "gallerydrawmodel.hxx"
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>

using namespace ::rtl;
using namespace ::com::sun::star;

// - SgaTheme -


GalleryTheme::GalleryTheme( Gallery* pGallery, GalleryThemeEntry* pThemeEntry )
    : m_bDestDirRelative(false)
    , pParent(pGallery)
    , pThm(pThemeEntry)
    , mnThemeLockCount(0)
    , mnBroadcasterLockCount(0)
    , nDragPos(0)
    , bDragging(false)
    , bAbortActualize(false)
{
    ImplCreateSvDrawStorage();
}

GalleryTheme::~GalleryTheme()
{
    ImplWrite();

    for ( size_t i = 0, n = aObjectList.size(); i < n; ++i )
    {
        GalleryObject* pEntry = aObjectList[ i ];
        Broadcast( GalleryHint( GALLERY_HINT_CLOSE_OBJECT, GetName(), reinterpret_cast< sal_uIntPtr >( pEntry ) ) );
        Broadcast( GalleryHint( GALLERY_HINT_OBJECT_REMOVED, GetName(), reinterpret_cast< sal_uIntPtr >( pEntry ) ) );
        delete pEntry;
    }
    aObjectList.clear();

}

void GalleryTheme::ImplCreateSvDrawStorage()
{
    try
    {
        aSvDrawStorageRef = new SvStorage( false, GetSdvURL().GetMainURL( INetURLObject::NO_DECODE ), pThm->IsReadOnly() ? STREAM_READ : STREAM_STD_READWRITE );
        // #i50423# ReadOnly may not been set though the file can't be written (because of security reasons)
        if ( ( aSvDrawStorageRef->GetError() != ERRCODE_NONE ) && !pThm->IsReadOnly() )
            aSvDrawStorageRef = new SvStorage( false, GetSdvURL().GetMainURL( INetURLObject::NO_DECODE ), STREAM_READ );
    }
    catch (const css::ucb::ContentCreationException& e)
    {
        SAL_WARN("svx", "failed to open: "
                  << GetSdvURL().GetMainURL(INetURLObject::NO_DECODE)
                  << "due to : " << e.Message);
    }
}

bool GalleryTheme::ImplWriteSgaObject( const SgaObject& rObj, size_t nPos, GalleryObject* pExistentEntry )
{
    boost::scoped_ptr<SvStream> pOStm(::utl::UcbStreamHelper::CreateStream( GetSdgURL().GetMainURL( INetURLObject::NO_DECODE ), STREAM_WRITE ));
    bool        bRet = false;

    if( pOStm )
    {
        const sal_uInt32 nOffset = pOStm->Seek( STREAM_SEEK_TO_END );

        rObj.WriteData( *pOStm, m_aDestDir );

        if( !pOStm->GetError() )
        {
            GalleryObject* pEntry;

            if( !pExistentEntry )
            {
                pEntry = new GalleryObject;
                if ( nPos < aObjectList.size() )
                {
                    GalleryObjectList::iterator it = aObjectList.begin();
                    ::std::advance( it, nPos );
                    aObjectList.insert( it, pEntry );
                }
                else
                    aObjectList.push_back( pEntry );
            }
            else
                pEntry = pExistentEntry;

            pEntry->aURL = rObj.GetURL();
            pEntry->nOffset = nOffset;
            pEntry->eObjKind = rObj.GetObjKind();
            bRet = true;
        }
    }

    return bRet;
}

SgaObject* GalleryTheme::ImplReadSgaObject( GalleryObject* pEntry )
{
    SgaObject* pSgaObj = NULL;

    if( pEntry )
    {
        boost::scoped_ptr<SvStream> pIStm(::utl::UcbStreamHelper::CreateStream( GetSdgURL().GetMainURL( INetURLObject::NO_DECODE ), STREAM_READ ));

        if( pIStm )
        {
            sal_uInt32 nInventor;

            // Check to ensure that the file is a valid SGA file
            pIStm->Seek( pEntry->nOffset );
            pIStm->ReadUInt32( nInventor );

            if( nInventor == COMPAT_FORMAT( 'S', 'G', 'A', '3' ) )
            {
                pIStm->Seek( pEntry->nOffset );

                switch( pEntry->eObjKind )
                {
                    case( SGA_OBJ_BMP ):    pSgaObj = new SgaObjectBmp(); break;
                    case( SGA_OBJ_ANIM ):   pSgaObj = new SgaObjectAnim(); break;
                    case( SGA_OBJ_INET ):   pSgaObj = new SgaObjectINet(); break;
                    case( SGA_OBJ_SVDRAW ): pSgaObj = new SgaObjectSvDraw(); break;
                    case( SGA_OBJ_SOUND ):  pSgaObj = new SgaObjectSound(); break;

                    default:
                    break;
                }

                if( pSgaObj )
                {
                    ReadSgaObject( *pIStm, *pSgaObj );
                    pSgaObj->ImplUpdateURL( pEntry->aURL );
                }
            }
        }
    }

    return pSgaObj;
}

void GalleryTheme::ImplWrite()
{
    if( IsModified() )
    {
        INetURLObject aPathURL( GetThmURL() );

        aPathURL.removeSegment();
        aPathURL.removeFinalSlash();

        DBG_ASSERT( aPathURL.GetProtocol() != INET_PROT_NOT_VALID, "invalid URL" );

        if( FileExists( aPathURL ) || CreateDir( aPathURL ) )
        {
#ifdef UNX
            boost::scoped_ptr<SvStream> pOStm(::utl::UcbStreamHelper::CreateStream( GetThmURL().GetMainURL( INetURLObject::NO_DECODE ), STREAM_WRITE | STREAM_COPY_ON_SYMLINK | STREAM_TRUNC ));
#else
            boost::scoped_ptr<SvStream> pOStm(::utl::UcbStreamHelper::CreateStream( GetThmURL().GetMainURL( INetURLObject::NO_DECODE ), STREAM_WRITE | STREAM_TRUNC ));
#endif

            if( pOStm )
            {
                WriteGalleryTheme( *pOStm, *this );
                pOStm.reset();
            }

            ImplSetModified( false );
        }
    }
}

const GalleryObject* GalleryTheme::ImplGetGalleryObject( const INetURLObject& rURL )
{
    for ( size_t i = 0, n = aObjectList.size(); i < n; ++i )
        if ( aObjectList[ i ]->aURL == rURL )
            return aObjectList[ i ];
    return NULL;
}

INetURLObject GalleryTheme::ImplGetURL( const GalleryObject* pObject ) const
{
    INetURLObject aURL;

    if( pObject )
        aURL = pObject->aURL;

    return aURL;
}

INetURLObject GalleryTheme::ImplCreateUniqueURL( SgaObjKind eObjKind, sal_uIntPtr nFormat )
{
    INetURLObject   aDir( GetParent()->GetUserURL() );
    INetURLObject   aInfoFileURL( GetParent()->GetUserURL() );
    INetURLObject   aNewURL;
    sal_uInt32      nNextNumber = 1999;
    sal_Char const* pExt = NULL;
    bool            bExists;

    aDir.Append( OUString("dragdrop") );
    CreateDir( aDir );

    aInfoFileURL.Append( OUString("sdddndx1") );

    // read next possible number
    if( FileExists( aInfoFileURL ) )
    {
        boost::scoped_ptr<SvStream> pIStm(::utl::UcbStreamHelper::CreateStream( aInfoFileURL.GetMainURL( INetURLObject::NO_DECODE ), STREAM_READ ));

        if( pIStm )
        {
            pIStm->ReadUInt32( nNextNumber );
        }
    }

    // create extension
    if( nFormat )
    {
        switch( nFormat )
        {
            case( CVT_BMP ): pExt = ".bmp"; break;
            case( CVT_GIF ): pExt = ".gif"; break;
            case( CVT_JPG ): pExt = ".jpg"; break;
            case( CVT_MET ): pExt = ".met"; break;
            case( CVT_PCT ): pExt = ".pct"; break;
            case( CVT_PNG ): pExt = ".png"; break;
            case( CVT_SVM ): pExt = ".svm"; break;
            case( CVT_TIF ): pExt = ".tif"; break;
            case( CVT_WMF ): pExt = ".wmf"; break;
            case( CVT_EMF ): pExt = ".emf"; break;

            default:
                pExt = ".grf";
            break;
        }
    }

    do
    {
        // get URL
        if( SGA_OBJ_SVDRAW == eObjKind )
        {
            OUString aFileName( "gallery/svdraw/dd" );
            aNewURL = INetURLObject( aFileName += OUString::number( ++nNextNumber % 99999999 ), INET_PROT_PRIV_SOFFICE );

            bExists = false;

            for ( size_t i = 0, n = aObjectList.size(); i < n; ++i )
                if ( aObjectList[ i ]->aURL == aNewURL )
                {
                    bExists = true;
                    break;
                }
        }
        else
        {
            OUString aFileName( "dd" );

            aFileName += OUString::number( ++nNextNumber % 999999 );

            if (pExt)
                aFileName += OUString( pExt, strlen(pExt), RTL_TEXTENCODING_ASCII_US );

            aNewURL = aDir;
            aNewURL.Append( aFileName );

            bExists = FileExists( aNewURL );
        }
    }
    while( bExists );

    // write updated number
    boost::scoped_ptr<SvStream> pOStm(::utl::UcbStreamHelper::CreateStream( aInfoFileURL.GetMainURL( INetURLObject::NO_DECODE ), STREAM_WRITE ));

    if( pOStm )
    {
        pOStm->WriteUInt32( nNextNumber );
    }

    return aNewURL;
}

void GalleryTheme::ImplBroadcast( sal_uIntPtr nUpdatePos )
{
    if( !IsBroadcasterLocked() )
    {
        if( GetObjectCount() && ( nUpdatePos >= GetObjectCount() ) )
            nUpdatePos = GetObjectCount() - 1;

        Broadcast( GalleryHint( GALLERY_HINT_THEME_UPDATEVIEW, GetName(), nUpdatePos ) );
    }
}

bool GalleryTheme::UnlockTheme()
{
    DBG_ASSERT( mnThemeLockCount, "Theme is not locked" );

    bool bRet = false;

    if( mnThemeLockCount )
    {
        --mnThemeLockCount;
        bRet = true;
    }

    return bRet;
}

void GalleryTheme::UnlockBroadcaster( sal_uIntPtr nUpdatePos )
{
    DBG_ASSERT( mnBroadcasterLockCount, "Broadcaster is not locked" );

    if( mnBroadcasterLockCount && !--mnBroadcasterLockCount )
        ImplBroadcast( nUpdatePos );
}

bool GalleryTheme::InsertObject( const SgaObject& rObj, sal_uIntPtr nInsertPos )
{
    if (!rObj.IsValid())
        return false;

    GalleryObject* pFoundEntry = NULL;
    size_t iFoundPos = 0;
    for (size_t n = aObjectList.size(); iFoundPos < n; ++iFoundPos)
    {
        if (aObjectList[ iFoundPos ]->aURL == rObj.GetURL())
        {
            pFoundEntry = aObjectList[ iFoundPos ];
            break;
        }
    }

    if (pFoundEntry)
    {
        GalleryObject aNewEntry;

        // update title of new object if necessary
        if (rObj.GetTitle().isEmpty())
        {
            boost::scoped_ptr<SgaObject> pOldObj(ImplReadSgaObject(pFoundEntry));

            if (pOldObj)
            {
                ((SgaObject&) rObj).SetTitle( pOldObj->GetTitle() );
            }
        }
        else if (rObj.GetTitle() == "__<empty>__")
            ((SgaObject&) rObj).SetTitle("");

        ImplWriteSgaObject(rObj, nInsertPos, &aNewEntry);
        pFoundEntry->nOffset = aNewEntry.nOffset;
    }
    else
        ImplWriteSgaObject(rObj, nInsertPos, NULL);

    ImplSetModified(true);
    ImplBroadcast(pFoundEntry? iFoundPos: nInsertPos);

    return true;
}

SgaObject* GalleryTheme::AcquireObject( size_t nPos )
{
    return ImplReadSgaObject( aObjectList[ nPos ] );
}

void GalleryTheme::GetPreviewBitmapExAndStrings(sal_uIntPtr nPos, BitmapEx& rBitmapEx, Size& rSize, OUString& rTitle, OUString& rPath) const
{
    const GalleryObject* pGalleryObject = nPos < aObjectList.size() ? aObjectList[ nPos ] : NULL;

    if(pGalleryObject)
    {
        rBitmapEx = pGalleryObject->maPreviewBitmapEx;
        rSize = pGalleryObject->maPreparedSize;
        rTitle = pGalleryObject->maTitle;
        rPath = pGalleryObject->maPath;
    }
    else
    {
        OSL_ENSURE(false, "OOps, no GalleryObject at this index (!)");
    }
}

void GalleryTheme::SetPreviewBitmapExAndStrings(sal_uIntPtr nPos, const BitmapEx& rBitmapEx, const Size& rSize, const OUString& rTitle, const OUString& rPath)
{
    GalleryObject* pGalleryObject = nPos < aObjectList.size() ? aObjectList[ nPos ] : NULL;

    if(pGalleryObject)
    {
        pGalleryObject->maPreviewBitmapEx = rBitmapEx;
        pGalleryObject->maPreparedSize = rSize;
        pGalleryObject->maTitle = rTitle;
        pGalleryObject->maPath = rPath;
    }
    else
    {
        OSL_ENSURE(false, "OOps, no GalleryObject at this index (!)");
    }
}

void GalleryTheme::ReleaseObject( SgaObject* pObject )
{
    delete pObject;
}

bool GalleryTheme::RemoveObject( size_t nPos )
{
    GalleryObject* pEntry = NULL;
    if ( nPos < aObjectList.size() )
    {
        GalleryObjectList::iterator it = aObjectList.begin();
        ::std::advance( it, nPos );
        pEntry = *it;
        aObjectList.erase( it );
    }

    if( aObjectList.empty() )
        KillFile( GetSdgURL() );

    if( pEntry )
    {
        if( SGA_OBJ_SVDRAW == pEntry->eObjKind )
            aSvDrawStorageRef->Remove( pEntry->aURL.GetMainURL( INetURLObject::NO_DECODE ) );

        Broadcast( GalleryHint( GALLERY_HINT_CLOSE_OBJECT, GetName(), reinterpret_cast< sal_uIntPtr >( pEntry ) ) );
        Broadcast( GalleryHint( GALLERY_HINT_OBJECT_REMOVED, GetName(), reinterpret_cast< sal_uIntPtr >( pEntry ) ) );
        delete pEntry;

        ImplSetModified( true );
        ImplBroadcast( nPos );
    }

    return( pEntry != NULL );
}

bool GalleryTheme::ChangeObjectPos( size_t nOldPos, size_t nNewPos )
{
    if (nOldPos == nNewPos || nOldPos >= aObjectList.size())
        return false;

    GalleryObject* pEntry = aObjectList[nOldPos];

    GalleryObjectList::iterator it = aObjectList.begin();
    ::std::advance(it, nNewPos);
    aObjectList.insert(it, pEntry);

    if (nNewPos < nOldPos)
        nOldPos++;

    it = aObjectList.begin();
    ::std::advance(it, nOldPos);
    aObjectList.erase(it);

    ImplSetModified(true);
    ImplBroadcast((nNewPos < nOldPos)? nNewPos: (nNewPos - 1));

    return true;
}

void GalleryTheme::Actualize( const Link& rActualizeLink, GalleryProgress* pProgress )
{
    if( !IsReadOnly() )
    {
        Graphic         aGraphic;
        OUString        aFormat;
        GalleryObject*  pEntry;
        const size_t    nCount = aObjectList.size();

        LockBroadcaster();
        bAbortActualize = false;

        // reset delete flag
        for (size_t i = 0; i < nCount; i++)
            aObjectList[ i ]->mbDelete = false;

        for(size_t i = 0; ( i < nCount ) && !bAbortActualize; i++)
        {
            if( pProgress )
                pProgress->Update( i, nCount - 1 );

            pEntry = aObjectList[ i ];

            const INetURLObject aURL( pEntry->aURL );

            rActualizeLink.Call( (void*) &aURL );

            // SvDraw objects will be updated later
            if( pEntry->eObjKind != SGA_OBJ_SVDRAW )
            {
                // Still a function should be implemented,
                // which assigns files to the relevant entry.
                // insert graphics as graphic objects into the gallery
                if( pEntry->eObjKind == SGA_OBJ_SOUND )
                {
                    SgaObjectSound aObjSound( aURL );
                    if( !InsertObject( aObjSound ) )
                        pEntry->mbDelete = true;
                }
                else
                {
                    aGraphic.Clear();

                    if ( GalleryGraphicImport( aURL, aGraphic, aFormat ) )
                    {
                        boost::scoped_ptr<SgaObject> pNewObj;

                        if ( SGA_OBJ_INET == pEntry->eObjKind )
                            pNewObj.reset((SgaObject*) new SgaObjectINet( aGraphic, aURL, aFormat ));
                        else if ( aGraphic.IsAnimated() )
                            pNewObj.reset((SgaObject*) new SgaObjectAnim( aGraphic, aURL, aFormat ));
                        else
                            pNewObj.reset((SgaObject*) new SgaObjectBmp( aGraphic, aURL, aFormat ));

                        if( !InsertObject( *pNewObj ) )
                            pEntry->mbDelete = true;
                    }
                    else
                        pEntry->mbDelete = true; // set delete flag
                }
            }
            else
            {
                if ( aSvDrawStorageRef.Is() )
                {
                    const OUString        aStmName( GetSvDrawStreamNameFromURL( pEntry->aURL ) );
                    SvStorageStreamRef  pIStm = aSvDrawStorageRef->OpenSotStream( aStmName, STREAM_READ );

                    if( pIStm && !pIStm->GetError() )
                    {
                        pIStm->SetBufferSize( 16384 );

                        SgaObjectSvDraw aNewObj( *pIStm, pEntry->aURL );

                        if( !InsertObject( aNewObj ) )
                            pEntry->mbDelete = true;

                        pIStm->SetBufferSize( 0L );
                    }
                }
            }
        }

        // remove all entries with set flag
        for ( size_t i = 0; i < aObjectList.size(); )
        {
            pEntry = aObjectList[ i ];
            if( pEntry->mbDelete )
            {
                Broadcast( GalleryHint( GALLERY_HINT_CLOSE_OBJECT, GetName(), reinterpret_cast< sal_uIntPtr >( pEntry ) ) );
                Broadcast( GalleryHint( GALLERY_HINT_OBJECT_REMOVED, GetName(), reinterpret_cast< sal_uLong >( pEntry ) ) );
                GalleryObjectList::iterator it = aObjectList.begin();
                ::std::advance( it, i );
                aObjectList.erase( it );
                delete pEntry;
            }
            else ++i;
        }

        // update theme
        ::utl::TempFile aTmp;
        INetURLObject   aInURL( GetSdgURL() );
        INetURLObject   aTmpURL( aTmp.GetURL() );

        DBG_ASSERT( aInURL.GetProtocol() != INET_PROT_NOT_VALID, "invalid URL" );
        DBG_ASSERT( aTmpURL.GetProtocol() != INET_PROT_NOT_VALID, "invalid URL" );

        boost::scoped_ptr<SvStream> pIStm(::utl::UcbStreamHelper::CreateStream( aInURL.GetMainURL( INetURLObject::NO_DECODE ), STREAM_READ ));
        boost::scoped_ptr<SvStream> pTmpStm(::utl::UcbStreamHelper::CreateStream( aTmpURL.GetMainURL( INetURLObject::NO_DECODE ), STREAM_WRITE | STREAM_TRUNC ));

        if( pIStm && pTmpStm )
        {
            for ( size_t i = 0, n = aObjectList.size(); i < n; ++i )
            {
                pEntry = aObjectList[ i ];
                boost::scoped_ptr<SgaObject> pObj;

                switch( pEntry->eObjKind )
                {
                case( SGA_OBJ_BMP ):    pObj.reset(new SgaObjectBmp());      break;
                case( SGA_OBJ_ANIM ):   pObj.reset(new SgaObjectAnim());     break;
                case( SGA_OBJ_INET ):   pObj.reset(new SgaObjectINet());     break;
                case( SGA_OBJ_SVDRAW ): pObj.reset(new SgaObjectSvDraw());   break;
                case (SGA_OBJ_SOUND):   pObj.reset(new SgaObjectSound());    break;

                    default:
                    break;
                }

                if( pObj )
                {
                    pIStm->Seek( pEntry->nOffset );
                    ReadSgaObject( *pIStm, *pObj);
                    pEntry->nOffset = pTmpStm->Tell();
                    WriteSgaObject( *pTmpStm, *pObj );
                }
            }
        }
        else
        {
            OSL_FAIL( "File(s) could not be opened" );
        }

        pIStm.reset();
        pTmpStm.reset();

        CopyFile( aTmpURL, aInURL );
        KillFile( aTmpURL );

        sal_uIntPtr nStorErr = 0;

        try
        {
            SvStorageRef aTempStorageRef( new SvStorage( false, aTmpURL.GetMainURL( INetURLObject::NO_DECODE ), STREAM_STD_READWRITE ) );
            aSvDrawStorageRef->CopyTo( aTempStorageRef );
            nStorErr = aSvDrawStorageRef->GetError();
        }
        catch (const css::ucb::ContentCreationException& e)
        {
            SAL_WARN("svx", "failed to open: "
                      << aTmpURL.GetMainURL(INetURLObject::NO_DECODE)
                      << "due to : " << e.Message);
            nStorErr = ERRCODE_IO_GENERAL;
        }

        if( !nStorErr )
        {
            aSvDrawStorageRef.Clear();
            CopyFile( aTmpURL, GetSdvURL() );
            ImplCreateSvDrawStorage();
        }

        KillFile( aTmpURL );
        ImplSetModified( true );
        ImplWrite();
        UnlockBroadcaster();
    }
}

GalleryThemeEntry* GalleryTheme::CreateThemeEntry( const INetURLObject& rURL, bool bReadOnly )
{
    DBG_ASSERT( rURL.GetProtocol() != INET_PROT_NOT_VALID, "invalid URL" );

    GalleryThemeEntry*  pRet = NULL;

    if( FileExists( rURL ) )
    {
        boost::scoped_ptr<SvStream> pIStm(::utl::UcbStreamHelper::CreateStream( rURL.GetMainURL( INetURLObject::NO_DECODE ), STREAM_READ ));

        if( pIStm )
        {
            OUString        aThemeName;
            sal_uInt16      nVersion;
            bool        bThemeNameFromResource = false;

            pIStm->ReadUInt16( nVersion );

            if( nVersion <= 0x00ff )
            {
                sal_uInt32      nThemeId = 0;

                OString aTmpStr = read_uInt16_lenPrefixed_uInt8s_ToOString(*pIStm);
                aThemeName = OStringToOUString(aTmpStr, RTL_TEXTENCODING_UTF8);

                // execute a charakter conversion
                if( nVersion >= 0x0004 )
                {
                    sal_uInt32  nCount;
                    sal_uInt16  nTemp16;

                    pIStm->ReadUInt32( nCount ).ReadUInt16( nTemp16 );
                    pIStm->Seek( STREAM_SEEK_TO_END );

                    // check whether there is a newer version;
                    // therefore jump back by 520Bytes (8 bytes ID + 512Bytes reserve buffer)
                    // if this is at all possible.
                    if( pIStm->Tell() >= 520 )
                    {
                        sal_uInt32 nId1, nId2;

                        pIStm->SeekRel( -520 );
                        pIStm->ReadUInt32( nId1 ).ReadUInt32( nId2 );

                        if( nId1 == COMPAT_FORMAT( 'G', 'A', 'L', 'R' ) &&
                            nId2 == COMPAT_FORMAT( 'E', 'S', 'R', 'V' ) )
                        {
                            boost::scoped_ptr<VersionCompat> pCompat(new VersionCompat( *pIStm, STREAM_READ ));

                            pIStm->ReadUInt32( nThemeId );

                            if( pCompat->GetVersion() >= 2 )
                            {
                                pIStm->ReadCharAsBool( bThemeNameFromResource );
                            }
                        }
                    }
                }

                INetURLObject aPathURL( rURL );
                pRet = new GalleryThemeEntry( false, aPathURL, aThemeName,
                                              bReadOnly, false, nThemeId,
                                              bThemeNameFromResource );
            }
        }
    }

    return pRet;
}

bool GalleryTheme::GetThumb( sal_uIntPtr nPos, BitmapEx& rBmp, bool )
{
    SgaObject*  pObj = AcquireObject( nPos );
    bool        bRet = false;

    if( pObj )
    {
        rBmp = pObj->GetThumbBmp();
        ReleaseObject( pObj );
        bRet = true;
    }

    return bRet;
}

bool GalleryTheme::GetGraphic( sal_uIntPtr nPos, Graphic& rGraphic, bool bProgress )
{
    const GalleryObject*    pObject = ImplGetGalleryObject( nPos );
    bool                    bRet = false;

    if( pObject )
    {
        const INetURLObject aURL( ImplGetURL( pObject ) );

        switch( pObject->eObjKind )
        {
            case( SGA_OBJ_BMP ):
            case( SGA_OBJ_ANIM ):
            case( SGA_OBJ_INET ):
            {
                OUString aFilterDummy;
                bRet = ( GalleryGraphicImport( aURL, rGraphic, aFilterDummy, bProgress ) != SGA_IMPORT_NONE );
            }
            break;

            case( SGA_OBJ_SVDRAW ):
            {
                SvxGalleryDrawModel aModel;

                if( aModel.GetModel() )
                {
                    if( GetModel( nPos, *aModel.GetModel(), bProgress ) )
                    {
                        ImageMap aIMap;

                        if( CreateIMapGraphic( *aModel.GetModel(), rGraphic, aIMap ) )
                            bRet = true;
                        else
                        {
                            VirtualDevice aVDev;
                            aVDev.SetMapMode( MapMode( MAP_100TH_MM ) );
                            FmFormView aView( aModel.GetModel(), &aVDev );

                            aView.hideMarkHandles();
                            aView.ShowSdrPage(aView.GetModel()->GetPage(0));
                            aView.MarkAll();
                            rGraphic = aView.GetAllMarkedGraphic();
                            bRet = true;
                        }
                    }
                }
            }
            break;

            case( SGA_OBJ_SOUND ):
            {
                SgaObject* pObj = AcquireObject( nPos );

                if( pObj )
                {
                    rGraphic = pObj->GetThumbBmp();
                    //Bitmap aBmp( pObj->GetThumbBmp() );
                    //aBmp.Replace( COL_LIGHTMAGENTA, COL_WHITE );
                    //rGraphic = aBmp;
                    ReleaseObject( pObj );
                    bRet = true;
                }
            }
            break;

            default:
            break;
        }
    }

    return bRet;
}

bool GalleryTheme::InsertGraphic( const Graphic& rGraphic, sal_uIntPtr nInsertPos )
{
    bool bRet = false;

    if( rGraphic.GetType() != GRAPHIC_NONE )
    {
        sal_uIntPtr           nExportFormat = CVT_UNKNOWN;
        const GfxLink   aGfxLink( ( (Graphic&) rGraphic ).GetLink() );

        if( aGfxLink.GetDataSize() )
        {
            switch( aGfxLink.GetType() )
            {
                case( GFX_LINK_TYPE_EPS_BUFFER ): nExportFormat = CVT_SVM; break;
                case( GFX_LINK_TYPE_NATIVE_GIF ): nExportFormat = CVT_GIF; break;

                // #i15508# added BMP type
                // could not find/trigger a call to this, but should do no harm
                case( GFX_LINK_TYPE_NATIVE_BMP ): nExportFormat = CVT_BMP; break;

                case( GFX_LINK_TYPE_NATIVE_JPG ): nExportFormat = CVT_JPG; break;
                case( GFX_LINK_TYPE_NATIVE_PNG ): nExportFormat = CVT_PNG; break;
                case( GFX_LINK_TYPE_NATIVE_TIF ): nExportFormat = CVT_TIF; break;
                case( GFX_LINK_TYPE_NATIVE_WMF ): nExportFormat = CVT_WMF; break;
                case( GFX_LINK_TYPE_NATIVE_MET ): nExportFormat = CVT_MET; break;
                case( GFX_LINK_TYPE_NATIVE_PCT ): nExportFormat = CVT_PCT; break;
                case( GFX_LINK_TYPE_NATIVE_SVG ): nExportFormat = CVT_SVG; break;
                default:
                    break;
            }
        }
        else
        {
            if( rGraphic.GetType() == GRAPHIC_BITMAP )
            {
                if( rGraphic.IsAnimated() )
                    nExportFormat = CVT_GIF;
                else
                    nExportFormat = CVT_PNG;
            }
            else
                nExportFormat = CVT_SVM;
        }

        const INetURLObject aURL( ImplCreateUniqueURL( SGA_OBJ_BMP, nExportFormat ) );
        boost::scoped_ptr<SvStream> pOStm(::utl::UcbStreamHelper::CreateStream( aURL.GetMainURL( INetURLObject::NO_DECODE ), STREAM_WRITE | STREAM_TRUNC ));

        if( pOStm )
        {
            pOStm->SetVersion( SOFFICE_FILEFORMAT_50 );

            if( CVT_SVM == nExportFormat )
            {
                GDIMetaFile aMtf( rGraphic.GetGDIMetaFile() );

                aMtf.Write( *pOStm );
                bRet = ( pOStm->GetError() == ERRCODE_NONE );
            }
            else
            {
                if( aGfxLink.GetDataSize() && aGfxLink.GetData() )
                {
                    pOStm->Write( aGfxLink.GetData(), aGfxLink.GetDataSize() );
                    bRet = ( pOStm->GetError() == ERRCODE_NONE );
                }
                else
                    bRet = ( GraphicConverter::Export( *pOStm, rGraphic, nExportFormat ) == ERRCODE_NONE );
            }

            pOStm.reset();
        }

        if( bRet )
        {
            const SgaObjectBmp aObjBmp( aURL );
            InsertObject( aObjBmp, nInsertPos );
        }
    }

    return bRet;
}

bool GalleryTheme::GetModel( sal_uIntPtr nPos, SdrModel& rModel, bool )
{
    const GalleryObject*    pObject = ImplGetGalleryObject( nPos );
    bool                    bRet = false;

    if( pObject && ( SGA_OBJ_SVDRAW == pObject->eObjKind ) )
    {
        const INetURLObject aURL( ImplGetURL( pObject ) );
        SvStorageRef        xStor( GetSvDrawStorage() );

        if( xStor.Is() )
        {
            const OUString        aStmName( GetSvDrawStreamNameFromURL( aURL ) );
            SvStorageStreamRef  xIStm( xStor->OpenSotStream( aStmName, STREAM_READ ) );

            if( xIStm.Is() && !xIStm->GetError() )
            {
                xIStm->SetBufferSize( STREAMBUF_SIZE );
                bRet = GallerySvDrawImport( *xIStm, rModel );
                xIStm->SetBufferSize( 0L );
            }
        }
    }

    return bRet;
}

bool GalleryTheme::InsertModel( const FmFormModel& rModel, sal_uIntPtr nInsertPos )
{
    INetURLObject   aURL( ImplCreateUniqueURL( SGA_OBJ_SVDRAW ) );
    SvStorageRef    xStor( GetSvDrawStorage() );
    bool            bRet = false;

    if( xStor.Is() )
    {
        const OUString        aStmName( GetSvDrawStreamNameFromURL( aURL ) );
        SvStorageStreamRef  xOStm( xStor->OpenSotStream( aStmName, STREAM_WRITE | STREAM_TRUNC ) );

        if( xOStm.Is() && !xOStm->GetError() )
        {
            SvMemoryStream  aMemStm( 65535, 65535 );
            FmFormModel*    pFormModel = (FmFormModel*) &rModel;

            pFormModel->BurnInStyleSheetAttributes();

            {
                uno::Reference< io::XOutputStream > xDocOut( new utl::OOutputStreamWrapper( aMemStm ) );

                if( xDocOut.is() )
                    SvxDrawingLayerExport( pFormModel, xDocOut );
            }

            aMemStm.Seek( 0 );

            xOStm->SetBufferSize( 16348 );
            GalleryCodec aCodec( *xOStm );
            aCodec.Write( aMemStm );

            if( !xOStm->GetError() )
            {
                SgaObjectSvDraw aObjSvDraw( rModel, aURL );
                bRet = InsertObject( aObjSvDraw, nInsertPos );
            }

            xOStm->SetBufferSize( 0L );
            xOStm->Commit();
        }
    }

    return bRet;
}

bool GalleryTheme::GetModelStream( sal_uIntPtr nPos, SotStorageStreamRef& rxModelStream, bool )
{
    const GalleryObject*    pObject = ImplGetGalleryObject( nPos );
    bool                    bRet = false;

    if( pObject && ( SGA_OBJ_SVDRAW == pObject->eObjKind ) )
    {
        const INetURLObject aURL( ImplGetURL( pObject ) );
        SvStorageRef        xStor( GetSvDrawStorage() );

        if( xStor.Is() )
        {
            const OUString        aStmName( GetSvDrawStreamNameFromURL( aURL ) );
            SvStorageStreamRef  xIStm( xStor->OpenSotStream( aStmName, STREAM_READ ) );

            if( xIStm.Is() && !xIStm->GetError() )
            {
                sal_uInt32 nVersion = 0;

                xIStm->SetBufferSize( 16348 );

                if( GalleryCodec::IsCoded( *xIStm, nVersion ) )
                {
                    SvxGalleryDrawModel aModel;

                    if( aModel.GetModel() )
                    {
                        if( GallerySvDrawImport( *xIStm, *aModel.GetModel() ) )
                        {
                            aModel.GetModel()->BurnInStyleSheetAttributes();

                            {
                                uno::Reference< io::XOutputStream > xDocOut( new utl::OOutputStreamWrapper( *rxModelStream ) );

                                if( SvxDrawingLayerExport( aModel.GetModel(), xDocOut ) )
                                    rxModelStream->Commit();
                            }
                        }

                        bRet = ( rxModelStream->GetError() == ERRCODE_NONE );
                    }
                }

                xIStm->SetBufferSize( 0 );
            }
        }
    }

    return bRet;
}

bool GalleryTheme::InsertModelStream( const SotStorageStreamRef& rxModelStream, sal_uIntPtr nInsertPos )
{
    INetURLObject   aURL( ImplCreateUniqueURL( SGA_OBJ_SVDRAW ) );
    SvStorageRef    xStor( GetSvDrawStorage() );
    bool            bRet = false;

    if( xStor.Is() )
    {
        const OUString        aStmName( GetSvDrawStreamNameFromURL( aURL ) );
        SvStorageStreamRef  xOStm( xStor->OpenSotStream( aStmName, STREAM_WRITE | STREAM_TRUNC ) );

        if( xOStm.Is() && !xOStm->GetError() )
        {
            GalleryCodec    aCodec( *xOStm );
            SvMemoryStream  aMemStm( 65535, 65535 );

            xOStm->SetBufferSize( 16348 );
            aCodec.Write( *rxModelStream );

            if( !xOStm->GetError() )
            {
                xOStm->Seek( 0 );
                SgaObjectSvDraw aObjSvDraw( *xOStm, aURL );
                bRet = InsertObject( aObjSvDraw, nInsertPos );
            }

            xOStm->SetBufferSize( 0L );
            xOStm->Commit();
        }
    }

    return bRet;
}

bool GalleryTheme::GetURL( sal_uIntPtr nPos, INetURLObject& rURL, bool )
{
    const GalleryObject*    pObject = ImplGetGalleryObject( nPos );
    bool                    bRet = false;

    if( pObject )
    {
        rURL = INetURLObject( ImplGetURL( pObject ) );
        bRet = true;
    }

    return bRet;
}

bool GalleryTheme::InsertURL( const INetURLObject& rURL, sal_uIntPtr nInsertPos )
{
    Graphic         aGraphic;
    OUString        aFormat;
    boost::scoped_ptr<SgaObject> pNewObj;
    const sal_uInt16    nImportRet = GalleryGraphicImport( rURL, aGraphic, aFormat );
    bool            bRet = false;

    if( nImportRet != SGA_IMPORT_NONE )
    {
        if ( SGA_IMPORT_INET == nImportRet )
            pNewObj.reset((SgaObject*) new SgaObjectINet( aGraphic, rURL, aFormat ));
        else if ( aGraphic.IsAnimated() )
            pNewObj.reset((SgaObject*) new SgaObjectAnim( aGraphic, rURL, aFormat ));
        else
            pNewObj.reset((SgaObject*) new SgaObjectBmp( aGraphic, rURL, aFormat ));
    }
#if HAVE_FEATURE_AVMEDIA
    else if( ::avmedia::MediaWindow::isMediaURL( rURL.GetMainURL( INetURLObject::DECODE_UNAMBIGUOUS ), ""/*TODO?*/ ) )
        pNewObj.reset((SgaObject*) new SgaObjectSound( rURL ));
#endif
    if( pNewObj && InsertObject( *pNewObj, nInsertPos ) )
        bRet = true;

    return bRet;
}

bool GalleryTheme::InsertFileOrDirURL( const INetURLObject& rFileOrDirURL, sal_uIntPtr nInsertPos )
{
    INetURLObject                   aURL;
    ::std::vector< INetURLObject >  aURLVector;
    bool                            bRet = false;

    try
    {
        ::ucbhelper::Content         aCnt( rFileOrDirURL.GetMainURL( INetURLObject::NO_DECODE ), uno::Reference< ucb::XCommandEnvironment >(), comphelper::getProcessComponentContext() );
        bool        bFolder = false;

        aCnt.getPropertyValue("IsFolder") >>= bFolder;

        if( bFolder )
        {
            uno::Sequence< OUString > aProps( 1 );
            aProps[ 0 ] = "Url";
            uno::Reference< sdbc::XResultSet > xResultSet( aCnt.createCursor( aProps, ::ucbhelper::INCLUDE_DOCUMENTS_ONLY ) );
            uno::Reference< ucb::XContentAccess > xContentAccess( xResultSet, uno::UNO_QUERY );
            if( xContentAccess.is() )
            {
                while( xResultSet->next() )
                {
                    aURL.SetSmartURL( xContentAccess->queryContentIdentifierString() );
                    aURLVector.push_back( aURL );
                }
            }
        }
        else
            aURLVector.push_back( rFileOrDirURL );
    }
    catch( const ucb::ContentCreationException& )
    {
    }
    catch( const uno::RuntimeException& )
    {
    }
    catch( const uno::Exception& )
    {
    }

    ::std::vector< INetURLObject >::const_iterator aIter( aURLVector.begin() ), aEnd( aURLVector.end() );

    while( aIter != aEnd )
        bRet = bRet || InsertURL( *aIter++, nInsertPos );

    return bRet;
}

bool GalleryTheme::InsertTransferable( const uno::Reference< datatransfer::XTransferable >& rxTransferable, sal_uIntPtr nInsertPos )
{
    bool bRet = false;

    if( rxTransferable.is() )
    {
        TransferableDataHelper  aDataHelper( rxTransferable );
        boost::scoped_ptr<Graphic> pGraphic;

        if( aDataHelper.HasFormat( SOT_FORMATSTR_ID_DRAWING ) )
        {
            SotStorageStreamRef xModelStm;

            if( aDataHelper.GetSotStorageStream( SOT_FORMATSTR_ID_DRAWING, xModelStm ) )
                bRet = InsertModelStream( xModelStm, nInsertPos );
        }
        else if( aDataHelper.HasFormat( SOT_FORMAT_FILE_LIST ) ||
                 aDataHelper.HasFormat( FORMAT_FILE ) )
        {
            FileList aFileList;

            if( aDataHelper.HasFormat( SOT_FORMAT_FILE_LIST ) )
                aDataHelper.GetFileList( SOT_FORMAT_FILE_LIST, aFileList );
            else
            {
                OUString aFile;

                aDataHelper.GetString( FORMAT_FILE, aFile );

                if( !aFile.isEmpty() )
                    aFileList.AppendFile( aFile );
            }

            for( sal_uInt32 i = 0, nCount = aFileList.Count(); i < nCount; ++i )
            {
                const OUString  aFile( aFileList.GetFile( i ) );
                INetURLObject   aURL( aFile );

                if( aURL.GetProtocol() == INET_PROT_NOT_VALID )
                {
                    OUString aLocalURL;

                    if( ::utl::LocalFileHelper::ConvertPhysicalNameToURL( aFile, aLocalURL ) )
                        aURL = INetURLObject( aLocalURL );
                }

                if( aURL.GetProtocol() != INET_PROT_NOT_VALID )
                    bRet = InsertFileOrDirURL( aURL, nInsertPos );
            }
        }
        else
        {
            Graphic aGraphic;
            sal_uIntPtr nFormat = 0;

            if( aDataHelper.HasFormat( SOT_FORMATSTR_ID_SVXB ) )
                nFormat = SOT_FORMATSTR_ID_SVXB;
            else if( aDataHelper.HasFormat( FORMAT_GDIMETAFILE ) )
                nFormat = FORMAT_GDIMETAFILE;
            else if( aDataHelper.HasFormat( FORMAT_BITMAP ) )
                nFormat = FORMAT_BITMAP;

            if( nFormat && aDataHelper.GetGraphic( nFormat, aGraphic ) )
                pGraphic.reset(new Graphic( aGraphic ));
        }

        if( pGraphic )
        {
            bRet = false;

            if( aDataHelper.HasFormat( SOT_FORMATSTR_ID_SVIM ) )
            {

                ImageMap aImageMap;

                // according to KA we don't need a BaseURL here
                if( aDataHelper.GetImageMap( SOT_FORMATSTR_ID_SVIM, aImageMap ) )
                {
                    SvxGalleryDrawModel aModel;

                    if( aModel.GetModel() )
                    {
                        SgaUserDataFactory  aFactory;

                        SdrPage*    pPage = aModel.GetModel()->GetPage(0);
                        SdrGrafObj* pGrafObj = new SdrGrafObj( *pGraphic );

                        pGrafObj->AppendUserData( new SgaIMapInfo( aImageMap ) );
                        pPage->InsertObject( pGrafObj );
                        bRet = InsertModel( *aModel.GetModel(), nInsertPos );
                    }
                }
            }

            if( !bRet )
                bRet = InsertGraphic( *pGraphic, nInsertPos );
        }
    }

    return bRet;
}

void GalleryTheme::CopyToClipboard( Window* pWindow, sal_uIntPtr nPos )
{
    GalleryTransferable* pTransferable = new GalleryTransferable( this, nPos, false );
    pTransferable->CopyToClipboard( pWindow );
}

void GalleryTheme::StartDrag( Window* pWindow, sal_uIntPtr nPos )
{
    GalleryTransferable* pTransferable = new GalleryTransferable( this, nPos, true );
    pTransferable->StartDrag( pWindow, DND_ACTION_COPY | DND_ACTION_LINK );
}

SvStream& GalleryTheme::WriteData( SvStream& rOStm ) const
{
    const INetURLObject aRelURL1( GetParent()->GetRelativeURL() );
    const INetURLObject aRelURL2( GetParent()->GetUserURL() );
    sal_uInt32          nCount = GetObjectCount();
    bool                bRel;

    rOStm.WriteUInt16( (sal_uInt16) 0x0004 );
    write_uInt16_lenPrefixed_uInt8s_FromOUString(rOStm, GetRealName(), RTL_TEXTENCODING_UTF8);
    rOStm.WriteUInt32( nCount ).WriteUInt16( (sal_uInt16) osl_getThreadTextEncoding() );

    for( sal_uInt32 i = 0; i < nCount; i++ )
    {
        const GalleryObject* pObj = ImplGetGalleryObject( i );
        OUString               aPath;

        if( SGA_OBJ_SVDRAW == pObj->eObjKind )
        {
            aPath = GetSvDrawStreamNameFromURL( pObj->aURL );
            bRel = false;
        }
        else
        {
            aPath = pObj->aURL.GetMainURL( INetURLObject::NO_DECODE );
            aPath = aPath.copy( 0, std::min(aRelURL1.GetMainURL( INetURLObject::NO_DECODE ).getLength(), aPath.getLength()) );
            bRel = aPath == aRelURL1.GetMainURL( INetURLObject::NO_DECODE );

            if( bRel && ( pObj->aURL.GetMainURL( INetURLObject::NO_DECODE ).getLength() > ( aRelURL1.GetMainURL( INetURLObject::NO_DECODE ).getLength() + 1 ) ) )
            {
                aPath = pObj->aURL.GetMainURL( INetURLObject::NO_DECODE );
                aPath = aPath.copy( std::min(aRelURL1.GetMainURL( INetURLObject::NO_DECODE ).getLength(), aPath.getLength()) );
            }
            else
            {
                aPath = pObj->aURL.GetMainURL( INetURLObject::NO_DECODE );
                aPath = aPath.copy( 0, std::min(aRelURL2.GetMainURL( INetURLObject::NO_DECODE ).getLength(), aPath.getLength()) );
                bRel = aPath == aRelURL2.GetMainURL( INetURLObject::NO_DECODE );

                if( bRel && ( pObj->aURL.GetMainURL( INetURLObject::NO_DECODE ).getLength() > ( aRelURL2.GetMainURL( INetURLObject::NO_DECODE ).getLength() + 1 ) ) )
                {
                    aPath = pObj->aURL.GetMainURL( INetURLObject::NO_DECODE );
                    aPath = aPath.copy( std::min(aRelURL2.GetMainURL( INetURLObject::NO_DECODE ).getLength(), aPath.getLength()) );
                }
                else
                    aPath = pObj->aURL.GetMainURL( INetURLObject::NO_DECODE );
            }
        }

        if ( !m_aDestDir.isEmpty() )
        {
            bool aFound = aPath.indexOf(m_aDestDir) != -1;
            aPath = aPath.replaceFirst(m_aDestDir, "");
            if ( aFound )
                bRel = m_bDestDirRelative;
            else
                SAL_WARN( "svx", "failed to replace destdir of '"
                          << m_aDestDir << "' in '" << aPath << "'");
        }

        rOStm.WriteUChar( bRel );
        write_uInt16_lenPrefixed_uInt8s_FromOUString(rOStm, aPath, RTL_TEXTENCODING_UTF8);
        rOStm.WriteUInt32( pObj->nOffset ).WriteUInt16( (sal_uInt16) pObj->eObjKind );
    }

    // more recently, a 512-byte reserve buffer is written,
    // to recognize them two sal_uIntPtr-Ids will be written.
    rOStm.WriteUInt32( COMPAT_FORMAT( 'G', 'A', 'L', 'R' ) ).WriteUInt32( COMPAT_FORMAT( 'E', 'S', 'R', 'V' ) );

    const long      nReservePos = rOStm.Tell();
    boost::scoped_ptr<VersionCompat> pCompat(new VersionCompat( rOStm, STREAM_WRITE, 2 ));

    rOStm.WriteUInt32( (sal_uInt32) GetId() ).WriteUChar( IsThemeNameFromResource() ); // From version 2 and up

    pCompat.reset();

    // Fill the rest of the buffer.
    const long  nRest = std::max( 512L - ( (long) rOStm.Tell() - nReservePos ), 0L );

    if( nRest )
    {
        boost::scoped_array<char> pReserve(new char[ nRest ]);
        memset( pReserve.get(), 0, nRest );
        rOStm.Write( pReserve.get(), nRest );
    }

    return rOStm;
}

SvStream& GalleryTheme::ReadData( SvStream& rIStm )
{
    sal_uInt32          nCount;
    sal_uInt16          nVersion;
    OUString            aThemeName;
    rtl_TextEncoding    nTextEncoding;

    rIStm.ReadUInt16( nVersion );
    OString aTmpStr = read_uInt16_lenPrefixed_uInt8s_ToOString(rIStm);
    rIStm.ReadUInt32( nCount );

    if( nVersion >= 0x0004 )
    {
        sal_uInt16 nTmp16;
        rIStm.ReadUInt16( nTmp16 );
        nTextEncoding = (rtl_TextEncoding) nTmp16;
    }
    else
        nTextEncoding = RTL_TEXTENCODING_UTF8;

    aThemeName = OStringToOUString(aTmpStr, nTextEncoding);

    if( nCount <= ( 1L << 14 ) )
    {
        GalleryObject*  pObj;
        INetURLObject   aRelURL1( GetParent()->GetRelativeURL() );
        INetURLObject   aRelURL2( GetParent()->GetUserURL() );
        sal_uInt32      nId1, nId2;
        bool            bRel;

        for( size_t i = 0, n = aObjectList.size(); i < n; ++i )
        {
            pObj = aObjectList[ i ];
            Broadcast( GalleryHint( GALLERY_HINT_CLOSE_OBJECT, GetName(), reinterpret_cast< sal_uIntPtr >( pObj ) ) );
            Broadcast( GalleryHint( GALLERY_HINT_OBJECT_REMOVED, GetName(), reinterpret_cast< sal_uIntPtr >( pObj ) ) );
            delete pObj;
        }
        aObjectList.clear();

        for( sal_uInt32 i = 0; i < nCount; i++ )
        {
            pObj = new GalleryObject;

            OUString    aFileName;
            OUString    aPath;
            sal_uInt16  nTemp;

            rIStm.ReadCharAsBool( bRel );
            OString aTempFileName = read_uInt16_lenPrefixed_uInt8s_ToOString(rIStm);
            rIStm.ReadUInt32( pObj->nOffset );
            rIStm.ReadUInt16( nTemp ); pObj->eObjKind = (SgaObjKind) nTemp;

            aFileName = OStringToOUString(aTempFileName, osl_getThreadTextEncoding());

            if( bRel )
            {
                aFileName = aFileName.replaceAll( "\\", "/" );
                aPath = aRelURL1.GetMainURL( INetURLObject::NO_DECODE );

                if( aFileName[ 0 ] != '/' )
                        aPath += "/";

                aPath += aFileName;

                pObj->aURL = INetURLObject( aPath );

                if( !FileExists( pObj->aURL ) )
                {
                    aPath = aRelURL2.GetMainURL( INetURLObject::NO_DECODE );

                    if( aFileName[0] != '/' )
                        aPath += "/";

                    aPath += aFileName;

                    // assign this URL, even in the case it is not valid (#94482)
                    pObj->aURL = INetURLObject( aPath );
                }
            }
            else
            {
                if( SGA_OBJ_SVDRAW == pObj->eObjKind )
                {
                    OUString aDummyURL( "gallery/svdraw/" );
                    pObj->aURL = INetURLObject( aDummyURL += aFileName, INET_PROT_PRIV_SOFFICE );
                }
                else
                {
                    OUString aLocalURL;

                    pObj->aURL = INetURLObject( aFileName );

                    if( ( pObj->aURL.GetProtocol() == INET_PROT_NOT_VALID ) &&
                        ::utl::LocalFileHelper::ConvertPhysicalNameToURL( aFileName, aLocalURL ) )
                    {
                        pObj->aURL = INetURLObject( aLocalURL );
                    }
                }
            }
            aObjectList.push_back( pObj );
        }

        rIStm.ReadUInt32( nId1 ).ReadUInt32( nId2 );

        // In newer versions a 512 byte reserve buffer is located at the end,
        // the data is located at the beginning of this buffer and are clamped
        // by a VersionCompat.
        if( !rIStm.IsEof() &&
            nId1 == COMPAT_FORMAT( 'G', 'A', 'L', 'R' ) &&
            nId2 == COMPAT_FORMAT( 'E', 'S', 'R', 'V' ) )
        {
            boost::scoped_ptr<VersionCompat> pCompat(new VersionCompat( rIStm, STREAM_READ ));
            sal_uInt32      nTemp32;
            bool            bThemeNameFromResource = false;

            rIStm.ReadUInt32( nTemp32 );

            if( pCompat->GetVersion() >= 2 )
            {
                rIStm.ReadCharAsBool( bThemeNameFromResource );
            }

            SetId( nTemp32, bThemeNameFromResource );
        }
    }
    else
        rIStm.SetError( SVSTREAM_READ_ERROR );

    ImplSetModified( false );

    return rIStm;
}

SvStream& WriteGalleryTheme( SvStream& rOut, const GalleryTheme& rTheme )
{
    return rTheme.WriteData( rOut );
}

SvStream& ReadGalleryTheme( SvStream& rIn, GalleryTheme& rTheme )
{
    return rTheme.ReadData( rIn );
}

void GalleryTheme::ImplSetModified( bool bModified )
{
    pThm->SetModified(bModified);
}

const OUString& GalleryTheme::GetRealName() const { return pThm->GetThemeName(); }
const INetURLObject& GalleryTheme::GetThmURL() const { return pThm->GetThmURL(); }
const INetURLObject& GalleryTheme::GetSdgURL() const { return pThm->GetSdgURL(); }
const INetURLObject& GalleryTheme::GetSdvURL() const { return pThm->GetSdvURL(); }
sal_uInt32 GalleryTheme::GetId() const { return pThm->GetId(); }
void GalleryTheme::SetId( sal_uInt32 nNewId, bool bResetThemeName ) { pThm->SetId( nNewId, bResetThemeName ); }
bool GalleryTheme::IsThemeNameFromResource() const { return pThm->IsNameFromResource(); }
bool GalleryTheme::IsReadOnly() const { return pThm->IsReadOnly(); }
bool GalleryTheme::IsDefault() const { return pThm->IsDefault(); }
bool GalleryTheme::IsModified() const { return pThm->IsModified(); }
const OUString& GalleryTheme::GetName() const { return pThm->GetThemeName(); }

void GalleryTheme::InsertAllThemes( ListBox& rListBox )
{
    for( sal_uInt16 i = RID_GALLERYSTR_THEME_FIRST; i <= RID_GALLERYSTR_THEME_LAST; i++ )
        rListBox.InsertEntry(GAL_RESSTR(i));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
