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

#include <rsc/rscsfx.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/objsh.hxx>
#include <osl/diagnose.h>
#include <comphelper/processfactory.hxx>
#include <unotools/streamwrap.hxx>
#include <svx/xmlgrhlp.hxx>
#include <svtools/sfxecode.hxx>
#include <sfx2/frame.hxx>
#include <svl/itemset.hxx>
#include <svl/stritem.hxx>
#include <sfx2/sfxsids.hrc>
#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <com/sun/star/xml/sax/XErrorHandler.hpp>
#include <com/sun/star/xml/sax/XEntityResolver.hpp>
#include <com/sun/star/xml/sax/InputSource.hpp>
#include <com/sun/star/xml/sax/XDTDHandler.hpp>
#include <com/sun/star/xml/sax/Parser.hpp>
#include <com/sun/star/xml/sax/Writer.hpp>
#include <com/sun/star/io/XActiveDataSource.hpp>
#include <com/sun/star/io/XActiveDataControl.hpp>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/task/XStatusIndicatorFactory.hpp>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <comphelper/extract.hxx>
#include <comphelper/propertysetinfo.hxx>
#include <comphelper/genericpropertyset.hxx>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/lang/DisposedException.hpp>
#include <com/sun/star/packages/zip/ZipIOException.hpp>
#include <com/sun/star/embed/ElementModes.hpp>
#include <com/sun/star/script/vba/XVBACompatibility.hpp>
#include <com/sun/star/rdf/XDocumentMetadataAccess.hpp>
#include <com/sun/star/ucb/InteractiveAugmentedIOException.hpp>

#include <sfx2/DocumentMetadataAccess.hxx>
#include <comphelper/documentconstants.hxx>
#include <svx/xmleohlp.hxx>
#include <rtl/strbuf.hxx>
#include <unotools/saveopt.hxx>

#include "document.hxx"
#include "xmlwrap.hxx"
#include "xmlimprt.hxx"
#include "xmlexprt.hxx"
#include "global.hxx"
#include "globstr.hrc"
#include "scerrors.hxx"
#include "XMLExportSharedData.hxx"
#include "docuno.hxx"
#include "sheetdata.hxx"
#include "XMLCodeNameProvider.hxx"
#include <docsh.hxx>
#include <unonames.hxx>

using namespace com::sun::star;

ScXMLImportWrapper::ScXMLImportWrapper( ScDocShell& rDocSh, SfxMedium* pM, const uno::Reference < embed::XStorage >& xStor ) :
    mrDocShell(rDocSh),
    rDoc(rDocSh.GetDocument()),
    pMedium(pM),
    xStorage(xStor)
{
    OSL_ENSURE( pMedium || xStorage.is(), "ScXMLImportWrapper: Medium or Storage must be set" );
}

uno::Reference <task::XStatusIndicator> ScXMLImportWrapper::GetStatusIndicator()
{
    uno::Reference<task::XStatusIndicator> xStatusIndicator;
    if (pMedium)
    {
        SfxItemSet* pSet = pMedium->GetItemSet();
        if (pSet)
        {
            const SfxUnoAnyItem* pItem = static_cast<const SfxUnoAnyItem*>(pSet->GetItem(SID_PROGRESS_STATUSBAR_CONTROL));
            if (pItem)
                xStatusIndicator.set(pItem->GetValue(), uno::UNO_QUERY);
        }
    }
    return xStatusIndicator;
}

sal_uInt32 ScXMLImportWrapper::ImportFromComponent(const uno::Reference<uno::XComponentContext>& xContext,
    uno::Reference<frame::XModel>& xModel, uno::Reference<xml::sax::XParser>& xParser,
    xml::sax::InputSource& aParserInput,
    const OUString& sComponentName, const OUString& sDocName,
    const OUString& sOldDocName, uno::Sequence<uno::Any>& aArgs,
    bool bMustBeSuccessfull)
{
    uno::Reference < io::XStream > xDocStream;
    if ( !xStorage.is() && pMedium )
        xStorage = pMedium->GetStorage();

    bool bEncrypted = false;
    OUString sStream(sDocName);
    if( xStorage.is() )
    {
        try
        {
            uno::Reference < container::XNameAccess > xAccess( xStorage, uno::UNO_QUERY );
            if ( xAccess->hasByName(sDocName) && xStorage->isStreamElement( sDocName) )
                xDocStream = xStorage->openStreamElement( sDocName, embed::ElementModes::READ );
            else if (!sOldDocName.isEmpty() && xAccess->hasByName(sOldDocName) && xStorage->isStreamElement( sOldDocName) )
            {
                xDocStream = xStorage->openStreamElement( sOldDocName, embed::ElementModes::READ );
                sStream = sOldDocName;
            }
            else
                return SCERR_NONE;

            aParserInput.aInputStream = xDocStream->getInputStream();
            uno::Reference < beans::XPropertySet > xSet( xDocStream, uno::UNO_QUERY );

            uno::Any aAny = xSet->getPropertyValue("Encrypted");
            aAny >>= bEncrypted;
        }
        catch( const packages::WrongPasswordException& )
        {
            return ERRCODE_SFX_WRONGPASSWORD;
        }
        catch( const packages::zip::ZipIOException& )
        {
            return ERRCODE_IO_BROKENPACKAGE;
        }
        catch( const uno::Exception& )
        {
            return SCERR_IMPORT_UNKNOWN;
        }
    }
    else
        return SCERR_IMPORT_UNKNOWN;

    // set Base URL
    uno::Reference< beans::XPropertySet > xInfoSet;
    if( aArgs.getLength() > 0 )
        aArgs.getConstArray()[0] >>= xInfoSet;
    OSL_ENSURE( xInfoSet.is(), "missing property set" );
    if( xInfoSet.is() )
    {
        OUString sPropName("StreamName");
        xInfoSet->setPropertyValue( sPropName, uno::makeAny( sStream ) );
    }

    sal_uInt32 nReturn = SCERR_NONE;
    rDoc.SetRangeOverflowType(0);   // is modified by the importer if limits are exceeded

    uno::Reference<xml::sax::XDocumentHandler> xDocHandler(
        xContext->getServiceManager()->createInstanceWithArgumentsAndContext(
            sComponentName, aArgs, xContext ),
        uno::UNO_QUERY );
    OSL_ENSURE( xDocHandler.is(), "can't get Calc importer" );
    uno::Reference<document::XImporter> xImporter( xDocHandler, uno::UNO_QUERY );
    uno::Reference<lang::XComponent> xComponent( xModel, uno::UNO_QUERY );
    if (xImporter.is())
        xImporter->setTargetDocument( xComponent );

    ScXMLImport* pImporterImpl = dynamic_cast<ScXMLImport*>(xImporter.get());
    if (pImporterImpl)
        pImporterImpl->SetPostProcessData(&maPostProcessData);

    // connect parser and filter
    xParser->setDocumentHandler( xDocHandler );

    try
    {
        xParser->parseStream( aParserInput );
    }
    catch( const xml::sax::SAXParseException& r )
    {
        // sax parser sends wrapped exceptions,
        // try to find the original one
        xml::sax::SAXException aSaxEx = *(xml::sax::SAXException*)(&r);
        bool bTryChild = true;

        while( bTryChild )
        {
            xml::sax::SAXException aTmp;
            if ( aSaxEx.WrappedException >>= aTmp )
                aSaxEx = aTmp;
            else
                bTryChild = false;
        }

        packages::zip::ZipIOException aBrokenPackage;
        if ( aSaxEx.WrappedException >>= aBrokenPackage )
            return ERRCODE_IO_BROKENPACKAGE;
        else if( bEncrypted )
            nReturn = ERRCODE_SFX_WRONGPASSWORD;
        else
        {

#if OSL_DEBUG_LEVEL > 0
            OStringBuffer aError("SAX parse exception caught while importing:\n");
            aError.append(OUStringToOString(r.Message, RTL_TEXTENCODING_ASCII_US));
            OSL_FAIL(aError.getStr());
#endif

            OUString sErr = OUString::number( r.LineNumber ) +
                          "," +
                          OUString::number( r.ColumnNumber );

            if( !sDocName.isEmpty() )
            {
                nReturn = *new TwoStringErrorInfo(
                                (bMustBeSuccessfull ? SCERR_IMPORT_FILE_ROWCOL
                                                        : SCWARN_IMPORT_FILE_ROWCOL),
                                sDocName, sErr,
                                ERRCODE_BUTTON_OK | ERRCODE_MSG_ERROR );
            }
            else
            {
                OSL_ENSURE( bMustBeSuccessfull, "Warnings are not supported" );
                nReturn = *new StringErrorInfo( SCERR_IMPORT_FORMAT_ROWCOL, sErr,
                                 ERRCODE_BUTTON_OK | ERRCODE_MSG_ERROR );
            }
        }
    }
    catch( const xml::sax::SAXException& r )
    {
        packages::zip::ZipIOException aBrokenPackage;
        if ( r.WrappedException >>= aBrokenPackage )
            return ERRCODE_IO_BROKENPACKAGE;
        else if( bEncrypted )
            nReturn = ERRCODE_SFX_WRONGPASSWORD;
        else
        {

#if OSL_DEBUG_LEVEL > 0
            OStringBuffer aError("SAX exception caught while importing:\n");
            aError.append(OUStringToOString(r.Message,
                RTL_TEXTENCODING_ASCII_US));
            OSL_FAIL(aError.getStr());
#endif
            (void)r;    // avoid warning in product version

            nReturn = SCERR_IMPORT_FORMAT;
        }
    }
    catch( const packages::zip::ZipIOException& r )
    {
#if OSL_DEBUG_LEVEL > 0
        OStringBuffer aError("Zip exception caught while importing:\n");
        aError.append(OUStringToOString(r.Message,
            RTL_TEXTENCODING_ASCII_US));
        OSL_FAIL( aError.getStr() );
#endif
        (void)r;    // avoid warning in product version

        nReturn = ERRCODE_IO_BROKENPACKAGE;
    }
    catch( const io::IOException& r )
    {
#if OSL_DEBUG_LEVEL > 0
        OStringBuffer aError("IO exception caught while importing:\n");
        aError.append(OUStringToOString(r.Message,
            RTL_TEXTENCODING_ASCII_US));
        OSL_FAIL(aError.getStr());
#endif
        (void)r;    // avoid warning in product version

        nReturn = SCERR_IMPORT_OPEN;
    }
    catch( const uno::Exception& r )
    {
#if OSL_DEBUG_LEVEL > 0
        OStringBuffer aError("uno exception caught while importing:\n");
        aError.append(OUStringToOString(r.Message,
            RTL_TEXTENCODING_ASCII_US));
        OSL_FAIL(aError.getStr());
#endif
        (void)r;    // avoid warning in product version

        nReturn = SCERR_IMPORT_UNKNOWN;
    }

    // #i31130# Can't use getImplementation here to get the ScXMLImport from xDocHandler,
    // because when OOo 1.x files are loaded, xDocHandler is the OOo2OasisTransformer.
    // So the overflow warning ErrorCode is now stored in the document.
    // Export works differently, there getImplementation still works.

    if (rDoc.HasRangeOverflow() && !nReturn)
        nReturn = rDoc.GetRangeOverflowType();

    // free the component
    xParser->setDocumentHandler( NULL );

    // success!
    return nReturn;
}

bool ScXMLImportWrapper::Import( sal_uInt8 nMode, ErrCode& rError )
{
    uno::Reference<uno::XComponentContext> xContext = comphelper::getProcessComponentContext();

    xml::sax::InputSource aParserInput;
    if (pMedium)
        aParserInput.sSystemId = pMedium->GetName();

    if ( !xStorage.is() && pMedium )
        xStorage = pMedium->GetStorage();

    // get parser
    uno::Reference<xml::sax::XParser> xXMLParser = xml::sax::Parser::create(xContext);

    // get filter
    OUString sEmpty;
    uno::Reference<frame::XModel> xModel = mrDocShell.GetModel();

    /** property map for export info set */
    comphelper::PropertyMapEntry const aImportInfoMap[] =
    {
        { OUString("ProgressRange"), 0, ::cppu::UnoType<sal_Int32>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("ProgressMax"), 0, ::cppu::UnoType<sal_Int32>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("ProgressCurrent"), 0, ::cppu::UnoType<sal_Int32>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("NumberStyles"), 0, cppu::UnoType<container::XNameAccess>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("PrivateData"), 0, cppu::UnoType<uno::XInterface>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("BaseURI"), 0, ::cppu::UnoType<OUString>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("StreamRelPath"), 0, ::cppu::UnoType<OUString>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("StreamName"), 0, ::cppu::UnoType<OUString>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("BuildId"), 0, ::cppu::UnoType<OUString>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("VBACompatibilityMode"), 0, ::getBooleanCppuType(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("ScriptConfiguration"), 0, cppu::UnoType<container::XNameAccess>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("OrganizerMode"), 0, ::getBooleanCppuType(),
            ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("SourceStorage"), 0, cppu::UnoType<embed::XStorage>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString(SC_UNO_ODS_LOCK_SOLAR_MUTEX), 0, getBooleanCppuType(), css::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString(SC_UNO_ODS_IMPORT_STYLES), 0, getBooleanCppuType(), css::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString(), 0, css::uno::Type(), 0, 0 }
    };
    uno::Reference< beans::XPropertySet > xInfoSet( comphelper::GenericPropertySet_CreateInstance( new comphelper::PropertySetInfo( aImportInfoMap ) ) );

    // No need to lock solar mutex when calling from the wrapper.
    xInfoSet->setPropertyValue(SC_UNO_ODS_LOCK_SOLAR_MUTEX, uno::makeAny(false));

    // ---- get BuildId from parent container if available

    uno::Reference< container::XChild > xChild( xModel, uno::UNO_QUERY );
    if( xChild.is() )
    {
        uno::Reference< beans::XPropertySet > xParentSet( xChild->getParent(), uno::UNO_QUERY );
        if( xParentSet.is() )
        {
            uno::Reference< beans::XPropertySetInfo > xPropSetInfo( xParentSet->getPropertySetInfo() );
            OUString sPropName("BuildId" );
            if( xPropSetInfo.is() && xPropSetInfo->hasPropertyByName(sPropName) )
            {
                xInfoSet->setPropertyValue( sPropName, xParentSet->getPropertyValue(sPropName) );
            }
        }
    }

    uno::Reference<task::XStatusIndicator> xStatusIndicator = GetStatusIndicator();
    if (xStatusIndicator.is())
    {
        sal_Int32 nProgressRange(1000000);
        xStatusIndicator->start(ScGlobal::GetRscString(STR_LOAD_DOC), nProgressRange);
        xInfoSet->setPropertyValue("ProgressRange", uno::makeAny(nProgressRange));
    }

    // Set base URI
    OSL_ENSURE( pMedium, "There is no medium to get MediaDescriptor from!\n" );
    OUString aBaseURL = pMedium ? pMedium->GetBaseURL() : OUString();
    OUString sPropName("BaseURI");
    xInfoSet->setPropertyValue( sPropName, uno::makeAny( aBaseURL ) );

    // TODO/LATER: do not do it for embedded links
    OUString aName;
    if (SFX_CREATE_MODE_EMBEDDED == mrDocShell.GetCreateMode())
    {
        if ( pMedium && pMedium->GetItemSet() )
        {
            const SfxStringItem* pDocHierarchItem = static_cast<const SfxStringItem*>(
                pMedium->GetItemSet()->GetItem(SID_DOC_HIERARCHICALNAME) );
            if ( pDocHierarchItem )
                aName = pDocHierarchItem->GetValue();
        }
        else
            aName = "dummyObjectName";

        if( !aName.isEmpty() )
        {
            sPropName = "StreamRelPath";
            xInfoSet->setPropertyValue( sPropName, uno::makeAny( aName ) );
        }
    }

    if (mrDocShell.GetCreateMode() == SFX_CREATE_MODE_ORGANIZER)
        xInfoSet->setPropertyValue("OrganizerMode", uno::makeAny(true));

    xInfoSet->setPropertyValue( "SourceStorage", uno::Any( xStorage ) );

    bool bOasis = ( SotStorage::GetVersion( xStorage ) > SOFFICE_FILEFORMAT_60 );

    if ((nMode & METADATA) == METADATA && bOasis)
    {
        // RDF metadata: ODF >= 1.2
        try
        {
            const uno::Reference< rdf::XDocumentMetadataAccess > xDMA(
                xModel, uno::UNO_QUERY_THROW );
            const uno::Reference< rdf::XURI > xBaseURI(
                ::sfx2::createBaseURI( xContext, xStorage, aBaseURL, aName ) );
            uno::Reference<task::XInteractionHandler> xHandler =
                mrDocShell.GetMedium()->GetInteractionHandler();
            xDMA->loadMetadataFromStorage( xStorage, xBaseURI, xHandler );
        }
        catch ( const lang::WrappedTargetException & e)
        {
            ucb::InteractiveAugmentedIOException iaioe;
            if ( e.TargetException >>= iaioe )
            {
                rError = SCERR_IMPORT_UNKNOWN;
            }
            else
            {
                rError = SCWARN_IMPORT_FEATURES_LOST;
            }
        }
        catch ( const uno::Exception &)
        {
            rError = SCWARN_IMPORT_FEATURES_LOST;
        }
    }

    // #i103539#: always read meta.xml for generator
    sal_uInt32 nMetaRetval(0);
    if ((nMode & METADATA) == METADATA)
    {
        uno::Sequence<uno::Any> aMetaArgs(1);
        uno::Any* pMetaArgs = aMetaArgs.getArray();
        pMetaArgs[0] <<= xInfoSet;

        SAL_INFO( "sc.filter", "meta import start" );

        nMetaRetval = ImportFromComponent(
                                xContext, xModel, xXMLParser, aParserInput,
                                bOasis ? OUString("com.sun.star.comp.Calc.XMLOasisMetaImporter")
                                : OUString("com.sun.star.comp.Calc.XMLMetaImporter"),
                                "meta.xml", "Meta.xml", aMetaArgs, false);

        SAL_INFO( "sc.filter", "meta import end" );
    }

    SvXMLGraphicHelper* pGraphicHelper = NULL;
    uno::Reference< document::XGraphicObjectResolver > xGrfContainer;

    uno::Reference< document::XEmbeddedObjectResolver > xObjectResolver;
    SvXMLEmbeddedObjectHelper *pObjectHelper = NULL;

    if( xStorage.is() )
    {
        pGraphicHelper = SvXMLGraphicHelper::Create( xStorage, GRAPHICHELPER_MODE_READ );
        xGrfContainer = pGraphicHelper;

        pObjectHelper = SvXMLEmbeddedObjectHelper::Create(xStorage, mrDocShell, EMBEDDEDOBJECTHELPER_MODE_READ, false);
        xObjectResolver = pObjectHelper;
    }
    uno::Sequence<uno::Any> aStylesArgs(4);
    uno::Any* pStylesArgs = aStylesArgs.getArray();
    pStylesArgs[0] <<= xInfoSet;
    pStylesArgs[1] <<= xGrfContainer;
    pStylesArgs[2] <<= xStatusIndicator;
    pStylesArgs[3] <<= xObjectResolver;

    sal_uInt32 nSettingsRetval(0);
    if ((nMode & SETTINGS) == SETTINGS)
    {
        //  Settings must be loaded first because of the printer setting,
        //  which is needed in the page styles (paper tray).

        uno::Sequence<uno::Any> aSettingsArgs(1);
        uno::Any* pSettingsArgs = aSettingsArgs.getArray();
        pSettingsArgs[0] <<= xInfoSet;

        SAL_INFO( "sc.filter", "settings import start" );

        nSettingsRetval = ImportFromComponent(
                            xContext, xModel, xXMLParser, aParserInput,
                            bOasis ? OUString("com.sun.star.comp.Calc.XMLOasisSettingsImporter")
                                   : OUString("com.sun.star.comp.Calc.XMLSettingsImporter"),
                            "settings.xml", sEmpty, aSettingsArgs, false);

        SAL_INFO( "sc.filter", "settings import end" );
    }

    sal_uInt32 nStylesRetval(0);
    if ((nMode & STYLES) == STYLES)
    {
        SAL_INFO( "sc.filter", "styles import start" );

        nStylesRetval = ImportFromComponent(xContext, xModel, xXMLParser, aParserInput,
            bOasis ? OUString("com.sun.star.comp.Calc.XMLOasisStylesImporter")
                   : OUString("com.sun.star.comp.Calc.XMLStylesImporter"),
            OUString("styles.xml"),
            sEmpty, aStylesArgs, true);

        SAL_INFO( "sc.filter", "styles import end" );
    }

    sal_uInt32 nDocRetval(0);
    if ((nMode & CONTENT) == CONTENT)
    {
        if (mrDocShell.GetCreateMode() == SFX_CREATE_MODE_INTERNAL)
            // We only need to import content for external link cache document.
            xInfoSet->setPropertyValue(SC_UNO_ODS_IMPORT_STYLES, uno::makeAny(false));

        uno::Sequence<uno::Any> aDocArgs(4);
        uno::Any* pDocArgs = aDocArgs.getArray();
        pDocArgs[0] <<= xInfoSet;
        pDocArgs[1] <<= xGrfContainer;
        pDocArgs[2] <<= xStatusIndicator;
        pDocArgs[3] <<= xObjectResolver;

        SAL_INFO( "sc.filter", "content import start" );

        nDocRetval = ImportFromComponent(xContext, xModel, xXMLParser, aParserInput,
            bOasis ? OUString("com.sun.star.comp.Calc.XMLOasisContentImporter")
                   : OUString("com.sun.star.comp.Calc.XMLContentImporter"),
            OUString("content.xml"),
            OUString("Content.xml"), aDocArgs,
            true);

        SAL_INFO( "sc.filter", "content import end" );
    }
    if( pGraphicHelper )
        SvXMLGraphicHelper::Destroy( pGraphicHelper );

    if( pObjectHelper )
        SvXMLEmbeddedObjectHelper::Destroy( pObjectHelper );

    if (xStatusIndicator.is())
        xStatusIndicator->end();

    bool bRet = false;
    if (nDocRetval)
    {
        rError = nDocRetval;
        if (nDocRetval == SCWARN_IMPORT_RANGE_OVERFLOW ||
            nDocRetval == SCWARN_IMPORT_ROW_OVERFLOW ||
            nDocRetval == SCWARN_IMPORT_COLUMN_OVERFLOW ||
            nDocRetval == SCWARN_IMPORT_SHEET_OVERFLOW)
            bRet = true;
    }
    else if (nStylesRetval)
        rError = nStylesRetval;
    else if (nMetaRetval)
        rError = nMetaRetval;
    else if (nSettingsRetval)
        rError = nSettingsRetval;
    else
        bRet = true;

    // set BuildId on XModel for later OLE object loading
    if( xInfoSet.is() )
    {
        uno::Reference< beans::XPropertySet > xModelSet( xModel, uno::UNO_QUERY );
        if( xModelSet.is() )
        {
            uno::Reference< beans::XPropertySetInfo > xModelSetInfo( xModelSet->getPropertySetInfo() );
            OUString sBuildPropName("BuildId" );
            if( xModelSetInfo.is() && xModelSetInfo->hasPropertyByName(sBuildPropName) )
            {
                xModelSet->setPropertyValue( sBuildPropName, xInfoSet->getPropertyValue(sBuildPropName) );
            }
        }

        // Set Code Names
        uno::Any aAny = xInfoSet->getPropertyValue("ScriptConfiguration");
        uno::Reference <container::XNameAccess> xCodeNameAccess;
        if( aAny >>= xCodeNameAccess )
            XMLCodeNameProvider::set( xCodeNameAccess, &rDoc );

        // VBA compatibility
        bool bVBACompat = false;
        if ( (xInfoSet->getPropertyValue("VBACompatibilityMode") >>= bVBACompat) && bVBACompat )
        {
            /*  Set library container to VBA compatibility mode, this
                forces loading the Basic project, which in turn creates the
                VBA Globals object and does all related initialization. */
            if ( xModelSet.is() ) try
            {
                uno::Reference< script::vba::XVBACompatibility > xVBACompat( xModelSet->getPropertyValue(
                    OUString( "BasicLibraries" ) ), uno::UNO_QUERY_THROW );
                xVBACompat->setVBACompatibilityMode( sal_True );
            }
            catch( const uno::Exception& )
            {
            }
        }
    }

    // Don't test bStylesRetval and bMetaRetval, because it could be an older file which not contain such streams
    return bRet;//!bStylesOnly ? bDocRetval : bStylesRetval;
}

static bool lcl_HasValidStream(ScDocument& rDoc)
{
    SfxObjectShell* pObjSh = rDoc.GetDocumentShell();
    if ( pObjSh->IsDocShared() )
        return false;                       // never copy stream from shared file

    // don't read remote file again
    // (could instead re-use medium directly in that case)
    SfxMedium* pSrcMed = rDoc.GetDocumentShell()->GetMedium();
    if ( !pSrcMed || pSrcMed->IsRemote() )
        return false;

    SCTAB nTabCount = rDoc.GetTableCount();
    for (SCTAB nTab=0; nTab<nTabCount; ++nTab)
        if (rDoc.IsStreamValid(nTab))
            return true;
    return false;
}

bool ScXMLImportWrapper::ExportToComponent(const uno::Reference<uno::XComponentContext>& xContext,
    uno::Reference<frame::XModel>& xModel, uno::Reference<xml::sax::XWriter>& xWriter,
    uno::Sequence<beans::PropertyValue>& aDescriptor, const OUString& sName,
    const OUString& sMediaType, const OUString& sComponentName,
    uno::Sequence<uno::Any>& aArgs, ScMySharedData*& pSharedData)
{
    bool bRet(false);
    uno::Reference<io::XOutputStream> xOut;
    uno::Reference<io::XStream> xStream;

    if ( !xStorage.is() && pMedium )
        xStorage = pMedium->GetOutputStorage();

    if( xStorage.is() )
    {
        // #96807#; trunc stream before use, because it could be an existing stream
        // and the new content could be shorter than the old content. In this case
        // would not all be over written by the new content and the xml file
        // would not be valid.
        xStream = xStorage->openStreamElement( sName, embed::ElementModes::READWRITE | embed::ElementModes::TRUNCATE );
        uno::Reference < beans::XPropertySet > xSet( xStream, uno::UNO_QUERY );
        if (xSet.is())
        {
            xSet->setPropertyValue("MediaType", uno::makeAny(sMediaType));
            OUString aUseCommonPassPropName("UseCommonStoragePasswordEncryption");

            // advise storage impl to use common encryption
            xSet->setPropertyValue( aUseCommonPassPropName, uno::makeAny(sal_True) );
        }

        xOut = xStream->getOutputStream();
    }

    // set Base URL
    uno::Reference< beans::XPropertySet > xInfoSet;
    if( aArgs.getLength() > 0 )
        aArgs.getConstArray()[0] >>= xInfoSet;
    OSL_ENSURE( xInfoSet.is(), "missing property set" );
    if( xInfoSet.is() )
    {
        OUString sPropName("StreamName");
        xInfoSet->setPropertyValue( sPropName, uno::makeAny( sName ) );
    }

    xWriter->setOutputStream( xOut );

    uno::Reference<document::XFilter> xFilter(
        xContext->getServiceManager()->createInstanceWithArgumentsAndContext(
            sComponentName , aArgs, xContext ),
        uno::UNO_QUERY );
    OSL_ENSURE( xFilter.is(), "can't get exporter" );
    uno::Reference<document::XExporter> xExporter( xFilter, uno::UNO_QUERY );
    uno::Reference<lang::XComponent> xComponent( xModel, uno::UNO_QUERY );
    if (xExporter.is())
        xExporter->setSourceDocument( xComponent );

    if ( xFilter.is() )
    {
        ScXMLExport* pExport = static_cast<ScXMLExport*>(SvXMLExport::getImplementation(xFilter));
        pExport->SetSharedData(pSharedData);

        // if there are sheets to copy, get the source stream
        if ( sName == "content.xml" && lcl_HasValidStream(rDoc) && ( pExport->getExportFlags() & EXPORT_OASIS ) )
        {
            // old stream is still in this file's storage - open read-only

            // #i106854# use the document's storage directly, without a temporary SfxMedium
            uno::Reference<embed::XStorage> xTmpStorage = rDoc.GetDocumentShell()->GetStorage();
            uno::Reference<io::XStream> xSrcStream;
            uno::Reference<io::XInputStream> xSrcInput;

            // #i108978# If an embedded object is saved and no events are notified, don't use the stream
            // because without the ...DONE events, stream positions aren't updated.
            ScSheetSaveData* pSheetData = ScModelObj::getImplementation(xModel)->GetSheetSaveData();
            if (pSheetData && pSheetData->IsInSupportedSave())
            {
                try
                {
                    if (xTmpStorage.is())
                        xSrcStream = xTmpStorage->openStreamElement( sName, embed::ElementModes::READ );
                    if (xSrcStream.is())
                        xSrcInput = xSrcStream->getInputStream();
                }
                catch ( const uno::Exception&)
                {
                    // stream not available (for example, password protected) - save normally (xSrcInput is null)
                }
            }

            pExport->SetSourceStream( xSrcInput );
            bRet = xFilter->filter( aDescriptor );
            pExport->SetSourceStream( uno::Reference<io::XInputStream>() );

            // If there was an error, reset all stream flags, so the next save attempt will use normal saving.
            // #i110692# For embedded objects, the stream may be unavailable for one save operation (m_pAntiImpl)
            // and become available again later. But after saving normally once, the stream positions aren't
            // valid anymore, so the flags also have to be reset if the stream wasn't available.
            if ( !bRet || !xSrcInput.is() )
            {
                SCTAB nTabCount = rDoc.GetTableCount();
                for (SCTAB nTab=0; nTab<nTabCount; nTab++)
                    if (rDoc.IsStreamValid(nTab))
                        rDoc.SetStreamValid(nTab, false);
            }
        }
        else
            bRet = xFilter->filter( aDescriptor );

        pSharedData = pExport->GetSharedData();
    }

    return bRet;
}

bool ScXMLImportWrapper::Export(bool bStylesOnly)
{
    rDoc.CreateAllNoteCaptions();

    uno::Reference<uno::XComponentContext> xContext(comphelper::getProcessComponentContext());

    uno::Reference<xml::sax::XWriter> xWriter = xml::sax::Writer::create(xContext);

    if ( !xStorage.is() && pMedium )
        xStorage = pMedium->GetOutputStorage();

    OUString sFileName;
    OUString sTextMediaType("text/xml");
    if (pMedium)
        sFileName = pMedium->GetName();
    SfxObjectShell* pObjSh = rDoc.GetDocumentShell();
    uno::Sequence<beans::PropertyValue> aDescriptor(1);
    beans::PropertyValue* pProps = aDescriptor.getArray();
    pProps[0].Name = "FileName";
    pProps[0].Value <<= sFileName;

    /** property map for export info set */
    comphelper::PropertyMapEntry const aExportInfoMap[] =
    {
        { OUString("ProgressRange"), 0, ::cppu::UnoType<sal_Int32>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("ProgressMax"), 0, ::cppu::UnoType<sal_Int32>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("ProgressCurrent"), 0, ::cppu::UnoType<sal_Int32>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("WrittenNumberStyles"), 0, ::getCppuType((uno::Sequence<sal_Int32>*)0), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("UsePrettyPrinting"), 0, ::cppu::UnoType<sal_Bool>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0},
        { OUString("BaseURI"), 0, ::cppu::UnoType<OUString>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("StreamRelPath"), 0, ::cppu::UnoType<OUString>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("StreamName"), 0, ::cppu::UnoType<OUString>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("StyleNames"), 0, ::getCppuType( (uno::Sequence<OUString>*)0 ), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("StyleFamilies"), 0, ::getCppuType( (uno::Sequence<sal_Int32>*)0 ), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString("TargetStorage"), 0, cppu::UnoType<embed::XStorage>::get(), ::com::sun::star::beans::PropertyAttribute::MAYBEVOID, 0 },
        { OUString(), 0, css::uno::Type(), 0, 0 }
    };
    uno::Reference< beans::XPropertySet > xInfoSet( comphelper::GenericPropertySet_CreateInstance( new comphelper::PropertySetInfo( aExportInfoMap ) ) );

    if ( pObjSh && xStorage.is() )
    {
        uno::Reference<frame::XModel> xModel(pObjSh->GetModel());
        uno::Reference<task::XStatusIndicator> xStatusIndicator(GetStatusIndicator());
        sal_Int32 nProgressRange(1000000);
        if(xStatusIndicator.is())
            xStatusIndicator->start(ScGlobal::GetRscString(STR_SAVE_DOC), nProgressRange);
        xInfoSet->setPropertyValue("ProgressRange", uno::makeAny(nProgressRange));

        SvtSaveOptions aSaveOpt;
        bool bUsePrettyPrinting(aSaveOpt.IsPrettyPrinting());
        xInfoSet->setPropertyValue("UsePrettyPrinting", uno::makeAny(bUsePrettyPrinting));

        const OUString sTargetStorage("TargetStorage");
        xInfoSet->setPropertyValue( sTargetStorage, uno::Any( xStorage ) );

        OSL_ENSURE( pMedium, "There is no medium to get MediaDescriptor from!\n" );
        OUString aBaseURL = pMedium ? pMedium->GetBaseURL( true ) : OUString();
        OUString sPropName("BaseURI");
        xInfoSet->setPropertyValue( sPropName, uno::makeAny( aBaseURL ) );

        // TODO/LATER: do not do it for embedded links
        if( SFX_CREATE_MODE_EMBEDDED == pObjSh->GetCreateMode() )
        {
            OUString aName("dummyObjectName");
            if ( pMedium && pMedium->GetItemSet() )
            {
                const SfxStringItem* pDocHierarchItem = static_cast<const SfxStringItem*>(
                    pMedium->GetItemSet()->GetItem(SID_DOC_HIERARCHICALNAME) );
                if ( pDocHierarchItem )
                    aName = pDocHierarchItem->GetValue();
            }

            if( !aName.isEmpty() )
            {
                sPropName = "StreamRelPath";
                xInfoSet->setPropertyValue( sPropName, uno::makeAny( aName ) );
            }
        }

        bool bMetaRet(pObjSh->GetCreateMode() == SFX_CREATE_MODE_EMBEDDED);
        bool bStylesRet (false);
        bool bDocRet(false);
        bool bSettingsRet(false);
        ScMySharedData* pSharedData = NULL;

        bool bOasis = ( SotStorage::GetVersion( xStorage ) > SOFFICE_FILEFORMAT_60 );

        // RDF metadata: ODF >= 1.2
        if ( !bStylesOnly && bOasis )
        {
            const uno::Reference< beans::XPropertySet > xPropSet( xStorage, uno::UNO_QUERY_THROW );
            try
            {
                OUString aVersion;
                if (( xPropSet->getPropertyValue(
                    OUString("Version")) >>= aVersion )
                    && !aVersion.equals(ODFVER_010_TEXT)
                    && !aVersion.equals(ODFVER_011_TEXT) )
                {
                    const uno::Reference< rdf::XDocumentMetadataAccess > xDMA(
                        xModel, uno::UNO_QUERY_THROW );
                    xDMA->storeMetadataToStorage( xStorage );
                }
            }
            catch ( const beans::UnknownPropertyException &)
            {
            }
            catch ( const uno::Exception &)
            {
            }
        }

        // meta export
        if (!bStylesOnly && !bMetaRet)
        {
            uno::Sequence<uno::Any> aMetaArgs(3);
            uno::Any* pMetaArgs = aMetaArgs.getArray();
            pMetaArgs[0] <<= xInfoSet;
            pMetaArgs[1] <<= xWriter;
            pMetaArgs[2] <<= xStatusIndicator;

            SAL_INFO( "sc.filter", "meta export start" );

            bMetaRet = ExportToComponent(xContext, xModel, xWriter, aDescriptor,
                OUString("meta.xml"),
                sTextMediaType,
                bOasis ? OUString("com.sun.star.comp.Calc.XMLOasisMetaExporter")
                       : OUString("com.sun.star.comp.Calc.XMLMetaExporter"),
                aMetaArgs, pSharedData);

            SAL_INFO( "sc.filter", "meta export end" );
        }

        uno::Reference< document::XEmbeddedObjectResolver > xObjectResolver;
        SvXMLEmbeddedObjectHelper *pObjectHelper = 0;

        uno::Reference< document::XGraphicObjectResolver > xGrfContainer;
        SvXMLGraphicHelper* pGraphicHelper = 0;

        if( xStorage.is() )
        {
            pGraphicHelper = SvXMLGraphicHelper::Create( xStorage, GRAPHICHELPER_MODE_WRITE, false );
            xGrfContainer = pGraphicHelper;
        }

        if( pObjSh )
        {
            pObjectHelper = SvXMLEmbeddedObjectHelper::Create( xStorage, *pObjSh, EMBEDDEDOBJECTHELPER_MODE_WRITE, false );
            xObjectResolver = pObjectHelper;
        }

        // styles export

        {
            uno::Sequence<uno::Any> aStylesArgs(5);
            uno::Any* pStylesArgs = aStylesArgs.getArray();
            pStylesArgs[0] <<= xInfoSet;
            pStylesArgs[1] <<= xGrfContainer;
            pStylesArgs[2] <<= xStatusIndicator;
            pStylesArgs[3] <<= xWriter;
            pStylesArgs[4] <<= xObjectResolver;

            SAL_INFO( "sc.filter", "styles export start" );

            bStylesRet = ExportToComponent(xContext, xModel, xWriter, aDescriptor,
                OUString("styles.xml"),
                sTextMediaType,
                bOasis ? OUString("com.sun.star.comp.Calc.XMLOasisStylesExporter")
                       : OUString("com.sun.star.comp.Calc.XMLStylesExporter"),
                aStylesArgs, pSharedData);

            SAL_INFO( "sc.filter", "styles export end" );
        }

        // content export

        if (!bStylesOnly)
        {
            uno::Sequence<uno::Any> aDocArgs(5);
            uno::Any* pDocArgs = aDocArgs.getArray();
            pDocArgs[0] <<= xInfoSet;
            pDocArgs[1] <<= xGrfContainer;
            pDocArgs[2] <<= xStatusIndicator;
            pDocArgs[3] <<= xWriter;
            pDocArgs[4] <<= xObjectResolver;

            SAL_INFO( "sc.filter", "content export start" );

            bDocRet = ExportToComponent(xContext, xModel, xWriter, aDescriptor,
                OUString("content.xml"),
                sTextMediaType,
                bOasis ? OUString("com.sun.star.comp.Calc.XMLOasisContentExporter")
                       : OUString("com.sun.star.comp.Calc.XMLContentExporter"),
                aDocArgs, pSharedData);

            SAL_INFO( "sc.filter", "content export end" );
        }

        if( pGraphicHelper )
            SvXMLGraphicHelper::Destroy( pGraphicHelper );

        if( pObjectHelper )
            SvXMLEmbeddedObjectHelper::Destroy( pObjectHelper );

        // settings export

        if (!bStylesOnly)
        {
            uno::Sequence<uno::Any> aSettingsArgs(3);
            uno::Any* pSettingsArgs = aSettingsArgs.getArray();
            pSettingsArgs[0] <<= xInfoSet;
            pSettingsArgs[1] <<= xWriter;
            pSettingsArgs[2] <<= xStatusIndicator;

            SAL_INFO( "sc.filter", "settings export start" );

            bSettingsRet = ExportToComponent(xContext, xModel, xWriter, aDescriptor,
                OUString("settings.xml"),
                sTextMediaType,
                bOasis ? OUString("com.sun.star.comp.Calc.XMLOasisSettingsExporter")
                       : OUString("com.sun.star.comp.Calc.XMLSettingsExporter"),
                aSettingsArgs, pSharedData);

            SAL_INFO( "sc.filter", "settings export end" );
        }

        if (pSharedData)
            delete pSharedData;

        if (xStatusIndicator.is())
            xStatusIndicator->end();
        return bStylesRet && ((!bStylesOnly && bDocRet && bMetaRet && bSettingsRet) || bStylesOnly);
    }

    // later: give string descriptor as parameter for doc type

    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
