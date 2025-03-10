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

#include "workbookfragment.hxx"

#include <com/sun/star/table/CellAddress.hpp>
#include <oox/core/filterbase.hxx>
#include <oox/drawingml/themefragmenthandler.hxx>
#include <oox/helper/attributelist.hxx>
#include <oox/helper/progressbar.hxx>
#include <oox/helper/propertyset.hxx>
#include <oox/ole/olestorage.hxx>

#include "biffinputstream.hxx"
#include "chartsheetfragment.hxx"
#include "connectionsfragment.hxx"
#include "externallinkbuffer.hxx"
#include "externallinkfragment.hxx"
#include "formulabuffer.hxx"
#include "pivotcachebuffer.hxx"
#include "sharedstringsbuffer.hxx"
#include "sharedstringsfragment.hxx"
#include "revisionfragment.hxx"
#include "stylesfragment.hxx"
#include "tablebuffer.hxx"
#include "themebuffer.hxx"
#include "viewsettings.hxx"
#include "workbooksettings.hxx"
#include "worksheetbuffer.hxx"
#include "worksheethelper.hxx"
#include "worksheetfragment.hxx"
#include "sheetdatacontext.hxx"
#include "threadpool.hxx"
#include "officecfg/Office/Common.hxx"

#include "document.hxx"
#include "docsh.hxx"
#include "calcconfig.hxx"

#include <vcl/svapp.hxx>
#include <vcl/timer.hxx>

#include <oox/core/fastparser.hxx>
#include <salhelper/thread.hxx>
#include <osl/conditn.hxx>

#include <queue>
#include <boost/scoped_ptr.hpp>

#include <oox/ole/vbaproject.hxx>

namespace oox {
namespace xls {

using namespace ::com::sun::star::io;
using namespace ::com::sun::star::table;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::sheet;
using namespace ::oox::core;

using ::oox::drawingml::ThemeFragmentHandler;

namespace {

const double PROGRESS_LENGTH_GLOBALS        = 0.1;      /// 10% of progress bar for globals import.

} // namespace

WorkbookFragment::WorkbookFragment( const WorkbookHelper& rHelper, const OUString& rFragmentPath ) :
    WorkbookFragmentBase( rHelper, rFragmentPath )
{
}

ContextHandlerRef WorkbookFragment::onCreateContext( sal_Int32 nElement, const AttributeList& rAttribs )
{
    switch( getCurrentElement() )
    {
        case XML_ROOT_CONTEXT:
            if( nElement == XLS_TOKEN( workbook ) ) return this;
        break;

        case XLS_TOKEN( workbook ):
            switch( nElement )
            {
                case XLS_TOKEN( sheets ):
                case XLS_TOKEN( bookViews ):
                case XLS_TOKEN( externalReferences ):
                case XLS_TOKEN( definedNames ):
                case XLS_TOKEN( pivotCaches ):          return this;

                case XLS_TOKEN( fileSharing ):          getWorkbookSettings().importFileSharing( rAttribs );    break;
                case XLS_TOKEN( workbookPr ):           getWorkbookSettings().importWorkbookPr( rAttribs );     break;
                case XLS_TOKEN( calcPr ):               getWorkbookSettings().importCalcPr( rAttribs );         break;
                case XLS_TOKEN( oleSize ):              getViewSettings().importOleSize( rAttribs );            break;
            }
        break;

        case XLS_TOKEN( sheets ):
            if( nElement == XLS_TOKEN( sheet ) ) getWorksheets().importSheet( rAttribs );
        break;
        case XLS_TOKEN( bookViews ):
            if( nElement == XLS_TOKEN( workbookView ) ) getViewSettings().importWorkbookView( rAttribs );
        break;
        case XLS_TOKEN( externalReferences ):
            if( nElement == XLS_TOKEN( externalReference ) ) importExternalReference( rAttribs );
        break;
        case XLS_TOKEN( definedNames ):
            if( nElement == XLS_TOKEN( definedName ) ) { importDefinedName( rAttribs ); return this; } // collect formula
        break;
        case XLS_TOKEN( pivotCaches ):
            if( nElement == XLS_TOKEN( pivotCache ) ) importPivotCache( rAttribs );
        break;
    }
    return 0;
}

void WorkbookFragment::onCharacters( const OUString& rChars )
{
    if( isCurrentElement( XLS_TOKEN( definedName ) ) && mxCurrName.get() )
        mxCurrName->setFormula( rChars );
}

ContextHandlerRef WorkbookFragment::onCreateRecordContext( sal_Int32 nRecId, SequenceInputStream& rStrm )
{
    switch( getCurrentElement() )
    {
        case XML_ROOT_CONTEXT:
            if( nRecId == BIFF12_ID_WORKBOOK ) return this;
        break;

        case BIFF12_ID_WORKBOOK:
            switch( nRecId )
            {
                case BIFF12_ID_SHEETS:
                case BIFF12_ID_BOOKVIEWS:
                case BIFF12_ID_EXTERNALREFS:
                case BIFF12_ID_PIVOTCACHES:     return this;

                case BIFF12_ID_FILESHARING:     getWorkbookSettings().importFileSharing( rStrm );   break;
                case BIFF12_ID_WORKBOOKPR:      getWorkbookSettings().importWorkbookPr( rStrm );    break;
                case BIFF12_ID_CALCPR:          getWorkbookSettings().importCalcPr( rStrm );        break;
                case BIFF12_ID_OLESIZE:         getViewSettings().importOleSize( rStrm );           break;
                case BIFF12_ID_DEFINEDNAME:     getDefinedNames().importDefinedName( rStrm );       break;
            }
        break;

        case BIFF12_ID_SHEETS:
            if( nRecId == BIFF12_ID_SHEET ) getWorksheets().importSheet( rStrm );
        break;
        case BIFF12_ID_BOOKVIEWS:
            if( nRecId == BIFF12_ID_WORKBOOKVIEW ) getViewSettings().importWorkbookView( rStrm );
        break;

        case BIFF12_ID_EXTERNALREFS:
            switch( nRecId )
            {
                case BIFF12_ID_EXTERNALREF:     importExternalRef( rStrm );                         break;
                case BIFF12_ID_EXTERNALSELF:    getExternalLinks().importExternalSelf( rStrm );     break;
                case BIFF12_ID_EXTERNALSAME:    getExternalLinks().importExternalSame( rStrm );     break;
                case BIFF12_ID_EXTERNALADDIN:   getExternalLinks().importExternalAddin( rStrm );    break;
                case BIFF12_ID_EXTERNALSHEETS:  getExternalLinks().importExternalSheets( rStrm );   break;
            }
        break;

        case BIFF12_ID_PIVOTCACHES:
            if( nRecId == BIFF12_ID_PIVOTCACHE ) importPivotCache( rStrm );
    }
    return 0;
}

const RecordInfo* WorkbookFragment::getRecordInfos() const
{
    static const RecordInfo spRecInfos[] =
    {
        { BIFF12_ID_BOOKVIEWS,      BIFF12_ID_BOOKVIEWS + 1         },
        { BIFF12_ID_EXTERNALREFS,   BIFF12_ID_EXTERNALREFS + 1      },
        { BIFF12_ID_FUNCTIONGROUPS, BIFF12_ID_FUNCTIONGROUPS + 2    },
        { BIFF12_ID_PIVOTCACHE,     BIFF12_ID_PIVOTCACHE + 1        },
        { BIFF12_ID_PIVOTCACHES,    BIFF12_ID_PIVOTCACHES + 1       },
        { BIFF12_ID_SHEETS,         BIFF12_ID_SHEETS + 1            },
        { BIFF12_ID_WORKBOOK,       BIFF12_ID_WORKBOOK + 1          },
        { -1,                       -1                              }
    };
    return spRecInfos;
}

namespace {

typedef std::pair<WorksheetGlobalsRef, FragmentHandlerRef> SheetFragmentHandler;
typedef std::vector<SheetFragmentHandler> SheetFragmentVector;

class WorkerThread : public ThreadTask
{
    sal_Int32 &mrSheetsLeft;
    WorkbookFragment& mrWorkbookHandler;
    rtl::Reference<FragmentHandler> mxHandler;

public:
    WorkerThread( WorkbookFragment& rWorkbookHandler,
                  const rtl::Reference<FragmentHandler>& xHandler,
                  sal_Int32 &rSheetsLeft ) :
        mrSheetsLeft( rSheetsLeft ),
        mrWorkbookHandler( rWorkbookHandler ),
        mxHandler( xHandler )
    {
    }

    virtual void doWork() SAL_OVERRIDE
    {
        // We hold the solar mutex in all threads except for
        // the small safe section of the inner loop in
        // sheetdatacontext.cxx
        SAL_INFO( "sc.filter",  "start wait on solar\n" );
        SolarMutexGuard maGuard;
        SAL_INFO( "sc.filter",  "got solar\n" );

        boost::scoped_ptr<oox::core::FastParser> xParser(
                mrWorkbookHandler.getOoxFilter().createParser() );

        SAL_INFO( "sc.filter",  "start import\n" );
        mrWorkbookHandler.importOoxFragment( mxHandler, *xParser );
        SAL_INFO( "sc.filter",  "end import, release solar\n" );
        mrSheetsLeft--;
        assert( mrSheetsLeft >= 0 );
        if( mrSheetsLeft == 0 )
            Application::EndYield();
    }
};

class ProgressBarTimer : Timer
{
    // FIXME: really we should unify all sheet loading
    // progress reporting into something pleasant.
    class ProgressWrapper : public ISegmentProgressBar
    {
        double mfPosition;
        ISegmentProgressBarRef mxWrapped;
    public:
        ProgressWrapper(const ISegmentProgressBarRef &xRef)
            : mfPosition(0.0)
            , mxWrapped(xRef)
        {
        }
        virtual ~ProgressWrapper() {}
        // IProgressBar
        virtual double getPosition() const SAL_OVERRIDE { return mfPosition; }
        virtual void   setPosition( double fPosition ) SAL_OVERRIDE { mfPosition = fPosition; }
        // ISegmentProgressBar
        virtual double getFreeLength() const SAL_OVERRIDE { return 0.0; }
        virtual ISegmentProgressBarRef createSegment( double /* fLength */ ) SAL_OVERRIDE
        {
            return ISegmentProgressBarRef();
        }
        void UpdateBar()
        {
            mxWrapped->setPosition( mfPosition );
        }
    };
    std::vector< ISegmentProgressBarRef > aSegments;
public:
    ProgressBarTimer() : Timer()
    {
        SetTimeout( 500 );
    }
    virtual ~ProgressBarTimer()
    {
        aSegments.clear();
    }
    ISegmentProgressBarRef wrapProgress( const ISegmentProgressBarRef &xProgress )
    {
        aSegments.push_back( ISegmentProgressBarRef( new ProgressWrapper( xProgress ) ) );
        return aSegments.back();
    }
    virtual void Timeout() SAL_OVERRIDE
    {
        for( size_t i = 0; i < aSegments.size(); i++)
            static_cast< ProgressWrapper *>( aSegments[ i ].get() )->UpdateBar();
    }
};

void importSheetFragments( WorkbookFragment& rWorkbookHandler, SheetFragmentVector& rSheets )
{
    sal_Int32 nThreads = std::min( rSheets.size(), (size_t) 4 /* FIXME: ncpus/2 */ );

    Reference< XComponentContext > xContext = comphelper::getProcessComponentContext();

    // Force threading off unless experimental mode or env. var is set.
    if( !officecfg::Office::Common::Misc::ExperimentalMode::get( xContext ) )
        nThreads = 0;

    const char *pEnv;
    if( ( pEnv = getenv( "SC_IMPORT_THREADS" ) ) )
        nThreads = rtl_str_toInt32( pEnv, 10 );

    if( nThreads != 0 )
    {
        // test sequential read in this mode
        if( nThreads < 0)
            nThreads = 0;
        ThreadPool aPool( nThreads );

        sal_Int32 nSheetsLeft = 0;
        ProgressBarTimer aProgressUpdater;
        SheetFragmentVector::iterator it = rSheets.begin(), itEnd = rSheets.end();
        for( ; it != itEnd; ++it )
        {
            // getting at the WorksheetGlobals is rather unpleasant
            IWorksheetProgress *pProgress = WorksheetHelper::getWorksheetInterface( it->first );
            pProgress->setCustomRowProgress(
                        aProgressUpdater.wrapProgress(
                                pProgress->getRowProgress() ) );
            aPool.pushTask( new WorkerThread( rWorkbookHandler, it->second,
                                              /* ref */ nSheetsLeft ) );
            nSheetsLeft++;
        }

        while( nSheetsLeft > 0)
        {
            // This is a much more controlled re-enterancy hazard than
            // allowing a yield deeper inside the filter code for progress
            // bar updating.
            Application::Yield();
        }
        // join all the threads:
        aPool.waitUntilWorkersDone();
    }
    else
    {
        SheetFragmentVector::iterator it = rSheets.begin(), itEnd = rSheets.end();
        for( ; it != itEnd; ++it )
            rWorkbookHandler.importOoxFragment( it->second );
    }
}

}

void WorkbookFragment::finalizeImport()
{
    ISegmentProgressBarRef xGlobalSegment = getProgressBar().createSegment( PROGRESS_LENGTH_GLOBALS );

    // read the theme substream
    OUString aThemeFragmentPath = getFragmentPathFromFirstTypeFromOfficeDoc( "theme" );
    if( !aThemeFragmentPath.isEmpty() )
        importOoxFragment( new ThemeFragmentHandler( getFilter(), aThemeFragmentPath, getTheme() ) );
    xGlobalSegment->setPosition( 0.25 );

    // read the styles substream (requires finalized theme buffer)
    OUString aStylesFragmentPath = getFragmentPathFromFirstTypeFromOfficeDoc( "styles" );
    if( !aStylesFragmentPath.isEmpty() )
        importOoxFragment( new StylesFragment( *this, aStylesFragmentPath ) );
    xGlobalSegment->setPosition( 0.5 );

    // read the shared string table substream (requires finalized styles buffer)
    OUString aSstFragmentPath = getFragmentPathFromFirstTypeFromOfficeDoc( "sharedStrings" );
    if( !aSstFragmentPath.isEmpty() )
        if (!importOoxFragment( new SharedStringsFragment( *this, aSstFragmentPath ) ))
            importOoxFragment(new SharedStringsFragment(*this, aSstFragmentPath.replaceFirst("sharedStrings","SharedStrings")));
    xGlobalSegment->setPosition( 0.75 );

    // read the connections substream
    OUString aConnFragmentPath = getFragmentPathFromFirstTypeFromOfficeDoc( "connections" );
    if( !aConnFragmentPath.isEmpty() )
        importOoxFragment( new ConnectionsFragment( *this, aConnFragmentPath ) );
    xGlobalSegment->setPosition( 1.0 );

    /*  Create fragments for all sheets, before importing them. Needed to do
        some preprocessing in the fragment constructors, e.g. loading the table
        fragments for all sheets that are needed before the cell formulas are
        loaded. Additionally, the instances of the WorkbookGlobals structures
        have to be stored for every sheet. */
    SheetFragmentVector aSheetFragments;
    std::vector<WorksheetHelper*> maHelpers;
    WorksheetBuffer& rWorksheets = getWorksheets();
    sal_Int32 nWorksheetCount = rWorksheets.getWorksheetCount();
    for( sal_Int32 nWorksheet = 0; nWorksheet < nWorksheetCount; ++nWorksheet )
    {
        sal_Int16 nCalcSheet = rWorksheets.getCalcSheetIndex( nWorksheet );
        const Relation* pRelation = getRelations().getRelationFromRelId( rWorksheets.getWorksheetRelId( nWorksheet ) );
        if( (nCalcSheet >= 0) && pRelation )
        {
            // get fragment path of the sheet
            OUString aFragmentPath = getFragmentPathFromRelation( *pRelation );
            OSL_ENSURE( !aFragmentPath.isEmpty(), "WorkbookFragment::finalizeImport - cannot access sheet fragment" );
            if( !aFragmentPath.isEmpty() )
            {
                // leave space for formula processing ( calcuate the segments as
                // if there is an extra sheet )
                double fSegmentLength = getProgressBar().getFreeLength() / (nWorksheetCount - ( nWorksheet - 1) );
                ISegmentProgressBarRef xSheetSegment = getProgressBar().createSegment( fSegmentLength );

                // get the sheet type according to the relations type
                WorksheetType eSheetType = SHEETTYPE_EMPTYSHEET;
                if( pRelation->maType == CREATE_OFFICEDOC_RELATION_TYPE( "worksheet" ) ||
                        pRelation->maType == CREATE_OFFICEDOC_RELATION_TYPE_STRICT( "worksheet" ))
                    eSheetType = SHEETTYPE_WORKSHEET;
                else if( pRelation->maType == CREATE_OFFICEDOC_RELATION_TYPE( "chartsheet" ) ||
                        pRelation->maType == CREATE_OFFICEDOC_RELATION_TYPE_STRICT( "chartsheet" ))
                    eSheetType = SHEETTYPE_CHARTSHEET;
                else if( (pRelation->maType == CREATE_MSOFFICE_RELATION_TYPE( "xlMacrosheet" )) ||
                         (pRelation->maType == CREATE_MSOFFICE_RELATION_TYPE( "xlIntlMacrosheet" )) )
                    eSheetType = SHEETTYPE_MACROSHEET;
                else if( pRelation->maType == CREATE_OFFICEDOC_RELATION_TYPE( "dialogsheet" ) ||
                        pRelation->maType == CREATE_OFFICEDOC_RELATION_TYPE_STRICT(" dialogsheet" ))
                    eSheetType = SHEETTYPE_DIALOGSHEET;
                OSL_ENSURE( eSheetType != SHEETTYPE_EMPTYSHEET, "WorkbookFragment::finalizeImport - unknown sheet type" );
                if( eSheetType != SHEETTYPE_EMPTYSHEET )
                {
                    // create the WorksheetGlobals object
                    WorksheetGlobalsRef xSheetGlob = WorksheetHelper::constructGlobals( *this, xSheetSegment, eSheetType, nCalcSheet );
                    OSL_ENSURE( xSheetGlob.get(), "WorkbookFragment::finalizeImport - missing sheet in document" );
                    if( xSheetGlob.get() )
                    {
                        // create the sheet fragment handler
                        ::rtl::Reference< WorksheetFragmentBase > xFragment;
                        switch( eSheetType )
                        {
                            case SHEETTYPE_WORKSHEET:
                            case SHEETTYPE_MACROSHEET:
                            case SHEETTYPE_DIALOGSHEET:
                                xFragment.set( new WorksheetFragment( *xSheetGlob, aFragmentPath ) );
                            break;
                            case SHEETTYPE_CHARTSHEET:
                                xFragment.set( new ChartsheetFragment( *xSheetGlob, aFragmentPath ) );
                            break;
                            case SHEETTYPE_EMPTYSHEET:
                            case SHEETTYPE_MODULESHEET:
                                break;
                        }

                        // insert the fragment into the map
                        if( xFragment.is() )
                        {
                            aSheetFragments.push_back( SheetFragmentHandler( xSheetGlob, xFragment.get() ) );
                            maHelpers.push_back(xFragment.get());
                        }
                    }
                }
            }
        }
    }

    // setup structure sizes for the number of sheets
    getFormulaBuffer().SetSheetCount( aSheetFragments.size() );

    // create all defined names and database ranges
    getDefinedNames().finalizeImport();
    getTables().finalizeImport();
    // open the VBA project storage
    OUString aVbaFragmentPath = getFragmentPathFromFirstType( CREATE_MSOFFICE_RELATION_TYPE( "vbaProject" ) );
    if( !aVbaFragmentPath.isEmpty() )
    {
        Reference< XInputStream > xInStrm = getBaseFilter().openInputStream( aVbaFragmentPath );
        if( xInStrm.is() )
        {
            StorageRef xPrjStrg( new ::oox::ole::OleStorage( getBaseFilter().getComponentContext(), xInStrm, false ) );
            setVbaProjectStorage( xPrjStrg );
            getBaseFilter().getVbaProject().readVbaModules( *xPrjStrg );
        }
    }

    // load all worksheets
    importSheetFragments(*this, aSheetFragments);

    for( std::vector<WorksheetHelper*>::iterator aIt = maHelpers.begin(), aEnd = maHelpers.end(); aIt != aEnd; ++aIt )
    {
        (*aIt)->finalizeDrawingImport();
    }

    for( SheetFragmentVector::iterator aIt = aSheetFragments.begin(), aEnd = aSheetFragments.end(); aIt != aEnd; ++aIt )
    {
        // delete fragment object and WorkbookGlobals object, will free all allocated sheet buffers
        aIt->second.clear();
        aIt->first.reset();
    }

    // final conversions, e.g. calculation settings and view settings
    finalizeWorkbookImport();

    OUString aRevHeadersPath = getFragmentPathFromFirstType(CREATE_OFFICEDOC_RELATION_TYPE("revisionHeaders"));
    if (!aRevHeadersPath.isEmpty())
    {
        boost::scoped_ptr<oox::core::FastParser> xParser(getOoxFilter().createParser());
        rtl::Reference<oox::core::FragmentHandler> xFragment(new RevisionHeadersFragment(*this, aRevHeadersPath));
        importOoxFragment(xFragment, *xParser);
    }
}

// private --------------------------------------------------------------------

void WorkbookFragment::importExternalReference( const AttributeList& rAttribs )
{
    if( ExternalLink* pExtLink = getExternalLinks().importExternalReference( rAttribs ).get() )
        importExternalLinkFragment( *pExtLink );
}

void WorkbookFragment::importDefinedName( const AttributeList& rAttribs )
{
    mxCurrName = getDefinedNames().importDefinedName( rAttribs );
}

void WorkbookFragment::importPivotCache( const AttributeList& rAttribs )
{
    sal_Int32 nCacheId = rAttribs.getInteger( XML_cacheId, -1 );
    OUString aRelId = rAttribs.getString( R_TOKEN( id ), OUString() );
    importPivotCacheDefFragment( aRelId, nCacheId );
}

void WorkbookFragment::importExternalRef( SequenceInputStream& rStrm )
{
    if( ExternalLink* pExtLink = getExternalLinks().importExternalRef( rStrm ).get() )
        importExternalLinkFragment( *pExtLink );
}

void WorkbookFragment::importPivotCache( SequenceInputStream& rStrm )
{
    sal_Int32 nCacheId = rStrm.readInt32();
    OUString aRelId = BiffHelper::readString( rStrm );
    importPivotCacheDefFragment( aRelId, nCacheId );
}

void WorkbookFragment::importExternalLinkFragment( ExternalLink& rExtLink )
{
    OUString aFragmentPath = getFragmentPathFromRelId( rExtLink.getRelId() );
    if( !aFragmentPath.isEmpty() )
        importOoxFragment( new ExternalLinkFragment( *this, aFragmentPath, rExtLink ) );
}

void WorkbookFragment::importPivotCacheDefFragment( const OUString& rRelId, sal_Int32 nCacheId )
{
    // pivot caches will be imported on demand, here we just store the fragment path in the buffer
    getPivotCaches().registerPivotCacheFragment( nCacheId, getFragmentPathFromRelId( rRelId ) );
}

} // namespace xls
} // namespace oox

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
