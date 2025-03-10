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

#include "svgwriter.hxx"
#include "svgfontexport.hxx"
#include "svgfilter.hxx"
#include "svgscript.hxx"
#include "impsvgdialog.hxx"

#include <com/sun/star/graphic/PrimitiveFactory2D.hpp>
#include <com/sun/star/drawing/GraphicExportFilter.hpp>
#include <com/sun/star/text/textfield/Type.hpp>
#include <com/sun/star/util/MeasureUnit.hpp>
#include <com/sun/star/xml/sax/Writer.hpp>

#include <rtl/bootstrap.hxx>
#include <svtools/miscopt.hxx>
#include <svx/unopage.hxx>
#include <svx/unoshape.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdoutl.hxx>
#include <editeng/outliner.hxx>
#include <editeng/flditem.hxx>
#include <editeng/numitem.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/sequenceashashmap.hxx>
#include <i18nlangtag/lang.h>
#include <svl/zforlist.hxx>
#include <xmloff/unointerfacetouniqueidentifiermapper.hxx>
#include <xmloff/animationexport.hxx>

#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/scoped_ptr.hpp>

using namespace ::com::sun::star::graphic;
using namespace ::com::sun::star::geometry;
using namespace ::com::sun::star;


// - ooo elements and attributes -


#define NSPREFIX "ooo:"

// ooo xml elements
static const char    aOOOElemMetaSlides[] = NSPREFIX "meta_slides";
static const char    aOOOElemMetaSlide[] = NSPREFIX "meta_slide";
static const char    aOOOElemTextField[] = NSPREFIX "text_field";

// ooo xml attributes for meta_slides
static const char    aOOOAttrNumberOfSlides[] = NSPREFIX "number-of-slides";
static const char    aOOOAttrStartSlideNumber[] = NSPREFIX "start-slide-number";
static const char    aOOOAttrNumberingType[] = NSPREFIX "page-numbering-type";

// ooo xml attributes for meta_slide
static const char    aOOOAttrSlide[] = NSPREFIX "slide";
static const char    aOOOAttrMaster[] = NSPREFIX "master";
static const char    aOOOAttrBackgroundVisibility[] = NSPREFIX "background-visibility";
static const char    aOOOAttrMasterObjectsVisibility[] = NSPREFIX "master-objects-visibility";
static const char    aOOOAttrPageNumberVisibility[] = NSPREFIX "page-number-visibility";
static const char    aOOOAttrDateTimeVisibility[] = NSPREFIX "date-time-visibility";
static const char    aOOOAttrFooterVisibility[] = NSPREFIX "footer-visibility";
static const OUString aOOOAttrDateTimeField = NSPREFIX "date-time-field";
static const char    aOOOAttrFooterField[] = NSPREFIX "footer-field";
static const char    aOOOAttrHeaderField[] = NSPREFIX "header-field";
static const char    aOOOAttrHasTransition[] = NSPREFIX "has-transition";
static const char    aOOOAttrIdList[] = NSPREFIX "id-list";

// ooo xml attributes for pages and shapes
static const char    aOOOAttrName[] = NSPREFIX "name";

// ooo xml attributes for date_time_field
static const char    aOOOAttrDateTimeFormat[] = NSPREFIX "date-time-format";

// ooo xml attributes for Placeholder Shapes
static const char    aOOOAttrTextAdjust[] = NSPREFIX "text-adjust";

static const char    constSvgNamespace[] = "http://www.w3.org/2000/svg";



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * - Text Field Class Hierarchy -                                  *
 *                                                                 *
 *  This is a set of classes for exporting text field meta info.   *
 *                                                                 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class TextField
{
protected:
    SVGFilter::ObjectSet mMasterPageSet;
public:

    virtual OUString getClassName() const
    {
        return OUString( "TextField" );
    }
    virtual bool equalTo( const TextField & aTextField ) const = 0;
    virtual void growCharSet( SVGFilter::UCharSetMapMap & aTextFieldCharSets ) const = 0;
    virtual void elementExport( SVGExport* pSVGExport ) const
    {
        pSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", getClassName() );
    }
    void insertMasterPage( Reference< XDrawPage> xMasterPage )
    {
        mMasterPageSet.insert( xMasterPage );
    }
    virtual ~TextField() {}
protected:
    void implGrowCharSet( SVGFilter::UCharSetMapMap & aTextFieldCharSets, const OUString& sText, const OUString& sTextFieldId ) const
    {
        const sal_Unicode * ustr = sText.getStr();
        sal_Int32 nLength = sText.getLength();
        SVGFilter::ObjectSet::const_iterator aMasterPageIt = mMasterPageSet.begin();
        for( ; aMasterPageIt != mMasterPageSet.end(); ++aMasterPageIt )
        {
            const Reference< XInterface > & xMasterPage = *aMasterPageIt;
            for( sal_Int32 i = 0; i < nLength; ++i )
            {
                aTextFieldCharSets[ xMasterPage ][ sTextFieldId ].insert( ustr[i] );
            }
        }
    }
};

class FixedTextField : public TextField
{
public:
    OUString text;

    virtual OUString getClassName() const SAL_OVERRIDE
    {
        return OUString( "FixedTextField" );
    }
    virtual bool equalTo( const TextField & aTextField ) const SAL_OVERRIDE
    {
        if( const FixedTextField* aFixedTextField = dynamic_cast< const FixedTextField* >( &aTextField ) )
        {
            return ( text == aFixedTextField->text );
        }
        return false;
    }
    virtual void elementExport( SVGExport* pSVGExport ) const SAL_OVERRIDE
    {
        TextField::elementExport( pSVGExport );
        SvXMLElementExport aExp( *pSVGExport, XML_NAMESPACE_NONE, "g", true, true );
        pSVGExport->GetDocHandler()->characters( text );
    }
    virtual ~FixedTextField() {}
};

class FixedDateTimeField : public FixedTextField
{
public:
    FixedDateTimeField() {}
    virtual OUString getClassName() const SAL_OVERRIDE
    {
        return OUString( "FixedDateTimeField" );
    }
    virtual void growCharSet( SVGFilter::UCharSetMapMap & aTextFieldCharSets ) const SAL_OVERRIDE
    {
        implGrowCharSet( aTextFieldCharSets, text, aOOOAttrDateTimeField );
    }
    virtual ~FixedDateTimeField() {}
};

class FooterField : public FixedTextField
{
public:
    FooterField() {}
    virtual OUString getClassName() const SAL_OVERRIDE
    {
        return OUString( "FooterField" );
    }
    virtual void growCharSet( SVGFilter::UCharSetMapMap & aTextFieldCharSets ) const SAL_OVERRIDE
    {
        static const OUString sFieldId = aOOOAttrFooterField;
        implGrowCharSet( aTextFieldCharSets, text, sFieldId );
    }
    virtual ~FooterField() {}
};

class VariableTextField : public TextField
{
public:
    virtual OUString getClassName() const SAL_OVERRIDE
    {
        return OUString( "VariableTextField" );
    }
    virtual ~VariableTextField() {}
};

class VariableDateTimeField : public VariableTextField
{
public:
    sal_Int32 format;

    VariableDateTimeField()
        : format(0)
    {
    }
    virtual OUString getClassName() const SAL_OVERRIDE
    {
        return OUString( "VariableDateTimeField" );
    }
    virtual bool equalTo( const TextField & aTextField ) const SAL_OVERRIDE
    {
        if( const VariableDateTimeField* aField = dynamic_cast< const VariableDateTimeField* >( &aTextField ) )
        {
            return ( format == aField->format );
        }
        return false;
    }
    virtual void elementExport( SVGExport* pSVGExport ) const SAL_OVERRIDE
    {
        VariableTextField::elementExport( pSVGExport );
        OUString sDateFormat, sTimeFormat;
        SvxDateFormat eDateFormat = (SvxDateFormat)( format & 0x0f );
        if( eDateFormat )
        {
            switch( eDateFormat )
            {
                case SVXDATEFORMAT_STDSMALL:
                case SVXDATEFORMAT_A:       // 13.02.96
                    sDateFormat = "";
                    break;
                case SVXDATEFORMAT_C:       // 13.Feb 1996
                    sDateFormat = "";
                    break;
                case SVXDATEFORMAT_D:       // 13.February 1996
                    sDateFormat = "";
                    break;
                case SVXDATEFORMAT_E:       // Tue, 13.February 1996
                    sDateFormat = "";
                    break;
                case SVXDATEFORMAT_STDBIG:
                case SVXDATEFORMAT_F:       // Tuesday, 13.February 1996
                    sDateFormat = "";
                    break;
                // default case
                case SVXDATEFORMAT_B:      // 13.02.1996
                default:
                    sDateFormat = "";
                    break;
            }
        }

        SvxTimeFormat eTimeFormat = (SvxTimeFormat)( ( format >> 4 ) & 0x0f );
        if( eTimeFormat )
        {
            switch( eTimeFormat )
            {
                case SVXTIMEFORMAT_24_HMS:      // 13:49:38
                    sTimeFormat = "";
                    break;
                case SVXTIMEFORMAT_AM_HM:      // 01:49 PM
                case SVXTIMEFORMAT_12_HM:
                    sTimeFormat = "";
                    break;
                case SVXTIMEFORMAT_AM_HMS:     // 01:49:38 PM
                case SVXTIMEFORMAT_12_HMS:
                    sTimeFormat = "";
                    break;
                // default case
                case SVXTIMEFORMAT_24_HM:     // 13:49
                default:
                    sTimeFormat = "";
                    break;
            }
        }

        OUString sDateTimeFormat = sDateFormat + " " + sTimeFormat;

        pSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrDateTimeFormat, sDateTimeFormat );
        SvXMLElementExport aExp( *pSVGExport, XML_NAMESPACE_NONE, "g", true, true );
    }
    virtual void growCharSet( SVGFilter::UCharSetMapMap & aTextFieldCharSets ) const SAL_OVERRIDE
    {
        // we use the unicode char set in an improper way: we put in the date/time fortat
        // in order to pass it to the CalcFieldValue method
        static const OUString sFieldId = aOOOAttrDateTimeField + "-variable";
        SVGFilter::ObjectSet::const_iterator aMasterPageIt = mMasterPageSet.begin();
        for( ; aMasterPageIt != mMasterPageSet.end(); ++aMasterPageIt )
        {
            aTextFieldCharSets[ *aMasterPageIt ][ sFieldId ].insert( (sal_Unicode)( format ) );
        }
    }
    virtual ~VariableDateTimeField() {}
};

bool operator==( const TextField & aLhsTextField, const TextField & aRhsTextField )
{
    return aLhsTextField.equalTo( aRhsTextField );
}




// - SVGExport -


SVGExport::SVGExport(
    const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XComponentContext > xContext,
    const Reference< XDocumentHandler >& rxHandler,
    const Sequence< PropertyValue >& rFilterData )
    : SvXMLExport( util::MeasureUnit::MM_100TH,
                   xContext, "",
                   xmloff::token::XML_TOKEN_INVALID,
                   EXPORT_META|EXPORT_PRETTY )
{
    SetDocHandler( rxHandler );
    GetDocHandler()->startDocument();

    // initializing filter settings from filter data
    comphelper::SequenceAsHashMap aFilterDataHashMap = rFilterData;

    // TinyProfile
    mbIsUseTinyProfile = aFilterDataHashMap.getUnpackedValueOrDefault(SVG_PROP_TINYPROFILE, sal_True);

    // Font Embedding
    comphelper::SequenceAsHashMap::const_iterator iter = aFilterDataHashMap.find(SVG_PROP_EMBEDFONTS);
    if(iter==aFilterDataHashMap.end())
    {
        const char* pSVGDisableFontEmbedding = getenv( "SVG_DISABLE_FONT_EMBEDDING" );
        OUString aEmbedFontEnv("${SVG_DISABLE_FONT_EMBEDDING}");
        rtl::Bootstrap::expandMacros(aEmbedFontEnv);
        mbIsEmbedFonts=pSVGDisableFontEmbedding ? sal_False : (
            aEmbedFontEnv.getLength() ? sal_False : sal_True);
    }
    else
    {
        if(!(iter->second >>= mbIsEmbedFonts))
            mbIsEmbedFonts = false;
    }

    // Native Decoration
    mbIsUseNativeTextDecoration = mbIsUseTinyProfile ? sal_False : aFilterDataHashMap.getUnpackedValueOrDefault(SVG_PROP_NATIVEDECORATION, sal_False);

    // Tiny Opacity (supported from SVG Tiny 1.2)
    mbIsUseOpacity = aFilterDataHashMap.getUnpackedValueOrDefault(SVG_PROP_OPACITY, sal_True);

    // Positioned Characters    (Seems to be experimental, as it was always initialized with false)
    mbIsUsePositionedCharacters = aFilterDataHashMap.getUnpackedValueOrDefault(SVG_PROP_POSITIONED_CHARACTERS, sal_False);

}



SVGExport::~SVGExport()
{
    GetDocHandler()->endDocument();
}



// - ObjectRepresentation -


ObjectRepresentation::ObjectRepresentation() :
    mpMtf( NULL )
{
}



ObjectRepresentation::ObjectRepresentation( const Reference< XInterface >& rxObject,
                                            const GDIMetaFile& rMtf ) :
    mxObject( rxObject ),
    mpMtf( new GDIMetaFile( rMtf ) )
{
}



ObjectRepresentation::ObjectRepresentation( const ObjectRepresentation& rPresentation ) :
    mxObject( rPresentation.mxObject ),
    mpMtf( rPresentation.mpMtf ? new GDIMetaFile( *rPresentation.mpMtf ) : NULL )
{
}



ObjectRepresentation::~ObjectRepresentation()
{
    delete mpMtf;
}



ObjectRepresentation& ObjectRepresentation::operator=( const ObjectRepresentation& rPresentation )
{
    // Check for self-assignment
    if (this == &rPresentation)
        return *this;
    mxObject = rPresentation.mxObject;
    delete mpMtf, ( mpMtf = rPresentation.mpMtf ? new GDIMetaFile( *rPresentation.mpMtf ) : NULL );

    return *this;
}



bool ObjectRepresentation::operator==( const ObjectRepresentation& rPresentation ) const
{
    return( ( mxObject == rPresentation.mxObject ) &&
            ( *mpMtf == *rPresentation.mpMtf ) );
}



sal_uLong GetBitmapChecksum( const MetaAction* pAction )
{
    sal_uLong nChecksum = 0;
    const sal_uInt16 nType = pAction->GetType();

    switch( nType )
    {
        case( META_BMPSCALE_ACTION ):
        {
            const MetaBmpScaleAction* pA = (const MetaBmpScaleAction*) pAction;
            if( pA  )
                nChecksum = pA->GetBitmap().GetChecksum();
            else
                OSL_FAIL( "GetBitmapChecksum: MetaBmpScaleAction pointer is null." );
        }
        break;
        case( META_BMPEXSCALE_ACTION ):
        {
            const MetaBmpExScaleAction* pA = (const MetaBmpExScaleAction*) pAction;
            if( pA )
                nChecksum = pA->GetBitmapEx().GetChecksum();
            else
                OSL_FAIL( "GetBitmapChecksum: MetaBmpExScaleAction pointer is null." );
        }
        break;
    }
    return nChecksum;
}


void MetaBitmapActionGetPoint( const MetaAction* pAction, Point& rPt )
{
    const sal_uInt16 nType = pAction->GetType();
    switch( nType )
    {
        case( META_BMPSCALE_ACTION ):
        {
            const MetaBmpScaleAction* pA = (const MetaBmpScaleAction*) pAction;
            if( pA  )
                rPt = pA->GetPoint();
            else
                OSL_FAIL( "MetaBitmapActionGetPoint: MetaBmpScaleAction pointer is null." );
        }
        break;
        case( META_BMPEXSCALE_ACTION ):
        {
            const MetaBmpExScaleAction* pA = (const MetaBmpExScaleAction*) pAction;
            if( pA )
                rPt = pA->GetPoint();
            else
                OSL_FAIL( "MetaBitmapActionGetPoint: MetaBmpExScaleAction pointer is null." );
        }
        break;
    }

}



size_t HashBitmap::operator()( const ObjectRepresentation& rObjRep ) const
{
    const GDIMetaFile& aMtf = rObjRep.GetRepresentation();
    if( aMtf.GetActionSize() == 1 )
    {
        return static_cast< size_t >( GetBitmapChecksum( aMtf.GetAction( 0 ) ) );
    }
    else
    {
        OSL_FAIL( "HashBitmap: metafile should have a single action." );
        return 0;
    }
}



bool EqualityBitmap::operator()( const ObjectRepresentation& rObjRep1,
                                     const ObjectRepresentation& rObjRep2 ) const
{
    const GDIMetaFile& aMtf1 = rObjRep1.GetRepresentation();
    const GDIMetaFile& aMtf2 = rObjRep2.GetRepresentation();
    if( aMtf1.GetActionSize() == 1 && aMtf2.GetActionSize() == 1 )
    {
        sal_uLong nChecksum1 = GetBitmapChecksum( aMtf1.GetAction( 0 ) );
        sal_uLong nChecksum2 = GetBitmapChecksum( aMtf2.GetAction( 0 ) );
        return ( nChecksum1 == nChecksum2 );
    }
    else
    {
        OSL_FAIL( "EqualityBitmap: metafile should have a single action." );
        return false;
    }
}



// - SVGFilter -


bool SVGFilter::implExport( const Sequence< PropertyValue >& rDescriptor )
    throw (RuntimeException)
{
    Reference< XComponentContext >      xContext( ::comphelper::getProcessComponentContext() ) ;
    Reference< XOutputStream >          xOStm;
    boost::scoped_ptr<SvStream>         pOStm;
    sal_Int32                            nLength = rDescriptor.getLength();
    const PropertyValue*                pValue = rDescriptor.getConstArray();
    bool                            bRet = false;

    maFilterData.realloc( 0 );

    for ( sal_Int32 i = 0 ; i < nLength; ++i)
    {
        if ( pValue[ i ].Name == "OutputStream" )
            pValue[ i ].Value >>= xOStm;
        else if ( pValue[ i ].Name == "FileName" )
        {
            OUString aFileName;

            pValue[ i ].Value >>= aFileName;
            pOStm.reset(::utl::UcbStreamHelper::CreateStream( aFileName, STREAM_WRITE | STREAM_TRUNC ));

            if( pOStm )
                xOStm = Reference< XOutputStream >( new ::utl::OOutputStreamWrapper ( *pOStm ) );
        }
        else if ( pValue[ i ].Name == "FilterData" )
        {
            pValue[ i ].Value >>= maFilterData;
        }
    }

    if( xOStm.is() )
    {
        if( mSelectedPages.hasElements() && mMasterPageTargets.hasElements() )
        {
            Reference< XDocumentHandler > xDocHandler( implCreateExportDocumentHandler( xOStm ), UNO_QUERY );

            if( xDocHandler.is() )
            {
                mbPresentation = Reference< XPresentationSupplier >( mxSrcDoc, UNO_QUERY ).is();
                mpObjects = new ObjectMap;

                // #110680#
                // mpSVGExport = new SVGExport( xDocHandler );
                mpSVGExport = new SVGExport( xContext, xDocHandler, maFilterData );

                if( mpSVGExport != NULL )
                {
                    // xSVGExport is set up only to manage the life-time of the object pointed by mpSVGExport,
                    // and in order to prevent that it is destroyed when passed to AnimationExporter.
                    Reference< XInterface > xSVGExport = static_cast< ::com::sun::star::document::XFilter* >( mpSVGExport );

                    // create an id for each draw page
                    for( sal_Int32 i = 0; i < mSelectedPages.getLength(); ++i )
                        implRegisterInterface( mSelectedPages[i] );

                    // create an id for each master page
                    for( sal_Int32 i = 0; i < mMasterPageTargets.getLength(); ++i )
                        implRegisterInterface( mMasterPageTargets[i] );

                    try
                    {
                        mxDefaultPage = mSelectedPages[0];

                        if( mxDefaultPage.is() )
                        {
                            SvxDrawPage* pSvxDrawPage = SvxDrawPage::getImplementation( mxDefaultPage );

                            if( pSvxDrawPage )
                            {
                                mpDefaultSdrPage = pSvxDrawPage->GetSdrPage();
                                mpSdrModel = mpDefaultSdrPage->GetModel();

                                if( mpSdrModel )
                                {
                                    SdrOutliner& rOutl = mpSdrModel->GetDrawOutliner(NULL);

                                    maOldFieldHdl = rOutl.GetCalcFieldValueHdl();
                                    rOutl.SetCalcFieldValueHdl( LINK( this, SVGFilter, CalcFieldHdl) );
                                }
                            }
                            bRet = implExportDocument();
                        }
                    }
                    catch( ... )
                    {
                        delete mpSVGDoc, mpSVGDoc = NULL;
                        OSL_FAIL( "Exception caught" );
                    }

                    if( mpSdrModel )
                        mpSdrModel->GetDrawOutliner( NULL ).SetCalcFieldValueHdl( maOldFieldHdl );

                    delete mpSVGWriter, mpSVGWriter = NULL;
                    mpSVGExport = NULL; // pointed object is released by xSVGExport dtor at the end of this scope
                    delete mpSVGFontExport, mpSVGFontExport = NULL;
                    delete mpObjects, mpObjects = NULL;
                    mbPresentation = false;
                }
            }
        }
    }

    return bRet;
}



Reference< XWriter > SVGFilter::implCreateExportDocumentHandler( const Reference< XOutputStream >& rxOStm )
{
    Reference< XWriter >       xSaxWriter;

    if( rxOStm.is() )
    {
        xSaxWriter = Writer::create( ::comphelper::getProcessComponentContext() );
        xSaxWriter->setOutputStream( rxOStm );
    }

    return xSaxWriter;
}



bool SVGFilter::implLookForFirstVisiblePage()
{
    sal_Int32 nCurPage = 0, nLastPage = mSelectedPages.getLength() - 1;

    while( ( nCurPage <= nLastPage ) && ( -1 == mnVisiblePage ) )
    {
        const Reference< XDrawPage > & xDrawPage = mSelectedPages[nCurPage];

        if( xDrawPage.is() )
        {
            Reference< XPropertySet > xPropSet( xDrawPage, UNO_QUERY );

            if( xPropSet.is() )
            {
                bool bVisible = false;

                if( !mbPresentation || mbSinglePage ||
                    ( ( xPropSet->getPropertyValue( "Visible" ) >>= bVisible ) && bVisible ) )
                {
                    mnVisiblePage = nCurPage;
                }
            }
        }
        ++nCurPage;
    }

    return ( mnVisiblePage != -1 );
}


bool SVGFilter::implExportDocument()
{
    OUString         aAttr;
    sal_Int32        nDocX = 0, nDocY = 0; // #i124608#
    sal_Int32        nDocWidth = 0, nDocHeight = 0;
    bool         bRet = false;
    sal_Int32        nLastPage = mSelectedPages.getLength() - 1;

    SvtMiscOptions aMiscOptions;
    const bool bExperimentalMode = aMiscOptions.IsExperimentalMode();

    mbSinglePage = (nLastPage == 0) || !bExperimentalMode;
    mnVisiblePage = -1;

    const Reference< XPropertySet >             xDefaultPagePropertySet( mxDefaultPage, UNO_QUERY );
    const Reference< XExtendedDocumentHandler > xExtDocHandler( mpSVGExport->GetDocHandler(), UNO_QUERY );

    // #i124608#
    mbExportSelection = mbSinglePage && maShapeSelection.is() && maShapeSelection->getCount();

    if(xDefaultPagePropertySet.is())
    {
        xDefaultPagePropertySet->getPropertyValue( "Width" ) >>= nDocWidth;
        xDefaultPagePropertySet->getPropertyValue( "Height" ) >>= nDocHeight;
    }

    if(mbExportSelection)
    {
        // #i124608# create BoundRange and set nDocX, nDocY, nDocWidth and nDocHeight
        basegfx::B2DRange aShapeRange;

        uno::Reference< XPrimitiveFactory2D > xPrimitiveFactory = PrimitiveFactory2D::create( mxContext );

        // use XPrimitiveFactory2D and go the way over getting the primitives; this
        // will give better precision (doubles) and be based on the true object
        // geometry. If needed aViewInformation may be expanded to carry a view
        // resolution for which to prepare the geometry.
        if(xPrimitiveFactory.is())
        {
            Reference< XShape > xShapeCandidate;
            const Sequence< PropertyValue > aViewInformation;
            const Sequence< PropertyValue > aParams;

            for(sal_Int32 a(0); a < maShapeSelection->getCount(); a++)
            {
                if((maShapeSelection->getByIndex(a) >>= xShapeCandidate) && xShapeCandidate.is())
                {
                    const Sequence< Reference< XPrimitive2D > > aPrimitiveSequence(
                        xPrimitiveFactory->createPrimitivesFromXShape( xShapeCandidate, aParams ));
                    const sal_Int32 nCount(aPrimitiveSequence.getLength());

                    for(sal_Int32 nIndex = 0; nIndex < nCount; nIndex++)
                    {
                        const RealRectangle2D aRect(aPrimitiveSequence[nIndex]->getRange(aViewInformation));

                        aShapeRange.expand(basegfx::B2DTuple(aRect.X1, aRect.Y1));
                        aShapeRange.expand(basegfx::B2DTuple(aRect.X2, aRect.Y2));
                    }
                }
            }
        }

        if(!aShapeRange.isEmpty())
        {
            nDocX = basegfx::fround(aShapeRange.getMinX());
            nDocY = basegfx::fround(aShapeRange.getMinY());
            nDocWidth  = basegfx::fround(aShapeRange.getWidth());
            nDocHeight = basegfx::fround(aShapeRange.getHeight());
        }
    }

    if( xExtDocHandler.is() && !mpSVGExport->IsUseTinyProfile() )
    {
        xExtDocHandler->unknown( SVG_DTD_STRING );
    }

    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "version", "1.2" );

    if( mpSVGExport->IsUseTinyProfile() )
         mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "baseProfile", "tiny" );

    // enabling _SVG_WRITE_EXTENTS means that the slide size is not adapted
    // to the size of the browser window, moreover the slide is top left aligned
    // instead of centered.
    #define _SVG_WRITE_EXTENTS
    #ifdef _SVG_WRITE_EXTENTS
    if( mbSinglePage )
    {
        aAttr = OUString::number( nDocWidth * 0.01 ) + "mm";
        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "width", aAttr );

        aAttr = OUString::number( nDocHeight * 0.01 ) + "mm";
        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "height", aAttr );
    }
    #endif

    // #i124608# set viewBox explicitely to the exported content
    if (mbExportSelection)
    {
        aAttr = OUString::number(nDocX) + " " + OUString::number(nDocY) + " ";
    }
    else
    {
        aAttr = "0 0 ";
    }

    aAttr += OUString::number(nDocWidth) + " " + OUString::number(nDocHeight);

    msClipPathId = "presentation_clip_path";
    OUString sClipPathAttrValue = "url(#" + msClipPathId + ")";

    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "viewBox", aAttr );
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "preserveAspectRatio", "xMidYMid" );
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "fill-rule", "evenodd" );
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "clip-path", sClipPathAttrValue );

    // standard line width is based on 1 pixel on a 90 DPI device (0.28222mmm)
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "stroke-width", OUString::number( 28.222 ) );
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "stroke-linejoin", "round" );
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "xmlns", constSvgNamespace );
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "xmlns:ooo", "http://xml.openoffice.org/svg/export" );
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "xmlns:xlink", "http://www.w3.org/1999/xlink" );
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "xml:space", "preserve" );

    mpSVGDoc = new SvXMLElementExport( *mpSVGExport, XML_NAMESPACE_NONE, "svg", true, true );

    // Create a ClipPath element that will be used for cutting bitmaps and other elements that could exceed the page margins.
    {
        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", "ClipPathGroup" );
        SvXMLElementExport aDefsElem( *mpSVGExport, XML_NAMESPACE_NONE, "defs", true, true );
        {
            mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", msClipPathId );
            mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "clipPathUnits", "userSpaceOnUse" );
            SvXMLElementExport aClipPathElem( *mpSVGExport, XML_NAMESPACE_NONE, "clipPath", true, true );
            {
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "x", OUString::number( 0 ) );
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "y", OUString::number( 0 ) );
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "width", OUString::number( nDocWidth ) );
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "height", OUString::number( nDocHeight ) );
                SvXMLElementExport aRectElem( *mpSVGExport, XML_NAMESPACE_NONE, "rect", true, true );
            }
        }
    }

    if( implLookForFirstVisiblePage() )  // OK! We found at least one visible page.
    {
        if( !mbSinglePage )
        {
            implGenerateMetaData();
            if( bExperimentalMode )
                implExportAnimations();
        }
        else
        {
            implGetPagePropSet( mSelectedPages[0] );
        }

        // Create the (Shape, GDIMetaFile) map
        if( implCreateObjects() )
        {
            ObjectMap::const_iterator                aIter( mpObjects->begin() );
            ::std::vector< ObjectRepresentation >    aObjects( mpObjects->size() );
            sal_uInt32                               nPos = 0;

            while( aIter != mpObjects->end() )
            {
                aObjects[ nPos++ ] = (*aIter).second;
                ++aIter;
            }

            mpSVGFontExport = new SVGFontExport( *mpSVGExport, aObjects );
            mpSVGWriter = new SVGActionWriter( *mpSVGExport, *mpSVGFontExport );

            if( mpSVGExport->IsEmbedFonts() )
            {
                mpSVGFontExport->EmbedFonts();
            }
            if( !mpSVGExport->IsUsePositionedCharacters() )
            {
                implExportTextShapeIndex();
                implEmbedBulletGlyphs();
                implExportTextEmbeddedBitmaps();
            }

            // #i124608# export a given object selection, so no MasterPage export at all
            if (!mbExportSelection)
                implExportMasterPages( mMasterPageTargets, 0, mMasterPageTargets.getLength() - 1 );
            implExportDrawPages( mSelectedPages, 0, nLastPage );

            if( !mbSinglePage )
            {
                implGenerateScript();
            }

            delete mpSVGDoc, mpSVGDoc = NULL;
            bRet = true;
        }
    }

    return bRet;
}



// Append aField to aFieldSet if it is not already present in the set
// and create the field id sFieldId


template< typename TextFieldType >
OUString implGenerateFieldId( std::vector< TextField* > & aFieldSet,
                              const TextFieldType & aField,
                              const OUString & sOOOElemField,
                              Reference< XDrawPage > xMasterPage )
{
    bool bFound = false;
    sal_Int32 i;
    sal_Int32 nSize = aFieldSet.size();
    for( i = 0; i < nSize; ++i )
    {
        if( *(aFieldSet[i]) == aField )
        {
            bFound = true;
            break;
        }
    }
    OUString sFieldId( sOOOElemField );
    sFieldId += OUString( '_' );
    if( !bFound )
    {
        aFieldSet.push_back( new TextFieldType( aField ) );
    }
    aFieldSet[i]->insertMasterPage( xMasterPage );
    sFieldId += OUString::number( i );
    return sFieldId;
}



bool SVGFilter::implGenerateMetaData()
{
    bool bRet = false;
    sal_Int32 nCount = mSelectedPages.getLength();
    if( nCount != 0 )
    {
        // we wrap all meta presentation info into a svg:defs element
        SvXMLElementExport aDefsElem( *mpSVGExport, XML_NAMESPACE_NONE, "defs", true, true );

        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", aOOOElemMetaSlides );
        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrNumberOfSlides, OUString::number( nCount ) );
        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrStartSlideNumber, OUString::number( mnVisiblePage ) );

        /*
         *  Add a (global) Page Numbering Type attribute for the document
         */
        // NOTE:
        // at present pSdrModel->GetPageNumType() returns always SVX_ARABIC
        // so the following code fragment is pretty unuseful
        sal_Int32 nPageNumberingType =  SVX_ARABIC;
        SvxDrawPage* pSvxDrawPage = SvxDrawPage::getImplementation( mSelectedPages[0] );
        if( pSvxDrawPage )
        {
            SdrPage* pSdrPage = pSvxDrawPage->GetSdrPage();
            SdrModel* pSdrModel = pSdrPage->GetModel();
            nPageNumberingType = pSdrModel->GetPageNumType();

            // That is used by CalcFieldHdl method.
            mVisiblePagePropSet.nPageNumberingType = nPageNumberingType;
        }
        if( nPageNumberingType != SVX_NUMBER_NONE )
        {
            OUString sNumberingType;
            switch( nPageNumberingType )
            {
                case SVX_CHARS_UPPER_LETTER:
                    sNumberingType = "alpha-upper";
                    break;
                case SVX_CHARS_LOWER_LETTER:
                    sNumberingType = "alpha-lower";
                    break;
                case SVX_ROMAN_UPPER:
                    sNumberingType = "roman-upper";
                    break;
                case SVX_ROMAN_LOWER:
                    sNumberingType = "roman-lower";
                    break;
                // arabic numbering type is the default, so we do not append any attribute for it
                case SVX_ARABIC:
                // in case the numbering type is not handled we fall back on arabic numbering
                default:
                    break;
            }
            if( !sNumberingType.isEmpty() )
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrNumberingType, sNumberingType );
        }


        {
            SvXMLElementExport    aExp( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );
            const OUString                aId( aOOOElemMetaSlide );
            const OUString                aElemTextFieldId( aOOOElemTextField );
            std::vector< TextField* >     aFieldSet;

            for( sal_Int32 i = 0; i < nCount; ++i )
            {
                const Reference< XDrawPage > &    xDrawPage = mSelectedPages[i];
                Reference< XMasterPageTarget >    xMasterPageTarget( xDrawPage, UNO_QUERY );
                Reference< XDrawPage >            xMasterPage( xMasterPageTarget->getMasterPage(), UNO_QUERY );
                OUString                          aSlideId( aId );
                aSlideId += OUString( '_' );
                aSlideId += OUString::number( i );

                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", aSlideId );
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrSlide, implGetValidIDFromInterface( xDrawPage ) );
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrMaster, implGetValidIDFromInterface( xMasterPage ) );


                if( mbPresentation )
                {
                    Reference< XPropertySet > xPropSet( xDrawPage, UNO_QUERY );

                    if( xPropSet.is() )
                    {
                        bool bBackgroundVisibility                = true;     // default: visible
                        bool bBackgroundObjectsVisibility         = true;     // default: visible
                        bool bPageNumberVisibility                = false;    // default: hidden
                        bool bDateTimeVisibility                  = true;     // default: visible
                        bool bFooterVisibility                    = true;     // default: visible
                        bool bDateTimeFixed                       = true;     // default: fixed

                        FixedDateTimeField            aFixedDateTimeField;
                        VariableDateTimeField         aVariableDateTimeField;
                        FooterField                   aFooterField;

                        xPropSet->getPropertyValue( "IsBackgroundVisible" )  >>= bBackgroundVisibility;
                        // in case the attribute is set to its default value it is not appended to the meta-slide element
                        if( !bBackgroundVisibility ) // visibility default value: 'visible'
                            mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrBackgroundVisibility, "hidden" );

                        // Page Number, Date/Time, Footer and Header Fields are regarded as background objects.
                        // So bBackgroundObjectsVisibility overrides visibility of master page text fields.
                        xPropSet->getPropertyValue( "IsBackgroundObjectsVisible" )  >>= bBackgroundObjectsVisibility;
                        if( bBackgroundObjectsVisibility ) // visibility default value: 'visible'
                        {
                            /*
                             *  Page Number Field
                             */
                            xPropSet->getPropertyValue( "IsPageNumberVisible" )  >>= bPageNumberVisibility;
                            bPageNumberVisibility = bPageNumberVisibility && ( nPageNumberingType != SVX_NUMBER_NONE );
                            if( bPageNumberVisibility ) // visibility default value: 'hidden'
                            {
                                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrPageNumberVisibility, "visible" );
                            }
                            /*
                             *  Date/Time Field
                             */
                            xPropSet->getPropertyValue( "IsDateTimeVisible" ) >>= bDateTimeVisibility;
                            if( bDateTimeVisibility ) // visibility default value: 'visible'
                            {
                                xPropSet->getPropertyValue( "IsDateTimeFixed" ) >>= bDateTimeFixed;
                                if( bDateTimeFixed ) // we are interested only in the field text not in the date/time format
                                {
                                    xPropSet->getPropertyValue( "DateTimeText" ) >>= aFixedDateTimeField.text;
                                    if( !aFixedDateTimeField.text.isEmpty() )
                                    {
                                        OUString sFieldId = implGenerateFieldId( aFieldSet, aFixedDateTimeField, aElemTextFieldId, xMasterPage );
                                        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrDateTimeField, sFieldId );
                                    }
                                }
                                else // the inverse applies: we are interested only in the date/time format not in the field text
                                {
                                    xPropSet->getPropertyValue( "DateTimeFormat" ) >>= aVariableDateTimeField.format;
                                    OUString sFieldId = implGenerateFieldId( aFieldSet, aVariableDateTimeField, aElemTextFieldId, xMasterPage );
                                    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrDateTimeField, sFieldId );
                                }
                            }
                            else
                            {
                                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrDateTimeVisibility, "hidden" );
                            }
                            /*
                             *  Footer Field
                             */
                            xPropSet->getPropertyValue( "IsFooterVisible" )  >>= bFooterVisibility;
                            if( bFooterVisibility ) // visibility default value: 'visible'
                            {
                                xPropSet->getPropertyValue( "FooterText" ) >>= aFooterField.text;
                                if( !aFooterField.text.isEmpty() )
                                {
                                    OUString sFieldId = implGenerateFieldId( aFieldSet, aFooterField, aElemTextFieldId, xMasterPage );
                                    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrFooterField, sFieldId );
                                }
                            }
                            else
                            {
                                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrFooterVisibility, "hidden" );
                            }
                        }
                        else
                        {
                            mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrMasterObjectsVisibility, "hidden" );
                        }

                        // We look for a slide transition.
                        // Transition properties are exported together with animations.
                        sal_Int16 nTransitionType(0);
                        if( xPropSet->getPropertySetInfo()->hasPropertyByName( "TransitionType" ) &&
                            (xPropSet->getPropertyValue( "TransitionType" ) >>= nTransitionType) )
                        {
                            sal_Int16 nTransitionSubType(0);
                            if( xPropSet->getPropertyValue( "TransitionSubtype" )  >>= nTransitionSubType )
                            {
                                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrHasTransition, "true" );
                            }
                        }

                    }
                }

                {
                    SvXMLElementExport aExp2( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );
                }  // when the aExp2 destructor is called the </g> tag is appended to the output file
            }

            // export text field elements
            if( mbPresentation )
            {
                for( sal_Int32 i = 0, nSize = aFieldSet.size(); i < nSize; ++i )
                {
                    OUString sElemId = OUString(aOOOElemTextField) + "_" + OUString::number( i );
                    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", sElemId );
                    aFieldSet[i]->elementExport( mpSVGExport );
                }
                if( mpSVGExport->IsEmbedFonts() && mpSVGExport->IsUsePositionedCharacters() )
                {
                    for( sal_Int32 i = 0, nSize = aFieldSet.size(); i < nSize; ++i )
                    {
                        aFieldSet[i]->growCharSet( mTextFieldCharSets );
                    }
                }
            }
            // text fields are used only for generating meta info so we don't need them anymore
            for( sal_uInt32 i = 0; i < aFieldSet.size(); ++i )
            {
                if( aFieldSet[i] != NULL )
                {
                    delete aFieldSet[i];
                }
            }
        }
        bRet = true;
    }

    return bRet;
}



bool SVGFilter::implExportAnimations()
{
    bool bRet = false;

    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", "presentation-animations" );
    SvXMLElementExport aDefsContainerElem( *mpSVGExport, XML_NAMESPACE_NONE, "defs", true, true );

    for( sal_Int32 i = 0; i < mSelectedPages.getLength(); ++i )
    {
        Reference< XPropertySet > xProps( mSelectedPages[i], UNO_QUERY );

        if( xProps.is() && xProps->getPropertySetInfo()->hasPropertyByName( "TransitionType" ) )
        {
            sal_Int16 nTransition = 0;
            xProps->getPropertyValue( "TransitionType" )  >>= nTransition;
            // we have a slide transition ?
            bool bHasEffects = ( nTransition != 0 );

            Reference< XAnimationNodeSupplier > xAnimNodeSupplier( mSelectedPages[i], UNO_QUERY );
            if( xAnimNodeSupplier.is() )
            {
                Reference< XAnimationNode > xRootNode = xAnimNodeSupplier->getAnimationNode();
                if( xRootNode.is() )
                {
                    if( !bHasEffects )
                    {
                        // first check if there are no animations
                        Reference< XEnumerationAccess > xEnumerationAccess( xRootNode, UNO_QUERY_THROW );
                        Reference< XEnumeration > xEnumeration( xEnumerationAccess->createEnumeration(), UNO_QUERY_THROW );
                        if( xEnumeration->hasMoreElements() )
                        {
                            // first child node may be an empty main sequence, check this
                            Reference< XAnimationNode > xMainNode( xEnumeration->nextElement(), UNO_QUERY_THROW );
                            Reference< XEnumerationAccess > xMainEnumerationAccess( xMainNode, UNO_QUERY_THROW );
                            Reference< XEnumeration > xMainEnumeration( xMainEnumerationAccess->createEnumeration(), UNO_QUERY_THROW );

                            // only export if the main sequence is not empty or if there are additional
                            // trigger sequences
                            bHasEffects = xMainEnumeration->hasMoreElements() || xEnumeration->hasMoreElements();
                        }
                    }
                    if( bHasEffects )
                    {
                        OUString sId = implGetValidIDFromInterface( mSelectedPages[i] );
                        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrSlide, sId  );
                        sId += "-animations";
                        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", sId  );
                        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", "Animations" );
                        SvXMLElementExport aDefsElem( *mpSVGExport, XML_NAMESPACE_NONE, "defs", true, true );

                        rtl::Reference< xmloff::AnimationsExporter > xAnimationsExporter;
                        xAnimationsExporter = new xmloff::AnimationsExporter( *mpSVGExport, xProps );
                        xAnimationsExporter->prepare( xRootNode );
                        xAnimationsExporter->exportAnimations( xRootNode );
                    }
                }
            }
        }
    }

    bRet = true;
    return bRet;
}



void SVGFilter::implExportTextShapeIndex()
{
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", "TextShapeIndex" );
    SvXMLElementExport aDefsContainerElem( *mpSVGExport, XML_NAMESPACE_NONE, "defs", true, true );

    sal_Int32 nCount = mSelectedPages.getLength();
    for( sal_Int32 i = 0; i < nCount; ++i )
    {
        const Reference< XDrawPage > & xDrawPage = mSelectedPages[i];
        if( mTextShapeIdListMap.find( xDrawPage ) != mTextShapeIdListMap.end() )
        {
            OUString sTextShapeIdList = mTextShapeIdListMap[xDrawPage].trim();

            const OUString& rPageId = implGetValidIDFromInterface( Reference<XInterface>(xDrawPage, UNO_QUERY) );
            if( !rPageId.isEmpty() && !sTextShapeIdList.isEmpty() )
            {
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrSlide, rPageId  );
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrIdList, sTextShapeIdList );
                SvXMLElementExport aGElem( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );
            }
        }
    }
}



void SVGFilter::implEmbedBulletGlyphs()
{
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", "EmbeddedBulletChars" );
    SvXMLElementExport aDefsContainerElem( *mpSVGExport, XML_NAMESPACE_NONE, "defs", true, true );

    OUString sPathData = "M 580,1141 L 1163,571 580,0 -4,571 580,1141 Z";
    implEmbedBulletGlyph( 57356, sPathData );
    sPathData = "M 8,1128 L 1137,1128 1137,0 8,0 8,1128 Z";
    implEmbedBulletGlyph( 57354, sPathData );
    sPathData = "M 174,0 L 602,739 174,1481 1456,739 174,0 Z M 1358,739 L 309,1346 659,739 1358,739 Z";
    implEmbedBulletGlyph( 10146, sPathData );
    sPathData = "M 2015,739 L 1276,0 717,0 1260,543 174,543 174,936 1260,936 717,1481 1274,1481 2015,739 Z";
    implEmbedBulletGlyph( 10132, sPathData );
    sPathData = "M 0,-2 C -7,14 -16,27 -25,37 L 356,567 C 262,823 215,952 215,954 215,979 228,992 255,992 264,992 276,990 289,987 310,991 331,999 354,1012 L 381,999 492,748 772,1049 836,1024 860,1049 C 881,1039 901,1025 922,1006 886,937 835,863 770,784 769,783 710,716 594,584 L 774,223 C 774,196 753,168 711,139 L 727,119 C 717,90 699,76 672,76 641,76 570,178 457,381 L 164,-76 C 142,-110 111,-127 72,-127 30,-127 9,-110 8,-76 1,-67 -2,-52 -2,-32 -2,-23 -1,-13 0,-2 Z";
    implEmbedBulletGlyph( 10007, sPathData );
    sPathData = "M 285,-33 C 182,-33 111,30 74,156 52,228 41,333 41,471 41,549 55,616 82,672 116,743 169,778 240,778 293,778 328,747 346,684 L 369,508 C 377,444 397,411 428,410 L 1163,1116 C 1174,1127 1196,1133 1229,1133 1271,1133 1292,1118 1292,1087 L 1292,965 C 1292,929 1282,901 1262,881 L 442,47 C 390,-6 338,-33 285,-33 Z";
    implEmbedBulletGlyph( 10004, sPathData );
    sPathData = "M 813,0 C 632,0 489,54 383,161 276,268 223,411 223,592 223,773 276,916 383,1023 489,1130 632,1184 813,1184 992,1184 1136,1130 1245,1023 1353,916 1407,772 1407,592 1407,412 1353,268 1245,161 1136,54 992,0 813,0 Z";
    implEmbedBulletGlyph( 9679, sPathData );
    sPathData = "M 346,457 C 273,457 209,483 155,535 101,586 74,649 74,723 74,796 101,859 155,911 209,963 273,989 346,989 419,989 480,963 531,910 582,859 608,796 608,723 608,648 583,586 532,535 482,483 420,457 346,457 Z";
    implEmbedBulletGlyph( 8226, sPathData );
    sPathData = "M -4,459 L 1135,459 1135,606 -4,606 -4,459 Z";
    implEmbedBulletGlyph( 8211, sPathData );
}



void SVGFilter::implEmbedBulletGlyph( sal_Unicode cBullet, const OUString & sPathData )
{
    OUString sId = "bullet-char-template(" + OUString::number( (sal_Int32)cBullet ) + ")";
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", sId );

    double fFactor = 1.0 / 2048;
    OUString sFactor = OUString::number( fFactor );
    OUString sTransform = "scale(" + sFactor + ",-" + sFactor + ")";
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "transform", sTransform );

    SvXMLElementExport aGElem( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "d", sPathData );
    SvXMLElementExport aPathElem( *mpSVGExport, XML_NAMESPACE_NONE, "path", true, true );

}



/** SVGFilter::implExportTextEmbeddedBitmaps
 *  We export bitmaps embedded into text shapes, such as those used by list
 *  items with image style, only once in a specic <defs> element.
 */
bool SVGFilter::implExportTextEmbeddedBitmaps()
{
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", "TextEmbeddedBitmaps" );
    SvXMLElementExport aDefsContainerElem( *mpSVGExport, XML_NAMESPACE_NONE, "defs", true, true );

    OUString sId;

    MetaBitmapActionSet::const_iterator it = mEmbeddedBitmapActionSet.begin();
    MetaBitmapActionSet::const_iterator end = mEmbeddedBitmapActionSet.end();
    for( ; it != end; ++it)
    {
        const GDIMetaFile& aMtf = it->GetRepresentation();

        if( aMtf.GetActionSize() == 1 )
        {
            MetaAction* pAction = aMtf.GetAction( 0 );
            if( pAction )
            {
                sal_uLong nId = GetBitmapChecksum( pAction );
                sId = "bitmap(" + OUString::number( nId ) + ")";
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", sId );

                const Reference< XShape >& rxShape = (const Reference< XShape >&)( it->GetObject() );
                Reference< XPropertySet > xShapePropSet( rxShape, UNO_QUERY );
                ::com::sun::star::awt::Rectangle    aBoundRect;
                if( xShapePropSet.is() && ( xShapePropSet->getPropertyValue( "BoundRect" ) >>= aBoundRect ) )
                {
                    // Origin of the coordinate device must be (0,0).
                    const Point aTopLeft;
                    const Size  aSize( aBoundRect.Width, aBoundRect.Height );

                    Point aPt;
                    MetaBitmapActionGetPoint( pAction, aPt );
                    // The image must be exported with x, y attribute set to 0,
                    // on the contrary when referenced by a <use> element,
                    // specifying the wanted position, they will result
                    // misplaced.
                    pAction->Move( -aPt.X(), -aPt.Y() );
                    mpSVGWriter->WriteMetaFile( aTopLeft, aSize, aMtf, 0xffffffff, NULL );
                    // We reset to the original values so that when the <use>
                    // element is created the x, y attributes are correct.
                    pAction->Move( aPt.X(), aPt.Y() );
                }
                else
                {
                    OSL_FAIL( "implExportTextEmbeddedBitmaps: no shape bounding box." );
                    return false;
                }
            }
            else
            {
                OSL_FAIL( "implExportTextEmbeddedBitmaps: metafile should have MetaBmpExScaleAction only." );
                return false;
            }
        }
        else
        {
            OSL_FAIL( "implExportTextEmbeddedBitmaps: metafile should have a single action." );
            return false;
        }

    }
    return true;
}



#define SVGFILTER_EXPORT_SVGSCRIPT( z, n, aFragment ) \
        xExtDocHandler->unknown( OUString::createFromAscii( aFragment ## n ) );

bool SVGFilter::implGenerateScript()
{
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "type", "text/ecmascript" );

    {
        SvXMLElementExport                       aExp( *mpSVGExport, XML_NAMESPACE_NONE, "script", true, true );
        Reference< XExtendedDocumentHandler >    xExtDocHandler( mpSVGExport->GetDocHandler(), UNO_QUERY );

        if( xExtDocHandler.is() )
        {
            BOOST_PP_REPEAT( N_SVGSCRIPT_FRAGMENTS, SVGFILTER_EXPORT_SVGSCRIPT, aSVGScript )
        }
    }

    return true;
}



Any SVGFilter::implSafeGetPagePropSet( const OUString & sPropertyName,
                                            const Reference< XPropertySet > & rxPropSet,
                                            const Reference< XPropertySetInfo > & rxPropSetInfo )
{
    Any result;
    if( rxPropSetInfo->hasPropertyByName( sPropertyName ) )
    {
        result = rxPropSet->getPropertyValue( sPropertyName );
    }
    return result;
}



/*  SVGFilter::implGetPagePropSet
 *
 *  We collect info on master page elements visibility,
 *  and placeholder text shape content.
 *  This method is used when exporting a single page
 *  as implGenerateMetaData is not invoked.
 */
bool SVGFilter::implGetPagePropSet( const Reference< XDrawPage > & rxPage )
{
    bool bRet = false;

    mVisiblePagePropSet.bIsBackgroundVisible                = true;
    mVisiblePagePropSet.bAreBackgroundObjectsVisible        = true;
    mVisiblePagePropSet.bIsPageNumberFieldVisible           = false;;
    mVisiblePagePropSet.bIsHeaderFieldVisible               = false;
    mVisiblePagePropSet.bIsFooterFieldVisible               = true;
    mVisiblePagePropSet.bIsDateTimeFieldVisible             = true;
    mVisiblePagePropSet.bIsDateTimeFieldFixed               = true;
    mVisiblePagePropSet.nDateTimeFormat                     = SVXDATEFORMAT_B;
    mVisiblePagePropSet.nPageNumberingType                  = SVX_ARABIC;

    /*  We collect info on master page elements visibility,
     *  and placeholder text shape content.
     */
    Reference< XPropertySet > xPropSet( rxPage, UNO_QUERY );
    if( xPropSet.is() )
    {
        Reference< XPropertySetInfo > xPropSetInfo( xPropSet->getPropertySetInfo() );
        if( xPropSetInfo.is() )
        {
            implSafeGetPagePropSet( "IsBackgroundVisible", xPropSet, xPropSetInfo )          >>= mVisiblePagePropSet.bIsBackgroundVisible;
            implSafeGetPagePropSet( "IsBackgroundObjectsVisible", xPropSet, xPropSetInfo )   >>= mVisiblePagePropSet.bAreBackgroundObjectsVisible;
            implSafeGetPagePropSet( "IsPageNumberVisible", xPropSet, xPropSetInfo )          >>= mVisiblePagePropSet.bIsPageNumberFieldVisible;
            implSafeGetPagePropSet( "IsHeaderVisible", xPropSet, xPropSetInfo )              >>= mVisiblePagePropSet.bIsHeaderFieldVisible;
            implSafeGetPagePropSet( "IsFooterVisible", xPropSet, xPropSetInfo )              >>= mVisiblePagePropSet.bIsFooterFieldVisible;
            implSafeGetPagePropSet( "IsDateTimeVisible", xPropSet, xPropSetInfo )            >>= mVisiblePagePropSet.bIsDateTimeFieldVisible;

            implSafeGetPagePropSet( "IsDateTimeFixed", xPropSet, xPropSetInfo )              >>= mVisiblePagePropSet.bIsDateTimeFieldFixed;
            implSafeGetPagePropSet( "DateTimeFormat", xPropSet, xPropSetInfo )               >>= mVisiblePagePropSet.nDateTimeFormat;
            implSafeGetPagePropSet( "Number", xPropSet, xPropSetInfo )                       >>= mVisiblePagePropSet.nPageNumber;
            implSafeGetPagePropSet( "DateTimeText", xPropSet, xPropSetInfo )                 >>= mVisiblePagePropSet.sDateTimeText;
            implSafeGetPagePropSet( "FooterText", xPropSet, xPropSetInfo )                   >>= mVisiblePagePropSet.sFooterText;
            implSafeGetPagePropSet( "HeaderText", xPropSet, xPropSetInfo )                   >>= mVisiblePagePropSet.sHeaderText;

            if( mVisiblePagePropSet.bIsPageNumberFieldVisible )
            {
                SvxDrawPage* pSvxDrawPage = SvxDrawPage::getImplementation( rxPage );
                if( pSvxDrawPage )
                {
                    SdrPage* pSdrPage = pSvxDrawPage->GetSdrPage();
                    SdrModel* pSdrModel = pSdrPage->GetModel();
                    mVisiblePagePropSet.nPageNumberingType = pSdrModel->GetPageNumType();
                }
            }

            bRet = true;
        }
    }

    return bRet;
}




bool SVGFilter::implExportMasterPages( const SVGFilter::XDrawPageSequence & rxPages,
                                           sal_Int32 nFirstPage, sal_Int32 nLastPage )
{
    DBG_ASSERT( nFirstPage <= nLastPage,
                "SVGFilter::implExportMasterPages: nFirstPage > nLastPage" );

    // When the exported slides are more than one we wrap master page elements
    // with a svg <defs> element.
    OUString aContainerTag = (mbSinglePage) ? OUString( "g" ) : OUString( "defs" );
    SvXMLElementExport aContainerElement( *mpSVGExport, XML_NAMESPACE_NONE, aContainerTag, true, true );

    bool bRet = false;
    for( sal_Int32 i = nFirstPage; i <= nLastPage; ++i )
    {
        if( rxPages[i].is() )
        {
            Reference< XShapes > xShapes( rxPages[i], UNO_QUERY );

            if( xShapes.is() )
            {
                // add id attribute
                const OUString & sPageId = implGetValidIDFromInterface( rxPages[i] );
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", sPageId );

                bRet = implExportPage( sPageId, rxPages[i], xShapes, true /* is a master page */ ) || bRet;
            }
        }
    }
    return bRet;
}



bool SVGFilter::implExportDrawPages( const SVGFilter::XDrawPageSequence & rxPages,
                                           sal_Int32 nFirstPage, sal_Int32 nLastPage )
{
    DBG_ASSERT( nFirstPage <= nLastPage,
                "SVGFilter::implExportDrawPages: nFirstPage > nLastPage" );

    // We wrap all slide in a group element with class name "SlideGroup".
    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", "SlideGroup" );
    SvXMLElementExport aExp( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

    bool bRet = false;
    for( sal_Int32 i = nFirstPage; i <= nLastPage; ++i )
    {
        Reference< XShapes > xShapes;

        if (mbExportSelection)
        {
            // #i124608# export a given object selection
            xShapes = maShapeSelection;
        }
        else
        {
            xShapes = Reference< XShapes >( rxPages[i], UNO_QUERY );
        }

        if( xShapes.is() )
        {
            // Insert the <g> open tag related to the svg element for
            // handling a slide visibility.
            // In case the exported slides are more than one the initial
            // visibility of each slide is set to 'hidden'.
            if( !mbSinglePage )
            {
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "visibility", "hidden" );
            }
            SvXMLElementExport aGElement( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

            {
                // add id attribute
                const OUString & sPageId = implGetValidIDFromInterface( rxPages[i] );
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", sPageId );

                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", "Slide" );

                // Adding a clip path to each exported slide , so in case
                // bitmaps or other elements exceed the slide margins, they are
                // trimmed, even when they are shown inside a thumbnail view.
                OUString sClipPathAttrValue = "url(#" + msClipPathId + ")";
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "clip-path", sClipPathAttrValue );

                SvXMLElementExport aSlideElement( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

                bRet = implExportPage( sPageId, rxPages[i], xShapes, false /* is not a master page */ ) || bRet;
            }
        } // append the </g> closing tag related to the svg element handling the slide visibility
    }

    return bRet;
}


bool SVGFilter::implExportPage( const OUString & sPageId,
                                    const Reference< XDrawPage > & rxPage,
                                    const Reference< XShapes > & xShapes,
                                    bool bMaster )
{
    bool bRet = false;

    {
        OUString sPageName = implGetInterfaceName( rxPage );
        if( !(sPageName.isEmpty() || mbSinglePage ))
            mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrName, sPageName );

        {
            Reference< XExtendedDocumentHandler > xExtDocHandler( mpSVGExport->GetDocHandler(), UNO_QUERY );

            if( xExtDocHandler.is() )
            {
                OUString aDesc;

                if( bMaster )
                    aDesc = "Master_Slide";
                else
                    aDesc = "Page";

                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", aDesc );
            }
        }

        // insert the <g> open tag related to the DrawPage/MasterPage
        SvXMLElementExport aExp( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

        // In case the page has a background object we append it .
        if( (mpObjects->find( rxPage ) != mpObjects->end()) )
        {
            const GDIMetaFile& rMtf = (*mpObjects)[ rxPage ].GetRepresentation();
            if( rMtf.GetActionSize() )
            {
                // background id = "bg-" + page id
                OUString sBackgroundId = "bg-";
                sBackgroundId += sPageId;
                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", sBackgroundId );

                // At present (LibreOffice 3.4.0) the 'IsBackgroundVisible' property is not handled
                // by Impress; anyway we handle this property as referring only to the visibility
                // of the master page background. So if a slide has its own background object,
                // the visibility of such a background object is always inherited from the visibility
                // of the parent slide regardless of the value of the 'IsBackgroundVisible' property.
                // This means that we need to set up the visibility attribute only for the background
                // element of a master page.
                if( mbSinglePage && bMaster )
                {
                    if( !mVisiblePagePropSet.bIsBackgroundVisible )
                    {
                        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "visibility", "hidden" );
                    }
                }

                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class",  "Background" );

                // insert the <g> open tag related to the Background
                SvXMLElementExport aExp2( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

                // append all elements that make up the Background
                const Point aNullPt;
                mpSVGWriter->WriteMetaFile( aNullPt, rMtf.GetPrefSize(), rMtf, SVGWRITER_WRITE_FILL );
            }   // insert the </g> closing tag related to the Background
        }

        // In case we are dealing with a master page we need to to group all its shapes
        // into a group element, this group will make up the so named "background objects"
        if( bMaster )
        {
            // background objects id = "bo-" + page id
            OUString sBackgroundObjectsId = "bo-";
            sBackgroundObjectsId += sPageId;
            mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", sBackgroundObjectsId );
            if( mbSinglePage )
            {
                if( !mVisiblePagePropSet.bAreBackgroundObjectsVisible )
                {
                    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "visibility", "hidden" );
                }
            }
            mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class",  "BackgroundObjects" );

            // insert the <g> open tag related to the Background Objects
            SvXMLElementExport aExp2( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

            // append all shapes that make up the Master Slide
            bRet = implExportShapes( xShapes, true ) || bRet;
        }   // append the </g> closing tag related to the Background Objects
        else
        {
            // append all shapes that make up the Slide
            bRet = implExportShapes( xShapes, false ) || bRet;
        }
    }  // append the </g> closing tag related to the Slide/Master_Slide

    return bRet;
}




bool SVGFilter::implExportShapes( const Reference< XShapes >& rxShapes,
                                      bool bMaster )
{
    Reference< XShape > xShape;
    bool            bRet = false;

    for( sal_Int32 i = 0, nCount = rxShapes->getCount(); i < nCount; ++i )
    {
        if( ( rxShapes->getByIndex( i ) >>= xShape ) && xShape.is() )
            bRet = implExportShape( xShape, bMaster ) || bRet;

        xShape = NULL;
    }

    return bRet;
}



bool SVGFilter::implExportShape( const Reference< XShape >& rxShape,
                                     bool bMaster )
{
    Reference< XPropertySet >   xShapePropSet( rxShape, UNO_QUERY );
    bool                    bRet = false;

    if( xShapePropSet.is() )
    {
        const OUString   aShapeType( rxShape->getShapeType() );
        bool                    bHideObj = false;

        if( mbPresentation )
        {
            xShapePropSet->getPropertyValue( "IsEmptyPresentationObject" )  >>= bHideObj;
        }

        OUString aShapeClass = implGetClassFromShape( rxShape );
        if( bMaster )
        {
            if( aShapeClass == "TitleText" || aShapeClass == "Outline" )
                bHideObj = true;
        }

        if( !bHideObj )
        {
            if( aShapeType.lastIndexOf( "drawing.GroupShape" ) != -1 )
            {
                Reference< XShapes > xShapes( rxShape, UNO_QUERY );

                if( xShapes.is() )
                {
                    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", "Group" );
                    SvXMLElementExport aExp( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

                    bRet = implExportShapes( xShapes, bMaster );
                }
            }

            if( !bRet && mpObjects->find( rxShape ) !=  mpObjects->end() )
            {
                const OUString*                    pElementId = NULL;

                ::com::sun::star::awt::Rectangle    aBoundRect;
                const GDIMetaFile&                  rMtf = (*mpObjects)[ rxShape ].GetRepresentation();

                xShapePropSet->getPropertyValue( "BoundRect" ) >>= aBoundRect;

                const Point aTopLeft( aBoundRect.X, aBoundRect.Y );
                const Size  aSize( aBoundRect.Width, aBoundRect.Height );

                if( rMtf.GetActionSize() )
                {   // for text field shapes we set up text-adjust attributes
                    // and set visibility to hidden
                    if( mbPresentation )
                    {
                        bool bIsPageNumber  = ( aShapeClass == "Slide_Number" );
                        bool bIsFooter      = ( aShapeClass == "Footer" );
                        bool bIsDateTime    = ( aShapeClass == "Date/Time" );
                        if( bIsPageNumber || bIsDateTime || bIsFooter )
                        {
                            if( !mbSinglePage )
                            {
                                // to notify to the SVGActionWriter::ImplWriteActions method
                                // that we are dealing with a placeholder shape
                                pElementId = &sPlaceholderTag;

                                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "visibility", "hidden" );

                                sal_uInt16 nTextAdjust = ParagraphAdjust_LEFT;
                                OUString sTextAdjust;
                                xShapePropSet->getPropertyValue( "ParaAdjust" ) >>= nTextAdjust;

                                switch( nTextAdjust )
                                {
                                    case ParagraphAdjust_LEFT:
                                            sTextAdjust = "left";
                                            break;
                                    case ParagraphAdjust_CENTER:
                                            sTextAdjust = "center";
                                            break;
                                    case ParagraphAdjust_RIGHT:
                                            sTextAdjust = "right";
                                            break;
                                    default:
                                        break;
                                }
                                mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, aOOOAttrTextAdjust, sTextAdjust );
                            }
                            else // single page case
                            {
                                if( !mVisiblePagePropSet.bAreBackgroundObjectsVisible || (
                                    ( bIsPageNumber && !mVisiblePagePropSet.bIsPageNumberFieldVisible ) ||
                                    ( bIsDateTime && !mVisiblePagePropSet.bIsDateTimeFieldVisible ) ||
                                    ( bIsFooter && !mVisiblePagePropSet.bIsFooterFieldVisible ) ) )
                                {
                                    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "visibility", "hidden" );
                                }
                            }
                        }
                    }
                    mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "class", aShapeClass );
                    SvXMLElementExport aExp( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );

                    Reference< XExtendedDocumentHandler > xExtDocHandler( mpSVGExport->GetDocHandler(), UNO_QUERY );

                    OUString aTitle;
                    xShapePropSet->getPropertyValue( "Title" ) >>= aTitle;
                    if( !aTitle.isEmpty() )
                    {
                        SvXMLElementExport aExp2( *mpSVGExport, XML_NAMESPACE_NONE, "title", true, true );
                        xExtDocHandler->characters( aTitle );
                    }

                    OUString aDescription;
                    xShapePropSet->getPropertyValue( "Description" ) >>= aDescription;
                    if( !aDescription.isEmpty() )
                    {
                        SvXMLElementExport aExp2( *mpSVGExport, XML_NAMESPACE_NONE, "desc", true, true );
                        xExtDocHandler->characters( aDescription );
                    }


                    const OUString& rShapeId = implGetValidIDFromInterface( Reference<XInterface>(rxShape, UNO_QUERY) );
                    if( !rShapeId.isEmpty() )
                    {
                        mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "id", rShapeId );
                    }

                    const GDIMetaFile* pEmbeddedBitmapsMtf = NULL;
                    if( mEmbeddedBitmapActionMap.find( rxShape ) !=  mEmbeddedBitmapActionMap.end() )
                    {
                        pEmbeddedBitmapsMtf = &( mEmbeddedBitmapActionMap[ rxShape ].GetRepresentation() );
                    }

                    {
                        OUString aBookmark;
                        Reference<XPropertySetInfo> xShapePropSetInfo = xShapePropSet->getPropertySetInfo();
                        if(xShapePropSetInfo->hasPropertyByName("Bookmark"))
                        {
                            xShapePropSet->getPropertyValue( "Bookmark" ) >>= aBookmark;
                        }

                        SvXMLElementExport aExp2( *mpSVGExport, XML_NAMESPACE_NONE, "g", true, true );
                        if( !aBookmark.isEmpty() )
                        {
                            mpSVGExport->AddAttribute( XML_NAMESPACE_NONE, "xlink:href", aBookmark);
                            SvXMLElementExport alinkA( *mpSVGExport, XML_NAMESPACE_NONE, "a", true, true );
                            mpSVGWriter->WriteMetaFile( aTopLeft, aSize, rMtf,
                                                        0xffffffff,
                                                        pElementId,
                                                        &rxShape,
                                                        pEmbeddedBitmapsMtf );
                        }
                        else
                        {
                            mpSVGWriter->WriteMetaFile( aTopLeft, aSize, rMtf,
                                                        0xffffffff,
                                                        pElementId,
                                                        &rxShape,
                                                        pEmbeddedBitmapsMtf );
                        }
                    }
                }

                bRet = true;
            }
        }
    }

    return bRet;
}



bool SVGFilter::implCreateObjects()
{
    if (mbExportSelection)
    {
        // #i124608# export a given object selection
        if (mSelectedPages.getLength() && mSelectedPages[0].is())
        {
            implCreateObjectsFromShapes(mSelectedPages[0], maShapeSelection);
            return true;
        }
        return false;
    }

    sal_Int32 i, nCount;

    for( i = 0, nCount = mMasterPageTargets.getLength(); i < nCount; ++i )
    {
        const Reference< XDrawPage > & xMasterPage = mMasterPageTargets[i];

        if( xMasterPage.is() )
        {
            mCreateOjectsCurrentMasterPage = xMasterPage;
            implCreateObjectsFromBackground( xMasterPage );

            if( xMasterPage.is() )
                implCreateObjectsFromShapes( xMasterPage, xMasterPage );
        }
    }

    for( i = 0, nCount = mSelectedPages.getLength(); i < nCount; ++i )
    {
        const Reference< XDrawPage > & xDrawPage = mSelectedPages[i];

        if( xDrawPage.is() )
        {
#ifdef ENABLE_EXPORT_CUSTOM_SLIDE_BACKGROUND
            // TODO complete the implementation for exporting custom background for each slide
            // implementation status:
            // - hatch stroke color is set to 'none' so the hatch is not visible, why ?
            // - gradient look is not really awesome, too few colors are used;
            // - stretched bitmap, gradient and hatch are not exported only once
            //   and then referenced in case more than one slide uses them.
            // - tiled bitmap: an image element is exported for each tile,
            //   this is really too expensive!
            Reference< XPropertySet > xPropSet( xDrawPage, UNO_QUERY );
            Reference< XPropertySet > xBackground;
            xPropSet->getPropertyValue( "Background" ) >>= xBackground;
            if( xBackground.is() )
            {
                drawing::FillStyle aFillStyle;
                sal_Bool assigned = ( xBackground->getPropertyValue( "FillStyle" ) >>= aFillStyle );
                if( assigned && aFillStyle != drawing::FillStyle_NONE )
                {
                    implCreateObjectsFromBackground( xDrawPage );
                }
            }
#endif

            if( xDrawPage.is() )
                implCreateObjectsFromShapes( xDrawPage, xDrawPage );
        }
    }
    return true;
}



bool SVGFilter::implCreateObjectsFromShapes( const Reference< XDrawPage > & rxPage, const Reference< XShapes >& rxShapes )
{
    Reference< XShape > xShape;
    bool            bRet = false;

    for( sal_Int32 i = 0, nCount = rxShapes->getCount(); i < nCount; ++i )
    {
        if( ( rxShapes->getByIndex( i ) >>= xShape ) && xShape.is() )
            bRet = implCreateObjectsFromShape( rxPage, xShape ) || bRet;

        xShape = NULL;
    }

    return bRet;
}



bool SVGFilter::implCreateObjectsFromShape( const Reference< XDrawPage > & rxPage, const Reference< XShape >& rxShape )
{
    bool bRet = false;
    if( rxShape->getShapeType().lastIndexOf( "drawing.GroupShape" ) != -1 )
    {
        Reference< XShapes > xShapes( rxShape, UNO_QUERY );

        if( xShapes.is() )
            bRet = implCreateObjectsFromShapes( rxPage, xShapes );
    }
    else
    {
        SdrObject*  pObj = GetSdrObjectFromXShape( rxShape );

        if( pObj )
        {
            Graphic aGraphic( SdrExchangeView::GetObjGraphic( pObj->GetModel(), pObj ) );

            if( aGraphic.GetType() != GRAPHIC_NONE )
            {
                if( aGraphic.GetType() == GRAPHIC_BITMAP )
                {
                    GDIMetaFile    aMtf;
                    const Point    aNullPt;
                    const Size    aSize( pObj->GetCurrentBoundRect().GetSize() );

                    aMtf.AddAction( new MetaBmpExScaleAction( aNullPt, aSize, aGraphic.GetBitmapEx() ) );
                    aMtf.SetPrefSize( aSize );
                    aMtf.SetPrefMapMode( MAP_100TH_MM );

                    (*mpObjects)[ rxShape ] = ObjectRepresentation( rxShape, aMtf );
                }
                else
                {
                    if( aGraphic.GetGDIMetaFile().GetActionSize() )
                    {
                        Reference< XText > xText( rxShape, UNO_QUERY );
                        bool bIsTextShape = xText.is();

                        if( !mpSVGExport->IsUsePositionedCharacters() && bIsTextShape )
                        {
                            Reference< XPropertySet >   xShapePropSet( rxShape, UNO_QUERY );

                            if( xShapePropSet.is() )
                            {
                                bool bHideObj = false;

                                if( mbPresentation )
                                {
                                    xShapePropSet->getPropertyValue( "IsEmptyPresentationObject" )  >>= bHideObj;
                                }

                                if( !bHideObj )
                                {
                                    // We create a map of text shape ids.
                                    implRegisterInterface( rxShape );
                                    const OUString& rShapeId = implGetValidIDFromInterface( Reference<XInterface>(rxShape, UNO_QUERY) );
                                    if( !rShapeId.isEmpty() )
                                    {
                                        mTextShapeIdListMap[rxPage] += rShapeId;
                                        mTextShapeIdListMap[rxPage] += " ";
                                    }

                                    // We create a set of bitmaps embedded into text shape.
                                    GDIMetaFile   aMtf;
                                    const Size    aSize( pObj->GetCurrentBoundRect().GetSize() );
                                    MetaAction*   pAction;
                                    bool bIsTextShapeStarted = false;
                                    const GDIMetaFile& rMtf = aGraphic.GetGDIMetaFile();
                                    sal_uLong nCount = rMtf.GetActionSize();
                                    for( sal_uLong nCurAction = 0; nCurAction < nCount; ++nCurAction )
                                    {
                                        pAction = rMtf.GetAction( nCurAction );
                                        const sal_uInt16    nType = pAction->GetType();

                                        if( nType == META_COMMENT_ACTION )
                                        {
                                            const MetaCommentAction* pA = (const MetaCommentAction*) pAction;
                                            if( ( pA->GetComment().equalsIgnoreAsciiCase("XTEXT_PAINTSHAPE_BEGIN") ) )
                                            {
                                                bIsTextShapeStarted = true;
                                            }
                                            else if( ( pA->GetComment().equalsIgnoreAsciiCase( "XTEXT_PAINTSHAPE_END" ) ) )
                                            {
                                                bIsTextShapeStarted = false;
                                            }
                                        }
                                        if( bIsTextShapeStarted && ( nType == META_BMPSCALE_ACTION  || nType == META_BMPEXSCALE_ACTION ) )
                                        {
                                            GDIMetaFile aEmbeddedBitmapMtf;
                                            pAction->Duplicate();
                                            aEmbeddedBitmapMtf.AddAction( pAction );
                                            aEmbeddedBitmapMtf.SetPrefSize( aSize );
                                            aEmbeddedBitmapMtf.SetPrefMapMode( MAP_100TH_MM );
                                            mEmbeddedBitmapActionSet.insert( ObjectRepresentation( rxShape, aEmbeddedBitmapMtf ) );
                                            pAction->Duplicate();
                                            aMtf.AddAction( pAction );
                                        }
                                    }
                                    aMtf.SetPrefSize( aSize );
                                    aMtf.SetPrefMapMode( MAP_100TH_MM );
                                    mEmbeddedBitmapActionMap[ rxShape ] = ObjectRepresentation( rxShape, aMtf );
                                }
                            }
                        }
                    }
                    (*mpObjects)[ rxShape ] = ObjectRepresentation( rxShape, aGraphic.GetGDIMetaFile() );
                }
                bRet = true;
            }
        }
    }

    return bRet;
}



bool SVGFilter::implCreateObjectsFromBackground( const Reference< XDrawPage >& rxDrawPage )
{
    Reference< XGraphicExportFilter >  xExporter = drawing::GraphicExportFilter::create( mxContext );

    GDIMetaFile             aMtf;

    utl::TempFile aFile;
    aFile.EnableKillingFile();

    Sequence< PropertyValue > aDescriptor( 3 );
    aDescriptor[0].Name = "FilterName";
    aDescriptor[0].Value <<= OUString( "SVM" );
    aDescriptor[1].Name = "URL";
    aDescriptor[1].Value <<= OUString( aFile.GetURL() );
    aDescriptor[2].Name = "ExportOnlyBackground";
    aDescriptor[2].Value <<= true;

    xExporter->setSourceDocument( Reference< XComponent >( rxDrawPage, UNO_QUERY ) );
    xExporter->filter( aDescriptor );
    aMtf.Read( *aFile.GetStream( STREAM_READ ) );

    (*mpObjects)[ rxDrawPage ] = ObjectRepresentation( rxDrawPage, aMtf );

    return true;
}



OUString SVGFilter::implGetClassFromShape( const Reference< XShape >& rxShape )
{
    OUString            aRet;
    const OUString      aShapeType( rxShape->getShapeType() );

    if( aShapeType.lastIndexOf( "drawing.GroupShape" ) != -1 )
        aRet = "Group";
    else if( aShapeType.lastIndexOf( "drawing.GraphicObjectShape" ) != -1 )
        aRet = "Graphic";
    else if( aShapeType.lastIndexOf( "drawing.OLE2Shape" ) != -1 )
        aRet = "OLE2";
    else if( aShapeType.lastIndexOf( "presentation.HeaderShape" ) != -1 )
        aRet = "Header";
    else if( aShapeType.lastIndexOf( "presentation.FooterShape" ) != -1 )
        aRet = "Footer";
    else if( aShapeType.lastIndexOf( "presentation.DateTimeShape" ) != -1 )
        aRet = "Date/Time";
    else if( aShapeType.lastIndexOf( "presentation.SlideNumberShape" ) != -1 )
        aRet = "Slide_Number";
    else if( aShapeType.lastIndexOf( "presentation.TitleTextShape" ) != -1 )
        aRet = "TitleText";
    else if( aShapeType.lastIndexOf( "presentation.OutlinerShape" ) != -1 )
        aRet = "Outline";
    else
        aRet = aShapeType;

    return aRet;
}



void SVGFilter::implRegisterInterface( const Reference< XInterface >& rxIf )
{
    if( rxIf.is() )
        (mpSVGExport->getInterfaceToIdentifierMapper()).registerReference( rxIf );
}



const OUString & SVGFilter::implGetValidIDFromInterface( const Reference< XInterface >& rxIf )
{
   return (mpSVGExport->getInterfaceToIdentifierMapper()).getIdentifier( rxIf );
}



OUString SVGFilter::implGetInterfaceName( const Reference< XInterface >& rxIf )
{
    Reference< XNamed > xNamed( rxIf, UNO_QUERY );
    OUString            aRet;
    if( xNamed.is() )
    {
        aRet = xNamed->getName().replace( ' ', '_' );
    }
    return aRet;
}



IMPL_LINK( SVGFilter, CalcFieldHdl, EditFieldInfo*, pInfo )
{
    bool bFieldProcessed = false;
    if( pInfo && mbPresentation )
    {
        bFieldProcessed = true;
        OUString   aRepresentation;
        if( !mbSinglePage )
        {
            if( mpSVGExport->IsEmbedFonts() && mpSVGExport->IsUsePositionedCharacters() )
            {
                // to notify to the SVGActionWriter::ImplWriteText method
                // that we are dealing with a placeholder shape
                aRepresentation = sPlaceholderTag;

                if( !mCreateOjectsCurrentMasterPage.is() )
                {
                    OSL_FAIL( "error: !mCreateOjectsCurrentMasterPage.is()" );
                    return 0;
                }
                bool bHasCharSetMap = !( mTextFieldCharSets.find( mCreateOjectsCurrentMasterPage ) == mTextFieldCharSets.end() );

                static const OUString aHeaderId( aOOOAttrHeaderField );
                static const OUString aFooterId( aOOOAttrFooterField );
                static const OUString aDateTimeId( aOOOAttrDateTimeField );
                static const OUString aVariableDateTimeId( aOOOAttrDateTimeField + "-variable" );

                const UCharSet * pCharSet = NULL;
                UCharSetMap * pCharSetMap = NULL;
                if( bHasCharSetMap )
                {
                    pCharSetMap = &( mTextFieldCharSets[ mCreateOjectsCurrentMasterPage ] );
                }
                const SvxFieldData* pField = pInfo->GetField().GetField();
                if( bHasCharSetMap && ( pField->GetClassId() == text::textfield::Type::PRESENTATION_HEADER ) && ( pCharSetMap->find( aHeaderId ) != pCharSetMap->end() ) )
                {
                    pCharSet = &( (*pCharSetMap)[ aHeaderId ] );
                }
                else if( bHasCharSetMap && ( pField->GetClassId() == text::textfield::Type::PRESENTATION_FOOTER ) && ( pCharSetMap->find( aFooterId ) != pCharSetMap->end() ) )
                {
                    pCharSet = &( (*pCharSetMap)[ aFooterId ] );
                }
                else if( pField->GetClassId() == text::textfield::Type::PRESENTATION_DATE_TIME )
                {
                    if( bHasCharSetMap && ( pCharSetMap->find( aDateTimeId ) != pCharSetMap->end() ) )
                    {
                        pCharSet = &( (*pCharSetMap)[ aDateTimeId ] );
                    }
                    if( bHasCharSetMap && ( pCharSetMap->find( aVariableDateTimeId ) != pCharSetMap->end() ) && !(*pCharSetMap)[ aVariableDateTimeId ].empty() )
                    {
                        SvxDateFormat eDateFormat = SVXDATEFORMAT_B, eCurDateFormat;
                        const UCharSet & aCharSet = (*pCharSetMap)[ aVariableDateTimeId ];
                        UCharSet::const_iterator aChar = aCharSet.begin();
                        // we look for the most verbose date format
                        for( ; aChar != aCharSet.end(); ++aChar )
                        {
                            eCurDateFormat = (SvxDateFormat)( (int)( *aChar ) & 0x0f );
                            switch( eDateFormat )
                            {
                                case SVXDATEFORMAT_STDSMALL:
                                case SVXDATEFORMAT_A:       // 13.02.96
                                case SVXDATEFORMAT_B:       // 13.02.1996
                                    switch( eCurDateFormat )
                                    {
                                        case SVXDATEFORMAT_C:       // 13.Feb 1996
                                        case SVXDATEFORMAT_D:       // 13.February 1996
                                        case SVXDATEFORMAT_E:       // Tue, 13.February 1996
                                        case SVXDATEFORMAT_STDBIG:
                                        case SVXDATEFORMAT_F:       // Tuesday, 13.February 1996
                                            eDateFormat = eCurDateFormat;
                                            break;
                                        default:
                                            break;
                                    }
                                case SVXDATEFORMAT_C:       // 13.Feb 1996
                                case SVXDATEFORMAT_D:       // 13.February 1996
                                    switch( eCurDateFormat )
                                    {
                                        case SVXDATEFORMAT_E:       // Tue, 13.February 1996
                                        case SVXDATEFORMAT_STDBIG:
                                        case SVXDATEFORMAT_F:       // Tuesday, 13.February 1996
                                            eDateFormat = eCurDateFormat;
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                                default:
                                    break;
                            }
                        }
                        // Independently of the date format, we always put all these characters by default.
                        // They should be enough to cover every time format.
                        aRepresentation += "0123456789.:/-APM";

                        if( eDateFormat )
                        {
                            OUString sDate;
                            LanguageType eLang = pInfo->GetOutliner()->GetLanguage( pInfo->GetPara(), pInfo->GetPos() );
                            SvNumberFormatter * pNumberFormatter = new SvNumberFormatter( ::comphelper::getProcessComponentContext(), LANGUAGE_SYSTEM );
                            // We always collect the characters obtained by using the SVXDATEFORMAT_B (as: 13.02.1996)
                            // so we are sure to include any unusual day|month|year separator.
                            Date aDate( 1, 1, 1996 );
                            sDate += SvxDateField::GetFormatted( aDate, SVXDATEFORMAT_B, *pNumberFormatter, eLang );
                            switch( eDateFormat )
                            {
                                case SVXDATEFORMAT_E:       // Tue, 13.February 1996
                                case SVXDATEFORMAT_STDBIG:
                                case SVXDATEFORMAT_F:       // Tuesday, 13.February 1996
                                    for( sal_uInt16 i = 1; i <= 7; ++i )  // we get all days in a week
                                    {
                                        aDate.SetDay( i );
                                        sDate += SvxDateField::GetFormatted( aDate, eDateFormat, *pNumberFormatter, eLang );
                                    }
                                    // No break here! We need months too!
                                case SVXDATEFORMAT_C:       // 13.Feb 1996
                                case SVXDATEFORMAT_D:       // 13.February 1996
                                    for( sal_uInt16 i = 1; i <= 12; ++i ) // we get all months in a year
                                    {
                                        aDate.SetMonth( i );
                                        sDate += SvxDateField::GetFormatted( aDate, eDateFormat, *pNumberFormatter, eLang );
                                    }
                                    break;
                                case SVXDATEFORMAT_STDSMALL:
                                case SVXDATEFORMAT_A:       // 13.02.96
                                case SVXDATEFORMAT_B:       // 13.02.1996
                                default:
                                    // nothing to do here, we always collect the characters needed for these cases.
                                    break;
                            }
                            aRepresentation += sDate;
                        }
                    }
                }
                else if( pField->GetClassId() == text::textfield::Type::PAGE )
                {
                    switch( mVisiblePagePropSet.nPageNumberingType )
                    {
                        case SVX_CHARS_UPPER_LETTER:
                            aRepresentation += "QWERTYUIOPASDFGHJKLZXCVBNM";
                            break;
                        case SVX_CHARS_LOWER_LETTER:
                            aRepresentation += "qwertyuiopasdfghjklzxcvbnm";
                            break;
                        case SVX_ROMAN_UPPER:
                            aRepresentation += "IVXLCDM";
                            break;
                        case SVX_ROMAN_LOWER:
                            aRepresentation += "ivxlcdm";
                            break;
                        // arabic numbering type is the default
                        case SVX_ARABIC:
                        // in case the numbering type is not handled we fall back on arabic numbering
                        default:
                            aRepresentation += "0123456789";
                            break;
                    }
                }
                else
                {
                    bFieldProcessed = false;
                }
                if( bFieldProcessed )
                {
                    if( pCharSet != NULL )
                    {
                        UCharSet::const_iterator aChar = pCharSet->begin();
                        for( ; aChar != pCharSet->end(); ++aChar )
                        {
                            aRepresentation += OUString( *aChar );
                        }
                    }
                    pInfo->SetRepresentation( aRepresentation );
                }
            }
            else
            {
                bFieldProcessed = false;
            }
        }
        else  // single page case
        {
            if( mVisiblePagePropSet.bAreBackgroundObjectsVisible )
            {
                const SvxFieldData* pField = pInfo->GetField().GetField();
                if( ( pField->GetClassId() == text::textfield::Type::PRESENTATION_HEADER ) && mVisiblePagePropSet.bIsHeaderFieldVisible )
                {
                    aRepresentation += mVisiblePagePropSet.sHeaderText;
                }
                else if( ( pField->GetClassId() == text::textfield::Type::PRESENTATION_FOOTER ) && mVisiblePagePropSet.bIsFooterFieldVisible )
                {
                    aRepresentation += mVisiblePagePropSet.sFooterText;
                }
                else if( ( pField->GetClassId() == text::textfield::Type::PRESENTATION_DATE_TIME ) && mVisiblePagePropSet.bIsDateTimeFieldVisible )
                {
                    // TODO: implement the variable case
                    aRepresentation += mVisiblePagePropSet.sDateTimeText;
                }
                else if( ( pField->GetClassId() == text::textfield::Type::PAGE ) && mVisiblePagePropSet.bIsPageNumberFieldVisible )
                {
                    sal_Int16 nPageNumber = mVisiblePagePropSet.nPageNumber;
                    switch( mVisiblePagePropSet.nPageNumberingType )
                    {
                        case SVX_CHARS_UPPER_LETTER:
                            aRepresentation += OUString( (sal_Unicode)(char)( ( nPageNumber - 1 ) % 26 + 'A' ) );
                            break;
                        case SVX_CHARS_LOWER_LETTER:
                            aRepresentation += OUString( (sal_Unicode)(char)( ( nPageNumber - 1 ) % 26 + 'a' ) );
                            break;
                        case SVX_ROMAN_UPPER:
                            aRepresentation += SvxNumberFormat::CreateRomanString( nPageNumber, true /* upper */ );
                            break;
                        case SVX_ROMAN_LOWER:
                            aRepresentation += SvxNumberFormat::CreateRomanString( nPageNumber, false /* lower */ );
                            break;
                        // arabic numbering type is the default
                        case SVX_ARABIC:
                        // in case the numbering type is not handled we fall back on arabic numbering
                        default:
                            aRepresentation += OUString::number( nPageNumber );
                            break;
                    }
                }
                else
                {
                    bFieldProcessed = false;
                }
                if( bFieldProcessed )
                {
                    pInfo->SetRepresentation( aRepresentation );
                }
            }

        }
    }
    return ( bFieldProcessed ? 0 : maOldFieldHdl.Call( pInfo ) );
}



void SVGExport::writeMtf( const GDIMetaFile& rMtf )
{
    const Size aSize( OutputDevice::LogicToLogic( rMtf.GetPrefSize(), rMtf.GetPrefMapMode(), MAP_MM ) );
    rtl::OUString aAttr;
    Reference< XExtendedDocumentHandler> xExtDocHandler( GetDocHandler(), UNO_QUERY );

    if( xExtDocHandler.is() )
        xExtDocHandler->unknown( SVG_DTD_STRING );

    aAttr = OUString::number( aSize.Width() );
    aAttr += "mm";
    AddAttribute( XML_NAMESPACE_NONE, "width", aAttr );

    aAttr = OUString::number( aSize.Height() );
    aAttr += "mm";
    AddAttribute( XML_NAMESPACE_NONE, "height", aAttr );

    aAttr = "0 0 ";
    aAttr += OUString::number( aSize.Width() * 100L );
    aAttr += " ";
    aAttr += OUString::number( aSize.Height() * 100L );
    AddAttribute( XML_NAMESPACE_NONE, "viewBox", aAttr );

    AddAttribute( XML_NAMESPACE_NONE, "version", "1.1" );

    if( IsUseTinyProfile() )
         AddAttribute( XML_NAMESPACE_NONE, "baseProfile", "tiny" );

    AddAttribute( XML_NAMESPACE_NONE, "xmlns", constSvgNamespace );
    AddAttribute( XML_NAMESPACE_NONE, "stroke-width", OUString::number( 28.222 ) );
    AddAttribute( XML_NAMESPACE_NONE, "stroke-linejoin", "round" );
    AddAttribute( XML_NAMESPACE_NONE, "xml:space", "preserve" );

    {
        SvXMLElementExport  aSVG( *this, XML_NAMESPACE_NONE, "svg", true, true );

        std::vector< ObjectRepresentation > aObjects;

        aObjects.push_back( ObjectRepresentation( Reference< XInterface >(), rMtf ) );
        SVGFontExport aSVGFontExport( *this, aObjects );

        Point aPoint100thmm( OutputDevice::LogicToLogic( rMtf.GetPrefMapMode().GetOrigin(), rMtf.GetPrefMapMode(), MAP_100TH_MM ) );
        Size  aSize100thmm( OutputDevice::LogicToLogic( rMtf.GetPrefSize(), rMtf.GetPrefMapMode(), MAP_100TH_MM ) );

        SVGActionWriter aWriter( *this, aSVGFontExport );
        aWriter.WriteMetaFile( aPoint100thmm, aSize100thmm, rMtf,
            SVGWRITER_WRITE_FILL | SVGWRITER_WRITE_TEXT, NULL );
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
