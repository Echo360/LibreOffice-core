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

#include <com/sun/star/presentation/XPresentation2.hpp>

#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/lang/ServiceNotRegisteredException.hpp>
#include <com/sun/star/lang/Locale.hpp>
#include <com/sun/star/style/XStyle.hpp>
#include <com/sun/star/awt/XDevice.hpp>
#include <com/sun/star/document/IndexedPropertyValues.hpp>

#include <com/sun/star/embed/Aspects.hpp>

#include <osl/mutex.hxx>
#include <comphelper/sequence.hxx>
#include <comphelper/servicehelper.hxx>
#include <cppuhelper/supportsservice.hxx>

#include <editeng/unofield.hxx>
#include <notifydocumentevent.hxx>
#include <unomodel.hxx>
#include <unopool.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/bindings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/settings.hxx>

#include <editeng/UnoForbiddenCharsTable.hxx>
#include <svx/svdoutl.hxx>
#include <editeng/forbiddencharacterstable.hxx>
#include <svx/UnoNamespaceMap.hxx>
#include <svx/svdlayer.hxx>
#include <svx/svdsob.hxx>
#include <svx/unoapi.hxx>
#include <svx/unofill.hxx>
#include <svx/unopool.hxx>
#include <svx/svdorect.hxx>
#include <editeng/flditem.hxx>
#include <editeng/fontitem.hxx>
#include <toolkit/awt/vclxdevice.hxx>
#include <svx/svdpool.hxx>
#include <editeng/unolingu.hxx>
#include <svx/svdpagv.hxx>
#include <svtools/unoimap.hxx>
#include <svx/unoshape.hxx>
#include <editeng/unonrule.hxx>
#include <editeng/eeitem.hxx>

// Support creation of GraphicObjectResolver and EmbeddedObjectResolver
#include <svx/xmleohlp.hxx>
#include <svx/xmlgrhlp.hxx>
#include "DrawDocShell.hxx"
#include "ViewShellBase.hxx"
#include <UnoDocumentSettings.hxx>

#include <drawdoc.hxx>
#include <glob.hrc>
#include <sdresid.hxx>
#include <sdpage.hxx>

#include <strings.hrc>
#include "unohelp.hxx"
#include <unolayer.hxx>
#include <unoprnms.hxx>
#include <unopage.hxx>
#include <unocpres.hxx>
#include <unoobj.hxx>
#include <stlpool.hxx>
#include <unopback.hxx>
#include <unokywds.hxx>
#include "FrameView.hxx"
#include "ClientView.hxx"
#include "DrawViewShell.hxx"
#include "ViewShell.hxx"
#include "app.hrc"
#include <vcl/pdfextoutdevdata.hxx>
#include <com/sun/star/presentation/AnimationEffect.hpp>
#include <com/sun/star/presentation/AnimationSpeed.hpp>
#include <com/sun/star/presentation/ClickAction.hpp>
#include <svx/sdr/contact/viewobjectcontact.hxx>
#include <svx/sdr/contact/viewcontact.hxx>
#include <svx/sdr/contact/displayinfo.hxx>

#include <com/sun/star/office/XAnnotation.hpp>
#include <com/sun/star/office/XAnnotationAccess.hpp>
#include <com/sun/star/office/XAnnotationEnumeration.hpp>
#include <com/sun/star/geometry/RealPoint2D.hpp>
#include <com/sun/star/util/DateTime.hpp>

#include <drawinglayer/primitive2d/structuretagprimitive2d.hxx>

using namespace ::osl;
using namespace ::cppu;
using namespace ::com::sun::star;
using namespace ::sd;

class SdUnoForbiddenCharsTable : public SvxUnoForbiddenCharsTable,
                                 public SfxListener
{
public:
    SdUnoForbiddenCharsTable( SdrModel* pModel );
    virtual ~SdUnoForbiddenCharsTable();

    // SfxListener
    virtual void Notify( SfxBroadcaster& rBC, const SfxHint& rHint ) throw () SAL_OVERRIDE;
protected:
    virtual void onChange() SAL_OVERRIDE;

private:
    SdrModel*   mpModel;
};

SdUnoForbiddenCharsTable::SdUnoForbiddenCharsTable( SdrModel* pModel )
: SvxUnoForbiddenCharsTable( pModel->GetForbiddenCharsTable() ), mpModel( pModel )
{
    StartListening( *pModel );
}

void SdUnoForbiddenCharsTable::onChange()
{
    if( mpModel )
    {
        mpModel->ReformatAllTextObjects();
    }
}

SdUnoForbiddenCharsTable::~SdUnoForbiddenCharsTable()
{
    if( mpModel )
        EndListening( *mpModel );
}

void SdUnoForbiddenCharsTable::Notify( SfxBroadcaster&, const SfxHint& rHint ) throw()
{
    const SdrHint* pSdrHint = PTR_CAST( SdrHint, &rHint );

    if( pSdrHint )
    {
        if( HINT_MODELCLEARED == pSdrHint->GetKind() )
        {
            mpModel = NULL;
        }
    }
}

const sal_Int32 WID_MODEL_LANGUAGE           =  1;
const sal_Int32 WID_MODEL_TABSTOP            =  2;
const sal_Int32 WID_MODEL_VISAREA            =  3;
const sal_Int32 WID_MODEL_MAPUNIT            =  4;
const sal_Int32 WID_MODEL_FORBCHARS          =  5;
const sal_Int32 WID_MODEL_CONTFOCUS          =  6;
const sal_Int32 WID_MODEL_DSGNMODE           =  7;
const sal_Int32 WID_MODEL_BASICLIBS          =  8;
const sal_Int32 WID_MODEL_RUNTIMEUID         =  9;
const sal_Int32 WID_MODEL_BUILDID            = 10;
const sal_Int32 WID_MODEL_HASVALIDSIGNATURES = 11;
const sal_Int32 WID_MODEL_DIALOGLIBS         = 12;
const sal_Int32 WID_MODEL_FONTS              = 13;
const sal_Int32 WID_MODEL_INTEROPGRABBAG     = 14;

const SvxItemPropertySet* ImplGetDrawModelPropertySet()
{
    // Attention: the first parameter HAS TO BE sorted!!!
    const static SfxItemPropertyMapEntry aDrawModelPropertyMap_Impl[] =
    {
        { OUString("BuildId"),                      WID_MODEL_BUILDID,            ::cppu::UnoType<OUString>::get(),                      0, 0},
        { OUString(sUNO_Prop_CharLocale),           WID_MODEL_LANGUAGE,           ::cppu::UnoType<lang::Locale>::get(),                                  0, 0},
        { OUString(sUNO_Prop_TabStop),              WID_MODEL_TABSTOP,            ::cppu::UnoType<sal_Int32>::get(),                                     0, 0},
        { OUString(sUNO_Prop_VisibleArea),          WID_MODEL_VISAREA,            ::cppu::UnoType<awt::Rectangle>::get(),                                0, 0},
        { OUString(sUNO_Prop_MapUnit),              WID_MODEL_MAPUNIT,            ::cppu::UnoType<sal_Int16>::get(),                                     beans::PropertyAttribute::READONLY, 0},
        { OUString(sUNO_Prop_ForbiddenCharacters),  WID_MODEL_FORBCHARS,          cppu::UnoType<i18n::XForbiddenCharacters>::get(), beans::PropertyAttribute::READONLY, 0},
        { OUString(sUNO_Prop_AutomContFocus),       WID_MODEL_CONTFOCUS,          ::getBooleanCppuType(),                                                 0, 0},
        { OUString(sUNO_Prop_ApplyFrmDsgnMode),     WID_MODEL_DSGNMODE,           ::getBooleanCppuType(),                                                 0, 0},
        { OUString("BasicLibraries"),               WID_MODEL_BASICLIBS,          cppu::UnoType<script::XLibraryContainer>::get(),  beans::PropertyAttribute::READONLY, 0},
        { OUString("DialogLibraries"),              WID_MODEL_DIALOGLIBS,         cppu::UnoType<script::XLibraryContainer>::get(),  beans::PropertyAttribute::READONLY, 0},
        { OUString(sUNO_Prop_RuntimeUID),           WID_MODEL_RUNTIMEUID,         ::cppu::UnoType<OUString>::get(),                      beans::PropertyAttribute::READONLY, 0},
        { OUString(sUNO_Prop_HasValidSignatures),   WID_MODEL_HASVALIDSIGNATURES, ::cppu::UnoType<sal_Bool>::get(),                      beans::PropertyAttribute::READONLY, 0},
        { OUString("Fonts"),                        WID_MODEL_FONTS,              ::getCppuType((uno::Sequence<uno::Any>*)0),                     beans::PropertyAttribute::READONLY, 0},
        { OUString(sUNO_Prop_InteropGrabBag),       WID_MODEL_INTEROPGRABBAG,     ::getCppuType((uno::Sequence< beans::PropertyValue >*)0),       0, 0},
        { OUString(), 0, css::uno::Type(), 0, 0 }
    };
    static SvxItemPropertySet aDrawModelPropertySet_Impl( aDrawModelPropertyMap_Impl, SdrObject::GetGlobalDrawObjectItemPool() );
    return &aDrawModelPropertySet_Impl;
}

// this ctor is used from the DocShell
SdXImpressDocument::SdXImpressDocument (::sd::DrawDocShell* pShell, bool bClipBoard ) throw()
:   SfxBaseModel( pShell ),
    mpDocShell( pShell ),
    mpDoc( pShell ? pShell->GetDoc() : NULL ),
    mbDisposed(false),
    mbImpressDoc( pShell && pShell->GetDoc() && pShell->GetDoc()->GetDocumentType() == DOCUMENT_TYPE_IMPRESS ),
    mbClipBoard( bClipBoard ),
    mpPropSet( ImplGetDrawModelPropertySet() )
{
    if( mpDoc )
    {
        StartListening( *mpDoc );
    }
    else
    {
        OSL_FAIL("DocShell is invalid");
    }
}

SdXImpressDocument::SdXImpressDocument( SdDrawDocument* pDoc, bool bClipBoard ) throw()
:   SfxBaseModel( NULL ),
    mpDocShell( NULL ),
    mpDoc( pDoc ),
    mbDisposed(false),
    mbImpressDoc( pDoc && pDoc->GetDocumentType() == DOCUMENT_TYPE_IMPRESS ),
    mbClipBoard( bClipBoard ),
    mpPropSet( ImplGetDrawModelPropertySet() )
{
    if( mpDoc )
    {
        StartListening( *mpDoc );
    }
    else
    {
        OSL_FAIL("SdDrawDocument is invalid");
    }
}

/***********************************************************************
*                                                                      *
***********************************************************************/
SdXImpressDocument::~SdXImpressDocument() throw()
{
}

// XInterface
uno::Any SAL_CALL SdXImpressDocument::queryInterface( const uno::Type & rType ) throw(uno::RuntimeException, std::exception)
{
    uno::Any aAny;

    QUERYINT(lang::XServiceInfo);
    else QUERYINT(beans::XPropertySet);
    else QUERYINT(lang::XMultiServiceFactory);
    else QUERYINT(drawing::XDrawPageDuplicator);
    else QUERYINT(drawing::XLayerSupplier);
    else QUERYINT(drawing::XMasterPagesSupplier);
    else QUERYINT(drawing::XDrawPagesSupplier);
    else QUERYINT(presentation::XHandoutMasterSupplier);
    else QUERYINT(document::XLinkTargetSupplier);
    else QUERYINT(style::XStyleFamiliesSupplier);
    else QUERYINT(com::sun::star::ucb::XAnyCompareFactory);
    else QUERYINT(view::XRenderable);
    else if( mbImpressDoc && rType == cppu::UnoType<presentation::XPresentationSupplier>::get() )
            aAny <<= uno::Reference< presentation::XPresentationSupplier >(this);
    else if( mbImpressDoc && rType == cppu::UnoType<presentation::XCustomPresentationSupplier>::get() )
            aAny <<= uno::Reference< presentation::XCustomPresentationSupplier >(this);
    else
        return SfxBaseModel::queryInterface( rType );

    return aAny;
}

void SAL_CALL SdXImpressDocument::acquire() throw ( )
{
    SfxBaseModel::acquire();
}

void SAL_CALL SdXImpressDocument::release() throw ( )
{
    if (osl_atomic_decrement( &m_refCount ) == 0)
    {
        // restore reference count:
        osl_atomic_increment( &m_refCount );
        if(!mbDisposed)
        {
            try
            {
                dispose();
            }
            catch (const uno::RuntimeException& exc)
            { // don't break throw ()
                OSL_FAIL(
                    OUStringToOString(
                        exc.Message, RTL_TEXTENCODING_ASCII_US ).getStr() );
                static_cast<void>(exc);
            }
        }
        SfxBaseModel::release();
    }
}

namespace
{
    class theSdXImpressDocumentUnoTunnelId : public rtl::Static< UnoTunnelIdInit, theSdXImpressDocumentUnoTunnelId> {};
}

// XUnoTunnel
const ::com::sun::star::uno::Sequence< sal_Int8 > & SdXImpressDocument::getUnoTunnelId() throw()
{
    return theSdXImpressDocumentUnoTunnelId::get().getSeq();
}

SdXImpressDocument* SdXImpressDocument::getImplementation( const uno::Reference< uno::XInterface >& xInt )
{
    ::com::sun::star::uno::Reference< ::com::sun::star::lang::XUnoTunnel > xUT( xInt, ::com::sun::star::uno::UNO_QUERY );
    if( xUT.is() )
        return reinterpret_cast<SdXImpressDocument*>(sal::static_int_cast<sal_IntPtr>(xUT->getSomething( SdXImpressDocument::getUnoTunnelId() )));
    else
        return NULL;
}

sal_Int64 SAL_CALL SdXImpressDocument::getSomething( const ::com::sun::star::uno::Sequence< sal_Int8 >& rIdentifier ) throw(::com::sun::star::uno::RuntimeException, std::exception)
{
    if( rIdentifier.getLength() == 16 )
    {
        if( (0 == memcmp( SdXImpressDocument::getUnoTunnelId().getConstArray(), rIdentifier.getConstArray(), 16 )) )
            return sal::static_int_cast<sal_Int64>(reinterpret_cast<sal_IntPtr>(this));

        if( (0 == memcmp( SdrModel::getUnoTunnelImplementationId().getConstArray(), rIdentifier.getConstArray(), 16 )) )
            return sal::static_int_cast<sal_Int64>(reinterpret_cast<sal_IntPtr>(mpDoc));
    }

    return SfxBaseModel::getSomething( rIdentifier );
}

// XTypeProvider
uno::Sequence< uno::Type > SAL_CALL SdXImpressDocument::getTypes(  ) throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( maTypeSequence.getLength() == 0 )
    {
        const uno::Sequence< uno::Type > aBaseTypes( SfxBaseModel::getTypes() );
        const sal_Int32 nBaseTypes = aBaseTypes.getLength();
        const uno::Type* pBaseTypes = aBaseTypes.getConstArray();

        const sal_Int32 nOwnTypes = mbImpressDoc ? 14 : 11;     // !DANGER! Keep this updated!

        maTypeSequence.realloc(  nBaseTypes + nOwnTypes );
        uno::Type* pTypes = maTypeSequence.getArray();

        *pTypes++ = cppu::UnoType<beans::XPropertySet>::get();
        *pTypes++ = cppu::UnoType<lang::XServiceInfo>::get();
        *pTypes++ = cppu::UnoType<lang::XMultiServiceFactory>::get();
        *pTypes++ = cppu::UnoType<drawing::XDrawPageDuplicator>::get();
        *pTypes++ = cppu::UnoType<drawing::XLayerSupplier>::get();
        *pTypes++ = cppu::UnoType<drawing::XMasterPagesSupplier>::get();
        *pTypes++ = cppu::UnoType<drawing::XDrawPagesSupplier>::get();
        *pTypes++ = cppu::UnoType<document::XLinkTargetSupplier>::get();
        *pTypes++ = cppu::UnoType<style::XStyleFamiliesSupplier>::get();
        *pTypes++ = cppu::UnoType<com::sun::star::ucb::XAnyCompareFactory>::get();
        *pTypes++ = cppu::UnoType<view::XRenderable>::get();
        if( mbImpressDoc )
        {
            *pTypes++ = cppu::UnoType<presentation::XPresentationSupplier>::get();
            *pTypes++ = cppu::UnoType<presentation::XCustomPresentationSupplier>::get();
            *pTypes++ = cppu::UnoType<presentation::XHandoutMasterSupplier>::get();
        }

        for( sal_Int32 nType = 0; nType < nBaseTypes; nType++ )
            *pTypes++ = *pBaseTypes++;
    }

    return maTypeSequence;
}

uno::Sequence< sal_Int8 > SAL_CALL SdXImpressDocument::getImplementationId(  ) throw(uno::RuntimeException, std::exception)
{
    return css::uno::Sequence<sal_Int8>();
}

/***********************************************************************
*                                                                      *
***********************************************************************/
void SdXImpressDocument::Notify( SfxBroadcaster& rBC, const SfxHint& rHint )
{
    if( mpDoc )
    {
        const SdrHint* pSdrHint = PTR_CAST( SdrHint, &rHint );

        if( pSdrHint )
        {
            if( hasEventListeners() )
            {
                document::EventObject aEvent;
                if( SvxUnoDrawMSFactory::createEvent( mpDoc, pSdrHint, aEvent ) )
                    notifyEvent( aEvent );
            }

            if( pSdrHint->GetKind() == HINT_MODELCLEARED )
            {
                if( mpDoc )
                    EndListening( *mpDoc );
                mpDoc = NULL;
                mpDocShell = NULL;
            }
        }
        else
        {
            const SfxSimpleHint* pSfxHint = PTR_CAST(SfxSimpleHint, &rHint );

            // did our SdDrawDocument just died?
            if(pSfxHint && pSfxHint->GetId() == SFX_HINT_DYING)
            {
                // yes, so we ask for a new one
                if( mpDocShell )
                {
                    SdDrawDocument *pNewDoc = mpDocShell->GetDoc();

                    // is there a new one?
                    if( pNewDoc != mpDoc )
                    {
                        mpDoc = pNewDoc;
                        if(mpDoc)
                            StartListening( *mpDoc );
                    }
                }
            }
        }
    }
    SfxBaseModel::Notify( rBC, rHint );
}

/******************************************************************************
*                                                                             *
******************************************************************************/
SdPage* SdXImpressDocument::InsertSdPage( sal_uInt16 nPage, bool bDuplicate )
    throw (css::uno::RuntimeException, std::exception)
{
    sal_uInt16 nPageCount = mpDoc->GetSdPageCount( PK_STANDARD );
    SdrLayerAdmin& rLayerAdmin = mpDoc->GetLayerAdmin();
    sal_uInt8 aBckgrnd = rLayerAdmin.GetLayerID(SD_RESSTR(STR_LAYER_BCKGRND), false);
    sal_uInt8 aBckgrndObj = rLayerAdmin.GetLayerID(SD_RESSTR(STR_LAYER_BCKGRNDOBJ), false);

    SdPage* pStandardPage = NULL;

    if( 0 == nPageCount )
    {
        // this is only used for clipboard where we only have one page
        pStandardPage = mpDoc->AllocSdPage(false);

        Size aDefSize(21000, 29700);   // A4-Hochformat
        pStandardPage->SetSize( aDefSize );
        mpDoc->InsertPage(pStandardPage, 0);
    }
    else
    {
        // here we determine the page after which we should insert
        SdPage* pPreviousStandardPage = mpDoc->GetSdPage( std::min( (sal_uInt16)(nPageCount - 1), nPage ), PK_STANDARD );
        SetOfByte aVisibleLayers = pPreviousStandardPage->TRG_GetMasterPageVisibleLayers();
        bool bIsPageBack = aVisibleLayers.IsSet( aBckgrnd );
        bool bIsPageObj = aVisibleLayers.IsSet( aBckgrndObj );

        // AutoLayouts must be ready
        mpDoc->StopWorkStartupDelay();

        /* First we create a standard page and then a notes page. It is
           guaranteed, that after a standard page the corresponding notes page
           follows. */

        sal_uInt16 nStandardPageNum = pPreviousStandardPage->GetPageNum() + 2;
        SdPage* pPreviousNotesPage = (SdPage*) mpDoc->GetPage( nStandardPageNum - 1 );
        sal_uInt16 nNotesPageNum = nStandardPageNum + 1;
        OUString aStandardPageName;
        OUString aNotesPageName;

        /**************************************************************
        * standard page
        **************************************************************/
        if( bDuplicate )
            pStandardPage = (SdPage*) pPreviousStandardPage->Clone();
        else
            pStandardPage = mpDoc->AllocSdPage(false);

        pStandardPage->SetSize( pPreviousStandardPage->GetSize() );
        pStandardPage->SetBorder( pPreviousStandardPage->GetLftBorder(),
                                    pPreviousStandardPage->GetUppBorder(),
                                    pPreviousStandardPage->GetRgtBorder(),
                                    pPreviousStandardPage->GetLwrBorder() );
        pStandardPage->SetOrientation( pPreviousStandardPage->GetOrientation() );
        pStandardPage->SetName(aStandardPageName);

        // insert page after current page
        mpDoc->InsertPage(pStandardPage, nStandardPageNum);

        if( !bDuplicate )
        {
            // use MasterPage of the current page
            pStandardPage->TRG_SetMasterPage(pPreviousStandardPage->TRG_GetMasterPage());
            pStandardPage->SetLayoutName( pPreviousStandardPage->GetLayoutName() );
            pStandardPage->SetAutoLayout(AUTOLAYOUT_NONE, true );
        }

        aBckgrnd = rLayerAdmin.GetLayerID(SD_RESSTR(STR_LAYER_BCKGRND), false);
        aBckgrndObj = rLayerAdmin.GetLayerID(SD_RESSTR(STR_LAYER_BCKGRNDOBJ), false);
        aVisibleLayers.Set(aBckgrnd, bIsPageBack);
        aVisibleLayers.Set(aBckgrndObj, bIsPageObj);
        pStandardPage->TRG_SetMasterPageVisibleLayers(aVisibleLayers);

        /**************************************************************
        * notes page
        **************************************************************/
        SdPage* pNotesPage = NULL;

        if( bDuplicate )
            pNotesPage = (SdPage*) pPreviousNotesPage->Clone();
        else
            pNotesPage = mpDoc->AllocSdPage(false);

        pNotesPage->SetSize( pPreviousNotesPage->GetSize() );
        pNotesPage->SetBorder( pPreviousNotesPage->GetLftBorder(),
                                pPreviousNotesPage->GetUppBorder(),
                                pPreviousNotesPage->GetRgtBorder(),
                                pPreviousNotesPage->GetLwrBorder() );
        pNotesPage->SetOrientation( pPreviousNotesPage->GetOrientation() );
        pNotesPage->SetName(aNotesPageName);
        pNotesPage->SetPageKind(PK_NOTES);

        // insert page after current page
        mpDoc->InsertPage(pNotesPage, nNotesPageNum);

        if( !bDuplicate )
        {
            // use MasterPage of the current page
            pNotesPage->TRG_SetMasterPage(pPreviousNotesPage->TRG_GetMasterPage());
            pNotesPage->SetLayoutName( pPreviousNotesPage->GetLayoutName() );
            pNotesPage->SetAutoLayout(AUTOLAYOUT_NOTES, true );
        }
    }

    SetModified();

    return( pStandardPage );
}

void SdXImpressDocument::SetModified( bool bModified /* = sal_True */ ) throw()
{
    if( mpDoc )
        mpDoc->SetChanged( bModified );
}

// XModel
void SAL_CALL SdXImpressDocument    ::lockControllers(  )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    mpDoc->setLock(true);
}

void SAL_CALL SdXImpressDocument::unlockControllers(  )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    if( mpDoc->isLocked() )
    {
        mpDoc->setLock(false);
    }
}

sal_Bool SAL_CALL SdXImpressDocument::hasControllersLocked(  )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    return mpDoc && mpDoc->isLocked();
}

#include <comphelper/processfactory.hxx>

uno::Reference < container::XIndexAccess > SAL_CALL SdXImpressDocument::getViewData() throw( uno::RuntimeException, std::exception )
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    uno::Reference < container::XIndexAccess > xRet( SfxBaseModel::getViewData() );

    if( !xRet.is() )
    {
        const std::vector<sd::FrameView*> &rList = mpDoc->GetFrameViewList();

        if( !rList.empty() )
        {
            xRet = uno::Reference< container::XIndexAccess >(document::IndexedPropertyValues::create( ::comphelper::getProcessComponentContext() ), uno::UNO_QUERY);

            uno::Reference < container::XIndexContainer > xCont( xRet, uno::UNO_QUERY );
            DBG_ASSERT( xCont.is(), "SdXImpressDocument::getViewData() failed for OLE object" );
            if( xCont.is() )
            {
                for( sal_uInt32 i = 0, n = rList.size(); i < n; i++ )
                {
                    ::sd::FrameView* pFrameView = rList[ i ];

                    uno::Sequence< beans::PropertyValue > aSeq;
                    pFrameView->WriteUserDataSequence( aSeq );
                    xCont->insertByIndex( i, uno::makeAny( aSeq ) );
                }
            }
        }
    }

    return xRet;
}

void SAL_CALL SdXImpressDocument::setViewData( const uno::Reference < container::XIndexAccess >& xData ) throw(::com::sun::star::uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    SfxBaseModel::setViewData( xData );
    if( mpDocShell && (mpDocShell->GetCreateMode() == SFX_CREATE_MODE_EMBEDDED) && xData.is() )
    {
        const sal_Int32 nCount = xData->getCount();

        std::vector<sd::FrameView*>::iterator pIter;
        std::vector<sd::FrameView*> &rViews = mpDoc->GetFrameViewList();

        for ( pIter = rViews.begin(); pIter != rViews.end(); ++pIter )
            delete *pIter;

        rViews.clear();

        ::sd::FrameView* pFrameView;
        uno::Sequence< beans::PropertyValue > aSeq;
        for( sal_Int32 nIndex = 0; nIndex < nCount; nIndex++ )
        {
            if( xData->getByIndex( nIndex ) >>= aSeq )
            {
                pFrameView = new ::sd::FrameView( mpDoc );

                pFrameView->ReadUserDataSequence( aSeq );
                rViews.push_back( pFrameView );
            }
        }
    }
}

// XDrawPageDuplicator
uno::Reference< drawing::XDrawPage > SAL_CALL SdXImpressDocument::duplicate( const uno::Reference< drawing::XDrawPage >& xPage )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    // get pPage from xPage and determine the Id (nPos ) afterwards
    SvxDrawPage* pSvxPage = SvxDrawPage::getImplementation( xPage );
    if( pSvxPage )
    {
        SdPage* pPage = (SdPage*) pSvxPage->GetSdrPage();
        sal_uInt16 nPos = pPage->GetPageNum();
        nPos = ( nPos - 1 ) / 2;
        pPage = InsertSdPage( nPos, true );
        if( pPage )
        {
            uno::Reference< drawing::XDrawPage > xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
            return xDrawPage;
        }
    }

    uno::Reference< drawing::XDrawPage > xDrawPage;
    return xDrawPage;
}

// XDrawPagesSupplier
uno::Reference< drawing::XDrawPages > SAL_CALL SdXImpressDocument::getDrawPages()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    uno::Reference< drawing::XDrawPages >  xDrawPages( mxDrawPagesAccess );

    if( !xDrawPages.is() )
    {
        initializeDocument();
        mxDrawPagesAccess = xDrawPages = (drawing::XDrawPages*)new SdDrawPagesAccess(*this);
    }

    return xDrawPages;
}

// XMasterPagesSupplier
uno::Reference< drawing::XDrawPages > SAL_CALL SdXImpressDocument::getMasterPages()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    uno::Reference< drawing::XDrawPages >  xMasterPages( mxMasterPagesAccess );

    if( !xMasterPages.is() )
    {
        if ( !hasControllersLocked() )
            initializeDocument();
        mxMasterPagesAccess = xMasterPages = new SdMasterPagesAccess(*this);
    }

    return xMasterPages;
}

// XLayerManagerSupplier
uno::Reference< container::XNameAccess > SAL_CALL SdXImpressDocument::getLayerManager(  )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    uno::Reference< container::XNameAccess >  xLayerManager( mxLayerManager );

    if( !xLayerManager.is() )
        mxLayerManager = xLayerManager = new SdLayerManager(*this);

    return xLayerManager;
}

// XCustomPresentationSupplier
uno::Reference< container::XNameContainer > SAL_CALL SdXImpressDocument::getCustomPresentations()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    uno::Reference< container::XNameContainer >  xCustomPres( mxCustomPresentationAccess );

    if( !xCustomPres.is() )
        mxCustomPresentationAccess = xCustomPres = new SdXCustomPresentationAccess(*this);

    return xCustomPres;
}

// XPresentationSupplier
uno::Reference< presentation::XPresentation > SAL_CALL SdXImpressDocument::getPresentation()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    return uno::Reference< presentation::XPresentation >( mpDoc->getPresentation().get() );
}

// XHandoutMasterSupplier
uno::Reference< drawing::XDrawPage > SAL_CALL SdXImpressDocument::getHandoutMasterPage()
    throw (uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    uno::Reference< drawing::XDrawPage > xPage;

    if( mpDoc )
    {
        initializeDocument();
        SdPage* pPage = mpDoc->GetMasterSdPage( 0, PK_HANDOUT );
        if( pPage )
            xPage = uno::Reference< drawing::XDrawPage >::query( pPage->getUnoPage() );
    }
    return xPage;
}

// XMultiServiceFactory ( SvxFmMSFactory )

css::uno::Reference<css::uno::XInterface> SdXImpressDocument::create(
    OUString const & aServiceSpecifier, OUString const & referer)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    if( aServiceSpecifier == "com.sun.star.drawing.DashTable" )
    {
        if( !mxDashTable.is() )
            mxDashTable = SvxUnoDashTable_createInstance( mpDoc );

        return mxDashTable;
    }
    if( aServiceSpecifier == "com.sun.star.drawing.GradientTable" )
    {
        if( !mxGradientTable.is() )
            mxGradientTable = SvxUnoGradientTable_createInstance( mpDoc );

        return mxGradientTable;
    }
    if( aServiceSpecifier == "com.sun.star.drawing.HatchTable" )
    {
        if( !mxHatchTable.is() )
            mxHatchTable = SvxUnoHatchTable_createInstance( mpDoc );

        return mxHatchTable;
    }
    if( aServiceSpecifier == "com.sun.star.drawing.BitmapTable" )
    {
        if( !mxBitmapTable.is() )
            mxBitmapTable = SvxUnoBitmapTable_createInstance( mpDoc );

        return mxBitmapTable;
    }
    if( aServiceSpecifier == "com.sun.star.drawing.TransparencyGradientTable" )
    {
        if( !mxTransGradientTable.is() )
            mxTransGradientTable = SvxUnoTransGradientTable_createInstance( mpDoc );

        return mxTransGradientTable;
    }
    if( aServiceSpecifier == "com.sun.star.drawing.MarkerTable" )
    {
        if( !mxMarkerTable.is() )
            mxMarkerTable = SvxUnoMarkerTable_createInstance( mpDoc );

        return mxMarkerTable;
    }
    if( aServiceSpecifier == "com.sun.star.text.NumberingRules" )
    {
        return uno::Reference< uno::XInterface >( SvxCreateNumRule( mpDoc ), uno::UNO_QUERY );
    }
    if( aServiceSpecifier == "com.sun.star.drawing.Background" )
    {
        return uno::Reference< uno::XInterface >(
            static_cast<uno::XWeak*>(new SdUnoPageBackground( mpDoc )));
    }

    if( aServiceSpecifier == "com.sun.star.drawing.Defaults" )
    {
        if( !mxDrawingPool.is() )
            mxDrawingPool = SdUnoCreatePool( mpDoc );

        return mxDrawingPool;

    }

    if ( aServiceSpecifier == sUNO_Service_ImageMapRectangleObject )
    {
        return SvUnoImageMapRectangleObject_createInstance( ImplGetSupportedMacroItems() );
    }

    if ( aServiceSpecifier == sUNO_Service_ImageMapCircleObject )
    {
        return SvUnoImageMapCircleObject_createInstance( ImplGetSupportedMacroItems() );
    }

    if ( aServiceSpecifier == sUNO_Service_ImageMapPolygonObject )
    {
        return SvUnoImageMapPolygonObject_createInstance( ImplGetSupportedMacroItems() );
    }

    if( aServiceSpecifier == "com.sun.star.document.Settings" ||
        ( !mbImpressDoc && ( aServiceSpecifier == "com.sun.star.drawing.DocumentSettings" ) ) ||
        (  mbImpressDoc && ( aServiceSpecifier == "com.sun.star.presentation.DocumentSettings" ) ) )
    {
        return sd::DocumentSettings_createInstance( this );
    }

    if( aServiceSpecifier == "com.sun.star.text.TextField.DateTime" ||
        aServiceSpecifier == "com.sun.star.text.textfield.DateTime" )
    {
        return (::cppu::OWeakObject * )new SvxUnoTextField( text::textfield::Type::DATE );
    }

    if( aServiceSpecifier == "com.sun.star.presentation.TextField.Header" ||
        aServiceSpecifier == "com.sun.star.presentation.textfield.Header" )
    {
        return (::cppu::OWeakObject * )new SvxUnoTextField( text::textfield::Type::PRESENTATION_HEADER );
    }

    if( aServiceSpecifier == "com.sun.star.presentation.TextField.Footer" ||
        aServiceSpecifier == "com.sun.star.presentation.textfield.Footer" )
    {
        return (::cppu::OWeakObject * )new SvxUnoTextField( text::textfield::Type::PRESENTATION_FOOTER );
    }

    if( aServiceSpecifier == "com.sun.star.presentation.TextField.DateTime" ||
        aServiceSpecifier == "com.sun.star.presentation.textfield.DateTime" )
    {
        return (::cppu::OWeakObject * )new SvxUnoTextField( text::textfield::Type::PRESENTATION_DATE_TIME );
    }

    if( aServiceSpecifier == "com.sun.star.xml.NamespaceMap" )
    {
        static sal_uInt16 aWhichIds[] = { SDRATTR_XMLATTRIBUTES, EE_CHAR_XMLATTRIBS, EE_PARA_XMLATTRIBS, 0 };

        return svx::NamespaceMap_createInstance( aWhichIds, &mpDoc->GetItemPool() );
    }

    // Support creation of GraphicObjectResolver and EmbeddedObjectResolver
    if( aServiceSpecifier == "com.sun.star.document.ExportGraphicObjectResolver" )
    {
        return (::cppu::OWeakObject * )new SvXMLGraphicHelper( GRAPHICHELPER_MODE_WRITE );
    }

    if( aServiceSpecifier == "com.sun.star.document.ImportGraphicObjectResolver" )
    {
        return (::cppu::OWeakObject * )new SvXMLGraphicHelper( GRAPHICHELPER_MODE_READ );
    }

    if( aServiceSpecifier == "com.sun.star.document.ExportEmbeddedObjectResolver" )
    {
        ::comphelper::IEmbeddedHelper *pPersist = mpDoc ? mpDoc->GetPersist() : NULL;
        if( NULL == pPersist )
            throw lang::DisposedException();

        return (::cppu::OWeakObject * )new SvXMLEmbeddedObjectHelper( *pPersist, EMBEDDEDOBJECTHELPER_MODE_WRITE );
    }

    if( aServiceSpecifier == "com.sun.star.document.ImportEmbeddedObjectResolver" )
    {
        ::comphelper::IEmbeddedHelper *pPersist = mpDoc ? mpDoc->GetPersist() : NULL;
        if( NULL == pPersist )
            throw lang::DisposedException();

        return (::cppu::OWeakObject * )new SvXMLEmbeddedObjectHelper( *pPersist, EMBEDDEDOBJECTHELPER_MODE_READ );
    }

    uno::Reference< uno::XInterface > xRet;

    if( aServiceSpecifier.startsWith( "com.sun.star.presentation.") )
    {
        const OUString aType( aServiceSpecifier.copy(26) );
        SvxShape* pShape = NULL;

        sal_uInt16 nType = OBJ_TEXT;
        // create a shape wrapper
        if( aType.startsWith( "TitleTextShape" ) )
        {
            nType = OBJ_TEXT;
        }
        else if( aType.startsWith( "OutlinerShape" ) )
        {
            nType = OBJ_TEXT;
        }
        else if( aType.startsWith( "SubtitleShape" ) )
        {
            nType = OBJ_TEXT;
        }
        else if( aType.startsWith( "GraphicObjectShape" ) )
        {
            nType = OBJ_GRAF;
        }
        else if( aType.startsWith( "PageShape" ) )
        {
            nType = OBJ_PAGE;
        }
        else if( aType.startsWith( "OLE2Shape" ) )
        {
            nType = OBJ_OLE2;
        }
        else if( aType.startsWith( "ChartShape" ) )
        {
            nType = OBJ_OLE2;
        }
        else if( aType.startsWith( "CalcShape" ) )
        {
            nType = OBJ_OLE2;
        }
        else if( aType.startsWith( "TableShape" ) )
        {
            nType = OBJ_TABLE;
        }
        else if( aType.startsWith( "OrgChartShape" ) )
        {
            nType = OBJ_OLE2;
        }
        else if( aType.startsWith( "NotesShape" ) )
        {
            nType = OBJ_TEXT;
        }
        else if( aType.startsWith( "HandoutShape" ) )
        {
            nType = OBJ_PAGE;
        }
        else if( aType.startsWith( "FooterShape" ) )
        {
            nType = OBJ_TEXT;
        }
        else if( aType.startsWith( "HeaderShape" ) )
        {
            nType = OBJ_TEXT;
        }
        else if( aType.startsWith( "SlideNumberShape" ) )
        {
            nType = OBJ_TEXT;
        }
        else if( aType.startsWith( "DateTimeShape" ) )
        {
            nType = OBJ_TEXT;
        }
        else if( aType.startsWith( "MediaShape" ) )
        {
            nType = OBJ_MEDIA;
        }
        else
        {
            throw lang::ServiceNotRegisteredException();
        }

        // create the API wrapper
        pShape = CreateSvxShapeByTypeAndInventor( nType, SdrInventor, referer );

        // set shape type
        if( pShape && !mbClipBoard )
            pShape->SetShapeType(aServiceSpecifier);

        xRet = (uno::XWeak*)pShape;
    }
    else if ( aServiceSpecifier == "com.sun.star.drawing.TableShape" )
    {
        SvxShape* pShape = CreateSvxShapeByTypeAndInventor( OBJ_TABLE, SdrInventor, referer );
        if( pShape && !mbClipBoard )
            pShape->SetShapeType(aServiceSpecifier);

        xRet = (uno::XWeak*)pShape;
    }
    else
    {
        xRet = SvxFmMSFactory::createInstance( aServiceSpecifier );
    }

    uno::Reference< drawing::XShape > xShape( xRet, uno::UNO_QUERY );
    SvxShape* pShape = xShape.is() ? SvxShape::getImplementation(xShape) : NULL;
    if (pShape)
    {
        xRet.clear();
        new SdXShape( pShape, (SdXImpressDocument*)this );
        xRet = xShape;
        xShape.clear();
    }

    return xRet;
}

uno::Reference< uno::XInterface > SAL_CALL SdXImpressDocument::createInstance( const OUString& aServiceSpecifier )
    throw(uno::Exception, uno::RuntimeException, std::exception)
{
    return create(aServiceSpecifier, "");
}

css::uno::Reference<css::uno::XInterface>
SdXImpressDocument::createInstanceWithArguments(
    OUString const & ServiceSpecifier,
    css::uno::Sequence<css::uno::Any> const & Arguments)
    throw (css::uno::Exception, css::uno::RuntimeException, std::exception)
{
    OUString arg;
    if ((ServiceSpecifier == "com.sun.star.drawing.GraphicObjectShape"
         || ServiceSpecifier == "com.sun.star.drawing.MediaShape"
         || ServiceSpecifier == "com.sun.star.presentation.MediaShape")
        && Arguments.getLength() == 1 && (Arguments[0] >>= arg))
    {
        return create(ServiceSpecifier, arg);
    }
    return SvxFmMSFactory::createInstanceWithArguments(
        ServiceSpecifier, Arguments);
}

uno::Sequence< OUString > SAL_CALL SdXImpressDocument::getAvailableServiceNames()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    const uno::Sequence< OUString > aSNS_ORG( SvxFmMSFactory::getAvailableServiceNames() );

    uno::Sequence< OUString > aSNS( mbImpressDoc ? (36) : (19) );

    sal_uInt16 i(0);

    aSNS[i++] = "com.sun.star.drawing.DashTable";
    aSNS[i++] = "com.sun.star.drawing.GradientTable";
    aSNS[i++] = "com.sun.star.drawing.HatchTable";
    aSNS[i++] = "com.sun.star.drawing.BitmapTable";
    aSNS[i++] = "com.sun.star.drawing.TransparencyGradientTable";
    aSNS[i++] = "com.sun.star.drawing.MarkerTable";
    aSNS[i++] = "com.sun.star.text.NumberingRules";
    aSNS[i++] = "com.sun.star.drawing.Background";
    aSNS[i++] = "com.sun.star.document.Settings";
    aSNS[i++] = sUNO_Service_ImageMapRectangleObject;
    aSNS[i++] = sUNO_Service_ImageMapCircleObject;
    aSNS[i++] = sUNO_Service_ImageMapPolygonObject;
    aSNS[i++] = "com.sun.star.xml.NamespaceMap";

    // Support creation of GraphicObjectResolver and EmbeddedObjectResolver
    aSNS[i++] = "com.sun.star.document.ExportGraphicObjectResolver";
    aSNS[i++] = "com.sun.star.document.ImportGraphicObjectResolver";
    aSNS[i++] = "com.sun.star.document.ExportEmbeddedObjectResolver";
    aSNS[i++] = "com.sun.star.document.ImportEmbeddedObjectResolver";
    aSNS[i++] = "com.sun.star.drawing.TableShape";

    if(mbImpressDoc)
    {
        aSNS[i++] = "com.sun.star.presentation.TitleTextShape";
        aSNS[i++] = "com.sun.star.presentation.OutlinerShape";
        aSNS[i++] = "com.sun.star.presentation.SubtitleShape";
        aSNS[i++] = "com.sun.star.presentation.GraphicObjectShape";
        aSNS[i++] = "com.sun.star.presentation.ChartShape";
        aSNS[i++] = "com.sun.star.presentation.PageShape";
        aSNS[i++] = "com.sun.star.presentation.OLE2Shape";
        aSNS[i++] = "com.sun.star.presentation.TableShape";
        aSNS[i++] = "com.sun.star.presentation.OrgChartShape";
        aSNS[i++] = "com.sun.star.presentation.NotesShape";
        aSNS[i++] = "com.sun.star.presentation.HandoutShape";
        aSNS[i++] = "com.sun.star.presentation.DocumentSettings";
        aSNS[i++] = "com.sun.star.presentation.FooterShape";
        aSNS[i++] = "com.sun.star.presentation.HeaderShape";
        aSNS[i++] = "com.sun.star.presentation.SlideNumberShape";
        aSNS[i++] = "com.sun.star.presentation.DateTimeShape";
        aSNS[i++] = "com.sun.star.presentation.CalcShape";
        aSNS[i++] = "com.sun.star.presentation.MediaShape";
    }
    else
    {
        aSNS[i++] = "com.sun.star.drawing.DocumentSettings";
    }

    DBG_ASSERT( i == aSNS.getLength(), "Sequence overrun!" );

    return comphelper::concatSequences( aSNS_ORG, aSNS );
}

// lang::XServiceInfo
OUString SAL_CALL SdXImpressDocument::getImplementationName()
    throw(uno::RuntimeException, std::exception)
{
    return OUString( "SdXImpressDocument" );
}

sal_Bool SAL_CALL SdXImpressDocument::supportsService( const OUString& ServiceName )
    throw(uno::RuntimeException, std::exception)
{
    return cppu::supportsService(this, ServiceName);
}

uno::Sequence< OUString > SAL_CALL SdXImpressDocument::getSupportedServiceNames() throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    uno::Sequence< OUString > aSeq( 4 );
    OUString* pServices = aSeq.getArray();

    *pServices++ = "com.sun.star.document.OfficeDocument";
    *pServices++ = "com.sun.star.drawing.GenericDrawingDocument";
    *pServices++ = "com.sun.star.drawing.DrawingDocumentFactory";

    if( mbImpressDoc )
        *pServices++ = "com.sun.star.presentation.PresentationDocument";
    else
        *pServices++ = "com.sun.star.drawing.DrawingDocument";

    return aSeq;
}

// XPropertySet
uno::Reference< beans::XPropertySetInfo > SAL_CALL SdXImpressDocument::getPropertySetInfo(  )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;
    return mpPropSet->getPropertySetInfo();
}

void SAL_CALL SdXImpressDocument::setPropertyValue( const OUString& aPropertyName, const uno::Any& aValue )
    throw(beans::UnknownPropertyException, beans::PropertyVetoException, lang::IllegalArgumentException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    const SfxItemPropertySimpleEntry* pEntry = mpPropSet->getPropertyMapEntry(aPropertyName);

    switch( pEntry ? pEntry->nWID : -1 )
    {
        case WID_MODEL_LANGUAGE:
        {
            lang::Locale aLocale;
            if(!(aValue >>= aLocale))
                throw lang::IllegalArgumentException();

            mpDoc->SetLanguage( LanguageTag::convertToLanguageType(aLocale), EE_CHAR_LANGUAGE );
            break;
        }
        case WID_MODEL_TABSTOP:
        {
            sal_Int32 nValue = 0;
            if(!(aValue >>= nValue) || nValue < 0 )
                throw lang::IllegalArgumentException();

            mpDoc->SetDefaultTabulator((sal_uInt16)nValue);
            break;
        }
        case WID_MODEL_VISAREA:
            {
                SfxObjectShell* pEmbeddedObj = mpDoc->GetDocSh();
                if( !pEmbeddedObj )
                    break;

                awt::Rectangle aVisArea;
                if( !(aValue >>= aVisArea) || (aVisArea.Width < 0) || (aVisArea.Height < 0) )
                    throw lang::IllegalArgumentException();

                pEmbeddedObj->SetVisArea( Rectangle( aVisArea.X, aVisArea.Y, aVisArea.X + aVisArea.Width - 1, aVisArea.Y + aVisArea.Height - 1 ) );
            }
            break;
        case WID_MODEL_CONTFOCUS:
            {
                bool bFocus = false;
                if( !(aValue >>= bFocus ) )
                    throw lang::IllegalArgumentException();
                mpDoc->SetAutoControlFocus( bFocus );
            }
            break;
        case WID_MODEL_DSGNMODE:
            {
                bool bMode = false;
                if( !(aValue >>= bMode ) )
                    throw lang::IllegalArgumentException();
                mpDoc->SetOpenInDesignMode( bMode );
            }
            break;
        case WID_MODEL_BUILDID:
            aValue >>= maBuildId;
            return;
        case WID_MODEL_MAPUNIT:
        case WID_MODEL_BASICLIBS:
        case WID_MODEL_RUNTIMEUID: // is read-only
        case WID_MODEL_DIALOGLIBS:
        case WID_MODEL_FONTS:
            throw beans::PropertyVetoException();
        case WID_MODEL_INTEROPGRABBAG:
            setGrabBagItem(aValue);
            break;
        default:
            throw beans::UnknownPropertyException();
    }

    SetModified();
}

uno::Any SAL_CALL SdXImpressDocument::getPropertyValue( const OUString& PropertyName )
    throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    uno::Any aAny;
    if( NULL == mpDoc )
        throw lang::DisposedException();

    const SfxItemPropertySimpleEntry* pEntry = mpPropSet->getPropertyMapEntry(PropertyName);

    switch( pEntry ? pEntry->nWID : -1 )
    {
        case WID_MODEL_LANGUAGE:
        {
            LanguageType eLang = mpDoc->GetLanguage( EE_CHAR_LANGUAGE );
            aAny <<= LanguageTag::convertToLocale( eLang);
            break;
        }
        case WID_MODEL_TABSTOP:
            aAny <<= (sal_Int32)mpDoc->GetDefaultTabulator();
            break;
        case WID_MODEL_VISAREA:
            {
                SfxObjectShell* pEmbeddedObj = mpDoc->GetDocSh();
                if( !pEmbeddedObj )
                    break;

                const Rectangle& aRect = pEmbeddedObj->GetVisArea();
                awt::Rectangle aVisArea( aRect.Left(), aRect.Top(), aRect.getWidth(), aRect.getHeight() );
                aAny <<= aVisArea;
            }
            break;
        case WID_MODEL_MAPUNIT:
            {
                SfxObjectShell* pEmbeddedObj = mpDoc->GetDocSh();
                if( !pEmbeddedObj )
                    break;

                sal_Int16 nMeasureUnit = 0;
                SvxMapUnitToMeasureUnit( pEmbeddedObj->GetMapUnit(), nMeasureUnit );
                aAny <<= (sal_Int16)nMeasureUnit;
        }
        break;
        case WID_MODEL_FORBCHARS:
        {
            aAny <<= getForbiddenCharsTable();
        }
        break;
        case WID_MODEL_CONTFOCUS:
            aAny <<= mpDoc->GetAutoControlFocus();
            break;
        case WID_MODEL_DSGNMODE:
            aAny <<= mpDoc->GetOpenInDesignMode();
            break;
        case WID_MODEL_BASICLIBS:
            aAny <<= mpDocShell->GetBasicContainer();
            break;
        case WID_MODEL_DIALOGLIBS:
            aAny <<= mpDocShell->GetDialogContainer();
            break;
        case WID_MODEL_RUNTIMEUID:
            aAny <<= getRuntimeUID();
            break;
        case WID_MODEL_BUILDID:
            return uno::Any( maBuildId );
        case WID_MODEL_HASVALIDSIGNATURES:
            aAny <<= hasValidSignatures();
            break;
        case WID_MODEL_FONTS:
            {
                uno::Sequence<uno::Any> aSeq;
                int nSeqIndex = 0;

                sal_uInt16 aWhichIds[3] = { EE_CHAR_FONTINFO, EE_CHAR_FONTINFO_CJK,
                                            EE_CHAR_FONTINFO_CTL };

                const SfxItemPool& rPool = mpDoc->GetPool();
                const SfxPoolItem* pItem;

                for( sal_uInt16 i=0; i<3; i++ )
                {
                    sal_uInt16 nWhichId = aWhichIds[i];
                    sal_uInt32 nItems = rPool.GetItemCount2( nWhichId );

                    aSeq.realloc( aSeq.getLength() + nItems*5 + 5 );

                    for( sal_uInt32 j = 0; j < nItems; ++j )
                    {
                        if( 0 != (pItem = rPool.GetItem2( nWhichId, j ) ) )
                        {
                            const SvxFontItem *pFont = (const SvxFontItem *)pItem;

                            aSeq[nSeqIndex++] <<= OUString(pFont->GetFamilyName());
                            aSeq[nSeqIndex++] <<= OUString(pFont->GetStyleName());
                            aSeq[nSeqIndex++] <<= sal_Int16(pFont->GetFamily());
                            aSeq[nSeqIndex++] <<= sal_Int16(pFont->GetPitch());
                            aSeq[nSeqIndex++] <<= sal_Int16(pFont->GetCharSet());
                        }
                    }

                    const SvxFontItem& rFont = (const SvxFontItem&)rPool.GetDefaultItem( nWhichId );

                    aSeq[nSeqIndex++] <<= OUString(rFont.GetFamilyName());
                    aSeq[nSeqIndex++] <<= OUString(rFont.GetStyleName());
                    aSeq[nSeqIndex++] <<= sal_Int16(rFont.GetFamily());
                    aSeq[nSeqIndex++] <<= sal_Int16(rFont.GetPitch());
                    aSeq[nSeqIndex++] <<= sal_Int16(rFont.GetCharSet());

                }

                aSeq.realloc( nSeqIndex );
                aAny <<= aSeq;
            break;
            }
        case WID_MODEL_INTEROPGRABBAG:
            getGrabBagItem(aAny);
            break;
        default:
            throw beans::UnknownPropertyException();
    }

    return aAny;
}

void SAL_CALL SdXImpressDocument::addPropertyChangeListener( const OUString& , const uno::Reference< beans::XPropertyChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception) {}
void SAL_CALL SdXImpressDocument::removePropertyChangeListener( const OUString& , const uno::Reference< beans::XPropertyChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception) {}
void SAL_CALL SdXImpressDocument::addVetoableChangeListener( const OUString& , const uno::Reference< beans::XVetoableChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception) {}
void SAL_CALL SdXImpressDocument::removeVetoableChangeListener( const OUString& , const uno::Reference< beans::XVetoableChangeListener >&  ) throw(beans::UnknownPropertyException, lang::WrappedTargetException, uno::RuntimeException, std::exception) {}

// XLinkTargetSupplier
uno::Reference< container::XNameAccess > SAL_CALL SdXImpressDocument::getLinks()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    uno::Reference< container::XNameAccess > xLinks( mxLinks );
    if( !xLinks.is() )
        mxLinks = xLinks = new SdDocLinkTargets( *this );
    return xLinks;
}

// XStyleFamiliesSupplier
uno::Reference< container::XNameAccess > SAL_CALL SdXImpressDocument::getStyleFamilies(  )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    uno::Reference< container::XNameAccess > xStyles( dynamic_cast< container::XNameAccess* >( mpDoc->GetStyleSheetPool()) );
    return xStyles;
}

// XAnyCompareFactory
uno::Reference< com::sun::star::ucb::XAnyCompare > SAL_CALL SdXImpressDocument::createAnyCompareByName( const OUString& )
    throw (uno::RuntimeException, std::exception)
{
    return SvxCreateNumRuleCompare();
}

// XRenderable
sal_Int32 SAL_CALL SdXImpressDocument::getRendererCount( const uno::Any& rSelection,
                                                         const uno::Sequence< beans::PropertyValue >&  )
    throw (lang::IllegalArgumentException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;
    sal_Int32   nRet = 0;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    if( mpDocShell && mpDoc )
    {
        uno::Reference< frame::XModel > xModel;

        rSelection >>= xModel;

        if( xModel == mpDocShell->GetModel() )
            nRet = mpDoc->GetSdPageCount( PK_STANDARD );
        else
        {
            uno::Reference< drawing::XShapes > xShapes;

            rSelection >>= xShapes;

            if( xShapes.is() && xShapes->getCount() )
                nRet = 1;
        }
    }
    return nRet;
}

uno::Sequence< beans::PropertyValue > SAL_CALL SdXImpressDocument::getRenderer( sal_Int32 , const uno::Any& ,
                                                                                const uno::Sequence< beans::PropertyValue >& rxOptions )
    throw (lang::IllegalArgumentException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    bool bExportNotesPages = false;
    for( sal_Int32 nProperty = 0, nPropertyCount = rxOptions.getLength(); nProperty < nPropertyCount; ++nProperty )
    {
        if ( rxOptions[ nProperty ].Name == "ExportNotesPages" )
            rxOptions[ nProperty].Value >>= bExportNotesPages;
    }
    uno::Sequence< beans::PropertyValue > aRenderer;
    if( mpDocShell && mpDoc )
    {
        awt::Size aPageSize;
        if ( bExportNotesPages )
        {
            Size aNotesPageSize = mpDoc->GetSdPage( 0, PK_NOTES )->GetSize();
            aPageSize = awt::Size( aNotesPageSize.Width(), aNotesPageSize.Height() );
        }
        else
        {
            const Rectangle aVisArea( mpDocShell->GetVisArea( embed::Aspects::MSOLE_DOCPRINT ) );
            aPageSize = awt::Size( aVisArea.GetWidth(), aVisArea.GetHeight() );
        }
        aRenderer.realloc( 1 );

        aRenderer[ 0 ].Name = "PageSize" ;
        aRenderer[ 0 ].Value <<= aPageSize;
    }
    return aRenderer;
}

class ImplRenderPaintProc : public ::sdr::contact::ViewObjectContactRedirector
{
    const SdrLayerAdmin&    rLayerAdmin;
    SdrPageView*            pSdrPageView;
    vcl::PDFExtOutDevData*  pPDFExtOutDevData;

    vcl::PDFWriter::StructElement ImplBegStructureTag( SdrObject& rObject );

public:
    bool IsVisible  ( const SdrObject* pObj ) const;
    bool IsPrintable( const SdrObject* pObj ) const;

    ImplRenderPaintProc( const SdrLayerAdmin& rLA, SdrPageView* pView, vcl::PDFExtOutDevData* pData );
    virtual ~ImplRenderPaintProc();

    // all default implementations just call the same methods at the original. To do something
    // different, overload the method and at least do what the method does.
    virtual drawinglayer::primitive2d::Primitive2DSequence createRedirectedPrimitive2DSequence(
        const sdr::contact::ViewObjectContact& rOriginal,
        const sdr::contact::DisplayInfo& rDisplayInfo) SAL_OVERRIDE;
};

ImplRenderPaintProc::ImplRenderPaintProc( const SdrLayerAdmin& rLA, SdrPageView* pView, vcl::PDFExtOutDevData* pData )
:   ViewObjectContactRedirector(),
    rLayerAdmin         ( rLA ),
    pSdrPageView        ( pView ),
    pPDFExtOutDevData   ( pData )
{
}

ImplRenderPaintProc::~ImplRenderPaintProc()
{
}

sal_Int32 ImplPDFGetBookmarkPage( const OUString& rBookmark, SdDrawDocument& rDoc )
{
    sal_Int32 nPage = -1;

    OSL_TRACE("GotoBookmark %s",
        OUStringToOString(rBookmark, RTL_TEXTENCODING_UTF8).getStr());

    OUString aBookmark( rBookmark );

    if( rBookmark.startsWith("#") )
        aBookmark = rBookmark.copy( 1 );

    // is the bookmark a page ?
    bool        bIsMasterPage;
    sal_uInt16  nPgNum = rDoc.GetPageByName( aBookmark, bIsMasterPage );
    SdrObject*  pObj = NULL;

    if ( nPgNum == SDRPAGE_NOTFOUND )
    {
        // is the bookmark a object ?
        pObj = rDoc.GetObj( aBookmark );
        if (pObj)
            nPgNum = pObj->GetPage()->GetPageNum();
    }
    if ( nPgNum != SDRPAGE_NOTFOUND )
        nPage = ( nPgNum - 1 ) / 2;
    return nPage;
}

void ImplPDFExportComments( uno::Reference< drawing::XDrawPage > xPage, vcl::PDFExtOutDevData& rPDFExtOutDevData )
{
    try
    {
        uno::Reference< office::XAnnotationAccess > xAnnotationAccess( xPage, uno::UNO_QUERY_THROW );
        uno::Reference< office::XAnnotationEnumeration > xAnnotationEnumeration( xAnnotationAccess->createAnnotationEnumeration() );

        LanguageType eLanguage = Application::GetSettings().GetLanguageTag().getLanguageType();
        while( xAnnotationEnumeration->hasMoreElements() )
        {
            uno::Reference< office::XAnnotation > xAnnotation( xAnnotationEnumeration->nextElement() );

            geometry::RealPoint2D aRealPoint2D( xAnnotation->getPosition() );
            uno::Reference< text::XText > xText( xAnnotation->getTextRange() );
            util::DateTime aDateTime( xAnnotation->getDateTime() );

            Date aDate( aDateTime.Day, aDateTime.Month, aDateTime.Year );
            Time aTime( Time::EMPTY );
            OUString aStr( SvxDateTimeField::GetFormatted( aDate, aTime, SVXDATEFORMAT_B, *(SD_MOD()->GetNumberFormatter()), eLanguage ) );

            vcl::PDFNote aNote;
            OUString sTitle( xAnnotation->getAuthor() );
            sTitle += ", ";
            sTitle += aStr;
            aNote.Title = sTitle;
            aNote.Contents = xText->getString();
            rPDFExtOutDevData.CreateNote( Rectangle( Point( static_cast< long >( aRealPoint2D.X * 100 ),
                static_cast< long >( aRealPoint2D.Y * 100 ) ), Size( 1000, 1000 ) ), aNote );
        }
    }
    catch (const uno::Exception&)
    {
    }
}

void ImplPDFExportShapeInteraction( uno::Reference< drawing::XShape > xShape, SdDrawDocument& rDoc, vcl::PDFExtOutDevData& rPDFExtOutDevData )
{
    const OUString sGroup   ( "com.sun.star.drawing.GroupShape");
    const OUString sOnClick ( "OnClick" );
    const OUString sBookmark( "Bookmark" );

    if ( xShape->getShapeType().equals( sGroup ) )
    {
        uno::Reference< container::XIndexAccess > xIndexAccess( xShape, uno::UNO_QUERY );
        if ( xIndexAccess.is() )
        {
            sal_Int32 i, nCount = xIndexAccess->getCount();
            for ( i = 0; i < nCount; i++ )
            {
                uno::Reference< drawing::XShape > xSubShape( xIndexAccess->getByIndex( i ), uno::UNO_QUERY );
                if ( xSubShape.is() )
                    ImplPDFExportShapeInteraction( xSubShape, rDoc, rPDFExtOutDevData );
            }
        }
    }
    else
    {
        uno::Reference< beans::XPropertySet > xShapePropSet( xShape, uno::UNO_QUERY );
        if( xShapePropSet.is() )
        {
            Size        aPageSize( rDoc.GetSdPage( 0, PK_STANDARD )->GetSize() );
            Point aPoint( 0, 0 );
            Rectangle   aPageRect( aPoint, aPageSize );

            awt::Point  aShapePos( xShape->getPosition() );
            awt::Size   aShapeSize( xShape->getSize() );
            Rectangle   aLinkRect( Point( aShapePos.X, aShapePos.Y ), Size( aShapeSize.Width, aShapeSize.Height ) );

            presentation::ClickAction eCa;
            uno::Any aAny( xShapePropSet->getPropertyValue( sOnClick ) );
            if ( aAny >>= eCa )
            {
                switch ( eCa )
                {
                    case presentation::ClickAction_LASTPAGE :
                    {
                        sal_Int32 nCount = rDoc.GetSdPageCount( PK_STANDARD );
                        sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, nCount - 1, vcl::PDFWriter::FitRectangle );
                        sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
                        rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
                    }
                    break;
                    case presentation::ClickAction_FIRSTPAGE :
                    {
                        sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, 0, vcl::PDFWriter::FitRectangle );
                        sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
                        rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
                    }
                    break;
                    case presentation::ClickAction_PREVPAGE :
                    {
                        sal_Int32 nDestPage = rPDFExtOutDevData.GetCurrentPageNumber();
                        if ( nDestPage )
                            nDestPage--;
                        sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, nDestPage, vcl::PDFWriter::FitRectangle );
                        sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
                        rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
                    }
                    break;
                    case presentation::ClickAction_NEXTPAGE :
                    {
                        sal_Int32 nDestPage = rPDFExtOutDevData.GetCurrentPageNumber() + 1;
                        sal_Int32 nLastPage = rDoc.GetSdPageCount( PK_STANDARD ) - 1;
                        if ( nDestPage > nLastPage )
                            nDestPage = nLastPage;
                        sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, nDestPage, vcl::PDFWriter::FitRectangle );
                        sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
                        rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
                    }
                    break;

                    case presentation::ClickAction_PROGRAM :
                    case presentation::ClickAction_BOOKMARK :
                    case presentation::ClickAction_DOCUMENT :
                    {
                        OUString aBookmark;
                        xShapePropSet->getPropertyValue( sBookmark ) >>= aBookmark;
                        if( !aBookmark.isEmpty() )
                        {
                            switch( eCa )
                            {
                                case presentation::ClickAction_DOCUMENT :
                                case presentation::ClickAction_PROGRAM :
                                {
                                    sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
                                    rPDFExtOutDevData.SetLinkURL( nLinkId, aBookmark );
                                }
                                break;
                                case presentation::ClickAction_BOOKMARK :
                                {
                                    sal_Int32 nPage = ImplPDFGetBookmarkPage( aBookmark, rDoc );
                                    if ( nPage != -1 )
                                    {
                                        sal_Int32 nDestId = rPDFExtOutDevData.CreateDest( aPageRect, nPage, vcl::PDFWriter::FitRectangle );
                                        sal_Int32 nLinkId = rPDFExtOutDevData.CreateLink( aLinkRect, -1 );
                                        rPDFExtOutDevData.SetLinkDest( nLinkId, nDestId );
                                    }
                                }
                                break;
                                default:
                                    break;
                            }
                        }
                    }
                    break;

                    case presentation::ClickAction_STOPPRESENTATION :
                    case presentation::ClickAction_SOUND :
                    case presentation::ClickAction_INVISIBLE :
                    case presentation::ClickAction_VERB :
                    case presentation::ClickAction_VANISH :
                    case presentation::ClickAction_MACRO :
                    default :
                    break;
                }
            }
        }
    }
}

vcl::PDFWriter::StructElement ImplRenderPaintProc::ImplBegStructureTag( SdrObject& rObject )
{
    vcl::PDFWriter::StructElement eElement(vcl::PDFWriter::NonStructElement);

    if ( pPDFExtOutDevData && pPDFExtOutDevData->GetIsExportTaggedPDF() )
    {
        sal_uInt32 nInventor   = rObject.GetObjInventor();
        sal_uInt16 nIdentifier = rObject.GetObjIdentifier();
        bool   bIsTextObj  = rObject.ISA( SdrTextObj );

        if ( nInventor == SdrInventor )
        {
            if ( nIdentifier == OBJ_GRUP )
                eElement = vcl::PDFWriter::Section;
            else if ( nIdentifier == OBJ_TITLETEXT )
                eElement = vcl::PDFWriter::Heading;
            else if ( nIdentifier == OBJ_OUTLINETEXT )
                eElement = vcl::PDFWriter::Division;
            else if ( !bIsTextObj || !((SdrTextObj&)rObject).HasText() )
                eElement = vcl::PDFWriter::Figure;
        }
    }

    return eElement;
}

drawinglayer::primitive2d::Primitive2DSequence ImplRenderPaintProc::createRedirectedPrimitive2DSequence(
    const sdr::contact::ViewObjectContact& rOriginal,
    const sdr::contact::DisplayInfo& rDisplayInfo)
{
    SdrObject* pObject = rOriginal.GetViewContact().TryToGetSdrObject();

    if(pObject)
    {
        drawinglayer::primitive2d::Primitive2DSequence xRetval;

        if(pObject->GetPage())
        {
            if(pObject->GetPage()->checkVisibility(rOriginal, rDisplayInfo, false))
            {
                if(IsVisible(pObject) && IsPrintable(pObject))
                {
                    const vcl::PDFWriter::StructElement eElement(ImplBegStructureTag( *pObject ));
                    const bool bTagUsed(vcl::PDFWriter::NonStructElement != eElement);

                    xRetval = ::sdr::contact::ViewObjectContactRedirector::createRedirectedPrimitive2DSequence(rOriginal, rDisplayInfo);

                    if(xRetval.hasElements() && bTagUsed)
                    {
                        // embed Primitive2DSequence in a structure tag element for
                        // exactly this purpose (StructureTagPrimitive2D)
                        const drawinglayer::primitive2d::Primitive2DReference xReference(new drawinglayer::primitive2d::StructureTagPrimitive2D(eElement, xRetval));
                        xRetval = drawinglayer::primitive2d::Primitive2DSequence(&xReference, 1);
                    }
                }
            }
        }

        return xRetval;
    }
    else
    {
        // not an object, maybe a page
        return sdr::contact::ViewObjectContactRedirector::createRedirectedPrimitive2DSequence(rOriginal, rDisplayInfo);
    }
}

bool ImplRenderPaintProc::IsVisible( const SdrObject* pObj ) const
{
    bool bVisible = true;
    SdrLayerID nLayerId = pObj->GetLayer();
    if( pSdrPageView )
    {
        const SdrLayer* pSdrLayer = rLayerAdmin.GetLayer( nLayerId );
        if ( pSdrLayer )
        {
            OUString aLayerName = pSdrLayer->GetName();
            bVisible = pSdrPageView->IsLayerVisible( aLayerName );
        }
    }
    return bVisible;
}
bool ImplRenderPaintProc::IsPrintable( const SdrObject* pObj ) const
{
    bool bPrintable = true;
    SdrLayerID nLayerId = pObj->GetLayer();
    if( pSdrPageView )
    {
        const SdrLayer* pSdrLayer = rLayerAdmin.GetLayer( nLayerId );
        if ( pSdrLayer )
        {
            OUString aLayerName = pSdrLayer->GetName();
            bPrintable = pSdrPageView->IsLayerPrintable( aLayerName );
        }
    }
    return bPrintable;

}
void SAL_CALL SdXImpressDocument::render( sal_Int32 nRenderer, const uno::Any& rSelection,
                                          const uno::Sequence< beans::PropertyValue >& rxOptions )
    throw (lang::IllegalArgumentException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpDoc )
        throw lang::DisposedException();

    if( mpDocShell && mpDoc )
    {
        uno::Reference< awt::XDevice >  xRenderDevice;
        const sal_Int32                 nPageNumber = nRenderer + 1;
        PageKind                        ePageKind = PK_STANDARD;
        bool                        bExportNotesPages = false;

        for( sal_Int32 nProperty = 0, nPropertyCount = rxOptions.getLength(); nProperty < nPropertyCount; ++nProperty )
        {
            if ( rxOptions[ nProperty ].Name == "RenderDevice" )
                rxOptions[ nProperty ].Value >>= xRenderDevice;
            else if ( rxOptions[ nProperty ].Name == "ExportNotesPages" )
            {
                rxOptions[ nProperty].Value >>= bExportNotesPages;
                if ( bExportNotesPages )
                    ePageKind = PK_NOTES;
            }
        }

        if( xRenderDevice.is() && nPageNumber && ( nPageNumber <= mpDoc->GetSdPageCount( ePageKind ) ) )
        {
            VCLXDevice*     pDevice = VCLXDevice::GetImplementation( xRenderDevice );
            OutputDevice*   pOut = pDevice ? pDevice->GetOutputDevice() : NULL;

            if( pOut )
            {
                vcl::PDFExtOutDevData* pPDFExtOutDevData = PTR_CAST( vcl::PDFExtOutDevData, pOut->GetExtOutDevData() );

                if ( !( (mpDoc->GetSdPage((sal_Int16) nPageNumber-1, PK_STANDARD))->IsExcluded() ) ||
                    (pPDFExtOutDevData && pPDFExtOutDevData->GetIsExportHiddenSlides()) )
                {
                    ::sd::ClientView* pView = new ::sd::ClientView( mpDocShell, pOut, NULL );
                    Rectangle               aVisArea = Rectangle( Point(), mpDoc->GetSdPage( (sal_uInt16)nPageNumber - 1, ePageKind )->GetSize() );
                    Region                  aRegion( aVisArea );

                    ::sd::ViewShell* pOldViewSh = mpDocShell->GetViewShell();
                    ::sd::View* pOldSdView = pOldViewSh ? pOldViewSh->GetView() : NULL;

                    if  ( pOldSdView )
                        pOldSdView->SdrEndTextEdit();

                    pView->SetHlplVisible( false );
                    pView->SetGridVisible( false );
                    pView->SetBordVisible( false );
                    pView->SetPageVisible( false );
                    pView->SetGlueVisible( false );

                    pOut->SetMapMode( MAP_100TH_MM );
                    pOut->IntersectClipRegion( aVisArea );

                    uno::Reference< frame::XModel > xModel;
                    rSelection >>= xModel;

                    if( xModel == mpDocShell->GetModel() )
                    {
                        pView->ShowSdrPage( mpDoc->GetSdPage( (sal_uInt16)nPageNumber - 1, ePageKind ));
                        SdrPageView* pPV = pView->GetSdrPageView();

                        if( pOldSdView )
                        {
                            SdrPageView* pOldPV = pOldSdView->GetSdrPageView();
                            if( pPV && pOldPV )
                            {
                                pPV->SetVisibleLayers( pOldPV->GetVisibleLayers() );
                                pPV->SetPrintableLayers( pOldPV->GetPrintableLayers() );
                            }
                        }

                        ImplRenderPaintProc aImplRenderPaintProc( mpDoc->GetLayerAdmin(),
                            pPV, pPDFExtOutDevData );

                        // background color for outliner :o
                        SdPage* pPage = pPV ? (SdPage*)pPV->GetPage() : NULL;
                        if( pPage )
                        {
                            SdrOutliner& rOutl = mpDoc->GetDrawOutliner( NULL );
                            bool bScreenDisplay(true);

                            if(bScreenDisplay && pOut && OUTDEV_PRINTER == pOut->GetOutDevType())
                            {
                                // #i75566# printing; suppress AutoColor BackgroundColor generation
                                // for visibility reasons by giving GetPageBackgroundColor()
                                // the needed hint
                                bScreenDisplay = false;
                            }

                            if(bScreenDisplay && pOut && pOut->GetPDFWriter())
                            {
                                // #i75566# PDF export; suppress AutoColor BackgroundColor generation (see above)
                                bScreenDisplay = false;
                            }

                            // #i75566# Name change GetBackgroundColor -> GetPageBackgroundColor and
                            // hint value if screen display. Only then the AutoColor mechanisms shall be applied
                            rOutl.SetBackgroundColor( pPage->GetPageBackgroundColor( pPV, bScreenDisplay ) );
                        }
                        pView->SdrPaintView::CompleteRedraw( pOut, aRegion, &aImplRenderPaintProc );

                        if ( pPDFExtOutDevData && pPage )
                        {
                            try
                            {
                                uno::Any aAny;
                                uno::Reference< drawing::XDrawPage > xPage( uno::Reference< drawing::XDrawPage >::query( pPage->getUnoPage() ) );
                                if ( xPage.is() )
                                {
                                    if ( pPDFExtOutDevData->GetIsExportNotes() )
                                        ImplPDFExportComments( xPage, *pPDFExtOutDevData );
                                    uno::Reference< beans::XPropertySet > xPagePropSet( xPage, uno::UNO_QUERY );
                                    if( xPagePropSet.is() )
                                    {
                                        // exporting object interactions to pdf

                                        // if necessary, the master page interactions will be exported first
                                        bool bIsBackgroundObjectsVisible = false;   // #i39428# IsBackgroundObjectsVisible not available for Draw
                                        if ( mbImpressDoc && xPagePropSet->getPropertySetInfo()->hasPropertyByName( "IsBackgroundObjectsVisible" ) )
                                            xPagePropSet->getPropertyValue( "IsBackgroundObjectsVisible" ) >>= bIsBackgroundObjectsVisible;
                                        if ( bIsBackgroundObjectsVisible && !pPDFExtOutDevData->GetIsExportNotesPages() )
                                        {
                                            uno::Reference< drawing::XMasterPageTarget > xMasterPageTarget( xPage, uno::UNO_QUERY );
                                            if ( xMasterPageTarget.is() )
                                            {
                                                uno::Reference< drawing::XDrawPage > xMasterPage = xMasterPageTarget->getMasterPage();
                                                if ( xMasterPage.is() )
                                                {
                                                    uno::Reference< drawing::XShapes> xShapes( xMasterPage, uno::UNO_QUERY );
                                                    sal_Int32 i, nCount = xShapes->getCount();
                                                    for ( i = 0; i < nCount; i++ )
                                                    {
                                                        aAny = xShapes->getByIndex( i );
                                                        uno::Reference< drawing::XShape > xShape;
                                                        if ( aAny >>= xShape )
                                                            ImplPDFExportShapeInteraction( xShape, *mpDoc, *pPDFExtOutDevData );
                                                    }
                                                }
                                            }
                                        }

                                        // exporting slide page object interactions
                                        uno::Reference< drawing::XShapes> xShapes( xPage, uno::UNO_QUERY );
                                        sal_Int32 i, nCount = xShapes->getCount();
                                        for ( i = 0; i < nCount; i++ )
                                        {
                                            aAny = xShapes->getByIndex( i );
                                            uno::Reference< drawing::XShape > xShape;
                                            if ( aAny >>= xShape )
                                                ImplPDFExportShapeInteraction( xShape, *mpDoc, *pPDFExtOutDevData );
                                        }

                                        // exporting transition effects to pdf
                                        if ( mbImpressDoc && !pPDFExtOutDevData->GetIsExportNotesPages() && pPDFExtOutDevData->GetIsExportTransitionEffects() )
                                        {
                                            const OUString sEffect( "Effect" );
                                            const OUString sSpeed ( "Speed" );
                                            sal_Int32 nTime = 800;
                                            presentation::AnimationSpeed aAs;
                                            if ( xPagePropSet->getPropertySetInfo( )->hasPropertyByName( sSpeed ) )
                                            {
                                                aAny = xPagePropSet->getPropertyValue( sSpeed );
                                                if ( aAny >>= aAs )
                                                {
                                                    switch( aAs )
                                                    {
                                                        case presentation::AnimationSpeed_SLOW : nTime = 1500; break;
                                                        case presentation::AnimationSpeed_FAST : nTime = 300; break;
                                                        default:
                                                        case presentation::AnimationSpeed_MEDIUM : nTime = 800;
                                                    }
                                                }
                                            }
                                            presentation::FadeEffect eFe;
                                            vcl::PDFWriter::PageTransition eType = vcl::PDFWriter::Regular;
                                            if ( xPagePropSet->getPropertySetInfo( )->hasPropertyByName( sEffect ) )
                                            {
                                                aAny = xPagePropSet->getPropertyValue( sEffect );
                                                if ( aAny >>= eFe )
                                                {
                                                    switch( eFe )
                                                    {
                                                        case presentation::FadeEffect_HORIZONTAL_LINES :
                                                        case presentation::FadeEffect_HORIZONTAL_CHECKERBOARD :
                                                        case presentation::FadeEffect_HORIZONTAL_STRIPES : eType = vcl::PDFWriter::BlindsHorizontal; break;

                                                        case presentation::FadeEffect_VERTICAL_LINES :
                                                        case presentation::FadeEffect_VERTICAL_CHECKERBOARD :
                                                        case presentation::FadeEffect_VERTICAL_STRIPES : eType = vcl::PDFWriter::BlindsVertical; break;

                                                        case presentation::FadeEffect_UNCOVER_TO_RIGHT :
                                                        case presentation::FadeEffect_UNCOVER_TO_UPPERRIGHT :
                                                        case presentation::FadeEffect_ROLL_FROM_LEFT :
                                                        case presentation::FadeEffect_FADE_FROM_UPPERLEFT :
                                                        case presentation::FadeEffect_MOVE_FROM_UPPERLEFT :
                                                        case presentation::FadeEffect_FADE_FROM_LEFT :
                                                        case presentation::FadeEffect_MOVE_FROM_LEFT : eType = vcl::PDFWriter::WipeLeftToRight; break;

                                                        case presentation::FadeEffect_UNCOVER_TO_BOTTOM :
                                                        case presentation::FadeEffect_UNCOVER_TO_LOWERRIGHT :
                                                        case presentation::FadeEffect_ROLL_FROM_TOP :
                                                        case presentation::FadeEffect_FADE_FROM_UPPERRIGHT :
                                                        case presentation::FadeEffect_MOVE_FROM_UPPERRIGHT :
                                                        case presentation::FadeEffect_FADE_FROM_TOP :
                                                        case presentation::FadeEffect_MOVE_FROM_TOP : eType = vcl::PDFWriter::WipeTopToBottom; break;

                                                        case presentation::FadeEffect_UNCOVER_TO_LEFT :
                                                        case presentation::FadeEffect_UNCOVER_TO_LOWERLEFT :
                                                        case presentation::FadeEffect_ROLL_FROM_RIGHT :

                                                        case presentation::FadeEffect_FADE_FROM_LOWERRIGHT :
                                                        case presentation::FadeEffect_MOVE_FROM_LOWERRIGHT :
                                                        case presentation::FadeEffect_FADE_FROM_RIGHT :
                                                        case presentation::FadeEffect_MOVE_FROM_RIGHT : eType = vcl::PDFWriter::WipeRightToLeft; break;

                                                        case presentation::FadeEffect_UNCOVER_TO_TOP :
                                                        case presentation::FadeEffect_UNCOVER_TO_UPPERLEFT :
                                                        case presentation::FadeEffect_ROLL_FROM_BOTTOM :
                                                        case presentation::FadeEffect_FADE_FROM_LOWERLEFT :
                                                        case presentation::FadeEffect_MOVE_FROM_LOWERLEFT :
                                                        case presentation::FadeEffect_FADE_FROM_BOTTOM :
                                                        case presentation::FadeEffect_MOVE_FROM_BOTTOM : eType = vcl::PDFWriter::WipeBottomToTop; break;

                                                        case presentation::FadeEffect_OPEN_VERTICAL : eType = vcl::PDFWriter::SplitHorizontalInward; break;
                                                        case presentation::FadeEffect_CLOSE_HORIZONTAL : eType = vcl::PDFWriter::SplitHorizontalOutward; break;

                                                        case presentation::FadeEffect_OPEN_HORIZONTAL : eType = vcl::PDFWriter::SplitVerticalInward; break;
                                                        case presentation::FadeEffect_CLOSE_VERTICAL : eType = vcl::PDFWriter::SplitVerticalOutward; break;

                                                        case presentation::FadeEffect_FADE_TO_CENTER : eType = vcl::PDFWriter::BoxInward; break;
                                                        case presentation::FadeEffect_FADE_FROM_CENTER : eType = vcl::PDFWriter::BoxOutward; break;

                                                        case presentation::FadeEffect_NONE : eType = vcl::PDFWriter::Regular; break;

                                                        case presentation::FadeEffect_RANDOM :
                                                        case presentation::FadeEffect_DISSOLVE :
                                                        default: eType = vcl::PDFWriter::Dissolve; break;
                                                    }
                                                }
                                            }

                                            if ( xPagePropSet->getPropertySetInfo( )->hasPropertyByName( sEffect ) ||
                                                xPagePropSet->getPropertySetInfo( )->hasPropertyByName( sSpeed ) )
                                            {
                                                pPDFExtOutDevData->SetPageTransition( eType, nTime, -1 );
                                            }
                                        }
                                    }
                                }

                                Size        aPageSize( mpDoc->GetSdPage( 0, PK_STANDARD )->GetSize() );
                                Point aPoint( 0, 0 );
                                Rectangle   aPageRect( aPoint, aPageSize );

                                // resolving links found in this page by the method ImpEditEngine::Paint
                                std::vector< vcl::PDFExtOutDevBookmarkEntry >& rBookmarks = pPDFExtOutDevData->GetBookmarks();
                                std::vector< vcl::PDFExtOutDevBookmarkEntry >::iterator aIBeg = rBookmarks.begin();
                                std::vector< vcl::PDFExtOutDevBookmarkEntry >::iterator aIEnd = rBookmarks.end();
                                while ( aIBeg != aIEnd )
                                {
                                    sal_Int32 nPage = ImplPDFGetBookmarkPage( aIBeg->aBookmark, *mpDoc );
                                    if ( nPage != -1 )
                                    {
                                        if ( aIBeg->nLinkId != -1 )
                                            pPDFExtOutDevData->SetLinkDest( aIBeg->nLinkId, pPDFExtOutDevData->CreateDest( aPageRect, nPage, vcl::PDFWriter::FitRectangle ) );
                                        else
                                            pPDFExtOutDevData->DescribeRegisteredDest( aIBeg->nDestId, aPageRect, nPage, vcl::PDFWriter::FitRectangle );
                                    }
                                    else
                                        pPDFExtOutDevData->SetLinkURL( aIBeg->nLinkId, aIBeg->aBookmark );
                                    ++aIBeg;
                                }
                                rBookmarks.clear();
                                //---> #i56629, #i40318
                                //get the page name, will be used as outline element in PDF bookmark pane
                                OUString aPageName = mpDoc->GetSdPage( (sal_uInt16)nPageNumber - 1 , PK_STANDARD )->GetName();
                                if( !aPageName.isEmpty() )
                                {
                                    // insert the bookmark to this page into the NamedDestinations
                                    if( pPDFExtOutDevData->GetIsExportNamedDestinations() )
                                        pPDFExtOutDevData->CreateNamedDest( aPageName, aPageRect,  nPageNumber - 1 );

                                    // add the name to the outline, (almost) same code as in sc/source/ui/unoobj/docuno.cxx
                                    // issue #i40318.

                                    if( pPDFExtOutDevData->GetIsExportBookmarks() )
                                    {
                                        // Destination Export
                                        const sal_Int32 nDestId =
                                            pPDFExtOutDevData->CreateDest( aPageRect , nPageNumber - 1 );

                                        // Create a new outline item:
                                        pPDFExtOutDevData->CreateOutlineItem( -1 , aPageName, nDestId );
                                    }
                                }
                                //<--- #i56629, #i40318
                            }
                            catch (const uno::Exception&)
                            {
                            }

                        }
                    }
                    else
                    {
                        uno::Reference< drawing::XShapes > xShapes;
                        rSelection >>= xShapes;

                        if( xShapes.is() && xShapes->getCount() )
                        {
                        SdrPageView* pPV = NULL;

                        ImplRenderPaintProc  aImplRenderPaintProc( mpDoc->GetLayerAdmin(),
                                            pOldSdView ? pOldSdView->GetSdrPageView() : NULL, pPDFExtOutDevData );

                            for( sal_uInt32 i = 0, nCount = xShapes->getCount(); i < nCount; i++ )
                            {
                                uno::Reference< drawing::XShape > xShape;
                                xShapes->getByIndex( i ) >>= xShape;

                                if( xShape.is() )
                                {
                                    SvxShape* pShape = SvxShape::getImplementation( xShape );

                                    if( pShape )
                                    {
                                        SdrObject* pObj = pShape->GetSdrObject();
                                        if( pObj && pObj->GetPage()
                                            && aImplRenderPaintProc.IsVisible( pObj )
                                                && aImplRenderPaintProc.IsPrintable( pObj ) )
                                        {
                                            if( !pPV )
                                                pPV = pView->ShowSdrPage( pObj->GetPage() );

                                            if( pPV )
                                                pView->MarkObj( pObj, pPV );
                                        }
                                    }
                                }
                            }
                            pView->DrawMarkedObj(*pOut);
                        }
                    }

                    delete pView;
                }
            }
        }
    }
}

void SdXImpressDocument::paintTile( VirtualDevice& rDevice,
                            int nOutputWidth, int nOutputHeight,
                            int nTilePosX, int nTilePosY,
                            long nTileWidth, long nTileHeight )
{
    // Scaling. Must convert from pixels to twips. We know
    // that VirtualDevices use a DPI of 96.
    // We specifically calculate these scales first as we're still
    // in TWIPs, and might as well minimise the number of conversions.
    Fraction scaleX = Fraction( nOutputWidth, 96 ) * Fraction(1440L) /
                                Fraction( nTileWidth);
    Fraction scaleY = Fraction( nOutputHeight, 96 ) * Fraction(1440L) /
                                Fraction( nTileHeight);

    // svx seems to be the only component that works natively in
    // 100th mm rather than TWIP. It makes most sense just to
    // convert here and in getDocumentSize, and leave the tiled
    // rendering API working in TWIPs.
    nTileWidth = convertTwipToMm100( nTileWidth );
    nTileHeight = convertTwipToMm100( nTileHeight );
    nTilePosX = convertTwipToMm100( nTilePosX );
    nTilePosY = convertTwipToMm100( nTilePosY );

    MapMode aMapMode = rDevice.GetMapMode();
    aMapMode.SetMapUnit( MAP_100TH_MM );
    aMapMode.SetOrigin( Point( -nTilePosX,
                               -nTilePosY) );
    aMapMode.SetScaleX( scaleX );
    aMapMode.SetScaleY( scaleY );

    rDevice.SetMapMode( aMapMode );

    rDevice.SetOutputSizePixel( Size(nOutputWidth, nOutputHeight) );
    mpDoc->GetDocSh()->GetViewShell()->GetView()->CompleteRedraw(
        &rDevice,
        Region(
            Rectangle( Point( nTilePosX, nTilePosY ),
                       Size( nTileWidth, nTileHeight ) ) ) );
}

void SdXImpressDocument::setPart( int nPart )
{
    DrawViewShell* pViewSh = dynamic_cast< DrawViewShell* >( mpDoc->GetDocSh()->GetViewShell() );
    if (pViewSh)
    {
        // TODO: have an API to allow selecting between PK_STANDARD (just slide)
        // and PK_NOTES (which shows the combined slide above notes). There is alo
        // a PK_HANDOUT -- that however just shows multiple empty pages (it's also
        // only possible to select page 0 in this mode, I have no idea how you
        // then actually select what is on the handout page, which defaults to
        // a 4x4 grid of empty pages).
        pViewSh->SwitchPage( nPart );
    }
}

int SdXImpressDocument::getParts()
{
    // TODO: master pages?
    // Read: drviews1.cxx
    return mpDoc->GetSdPageCount(PK_STANDARD);
}

int SdXImpressDocument::getPart()
{
    DrawViewShell* pViewSh = dynamic_cast< DrawViewShell* >( mpDoc->GetDocSh()->GetViewShell() );
    if (pViewSh)
    {
        // curPageId seems to start at 1
        return pViewSh->GetCurPageId() - 1;
    }
    return 0;
}

OUString SdXImpressDocument::getPartName( int nPart )
{
    SdPage* pPage = mpDoc->GetSdPage( nPart, PK_STANDARD );
    assert( pPage );
    return pPage->GetName();
}

void SdXImpressDocument::setPartMode( LibreOfficeKitPartMode ePartMode )
{
    DrawViewShell* pViewSh = dynamic_cast< DrawViewShell* >( mpDoc->GetDocSh()->GetViewShell() );
    if (!pViewSh)
    {
        return;
    }

    PageKind aPageKind( PK_STANDARD );
    switch ( ePartMode )
    {
    case LOK_PARTMODE_EMBEDDEDOBJ:
        // This will probably be handled higher up, i.e.
        // we probably shouldn't be handling this here.
        // (However we don't offer embedded object-only
        //  rendering anywhere yet, so this may be a
        //  completely incorrect comment.)
        assert( false );
        // And let's fall through in a normal build.
    case LOK_PARTMODE_DEFAULT:
    case LOK_PARTMODE_SLIDE:
        break;
    case LOK_PARTMODE_SLIDENOTES:
        aPageKind = PK_NOTES;
        break;
    case LOK_PARTMODE_NOTES:
        // TODO: this shows combined slides + notes
        aPageKind = PK_NOTES;
        break;
    }
    pViewSh->SetPageKind( aPageKind );
}

Size SdXImpressDocument::getDocumentSize()
{
    DrawViewShell* pViewSh = dynamic_cast<DrawViewShell*>(mpDoc->GetDocSh()->GetViewShell());
    if (!pViewSh)
    {
        SAL_WARN("sd", "DrawViewShell not available!");
        return Size();
    }

    SdrPageView* pCurPageView = pViewSh->GetView()->GetSdrPageView();
    Size aSize = pCurPageView->GetPageRect().GetSize();
    // Convert the size in 100th mm to TWIP
    // See paintTile above for further info.
    return Size(convertMm100ToTwip(aSize.getWidth()), convertMm100ToTwip(aSize.getHeight()));
}


uno::Reference< i18n::XForbiddenCharacters > SdXImpressDocument::getForbiddenCharsTable()
{
    uno::Reference< i18n::XForbiddenCharacters > xForb(mxForbidenCharacters);

    if( !xForb.is() )
        mxForbidenCharacters = xForb = new SdUnoForbiddenCharsTable( mpDoc );

    return xForb;
}

void SdXImpressDocument::initializeDocument()
{
    if( !mbClipBoard )
    {
        switch( mpDoc->GetPageCount() )
        {
        case 1:
        {
            // nasty hack to detect clipboard document
            mbClipBoard = true;
            break;
        }
        case 0:
        {
            mpDoc->CreateFirstPages();
            mpDoc->StopWorkStartupDelay();
            break;
        }
        }
    }
}

void SAL_CALL SdXImpressDocument::dispose() throw (::com::sun::star::uno::RuntimeException, std::exception)
{
    if( !mbDisposed )
    {
        {
            ::SolarMutexGuard aGuard;

            if( mpDoc )
            {
                EndListening( *mpDoc );
                mpDoc = NULL;
            }

            // Call the base class dispose() before setting the mbDisposed flag
            // to true.  The reason for this is that if close() has not yet been
            // called this is done in SfxBaseModel::dispose().  At the end of
            // that dispose() is called again.  It is important to forward this
            // second dispose() to the base class, too.
            // As a consequence the following code has to be able to be run twice.
            mbDisposed = true;
            SfxBaseModel::dispose();

            uno::Reference< container::XNameAccess > xStyles(mxStyleFamilies);
            if( xStyles.is() )
            {
                uno::Reference< lang::XComponent > xComp( xStyles, uno::UNO_QUERY );
                if( xComp.is() )
                    xComp->dispose();

                xStyles = 0;
            }

            uno::Reference< presentation::XPresentation > xPresentation( mxPresentation );
            if( xPresentation.is() )
            {
                uno::Reference< ::com::sun::star::presentation::XPresentation2 > xPres( mpDoc->getPresentation().get() );
                uno::Reference< lang::XComponent > xPresComp( xPres, uno::UNO_QUERY );
                if( xPresComp.is() )
                    xPresComp->dispose();
            }

            uno::Reference< container::XNameAccess > xLinks( mxLinks );
            if( xLinks.is() )
            {
                uno::Reference< lang::XComponent > xComp( xLinks, uno::UNO_QUERY );
                if( xComp.is() )
                    xComp->dispose();

                xLinks = 0;
            }

            uno::Reference< drawing::XDrawPages > xDrawPagesAccess( mxDrawPagesAccess );
            if( xDrawPagesAccess.is() )
            {
                uno::Reference< lang::XComponent > xComp( xDrawPagesAccess, uno::UNO_QUERY );
                if( xComp.is() )
                    xComp->dispose();

                xDrawPagesAccess = 0;
            }

            uno::Reference< drawing::XDrawPages > xMasterPagesAccess( mxMasterPagesAccess );
            if( xDrawPagesAccess.is() )
            {
                uno::Reference< lang::XComponent > xComp( xMasterPagesAccess, uno::UNO_QUERY );
                if( xComp.is() )
                    xComp->dispose();

                xDrawPagesAccess = 0;
            }

            uno::Reference< container::XNameAccess > xLayerManager( mxLayerManager );
            if( xLayerManager.is() )
            {
                uno::Reference< lang::XComponent > xComp( xLayerManager, uno::UNO_QUERY );
                if( xComp.is() )
                    xComp->dispose();

                xLayerManager = 0;
            }

            uno::Reference< container::XNameContainer > xCustomPresentationAccess( mxCustomPresentationAccess );
            if( xCustomPresentationAccess.is() )
            {
                uno::Reference< lang::XComponent > xComp( xCustomPresentationAccess, uno::UNO_QUERY );
                if( xComp.is() )
                    xComp->dispose();

                xCustomPresentationAccess = 0;
            }

            mxDashTable = 0;
            mxGradientTable = 0;
            mxHatchTable = 0;
            mxBitmapTable = 0;
            mxTransGradientTable = 0;
            mxMarkerTable = 0;
            mxDrawingPool = 0;
        }
    }
}

// class SdDrawPagesAccess

SdDrawPagesAccess::SdDrawPagesAccess( SdXImpressDocument& rMyModel )  throw()
:   mpModel( &rMyModel)
{
}

SdDrawPagesAccess::~SdDrawPagesAccess() throw()
{
}

// XIndexAccess
sal_Int32 SAL_CALL SdDrawPagesAccess::getCount()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    return mpModel->mpDoc->GetSdPageCount( PK_STANDARD );
}

uno::Any SAL_CALL SdDrawPagesAccess::getByIndex( sal_Int32 Index )
    throw(lang::IndexOutOfBoundsException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    uno::Any aAny;

    if( (Index < 0) || (Index >= mpModel->mpDoc->GetSdPageCount( PK_STANDARD ) ) )
        throw lang::IndexOutOfBoundsException();

    SdPage* pPage = mpModel->mpDoc->GetSdPage( (sal_uInt16)Index, PK_STANDARD );
    if( pPage )
    {
        uno::Reference< drawing::XDrawPage >  xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
        aAny <<= xDrawPage;
    }

    return aAny;
}

// XNameAccess
uno::Any SAL_CALL SdDrawPagesAccess::getByName( const OUString& aName ) throw(container::NoSuchElementException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    if( !aName.isEmpty() )
    {
        const sal_uInt16 nCount = mpModel->mpDoc->GetSdPageCount( PK_STANDARD );
        sal_uInt16 nPage;
        for( nPage = 0; nPage < nCount; nPage++ )
        {
            SdPage* pPage = mpModel->mpDoc->GetSdPage( nPage, PK_STANDARD );
            if(NULL == pPage)
                continue;

            if( aName == SdDrawPage::getPageApiName( pPage ) )
            {
                uno::Any aAny;
                uno::Reference< drawing::XDrawPage >  xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
                aAny <<= xDrawPage;
                return aAny;
            }
        }
    }

    throw container::NoSuchElementException();
}

uno::Sequence< OUString > SAL_CALL SdDrawPagesAccess::getElementNames() throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    const sal_uInt16 nCount = mpModel->mpDoc->GetSdPageCount( PK_STANDARD );
    uno::Sequence< OUString > aNames( nCount );
    OUString* pNames = aNames.getArray();

    sal_uInt16 nPage;
    for( nPage = 0; nPage < nCount; nPage++ )
    {
        SdPage* pPage = mpModel->mpDoc->GetSdPage( nPage, PK_STANDARD );
        *pNames++ = SdDrawPage::getPageApiName( pPage );
    }

    return aNames;
}

sal_Bool SAL_CALL SdDrawPagesAccess::hasByName( const OUString& aName ) throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    const sal_uInt16 nCount = mpModel->mpDoc->GetSdPageCount( PK_STANDARD );
    sal_uInt16 nPage;
    for( nPage = 0; nPage < nCount; nPage++ )
    {
        SdPage* pPage = mpModel->mpDoc->GetSdPage( nPage, PK_STANDARD );
        if(NULL == pPage)
            continue;

        if( aName == SdDrawPage::getPageApiName( pPage ) )
            return sal_True;
    }

    return sal_False;
}

// XElementAccess
uno::Type SAL_CALL SdDrawPagesAccess::getElementType()
    throw(uno::RuntimeException, std::exception)
{
    return cppu::UnoType<drawing::XDrawPage>::get();
}

sal_Bool SAL_CALL SdDrawPagesAccess::hasElements()
    throw(uno::RuntimeException, std::exception)
{
    return getCount() > 0;
}

// XDrawPages

/**
 * Creates a new page with model at the specified position.
 * @returns corresponding SdDrawPage
 */
uno::Reference< drawing::XDrawPage > SAL_CALL SdDrawPagesAccess::insertNewByIndex( sal_Int32 nIndex )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    if( mpModel->mpDoc )
    {
        SdPage* pPage = mpModel->InsertSdPage( (sal_uInt16)nIndex );
        if( pPage )
        {
            uno::Reference< drawing::XDrawPage > xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
            return xDrawPage;
        }
    }
    uno::Reference< drawing::XDrawPage > xDrawPage;
    return xDrawPage;
}

/**
 * Removes the specified SdDrawPage from the model and the internal list. It
 * only works, if there is at least one *normal* page in the model after
 * removing this page.
 */
void SAL_CALL SdDrawPagesAccess::remove( const uno::Reference< drawing::XDrawPage >& xPage )
        throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel || mpModel->mpDoc == NULL )
        throw lang::DisposedException();

    SdDrawDocument& rDoc = *mpModel->mpDoc;

    sal_uInt16 nPageCount = rDoc.GetSdPageCount( PK_STANDARD );
    if( nPageCount > 1 )
    {
        // get pPage from xPage and determine the Id (nPos ) afterwards
        SdDrawPage* pSvxPage = SdDrawPage::getImplementation( xPage );
        if( pSvxPage )
        {
            SdPage* pPage = (SdPage*) pSvxPage->GetSdrPage();
            if(pPage && ( pPage->GetPageKind() == PK_STANDARD ) )
            {
                sal_uInt16 nPage = pPage->GetPageNum();

                SdPage* pNotesPage = static_cast< SdPage* >( rDoc.GetPage( nPage+1 ) );

                bool bUndo = rDoc.IsUndoEnabled();
                if( bUndo )
                {
                    // Add undo actions and delete the pages.  The order of adding
                    // the undo actions is important.
                    rDoc.BegUndo( SdResId( STR_UNDO_DELETEPAGES ) );
                    rDoc.AddUndo(rDoc.GetSdrUndoFactory().CreateUndoDeletePage(*pNotesPage));
                    rDoc.AddUndo(rDoc.GetSdrUndoFactory().CreateUndoDeletePage(*pPage));
                }

                rDoc.RemovePage( nPage ); // the page
                rDoc.RemovePage( nPage ); // the notes page

                if( bUndo )
                {
                    rDoc.EndUndo();
                }
                else
                {
                    delete pNotesPage;
                    delete pPage;
                }
            }
        }
    }

    mpModel->SetModified();
}

// XServiceInfo
const char pSdDrawPagesAccessService[] = "com.sun.star.drawing.DrawPages";

OUString SAL_CALL SdDrawPagesAccess::getImplementationName(  ) throw(uno::RuntimeException, std::exception)
{
    return OUString( "SdDrawPagesAccess" );
}

sal_Bool SAL_CALL SdDrawPagesAccess::supportsService( const OUString& ServiceName ) throw(uno::RuntimeException, std::exception)
{
    return cppu::supportsService(this, ServiceName);
}

uno::Sequence< OUString > SAL_CALL SdDrawPagesAccess::getSupportedServiceNames(  ) throw(uno::RuntimeException, std::exception)
{
    OUString aService( pSdDrawPagesAccessService );
    uno::Sequence< OUString > aSeq( &aService, 1 );
    return aSeq;
}

// XComponent
void SAL_CALL SdDrawPagesAccess::dispose(  ) throw (uno::RuntimeException, std::exception)
{
    mpModel = NULL;
}

void SAL_CALL SdDrawPagesAccess::addEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException, std::exception)
{
    OSL_FAIL( "not implemented!" );
}

void SAL_CALL SdDrawPagesAccess::removeEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException, std::exception)
{
    OSL_FAIL( "not implemented!" );
}

// class SdMasterPagesAccess

SdMasterPagesAccess::SdMasterPagesAccess( SdXImpressDocument& rMyModel ) throw()
:   mpModel(&rMyModel)
{
}

SdMasterPagesAccess::~SdMasterPagesAccess() throw()
{
}

// XComponent
void SAL_CALL SdMasterPagesAccess::dispose(  ) throw (uno::RuntimeException, std::exception)
{
    mpModel = NULL;
}

void SAL_CALL SdMasterPagesAccess::addEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException, std::exception)
{
    OSL_FAIL( "not implemented!" );
}

void SAL_CALL SdMasterPagesAccess::removeEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException, std::exception)
{
    OSL_FAIL( "not implemented!" );
}

// XIndexAccess
sal_Int32 SAL_CALL SdMasterPagesAccess::getCount()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel->mpDoc )
        throw lang::DisposedException();

    return mpModel->mpDoc->GetMasterSdPageCount(PK_STANDARD);
}

/**
 * Provides a drawing::XDrawPage interface for accessing the Masterpage at the
 * specified position in the model.
 */
uno::Any SAL_CALL SdMasterPagesAccess::getByIndex( sal_Int32 Index )
    throw(lang::IndexOutOfBoundsException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    uno::Any aAny;

    if( (Index < 0) || (Index >= mpModel->mpDoc->GetMasterSdPageCount( PK_STANDARD ) ) )
        throw lang::IndexOutOfBoundsException();

    SdPage* pPage = mpModel->mpDoc->GetMasterSdPage( (sal_uInt16)Index, PK_STANDARD );
    if( pPage )
    {
        uno::Reference< drawing::XDrawPage >  xDrawPage( pPage->getUnoPage(), uno::UNO_QUERY );
        aAny <<= xDrawPage;
    }

    return aAny;
}

// XElementAccess
uno::Type SAL_CALL SdMasterPagesAccess::getElementType()
    throw(uno::RuntimeException, std::exception)
{
    return cppu::UnoType<drawing::XDrawPage>::get();
}

sal_Bool SAL_CALL SdMasterPagesAccess::hasElements()
    throw(uno::RuntimeException, std::exception)
{
    return getCount() > 0;
}

// XDrawPages
uno::Reference< drawing::XDrawPage > SAL_CALL SdMasterPagesAccess::insertNewByIndex( sal_Int32 nInsertPos )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    uno::Reference< drawing::XDrawPage > xDrawPage;

    SdDrawDocument* mpDoc = mpModel->mpDoc;
    if( mpDoc )
    {
        // calculate internal index and check for range errors
        const sal_Int32 nMPageCount = mpDoc->GetMasterPageCount();
        nInsertPos = nInsertPos * 2 + 1;
        if( nInsertPos < 0 || nInsertPos > nMPageCount )
            nInsertPos = nMPageCount;

        // now generate a unique name for the new masterpage
        const OUString aStdPrefix( SdResId(STR_LAYOUT_DEFAULT_NAME) );
        OUString aPrefix( aStdPrefix );

        bool bUnique = true;
        sal_Int32 i = 0;
        do
        {
            bUnique = true;
            for( sal_Int32 nMaster = 1; nMaster < nMPageCount; nMaster++ )
            {
                SdPage* pPage = (SdPage*)mpDoc->GetMasterPage((sal_uInt16)nMaster);
                if( pPage && pPage->GetName() == aPrefix )
                {
                    bUnique = false;
                    break;
                }
            }

            if( !bUnique )
            {
                i++;
                aPrefix = aStdPrefix + " " + OUString::number( i );
            }

        } while( !bUnique );

        OUString aLayoutName( aPrefix );
        aLayoutName += SD_LT_SEPARATOR;
        aLayoutName += SD_RESSTR(STR_LAYOUT_OUTLINE);

        // create styles
        ((SdStyleSheetPool*)mpDoc->GetStyleSheetPool())->CreateLayoutStyleSheets( aPrefix );

        // get the first page for initial size and border settings
        SdPage* pPage = mpModel->mpDoc->GetSdPage( (sal_uInt16)0, PK_STANDARD );
        SdPage* pRefNotesPage = mpModel->mpDoc->GetSdPage( (sal_uInt16)0, PK_NOTES);

        // create and instert new draw masterpage
        SdPage* pMPage = mpModel->mpDoc->AllocSdPage(true);
        pMPage->SetSize( pPage->GetSize() );
        pMPage->SetBorder( pPage->GetLftBorder(),
                           pPage->GetUppBorder(),
                           pPage->GetRgtBorder(),
                           pPage->GetLwrBorder() );
        pMPage->SetLayoutName( aLayoutName );
        mpDoc->InsertMasterPage(pMPage,  (sal_uInt16)nInsertPos);

        {
            // ensure default MasterPage fill
            pMPage->EnsureMasterPageDefaultBackground();
        }

        xDrawPage = uno::Reference< drawing::XDrawPage >::query( pMPage->getUnoPage() );

        // create and instert new notes masterpage
        SdPage* pMNotesPage = mpModel->mpDoc->AllocSdPage(true);
        pMNotesPage->SetSize( pRefNotesPage->GetSize() );
        pMNotesPage->SetPageKind(PK_NOTES);
        pMNotesPage->SetBorder( pRefNotesPage->GetLftBorder(),
                                pRefNotesPage->GetUppBorder(),
                                pRefNotesPage->GetRgtBorder(),
                                pRefNotesPage->GetLwrBorder() );
        pMNotesPage->SetLayoutName( aLayoutName );
        mpDoc->InsertMasterPage(pMNotesPage,  (sal_uInt16)nInsertPos + 1);
        pMNotesPage->SetAutoLayout(AUTOLAYOUT_NOTES, true, true);
        mpModel->SetModified();
    }

    return( xDrawPage );
}

/**
 * Removes the specified SdMasterPage from the model and the internal list. It
 * only works, if there is no *normal* page using this page as MasterPage in
 * the model.
 */
void SAL_CALL SdMasterPagesAccess::remove( const uno::Reference< drawing::XDrawPage >& xPage )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel || mpModel->mpDoc == NULL )
        throw lang::DisposedException();

    SdDrawDocument& rDoc = *mpModel->mpDoc;

    SdMasterPage* pSdPage = SdMasterPage::getImplementation( xPage );
    if(pSdPage == NULL)
        return;

    SdPage* pPage = dynamic_cast< SdPage* > (pSdPage->GetSdrPage());

    DBG_ASSERT( pPage && pPage->IsMasterPage(), "SdMasterPage is not masterpage?");

    if( !pPage || !pPage->IsMasterPage() || (mpModel->mpDoc->GetMasterPageUserCount(pPage) > 0))
        return; //Todo: this should be excepted

    // only standard pages can be removed directly
    if( pPage->GetPageKind() == PK_STANDARD )
    {
        sal_uInt16 nPage = pPage->GetPageNum();

        SdPage* pNotesPage = static_cast< SdPage* >( rDoc.GetMasterPage( nPage+1 ) );

        bool bUndo = rDoc.IsUndoEnabled();
        if( bUndo )
        {
            // Add undo actions and delete the pages.  The order of adding
            // the undo actions is important.
            rDoc.BegUndo( SdResId( STR_UNDO_DELETEPAGES ) );
            rDoc.AddUndo(rDoc.GetSdrUndoFactory().CreateUndoDeletePage(*pNotesPage));
            rDoc.AddUndo(rDoc.GetSdrUndoFactory().CreateUndoDeletePage(*pPage));
        }

        // remove both pages
        rDoc.RemoveMasterPage( nPage );
        rDoc.RemoveMasterPage( nPage );

        if( bUndo )
        {
            rDoc.EndUndo();
        }
        else
        {
            delete pNotesPage;
            delete pPage;
        }
    }
}

// XServiceInfo
const char pSdMasterPagesAccessService[] = "com.sun.star.drawing.MasterPages";

OUString SAL_CALL SdMasterPagesAccess::getImplementationName(  ) throw(uno::RuntimeException, std::exception)
{
    return OUString( "SdMasterPagesAccess" );
}

sal_Bool SAL_CALL SdMasterPagesAccess::supportsService( const OUString& ServiceName ) throw(uno::RuntimeException, std::exception)
{
    return cppu::supportsService(this, ServiceName);
}

uno::Sequence< OUString > SAL_CALL SdMasterPagesAccess::getSupportedServiceNames(  ) throw(uno::RuntimeException, std::exception)
{
    OUString aService( pSdMasterPagesAccessService );
    uno::Sequence< OUString > aSeq( &aService, 1 );
    return aSeq;
}

// class SdDocLinkTargets

SdDocLinkTargets::SdDocLinkTargets( SdXImpressDocument& rMyModel ) throw()
: mpModel( &rMyModel )
{
}

SdDocLinkTargets::~SdDocLinkTargets() throw()
{
}

// XComponent
void SAL_CALL SdDocLinkTargets::dispose(  ) throw (uno::RuntimeException, std::exception)
{
    mpModel = NULL;
}

void SAL_CALL SdDocLinkTargets::addEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException, std::exception)
{
    OSL_FAIL( "not implemented!" );
}

void SAL_CALL SdDocLinkTargets::removeEventListener( const uno::Reference< lang::XEventListener >&  ) throw (uno::RuntimeException, std::exception)
{
    OSL_FAIL( "not implemented!" );
}

// XNameAccess
uno::Any SAL_CALL SdDocLinkTargets::getByName( const OUString& aName )
    throw(container::NoSuchElementException, lang::WrappedTargetException, uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    SdPage* pPage = FindPage( aName );

    if( pPage == NULL )
        throw container::NoSuchElementException();

    uno::Any aAny;

    uno::Reference< beans::XPropertySet > xProps( pPage->getUnoPage(), uno::UNO_QUERY );
    if( xProps.is() )
        aAny <<= xProps;

    return aAny;
}

uno::Sequence< OUString > SAL_CALL SdDocLinkTargets::getElementNames()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    SdDrawDocument* mpDoc = mpModel->GetDoc();
    if( mpDoc == NULL )
    {
        uno::Sequence< OUString > aSeq;
        return aSeq;
    }

    if( mpDoc->GetDocumentType() == DOCUMENT_TYPE_DRAW )
    {
        const sal_uInt16 nMaxPages = mpDoc->GetSdPageCount( PK_STANDARD );
        const sal_uInt16 nMaxMasterPages = mpDoc->GetMasterSdPageCount( PK_STANDARD );

        uno::Sequence< OUString > aSeq( nMaxPages + nMaxMasterPages );
        OUString* pStr = aSeq.getArray();

        sal_uInt16 nPage;
        // standard pages
        for( nPage = 0; nPage < nMaxPages; nPage++ )
            *pStr++ = mpDoc->GetSdPage( nPage, PK_STANDARD )->GetName();

        // master pages
        for( nPage = 0; nPage < nMaxMasterPages; nPage++ )
            *pStr++ = mpDoc->GetMasterSdPage( nPage, PK_STANDARD )->GetName();
        return aSeq;
    }
    else
    {
        const sal_uInt16 nMaxPages = mpDoc->GetPageCount();
        const sal_uInt16 nMaxMasterPages = mpDoc->GetMasterPageCount();

        uno::Sequence< OUString > aSeq( nMaxPages + nMaxMasterPages );
        OUString* pStr = aSeq.getArray();

        sal_uInt16 nPage;
        // standard pages
        for( nPage = 0; nPage < nMaxPages; nPage++ )
            *pStr++ = ((SdPage*)mpDoc->GetPage( nPage ))->GetName();

        // master pages
        for( nPage = 0; nPage < nMaxMasterPages; nPage++ )
            *pStr++ = ((SdPage*)mpDoc->GetMasterPage( nPage ))->GetName();
        return aSeq;
    }
}

sal_Bool SAL_CALL SdDocLinkTargets::hasByName( const OUString& aName )
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    return FindPage( aName ) != NULL;
}

// container::XElementAccess
uno::Type SAL_CALL SdDocLinkTargets::getElementType()
    throw(uno::RuntimeException, std::exception)
{
    return cppu::UnoType<beans::XPropertySet>::get();
}

sal_Bool SAL_CALL SdDocLinkTargets::hasElements()
    throw(uno::RuntimeException, std::exception)
{
    ::SolarMutexGuard aGuard;

    if( NULL == mpModel )
        throw lang::DisposedException();

    return mpModel->GetDoc() != NULL;
}

SdPage* SdDocLinkTargets::FindPage( const OUString& rName ) const throw()
{
    SdDrawDocument* mpDoc = mpModel->GetDoc();
    if( mpDoc == NULL )
        return NULL;

    const sal_uInt16 nMaxPages = mpDoc->GetPageCount();
    const sal_uInt16 nMaxMasterPages = mpDoc->GetMasterPageCount();

    sal_uInt16 nPage;
    SdPage* pPage;

    const OUString aName( rName );

    const bool bDraw = mpDoc->GetDocumentType() == DOCUMENT_TYPE_DRAW;

    // standard pages
    for( nPage = 0; nPage < nMaxPages; nPage++ )
    {
        pPage = (SdPage*)mpDoc->GetPage( nPage );
        if( (pPage->GetName() == aName) && (!bDraw || (pPage->GetPageKind() == PK_STANDARD)) )
            return pPage;
    }

    // master pages
    for( nPage = 0; nPage < nMaxMasterPages; nPage++ )
    {
        pPage = (SdPage*)mpDoc->GetMasterPage( nPage );
        if( (pPage->GetName() == aName) && (!bDraw || (pPage->GetPageKind() == PK_STANDARD)) )
            return pPage;
    }

    return NULL;
}

// XServiceInfo
OUString SAL_CALL SdDocLinkTargets::getImplementationName()
    throw(uno::RuntimeException, std::exception)
{
    return OUString( "SdDocLinkTargets" );
}

sal_Bool SAL_CALL SdDocLinkTargets::supportsService( const OUString& ServiceName )
    throw(uno::RuntimeException, std::exception)
{
    return cppu::supportsService( this, ServiceName );
}

uno::Sequence< OUString > SAL_CALL SdDocLinkTargets::getSupportedServiceNames()
    throw(uno::RuntimeException, std::exception)
{
    const OUString aSN( "com.sun.star.document.LinkTargets" );
    uno::Sequence< OUString > aSeq( &aSN, 1 );
    return aSeq;
}

rtl::Reference< SdXImpressDocument > SdXImpressDocument::GetModel( SdDrawDocument* pDocument )
{
    rtl::Reference< SdXImpressDocument > xRet;
    if( pDocument )
    {
        ::sd::DrawDocShell* pDocShell = pDocument->GetDocSh();
        if( pDocShell )
        {
            uno::Reference<frame::XModel> xModel(pDocShell->GetModel());

            xRet.set( dynamic_cast< SdXImpressDocument* >( xModel.get() ) );
        }
    }

    return xRet;
}

void NotifyDocumentEvent( SdDrawDocument* pDocument, const OUString& rEventName )
{
    rtl::Reference< SdXImpressDocument > xModel( SdXImpressDocument::GetModel( pDocument ) );

    if( xModel.is() )
    {
        uno::Reference< uno::XInterface > xSource( static_cast<uno::XWeak*>( xModel.get() ) );
        ::com::sun::star::document::EventObject aEvent( xSource, rEventName );
        xModel->notifyEvent(aEvent );
    }
}

void NotifyDocumentEvent( SdDrawDocument* pDocument, const OUString& rEventName, const uno::Reference< uno::XInterface >& xSource )
{
    rtl::Reference< SdXImpressDocument > xModel( SdXImpressDocument::GetModel( pDocument ) );

    if( xModel.is() )
    {
        ::com::sun::star::document::EventObject aEvent( xSource, rEventName );
        xModel->notifyEvent(aEvent );
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
