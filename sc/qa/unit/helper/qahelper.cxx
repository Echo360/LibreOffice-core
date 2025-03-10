/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "qahelper.hxx"
#include "csv_handler.hxx"
#include "drwlayer.hxx"
#include "compiler.hxx"
#include "conditio.hxx"
#include "stlsheet.hxx"
#include "formulacell.hxx"
#include <svx/svdpage.hxx>
#include <svx/svdoole2.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/justifyitem.hxx>

#include <config_orcus.h>

#if ENABLE_ORCUS
#if defined WNT
#define __ORCUS_STATIC_LIB
#endif
#include <orcus/csv_parser.hpp>
#endif

#include <fstream>

#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/text/textfield/Type.hpp>
#include <com/sun/star/chart2/XChartDocument.hpp>
#include <com/sun/star/chart2/data/XDataReceiver.hpp>

using namespace com::sun::star;
using namespace ::com::sun::star::uno;

// calc data structure pretty printer
std::ostream& operator<<(std::ostream& rStrm, const ScAddress& rAddr)
{
    rStrm << "Col: " << rAddr.Col() << " Row: " << rAddr.Row() << " Tab: " << rAddr.Tab() << "\n";
    return rStrm;
}

std::ostream& operator<<(std::ostream& rStrm, const ScRange& rRange)
{
    rStrm << "ScRange: " << rRange.aStart << rRange.aEnd << "\n";
    return rStrm;
}

std::ostream& operator<<(std::ostream& rStrm, const ScRangeList& rList)
{
    rStrm << "ScRangeList: \n";
    for(size_t i = 0; i < rList.size(); ++i)
        rStrm << *rList[i];
    return rStrm;
}

std::ostream& operator<<(std::ostream& rStrm, const Color& rColor)
{
    rStrm << "Color: R:" << rColor.GetRed() << " G:" << rColor.GetGreen() << " B: << rColor.GetBlue()";
    return rStrm;
}

FileFormat aFileFormats[] = {
    { "ods" , "calc8", "", ODS_FORMAT_TYPE },
    { "xls" , "MS Excel 97", "calc_MS_EXCEL_97", XLS_FORMAT_TYPE },
    { "xlsx", "Calc Office Open XML" , "Office Open XML Spreadsheet", XLSX_FORMAT_TYPE },
    { "xlsm", "Calc Office Open XML" , "Office Open XML Spreadsheet", XLSX_FORMAT_TYPE },
    { "csv" , "Text - txt - csv (StarCalc)", "generic_Text", CSV_FORMAT_TYPE },
    { "html" , "calc_HTML_WebQuery", "generic_HTML", HTML_FORMAT_TYPE },
    { "123" , "Lotus", "calc_Lotus", LOTUS123_FORMAT_TYPE },
    { "dif", "DIF", "calc_DIF", DIF_FORMAT_TYPE },
    { "xml", "MS Excel 2003 XML", "calc_MS_Excel_2003_XML", XLS_XML_FORMAT_TYPE }
};

bool testEqualsWithTolerance( long nVal1, long nVal2, long nTol )
{
    return ( labs( nVal1 - nVal2 ) <= nTol );
}

void loadFile(const OUString& aFileName, std::string& aContent)
{
    OString aOFileName = OUStringToOString(aFileName, RTL_TEXTENCODING_UTF8);

#ifdef ANDROID
    size_t size;
    if (strncmp(aOFileName.getStr(), "/assets/", sizeof("/assets/")-1) == 0) {
        const char *contents = (const char *) lo_apkentry(aOFileName.getStr(), &size);
        if (contents != 0) {
            aContent = std::string(contents, size);
            return;
        }
    }
#endif

    std::ifstream aFile(aOFileName.getStr());

    OStringBuffer aErrorMsg("Could not open csv file: ");
    aErrorMsg.append(aOFileName);
    CPPUNIT_ASSERT_MESSAGE(aErrorMsg.getStr(), aFile);
    std::ostringstream aOStream;
    aOStream << aFile.rdbuf();
    aFile.close();
    aContent = aOStream.str();
}

#if ENABLE_ORCUS

void testFile(OUString& aFileName, ScDocument& rDoc, SCTAB nTab, StringType aStringFormat)
{
    csv_handler aHandler(&rDoc, nTab, aStringFormat);
    orcus::csv::parser_config aConfig;
    aConfig.delimiters.push_back(',');
    aConfig.delimiters.push_back(';');
    aConfig.text_qualifier = '"';
    aConfig.trim_cell_value = false;

    std::string aContent;
    loadFile(aFileName, aContent);
    orcus::csv_parser<csv_handler> parser ( &aContent[0], aContent.size() , aHandler, aConfig);
    try
    {
        parser.parse();
    }
    catch (const orcus::csv::parse_error& e)
    {
        std::cout << "reading csv content file failed: " << e.what() << std::endl;
        OStringBuffer aErrorMsg("csv parser error: ");
        aErrorMsg.append(e.what());
        CPPUNIT_ASSERT_MESSAGE(aErrorMsg.getStr(), false);
    }
}

void testCondFile(OUString& aFileName, ScDocument* pDoc, SCTAB nTab)
{
    conditional_format_handler aHandler(pDoc, nTab);
    orcus::csv::parser_config aConfig;
    aConfig.delimiters.push_back(',');
    aConfig.delimiters.push_back(';');
    aConfig.text_qualifier = '"';
    std::string aContent;
    loadFile(aFileName, aContent);
    orcus::csv_parser<conditional_format_handler> parser ( &aContent[0], aContent.size() , aHandler, aConfig);
    try
    {
        parser.parse();
    }
    catch (const orcus::csv::parse_error& e)
    {
        std::cout << "reading csv content file failed: " << e.what() << std::endl;
        OStringBuffer aErrorMsg("csv parser error: ");
        aErrorMsg.append(e.what());
        CPPUNIT_ASSERT_MESSAGE(aErrorMsg.getStr(), false);
    }
}

#else

void testFile(OUString&, ScDocument*, SCTAB, StringType) {}
void testCondFile(OUString&, ScDocument*, SCTAB) {}

#endif

void testFormats(ScBootstrapFixture* pTest, ScDocument* pDoc, sal_Int32 nFormat)
{
    //test Sheet1 with csv file
    OUString aCSVFileName;
    pTest->createCSVPath(OUString("numberFormat."), aCSVFileName);
    testFile(aCSVFileName, *pDoc, 0, PureString);
    //need to test the color of B3
    //it's not a font color!
    //formatting for B5: # ??/100 gets lost during import

    //test Sheet2
    const ScPatternAttr* pPattern = NULL;
    pPattern = pDoc->GetPattern(0,0,1);
    Font aFont;
    pPattern->GetFont(aFont,SC_AUTOCOL_RAW);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("font size should be 10", 200l, aFont.GetSize().getHeight());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("font color should be black", COL_AUTO, aFont.GetColor().GetColor());
    pPattern = pDoc->GetPattern(0,1,1);
    pPattern->GetFont(aFont, SC_AUTOCOL_RAW);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("font size should be 12", 240l, aFont.GetSize().getHeight());
    pPattern = pDoc->GetPattern(0,2,1);
    pPattern->GetFont(aFont, SC_AUTOCOL_RAW);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("font should be italic", ITALIC_NORMAL, aFont.GetItalic());
    pPattern = pDoc->GetPattern(0,4,1);
    pPattern->GetFont(aFont, SC_AUTOCOL_RAW);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("font should be bold", WEIGHT_BOLD, aFont.GetWeight());
    pPattern = pDoc->GetPattern(1,0,1);
    pPattern->GetFont(aFont, SC_AUTOCOL_RAW);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("font should be blue", COL_BLUE, aFont.GetColor().GetColor());
    pPattern = pDoc->GetPattern(1,1,1);
    pPattern->GetFont(aFont, SC_AUTOCOL_RAW);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("font should be striked out with a single line", STRIKEOUT_SINGLE, aFont.GetStrikeout());
    //some tests on sheet2 only for ods
    if (nFormat == ODS)
    {
        pPattern = pDoc->GetPattern(1,2,1);
        pPattern->GetFont(aFont, SC_AUTOCOL_RAW);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("font should be striked out with a double line", STRIKEOUT_DOUBLE, aFont.GetStrikeout());
        pPattern = pDoc->GetPattern(1,3,1);
        pPattern->GetFont(aFont, SC_AUTOCOL_RAW);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("font should be underlined with a dotted line", UNDERLINE_DOTTED, aFont.GetUnderline());
        //check row height import
        //disable for now until we figure out cause of win tinderboxes test failures
        //CPPUNIT_ASSERT_EQUAL( static_cast<sal_uInt16>(256), pDoc->GetRowHeight(0,1) ); //0.178in
        //CPPUNIT_ASSERT_EQUAL( static_cast<sal_uInt16>(304), pDoc->GetRowHeight(1,1) ); //0.211in
        //CPPUNIT_ASSERT_EQUAL( static_cast<sal_uInt16>(477), pDoc->GetRowHeight(5,1) ); //0.3311in
        //check column width import
        CPPUNIT_ASSERT_EQUAL( static_cast<sal_uInt16>(555), pDoc->GetColWidth(4,1) );  //0.3854in
        CPPUNIT_ASSERT_EQUAL( static_cast<sal_uInt16>(1280), pDoc->GetColWidth(5,1) ); //0.889in
        CPPUNIT_ASSERT_EQUAL( static_cast<sal_uInt16>(4153), pDoc->GetColWidth(6,1) ); //2.8839in
        //test case for i53253 where a cell has text with different styles and space between the text.
        OUString aTestStr = pDoc->GetString(3,0,1);
        OUString aKnownGoodStr("text14 space");
        CPPUNIT_ASSERT_EQUAL( aKnownGoodStr, aTestStr );
        //test case for cell text with line breaks.
        aTestStr = pDoc->GetString(3,5,1);
        aKnownGoodStr = "Hello,\nCalc!";
        CPPUNIT_ASSERT_EQUAL( aKnownGoodStr, aTestStr );
    }
    pPattern = pDoc->GetPattern(1,4,1);
    Color aColor = static_cast<const SvxBrushItem&>(pPattern->GetItem(ATTR_BACKGROUND)).GetColor();
    CPPUNIT_ASSERT_MESSAGE("background color should be green", aColor == COL_LIGHTGREEN);
    pPattern = pDoc->GetPattern(2,0,1);
    SvxCellHorJustify eHorJustify = static_cast<SvxCellHorJustify>(static_cast<const SvxHorJustifyItem&>(pPattern->GetItem(ATTR_HOR_JUSTIFY)).GetValue());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("cell content should be aligned centre horizontally", SVX_HOR_JUSTIFY_CENTER, eHorJustify);
    //test alignment
    pPattern = pDoc->GetPattern(2,1,1);
    eHorJustify = static_cast<SvxCellHorJustify>(static_cast<const SvxHorJustifyItem&>(pPattern->GetItem(ATTR_HOR_JUSTIFY)).GetValue());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("cell content should be aligned right horizontally", SVX_HOR_JUSTIFY_RIGHT, eHorJustify);
    pPattern = pDoc->GetPattern(2,2,1);
    eHorJustify = static_cast<SvxCellHorJustify>(static_cast<const SvxHorJustifyItem&>(pPattern->GetItem(ATTR_HOR_JUSTIFY)).GetValue());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("cell content should be aligned block horizontally", SVX_HOR_JUSTIFY_BLOCK, eHorJustify);

    //test Sheet3 only for ods and xlsx
    if ( nFormat == ODS || nFormat == XLSX )
    {
        pTest->createCSVPath(OUString("conditionalFormatting."), aCSVFileName);
        testCondFile(aCSVFileName, pDoc, 2);
        // test parent cell style import ( fdo#55198 )
        if ( nFormat == XLSX )
        {
            pPattern = pDoc->GetPattern(1,1,3);
            ScStyleSheet* pStyleSheet = (ScStyleSheet*)pPattern->GetStyleSheet();
            // check parent style name
            OUString sExpected("Excel Built-in Date");
            OUString sResult = pStyleSheet->GetName();
            CPPUNIT_ASSERT_EQUAL_MESSAGE("parent style for Sheet4.B2 is 'Excel Built-in Date'", sExpected, sResult);
            // check  align of style
            SfxItemSet& rItemSet = pStyleSheet->GetItemSet();
            eHorJustify = static_cast<SvxCellHorJustify>(static_cast< const SvxHorJustifyItem& >(rItemSet.Get( ATTR_HOR_JUSTIFY ) ).GetValue() );
            CPPUNIT_ASSERT_EQUAL_MESSAGE("'Excel Built-in Date' style should be aligned centre horizontally", SVX_HOR_JUSTIFY_CENTER, eHorJustify);
            // check date format ( should be just month e.g. 29 )
            sResult =pDoc->GetString( 1,1,3 );
            sExpected = "29";
            CPPUNIT_ASSERT_EQUAL_MESSAGE("'Excel Built-in Date' style should just display month", sExpected, sResult );

            // check actual align applied to cell, should be the same as
            // the style
            eHorJustify = static_cast<SvxCellHorJustify>(static_cast< const SvxHorJustifyItem& >(pPattern->GetItem( ATTR_HOR_JUSTIFY ) ).GetValue() );
            CPPUNIT_ASSERT_EQUAL_MESSAGE("cell with 'Excel Built-in Date' style should be aligned centre horizontally", SVX_HOR_JUSTIFY_CENTER, eHorJustify);
        }
    }

    ScConditionalFormat* pCondFormat = pDoc->GetCondFormat(0,0,2);
    const ScRangeList& rRange = pCondFormat->GetRange();
    CPPUNIT_ASSERT(rRange == ScRange(0,0,2,3,0,2));

    pCondFormat = pDoc->GetCondFormat(0,1,2);
    const ScRangeList& rRange2 = pCondFormat->GetRange();
    CPPUNIT_ASSERT(rRange2 == ScRange(0,1,2,0,1,2));

    pCondFormat = pDoc->GetCondFormat(1,1,2);
    const ScRangeList& rRange3 = pCondFormat->GetRange();
    CPPUNIT_ASSERT(rRange3 == ScRange(1,1,2,3,1,2));
}

const SdrOle2Obj* getSingleChartObject(ScDocument& rDoc, sal_uInt16 nPage)
{
    // Retrieve the chart object instance from the 2nd page (for the 2nd sheet).
    ScDrawLayer* pDrawLayer = rDoc.GetDrawLayer();
    if (!pDrawLayer)
    {
        cout << "Failed to retrieve the drawing layer object." << endl;
        return NULL;
    }

    const SdrPage* pPage = pDrawLayer->GetPage(nPage);
    if (!pPage)
    {
        cout << "Failed to retrieve the page object." << endl;
        return NULL;
    }

    if (pPage->GetObjCount() != 1)
    {
        cout << "This page should contain one drawing object." << endl;
        return NULL;
    }

    const SdrObject* pObj = pPage->GetObj(0);
    if (!pObj)
    {
        cout << "Failed to retrieve the drawing object." << endl;
        return NULL;
    }

    if (pObj->GetObjIdentifier() != OBJ_OLE2)
    {
        cout << "This is not an OLE2 object." << endl;
        return NULL;
    }

    const SdrOle2Obj& rOleObj = static_cast<const SdrOle2Obj&>(*pObj);
    if (!rOleObj.IsChart())
    {
        cout << "This should be a chart object." << endl;
        return NULL;
    }

    return &rOleObj;
}

std::vector<OUString> getChartRangeRepresentations(const SdrOle2Obj& rChartObj)
{
    std::vector<OUString> aRangeReps;

    // Make sure the chart object has correct range references.
    Reference<frame::XModel> xModel = rChartObj.getXModel();
    if (!xModel.is())
    {
        cout << "Failed to get the embedded object interface." << endl;
        return aRangeReps;
    }

    Reference<chart2::XChartDocument> xChartDoc(xModel, UNO_QUERY);
    if (!xChartDoc.is())
    {
        cout << "Failed to get the chart document interface." << endl;
        return aRangeReps;
    }

    Reference<chart2::data::XDataSource> xDataSource(xChartDoc, UNO_QUERY);
    if (!xDataSource.is())
    {
        cout << "Failed to get the data source interface." << endl;
        return aRangeReps;
    }

    Sequence<Reference<chart2::data::XLabeledDataSequence> > xDataSeqs = xDataSource->getDataSequences();
    if (!xDataSeqs.getLength())
    {
        cout << "There should be at least one data sequences." << endl;
        return aRangeReps;
    }

    Reference<chart2::data::XDataReceiver> xDataRec(xChartDoc, UNO_QUERY);
    if (!xDataRec.is())
    {
        cout << "Failed to get the data receiver interface." << endl;
        return aRangeReps;
    }

    Sequence<OUString> aRangeRepSeqs = xDataRec->getUsedRangeRepresentations();
    for (sal_Int32 i = 0, n = aRangeRepSeqs.getLength(); i < n; ++i)
        aRangeReps.push_back(aRangeRepSeqs[i]);

    return aRangeReps;
}

ScRangeList getChartRanges(ScDocument& rDoc, const SdrOle2Obj& rChartObj)
{
    std::vector<OUString> aRangeReps = getChartRangeRepresentations(rChartObj);
    ScRangeList aRanges;
    for (size_t i = 0, n = aRangeReps.size(); i < n; ++i)
    {
        ScRange aRange;
        sal_uInt16 nRes = aRange.Parse(aRangeReps[i], &rDoc, rDoc.GetAddressConvention());
        if (nRes & SCA_VALID)
            // This is a range address.
            aRanges.Append(aRange);
        else
        {
            // Parse it as a single cell address.
            ScAddress aAddr;
            nRes = aAddr.Parse(aRangeReps[i], &rDoc, rDoc.GetAddressConvention());
            CPPUNIT_ASSERT_MESSAGE("Failed to parse a range representation.", (nRes & SCA_VALID));
            aRanges.Append(aAddr);
        }
    }

    return aRanges;
}

namespace {

ScTokenArray* getTokens(ScDocument& rDoc, const ScAddress& rPos)
{
    ScFormulaCell* pCell = rDoc.GetFormulaCell(rPos);
    if (!pCell)
    {
        OUString aStr = rPos.Format(SCA_VALID);
        cerr << aStr << " is not a formula cell." << endl;
        return NULL;
    }

    return pCell->GetCode();
}

}

bool checkFormula(ScDocument& rDoc, const ScAddress& rPos, const char* pExpected)
{
    ScTokenArray* pCode = getTokens(rDoc, rPos);
    if (!pCode)
    {
        cerr << "Empty token array." << endl;
        return false;
    }

    OUString aFormula = toString(rDoc, rPos, *pCode, rDoc.GetGrammar());
    if (aFormula != OUString::createFromAscii(pExpected))
    {
        cerr << "Formula '" << pExpected << "' expected, but '" << aFormula << "' found" << endl;
        return false;
    }

    return true;
}

bool checkFormulaPosition(ScDocument& rDoc, const ScAddress& rPos)
{
    OUString aStr(rPos.Format(SCA_VALID));
    const ScFormulaCell* pFC = rDoc.GetFormulaCell(rPos);
    if (!pFC)
    {
        cerr << "Formula cell expected at " << aStr << " but not found." << endl;
        return false;
    }

    if (pFC->aPos != rPos)
    {
        OUString aStr2(pFC->aPos.Format(SCA_VALID));
        cerr << "Formula cell at " << aStr << " has incorrect position of " << aStr2 << endl;
        return false;
    }

    return true;
}

bool checkFormulaPositions(
    ScDocument& rDoc, SCTAB nTab, SCCOL nCol, const SCROW* pRows, size_t nRowCount)
{
    ScAddress aPos(nCol, 0, nTab);
    for (size_t i = 0; i < nRowCount; ++i)
    {
        SCROW nRow = pRows[i];
        aPos.SetRow(nRow);

        if (!checkFormulaPosition(rDoc, aPos))
        {
            OUString aStr(aPos.Format(SCA_VALID));
            cerr << "Formula cell position failed at " << aStr << "." << endl;
            return false;
        }
    }
    return true;
}

ScTokenArray* compileFormula(
    ScDocument* pDoc, const OUString& rFormula, const ScAddress* pPos,
    formula::FormulaGrammar::Grammar eGram )
{
    ScAddress aPos(0,0,0);
    if (pPos)
        aPos = *pPos;
    ScCompiler aComp(pDoc, aPos);
    aComp.SetGrammar(eGram);
    return aComp.CompileString(rFormula);
}

void clearFormulaCellChangedFlag( ScDocument& rDoc, const ScRange& rRange )
{
    const ScAddress& s = rRange.aStart;
    const ScAddress& e = rRange.aEnd;
    for (SCTAB nTab = s.Tab(); nTab <= e.Tab(); ++nTab)
    {
        for (SCCOL nCol = s.Col(); nCol <= e.Col(); ++nCol)
        {
            for (SCROW nRow = s.Row(); nRow <= e.Row(); ++nRow)
            {
                ScAddress aPos(nCol, nRow, nTab);
                ScFormulaCell* pFC = rDoc.GetFormulaCell(aPos);
                if (pFC)
                    pFC->SetChanged(false);
            }
        }
    }
}

bool isFormulaWithoutError(ScDocument& rDoc, const ScAddress& rPos)
{
    ScFormulaCell* pFC = rDoc.GetFormulaCell(rPos);
    if (!pFC)
        return false;

    return pFC->GetErrCode() == 0;
}

OUString toString(
    ScDocument& rDoc, const ScAddress& rPos, ScTokenArray& rArray, formula::FormulaGrammar::Grammar eGram)
{
    ScCompiler aComp(&rDoc, rPos, rArray);
    aComp.SetGrammar(eGram);
    OUStringBuffer aBuf;
    aComp.CreateStringFromTokenArray(aBuf);
    return aBuf.makeStringAndClear();
}

ScDocShellRef ScBootstrapFixture::load( bool bReadWrite,
    const OUString& rURL, const OUString& rFilter, const OUString &rUserData,
    const OUString& rTypeName, unsigned int nFilterFlags, unsigned int nClipboardID,
    sal_uIntPtr nFilterVersion, const OUString* pPassword )
{
    SfxFilter* pFilter = new SfxFilter(
        rFilter,
        OUString(), nFilterFlags, nClipboardID, rTypeName, 0, OUString(),
        rUserData, OUString("private:factory/scalc*"));
    pFilter->SetVersion(nFilterVersion);

    ScDocShellRef xDocShRef = new ScDocShell;
    xDocShRef->GetDocument().EnableUserInteraction(false);
    SfxMedium* pSrcMed = new SfxMedium(rURL, bReadWrite ? STREAM_STD_READWRITE : STREAM_STD_READ );
    pSrcMed->SetFilter(pFilter);
    pSrcMed->UseInteractionHandler(false);
    if (pPassword)
    {
        SfxItemSet* pSet = pSrcMed->GetItemSet();
        pSet->Put(SfxStringItem(SID_PASSWORD, *pPassword));
    }
    SAL_INFO( "sc.qa", "about to load " << rURL );
    if (!xDocShRef->DoLoad(pSrcMed))
    {
        xDocShRef->DoClose();
        // load failed.
        xDocShRef.Clear();
    }

    return xDocShRef;
}

ScDocShellRef ScBootstrapFixture::load(
    const OUString& rURL, const OUString& rFilter, const OUString &rUserData,
    const OUString& rTypeName, unsigned int nFilterFlags, unsigned int nClipboardID,
    sal_uIntPtr nFilterVersion, const OUString* pPassword )
{
    return load( false, rURL, rFilter, rUserData, rTypeName, nFilterFlags, nClipboardID,  nFilterVersion, pPassword );
}

ScDocShellRef ScBootstrapFixture::loadDoc(
    const OUString& rFileName, sal_Int32 nFormat, bool bReadWrite )
{
    OUString aFileExtension(aFileFormats[nFormat].pName, strlen(aFileFormats[nFormat].pName), RTL_TEXTENCODING_UTF8 );
    OUString aFilterName(aFileFormats[nFormat].pFilterName, strlen(aFileFormats[nFormat].pFilterName), RTL_TEXTENCODING_UTF8) ;
    OUString aFileName;
    createFileURL( rFileName, aFileExtension, aFileName );
    OUString aFilterType(aFileFormats[nFormat].pTypeName, strlen(aFileFormats[nFormat].pTypeName), RTL_TEXTENCODING_UTF8);
    unsigned int nFormatType = aFileFormats[nFormat].nFormatType;
    unsigned int nClipboardId = nFormatType ? SFX_FILTER_IMPORT | SFX_FILTER_USESOPTIONS : 0;

    return load(bReadWrite, aFileName, aFilterName, OUString(), aFilterType, nFormatType, nClipboardId, nFormatType);
}

const FileFormat* ScBootstrapFixture::getFileFormats()
{
    return aFileFormats;
}

ScBootstrapFixture::ScBootstrapFixture( const OUString& rsBaseString ) : m_aBaseString( rsBaseString ) {}
ScBootstrapFixture::~ScBootstrapFixture() {}

void ScBootstrapFixture::createFileURL(
    const OUString& aFileBase, const OUString& aFileExtension, OUString& rFilePath)
{
    OUString aSep("/");
    OUStringBuffer aBuffer( getSrcRootURL() );
    aBuffer.append(m_aBaseString).append(aSep).append(aFileExtension);
    aBuffer.append(aSep).append(aFileBase).append(aFileExtension);
    rFilePath = aBuffer.makeStringAndClear();
}

void ScBootstrapFixture::createCSVPath(const OUString& aFileBase, OUString& rCSVPath)
{
    OUStringBuffer aBuffer( getSrcRootPath());
    aBuffer.append(m_aBaseString).append("/contentCSV/");
    aBuffer.append(aFileBase).append("csv");
    rCSVPath = aBuffer.makeStringAndClear();
}

ScDocShellRef ScBootstrapFixture::saveAndReload(
    ScDocShell* pShell, const OUString &rFilter,
    const OUString &rUserData, const OUString& rTypeName, sal_uLong nFormatType)
{

    utl::TempFile aTempFile;
    aTempFile.EnableKillingFile();
    SfxMedium aStoreMedium( aTempFile.GetURL(), STREAM_STD_WRITE );
    sal_uInt32 nExportFormat = 0;
    if (nFormatType == ODS_FORMAT_TYPE)
        nExportFormat = SFX_FILTER_EXPORT | SFX_FILTER_USESOPTIONS;
    SfxFilter* pExportFilter = new SfxFilter(
        rFilter,
        OUString(), nFormatType, nExportFormat, rTypeName, 0, OUString(),
        rUserData, OUString("private:factory/scalc*") );
    pExportFilter->SetVersion(SOFFICE_FILEFORMAT_CURRENT);
    aStoreMedium.SetFilter(pExportFilter);
    pShell->DoSaveAs( aStoreMedium );
    pShell->DoClose();

    //std::cout << "File: " << aTempFile.GetURL() << std::endl;

    sal_uInt32 nFormat = 0;
    if (nFormatType == ODS_FORMAT_TYPE)
        nFormat = SFX_FILTER_IMPORT | SFX_FILTER_USESOPTIONS;

    ScDocShellRef xDocSh = load(aTempFile.GetURL(), rFilter, rUserData, rTypeName, nFormatType, nFormat );
    if(nFormatType == XLSX_FORMAT_TYPE)
        validate(aTempFile.GetFileName(), test::OOXML);
    else if (nFormatType == ODS_FORMAT_TYPE)
        validate(aTempFile.GetFileName(), test::ODF);
    return xDocSh;
}

ScDocShellRef ScBootstrapFixture::saveAndReload( ScDocShell* pShell, sal_Int32 nFormat )
{
    OUString aFilterName(aFileFormats[nFormat].pFilterName, strlen(aFileFormats[nFormat].pFilterName), RTL_TEXTENCODING_UTF8) ;
    OUString aFilterType(aFileFormats[nFormat].pTypeName, strlen(aFileFormats[nFormat].pTypeName), RTL_TEXTENCODING_UTF8);
    ScDocShellRef xDocSh = saveAndReload(pShell, aFilterName, OUString(), aFilterType, aFileFormats[nFormat].nFormatType);

    CPPUNIT_ASSERT(xDocSh.Is());
    return xDocSh;
}

boost::shared_ptr<utl::TempFile> ScBootstrapFixture::exportTo( ScDocShell* pShell, sal_Int32 nFormat )
{
    OUString aFilterName(aFileFormats[nFormat].pFilterName, strlen(aFileFormats[nFormat].pFilterName), RTL_TEXTENCODING_UTF8) ;
    OUString aFilterType(aFileFormats[nFormat].pTypeName, strlen(aFileFormats[nFormat].pTypeName), RTL_TEXTENCODING_UTF8);

    boost::shared_ptr<utl::TempFile> pTempFile(new utl::TempFile());
    pTempFile->EnableKillingFile();
    SfxMedium aStoreMedium( pTempFile->GetURL(), STREAM_STD_WRITE );
    sal_uInt32 nExportFormat = 0;
    sal_Int32 nFormatType = aFileFormats[nFormat].nFormatType;
    if (nFormatType == ODS_FORMAT_TYPE)
        nExportFormat = SFX_FILTER_EXPORT | SFX_FILTER_USESOPTIONS;
    SfxFilter* pExportFilter = new SfxFilter(
        aFilterName,
        OUString(), nFormatType, nExportFormat, aFilterType, 0, OUString(),
        OUString(), OUString("private:factory/scalc*") );
    pExportFilter->SetVersion(SOFFICE_FILEFORMAT_CURRENT);
    aStoreMedium.SetFilter(pExportFilter);
    pShell->DoSaveAs( aStoreMedium );
    pShell->DoClose();

    if(nFormatType == XLSX_FORMAT_TYPE)
        validate(pTempFile->GetFileName(), test::OOXML);
    else if (nFormatType == ODS_FORMAT_TYPE)
        validate(pTempFile->GetFileName(), test::ODF);

    return pTempFile;
}

void ScBootstrapFixture::miscRowHeightsTest( TestParam* aTestValues, unsigned int numElems )
{
    for ( unsigned int index=0; index<numElems; ++index )
    {
        OUString sFileName = OUString::createFromAscii( aTestValues[ index ].sTestDoc );
        SAL_INFO( "sc.qa", "aTestValues[" << index << "] " << sFileName );
        int nImportType =  aTestValues[ index ].nImportType;
        int nExportType =  aTestValues[ index ].nExportType;
        ScDocShellRef xShell = loadDoc( sFileName, nImportType );
        CPPUNIT_ASSERT(xShell.Is());

        if ( nExportType != -1 )
            xShell = saveAndReload(&(*xShell), nExportType );

        CPPUNIT_ASSERT(xShell.Is());

        ScDocument& rDoc = xShell->GetDocument();

        for (int i=0; i<aTestValues[ index ].nRowData; ++i)
        {
            SCROW nRow = aTestValues[ index ].pData[ i].nStartRow;
            SCROW nEndRow = aTestValues[ index ].pData[ i ].nEndRow;
            SCTAB nTab = aTestValues[ index ].pData[ i ].nTab;
            int nExpectedHeight = aTestValues[ index ].pData[ i ].nExpectedHeight;
            if ( nExpectedHeight == -1 )
                nExpectedHeight =  sc::TwipsToHMM( ScGlobal::GetStandardRowHeight() );
            bool bCheckOpt = ( ( aTestValues[ index ].pData[ i ].nCheck & CHECK_OPTIMAL ) == CHECK_OPTIMAL );
            for ( ; nRow <= nEndRow; ++nRow )
            {
                SAL_INFO( "sc.qa", " checking row " << nRow << " for height " << nExpectedHeight );
                int nHeight = sc::TwipsToHMM( rDoc.GetRowHeight(nRow, nTab, false) );
                if ( bCheckOpt )
                {
                    bool bOpt = !(rDoc.GetRowFlags( nRow, nTab ) & CR_MANUALSIZE);
                    CPPUNIT_ASSERT_EQUAL(aTestValues[ index ].pData[ i ].bOptimal, bOpt);
                }
                CPPUNIT_ASSERT_EQUAL(nExpectedHeight, nHeight);
            }
        }
        xShell->DoClose();
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
