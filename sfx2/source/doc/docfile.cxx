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

#include <sfx2/docfile.hxx>
#include <sfx2/signaturestate.hxx>

#include <uno/mapping.hxx>
#include <com/sun/star/task/InteractionHandler.hpp>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/ucb/XContent.hpp>
#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/document/XDocumentRevisionListPersistence.hpp>
#include <com/sun/star/document/LockedDocumentRequest.hpp>
#include <com/sun/star/document/OwnLockOnDocumentRequest.hpp>
#include <com/sun/star/document/LockFileIgnoreRequest.hpp>
#include <com/sun/star/document/ChangedByOthersRequest.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/embed/XTransactedObject.hpp>
#include <com/sun/star/embed/ElementModes.hpp>
#include <com/sun/star/embed/UseBackupException.hpp>
#include <com/sun/star/embed/XOptimizedStorage.hpp>
#include <com/sun/star/ucb/InteractiveIOException.hpp>
#include <com/sun/star/ucb/UnsupportedDataSinkException.hpp>
#include <com/sun/star/ucb/CommandFailedException.hpp>
#include <com/sun/star/ucb/CommandAbortedException.hpp>
#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <com/sun/star/ucb/XContentIdentifierFactory.hpp>
#include <com/sun/star/ucb/XContentProvider.hpp>
#include <com/sun/star/ucb/XProgressHandler.hpp>
#include <com/sun/star/ucb/XCommandInfo.hpp>
#include <com/sun/star/io/XOutputStream.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include <com/sun/star/io/XTruncate.hpp>
#include <com/sun/star/io/XStreamListener.hpp>
#include <com/sun/star/io/XSeekable.hpp>
#include <com/sun/star/ucb/XSimpleFileAccess.hpp>
#include <com/sun/star/lang/XInitialization.hpp>
#include <com/sun/star/ucb/InsertCommandArgument.hpp>
#include <com/sun/star/ucb/NameClash.hpp>
#include <com/sun/star/ucb/TransferInfo.hpp>
#include <com/sun/star/ucb/OpenCommandArgument2.hpp>
#include <com/sun/star/ucb/OpenMode.hpp>
#include <com/sun/star/logging/DocumentIOLogRing.hpp>
#include <com/sun/star/logging/XSimpleLogRing.hpp>
#include <cppuhelper/implbase1.hxx>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/security/DocumentSignatureInformation.hpp>
#include <com/sun/star/security/DocumentDigitalSignatures.hpp>
#include <tools/urlobj.hxx>
#include <unotools/tempfile.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/interaction.hxx>
#include <framework/interaction.hxx>
#include <unotools/streamhelper.hxx>
#include <unotools/localedatawrapper.hxx>
#include <vcl/msgbox.hxx>
#include <svl/stritem.hxx>
#include <svl/eitem.hxx>
#include <svl/lckbitem.hxx>
#include <svtools/sfxecode.hxx>
#include <svl/itemset.hxx>
#include <svl/intitem.hxx>
#include <svtools/svparser.hxx>
#include <cppuhelper/weakref.hxx>

#include <unotools/streamwrap.hxx>

#include <osl/file.hxx>

#include <comphelper/storagehelper.hxx>
#include <unotools/mediadescriptor.hxx>
#include <comphelper/docpasswordhelper.hxx>
#include <tools/inetmime.hxx>
#include <unotools/ucblockbytes.hxx>
#include <unotools/pathoptions.hxx>
#include <svtools/asynclink.hxx>
#include <svl/inettype.hxx>
#include <ucbhelper/commandenvironment.hxx>
#include <unotools/localfilehelper.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <unotools/ucbhelper.hxx>
#include <unotools/progresshandlerwrap.hxx>
#include <ucbhelper/content.hxx>
#include <ucbhelper/interactionrequest.hxx>
#include <sot/stg.hxx>
#include <unotools/saveopt.hxx>
#include <svl/documentlockfile.hxx>
#include <com/sun/star/document/DocumentRevisionListPersistence.hpp>

#include "helper.hxx"
#include <sfx2/request.hxx>
#include <sfx2/app.hxx>
#include <sfx2/frame.hxx>
#include <sfx2/fcontnr.hxx>
#include <sfx2/docfilt.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/docfac.hxx>
#include "doc.hrc"
#include "openflag.hxx"
#include <sfx2/sfxresid.hxx>
#include "sfxacldetect.hxx"
#include <officecfg/Office/Common.hxx>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::ucb;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::io;

namespace {

#if HAVE_FEATURE_MULTIUSER_ENVIRONMENT

static const sal_Int8 LOCK_UI_NOLOCK = 0;
static const sal_Int8 LOCK_UI_SUCCEEDED = 1;
static const sal_Int8 LOCK_UI_TRY = 2;

bool IsSystemFileLockingUsed()
{
#if HAVE_FEATURE_MACOSX_SANDBOX
    return true;
#else
    return officecfg::Office::Common::Misc::UseDocumentSystemFileLocking::get();
#endif
}


bool IsOOoLockFileUsed()
{
#if HAVE_FEATURE_MACOSX_SANDBOX
    return false;
#else
    return officecfg::Office::Common::Misc::UseDocumentOOoLockFile::get();
#endif
}

bool IsLockingUsed()
{
    return officecfg::Office::Common::Misc::UseLocking::get();
}

#endif

} // anonymous namespace

class SfxMedium_Impl : boost::noncopyable
{
public:
    StreamMode m_nStorOpenMode;
    sal_uInt32 m_eError;

    ::ucbhelper::Content aContent;
    bool bUpdatePickList:1;
    bool bIsTemp:1;
    bool bDownloadDone:1;
    bool bIsStorage:1;
    bool bUseInteractionHandler:1;
    bool bAllowDefaultIntHdl:1;
    bool bDisposeStorage:1;
    bool bStorageBasedOnInStream:1;
    bool m_bSalvageMode:1;
    bool m_bVersionsAlreadyLoaded:1;
    bool m_bLocked:1;
    bool m_bGotDateTime:1;
    bool m_bRemoveBackup:1;
    bool m_bOriginallyReadOnly:1;
    bool m_bTriedStorage:1;
    bool m_bRemote:1;
    bool m_bInputStreamIsReadOnly:1;
    bool m_bInCheckIn:1;

    OUString m_aName;
    OUString m_aLogicName;
    OUString m_aLongName;

    mutable SfxItemSet* m_pSet;
    mutable INetURLObject* m_pURLObj;

    const SfxFilter* m_pFilter;
    boost::scoped_ptr<SfxFilter> m_pCustomFilter;

    SfxMedium*       pAntiImpl;
    SvStream* m_pInStream;
    SvStream* m_pOutStream;

    const SfxFilter* pOrigFilter;
    OUString    aOrigURL;
    DateTime         aExpireTime;
    SfxFrameWeakRef  wLoadTargetFrame;
    SvKeyValueIteratorRef xAttributes;

    svtools::AsynchronLink  aDoneLink;

    uno::Sequence < util::RevisionTag > aVersions;

    ::utl::TempFile*           pTempFile;

    uno::Reference<embed::XStorage> xStorage;
    uno::Reference<embed::XStorage> m_xZipStorage;
    uno::Reference<io::XInputStream> m_xInputStreamToLoadFrom;
    uno::Reference<io::XInputStream> xInputStream;
    uno::Reference<io::XStream> xStream;
    uno::Reference<io::XStream> m_xLockingStream;
    uno::Reference<task::XInteractionHandler> xInteraction;
    uno::Reference<logging::XSimpleLogRing> m_xLogRing;

    sal_uInt32                  nLastStorageError;

    OUString m_aBackupURL;

    // the following member is changed and makes sense only during saving
    // TODO/LATER: in future the signature state should be controlled by the medium not by the document
    //             in this case the member will hold this information
    sal_uInt16      m_nSignatureState;

    util::DateTime m_aDateTime;

    SfxMedium_Impl( SfxMedium* pAntiImplP );
    ~SfxMedium_Impl();

    OUString getFilterMimeType()
    { return m_pFilter == 0 ? OUString() : m_pFilter->GetMimeType(); }
};


SfxMedium_Impl::SfxMedium_Impl( SfxMedium* pAntiImplP ) :
    m_nStorOpenMode(SFX_STREAM_READWRITE),
    m_eError(SVSTREAM_OK),
    bUpdatePickList(true),
    bIsTemp( false ),
    bDownloadDone( true ),
    bIsStorage( false ),
    bUseInteractionHandler( true ),
    bAllowDefaultIntHdl( false ),
    bDisposeStorage( false ),
    bStorageBasedOnInStream( false ),
    m_bSalvageMode( false ),
    m_bVersionsAlreadyLoaded( false ),
    m_bLocked( false ),
    m_bGotDateTime( false ),
    m_bRemoveBackup( false ),
    m_bOriginallyReadOnly(false),
    m_bTriedStorage(false),
    m_bRemote(false),
    m_bInputStreamIsReadOnly(false),
    m_bInCheckIn(false),
    m_pSet(NULL),
    m_pURLObj(NULL),
    m_pFilter(NULL),
    pAntiImpl( pAntiImplP ),
    m_pInStream(NULL),
    m_pOutStream(NULL),
    pOrigFilter( 0 ),
    aExpireTime( Date( Date::SYSTEM ) + 10, Time( Time::SYSTEM ) ),
    pTempFile( NULL ),
    nLastStorageError( 0 ),
    m_nSignatureState( SIGNATURESTATE_NOSIGNATURES )
{
    aDoneLink.CreateMutex();
}


SfxMedium_Impl::~SfxMedium_Impl()
{
    aDoneLink.ClearPendingCall();

    delete pTempFile;
    delete m_pSet;
    delete m_pURLObj;
}

void SfxMedium::ResetError()
{
    pImp->m_eError = SVSTREAM_OK;
    if( pImp->m_pInStream )
        pImp->m_pInStream->ResetError();
    if( pImp->m_pOutStream )
        pImp->m_pOutStream->ResetError();
}


sal_uInt32 SfxMedium::GetLastStorageCreationState()
{
    return pImp->nLastStorageError;
}


void SfxMedium::AddLog( const OUString& aMessage )
{
    if ( !pImp->m_xLogRing.is() )
    {
        try
        {
            Reference<XComponentContext> xContext( ::comphelper::getProcessComponentContext() );
            pImp->m_xLogRing.set( logging::DocumentIOLogRing::get(xContext) );
        }
        catch( const uno::Exception& )
        {}
    }

    if ( pImp->m_xLogRing.is() )
        pImp->m_xLogRing->logString( aMessage );
}


void SfxMedium::SetError( sal_uInt32 nError, const OUString& aLogMessage )
{
    pImp->m_eError = nError;
    if ( pImp->m_eError != ERRCODE_NONE && !aLogMessage.isEmpty() )
        AddLog( aLogMessage );
}


sal_uInt32 SfxMedium::GetErrorCode() const
{
    sal_uInt32 lError = pImp->m_eError;
    if(!lError && pImp->m_pInStream)
        lError = pImp->m_pInStream->GetErrorCode();
    if(!lError && pImp->m_pOutStream)
        lError = pImp->m_pOutStream->GetErrorCode();
    return lError;
}


void SfxMedium::CheckFileDate( const util::DateTime& aInitDate )
{
    GetInitFileDate( true );
    if ( pImp->m_aDateTime.Seconds != aInitDate.Seconds
      || pImp->m_aDateTime.Minutes != aInitDate.Minutes
      || pImp->m_aDateTime.Hours != aInitDate.Hours
      || pImp->m_aDateTime.Day != aInitDate.Day
      || pImp->m_aDateTime.Month != aInitDate.Month
      || pImp->m_aDateTime.Year != aInitDate.Year )
    {
        uno::Reference< task::XInteractionHandler > xHandler = GetInteractionHandler();

        if ( xHandler.is() )
        {
            try
            {
                ::rtl::Reference< ::ucbhelper::InteractionRequest > xInteractionRequestImpl = new ::ucbhelper::InteractionRequest( uno::makeAny(
                    document::ChangedByOthersRequest() ) );
                uno::Sequence< uno::Reference< task::XInteractionContinuation > > aContinuations( 3 );
                aContinuations[0] = new ::ucbhelper::InteractionAbort( xInteractionRequestImpl.get() );
                aContinuations[1] = new ::ucbhelper::InteractionApprove( xInteractionRequestImpl.get() );
                xInteractionRequestImpl->setContinuations( aContinuations );

                xHandler->handle( xInteractionRequestImpl.get() );

                ::rtl::Reference< ::ucbhelper::InteractionContinuation > xSelected = xInteractionRequestImpl->getSelection();
                if ( uno::Reference< task::XInteractionAbort >( xSelected.get(), uno::UNO_QUERY ).is() )
                {
                    SetError( ERRCODE_ABORT, OUString( OSL_LOG_PREFIX  ) );
                }
            }
            catch ( const uno::Exception& )
            {}
        }
    }
}


bool SfxMedium::DocNeedsFileDateCheck()
{
    return ( !IsReadOnly() && ::utl::LocalFileHelper::IsLocalFile( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) ) );
}


util::DateTime SfxMedium::GetInitFileDate( bool bIgnoreOldValue )
{
    if ( ( bIgnoreOldValue || !pImp->m_bGotDateTime ) && !pImp->m_aLogicName.isEmpty() )
    {
        try
        {
            uno::Reference< ::com::sun::star::ucb::XCommandEnvironment > xDummyEnv;
            ::ucbhelper::Content aContent( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ), xDummyEnv, comphelper::getProcessComponentContext() );

            aContent.getPropertyValue("DateModified") >>= pImp->m_aDateTime;
            pImp->m_bGotDateTime = true;
        }
        catch ( const ::com::sun::star::uno::Exception& )
        {
        }
    }

    return pImp->m_aDateTime;
}


Reference < XContent > SfxMedium::GetContent() const
{
    if ( !pImp->aContent.get().is() )
    {
        Reference < ::com::sun::star::ucb::XContent > xContent;
        Reference < ::com::sun::star::ucb::XCommandEnvironment > xEnv;

        SFX_ITEMSET_ARG( pImp->m_pSet, pItem, SfxUnoAnyItem, SID_CONTENT, false);
        if ( pItem )
            pItem->GetValue() >>= xContent;

        if ( xContent.is() )
        {
            try
            {
                pImp->aContent = ::ucbhelper::Content( xContent, xEnv, comphelper::getProcessComponentContext() );
            }
            catch ( const Exception& )
            {
            }
        }
        else
        {
            // TODO: SAL_WARN( "sfx.doc", "SfxMedium::GetContent()\nCreate Content? This code exists as fallback only. Please clarify, why its used.");
            OUString aURL;
            if ( !pImp->m_aName.isEmpty() )
                ::utl::LocalFileHelper::ConvertPhysicalNameToURL( pImp->m_aName, aURL );
            else if ( !pImp->m_aLogicName.isEmpty() )
                aURL = GetURLObject().GetMainURL( INetURLObject::NO_DECODE );
            if (!aURL.isEmpty() )
                ::ucbhelper::Content::create( aURL, xEnv, comphelper::getProcessComponentContext(), pImp->aContent );
        }
    }

    return pImp->aContent.get();
}


OUString SfxMedium::GetBaseURL( bool bForSaving )
{
    OUString aBaseURL;
    const SfxStringItem* pBaseURLItem = static_cast<const SfxStringItem*>( GetItemSet()->GetItem(SID_DOC_BASEURL) );
    if ( pBaseURLItem )
        aBaseURL = pBaseURLItem->GetValue();
    else if ( GetContent().is() )
    {
        try
        {
            Any aAny = pImp->aContent.getPropertyValue("BaseURI");
            aAny >>= aBaseURL;
        }
        catch ( const ::com::sun::star::uno::Exception& )
        {
        }

        if ( aBaseURL.isEmpty() )
            aBaseURL = GetURLObject().GetMainURL( INetURLObject::NO_DECODE );
    }

    if ( bForSaving )
    {
        SvtSaveOptions aOpt;
        bool bIsRemote = IsRemote();
        if( (bIsRemote && !aOpt.IsSaveRelINet()) || (!pImp->m_bRemote && !aOpt.IsSaveRelFSys()) )
            return OUString();
    }

    return aBaseURL;
}


SvStream* SfxMedium::GetInStream()
{
    if ( pImp->m_pInStream )
        return pImp->m_pInStream;

    if ( pImp->pTempFile )
    {
        pImp->m_pInStream = new SvFileStream(pImp->m_aName, pImp->m_nStorOpenMode);

        pImp->m_eError = pImp->m_pInStream->GetError();

        if (!pImp->m_eError && (pImp->m_nStorOpenMode & STREAM_WRITE)
                    && ! pImp->m_pInStream->IsWritable() )
        {
            pImp->m_eError = ERRCODE_IO_ACCESSDENIED;
            delete pImp->m_pInStream;
            pImp->m_pInStream = NULL;
        }
        else
            return pImp->m_pInStream;
    }

    GetMedium_Impl();

    if ( GetError() )
        return NULL;

    return pImp->m_pInStream;
}


void SfxMedium::CloseInStream()
{
    CloseInStream_Impl();
}

void SfxMedium::CloseInStream_Impl()
{
    // if there is a storage based on the InStream, we have to
    // close the storage, too, because otherwise the storage
    // would use an invalid ( deleted ) stream.
    if ( pImp->m_pInStream && pImp->xStorage.is() )
    {
        if ( pImp->bStorageBasedOnInStream )
            CloseStorage();
    }

    if ( pImp->m_pInStream && !GetContent().is() )
    {
        CreateTempFile( true );
        return;
    }

    DELETEZ( pImp->m_pInStream );
    if ( pImp->m_pSet )
        pImp->m_pSet->ClearItem( SID_INPUTSTREAM );

    CloseZipStorage_Impl();
    pImp->xInputStream.clear();

    if ( !pImp->m_pOutStream )
    {
        // output part of the stream is not used so the whole stream can be closed
        // TODO/LATER: is it correct?
        pImp->xStream.clear();
        if ( pImp->m_pSet )
            pImp->m_pSet->ClearItem( SID_STREAM );
    }
}


SvStream* SfxMedium::GetOutStream()
{
    if ( !pImp->m_pOutStream )
    {
        // Create a temp. file if there is none because we always
        // need one.
        CreateTempFile( false );

        if ( pImp->pTempFile )
        {
            // On windows we try to re-use XOutStream from xStream if that exists;
            // because opening new SvFileStream in this situation may fail with ERROR_SHARING_VIOLATION
            #ifdef WNT
            if (pImp->xStream.is())
            {
                assert(pImp->xStream->getOutputStream().is()); // need that...
                pImp->m_pOutStream = utl::UcbStreamHelper::CreateStream(
                        pImp->xStream, false);
            }
            else
            {
                pImp->m_pOutStream = new SvFileStream(
                        pImp->m_aName, STREAM_STD_READWRITE);
            }
            // On Unix don't try to re-use XOutStream from xStream if that exists;
            // it causes fdo#59022 (fails opening files via SMB on Linux)
            #else
            pImp->m_pOutStream = new SvFileStream(
                        pImp->m_aName, STREAM_STD_READWRITE);
            #endif
            CloseStorage();
        }
    }

    return pImp->m_pOutStream;
}


bool SfxMedium::CloseOutStream()
{
    CloseOutStream_Impl();
    return true;
}

bool SfxMedium::CloseOutStream_Impl()
{
    if ( pImp->m_pOutStream )
    {
        // if there is a storage based on the OutStream, we have to
        // close the storage, too, because otherwise the storage
        // would use an invalid ( deleted ) stream.
        //TODO/MBA: how to deal with this?!
        //maybe we need a new flag when the storage was created from the outstream
        if ( pImp->xStorage.is() )
        {
                CloseStorage();
        }

        delete pImp->m_pOutStream;
        pImp->m_pOutStream = NULL;
    }

    if ( !pImp->m_pInStream )
    {
        // input part of the stream is not used so the whole stream can be closed
        // TODO/LATER: is it correct?
        pImp->xStream.clear();
        if ( pImp->m_pSet )
            pImp->m_pSet->ClearItem( SID_STREAM );
    }

    return true;
}


const OUString& SfxMedium::GetPhysicalName() const
{
    if ( pImp->m_aName.isEmpty() && !pImp->m_aLogicName.isEmpty() )
        (( SfxMedium*)this)->CreateFileStream();

    // return the name then
    return pImp->m_aName;
}


void SfxMedium::CreateFileStream()
{
    ForceSynchronStream_Impl( true );
    GetInStream();
    if( pImp->m_pInStream )
    {
        CreateTempFile( false );
        pImp->bIsTemp = true;
        CloseInStream_Impl();
    }
}


bool SfxMedium::Commit()
{
    if( pImp->xStorage.is() )
        StorageCommit_Impl();
    else if( pImp->m_pOutStream  )
        pImp->m_pOutStream->Flush();
    else if( pImp->m_pInStream  )
        pImp->m_pInStream->Flush();

    if ( GetError() == SVSTREAM_OK )
    {
        // does something only in case there is a temporary file ( means aName points to different location than aLogicName )
        Transfer_Impl();
    }

    bool bResult = ( GetError() == SVSTREAM_OK );

    if ( bResult && DocNeedsFileDateCheck() )
        GetInitFileDate( true );

    // remove truncation mode from the flags
    pImp->m_nStorOpenMode &= (~STREAM_TRUNC);
    return bResult;
}


bool SfxMedium::IsStorage()
{
    if ( pImp->xStorage.is() )
        return true;

    if ( pImp->m_bTriedStorage )
        return pImp->bIsStorage;

    if ( pImp->pTempFile )
    {
        OUString aURL;
        if ( !::utl::LocalFileHelper::ConvertPhysicalNameToURL( pImp->m_aName, aURL ) )
        {
            SAL_WARN( "sfx.doc", "Physical name not convertible!");
        }
        pImp->bIsStorage = SotStorage::IsStorageFile( aURL ) && !SotStorage::IsOLEStorage( aURL);
        if ( !pImp->bIsStorage )
            pImp->m_bTriedStorage = true;
    }
    else if ( GetInStream() )
    {
        pImp->bIsStorage = SotStorage::IsStorageFile( pImp->m_pInStream ) && !SotStorage::IsOLEStorage( pImp->m_pInStream );
        if ( !pImp->m_pInStream->GetError() && !pImp->bIsStorage )
            pImp->m_bTriedStorage = true;
    }

    return pImp->bIsStorage;
}


bool SfxMedium::IsPreview_Impl()
{
    bool bPreview = false;
    SFX_ITEMSET_ARG( GetItemSet(), pPreview, SfxBoolItem, SID_PREVIEW, false);
    if ( pPreview )
        bPreview = pPreview->GetValue();
    else
    {
        SFX_ITEMSET_ARG( GetItemSet(), pFlags, SfxStringItem, SID_OPTIONS, false);
        if ( pFlags )
        {
            OUString aFileFlags = pFlags->GetValue();
            aFileFlags = aFileFlags.toAsciiUpperCase();
            if ( -1 != aFileFlags.indexOf( 'B' ) )
                bPreview = true;
        }
    }

    return bPreview;
}


void SfxMedium::StorageBackup_Impl()
{
    ::ucbhelper::Content aOriginalContent;
    Reference< ::com::sun::star::ucb::XCommandEnvironment > xDummyEnv;

    bool bBasedOnOriginalFile = ( !pImp->pTempFile && !( !pImp->m_aLogicName.isEmpty() && pImp->m_bSalvageMode )
        && !GetURLObject().GetMainURL( INetURLObject::NO_DECODE ).isEmpty()
        && ::utl::LocalFileHelper::IsLocalFile( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) )
        && ::utl::UCBContentHelper::IsDocument( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) ) );

    if ( bBasedOnOriginalFile && pImp->m_aBackupURL.isEmpty()
      && ::ucbhelper::Content::create( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ), xDummyEnv, comphelper::getProcessComponentContext(), aOriginalContent ) )
    {
        DoInternalBackup_Impl( aOriginalContent );
        if( pImp->m_aBackupURL.isEmpty() )
            SetError( ERRCODE_SFX_CANTCREATEBACKUP, OUString( OSL_LOG_PREFIX  ) );
    }
}


OUString SfxMedium::GetBackup_Impl()
{
    if ( pImp->m_aBackupURL.isEmpty() )
        StorageBackup_Impl();

    return pImp->m_aBackupURL;
}


uno::Reference < embed::XStorage > SfxMedium::GetOutputStorage()
{
    if ( GetError() )
        return uno::Reference< embed::XStorage >();

    // if the medium was constructed with a Storage: use this one, not a temp. storage
    // if a temporary storage already exists: use it
    if ( pImp->xStorage.is() && ( pImp->m_aLogicName.isEmpty() || pImp->pTempFile ) )
        return pImp->xStorage;

    // if necessary close stream that was used for reading
    if ( pImp->m_pInStream && !pImp->m_pInStream->IsWritable() )
        CloseInStream();

    DBG_ASSERT( !pImp->m_pOutStream, "OutStream in a readonly Medium?!" );

    // TODO/LATER: The current solution is to store the document temporary and then copy it to the target location;
    // in future it should be stored directly and then copied to the temporary location, since in this case no
    // file attributes have to be preserved and system copying mechanics could be used instead of streaming.
    CreateTempFileNoCopy();

    return GetStorage();
}


void SfxMedium::SetEncryptionDataToStorage_Impl()
{
    // in case media-descriptor contains password it should be used on opening
    if ( pImp->xStorage.is() && pImp->m_pSet )
    {
        uno::Sequence< beans::NamedValue > aEncryptionData;
        if ( GetEncryptionData_Impl( pImp->m_pSet, aEncryptionData ) )
        {
            // replace the password with encryption data
            pImp->m_pSet->ClearItem( SID_PASSWORD );
            pImp->m_pSet->Put( SfxUnoAnyItem( SID_ENCRYPTIONDATA, uno::makeAny( aEncryptionData ) ) );

            try
            {
                ::comphelper::OStorageHelper::SetCommonStorageEncryptionData( pImp->xStorage, aEncryptionData );
            }
            catch( const uno::Exception& )
            {
                SAL_WARN( "sfx.doc", "It must be possible to set a common password for the storage" );
                // TODO/LATER: set the error code in case of problem
                // SetError( ERRCODE_IO_GENERAL, OUString( OSL_LOG_PREFIX  ) );
            }
        }
    }
}

#if HAVE_FEATURE_MULTIUSER_ENVIRONMENT

// FIXME: Hmm actually lock files should be used for sftp: documents
// even if !HAVE_FEATURE_MULTIUSER_ENVIRONMENT. Only the use of lock
// files for *local* documents is unnecessary in that case. But
// actually, the checks for sftp: here are just wishful thinking; I
// don't this there is any support for actually editing documents
// behind sftp: URLs anyway.

// Sure, there could perhaps be a 3rd-party extension that brings UCB
// the potential to handle files behind sftp:. But there could also be
// an extension that handles some arbitrary foobar: scheme *and* it
// could be that lock files would be the correct thing to use for
// foobar: documents, too. But the hardcoded test below won't know
// that. Clearly the knowledge whether lock files should be used or
// not for some URL scheme belongs in UCB, not here.


sal_Int8 SfxMedium::ShowLockedDocumentDialog( const uno::Sequence< OUString >& aData, bool bIsLoading, bool bOwnLock )
{
    sal_Int8 nResult = LOCK_UI_NOLOCK;

    // show the interaction regarding the document opening
    uno::Reference< task::XInteractionHandler > xHandler = GetInteractionHandler();

    if ( ::svt::DocumentLockFile::IsInteractionAllowed() && xHandler.is() && ( bIsLoading || bOwnLock ) )
    {
        OUString aDocumentURL = GetURLObject().GetLastName();
        OUString aInfo;
        ::rtl::Reference< ::ucbhelper::InteractionRequest > xInteractionRequestImpl;

        if ( bOwnLock )
        {
            if ( aData.getLength() > LOCKFILE_EDITTIME_ID )
                aInfo = aData[LOCKFILE_EDITTIME_ID];

            xInteractionRequestImpl = new ::ucbhelper::InteractionRequest( uno::makeAny(
                document::OwnLockOnDocumentRequest( OUString(), uno::Reference< uno::XInterface >(), aDocumentURL, aInfo, !bIsLoading ) ) );
        }
        else /*logically therefore bIsLoading is set */
        {
            if ( aData.getLength() > LOCKFILE_EDITTIME_ID )
            {
                if ( !aData[LOCKFILE_OOOUSERNAME_ID].isEmpty() )
                    aInfo = aData[LOCKFILE_OOOUSERNAME_ID];
                else
                    aInfo = aData[LOCKFILE_SYSUSERNAME_ID];

                if ( !aInfo.isEmpty() && !aData[LOCKFILE_EDITTIME_ID].isEmpty() )
                {
                    aInfo +=  " ( " ;
                    aInfo += aData[LOCKFILE_EDITTIME_ID];
                    aInfo += " )";
                }
            }

            xInteractionRequestImpl = new ::ucbhelper::InteractionRequest( uno::makeAny(
                document::LockedDocumentRequest( OUString(), uno::Reference< uno::XInterface >(), aDocumentURL, aInfo ) ) );
        }

        uno::Sequence< uno::Reference< task::XInteractionContinuation > > aContinuations( 3 );
        aContinuations[0] = new ::ucbhelper::InteractionAbort( xInteractionRequestImpl.get() );
        aContinuations[1] = new ::ucbhelper::InteractionApprove( xInteractionRequestImpl.get() );
        aContinuations[2] = new ::ucbhelper::InteractionDisapprove( xInteractionRequestImpl.get() );
        xInteractionRequestImpl->setContinuations( aContinuations );

        xHandler->handle( xInteractionRequestImpl.get() );

        ::rtl::Reference< ::ucbhelper::InteractionContinuation > xSelected = xInteractionRequestImpl->getSelection();
        if ( uno::Reference< task::XInteractionAbort >( xSelected.get(), uno::UNO_QUERY ).is() )
        {
            SetError( ERRCODE_ABORT, OUString( OSL_LOG_PREFIX  ) );
        }
        else if ( uno::Reference< task::XInteractionDisapprove >( xSelected.get(), uno::UNO_QUERY ).is() )
        {
            // own lock on loading, user has selected to ignore the lock
            // own lock on saving, user has selected to ignore the lock
            // alien lock on loading, user has selected to edit a copy of document
            // TODO/LATER: alien lock on saving, user has selected to do SaveAs to different location
            if ( bIsLoading && !bOwnLock )
            {
                // means that a copy of the document should be opened
                GetItemSet()->Put( SfxBoolItem( SID_TEMPLATE, true ) );
            }
            else if ( bOwnLock )
                nResult = LOCK_UI_SUCCEEDED;
        }
        else // if ( XSelected == aContinuations[1] )
        {
            // own lock on loading, user has selected to open readonly
            // own lock on saving, user has selected to open readonly
            // alien lock on loading, user has selected to retry saving
            // TODO/LATER: alien lock on saving, user has selected to retry saving

            if ( bIsLoading )
                GetItemSet()->Put( SfxBoolItem( SID_DOC_READONLY, true ) );
            else
                nResult = LOCK_UI_TRY;
        }
    }
    else
    {
        if ( bIsLoading )
        {
            // if no interaction handler is provided the default answer is open readonly
            // that usually happens in case the document is loaded per API
            // so the document must be opened readonly for backward compatibility
            GetItemSet()->Put( SfxBoolItem( SID_DOC_READONLY, true ) );
        }
        else
            SetError( ERRCODE_IO_ACCESSDENIED, OUString( OSL_LOG_PREFIX  ) );

    }

    return nResult;
}

namespace
{
    bool isSuitableProtocolForLocking(const OUString & rLogicName)
    {
        INetURLObject aUrl( rLogicName );
        INetProtocol eProt = aUrl.GetProtocol();
#if HAVE_FEATURE_MACOSX_SANDBOX
        return eProt == INET_PROT_SFTP;
#else
        return eProt == INET_PROT_FILE || eProt == INET_PROT_SFTP;
#endif
    }
}

#endif // HAVE_FEATURE_MULTIUSER_ENVIRONMENT

// sets SID_DOC_READONLY if the document cannot be opened for editing
// if user cancel the loading the ERROR_ABORT is set
void SfxMedium::LockOrigFileOnDemand( bool bLoading, bool bNoUI )
{
#if !HAVE_FEATURE_MULTIUSER_ENVIRONMENT
    (void) bLoading;
    (void) bNoUI;
#else
    if (!IsLockingUsed() || GetURLObject().HasError())
        return;

    try
    {
        if ( pImp->m_bLocked && bLoading && ::utl::LocalFileHelper::IsLocalFile( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) ) )
        {
            // if the document is already locked the system locking might be temporarely off after storing
            // check whether the system file locking should be taken again
            GetLockingStream_Impl();
        }

        bool bResult = pImp->m_bLocked;

        if ( !bResult )
        {
            // no read-write access is necessary on loading if the document is explicitly opened as copy
            SFX_ITEMSET_ARG( GetItemSet(), pTemplateItem, SfxBoolItem, SID_TEMPLATE, false);
            bResult = ( bLoading && pTemplateItem && pTemplateItem->GetValue() );
        }

        if ( !bResult && !IsReadOnly() )
        {
            bool bContentReadonly = false;
            if ( bLoading && ::utl::LocalFileHelper::IsLocalFile( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) ) )
            {
                // let the original document be opened to check the possibility to open it for editing
                // and to let the writable stream stay open to hold the lock on the document
                GetLockingStream_Impl();
            }

            // "IsReadOnly" property does not allow to detect whether the file is readonly always
            // so we try always to open the file for editing
            // the file is readonly only in case the read-write stream can not be opened
            if ( bLoading && !pImp->m_xLockingStream.is() )
            {
                try
                {
                    // MediaDescriptor does this check also, the duplication should be avoided in future
                    Reference< ::com::sun::star::ucb::XCommandEnvironment > xDummyEnv;
                    ::ucbhelper::Content aContent( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ), xDummyEnv, comphelper::getProcessComponentContext() );
                    aContent.getPropertyValue("IsReadOnly") >>= bContentReadonly;
                }
                catch( const uno::Exception& ) {}

#if EXTRA_ACL_CHECK
                // This block was introduced as a fix to i#102464, but removing
                // this does not make the problem re-appear.  But leaving this
                // part would interfere with documents saved in samba share.  This
                // affects Windows only.
                if ( !bContentReadonly )
                {
                    // the file is not readonly, check the ACL

                    OUString aPhysPath;
                    if ( ::utl::LocalFileHelper::ConvertURLToPhysicalName( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ), aPhysPath ) )
                        bContentReadonly = IsReadonlyAccordingACL( aPhysPath.getStr() );
                }
#endif

                if ( bContentReadonly )
                    pImp->m_bOriginallyReadOnly = true;
            }

            // do further checks only if the file not readonly in fs
            if ( !bContentReadonly )
            {
                // the special file locking should be used only for suitable URLs
                if ( isSuitableProtocolForLocking( pImp->m_aLogicName ) )
                {

                    // in case of storing the document should request the output before locking
                    if ( bLoading )
                    {
                        // let the stream be opened to check the system file locking
                        GetMedium_Impl();
                        if (GetError() != ERRCODE_NONE) {
                            return;
                        }
                    }

                    sal_Int8 bUIStatus = LOCK_UI_NOLOCK;

                    // check whether system file locking has been used, the default value is false
                    bool bUseSystemLock = ::utl::LocalFileHelper::IsLocalFile( pImp->m_aLogicName ) && IsSystemFileLockingUsed();

                    // TODO/LATER: This implementation does not allow to detect the system lock on saving here, actually this is no big problem
                    // if system lock is used the writeable stream should be available
                    bool bHandleSysLocked = ( bLoading && bUseSystemLock && !pImp->xStream.is() && !pImp->m_pOutStream );

                    do
                    {
                        try
                        {
                            ::svt::DocumentLockFile aLockFile( pImp->m_aLogicName );
                            if ( !bHandleSysLocked )
                            {
                                try
                                {
                                    bResult = aLockFile.CreateOwnLockFile();
                                }
                                catch ( const ucb::InteractiveIOException& e )
                                {
                                    // exception means that the lock file can not be successfully accessed
                                    // in this case it should be ignored if system file locking is anyway active
                                    if ( bUseSystemLock || !IsOOoLockFileUsed() )
                                    {
                                        bResult = true;
                                        // take the ownership over the lock file
                                        aLockFile.OverwriteOwnLockFile();
                                    }
                                    else if ( e.Code == IOErrorCode_INVALID_PARAMETER )
                                    {
                                        // system file locking is not active, ask user whether he wants to open the document without any locking
                                        uno::Reference< task::XInteractionHandler > xHandler = GetInteractionHandler();

                                        if ( xHandler.is() )
                                        {
                                            ::rtl::Reference< ::ucbhelper::InteractionRequest > xIgnoreRequestImpl
                                                = new ::ucbhelper::InteractionRequest( uno::makeAny( document::LockFileIgnoreRequest() ) );

                                            uno::Sequence< uno::Reference< task::XInteractionContinuation > > aContinuations( 2 );
                                            aContinuations[0] = new ::ucbhelper::InteractionAbort( xIgnoreRequestImpl.get() );
                                            aContinuations[1] = new ::ucbhelper::InteractionApprove( xIgnoreRequestImpl.get() );
                                            xIgnoreRequestImpl->setContinuations( aContinuations );

                                            xHandler->handle( xIgnoreRequestImpl.get() );

                                            ::rtl::Reference< ::ucbhelper::InteractionContinuation > xSelected = xIgnoreRequestImpl->getSelection();
                                            bResult = uno::Reference< task::XInteractionApprove >( xSelected.get(), uno::UNO_QUERY ).is();
                                        }
                                    }
                                }
                                catch ( const uno::Exception& )
                                {
                                    // exception means that the lock file can not be successfully accessed
                                    // in this case it should be ignored if system file locking is anyway active
                                    if ( bUseSystemLock || !IsOOoLockFileUsed() )
                                    {
                                        bResult = true;
                                        // take the ownership over the lock file
                                        aLockFile.OverwriteOwnLockFile();
                                    }
                                }

                                // in case OOo locking is turned off the lock file is still written if possible
                                // but it is ignored while deciding whether the document should be opened for editing or not
                                if ( !bResult && !IsOOoLockFileUsed() )
                                {
                                    bResult = true;
                                    // take the ownership over the lock file
                                    aLockFile.OverwriteOwnLockFile();
                                }
                            }


                            if ( !bResult )
                            {
                                uno::Sequence< OUString > aData;
                                try
                                {
                                    // impossibility to get data is no real problem
                                    aData = aLockFile.GetLockData();
                                }
                                catch( const uno::Exception& )
                                {
                                }

                                bool bOwnLock = false;

                                if ( !bHandleSysLocked )
                                {
                                    uno::Sequence< OUString > aOwnData = svt::LockFileCommon::GenerateOwnEntry();
                                    bOwnLock = ( aData.getLength() > LOCKFILE_USERURL_ID
                                              && aOwnData.getLength() > LOCKFILE_USERURL_ID
                                              && aOwnData[LOCKFILE_SYSUSERNAME_ID].equals( aData[LOCKFILE_SYSUSERNAME_ID] ) );

                                    if ( bOwnLock
                                      && aOwnData[LOCKFILE_LOCALHOST_ID].equals( aData[LOCKFILE_LOCALHOST_ID] )
                                      && aOwnData[LOCKFILE_USERURL_ID].equals( aData[LOCKFILE_USERURL_ID] ) )
                                    {
                                        // this is own lock from the same installation, it could remain because of crash
                                        bResult = true;
                                    }
                                }

                                if ( !bResult && !bNoUI )
                                {
                                    bUIStatus = ShowLockedDocumentDialog( aData, bLoading, bOwnLock );
                                    if ( bUIStatus == LOCK_UI_SUCCEEDED )
                                    {
                                        // take the ownership over the lock file
                                        bResult = aLockFile.OverwriteOwnLockFile();
                                    }
                                }

                                bHandleSysLocked = false;
                            }
                        }
                        catch( const uno::Exception& )
                        {
                        }
                    } while( !bResult && bUIStatus == LOCK_UI_TRY );

                    pImp->m_bLocked = bResult;
                }
                else
                {
                    // this is no file URL, check whether the file is readonly
                    bResult = !bContentReadonly;
                }
            }
        }

        if ( !bResult && GetError() == ERRCODE_NONE )
        {
            // the error should be set in case it is storing process
            // or the document has been opened for editing explicitly
            SFX_ITEMSET_ARG( pImp->m_pSet, pReadOnlyItem, SfxBoolItem, SID_DOC_READONLY, false );

            if ( !bLoading || (pReadOnlyItem && !pReadOnlyItem->GetValue()) )
                SetError( ERRCODE_IO_ACCESSDENIED, OUString( OSL_LOG_PREFIX  ) );
            else
                GetItemSet()->Put( SfxBoolItem( SID_DOC_READONLY, true ) );
        }

        // when the file is locked, get the current file date
        if ( bResult && DocNeedsFileDateCheck() )
            GetInitFileDate( true );
    }
    catch( const uno::Exception& )
    {
        SAL_WARN( "sfx.doc", "Locking exception: high probability, that the content has not been created" );
    }
#endif
}


uno::Reference < embed::XStorage > SfxMedium::GetStorage( bool bCreateTempIfNo )
{
    if ( pImp->xStorage.is() || pImp->m_bTriedStorage )
        return pImp->xStorage;

    uno::Sequence< uno::Any > aArgs( 2 );

    // the medium should be retrieved before temporary file creation
    // to let the MediaDescriptor be filled with the streams
    GetMedium_Impl();

    if ( bCreateTempIfNo )
        CreateTempFile( false );

    GetMedium_Impl();

    if ( GetError() )
        return pImp->xStorage;

    SFX_ITEMSET_ARG( GetItemSet(), pRepairItem, SfxBoolItem, SID_REPAIRPACKAGE, false);
    if ( pRepairItem && pRepairItem->GetValue() )
    {
        // the storage should be created for repairing mode
        CreateTempFile( false );
        GetMedium_Impl();

        Reference< ::com::sun::star::ucb::XProgressHandler > xProgressHandler;
        Reference< ::com::sun::star::task::XStatusIndicator > xStatusIndicator;

        SFX_ITEMSET_ARG( GetItemSet(), pxProgressItem, SfxUnoAnyItem, SID_PROGRESS_STATUSBAR_CONTROL, false );
        if( pxProgressItem && ( pxProgressItem->GetValue() >>= xStatusIndicator ) )
            xProgressHandler = Reference< ::com::sun::star::ucb::XProgressHandler >(
                                    new utl::ProgressHandlerWrap( xStatusIndicator ) );

        uno::Sequence< beans::PropertyValue > aAddProps( 2 );
        aAddProps[0].Name = "RepairPackage";
        aAddProps[0].Value <<= true;
        aAddProps[1].Name = "StatusIndicator";
        aAddProps[1].Value <<= xProgressHandler;

        // the first arguments will be filled later
        aArgs.realloc( 3 );
        aArgs[2] <<= aAddProps;
    }

    if ( pImp->xStream.is() )
    {
        // since the storage is based on temporary stream we open it always read-write
        aArgs[0] <<= pImp->xStream;
        aArgs[1] <<= embed::ElementModes::READWRITE;
        pImp->bStorageBasedOnInStream = true;
    }
    else if ( pImp->xInputStream.is() )
    {
        // since the storage is based on temporary stream we open it always read-write
        aArgs[0] <<= pImp->xInputStream;
        aArgs[1] <<= embed::ElementModes::READ;
        pImp->bStorageBasedOnInStream = true;
    }
    else
    {
        CloseStreams_Impl();
        aArgs[0] <<= pImp->m_aName;
        aArgs[1] <<= embed::ElementModes::READ;
        pImp->bStorageBasedOnInStream = false;
    }

    try
    {
        pImp->xStorage = uno::Reference< embed::XStorage >(
                            ::comphelper::OStorageHelper::GetStorageFactory()->createInstanceWithArguments( aArgs ),
                            uno::UNO_QUERY );
    }
    catch( const uno::Exception& )
    {
        // impossibility to create the storage is no error
    }

    if( ( pImp->nLastStorageError = GetError() ) != SVSTREAM_OK )
    {
        pImp->xStorage = 0;
        if ( pImp->m_pInStream )
            pImp->m_pInStream->Seek(0);
        return uno::Reference< embed::XStorage >();
    }

    pImp->m_bTriedStorage = true;

    // TODO/LATER: Get versionlist on demand
    if ( pImp->xStorage.is() )
    {
        SetEncryptionDataToStorage_Impl();
        GetVersionList();
    }

    SFX_ITEMSET_ARG( pImp->m_pSet, pVersion, SfxInt16Item, SID_VERSION, false);

    bool bResetStorage = false;
    if ( pVersion && pVersion->GetValue() )
    {
        // Read all available versions
        if ( pImp->aVersions.getLength() )
        {
            // Search for the version fits the comment
            // The versions are numbered startign with 1, versions with
            // negative versions numbers are counted backwards from the
            // current version
            short nVersion = pVersion ? pVersion->GetValue() : 0;
            if ( nVersion<0 )
                nVersion = ( (short) pImp->aVersions.getLength() ) + nVersion;
            else if ( nVersion )
                nVersion--;

            util::RevisionTag& rTag = pImp->aVersions[nVersion];
            {
                // Open SubStorage for all versions
                uno::Reference < embed::XStorage > xSub = pImp->xStorage->openStorageElement( "Versions",
                        embed::ElementModes::READ );

                DBG_ASSERT( xSub.is(), "Version list, but no Versions!" );

                // There the version is stored as packed Stream
                uno::Reference < io::XStream > xStr = xSub->openStreamElement( rTag.Identifier, embed::ElementModes::READ );
                SvStream* pStream = utl::UcbStreamHelper::CreateStream( xStr );
                if ( pStream && pStream->GetError() == SVSTREAM_OK )
                {
                    // Unpack Stream  in TempDir
                    ::utl::TempFile aTempFile;
                    OUString          aTmpName = aTempFile.GetURL();
                    SvFileStream    aTmpStream( aTmpName, SFX_STREAM_READWRITE );

                    pStream->ReadStream( aTmpStream );
                    aTmpStream.Close();

                    // Open data as Storage
                    pImp->m_nStorOpenMode = SFX_STREAM_READONLY;
                    pImp->xStorage = comphelper::OStorageHelper::GetStorageFromURL( aTmpName, embed::ElementModes::READ );
                    pImp->bStorageBasedOnInStream = false;
                    OUString aTemp;
                    ::utl::LocalFileHelper::ConvertURLToPhysicalName( aTmpName, aTemp );
                    SetPhysicalName_Impl( aTemp );

                    pImp->bIsTemp = true;
                    GetItemSet()->Put( SfxBoolItem( SID_DOC_READONLY, true ) );
                    // TODO/MBA
                    pImp->aVersions.realloc(0);
                }
                else
                    bResetStorage = true;
            }
        }
        else
            bResetStorage = true;
    }

    if ( bResetStorage )
    {
        pImp->xStorage.clear();
        if ( pImp->m_pInStream )
            pImp->m_pInStream->Seek( 0L );
    }

    pImp->bIsStorage = pImp->xStorage.is();
    return pImp->xStorage;
}


uno::Reference< embed::XStorage > SfxMedium::GetZipStorageToSign_Impl( bool bReadOnly )
{
    if ( !GetError() && !pImp->m_xZipStorage.is() )
    {
        GetMedium_Impl();

        try
        {
            // we can not sign document if there is no stream
            // should it be possible at all?
            if ( !bReadOnly && pImp->xStream.is() )
            {
                pImp->m_xZipStorage = ::comphelper::OStorageHelper::GetStorageOfFormatFromStream( ZIP_STORAGE_FORMAT_STRING, pImp->xStream, embed::ElementModes::READWRITE );
            }
            else if ( pImp->xInputStream.is() )
            {
                pImp->m_xZipStorage = ::comphelper::OStorageHelper::GetStorageOfFormatFromInputStream( ZIP_STORAGE_FORMAT_STRING, pImp->xInputStream );
            }
        }
        catch( const uno::Exception& )
        {
            SAL_WARN( "sfx.doc", "No possibility to get readonly version of storage from medium!" );
        }

        if ( GetError() ) // do not remove warnings
            ResetError();
    }

    return pImp->m_xZipStorage;
}


void SfxMedium::CloseZipStorage_Impl()
{
    if ( pImp->m_xZipStorage.is() )
    {
        try {
            pImp->m_xZipStorage->dispose();
        } catch( const uno::Exception& )
        {}

        pImp->m_xZipStorage.clear();
    }
}

void SfxMedium::CloseStorage()
{
    if ( pImp->xStorage.is() )
    {
        uno::Reference < lang::XComponent > xComp( pImp->xStorage, uno::UNO_QUERY );
        // in the salvage mode the medium does not own the storage
        if ( pImp->bDisposeStorage && !pImp->m_bSalvageMode )
        {
            try {
                xComp->dispose();
            } catch( const uno::Exception& )
            {
                SAL_WARN( "sfx.doc", "Medium's storage is already disposed!" );
            }
        }

        pImp->xStorage.clear();
        pImp->bStorageBasedOnInStream = false;
    }

    pImp->m_bTriedStorage = false;
    pImp->bIsStorage = false;
}

void SfxMedium::CanDisposeStorage_Impl( bool bDisposeStorage )
{
    pImp->bDisposeStorage = bDisposeStorage;
}

bool SfxMedium::WillDisposeStorageOnClose_Impl()
{
    return pImp->bDisposeStorage;
}

StreamMode SfxMedium::GetOpenMode() const
{
    return pImp->m_nStorOpenMode;
}

void SfxMedium::SetOpenMode( StreamMode nStorOpen,
                             bool bDontClose )
{
    if ( pImp->m_nStorOpenMode != nStorOpen )
    {
        pImp->m_nStorOpenMode = nStorOpen;

        if( !bDontClose )
        {
            if ( pImp->xStorage.is() )
                CloseStorage();

            CloseStreams_Impl();
        }
    }
}


bool SfxMedium::UseBackupToRestore_Impl( ::ucbhelper::Content& aOriginalContent,
                                            const Reference< ::com::sun::star::ucb::XCommandEnvironment >& xComEnv )
{
    try
    {
        ::ucbhelper::Content aTransactCont( pImp->m_aBackupURL, xComEnv, comphelper::getProcessComponentContext() );

        Reference< XInputStream > aOrigInput = aTransactCont.openStream();
        aOriginalContent.writeStream( aOrigInput, true );
        return true;
    }
    catch( const Exception& )
    {
        // in case of failure here the backup file should not be removed
        // TODO/LATER: a message should be used to let user know about the backup
        pImp->m_bRemoveBackup = false;
        // TODO/LATER: needs a specific error code
        pImp->m_eError = ERRCODE_IO_GENERAL;
    }

    return false;
}


bool SfxMedium::StorageCommit_Impl()
{
    bool bResult = false;
    Reference< ::com::sun::star::ucb::XCommandEnvironment > xDummyEnv;
    ::ucbhelper::Content aOriginalContent;

    if ( pImp->xStorage.is() )
    {
        if ( !GetError() )
        {
            uno::Reference < embed::XTransactedObject > xTrans( pImp->xStorage, uno::UNO_QUERY );
            if ( xTrans.is() )
            {
                try
                {
                    xTrans->commit();
                    CloseZipStorage_Impl();
                    bResult = true;
                }
                catch ( const embed::UseBackupException& aBackupExc )
                {
                    // since the temporary file is created always now, the scenario is close to be impossible
                    if ( !pImp->pTempFile )
                    {
                        OSL_ENSURE( !pImp->m_aBackupURL.isEmpty(), "No backup on storage commit!\n" );
                        if ( !pImp->m_aBackupURL.isEmpty()
                            && ::ucbhelper::Content::create( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ),
                                                        xDummyEnv, comphelper::getProcessComponentContext(),
                                                        aOriginalContent ) )
                        {
                            // use backup to restore the file
                            // the storage has already disconnected from original location
                            CloseAndReleaseStreams_Impl();
                            if ( !UseBackupToRestore_Impl( aOriginalContent, xDummyEnv ) )
                            {
                                // connect the medium to the temporary file of the storage
                                pImp->aContent = ::ucbhelper::Content();
                                pImp->m_aName = aBackupExc.TemporaryFileURL;
                                OSL_ENSURE( !pImp->m_aName.isEmpty(), "The exception _must_ contain the temporary URL!\n" );
                            }
                        }

                        if ( !GetError() )
                            SetError( ERRCODE_IO_GENERAL, OUString( OSL_LOG_PREFIX  ) );
                    }
                }
                catch ( const uno::Exception& )
                {
                    //TODO/LATER: improve error handling
                    SetError( ERRCODE_IO_GENERAL, OUString( OSL_LOG_PREFIX  ) );
                }
            }
        }
    }

    return bResult;
}


bool SfxMedium::TransactedTransferForFS_Impl( const INetURLObject& aSource,
                                                 const INetURLObject& aDest,
                                                 const Reference< ::com::sun::star::ucb::XCommandEnvironment >& xComEnv )
{
    bool bResult = false;
    Reference< ::com::sun::star::ucb::XCommandEnvironment > xDummyEnv;
    Reference< XOutputStream > aDestStream;
    ::ucbhelper::Content aOriginalContent;

    try
    {
        aOriginalContent = ::ucbhelper::Content( aDest.GetMainURL( INetURLObject::NO_DECODE ), xComEnv, comphelper::getProcessComponentContext() );
    }
    catch ( const ::com::sun::star::ucb::CommandAbortedException& )
    {
        pImp->m_eError = ERRCODE_ABORT;
    }
    catch ( const ::com::sun::star::ucb::CommandFailedException& )
    {
        pImp->m_eError = ERRCODE_ABORT;
    }
    catch (const ::com::sun::star::ucb::ContentCreationException& ex)
    {
        pImp->m_eError = ERRCODE_IO_GENERAL;
        if (
            (ex.eError == ::com::sun::star::ucb::ContentCreationError_NO_CONTENT_PROVIDER    ) ||
            (ex.eError == ::com::sun::star::ucb::ContentCreationError_CONTENT_CREATION_FAILED)
           )
        {
            pImp->m_eError = ERRCODE_IO_NOTEXISTSPATH;
        }
    }
    catch (const ::com::sun::star::uno::Exception&)
    {
       pImp->m_eError = ERRCODE_IO_GENERAL;
    }

    if( !pImp->m_eError || (pImp->m_eError & ERRCODE_WARNING_MASK) )
    {
        if ( pImp->xStorage.is() )
            CloseStorage();

        CloseStreams_Impl();

        ::ucbhelper::Content aTempCont;
        if( ::ucbhelper::Content::create( aSource.GetMainURL( INetURLObject::NO_DECODE ), xDummyEnv, comphelper::getProcessComponentContext(), aTempCont ) )
        {
            bool bTransactStarted = false;
            SFX_ITEMSET_ARG( GetItemSet(), pOverWrite, SfxBoolItem, SID_OVERWRITE, false );
               SFX_ITEMSET_ARG( GetItemSet(), pRename, SfxBoolItem, SID_RENAME, false );
            bool bRename = pRename && pRename->GetValue();
            bool bOverWrite = pOverWrite ? pOverWrite->GetValue() : !bRename;

            try
            {
                if( bOverWrite && ::utl::UCBContentHelper::IsDocument( aDest.GetMainURL( INetURLObject::NO_DECODE ) ) )
                {
                    if( pImp->m_aBackupURL.isEmpty() )
                        DoInternalBackup_Impl( aOriginalContent );

                    if( !pImp->m_aBackupURL.isEmpty() )
                    {
                        Reference< XInputStream > aTempInput = aTempCont.openStream();
                        bTransactStarted = true;
                        aOriginalContent.setPropertyValue( "Size", uno::makeAny( (sal_Int64)0 ) );
                        aOriginalContent.writeStream( aTempInput, bOverWrite );
                        bResult = true;
                    }
                    else
                    {
                        pImp->m_eError = ERRCODE_SFX_CANTCREATEBACKUP;
                    }
                }
                else
                {
                    Reference< XInputStream > aTempInput = aTempCont.openStream();
                    aOriginalContent.writeStream( aTempInput, bOverWrite );
                    bResult = true;
                }
            }
            catch ( const ::com::sun::star::ucb::CommandAbortedException& )
            {
                pImp->m_eError = ERRCODE_ABORT;
            }
            catch ( const ::com::sun::star::ucb::CommandFailedException& )
            {
                pImp->m_eError = ERRCODE_ABORT;
            }
            catch ( const ::com::sun::star::ucb::InteractiveIOException& r )
            {
                if ( r.Code == IOErrorCode_ACCESS_DENIED )
                    pImp->m_eError = ERRCODE_IO_ACCESSDENIED;
                else if ( r.Code == IOErrorCode_NOT_EXISTING )
                    pImp->m_eError = ERRCODE_IO_NOTEXISTS;
                else if ( r.Code == IOErrorCode_CANT_READ )
                    pImp->m_eError = ERRCODE_IO_CANTREAD;
                else
                    pImp->m_eError = ERRCODE_IO_GENERAL;
            }
            catch ( const ::com::sun::star::uno::Exception& )
            {
                pImp->m_eError = ERRCODE_IO_GENERAL;
            }

               if ( bResult )
               {
                if ( pImp->pTempFile )
                {
                    pImp->pTempFile->EnableKillingFile( true );
                       delete pImp->pTempFile;
                       pImp->pTempFile = NULL;
                }
               }
            else if ( bTransactStarted )
            {
                UseBackupToRestore_Impl( aOriginalContent, xDummyEnv );
            }
        }
        else
            pImp->m_eError = ERRCODE_IO_CANTREAD;
    }

    return bResult;
}


bool SfxMedium::TryDirectTransfer( const OUString& aURL, SfxItemSet& aTargetSet )
{
    if ( GetError() )
        return false;

    // if the document had no password it should be stored without password
    // if the document had password it should be stored with the same password
    // otherwise the stream copying can not be done
    SFX_ITEMSET_ARG( &aTargetSet, pNewPassItem, SfxStringItem, SID_PASSWORD, false );
    SFX_ITEMSET_ARG( GetItemSet(), pOldPassItem, SfxStringItem, SID_PASSWORD, false );
    if ( ( !pNewPassItem && !pOldPassItem )
      || ( pNewPassItem && pOldPassItem && pNewPassItem->GetValue() == pOldPassItem->GetValue() ) )
    {
        // the filter must be the same
        SFX_ITEMSET_ARG( &aTargetSet, pNewFilterItem, SfxStringItem, SID_FILTER_NAME, false );
        SFX_ITEMSET_ARG( GetItemSet(), pOldFilterItem, SfxStringItem, SID_FILTER_NAME, false );
        if ( pNewFilterItem && pOldFilterItem && pNewFilterItem->GetValue() == pOldFilterItem->GetValue() )
        {
            // get the input stream and copy it
            // in case of success return true
            uno::Reference< io::XInputStream > xInStream = GetInputStream();

            ResetError();
            if ( xInStream.is() )
            {
                try
                {
                    uno::Reference< io::XSeekable > xSeek( xInStream, uno::UNO_QUERY );
                    sal_Int64 nPos = 0;
                    if ( xSeek.is() )
                    {
                        nPos = xSeek->getPosition();
                        xSeek->seek( 0 );
                    }

                    uno::Reference < ::com::sun::star::ucb::XCommandEnvironment > xEnv;
                    ::ucbhelper::Content aTargetContent( aURL, xEnv, comphelper::getProcessComponentContext() );

                    InsertCommandArgument aInsertArg;
                    aInsertArg.Data = xInStream;
                       SFX_ITEMSET_ARG( &aTargetSet, pRename, SfxBoolItem, SID_RENAME, false );
                       SFX_ITEMSET_ARG( &aTargetSet, pOverWrite, SfxBoolItem, SID_OVERWRITE, false );
                       if ( (pOverWrite && !pOverWrite->GetValue()) // argument says: never overwrite
                         || (pRename && pRename->GetValue()) ) // argument says: rename file
                        aInsertArg.ReplaceExisting = false;
                       else
                        aInsertArg.ReplaceExisting = true; // default is overwrite existing files

                    Any aCmdArg;
                    aCmdArg <<= aInsertArg;
                    aTargetContent.executeCommand( OUString( "insert"  ),
                                                    aCmdArg );

                    if ( xSeek.is() )
                        xSeek->seek( nPos );

                    return true;
                }
                catch( const uno::Exception& )
                {}
            }
        }
    }

    return false;
}


void SfxMedium::Transfer_Impl()
{
    // The transfer is required only in two cases: either if there is a temporary file or if there is a salvage item
    OUString aNameURL;
    if ( pImp->pTempFile )
        aNameURL = pImp->pTempFile->GetURL();
    else if ( !pImp->m_aLogicName.isEmpty() && pImp->m_bSalvageMode )
    {
        // makes sence only in case logic name is set
        if ( !::utl::LocalFileHelper::ConvertPhysicalNameToURL( pImp->m_aName, aNameURL ) )
            SAL_WARN( "sfx.doc", "The medium name is not convertible!" );
    }

    if ( !aNameURL.isEmpty() && ( !pImp->m_eError || (pImp->m_eError & ERRCODE_WARNING_MASK) ) )
    {
        SAL_INFO( "sfx.doc", "SfxMedium::Transfer_Impl, copying to target" );

        Reference < ::com::sun::star::ucb::XCommandEnvironment > xEnv;
        Reference< XOutputStream > rOutStream;

        // in case an output stream is provided from outside and the URL is correct
        // commit to the stream
        if (pImp->m_aLogicName.startsWith("private:stream"))
        {
            // TODO/LATER: support storing to SID_STREAM
               SFX_ITEMSET_ARG( pImp->m_pSet, pOutStreamItem, SfxUnoAnyItem, SID_OUTPUTSTREAM, false);
             if( pOutStreamItem && ( pOutStreamItem->GetValue() >>= rOutStream ) )
            {
                if ( pImp->xStorage.is() )
                    CloseStorage();

                CloseStreams_Impl();

                INetURLObject aSource( aNameURL );
                ::ucbhelper::Content aTempCont;
                if( ::ucbhelper::Content::create( aSource.GetMainURL( INetURLObject::NO_DECODE ), xEnv, comphelper::getProcessComponentContext(), aTempCont ) )
                {
                    try
                    {
                        sal_Int32 nRead;
                        sal_Int32 nBufferSize = 32767;
                        Sequence < sal_Int8 > aSequence ( nBufferSize );
                        Reference< XInputStream > aTempInput = aTempCont.openStream();

                        do
                        {
                            nRead = aTempInput->readBytes ( aSequence, nBufferSize );
                            if ( nRead < nBufferSize )
                            {
                                Sequence < sal_Int8 > aTempBuf ( aSequence.getConstArray(), nRead );
                                rOutStream->writeBytes ( aTempBuf );
                            }
                            else
                                rOutStream->writeBytes ( aSequence );
                        }
                        while ( nRead == nBufferSize );

                        // remove temporary file
                        if ( pImp->pTempFile )
                        {
                            pImp->pTempFile->EnableKillingFile( true );
                            delete pImp->pTempFile;
                            pImp->pTempFile = NULL;
                        }
                    }
                    catch( const Exception& )
                    {}
                }
               }
            else
            {
                SAL_WARN( "sfx.doc", "Illegal Output stream parameter!" );
                SetError( ERRCODE_IO_GENERAL, OUString( OSL_LOG_PREFIX  ) );
            }

            // free the reference
            if ( pImp->m_pSet )
                pImp->m_pSet->ClearItem( SID_OUTPUTSTREAM );

            return;
        }

        GetContent();
        if ( !pImp->aContent.get().is() )
        {
            pImp->m_eError = ERRCODE_IO_NOTEXISTS;
            return;
        }

        SFX_ITEMSET_ARG( GetItemSet(), pSegmentSize, SfxInt32Item, SID_SEGMENTSIZE, false);
        if ( pSegmentSize )
        {
            // this file must be stored into a disk spanned package
            try
            {
                uno::Reference < embed::XStorage > xStor = comphelper::OStorageHelper::GetStorageFromURL( GetName(),
                        embed::ElementModes::READWRITE | embed::ElementModes::TRUNCATE );

                // set segment size property; package will automatically be divided in pieces fitting
                // into this size
                ::com::sun::star::uno::Any aAny;
                aAny <<= pSegmentSize->GetValue();

                uno::Reference < beans::XPropertySet > xSet( pImp->xStorage, uno::UNO_QUERY );
                xSet->setPropertyValue("SegmentSize", aAny );

                // copy the temporary storage into the disk spanned package
                GetStorage()->copyToStorage( xStor );
                uno::Reference < embed::XTransactedObject > xTrans( pImp->xStorage, uno::UNO_QUERY );
                if ( xTrans.is() )
                    xTrans->commit();

            }
            catch ( const uno::Exception& )
            {
                //TODO/MBA: error handling
            }
            return;
        }

        INetURLObject aDest( GetURLObject() );

        // source is the temp file written so far
        INetURLObject aSource( aNameURL );

        // a special case, an interaction handler should be used for
        // authentication in case it is available
        Reference< ::com::sun::star::ucb::XCommandEnvironment > xComEnv;
           Reference< ::com::sun::star::task::XInteractionHandler > xInteractionHandler = GetInteractionHandler();
        if (xInteractionHandler.is())
            xComEnv = new ::ucbhelper::CommandEnvironment( xInteractionHandler,
                                                      Reference< ::com::sun::star::ucb::XProgressHandler >() );

        OUString aDestURL( aDest.GetMainURL( INetURLObject::NO_DECODE ) );

        if ( ::utl::LocalFileHelper::IsLocalFile( aDestURL ) || !aDest.removeSegment() )
        {
            TransactedTransferForFS_Impl( aSource, aDest, xComEnv );

            // Hideous - no clean way to do this, so we re-open the file just to fsync it
            osl::File aFile( aDestURL );
            if ( aFile.open( osl_File_OpenFlag_Write ) == osl::FileBase::E_None )
            {
                aFile.sync();
                SAL_INFO( "sfx.doc", "fsync'd saved file '" << aDestURL << "'" );
                aFile.close();
            }
        }
        else
        {
            // create content for the parent folder and call transfer on that content with the source content
            // and the destination file name as parameters
            ::ucbhelper::Content aSourceContent;
            ::ucbhelper::Content aTransferContent;

            ::ucbhelper::Content aDestContent;
            ::ucbhelper::Content::create( aDestURL, xComEnv, comphelper::getProcessComponentContext(), aDestContent );
            // For checkin, we need the object URL, not the parent folder:
            if ( !IsInCheckIn( ) )
            {
                // Get the parent URL from the XChild if possible: why would the URL necessarily have
                // a hierarchical path? It's not always the case for CMIS.
                Reference< ::com::sun::star::container::XChild> xChild( aDestContent.get(), uno::UNO_QUERY );
                OUString sParentUrl;
                if ( xChild.is( ) )
                {
                    Reference< ::com::sun::star::ucb::XContent > xParent( xChild->getParent( ), uno::UNO_QUERY );
                    if ( xParent.is( ) )
                    {
                        sParentUrl = xParent->getIdentifier( )->getContentIdentifier();
                    }
                }

                if ( sParentUrl.isEmpty() )
                    aDestURL = aDest.GetMainURL( INetURLObject::NO_DECODE );
                        // adjust to above aDest.removeSegment()
                else
                    aDestURL = sParentUrl;
            }

            // LongName wasn't defined anywhere, only used here... get the Title instead
            // as it's less probably empty
            OUString aFileName;
            Any aAny = aDestContent.getPropertyValue("Title");
            aAny >>= aFileName;
            aAny = aDestContent.getPropertyValue( OUString("ObjectId" ) );
            OUString sObjectId;
            aAny >>= sObjectId;
            if ( aFileName.isEmpty() )
                aFileName = GetURLObject().getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::DECODE_WITH_CHARSET );

            try
            {
                aTransferContent = ::ucbhelper::Content( aDestURL, xComEnv, comphelper::getProcessComponentContext() );
            }
            catch (const ::com::sun::star::ucb::ContentCreationException& ex)
            {
                pImp->m_eError = ERRCODE_IO_GENERAL;
                if (
                    (ex.eError == ::com::sun::star::ucb::ContentCreationError_NO_CONTENT_PROVIDER    ) ||
                    (ex.eError == ::com::sun::star::ucb::ContentCreationError_CONTENT_CREATION_FAILED)
                   )
                {
                    pImp->m_eError = ERRCODE_IO_NOTEXISTSPATH;
                }
            }
            catch (const ::com::sun::star::uno::Exception&)
            {
                pImp->m_eError = ERRCODE_IO_GENERAL;
            }

            if ( !pImp->m_eError || (pImp->m_eError & ERRCODE_WARNING_MASK) )
            {
                // free resources, otherwise the transfer may fail
                if ( pImp->xStorage.is() )
                    CloseStorage();

                CloseStreams_Impl();

                ::ucbhelper::Content::create( aSource.GetMainURL( INetURLObject::NO_DECODE ), xEnv, comphelper::getProcessComponentContext(), aSourceContent );

                // check for external parameters that may customize the handling of NameClash situations
                SFX_ITEMSET_ARG( GetItemSet(), pRename, SfxBoolItem, SID_RENAME, false );
                SFX_ITEMSET_ARG( GetItemSet(), pOverWrite, SfxBoolItem, SID_OVERWRITE, false );
                sal_Int32 nNameClash;
                if ( pOverWrite && !pOverWrite->GetValue() )
                    // argument says: never overwrite
                    nNameClash = NameClash::ERROR;
                else if ( pRename && pRename->GetValue() )
                    // argument says: rename file
                    nNameClash = NameClash::RENAME;
                else
                    // default is overwrite existing files
                    nNameClash = NameClash::OVERWRITE;

                try
                {
                    OUString aMimeType = pImp->getFilterMimeType();
                    ::ucbhelper::InsertOperation eOperation = ::ucbhelper::InsertOperation_COPY;
                    bool bMajor = false;
                    OUString sComment;
                    if ( IsInCheckIn( ) )
                    {
                        eOperation = ::ucbhelper::InsertOperation_CHECKIN;
                        SFX_ITEMSET_ARG( GetItemSet(), pMajor, SfxBoolItem, SID_DOCINFO_MAJOR, false );
                        bMajor = pMajor && pMajor->GetValue( );
                        SFX_ITEMSET_ARG( GetItemSet(), pComments, SfxStringItem, SID_DOCINFO_COMMENTS, false );
                        if ( pComments )
                            sComment = pComments->GetValue( );
                    }
                    OUString sResultURL;
                    if (!aTransferContent.transferContent( aSourceContent, eOperation,
                                aFileName, nNameClash, aMimeType, bMajor, sComment, &sResultURL, sObjectId))
                        pImp->m_eError = ERRCODE_IO_GENERAL;
                    else if ( !sResultURL.isEmpty( ) )  // Likely to happen only for checkin
                        SwitchDocumentToFile( sResultURL );
                }
                catch ( const ::com::sun::star::ucb::CommandAbortedException& )
                {
                    pImp->m_eError = ERRCODE_ABORT;
                }
                catch ( const ::com::sun::star::ucb::CommandFailedException& )
                {
                    pImp->m_eError = ERRCODE_ABORT;
                }
                catch ( const ::com::sun::star::ucb::InteractiveIOException& r )
                {
                    if ( r.Code == IOErrorCode_ACCESS_DENIED )
                        pImp->m_eError = ERRCODE_IO_ACCESSDENIED;
                    else if ( r.Code == IOErrorCode_NOT_EXISTING )
                        pImp->m_eError = ERRCODE_IO_NOTEXISTS;
                    else if ( r.Code == IOErrorCode_CANT_READ )
                        pImp->m_eError = ERRCODE_IO_CANTREAD;
                    else
                        pImp->m_eError = ERRCODE_IO_GENERAL;
                }
                catch ( const ::com::sun::star::uno::Exception& )
                {
                    pImp->m_eError = ERRCODE_IO_GENERAL;
                }

                // do not switch from temporary file in case of nonfile protocol
            }
        }

        if ( ( !pImp->m_eError || (pImp->m_eError & ERRCODE_WARNING_MASK) ) && !pImp->pTempFile )
        {
            // without a TempFile the physical and logical name should be the same after successful transfer
              ::utl::LocalFileHelper::ConvertURLToPhysicalName(
                  GetURLObject().GetMainURL( INetURLObject::NO_DECODE ), pImp->m_aName );
            pImp->m_bSalvageMode = false;
        }
    }
}


void SfxMedium::DoInternalBackup_Impl( const ::ucbhelper::Content& aOriginalContent,
                                       const OUString& aPrefix,
                                       const OUString& aExtension,
                                       const OUString& aDestDir )
{
    if ( !pImp->m_aBackupURL.isEmpty() )
        return; // the backup was done already

    ::utl::TempFile aTransactTemp( aPrefix, true, &aExtension, &aDestDir );

    INetURLObject aBackObj( aTransactTemp.GetURL() );
    OUString aBackupName = aBackObj.getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::DECODE_WITH_CHARSET );

    Reference < ::com::sun::star::ucb::XCommandEnvironment > xDummyEnv;
    ::ucbhelper::Content aBackupCont;
    if( ::ucbhelper::Content::create( aDestDir, xDummyEnv, comphelper::getProcessComponentContext(), aBackupCont ) )
    {
        try
        {
            OUString sMimeType = pImp->getFilterMimeType();
            if( aBackupCont.transferContent( aOriginalContent,
                                            ::ucbhelper::InsertOperation_COPY,
                                            aBackupName,
                                            NameClash::OVERWRITE,
                                            sMimeType ) )
            {
                pImp->m_aBackupURL = aBackObj.GetMainURL( INetURLObject::NO_DECODE );
                pImp->m_bRemoveBackup = true;
            }
        }
        catch( const Exception& )
        {}
    }

    if ( pImp->m_aBackupURL.isEmpty() )
        aTransactTemp.EnableKillingFile();
}


void SfxMedium::DoInternalBackup_Impl( const ::ucbhelper::Content& aOriginalContent )
{
    if ( !pImp->m_aBackupURL.isEmpty() )
        return; // the backup was done already

    OUString aFileName =  GetURLObject().getName( INetURLObject::LAST_SEGMENT,
                                                        true,
                                                        INetURLObject::NO_DECODE );

    sal_Int32 nPrefixLen = aFileName.lastIndexOf( '.' );
    OUString aPrefix = ( nPrefixLen == -1 ) ? aFileName : aFileName.copy( 0, nPrefixLen );
    OUString aExtension = ( nPrefixLen == -1 ) ? OUString() : aFileName.copy( nPrefixLen );
    OUString aBakDir = SvtPathOptions().GetBackupPath();

    // create content for the parent folder ( = backup folder )
    ::ucbhelper::Content  aContent;
    Reference < ::com::sun::star::ucb::XCommandEnvironment > xEnv;
    if( ::utl::UCBContentHelper::ensureFolder(comphelper::getProcessComponentContext(), xEnv, aBakDir, aContent) )
        DoInternalBackup_Impl( aOriginalContent, aPrefix, aExtension, aBakDir );

    if ( pImp->m_aBackupURL.isEmpty() )
    {
        // the copiing to the backup catalog failed ( for example because
        // of using an encrypted partition as target catalog )
        // since the user did not specify to make backup explicitly
        // office should try to make backup in another place,
        // target catalog does not look bad for this case ( and looks
        // to be the only way for encrypted partitions )

        INetURLObject aDest = GetURLObject();
        if ( aDest.removeSegment() )
            DoInternalBackup_Impl( aOriginalContent, aPrefix, aExtension, aDest.GetMainURL( INetURLObject::NO_DECODE ) );
    }
}



void SfxMedium::DoBackup_Impl()
{
    // source file name is the logical name of this medium
    INetURLObject aSource( GetURLObject() );

    // there is nothing to backup in case source file does not exist
    if ( !::utl::UCBContentHelper::IsDocument( aSource.GetMainURL( INetURLObject::NO_DECODE ) ) )
        return;

    bool        bSuccess = false;

    // get path for backups
    OUString aBakDir = SvtPathOptions().GetBackupPath();
    if( !aBakDir.isEmpty() )
    {
        // create content for the parent folder ( = backup folder )
        ::ucbhelper::Content  aContent;
        Reference < ::com::sun::star::ucb::XCommandEnvironment > xEnv;
        if( ::utl::UCBContentHelper::ensureFolder(comphelper::getProcessComponentContext(), xEnv, aBakDir, aContent) )
        {
            // save as ".bak" file
            INetURLObject aDest( aBakDir );
            aDest.insertName( aSource.getName() );
            aDest.setExtension( "bak" );
            OUString aFileName = aDest.getName( INetURLObject::LAST_SEGMENT, true, INetURLObject::DECODE_WITH_CHARSET );

            // create a content for the source file
            ::ucbhelper::Content aSourceContent;
            if ( ::ucbhelper::Content::create( aSource.GetMainURL( INetURLObject::NO_DECODE ), xEnv, comphelper::getProcessComponentContext(), aSourceContent ) )
            {
                try
                {
                    // do the transfer ( copy source file to backup dir )
                    OUString sMimeType = pImp->getFilterMimeType();
                    bSuccess = aContent.transferContent( aSourceContent,
                                                        ::ucbhelper::InsertOperation_COPY,
                                                        aFileName,
                                                        NameClash::OVERWRITE,
                                                        sMimeType );
                    if( bSuccess )
                    {
                        pImp->m_aBackupURL = aDest.GetMainURL( INetURLObject::NO_DECODE );
                        pImp->m_bRemoveBackup = false;
                    }
                }
                catch ( const ::com::sun::star::uno::Exception& )
                {
                }
            }
        }
    }

    if ( !bSuccess )
    {
        pImp->m_eError = ERRCODE_SFX_CANTCREATEBACKUP;
    }
}


void SfxMedium::ClearBackup_Impl()
{
    if( pImp->m_bRemoveBackup )
    {
        // currently a document is always stored in a new medium,
        // thus if a backup can not be removed the backup URL should not be cleaned
        if ( !pImp->m_aBackupURL.isEmpty() )
        {
            if ( ::utl::UCBContentHelper::Kill( pImp->m_aBackupURL ) )
            {
                pImp->m_bRemoveBackup = false;
                pImp->m_aBackupURL = "";
            }
            else
            {

                SAL_WARN( "sfx.doc", "Couldn't remove backup file!");
            }
        }
    }
    else
        pImp->m_aBackupURL = "";
}


void SfxMedium::GetLockingStream_Impl()
{
    if ( ::utl::LocalFileHelper::IsLocalFile( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) )
      && !pImp->m_xLockingStream.is() )
    {
        SFX_ITEMSET_ARG( pImp->m_pSet, pWriteStreamItem, SfxUnoAnyItem, SID_STREAM, false);
        if ( pWriteStreamItem )
            pWriteStreamItem->GetValue() >>= pImp->m_xLockingStream;

        if ( !pImp->m_xLockingStream.is() )
        {
            // open the original document
            uno::Sequence< beans::PropertyValue > xProps;
            TransformItems( SID_OPENDOC, *GetItemSet(), xProps );
            utl::MediaDescriptor aMedium( xProps );

            aMedium.addInputStreamOwnLock();

            uno::Reference< io::XInputStream > xInputStream;
            aMedium[utl::MediaDescriptor::PROP_STREAM()] >>= pImp->m_xLockingStream;
            aMedium[utl::MediaDescriptor::PROP_INPUTSTREAM()] >>= xInputStream;

            if ( !pImp->pTempFile && pImp->m_aName.isEmpty() )
            {
                // the medium is still based on the original file, it makes sence to initialize the streams
                if ( pImp->m_xLockingStream.is() )
                    pImp->xStream = pImp->m_xLockingStream;

                if ( xInputStream.is() )
                    pImp->xInputStream = xInputStream;

                if ( !pImp->xInputStream.is() && pImp->xStream.is() )
                    pImp->xInputStream = pImp->xStream->getInputStream();
            }
        }
    }
}


void SfxMedium::GetMedium_Impl()
{
    if ( !pImp->m_pInStream )
    {
        pImp->bDownloadDone = false;
        Reference< ::com::sun::star::task::XInteractionHandler > xInteractionHandler = GetInteractionHandler();

        //TODO/MBA: need support for SID_STREAM
        SFX_ITEMSET_ARG( pImp->m_pSet, pWriteStreamItem, SfxUnoAnyItem, SID_STREAM, false);
        SFX_ITEMSET_ARG( pImp->m_pSet, pInStreamItem, SfxUnoAnyItem, SID_INPUTSTREAM, false);
        if ( pWriteStreamItem )
        {
            pWriteStreamItem->GetValue() >>= pImp->xStream;

            if ( pInStreamItem )
                pInStreamItem->GetValue() >>= pImp->xInputStream;

            if ( !pImp->xInputStream.is() && pImp->xStream.is() )
                pImp->xInputStream = pImp->xStream->getInputStream();
        }
        else if ( pInStreamItem )
        {
            pInStreamItem->GetValue() >>= pImp->xInputStream;
        }
        else
        {
            uno::Sequence < beans::PropertyValue > xProps;
            OUString aFileName;
            if (!pImp->m_aName.isEmpty())
            {
                if ( !::utl::LocalFileHelper::ConvertPhysicalNameToURL( pImp->m_aName, aFileName ) )
                {
                    SAL_WARN( "sfx.doc", "Physical name not convertible!");
                }
            }
            else
                aFileName = GetName();

            // in case the temporary file exists the streams should be initialized from it,
            // but the original MediaDescriptor should not be changed
            bool bFromTempFile = ( pImp->pTempFile != NULL );

            if ( !bFromTempFile )
            {
                GetItemSet()->Put( SfxStringItem( SID_FILE_NAME, aFileName ) );
                if( !(pImp->m_nStorOpenMode & STREAM_WRITE) )
                    GetItemSet()->Put( SfxBoolItem( SID_DOC_READONLY, true ) );
                if (xInteractionHandler.is())
                    GetItemSet()->Put( SfxUnoAnyItem( SID_INTERACTIONHANDLER, makeAny(xInteractionHandler) ) );
            }

            if ( pImp->m_xInputStreamToLoadFrom.is() )
            {
                pImp->xInputStream = pImp->m_xInputStreamToLoadFrom;
                pImp->xInputStream->skipBytes(0);
                if (pImp->m_bInputStreamIsReadOnly)
                    GetItemSet()->Put( SfxBoolItem( SID_DOC_READONLY, true ) );
            }
            else
            {
                TransformItems( SID_OPENDOC, *GetItemSet(), xProps );
                utl::MediaDescriptor aMedium( xProps );

                if ( pImp->m_xLockingStream.is() && !bFromTempFile )
                {
                    // the medium is not based on the temporary file, so the original stream can be used
                    pImp->xStream = pImp->m_xLockingStream;
                }
                else
                {
                    if ( bFromTempFile )
                    {
                        aMedium[utl::MediaDescriptor::PROP_URL()] <<= OUString( aFileName );
                        aMedium.erase( utl::MediaDescriptor::PROP_READONLY() );
                        aMedium.addInputStream();
                    }
                    else if ( ::utl::LocalFileHelper::IsLocalFile( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) ) )
                    {
                        // use the special locking approach only for file URLs
                        aMedium.addInputStreamOwnLock();
                    }
                    else
                        aMedium.addInputStream();

                    // the ReadOnly property set in aMedium is ignored
                    // the check is done in LockOrigFileOnDemand() for file and non-file URLs

                    //TODO/MBA: what happens if property is not there?!
                    aMedium[utl::MediaDescriptor::PROP_STREAM()] >>= pImp->xStream;
                    aMedium[utl::MediaDescriptor::PROP_INPUTSTREAM()] >>= pImp->xInputStream;
                }

                GetContent();
                if ( !pImp->xInputStream.is() && pImp->xStream.is() )
                    pImp->xInputStream = pImp->xStream->getInputStream();
            }

            if ( !bFromTempFile )
            {
                //TODO/MBA: need support for SID_STREAM
                if ( pImp->xStream.is() )
                    GetItemSet()->Put( SfxUsrAnyItem( SID_STREAM, makeAny( pImp->xStream ) ) );

                GetItemSet()->Put( SfxUsrAnyItem( SID_INPUTSTREAM, makeAny( pImp->xInputStream ) ) );
            }
        }

        //TODO/MBA: ErrorHandling - how to transport error from MediaDescriptor
        if ( !GetError() && !pImp->xStream.is() && !pImp->xInputStream.is() )
            SetError( ERRCODE_IO_ACCESSDENIED, OUString( OSL_LOG_PREFIX  ) );

        if ( !GetError() )
        {
            if ( pImp->xStream.is() )
                pImp->m_pInStream = utl::UcbStreamHelper::CreateStream( pImp->xStream );
            else if ( pImp->xInputStream.is() )
                pImp->m_pInStream = utl::UcbStreamHelper::CreateStream( pImp->xInputStream );
        }

        pImp->bDownloadDone = true;
        pImp->aDoneLink.ClearPendingCall();
        sal_uIntPtr nError = GetError();
        pImp->aDoneLink.Call( (void*)nError );
    }
}


bool SfxMedium::IsRemote()
{
    return pImp->m_bRemote;
}



void SfxMedium::SetUpdatePickList(bool bVal)
{
    pImp->bUpdatePickList = bVal;
}


bool SfxMedium::IsUpdatePickList() const
{
    return pImp->bUpdatePickList;
}

void SfxMedium::SetLongName(const OUString &rName)
{
    pImp->m_aLongName = rName;
}

const OUString& SfxMedium::GetLongName() const
{
    return pImp->m_aLongName;
}

void SfxMedium::SetDoneLink( const Link& rLink )
{
    pImp->aDoneLink = rLink;
}

void SfxMedium::Download( const Link& aLink )
{
    SetDoneLink( aLink );
    GetInStream();
    if ( pImp->m_pInStream && !aLink.IsSet() )
    {
        while( !pImp->bDownloadDone )
            Application::Yield();
    }
}


void SfxMedium::Init_Impl()
/*  [Description]
    Includes a valid:: sun:: com:: star:: util:: URL (If a file name was
    previously in there) in the logical name and if available sets the
    physical name as the file name.
 */

{
    Reference< XOutputStream > rOutStream;

    // TODO/LATER: handle lifetime of storages
    pImp->bDisposeStorage = false;

    SFX_ITEMSET_ARG( pImp->m_pSet, pSalvageItem, SfxStringItem, SID_DOC_SALVAGE, false);
    if ( pSalvageItem && pSalvageItem->GetValue().isEmpty() )
    {
        pSalvageItem = NULL;
        pImp->m_pSet->ClearItem( SID_DOC_SALVAGE );
    }

    if (!pImp->m_aLogicName.isEmpty())
    {
        INetURLObject aUrl( pImp->m_aLogicName );
        INetProtocol eProt = aUrl.GetProtocol();
        if ( eProt == INET_PROT_NOT_VALID )
        {
            SAL_WARN( "sfx.doc", "Unknown protocol!" );
        }
        else
        {
            if ( aUrl.HasMark() )
            {
                pImp->m_aLogicName = aUrl.GetURLNoMark( INetURLObject::NO_DECODE );
                GetItemSet()->Put( SfxStringItem( SID_JUMPMARK, aUrl.GetMark() ) );
            }

            // try to convert the URL into a physical name - but never change a physical name
            // physical name may be set if the logical name is changed after construction
            if ( pImp->m_aName.isEmpty() )
                ::utl::LocalFileHelper::ConvertURLToPhysicalName( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ), pImp->m_aName );
            else
            {
                DBG_ASSERT( pSalvageItem, "Suspicious change of logical name!" );
            }
        }
    }

    if ( pSalvageItem && !pSalvageItem->GetValue().isEmpty() )
    {
        pImp->m_aLogicName = pSalvageItem->GetValue();
        DELETEZ( pImp->m_pURLObj );
        pImp->m_bSalvageMode = true;
    }

    // in case output stream is by mistake here
    // clear the reference
    SFX_ITEMSET_ARG( pImp->m_pSet, pOutStreamItem, SfxUnoAnyItem, SID_OUTPUTSTREAM, false);
    if( pOutStreamItem
     && ( !( pOutStreamItem->GetValue() >>= rOutStream )
          || !pImp->m_aLogicName.startsWith("private:stream")) )
    {
        pImp->m_pSet->ClearItem( SID_OUTPUTSTREAM );
        SAL_WARN( "sfx.doc", "Unexpected Output stream parameter!\n" );
    }

    if (!pImp->m_aLogicName.isEmpty())
    {
        // if the logic name is set it should be set in MediaDescriptor as well
        SFX_ITEMSET_ARG( pImp->m_pSet, pFileNameItem, SfxStringItem, SID_FILE_NAME, false );
        if ( !pFileNameItem )
        {
            // let the ItemSet be created if necessary
            GetItemSet()->Put(
                SfxStringItem(
                    SID_FILE_NAME, INetURLObject( pImp->m_aLogicName ).GetMainURL( INetURLObject::NO_DECODE ) ) );
        }
    }

    SetIsRemote_Impl();
}


SfxMedium::SfxMedium() : pImp(new SfxMedium_Impl(this))
{
    Init_Impl();
}



void SfxMedium::UseInteractionHandler( bool bUse )
{
    pImp->bAllowDefaultIntHdl = bUse;
}



::com::sun::star::uno::Reference< ::com::sun::star::task::XInteractionHandler >
SfxMedium::GetInteractionHandler()
{
    // if interaction isn't allowed explicitly ... return empty reference!
    if ( !pImp->bUseInteractionHandler )
        return ::com::sun::star::uno::Reference< ::com::sun::star::task::XInteractionHandler >();

    // search a possible existing handler inside cached item set
    if ( pImp->m_pSet )
    {
        ::com::sun::star::uno::Reference< ::com::sun::star::task::XInteractionHandler > xHandler;
        SFX_ITEMSET_ARG( pImp->m_pSet, pHandler, SfxUnoAnyItem, SID_INTERACTIONHANDLER, false);
        if ( pHandler && (pHandler->GetValue() >>= xHandler) && xHandler.is() )
            return xHandler;
    }

    // if default interaction isn't allowed explicitly ... return empty reference!
    if ( !pImp->bAllowDefaultIntHdl )
        return ::com::sun::star::uno::Reference< ::com::sun::star::task::XInteractionHandler >();

    // otherwise return cached default handler ... if it exist.
    if ( pImp->xInteraction.is() )
        return pImp->xInteraction;

    // create default handler and cache it!
    Reference< uno::XComponentContext > xContext = ::comphelper::getProcessComponentContext();
    pImp->xInteraction.set(
        task::InteractionHandler::createWithParent(xContext, 0), UNO_QUERY_THROW );
    return pImp->xInteraction;
}



void SfxMedium::SetFilter( const SfxFilter* pFilterP, bool /*bResetOrig*/ )
{
    pImp->m_pFilter = pFilterP;
}

const SfxFilter* SfxMedium::GetFilter() const
{
    return pImp->m_pFilter;
}



const SfxFilter* SfxMedium::GetOrigFilter( bool bNotCurrent ) const
{
    return ( pImp->pOrigFilter || bNotCurrent ) ? pImp->pOrigFilter : pImp->m_pFilter;
}



sal_uInt32 SfxMedium::CreatePasswordToModifyHash( const OUString& aPasswd, bool bWriter )
{
    sal_uInt32 nHash = 0;

    if ( !aPasswd.isEmpty() )
    {
        if ( bWriter )
        {
            nHash = ::comphelper::DocPasswordHelper::GetWordHashAsUINT32( aPasswd );
        }
        else
        {
            rtl_TextEncoding nEncoding = osl_getThreadTextEncoding();
            nHash = ::comphelper::DocPasswordHelper::GetXLHashAsUINT16( aPasswd, nEncoding );
        }
    }

    return nHash;
}



void SfxMedium::Close()
{
    if ( pImp->xStorage.is() )
    {
        CloseStorage();
    }

    CloseStreams_Impl();

    UnlockFile( false );
}

void SfxMedium::CloseAndRelease()
{
    if ( pImp->xStorage.is() )
    {
        CloseStorage();
    }

    CloseAndReleaseStreams_Impl();

    UnlockFile( true );
}

void SfxMedium::UnlockFile( bool bReleaseLockStream )
{
#if !HAVE_FEATURE_MULTIUSER_ENVIRONMENT
    (void) bReleaseLockStream;
#else
    if ( pImp->m_xLockingStream.is() )
    {
        if ( bReleaseLockStream )
        {
            try
            {
                uno::Reference< io::XInputStream > xInStream = pImp->m_xLockingStream->getInputStream();
                uno::Reference< io::XOutputStream > xOutStream = pImp->m_xLockingStream->getOutputStream();
                if ( xInStream.is() )
                    xInStream->closeInput();
                if ( xOutStream.is() )
                    xOutStream->closeOutput();
            }
            catch( const uno::Exception& )
            {}
        }

        pImp->m_xLockingStream.clear();
    }

    if ( pImp->m_bLocked )
    {
        try
        {
            pImp->m_bLocked = false;
            ::svt::DocumentLockFile aLockFile( pImp->m_aLogicName );
            // TODO/LATER: A warning could be shown in case the file is not the own one
            aLockFile.RemoveFile();
        }
        catch( const uno::Exception& )
        {}
    }
#endif
}

void SfxMedium::CloseAndReleaseStreams_Impl()
{
    CloseZipStorage_Impl();

    uno::Reference< io::XInputStream > xInToClose = pImp->xInputStream;
    uno::Reference< io::XOutputStream > xOutToClose;
    if ( pImp->xStream.is() )
    {
        xOutToClose = pImp->xStream->getOutputStream();

        // if the locking stream is closed here the related member should be cleaned
        if ( pImp->xStream == pImp->m_xLockingStream )
            pImp->m_xLockingStream.clear();
    }

    // The probably exsisting SvStream wrappers should be closed first
    CloseStreams_Impl();

    // in case of salvage mode the storage is based on the streams
    if ( !pImp->m_bSalvageMode )
    {
        try
        {
            if ( xInToClose.is() )
                xInToClose->closeInput();
            if ( xOutToClose.is() )
                xOutToClose->closeOutput();
        }
        catch ( const uno::Exception& )
        {
        }
    }
}


void SfxMedium::CloseStreams_Impl()
{
    CloseInStream_Impl();
    CloseOutStream_Impl();

    if ( pImp->m_pSet )
        pImp->m_pSet->ClearItem( SID_CONTENT );

    pImp->aContent = ::ucbhelper::Content();
}



void SfxMedium::SetIsRemote_Impl()
{
    INetURLObject aObj( GetName() );
    switch( aObj.GetProtocol() )
    {
        case INET_PROT_FTP:
        case INET_PROT_HTTP:
        case INET_PROT_HTTPS:
        case INET_PROT_POP3:
        case INET_PROT_NEWS:
        case INET_PROT_IMAP:
        case INET_PROT_VIM:
            pImp->m_bRemote = true;
        break;
        default:
            pImp->m_bRemote = GetName().startsWith("private:msgid");
            break;
    }

    // As files that are written to the remote transmission must also be able
    // to be read.
    if (pImp->m_bRemote)
        pImp->m_nStorOpenMode |= STREAM_READ;
}



void SfxMedium::SetName( const OUString& aNameP, bool bSetOrigURL )
{
    if (pImp->aOrigURL.isEmpty())
        pImp->aOrigURL = pImp->m_aLogicName;
    if( bSetOrigURL )
        pImp->aOrigURL = aNameP;
    pImp->m_aLogicName = aNameP;
    DELETEZ( pImp->m_pURLObj );
    pImp->aContent = ::ucbhelper::Content();
    Init_Impl();
}


const OUString& SfxMedium::GetOrigURL() const
{
    return pImp->aOrigURL.isEmpty() ? pImp->m_aLogicName : pImp->aOrigURL;
}



void SfxMedium::SetPhysicalName_Impl( const OUString& rNameP )
{
    if ( rNameP != pImp->m_aName )
    {
        if( pImp->pTempFile )
        {
            delete pImp->pTempFile;
            pImp->pTempFile = NULL;
        }

        if ( !pImp->m_aName.isEmpty() || !rNameP.isEmpty() )
            pImp->aContent = ::ucbhelper::Content();

        pImp->m_aName = rNameP;
        pImp->m_bTriedStorage = false;
        pImp->bIsStorage = false;
    }
}



void SfxMedium::ReOpen()
{
    bool bUseInteractionHandler = pImp->bUseInteractionHandler;
    pImp->bUseInteractionHandler = false;
    GetMedium_Impl();
    pImp->bUseInteractionHandler = bUseInteractionHandler;
}



void SfxMedium::CompleteReOpen()
{
    // do not use temporary file for reopen and in case of success throw the temporary file away
    bool bUseInteractionHandler = pImp->bUseInteractionHandler;
    pImp->bUseInteractionHandler = false;

    ::utl::TempFile* pTmpFile = NULL;
    if ( pImp->pTempFile )
    {
        pTmpFile = pImp->pTempFile;
        pImp->pTempFile = NULL;
        pImp->m_aName = "";
    }

    GetMedium_Impl();

    if ( GetError() )
    {
        if ( pImp->pTempFile )
        {
            pImp->pTempFile->EnableKillingFile( true );
            delete pImp->pTempFile;
        }
        pImp->pTempFile = pTmpFile;
        if ( pImp->pTempFile )
            pImp->m_aName = pImp->pTempFile->GetFileName();
    }
    else if (pTmpFile)
    {
        pTmpFile->EnableKillingFile( true );
        delete pTmpFile;

    }

    pImp->bUseInteractionHandler = bUseInteractionHandler;
}

SfxMedium::SfxMedium(const OUString &rName, StreamMode nOpenMode, const SfxFilter *pFlt, SfxItemSet *pInSet) :
    pImp(new SfxMedium_Impl(this))
{
    pImp->m_pSet = pInSet;
    pImp->m_pFilter = pFlt;
    pImp->m_aLogicName = rName;
    pImp->m_nStorOpenMode = nOpenMode;
    Init_Impl();
}

SfxMedium::SfxMedium(const OUString &rName, const OUString &rReferer, StreamMode nOpenMode, const SfxFilter *pFlt, SfxItemSet *pInSet) :
    pImp(new SfxMedium_Impl(this))
{
    pImp->m_pSet = pInSet;
    SfxItemSet * s = GetItemSet();
    if (s->GetItem(SID_REFERER) == 0) {
        s->Put(SfxStringItem(SID_REFERER, rReferer));
    }
    pImp->m_pFilter = pFlt;
    pImp->m_aLogicName = rName;
    pImp->m_nStorOpenMode = nOpenMode;
    Init_Impl();
}

SfxMedium::SfxMedium( const uno::Sequence<beans::PropertyValue>& aArgs ) :
    pImp(new SfxMedium_Impl(this))
{
    SfxAllItemSet *pParams = new SfxAllItemSet( SfxGetpApp()->GetPool() );
    pImp->m_pSet = pParams;
    TransformParameters( SID_OPENDOC, aArgs, *pParams );

    OUString aFilterProvider, aFilterName;
    {
        const SfxPoolItem* pItem = NULL;
        if (pImp->m_pSet->HasItem(SID_FILTER_PROVIDER, &pItem))
            aFilterProvider = static_cast<const SfxStringItem*>(pItem)->GetValue();

        if (pImp->m_pSet->HasItem(SID_FILTER_NAME, &pItem))
            aFilterName = static_cast<const SfxStringItem*>(pItem)->GetValue();
    }

    if (aFilterProvider.isEmpty())
    {
        // This is a conventional filter type.
        pImp->m_pFilter = SfxGetpApp()->GetFilterMatcher().GetFilter4FilterName( aFilterName );
    }
    else
    {
        // This filter is from an external provider such as orcus.
        pImp->m_pCustomFilter.reset(new SfxFilter(aFilterProvider, aFilterName));
        pImp->m_pFilter = pImp->m_pCustomFilter.get();
    }

    SFX_ITEMSET_ARG( pImp->m_pSet, pSalvageItem, SfxStringItem, SID_DOC_SALVAGE, false );
    if( pSalvageItem )
    {
        // QUESTION: there is some treatment of Salvage in Init_Impl; align!
        if ( !pSalvageItem->GetValue().isEmpty() )
        {
            // if an URL is provided in SalvageItem that means that the FileName refers to a temporary file
            // that must be copied here

            SFX_ITEMSET_ARG( pImp->m_pSet, pFileNameItem, SfxStringItem, SID_FILE_NAME, false );
            if (!pFileNameItem) throw uno::RuntimeException();
            OUString aNewTempFileURL = SfxMedium::CreateTempCopyWithExt( pFileNameItem->GetValue() );
            if ( !aNewTempFileURL.isEmpty() )
            {
                pImp->m_pSet->Put( SfxStringItem( SID_FILE_NAME, aNewTempFileURL ) );
                pImp->m_pSet->ClearItem( SID_INPUTSTREAM );
                pImp->m_pSet->ClearItem( SID_STREAM );
                pImp->m_pSet->ClearItem( SID_CONTENT );
            }
            else
            {
                SAL_WARN( "sfx.doc", "Can not create a new temporary file for crash recovery!\n" );
            }
        }
    }

    SFX_ITEMSET_ARG( pImp->m_pSet, pReadOnlyItem, SfxBoolItem, SID_DOC_READONLY, false );
    if ( pReadOnlyItem && pReadOnlyItem->GetValue() )
        pImp->m_bOriginallyReadOnly = true;

    SFX_ITEMSET_ARG( pImp->m_pSet, pFileNameItem, SfxStringItem, SID_FILE_NAME, false );
    if (!pFileNameItem) throw uno::RuntimeException();
    pImp->m_aLogicName = pFileNameItem->GetValue();
    pImp->m_nStorOpenMode = pImp->m_bOriginallyReadOnly ? SFX_STREAM_READONLY : SFX_STREAM_READWRITE;
    Init_Impl();
}




SfxMedium::SfxMedium( const uno::Reference < embed::XStorage >& rStor, const OUString& rBaseURL, const SfxItemSet* p ) :
    pImp(new SfxMedium_Impl(this))
{
    OUString aType = SfxFilter::GetTypeFromStorage(rStor);
    pImp->m_pFilter = SfxGetpApp()->GetFilterMatcher().GetFilter4EA( aType );
    DBG_ASSERT( pImp->m_pFilter, "No Filter for storage found!" );

    Init_Impl();
    pImp->xStorage = rStor;
    pImp->bDisposeStorage = false;

    // always take BaseURL first, could be overwritten by ItemSet
    GetItemSet()->Put( SfxStringItem( SID_DOC_BASEURL, rBaseURL ) );
    if ( p )
        GetItemSet()->Put( *p );
}



SfxMedium::SfxMedium( const uno::Reference < embed::XStorage >& rStor, const OUString& rBaseURL, const OUString &rTypeName, const SfxItemSet* p ) :
    pImp(new SfxMedium_Impl(this))
{
    pImp->m_pFilter = SfxGetpApp()->GetFilterMatcher().GetFilter4EA( rTypeName );
    DBG_ASSERT( pImp->m_pFilter, "No Filter for storage found!" );

    Init_Impl();
    pImp->xStorage = rStor;
    pImp->bDisposeStorage = false;

    // always take BaseURL first, could be overwritten by ItemSet
    GetItemSet()->Put( SfxStringItem( SID_DOC_BASEURL, rBaseURL ) );
    if ( p )
        GetItemSet()->Put( *p );
}



SfxMedium::~SfxMedium()
{
    // if there is a requirement to clean the backup this is the last possibility to do it
    ClearBackup_Impl();

    Close();

    if( pImp->bIsTemp && !pImp->m_aName.isEmpty() )
    {
        OUString aTemp;
        if ( !::utl::LocalFileHelper::ConvertPhysicalNameToURL( pImp->m_aName, aTemp ))
        {
            SAL_WARN( "sfx.doc", "Physical name not convertible!");
        }

        if ( !::utl::UCBContentHelper::Kill( aTemp ) )
        {
            SAL_WARN( "sfx.doc", "Couldn't remove temporary file!");
        }
    }

    delete pImp;
}

const OUString& SfxMedium::GetName() const
{
    return pImp->m_aLogicName;
}

const INetURLObject& SfxMedium::GetURLObject() const
{
    if (!pImp->m_pURLObj)
    {
        pImp->m_pURLObj = new INetURLObject( pImp->m_aLogicName );
        pImp->m_pURLObj->SetMark("");
    }

    return *pImp->m_pURLObj;
}

void SfxMedium::SetExpired_Impl( const DateTime& rDateTime )
{
    pImp->aExpireTime = rDateTime;
}


bool SfxMedium::IsExpired() const
{
    return pImp->aExpireTime.IsValidAndGregorian() && pImp->aExpireTime < DateTime( DateTime::SYSTEM );
}


void SfxMedium::ForceSynchronStream_Impl( bool bForce )
{
    if( pImp->m_pInStream )
    {
        SvLockBytes* pBytes = pImp->m_pInStream->GetLockBytes();
        if( pBytes )
            pBytes->SetSynchronMode( bForce );
    }
}


SfxFrame* SfxMedium::GetLoadTargetFrame() const
{
    return pImp->wLoadTargetFrame;
}

void SfxMedium::setStreamToLoadFrom(const com::sun::star::uno::Reference<com::sun::star::io::XInputStream>& xInputStream, bool bIsReadOnly )
{
    pImp->m_xInputStreamToLoadFrom = xInputStream;
    pImp->m_bInputStreamIsReadOnly = bIsReadOnly;
}

void SfxMedium::SetLoadTargetFrame(SfxFrame* pFrame )
{
    pImp->wLoadTargetFrame = pFrame;
}


void SfxMedium::SetStorage_Impl( const uno::Reference < embed::XStorage >& rStor )
{
    pImp->xStorage = rStor;
}


SfxItemSet* SfxMedium::GetItemSet() const
{
    // this method *must* return an ItemSet, returning NULL can cause crashes
    if (!pImp->m_pSet)
        pImp->m_pSet = new SfxAllItemSet( SfxGetpApp()->GetPool() );
    return pImp->m_pSet;
}


SvKeyValueIterator* SfxMedium::GetHeaderAttributes_Impl()
{
    if( !pImp->xAttributes.Is() )
    {
        pImp->xAttributes = SvKeyValueIteratorRef( new SvKeyValueIterator );

        if ( GetContent().is() )
        {
            try
            {
                Any aAny = pImp->aContent.getPropertyValue("MediaType");
                OUString aContentType;
                aAny >>= aContentType;

                pImp->xAttributes->Append( SvKeyValue( OUString("content-type"), aContentType ) );
            }
            catch ( const ::com::sun::star::uno::Exception& )
            {
            }
        }
    }

    return pImp->xAttributes;
}

::com::sun::star::uno::Reference< ::com::sun::star::io::XInputStream >  SfxMedium::GetInputStream()
{
    if ( !pImp->xInputStream.is() )
        GetMedium_Impl();
    return pImp->xInputStream;
}

const uno::Sequence < util::RevisionTag >& SfxMedium::GetVersionList( bool _bNoReload )
{
    // if the medium has no name, then this medium should represent a new document and can have no version info
    if ( ( !_bNoReload || !pImp->m_bVersionsAlreadyLoaded ) && !pImp->aVersions.getLength() &&
         ( !pImp->m_aName.isEmpty() || !pImp->m_aLogicName.isEmpty() ) && GetStorage().is() )
    {
        uno::Reference < document::XDocumentRevisionListPersistence > xReader =
                document::DocumentRevisionListPersistence::create( comphelper::getProcessComponentContext() );
        try
        {
            pImp->aVersions = xReader->load( GetStorage() );
        }
        catch ( const uno::Exception& )
        {
        }
    }

    if ( !pImp->m_bVersionsAlreadyLoaded )
        pImp->m_bVersionsAlreadyLoaded = true;

    return pImp->aVersions;
}

uno::Sequence < util::RevisionTag > SfxMedium::GetVersionList( const uno::Reference < embed::XStorage >& xStorage )
{
    uno::Reference < document::XDocumentRevisionListPersistence > xReader =
        document::DocumentRevisionListPersistence::create( comphelper::getProcessComponentContext() );
    try
    {
        return xReader->load( xStorage );
    }
    catch ( const uno::Exception& )
    {
    }

    return uno::Sequence < util::RevisionTag >();
}

sal_uInt16 SfxMedium::AddVersion_Impl( util::RevisionTag& rRevision )
{
    if ( GetStorage().is() )
    {
        // To determine a unique name for the stream
        std::vector<sal_uInt32> aLongs;
        sal_Int32 nLength = pImp->aVersions.getLength();
        for ( sal_Int32 m=0; m<nLength; m++ )
        {
            sal_uInt32 nVer = static_cast<sal_uInt32>( pImp->aVersions[m].Identifier.copy(7).toInt32());
            size_t n;
            for ( n=0; n<aLongs.size(); ++n )
                if ( nVer<aLongs[n] )
                    break;

            aLongs.insert( aLongs.begin()+n, nVer );
        }

        sal_uInt16 nKey;
        for ( nKey=0; nKey<aLongs.size(); ++nKey )
            if ( aLongs[nKey] > ( sal_uIntPtr ) nKey+1 )
                break;

        OUString aRevName = "Version" + OUString::number( nKey + 1 );
        pImp->aVersions.realloc( nLength+1 );
        rRevision.Identifier = aRevName;
        pImp->aVersions[nLength] = rRevision;
        return nKey;
    }

    return 0;
}

bool SfxMedium::RemoveVersion_Impl( const OUString& rName )
{
    if ( !pImp->aVersions.getLength() )
        return false;

    sal_Int32 nLength = pImp->aVersions.getLength();
    for ( sal_Int32 n=0; n<nLength; n++ )
    {
        if ( pImp->aVersions[n].Identifier == rName )
        {
            for ( sal_Int32 m=n; m<nLength-1; m++ )
                pImp->aVersions[m] = pImp->aVersions[m+1];
            pImp->aVersions.realloc(nLength-1);
            return true;
        }
    }

    return false;
}

bool SfxMedium::TransferVersionList_Impl( SfxMedium& rMedium )
{
    if ( rMedium.pImp->aVersions.getLength() )
    {
        pImp->aVersions = rMedium.pImp->aVersions;
        return true;
    }

    return false;
}

bool SfxMedium::SaveVersionList_Impl( bool /*bUseXML*/ )
{
    if ( GetStorage().is() )
    {
        if ( !pImp->aVersions.getLength() )
            return true;

        uno::Reference < document::XDocumentRevisionListPersistence > xWriter =
                 document::DocumentRevisionListPersistence::create( comphelper::getProcessComponentContext() );
        try
        {
            xWriter->store( GetStorage(), pImp->aVersions );
            return true;
        }
        catch ( const uno::Exception& )
        {
        }
    }

    return false;
}


bool SfxMedium::IsReadOnly()
{
    // a) ReadOnly filter can't produce read/write contents!
    bool bReadOnly = (
                    (pImp->m_pFilter                                                                         ) &&
                    ((pImp->m_pFilter->GetFilterFlags() & SFX_FILTER_OPENREADONLY) == SFX_FILTER_OPENREADONLY)
                );

    // b) if filter allow read/write contents .. check open mode of the storage
    if (!bReadOnly)
        bReadOnly = !( GetOpenMode() & STREAM_WRITE );

    // c) the API can force the readonly state!
    if (!bReadOnly)
    {
        SFX_ITEMSET_ARG( GetItemSet(), pItem, SfxBoolItem, SID_DOC_READONLY, false);
        if (pItem)
            bReadOnly = pItem->GetValue();
    }

    return bReadOnly;
}

bool SfxMedium::IsOriginallyReadOnly() const
{
    return pImp->m_bOriginallyReadOnly;
}


bool SfxMedium::SetWritableForUserOnly( const OUString& aURL )
{
    // UCB does not allow to allow write access only for the user,
    // use osl API
    bool bResult = false;

    ::osl::DirectoryItem aDirItem;
    if ( ::osl::DirectoryItem::get( aURL, aDirItem ) == ::osl::FileBase::E_None )
    {
        ::osl::FileStatus aFileStatus( osl_FileStatus_Mask_Attributes );
        if ( aDirItem.getFileStatus( aFileStatus ) == osl::FileBase::E_None
          && aFileStatus.isValid( osl_FileStatus_Mask_Attributes ) )
        {
            sal_uInt64 nAttributes = aFileStatus.getAttributes();

            nAttributes &= ~(osl_File_Attribute_OwnWrite |
                             osl_File_Attribute_GrpWrite |
                             osl_File_Attribute_OthWrite |
                             osl_File_Attribute_ReadOnly);
            nAttributes |=  (osl_File_Attribute_OwnWrite |
                             osl_File_Attribute_OwnRead);

            bResult = ( osl::File::setAttributes( aURL, nAttributes ) == ::osl::FileBase::E_None );
        }
    }

    return bResult;
}


void SfxMedium::CreateTempFile( bool bReplace )
{
    if ( pImp->pTempFile )
    {
        if ( !bReplace )
            return;

        DELETEZ( pImp->pTempFile );
        pImp->m_aName = "";
    }

    pImp->pTempFile = new ::utl::TempFile();
    pImp->pTempFile->EnableKillingFile( true );
    pImp->m_aName = pImp->pTempFile->GetFileName();
    OUString aTmpURL = pImp->pTempFile->GetURL();
    if ( pImp->m_aName.isEmpty() || aTmpURL.isEmpty() )
    {
        SetError( ERRCODE_IO_CANTWRITE, OUString( OSL_LOG_PREFIX  ) );
        return;
    }

    if ( !(pImp->m_nStorOpenMode & STREAM_TRUNC) )
    {
        bool bTransferSuccess = false;

        if ( GetContent().is()
          && ::utl::LocalFileHelper::IsLocalFile( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) )
          && ::utl::UCBContentHelper::IsDocument( GetURLObject().GetMainURL( INetURLObject::NO_DECODE ) ) )
        {
            // if there is already such a document, we should copy it
            // if it is a file system use OS copy process
            try
            {
                uno::Reference< ::com::sun::star::ucb::XCommandEnvironment > xComEnv;
                INetURLObject aTmpURLObj( aTmpURL );
                OUString aFileName = aTmpURLObj.getName( INetURLObject::LAST_SEGMENT,
                                                                true,
                                                                INetURLObject::DECODE_WITH_CHARSET );
                if ( !aFileName.isEmpty() && aTmpURLObj.removeSegment() )
                {
                    ::ucbhelper::Content aTargetContent( aTmpURLObj.GetMainURL( INetURLObject::NO_DECODE ), xComEnv, comphelper::getProcessComponentContext() );
                    OUString sMimeType = pImp->getFilterMimeType();
                    if ( aTargetContent.transferContent( pImp->aContent, ::ucbhelper::InsertOperation_COPY, aFileName, NameClash::OVERWRITE, sMimeType ) )
                    {
                        SetWritableForUserOnly( aTmpURL );
                        bTransferSuccess = true;
                    }
                }
            }
            catch( const uno::Exception& )
            {}

            if ( bTransferSuccess )
            {
                CloseOutStream();
                CloseInStream();
            }
        }

        if ( !bTransferSuccess && pImp->m_pInStream )
        {
            // the case when there is no URL-access available or this is a remote protocoll
            // but there is an input stream
            GetOutStream();
            if ( pImp->m_pOutStream )
            {
                char        *pBuf = new char [8192];
                sal_uInt32   nErr = ERRCODE_NONE;

                pImp->m_pInStream->Seek(0);
                pImp->m_pOutStream->Seek(0);

                while( !pImp->m_pInStream->IsEof() && nErr == ERRCODE_NONE )
                {
                    sal_uInt32 nRead = pImp->m_pInStream->Read( pBuf, 8192 );
                    nErr = pImp->m_pInStream->GetError();
                    pImp->m_pOutStream->Write( pBuf, nRead );
                }

                bTransferSuccess = true;
                delete[] pBuf;
                CloseInStream();
            }
            CloseOutStream_Impl();
        }
        else
        {
            // Quite strange design, but currently it is expected that in this case no transfer happens
            // TODO/LATER: get rid of this inconsistent part of the call design
            bTransferSuccess = true;
            CloseInStream();
        }

        if ( !bTransferSuccess )
        {
            SetError( ERRCODE_IO_CANTWRITE, OUString( OSL_LOG_PREFIX  ) );
            return;
        }
    }

    CloseStorage();
}


void SfxMedium::CreateTempFileNoCopy()
{
    // this call always replaces the existing temporary file
    if ( pImp->pTempFile )
        delete pImp->pTempFile;

    pImp->pTempFile = new ::utl::TempFile();
    pImp->pTempFile->EnableKillingFile( true );
    pImp->m_aName = pImp->pTempFile->GetFileName();
    if ( pImp->m_aName.isEmpty() )
    {
        SetError( ERRCODE_IO_CANTWRITE, OUString( OSL_LOG_PREFIX  ) );
        return;
    }

    CloseOutStream_Impl();
    CloseStorage();
}

bool SfxMedium::SignContents_Impl( bool bScriptingContent, const OUString& aODFVersion, bool bHasValidDocumentSignature )
{
    bool bChanges = false;

    // the medium should be closed to be able to sign, the caller is responsible to close it
    if ( !IsOpen() && !GetError() )
    {
        // The component should know if there was a valid document signature, since
        // it should show a warning in this case
        uno::Reference< security::XDocumentDigitalSignatures > xSigner(
            security::DocumentDigitalSignatures::createWithVersionAndValidSignature(
                comphelper::getProcessComponentContext(), aODFVersion, bHasValidDocumentSignature ) );

        uno::Reference< embed::XStorage > xWriteableZipStor;
        if ( !IsReadOnly() )
        {
            // we can reuse the temporary file if there is one already
            CreateTempFile( false );
            GetMedium_Impl();

            try
            {
                if ( !pImp->xStream.is() )
                    throw uno::RuntimeException();

                xWriteableZipStor = ::comphelper::OStorageHelper::GetStorageOfFormatFromStream( ZIP_STORAGE_FORMAT_STRING, pImp->xStream );
                if ( !xWriteableZipStor.is() )
                    throw uno::RuntimeException();

                uno::Reference< embed::XStorage > xMetaInf = xWriteableZipStor->openStorageElement(
                                                OUString( "META-INF"  ),
                                                embed::ElementModes::READWRITE );
                if ( !xMetaInf.is() )
                    throw uno::RuntimeException();

                if ( bScriptingContent )
                {
                    // If the signature has already the document signature it will be removed
                    // after the scripting signature is inserted.
                    uno::Reference< io::XStream > xStream(
                        xMetaInf->openStreamElement( xSigner->getScriptingContentSignatureDefaultStreamName(),
                                                     embed::ElementModes::READWRITE ),
                        uno::UNO_SET_THROW );

                    if ( xSigner->signScriptingContent( GetZipStorageToSign_Impl(), xStream ) )
                    {
                        // remove the document signature if any
                        OUString aDocSigName = xSigner->getDocumentContentSignatureDefaultStreamName();
                        if ( !aDocSigName.isEmpty() && xMetaInf->hasByName( aDocSigName ) )
                            xMetaInf->removeElement( aDocSigName );

                        uno::Reference< embed::XTransactedObject > xTransact( xMetaInf, uno::UNO_QUERY_THROW );
                        xTransact->commit();
                        xTransact.set( xWriteableZipStor, uno::UNO_QUERY_THROW );
                        xTransact->commit();

                        // the temporary file has been written, commit it to the original file
                        Commit();
                        bChanges = true;
                    }
                }
                else
                {
                     uno::Reference< io::XStream > xStream(
                        xMetaInf->openStreamElement( xSigner->getDocumentContentSignatureDefaultStreamName(),
                                                     embed::ElementModes::READWRITE ),
                        uno::UNO_SET_THROW );

                    if ( xSigner->signDocumentContent( GetZipStorageToSign_Impl(), xStream ) )
                    {
                        uno::Reference< embed::XTransactedObject > xTransact( xMetaInf, uno::UNO_QUERY_THROW );
                        xTransact->commit();
                        xTransact.set( xWriteableZipStor, uno::UNO_QUERY_THROW );
                        xTransact->commit();

                        // the temporary file has been written, commit it to the original file
                        Commit();
                        bChanges = true;
                    }
                }
            }
            catch ( const uno::Exception& )
            {
                SAL_WARN( "sfx.doc", "Couldn't use signing functionality!\n" );
            }

            CloseAndRelease();
        }
        else
        {
            try
            {
                if ( bScriptingContent )
                    xSigner->showScriptingContentSignatures( GetZipStorageToSign_Impl(), uno::Reference< io::XInputStream >() );
                else
                    xSigner->showDocumentContentSignatures( GetZipStorageToSign_Impl(), uno::Reference< io::XInputStream >() );
            }
            catch( const uno::Exception& )
            {
                SAL_WARN( "sfx.doc", "Couldn't use signing functionality!\n" );
            }
        }

        ResetError();
    }

    return bChanges;
}


sal_uInt16 SfxMedium::GetCachedSignatureState_Impl()
{
    return pImp->m_nSignatureState;
}


void SfxMedium::SetCachedSignatureState_Impl( sal_uInt16 nState )
{
    pImp->m_nSignatureState = nState;
}

bool SfxMedium::HasStorage_Impl() const
{
    return pImp->xStorage.is();
}

bool SfxMedium::IsOpen() const
{
    return pImp->m_pInStream || pImp->m_pOutStream || pImp->xStorage.is();
}

OUString SfxMedium::CreateTempCopyWithExt( const OUString& aURL )
{
    OUString aResult;

    if ( !aURL.isEmpty() )
    {
        sal_Int32 nPrefixLen = aURL.lastIndexOf( '.' );
        OUString aExt = ( nPrefixLen == -1 ) ? OUString() :  aURL.copy( nPrefixLen );

        OUString aNewTempFileURL = ::utl::TempFile( OUString(), true, &aExt ).GetURL();
        if ( !aNewTempFileURL.isEmpty() )
        {
            INetURLObject aSource( aURL );
            INetURLObject aDest( aNewTempFileURL );
            OUString aFileName = aDest.getName( INetURLObject::LAST_SEGMENT,
                                                        true,
                                                        INetURLObject::DECODE_WITH_CHARSET );
            if ( !aFileName.isEmpty() && aDest.removeSegment() )
            {
                try
                {
                    uno::Reference< ::com::sun::star::ucb::XCommandEnvironment > xComEnv;
                    ::ucbhelper::Content aTargetContent( aDest.GetMainURL( INetURLObject::NO_DECODE ), xComEnv, comphelper::getProcessComponentContext() );
                    ::ucbhelper::Content aSourceContent( aSource.GetMainURL( INetURLObject::NO_DECODE ), xComEnv, comphelper::getProcessComponentContext() );
                    if ( aTargetContent.transferContent( aSourceContent,
                                                        ::ucbhelper::InsertOperation_COPY,
                                                        aFileName,
                                                        NameClash::OVERWRITE ) )
                    {
                        // Success
                        aResult = aNewTempFileURL;
                    }
                }
                catch( const uno::Exception& )
                {}
            }
        }
    }

    return aResult;
}

bool SfxMedium::CallApproveHandler( const uno::Reference< task::XInteractionHandler >& xHandler, uno::Any aRequest, bool bAllowAbort )
{
    bool bResult = false;

    if ( xHandler.is() )
    {
        try
        {
            uno::Sequence< uno::Reference< task::XInteractionContinuation > > aContinuations( bAllowAbort ? 2 : 1 );

            ::rtl::Reference< ::comphelper::OInteractionApprove > pApprove( new ::comphelper::OInteractionApprove );
            aContinuations[ 0 ] = pApprove.get();

            if ( bAllowAbort )
            {
                ::rtl::Reference< ::comphelper::OInteractionAbort > pAbort( new ::comphelper::OInteractionAbort );
                aContinuations[ 1 ] = pAbort.get();
            }

            xHandler->handle(::framework::InteractionRequest::CreateRequest (aRequest,aContinuations));
            bResult = pApprove->wasSelected();
        }
        catch( const Exception& )
        {
        }
    }

    return bResult;
}

OUString SfxMedium::SwitchDocumentToTempFile()
{
    // the method returns empty string in case of failure
    OUString aResult;
    OUString aOrigURL = pImp->m_aLogicName;

    if ( !aOrigURL.isEmpty() )
    {
        sal_Int32 nPrefixLen = aOrigURL.lastIndexOf( '.' );
        OUString const aExt = (nPrefixLen == -1)
                                ? OUString()
                                : aOrigURL.copy(nPrefixLen);
        OUString aNewURL = ::utl::TempFile( OUString(), true, &aExt ).GetURL();

        // TODO/LATER: In future the aLogicName should be set to shared folder URL
        //             and a temporary file should be created. Transport_Impl should be impossible then.
        if ( !aNewURL.isEmpty() )
        {
            uno::Reference< embed::XStorage > xStorage = GetStorage();
            uno::Reference< embed::XOptimizedStorage > xOptStorage( xStorage, uno::UNO_QUERY );

            if ( xOptStorage.is() )
            {
                // TODO/LATER: reuse the pImp->pTempFile if it already exists
                CanDisposeStorage_Impl( false );
                Close();
                SetPhysicalName_Impl( OUString() );
                SetName( aNewURL );

                // remove the readonly state
                bool bWasReadonly = false;
                pImp->m_nStorOpenMode = SFX_STREAM_READWRITE;
                SFX_ITEMSET_ARG( pImp->m_pSet, pReadOnlyItem, SfxBoolItem, SID_DOC_READONLY, false );
                if ( pReadOnlyItem && pReadOnlyItem->GetValue() )
                    bWasReadonly = true;
                GetItemSet()->ClearItem( SID_DOC_READONLY );

                GetMedium_Impl();
                LockOrigFileOnDemand( false, false );
                CreateTempFile( true );
                GetMedium_Impl();

                if ( pImp->xStream.is() )
                {
                    try
                    {
                        xOptStorage->writeAndAttachToStream( pImp->xStream );
                        pImp->xStorage = xStorage;
                        aResult = aNewURL;
                    }
                    catch( const uno::Exception& )
                    {}
                }

                if ( aResult.isEmpty() )
                {
                    Close();
                    SetPhysicalName_Impl( OUString() );
                    SetName( aOrigURL );
                    if ( bWasReadonly )
                    {
                        // set the readonly state back
                        pImp->m_nStorOpenMode = SFX_STREAM_READONLY;
                        GetItemSet()->Put( SfxBoolItem(SID_DOC_READONLY, true));
                    }
                    GetMedium_Impl();
                    pImp->xStorage = xStorage;
                }
            }
        }
    }

    return aResult;
}

bool SfxMedium::SwitchDocumentToFile( const OUString& aURL )
{
    // the method is only for storage based documents
    bool bResult = false;
    OUString aOrigURL = pImp->m_aLogicName;

    if ( !aURL.isEmpty() && !aOrigURL.isEmpty() )
    {
        uno::Reference< embed::XStorage > xStorage = GetStorage();
        uno::Reference< embed::XOptimizedStorage > xOptStorage( xStorage, uno::UNO_QUERY );

        if ( xOptStorage.is() )
        {
            // TODO/LATER: reuse the pImp->pTempFile if it already exists
            CanDisposeStorage_Impl( false );
            Close();
            SetPhysicalName_Impl( OUString() );
            SetName( aURL );

            // open the temporary file based document
            GetMedium_Impl();
            LockOrigFileOnDemand( false, false );
            CreateTempFile( true );
            GetMedium_Impl();

            if ( pImp->xStream.is() )
            {
                try
                {
                    uno::Reference< io::XTruncate > xTruncate( pImp->xStream, uno::UNO_QUERY_THROW );
                    if ( xTruncate.is() )
                        xTruncate->truncate();

                    xOptStorage->writeAndAttachToStream( pImp->xStream );
                    pImp->xStorage = xStorage;
                    bResult = true;
                }
                catch( const uno::Exception& )
                {}
            }

            if ( !bResult )
            {
                Close();
                SetPhysicalName_Impl( OUString() );
                SetName( aOrigURL );
                GetMedium_Impl();
                pImp->xStorage = xStorage;
            }
        }
    }

    return bResult;
}

void SfxMedium::SetInCheckIn( bool bInCheckIn )
{
    pImp->m_bInCheckIn = bInCheckIn;
}

bool SfxMedium::IsInCheckIn( )
{
    return pImp->m_bInCheckIn;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
