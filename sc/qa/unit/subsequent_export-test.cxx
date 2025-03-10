/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <rtl/strbuf.hxx>
#include <osl/file.hxx>

#include <sfx2/app.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/frame.hxx>
#include <sfx2/sfxmodelfactory.hxx>
#include <svl/stritem.hxx>

#include "helper/qahelper.hxx"
#include "helper/xpath.hxx"
#include "helper/shared_test_impl.hxx"

#include "docsh.hxx"
#include "postit.hxx"
#include "patattr.hxx"
#include "scitems.hxx"
#include "document.hxx"
#include "cellform.hxx"
#include "formulacell.hxx"
#include "tokenarray.hxx"
#include "editutil.hxx"
#include "scopetools.hxx"
#include "cellvalue.hxx"
#include "docfunc.hxx"
#include <postit.hxx>
#include <tokenstringcontext.hxx>
#include <chgtrack.hxx>

#include <svx/svdoole2.hxx>
#include "tabprotection.hxx"
#include <editeng/wghtitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/editdata.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/editobj.hxx>
#include <editeng/section.hxx>
#include <editeng/crossedoutitem.hxx>
#include <editeng/borderline.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/udlnitem.hxx>
#include <formula/grammar.hxx>
#include <unotools/useroptions.hxx>
#include <tools/datetime.hxx>

#include <test/xmltesttools.hxx>

#include <com/sun/star/table/BorderLineStyle.hpp>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

class ScExportTest : public ScBootstrapFixture, XmlTestTools
{
protected:
    virtual void registerNamespaces(xmlXPathContextPtr& pXmlXPathCtx) SAL_OVERRIDE;
public:
    ScExportTest();

    virtual void setUp() SAL_OVERRIDE;
    virtual void tearDown() SAL_OVERRIDE;

#if !defined MACOSX && !defined DRAGONFLY
    ScDocShellRef saveAndReloadPassword( ScDocShell*, const OUString&, const OUString&, const OUString&, sal_uLong );
#endif

    void test();
#if !defined MACOSX && !defined DRAGONFLY
    void testPasswordExport();
#endif
    void testConditionalFormatExportODS();
    void testConditionalFormatExportXLSX();
    void testColorScaleExportODS();
    void testColorScaleExportXLSX();
    void testDataBarExportODS();
    void testDataBarExportXLSX();
    void testMiscRowHeightExport();
    void testNamedRangeBugfdo62729();
    void testRichTextExportODS();
    void testFormulaRefSheetNameODS();

    void testCellValuesExportODS();
    void testCellNoteExportODS();
    void testCellNoteExportXLS();
    void testFormatExportODS();

    void testInlineArrayXLS();
    void testEmbeddedChartXLS();
    void testFormulaReferenceXLS();
    void testSheetProtectionXLSX();

    void testCellBordersXLS();
    void testCellBordersXLSX();
    void testTrackChangesSimpleXLSX();
    void testSheetTabColorsXLSX();

    void testSharedFormulaExportXLS();
    void testSharedFormulaExportXLSX();
    void testSharedFormulaStringResultExportXLSX();

    void testFunctionsExcel2010( sal_uLong nFormatType );
    void testFunctionsExcel2010XLSX();
    void testFunctionsExcel2010XLS();
#if 0
    void testFunctionsExcel2010ODS();
#endif

    void testRelativePaths();

    CPPUNIT_TEST_SUITE(ScExportTest);
    CPPUNIT_TEST(test);
#if !defined(MACOSX) && !defined(DRAGONFLY)
    CPPUNIT_TEST(testPasswordExport);
#endif
    CPPUNIT_TEST(testConditionalFormatExportODS);
    CPPUNIT_TEST(testConditionalFormatExportXLSX);
    CPPUNIT_TEST(testColorScaleExportODS);
    CPPUNIT_TEST(testColorScaleExportXLSX);
    CPPUNIT_TEST(testDataBarExportODS);
    CPPUNIT_TEST(testDataBarExportXLSX);
    CPPUNIT_TEST(testMiscRowHeightExport);
    CPPUNIT_TEST(testNamedRangeBugfdo62729);
    CPPUNIT_TEST(testRichTextExportODS);
    CPPUNIT_TEST(testFormulaRefSheetNameODS);
    CPPUNIT_TEST(testCellValuesExportODS);
    CPPUNIT_TEST(testCellNoteExportODS);
    CPPUNIT_TEST(testCellNoteExportXLS);
    CPPUNIT_TEST(testFormatExportODS);
    CPPUNIT_TEST(testInlineArrayXLS);
    CPPUNIT_TEST(testEmbeddedChartXLS);
    CPPUNIT_TEST(testFormulaReferenceXLS);
    CPPUNIT_TEST(testSheetProtectionXLSX);
    CPPUNIT_TEST(testCellBordersXLS);
    CPPUNIT_TEST(testCellBordersXLSX);
    CPPUNIT_TEST(testTrackChangesSimpleXLSX);
    CPPUNIT_TEST(testSheetTabColorsXLSX);
    CPPUNIT_TEST(testSharedFormulaExportXLS);
    CPPUNIT_TEST(testSharedFormulaExportXLSX);
    CPPUNIT_TEST(testSharedFormulaStringResultExportXLSX);
    CPPUNIT_TEST(testFunctionsExcel2010XLSX);
    CPPUNIT_TEST(testFunctionsExcel2010XLS);
#if !defined(WNT)
    CPPUNIT_TEST(testRelativePaths);
#endif

    /* TODO: export to ODS currently (2014-04-28) makes the validator stumble,
     * probably due to a loext:fill-character attribute in a
     * <number:number-style> element (says number:text tag would not be
     * allowed, which is nonsense). Skip this test until solved. */
#if 0
    CPPUNIT_TEST(testFunctionsExcel2010ODS);
#endif

    CPPUNIT_TEST_SUITE_END();

private:
    void testExcelCellBorders( sal_uLong nFormatType );

    uno::Reference<uno::XInterface> m_xCalcComponent;

};

void ScExportTest::registerNamespaces(xmlXPathContextPtr& pXmlXPathCtx)
{
    struct { xmlChar* pPrefix; xmlChar* pURI; } aNamespaces[] =
    {
        { BAD_CAST("w"), BAD_CAST("http://schemas.openxmlformats.org/wordprocessingml/2006/main") },
        { BAD_CAST("v"), BAD_CAST("urn:schemas-microsoft-com:vml") },
        { BAD_CAST("c"), BAD_CAST("http://schemas.openxmlformats.org/drawingml/2006/chart") },
        { BAD_CAST("a"), BAD_CAST("http://schemas.openxmlformats.org/drawingml/2006/main") },
        { BAD_CAST("mc"), BAD_CAST("http://schemas.openxmlformats.org/markup-compatibility/2006") },
        { BAD_CAST("wps"), BAD_CAST("http://schemas.microsoft.com/office/word/2010/wordprocessingShape") },
        { BAD_CAST("wpg"), BAD_CAST("http://schemas.microsoft.com/office/word/2010/wordprocessingGroup") },
        { BAD_CAST("wp"), BAD_CAST("http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing") },
        { BAD_CAST("office"), BAD_CAST("urn:oasis:names:tc:opendocument:xmlns:office:1.0") },
        { BAD_CAST("table"), BAD_CAST("urn:oasis:names:tc:opendocument:xmlns:table:1.0") },
        { BAD_CAST("text"), BAD_CAST("urn:oasis:names:tc:opendocument:xmlns:text:1.0") },
        { BAD_CAST("xlink"), BAD_CAST("http://www.w3c.org/1999/xlink") }
    };
    for(size_t i = 0; i < SAL_N_ELEMENTS(aNamespaces); ++i)
    {
        xmlXPathRegisterNs(pXmlXPathCtx, aNamespaces[i].pPrefix, aNamespaces[i].pURI );
    }
}

#if !defined MACOSX && !defined DRAGONFLY
ScDocShellRef ScExportTest::saveAndReloadPassword(ScDocShell* pShell, const OUString &rFilter,
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
    SfxItemSet* pExportSet = aStoreMedium.GetItemSet();
    uno::Sequence< beans::NamedValue > aEncryptionData = comphelper::OStorageHelper::CreatePackageEncryptionData( OUString("test") );
    uno::Any xEncryptionData;
    xEncryptionData <<= aEncryptionData;
    pExportSet->Put(SfxUnoAnyItem(SID_ENCRYPTIONDATA, xEncryptionData));

    uno::Reference< embed::XStorage > xMedStorage = aStoreMedium.GetStorage();
    ::comphelper::OStorageHelper::SetCommonStorageEncryptionData( xMedStorage, aEncryptionData );

    pShell->DoSaveAs( aStoreMedium );
    pShell->DoClose();

    //std::cout << "File: " << aTempFile.GetURL() << std::endl;

    sal_uInt32 nFormat = 0;
    if (nFormatType == ODS_FORMAT_TYPE)
        nFormat = SFX_FILTER_IMPORT | SFX_FILTER_USESOPTIONS;

    OUString aPass("test");
    return load(aTempFile.GetURL(), rFilter, rUserData, rTypeName, nFormatType, nFormat, SOFFICE_FILEFORMAT_CURRENT, &aPass);
}
#endif

void ScExportTest::test()
{
    ScDocShell* pShell = new ScDocShell(
        SFXMODEL_STANDARD |
        SFXMODEL_DISABLE_EMBEDDED_SCRIPTS |
        SFXMODEL_DISABLE_DOCUMENT_RECOVERY);
    pShell->DoInitNew();

    ScDocument& rDoc = pShell->GetDocument();

    rDoc.SetValue(0,0,0, 1.0);

    ScDocShellRef xDocSh = saveAndReload( pShell, ODS );

    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rLoadedDoc = xDocSh->GetDocument();
    double aVal = rLoadedDoc.GetValue(0,0,0);
    ASSERT_DOUBLES_EQUAL(aVal, 1.0);
}

#if !defined MACOSX && !defined DRAGONFLY
void ScExportTest::testPasswordExport()
{
    ScDocShell* pShell = new ScDocShell(
        SFXMODEL_STANDARD |
        SFXMODEL_DISABLE_EMBEDDED_SCRIPTS |
        SFXMODEL_DISABLE_DOCUMENT_RECOVERY);
    pShell->DoInitNew();

    ScDocument& rDoc = pShell->GetDocument();

    rDoc.SetValue(0,0,0, 1.0);

    sal_Int32 nFormat = ODS;
    OUString aFilterName(getFileFormats()[nFormat].pFilterName, strlen(getFileFormats()[nFormat].pFilterName), RTL_TEXTENCODING_UTF8) ;
    OUString aFilterType(getFileFormats()[nFormat].pTypeName, strlen(getFileFormats()[nFormat].pTypeName), RTL_TEXTENCODING_UTF8);
    ScDocShellRef xDocSh = saveAndReloadPassword(pShell, aFilterName, OUString(), aFilterType, getFileFormats()[nFormat].nFormatType);

    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rLoadedDoc = xDocSh->GetDocument();
    double aVal = rLoadedDoc.GetValue(0,0,0);
    ASSERT_DOUBLES_EQUAL(aVal, 1.0);

    xDocSh->DoClose();
}
#endif

void ScExportTest::testConditionalFormatExportODS()
{
    ScDocShellRef xShell = loadDoc("new_cond_format_test.", ODS);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(&(*xShell), ODS);
    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rDoc = xDocSh->GetDocument();
    OUString aCSVFile("new_cond_format_test.");
    OUString aCSVPath;
    createCSVPath( aCSVFile, aCSVPath );
    testCondFile(aCSVPath, &rDoc, 0);

    xDocSh->DoClose();
}

void ScExportTest::testConditionalFormatExportXLSX()
{
    ScDocShellRef xShell = loadDoc("new_cond_format_test.", XLSX);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(&(*xShell), XLSX);
    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rDoc = xDocSh->GetDocument();
    {
        OUString aCSVFile("new_cond_format_test.");
        OUString aCSVPath;
        createCSVPath( aCSVFile, aCSVPath );
        testCondFile(aCSVPath, &rDoc, 0);
    }
    {
        OUString aCSVFile("new_cond_format_test_sheet2.");
        OUString aCSVPath;
        createCSVPath( aCSVFile, aCSVPath );
        testCondFile(aCSVPath, &rDoc, 1);
    }

    xDocSh->DoClose();
}

void ScExportTest::testColorScaleExportODS()
{
    ScDocShellRef xShell = loadDoc("colorscale.", ODS);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, ODS);
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();

    testColorScale2Entry_Impl(rDoc);
    testColorScale3Entry_Impl(rDoc);

    xDocSh->DoClose();
}

void ScExportTest::testColorScaleExportXLSX()
{
    ScDocShellRef xShell = loadDoc("colorscale.", XLSX);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, XLSX);
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();

    testColorScale2Entry_Impl(rDoc);
    testColorScale3Entry_Impl(rDoc);

    xDocSh->DoClose();
}

void ScExportTest::testDataBarExportODS()
{
    ScDocShellRef xShell = loadDoc("databar.", ODS);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, ODS);
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();

    testDataBar_Impl(rDoc);

    xDocSh->DoClose();
}

void ScExportTest::testFormatExportODS()
{
    ScDocShellRef xShell = loadDoc("formats.", ODS);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, ODS);
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();

    testFormats(this, &rDoc, ODS);

    xDocSh->DoClose();
}

void ScExportTest::testDataBarExportXLSX()
{
    ScDocShellRef xShell = loadDoc("databar.", XLSX);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, XLSX);
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();

    testDataBar_Impl(rDoc);

    xDocSh->DoClose();
}

void ScExportTest::testMiscRowHeightExport()
{
    TestParam::RowData DfltRowData[] =
    {
        { 0, 4, 0, 529, 0, false },
        { 5, 10, 0, 1058, 0, false },
        { 17, 20, 0, 1767, 0, false },
        // check last couple of row in document to ensure
        // they are 5.29mm ( effective default row xlsx height )
        { 1048573, 1048575, 0, 529, 0, false },
    };

    TestParam::RowData EmptyRepeatRowData[] =
    {
        // rows 0-4, 5-10, 17-20 are all set at various
        // heights, there is no content in the rows, there
        // was a bug where only the first row ( of repeated rows )
        // was set after export
        { 0, 4, 0, 529, 0, false },
        { 5, 10, 0, 1058, 0, false },
        { 17, 20, 0, 1767, 0, false },
    };

    TestParam aTestValues[] =
    {
        // Checks that some distributed ( non-empty ) heights remain set after export (roundtrip)
        // additionally there is effectively a default row height ( 5.29 mm ). So we test the
        // unset rows at the end of the document to ensure the effective xlsx default height
        // is set there too.
        { "miscrowheights.", XLSX, XLSX, SAL_N_ELEMENTS(DfltRowData), DfltRowData },
        // Checks that some distributed ( non-empty ) heights remain set after export (to xls)
        { "miscrowheights.", XLSX, XLS, SAL_N_ELEMENTS(DfltRowData), DfltRowData },
        // Checks that repreated rows ( of various heights ) remain set after export ( to xlsx )
        { "miscemptyrepeatedrowheights.", ODS, XLSX, SAL_N_ELEMENTS(EmptyRepeatRowData), EmptyRepeatRowData },
        // Checks that repreated rows ( of various heights ) remain set after export ( to xls )
        { "miscemptyrepeatedrowheights.", ODS, XLS, SAL_N_ELEMENTS(EmptyRepeatRowData), EmptyRepeatRowData },
    };
    miscRowHeightsTest( aTestValues, SAL_N_ELEMENTS(aTestValues) );
}

namespace {

void setAttribute( ScFieldEditEngine& rEE, sal_Int32 nPara, sal_Int32 nStart, sal_Int32 nEnd, sal_uInt16 nType )
{
    ESelection aSel;
    aSel.nStartPara = aSel.nEndPara = nPara;
    aSel.nStartPos = nStart;
    aSel.nEndPos = nEnd;

    SfxItemSet aItemSet = rEE.GetEmptyItemSet();
    switch (nType)
    {
        case EE_CHAR_WEIGHT:
        {
            SvxWeightItem aWeight(WEIGHT_BOLD, nType);
            aItemSet.Put(aWeight);
            rEE.QuickSetAttribs(aItemSet, aSel);
        }
        break;
        case EE_CHAR_ITALIC:
        {
            SvxPostureItem aItalic(ITALIC_NORMAL, nType);
            aItemSet.Put(aItalic);
            rEE.QuickSetAttribs(aItemSet, aSel);
        }
        break;
        case EE_CHAR_STRIKEOUT:
        {
            SvxCrossedOutItem aCrossOut(STRIKEOUT_SINGLE, nType);
            aItemSet.Put(aCrossOut);
            rEE.QuickSetAttribs(aItemSet, aSel);
        }
        break;
        case EE_CHAR_OVERLINE:
        {
            SvxOverlineItem aItem(UNDERLINE_DOUBLE, nType);
            aItemSet.Put(aItem);
            rEE.QuickSetAttribs(aItemSet, aSel);
        }
        break;
        case EE_CHAR_UNDERLINE:
        {
            SvxUnderlineItem aItem(UNDERLINE_DOUBLE, nType);
            aItemSet.Put(aItem);
            rEE.QuickSetAttribs(aItemSet, aSel);
        }
        break;
        default:
            ;
    }
}

void setFont( ScFieldEditEngine& rEE, sal_Int32 nPara, sal_Int32 nStart, sal_Int32 nEnd, const OUString& rFontName )
{
    ESelection aSel;
    aSel.nStartPara = aSel.nEndPara = nPara;
    aSel.nStartPos = nStart;
    aSel.nEndPos = nEnd;

    SfxItemSet aItemSet = rEE.GetEmptyItemSet();
    SvxFontItem aItem(FAMILY_MODERN, rFontName, "", PITCH_VARIABLE, RTL_TEXTENCODING_UTF8, EE_CHAR_FONTINFO);
    aItemSet.Put(aItem);
    rEE.QuickSetAttribs(aItemSet, aSel);
}

}

void ScExportTest::testNamedRangeBugfdo62729()
{
    ScDocShellRef xShell = loadDoc("fdo62729.", ODS);
    CPPUNIT_ASSERT(xShell.Is());
    ScDocument& rDoc = xShell->GetDocument();

    ScRangeName* pNames = rDoc.GetRangeName();
    //should be just a single named range
    CPPUNIT_ASSERT(pNames->size() == 1 );
    rDoc.DeleteTab(0);
    //should be still a single named range
    CPPUNIT_ASSERT(pNames->size() == 1 );
    ScDocShellRef xDocSh = saveAndReload(xShell, ODS);
    xShell->DoClose();

    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rDoc2 = xDocSh->GetDocument();

    pNames = rDoc2.GetRangeName();
    //after reload should still have a named range
    CPPUNIT_ASSERT(pNames->size() == 1 );

    xDocSh->DoClose();
}

void ScExportTest::testRichTextExportODS()
{
    struct
    {
        static bool isBold(const editeng::Section& rAttr)
        {
            if (rAttr.maAttributes.empty())
                return false;

            std::vector<const SfxPoolItem*>::const_iterator it = rAttr.maAttributes.begin(), itEnd = rAttr.maAttributes.end();
            for (; it != itEnd; ++it)
            {
                const SfxPoolItem* p = *it;
                if (p->Which() != EE_CHAR_WEIGHT)
                    continue;

                return static_cast<const SvxWeightItem*>(p)->GetWeight() == WEIGHT_BOLD;
            }
            return false;
        }

        static bool isItalic(const editeng::Section& rAttr)
        {
            if (rAttr.maAttributes.empty())
                return false;

            std::vector<const SfxPoolItem*>::const_iterator it = rAttr.maAttributes.begin(), itEnd = rAttr.maAttributes.end();
            for (; it != itEnd; ++it)
            {
                const SfxPoolItem* p = *it;
                if (p->Which() != EE_CHAR_ITALIC)
                    continue;

                return static_cast<const SvxPostureItem*>(p)->GetPosture() == ITALIC_NORMAL;
            }
            return false;
        }

        static bool isStrikeOut(const editeng::Section& rAttr)
        {
            if (rAttr.maAttributes.empty())
                return false;

            std::vector<const SfxPoolItem*>::const_iterator it = rAttr.maAttributes.begin(), itEnd = rAttr.maAttributes.end();
            for (; it != itEnd; ++it)
            {
                const SfxPoolItem* p = *it;
                if (p->Which() != EE_CHAR_STRIKEOUT)
                    continue;

                return static_cast<const SvxCrossedOutItem*>(p)->GetStrikeout() == STRIKEOUT_SINGLE;
            }
            return false;
        }

        static bool isOverline(const editeng::Section& rAttr, FontUnderline eStyle)
        {
            if (rAttr.maAttributes.empty())
                return false;

            std::vector<const SfxPoolItem*>::const_iterator it = rAttr.maAttributes.begin(), itEnd = rAttr.maAttributes.end();
            for (; it != itEnd; ++it)
            {
                const SfxPoolItem* p = *it;
                if (p->Which() != EE_CHAR_OVERLINE)
                    continue;

                return static_cast<const SvxOverlineItem*>(p)->GetLineStyle() == eStyle;
            }
            return false;
        }

        static bool isUnderline(const editeng::Section& rAttr, FontUnderline eStyle)
        {
            if (rAttr.maAttributes.empty())
                return false;

            std::vector<const SfxPoolItem*>::const_iterator it = rAttr.maAttributes.begin(), itEnd = rAttr.maAttributes.end();
            for (; it != itEnd; ++it)
            {
                const SfxPoolItem* p = *it;
                if (p->Which() != EE_CHAR_UNDERLINE)
                    continue;

                return static_cast<const SvxUnderlineItem*>(p)->GetLineStyle() == eStyle;
            }
            return false;
        }

        static bool isFont(const editeng::Section& rAttr, const OUString& rFontName)
        {
            if (rAttr.maAttributes.empty())
                return false;

            std::vector<const SfxPoolItem*>::const_iterator it = rAttr.maAttributes.begin(), itEnd = rAttr.maAttributes.end();
            for (; it != itEnd; ++it)
            {
                const SfxPoolItem* p = *it;
                if (p->Which() != EE_CHAR_FONTINFO)
                    continue;

                return static_cast<const SvxFontItem*>(p)->GetFamilyName() == rFontName;
            }
            return false;
        }

        bool checkB2(const EditTextObject* pText) const
        {
            if (!pText)
                return false;

            if (pText->GetParagraphCount() != 1)
                return false;

            if (pText->GetText(0) != "Bold and Italic")
                return false;

            std::vector<editeng::Section> aSecAttrs;
            pText->GetAllSections(aSecAttrs);
            if (aSecAttrs.size() != 3)
                return false;

            // Check the first bold section.
            const editeng::Section* pAttr = &aSecAttrs[0];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 0 || pAttr->mnEnd != 4)
                return false;

            if (pAttr->maAttributes.size() != 1 || !isBold(*pAttr))
                return false;

            // The middle section should be unformatted.
            pAttr = &aSecAttrs[1];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 4 || pAttr->mnEnd != 9)
                return false;

            if (!pAttr->maAttributes.empty())
                return false;

            // The last section should be italic.
            pAttr = &aSecAttrs[2];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 9 || pAttr->mnEnd != 15)
                return false;

            if (pAttr->maAttributes.size() != 1 || !isItalic(*pAttr))
                return false;

            return true;
        }

        bool checkB4(const EditTextObject* pText) const
        {
            if (!pText)
                return false;

            if (pText->GetParagraphCount() != 3)
                return false;

            if (pText->GetText(0) != "One")
                return false;

            if (pText->GetText(1) != "Two")
                return false;

            if (pText->GetText(2) != "Three")
                return false;

            return true;
        }

        bool checkB5(const EditTextObject* pText) const
        {
            if (!pText)
                return false;

            if (pText->GetParagraphCount() != 6)
                return false;

            if (pText->GetText(0) != "")
                return false;

            if (pText->GetText(1) != "Two")
                return false;

            if (pText->GetText(2) != "Three")
                return false;

            if (pText->GetText(3) != "")
                return false;

            if (pText->GetText(4) != "Five")
                return false;

            if (pText->GetText(5) != "")
                return false;

            return true;
        }

        bool checkB6(const EditTextObject* pText) const
        {
            if (!pText)
                return false;

            if (pText->GetParagraphCount() != 1)
                return false;

            if (pText->GetText(0) != "Strike Me")
                return false;

            std::vector<editeng::Section> aSecAttrs;
            pText->GetAllSections(aSecAttrs);
            if (aSecAttrs.size() != 2)
                return false;

            // Check the first strike-out section.
            const editeng::Section* pAttr = &aSecAttrs[0];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 0 || pAttr->mnEnd != 6)
                return false;

            if (pAttr->maAttributes.size() != 1 || !isStrikeOut(*pAttr))
                return false;

            // The last section should be unformatted.
            pAttr = &aSecAttrs[1];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 6 || pAttr->mnEnd != 9)
                return false;

            return true;
        }

        bool checkB7(const EditTextObject* pText) const
        {
            if (!pText)
                return false;

            if (pText->GetParagraphCount() != 1)
                return false;

            if (pText->GetText(0) != "Font1 and Font2")
                return false;

            std::vector<editeng::Section> aSecAttrs;
            pText->GetAllSections(aSecAttrs);
            if (aSecAttrs.size() != 3)
                return false;

            // First section should have "Courier" font applied.
            const editeng::Section* pAttr = &aSecAttrs[0];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 0 || pAttr->mnEnd != 5)
                return false;

            if (pAttr->maAttributes.size() != 1 || !isFont(*pAttr, "Courier"))
                return false;

            // Last section should have "Luxi Mono" applied.
            pAttr = &aSecAttrs[2];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 10 || pAttr->mnEnd != 15)
                return false;

            if (pAttr->maAttributes.size() != 1 || !isFont(*pAttr, "Luxi Mono"))
                return false;

            return true;
        }

        bool checkB8(const EditTextObject* pText) const
        {
            if (!pText)
                return false;

            if (pText->GetParagraphCount() != 1)
                return false;

            if (pText->GetText(0) != "Over and Under")
                return false;

            std::vector<editeng::Section> aSecAttrs;
            pText->GetAllSections(aSecAttrs);
            if (aSecAttrs.size() != 3)
                return false;

            // First section shoul have overline applied.
            const editeng::Section* pAttr = &aSecAttrs[0];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 0 || pAttr->mnEnd != 4)
                return false;

            if (pAttr->maAttributes.size() != 1 || !isOverline(*pAttr, UNDERLINE_DOUBLE))
                return false;

            // Last section should have underline applied.
            pAttr = &aSecAttrs[2];
            if (pAttr->mnParagraph != 0 ||pAttr->mnStart != 9 || pAttr->mnEnd != 14)
                return false;

            if (pAttr->maAttributes.size() != 1 || !isUnderline(*pAttr, UNDERLINE_DOUBLE))
                return false;

            return true;
        }

    } aCheckFunc;

    // Start with an empty document, put one edit text cell, and make sure it
    // survives the save and reload.
    ScDocShellRef xOrigDocSh = loadDoc("empty.", ODS, true);
    const EditTextObject* pEditText;
    {
        ScDocument& rDoc = xOrigDocSh->GetDocument();
        CPPUNIT_ASSERT_MESSAGE("This document should at least have one sheet.", rDoc.GetTableCount() > 0);

        // Insert an edit text cell.
        ScFieldEditEngine* pEE = &rDoc.GetEditEngine();
        pEE->SetText("Bold and Italic");
        // Set the 'Bold' part bold.
        setAttribute(*pEE, 0, 0, 4, EE_CHAR_WEIGHT);
        // Set the 'Italic' part italic.
        setAttribute(*pEE, 0, 9, 15, EE_CHAR_ITALIC);
        ESelection aSel;
        aSel.nStartPara = aSel.nEndPara = 0;

        // Set this edit text to cell B2.
        rDoc.SetEditText(ScAddress(1,1,0), pEE->CreateTextObject());
        pEditText = rDoc.GetEditText(ScAddress(1,1,0));
        CPPUNIT_ASSERT_MESSAGE("Incorret B2 value.", aCheckFunc.checkB2(pEditText));
    }

    // Now, save and reload this document.
    ScDocShellRef xNewDocSh = saveAndReload(xOrigDocSh, ODS);
    {
        xOrigDocSh->DoClose();
        CPPUNIT_ASSERT(xNewDocSh.Is());
        ScDocument& rDoc2 = xNewDocSh->GetDocument();
        CPPUNIT_ASSERT_MESSAGE("Reloaded document should at least have one sheet.", rDoc2.GetTableCount() > 0);
        ScFieldEditEngine* pEE = &rDoc2.GetEditEngine();

        // Make sure the content of B2 is still intact.
        CPPUNIT_ASSERT_MESSAGE("Incorret B2 value.", aCheckFunc.checkB2(pEditText));

        // Insert a multi-line content to B4.
        pEE->Clear();
        pEE->SetText("One\nTwo\nThree");
        rDoc2.SetEditText(ScAddress(1,3,0), pEE->CreateTextObject());
        pEditText = rDoc2.GetEditText(ScAddress(1,3,0));
        CPPUNIT_ASSERT_MESSAGE("Incorrect B4 value.", aCheckFunc.checkB4(pEditText));
    }

    // Reload the doc again, and check the content of B2 and B4.
    ScDocShellRef xNewDocSh2 = saveAndReload(xNewDocSh, ODS);
    {
        ScDocument& rDoc3 = xNewDocSh2->GetDocument();
        ScFieldEditEngine* pEE = &rDoc3.GetEditEngine();
        xNewDocSh->DoClose();

        pEditText = rDoc3.GetEditText(ScAddress(1,1,0));
        CPPUNIT_ASSERT_MESSAGE("B2 should be an edit text.", pEditText);
        pEditText = rDoc3.GetEditText(ScAddress(1,3,0));
        CPPUNIT_ASSERT_MESSAGE("Incorrect B4 value.", aCheckFunc.checkB4(pEditText));

        // Insert a multi-line content to B5, but this time, set some empty paragraphs.
        pEE->Clear();
        pEE->SetText("\nTwo\nThree\n\nFive\n");
        rDoc3.SetEditText(ScAddress(1,4,0), pEE->CreateTextObject());
        pEditText = rDoc3.GetEditText(ScAddress(1,4,0));
        CPPUNIT_ASSERT_MESSAGE("Incorrect B5 value.", aCheckFunc.checkB5(pEditText));

        // Insert a text with strikethrough in B6.
        pEE->Clear();
        pEE->SetText("Strike Me");
        // Set the 'Strike' part strikethrough.
        setAttribute(*pEE, 0, 0, 6, EE_CHAR_STRIKEOUT);
        rDoc3.SetEditText(ScAddress(1,5,0), pEE->CreateTextObject());
        pEditText = rDoc3.GetEditText(ScAddress(1,5,0));
        CPPUNIT_ASSERT_MESSAGE("Incorrect B6 value.", aCheckFunc.checkB6(pEditText));

        // Insert a text with different font segments in B7.
        pEE->Clear();
        pEE->SetText("Font1 and Font2");
        setFont(*pEE, 0, 0, 5, "Courier");
        setFont(*pEE, 0, 10, 15, "Luxi Mono");
        rDoc3.SetEditText(ScAddress(1,6,0), pEE->CreateTextObject());
        pEditText = rDoc3.GetEditText(ScAddress(1,6,0));
        CPPUNIT_ASSERT_MESSAGE("Incorrect B7 value.", aCheckFunc.checkB7(pEditText));

        // Insert a text with overline and underline in B8.
        pEE->Clear();
        pEE->SetText("Over and Under");
        setAttribute(*pEE, 0, 0, 4, EE_CHAR_OVERLINE);
        setAttribute(*pEE, 0, 9, 14, EE_CHAR_UNDERLINE);
        rDoc3.SetEditText(ScAddress(1,7,0), pEE->CreateTextObject());
        pEditText = rDoc3.GetEditText(ScAddress(1,7,0));
        CPPUNIT_ASSERT_MESSAGE("Incorrect B8 value.", aCheckFunc.checkB8(pEditText));
    }

    // Reload the doc again, and check the content of B2, B4, B6 and B7.
    ScDocShellRef xNewDocSh3 = saveAndReload(xNewDocSh2, ODS);
    ScDocument& rDoc4 = xNewDocSh3->GetDocument();
    xNewDocSh2->DoClose();

    pEditText = rDoc4.GetEditText(ScAddress(1,1,0));
    CPPUNIT_ASSERT_MESSAGE("Incorrect B2 value after save and reload.", aCheckFunc.checkB2(pEditText));
    pEditText = rDoc4.GetEditText(ScAddress(1,3,0));
    CPPUNIT_ASSERT_MESSAGE("Incorrect B4 value after save and reload.", aCheckFunc.checkB4(pEditText));
    pEditText = rDoc4.GetEditText(ScAddress(1,4,0));
    CPPUNIT_ASSERT_MESSAGE("Incorrect B5 value after save and reload.", aCheckFunc.checkB5(pEditText));
    pEditText = rDoc4.GetEditText(ScAddress(1,5,0));
    CPPUNIT_ASSERT_MESSAGE("Incorrect B6 value after save and reload.", aCheckFunc.checkB6(pEditText));
    pEditText = rDoc4.GetEditText(ScAddress(1,6,0));
    CPPUNIT_ASSERT_MESSAGE("Incorrect B7 value after save and reload.", aCheckFunc.checkB7(pEditText));
    pEditText = rDoc4.GetEditText(ScAddress(1,7,0));
    CPPUNIT_ASSERT_MESSAGE("Incorrect B8 value after save and reload.", aCheckFunc.checkB8(pEditText));

    xNewDocSh3->DoClose();
}

void ScExportTest::testFormulaRefSheetNameODS()
{
    ScDocShellRef xDocSh = loadDoc("formula-quote-in-sheet-name.", ODS, true);
    {
        ScDocument& rDoc = xDocSh->GetDocument();

        sc::AutoCalcSwitch aACSwitch(rDoc, true); // turn on auto calc.
        rDoc.SetString(ScAddress(1,1,0), "='90''s Data'.B2");
        CPPUNIT_ASSERT_EQUAL(1.1, rDoc.GetValue(ScAddress(1,1,0)));
        if (!checkFormula(rDoc, ScAddress(1,1,0), "'90''s Data'.B2"))
            CPPUNIT_FAIL("Wrong formula");
    }
    // Now, save and reload this document.
    ScDocShellRef xNewDocSh = saveAndReload(xDocSh, ODS);
    xDocSh->DoClose();

    ScDocument& rDoc = xNewDocSh->GetDocument();
    rDoc.CalcAll();
    CPPUNIT_ASSERT_EQUAL(1.1, rDoc.GetValue(ScAddress(1,1,0)));
    if (!checkFormula(rDoc, ScAddress(1,1,0), "'90''s Data'.B2"))
        CPPUNIT_FAIL("Wrong formula");

    xNewDocSh->DoClose();
}

void ScExportTest::testCellValuesExportODS()
{
    // Start with an empty document
    ScDocShellRef xOrigDocSh = loadDoc("empty.", ODS);
    {
        ScDocument& rDoc = xOrigDocSh->GetDocument();
        CPPUNIT_ASSERT_MESSAGE("This document should at least have one sheet.", rDoc.GetTableCount() > 0);

        // set a value double
        rDoc.SetValue(ScAddress(0,0,0), 2.0); // A1

        // set a formula
        rDoc.SetValue(ScAddress(2,0,0), 3.0); // C1
        rDoc.SetValue(ScAddress(3,0,0), 3); // D1
        rDoc.SetString(ScAddress(4,0,0), "=10*C1/4"); // E1
        rDoc.SetValue(ScAddress(5,0,0), 3.0); // F1
        rDoc.SetString(ScAddress(7,0,0), "=SUM(C1:F1)"); //H1

        // set a string
        rDoc.SetString(ScAddress(0,2,0), "a simple line"); //A3

        // set a digit string
        rDoc.SetString(ScAddress(0,4,0), "'12"); //A5
        // set a contiguous value
        rDoc.SetValue(ScAddress(0,5,0), 12.0); //A6
        // set acontiguous string
        rDoc.SetString(ScAddress(0,6,0), "a string"); //A7
        // set a contiguous formula
        rDoc.SetString(ScAddress(0,7,0), "=$A$6"); //A8
    }
    // save and reload
    ScDocShellRef xNewDocSh = saveAndReload(xOrigDocSh, ODS);
    xOrigDocSh->DoClose();
    CPPUNIT_ASSERT(xNewDocSh.Is());
    ScDocument& rDoc = xNewDocSh->GetDocument();
    CPPUNIT_ASSERT_MESSAGE("Reloaded document should at least have one sheet.", rDoc.GetTableCount() > 0);

    // check value
    CPPUNIT_ASSERT_EQUAL(2.0, rDoc.GetValue(0,0,0));
    CPPUNIT_ASSERT_EQUAL(3.0, rDoc.GetValue(2,0,0));
    CPPUNIT_ASSERT_EQUAL(3.0, rDoc.GetValue(3,0,0));
    CPPUNIT_ASSERT_EQUAL(7.5, rDoc.GetValue(4,0,0));
    CPPUNIT_ASSERT_EQUAL(3.0, rDoc.GetValue(5,0,0));

    // check formula
    if (!checkFormula(rDoc, ScAddress(4,0,0), "10*C1/4"))
        CPPUNIT_FAIL("Wrong formula =10*C1/4");
    if (!checkFormula(rDoc, ScAddress(7,0,0), "SUM(C1:F1)"))
        CPPUNIT_FAIL("Wrong formula =SUM(C1:F1)");
    CPPUNIT_ASSERT_EQUAL(16.5, rDoc.GetValue(7,0,0));

    // check string
    ScRefCellValue aCell;
    aCell.assign(rDoc, ScAddress(0,2,0));
    CPPUNIT_ASSERT_EQUAL( CELLTYPE_STRING, aCell.meType );

    // check for an empty cell
    aCell.assign(rDoc, ScAddress(0,3,0));
    CPPUNIT_ASSERT_EQUAL( CELLTYPE_NONE, aCell.meType);

    // check a digit string
    aCell.assign(rDoc, ScAddress(0,4,0));
    CPPUNIT_ASSERT_EQUAL( CELLTYPE_STRING, aCell.meType);

    //check contiguous values
    CPPUNIT_ASSERT_EQUAL( 12.0, rDoc.GetValue(0,5,0) );
    CPPUNIT_ASSERT_EQUAL( OUString("a string"), rDoc.GetString(0,6,0) );
    if (!checkFormula(rDoc, ScAddress(0,7,0), "$A$6"))
        CPPUNIT_FAIL("Wrong formula =$A$6");
    CPPUNIT_ASSERT_EQUAL( rDoc.GetValue(0,5,0), rDoc.GetValue(0,7,0) );

    xNewDocSh->DoClose();
}

void ScExportTest::testCellNoteExportODS()
{
    ScDocShellRef xOrigDocSh = loadDoc("single-note.", ODS);
    ScAddress aPos(0,0,0); // Start with A1.
    {
        ScDocument& rDoc = xOrigDocSh->GetDocument();

        CPPUNIT_ASSERT_MESSAGE("There should be a note at A1.", rDoc.HasNote(aPos));

        aPos.IncRow(); // Move to A2.
        ScPostIt* pNote = rDoc.GetOrCreateNote(aPos);
        pNote->SetText(aPos, "Note One");
        pNote->SetAuthor("Author One");
        CPPUNIT_ASSERT_MESSAGE("There should be a note at A2.", rDoc.HasNote(aPos));
    }
    // save and reload
    ScDocShellRef xNewDocSh = saveAndReload(xOrigDocSh, ODS);
    xOrigDocSh->DoClose();
    CPPUNIT_ASSERT(xNewDocSh.Is());
    ScDocument& rDoc = xNewDocSh->GetDocument();

    aPos.SetRow(0); // Move back to A1.
    CPPUNIT_ASSERT_MESSAGE("There should be a note at A1.", rDoc.HasNote(aPos));
    aPos.IncRow(); // Move to A2.
    CPPUNIT_ASSERT_MESSAGE("There should be a note at A2.", rDoc.HasNote(aPos));

    xNewDocSh->DoClose();
}

void ScExportTest::testCellNoteExportXLS()
{
    // Start with an empty document.s
    ScDocShellRef xOrigDocSh = loadDoc("notes-on-3-sheets.", ODS);
    {
        ScDocument& rDoc = xOrigDocSh->GetDocument();
        CPPUNIT_ASSERT_MESSAGE("This document should have 3 sheets.", rDoc.GetTableCount() == 3);

        // Check note's presence.
        CPPUNIT_ASSERT( rDoc.HasNote(ScAddress(0,0,0)));
        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,1,0)));
        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,2,0)));

        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,0,1)));
        CPPUNIT_ASSERT( rDoc.HasNote(ScAddress(0,1,1)));
        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,2,1)));

        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,0,2)));
        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,1,2)));
        CPPUNIT_ASSERT( rDoc.HasNote(ScAddress(0,2,2)));
    }
    // save and reload as XLS.
    ScDocShellRef xNewDocSh = saveAndReload(xOrigDocSh, XLS);
    {
        xOrigDocSh->DoClose();
        CPPUNIT_ASSERT(xNewDocSh.Is());
        ScDocument& rDoc = xNewDocSh->GetDocument();
        CPPUNIT_ASSERT_MESSAGE("This document should have 3 sheets.", rDoc.GetTableCount() == 3);

        // Check note's presence again.
        CPPUNIT_ASSERT( rDoc.HasNote(ScAddress(0,0,0)));
        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,1,0)));
        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,2,0)));

        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,0,1)));
        CPPUNIT_ASSERT( rDoc.HasNote(ScAddress(0,1,1)));
        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,2,1)));

        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,0,2)));
        CPPUNIT_ASSERT(!rDoc.HasNote(ScAddress(0,1,2)));
        CPPUNIT_ASSERT( rDoc.HasNote(ScAddress(0,2,2)));

        xNewDocSh->DoClose();
    }
}

namespace {

void checkMatrixRange(ScDocument& rDoc, const ScRange& rRange)
{
    ScRange aMatRange;
    ScAddress aMatOrigin;
    for (SCCOL nCol = rRange.aStart.Col(); nCol <= rRange.aEnd.Col(); ++nCol)
    {
        for (SCROW nRow = rRange.aStart.Row(); nRow <= rRange.aEnd.Row(); ++nRow)
        {
            ScAddress aPos(nCol, nRow, rRange.aStart.Tab());
            bool bIsMatrix = rDoc.GetMatrixFormulaRange(aPos, aMatRange);
            CPPUNIT_ASSERT_MESSAGE("Matrix expected, but not found.", bIsMatrix);
            CPPUNIT_ASSERT_MESSAGE("Wrong matrix range.", rRange == aMatRange);
            const ScFormulaCell* pCell = rDoc.GetFormulaCell(aPos);
            CPPUNIT_ASSERT_MESSAGE("This must be a formula cell.", pCell);

            bIsMatrix = pCell->GetMatrixOrigin(aMatOrigin);
            CPPUNIT_ASSERT_MESSAGE("Not a part of matrix formula.", bIsMatrix);
            CPPUNIT_ASSERT_MESSAGE("Wrong matrix origin.", aMatOrigin == aMatRange.aStart);
        }
    }
}

}

void ScExportTest::testInlineArrayXLS()
{
    ScDocShellRef xShell = loadDoc("inline-array.", XLS);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, XLS);
    xShell->DoClose();
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();

    // B2:C3 contains a matrix.
    checkMatrixRange(rDoc, ScRange(1,1,0,2,2,0));

    // B5:D6 contains a matrix.
    checkMatrixRange(rDoc, ScRange(1,4,0,3,5,0));

    // B8:C10 as well.
    checkMatrixRange(rDoc, ScRange(1,7,0,2,9,0));

    xDocSh->DoClose();
}

void ScExportTest::testEmbeddedChartXLS()
{
    ScDocShellRef xShell = loadDoc("embedded-chart.", XLS);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, XLS);
    xShell->DoClose();
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();

    // Make sure the 2nd sheet is named 'Chart1'.
    OUString aName;
    rDoc.GetName(1, aName);
    CPPUNIT_ASSERT_EQUAL(OUString("Chart1"), aName);

    const SdrOle2Obj* pOleObj = getSingleChartObject(rDoc, 1);
    CPPUNIT_ASSERT_MESSAGE("Failed to retrieve a chart object from the 2nd sheet.", pOleObj);

    ScRangeList aRanges = getChartRanges(rDoc, *pOleObj);
    CPPUNIT_ASSERT_MESSAGE("Label range (B3:B5) not found.", aRanges.In(ScRange(1,2,1,1,4,1)));
    CPPUNIT_ASSERT_MESSAGE("Data label (C2) not found.", aRanges.In(ScAddress(2,1,1)));
    CPPUNIT_ASSERT_MESSAGE("Data range (C3:C5) not found.", aRanges.In(ScRange(2,2,1,2,4,1)));

    xDocSh->DoClose();
}

void ScExportTest::testFormulaReferenceXLS()
{
    ScDocShellRef xShell = loadDoc("formula-reference.", XLS);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, XLS);
    xShell->DoClose();
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();

    if (!checkFormula(rDoc, ScAddress(3,1,0), "$A$2+$B$2+$C$2"))
        CPPUNIT_FAIL("Wrong formula in D2");

    if (!checkFormula(rDoc, ScAddress(3,2,0), "A3+B3+C3"))
        CPPUNIT_FAIL("Wrong formula in D3");

    if (!checkFormula(rDoc, ScAddress(3,5,0), "SUM($A$6:$C$6)"))
        CPPUNIT_FAIL("Wrong formula in D6");

    if (!checkFormula(rDoc, ScAddress(3,6,0), "SUM(A7:C7)"))
        CPPUNIT_FAIL("Wrong formula in D7");

    if (!checkFormula(rDoc, ScAddress(3,9,0), "$Two.$A$2+$Two.$B$2+$Two.$C$2"))
        CPPUNIT_FAIL("Wrong formula in D10");

    if (!checkFormula(rDoc, ScAddress(3,10,0), "$Two.A3+$Two.B3+$Two.C3"))
        CPPUNIT_FAIL("Wrong formula in D11");

    if (!checkFormula(rDoc, ScAddress(3,13,0), "MIN($Two.$A$2:$C$2)"))
        CPPUNIT_FAIL("Wrong formula in D14");

    if (!checkFormula(rDoc, ScAddress(3,14,0), "MAX($Two.A3:C3)"))
        CPPUNIT_FAIL("Wrong formula in D15");

    xDocSh->DoClose();
}

void ScExportTest::testSheetProtectionXLSX()
{
    ScDocShellRef xShell = loadDoc("ProtecteSheet1234Pass.", XLSX);
    CPPUNIT_ASSERT(xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, XLSX);
    CPPUNIT_ASSERT(xDocSh.Is());

    ScDocument& rDoc = xDocSh->GetDocument();
    const ScTableProtection* pTabProtect = rDoc.GetTabProtection(0);
    CPPUNIT_ASSERT(pTabProtect);
    if ( pTabProtect )
    {
        Sequence<sal_Int8> aHash = pTabProtect->getPasswordHash(PASSHASH_XL);
        // check has
        if (aHash.getLength() >= 2)
        {
            CPPUNIT_ASSERT( (sal_uInt8)aHash[0] == 204 );
            CPPUNIT_ASSERT( (sal_uInt8)aHash[1] == 61 );
        }
        // we could flesh out this check I guess
        CPPUNIT_ASSERT ( !pTabProtect->isOptionEnabled( ScTableProtection::OBJECTS ) );
        CPPUNIT_ASSERT ( !pTabProtect->isOptionEnabled( ScTableProtection::SCENARIOS ) );
    }
    xDocSh->DoClose();
}

namespace {

const char* toBorderName( sal_Int16 eStyle )
{
    switch (eStyle)
    {
        case table::BorderLineStyle::SOLID: return "SOLID";
        case table::BorderLineStyle::DOTTED: return "DOTTED";
        case table::BorderLineStyle::DASHED: return "DASHED";
        case table::BorderLineStyle::DASH_DOT: return "DASH_DOT";
        case table::BorderLineStyle::DASH_DOT_DOT: return "DASH_DOT_DOT";
        case table::BorderLineStyle::DOUBLE_THIN: return "DOUBLE_THIN";
        case table::BorderLineStyle::FINE_DASHED: return "FINE_DASHED";
        default:
            ;
    }

    return "";
}

}

void ScExportTest::testExcelCellBorders( sal_uLong nFormatType )
{
    struct
    {
        SCROW mnRow;
        sal_Int16 mnStyle;
        long mnWidth;
    } aChecks[] = {
        {  1, table::BorderLineStyle::SOLID,         1L }, // hair
        {  3, table::BorderLineStyle::DOTTED,       15L }, // dotted
        {  5, table::BorderLineStyle::DASH_DOT_DOT, 15L }, // dash dot dot
        {  7, table::BorderLineStyle::DASH_DOT,     15L }, // dash dot
        {  9, table::BorderLineStyle::FINE_DASHED,  15L }, // dashed
        { 11, table::BorderLineStyle::SOLID,        15L }, // thin
        { 13, table::BorderLineStyle::DASH_DOT_DOT, 35L }, // medium dash dot dot
        { 17, table::BorderLineStyle::DASH_DOT,     35L }, // medium dash dot
        { 19, table::BorderLineStyle::DASHED,       35L }, // medium dashed
        { 21, table::BorderLineStyle::SOLID,        35L }, // medium
        { 23, table::BorderLineStyle::SOLID,        50L }, // thick
        { 25, table::BorderLineStyle::DOUBLE_THIN,  -1L }, // double (don't check width)
    };

    ScDocShellRef xDocSh = loadDoc("cell-borders.", nFormatType);
    CPPUNIT_ASSERT_MESSAGE("Failed to load file", xDocSh.Is());
    {
        ScDocument& rDoc = xDocSh->GetDocument();

        for (size_t i = 0; i < SAL_N_ELEMENTS(aChecks); ++i)
        {
            const editeng::SvxBorderLine* pLine = NULL;
            rDoc.GetBorderLines(2, aChecks[i].mnRow, 0, NULL, &pLine, NULL, NULL);
            CPPUNIT_ASSERT(pLine);
            CPPUNIT_ASSERT_EQUAL(toBorderName(aChecks[i].mnStyle), toBorderName(pLine->GetBorderLineStyle()));
            if (aChecks[i].mnWidth >= 0)
                CPPUNIT_ASSERT_EQUAL(aChecks[i].mnWidth, pLine->GetWidth());
        }
    }

    ScDocShellRef xNewDocSh = saveAndReload(xDocSh, nFormatType);
    xDocSh->DoClose();
    ScDocument& rDoc = xNewDocSh->GetDocument();
    for (size_t i = 0; i < SAL_N_ELEMENTS(aChecks); ++i)
    {
        const editeng::SvxBorderLine* pLine = NULL;
        rDoc.GetBorderLines(2, aChecks[i].mnRow, 0, NULL, &pLine, NULL, NULL);
        CPPUNIT_ASSERT(pLine);
        CPPUNIT_ASSERT_EQUAL(toBorderName(aChecks[i].mnStyle), toBorderName(pLine->GetBorderLineStyle()));
        if (aChecks[i].mnWidth >= 0)
            CPPUNIT_ASSERT_EQUAL(aChecks[i].mnWidth, pLine->GetWidth());
    }

    xNewDocSh->DoClose();
}

void ScExportTest::testCellBordersXLS()
{
    testExcelCellBorders(XLS);
}

void ScExportTest::testCellBordersXLSX()
{
    testExcelCellBorders(XLSX);
}

OUString toString( const ScBigRange& rRange )
{
    OUStringBuffer aBuf;
    aBuf.appendAscii("(columns:");
    aBuf.append(rRange.aStart.Col());
    aBuf.append('-');
    aBuf.append(rRange.aEnd.Col());
    aBuf.appendAscii(";rows:");
    aBuf.append(rRange.aStart.Row());
    aBuf.append('-');
    aBuf.append(rRange.aEnd.Row());
    aBuf.appendAscii(";sheets:");
    aBuf.append(rRange.aStart.Tab());
    aBuf.append('-');
    aBuf.append(rRange.aEnd.Tab());
    aBuf.append(')');

    return aBuf.makeStringAndClear();
}

void ScExportTest::testTrackChangesSimpleXLSX()
{
    struct CheckItem
    {
        sal_uLong mnActionId;
        ScChangeActionType meType;

        sal_Int32 mnStartCol;
        sal_Int32 mnStartRow;
        sal_Int32 mnStartTab;
        sal_Int32 mnEndCol;
        sal_Int32 mnEndRow;
        sal_Int32 mnEndTab;

        bool mbRowInsertedAtBottom;
    };

    struct
    {
        bool checkRange( ScChangeActionType eType, const ScBigRange& rExpected, const ScBigRange& rActual )
        {
            ScBigRange aExpected(rExpected), aActual(rActual);

            switch (eType)
            {
                case SC_CAT_INSERT_ROWS:
                {
                    // Ignore columns.
                    aExpected.aStart.SetCol(0);
                    aExpected.aEnd.SetCol(0);
                    aActual.aStart.SetCol(0);
                    aActual.aEnd.SetCol(0);
                }
                break;
                default:
                    ;
            }

            return aExpected == aActual;
        }

        bool check( ScDocument& rDoc )
        {
            CheckItem aChecks[] =
            {
                {  1, SC_CAT_CONTENT     , 1, 1, 0, 1, 1, 0, false },
                {  2, SC_CAT_INSERT_ROWS , 0, 2, 0, 0, 2, 0, true },
                {  3, SC_CAT_CONTENT     , 1, 2, 0, 1, 2, 0, false },
                {  4, SC_CAT_INSERT_ROWS , 0, 3, 0, 0, 3, 0, true  },
                {  5, SC_CAT_CONTENT     , 1, 3, 0, 1, 3, 0, false },
                {  6, SC_CAT_INSERT_ROWS , 0, 4, 0, 0, 4, 0, true  },
                {  7, SC_CAT_CONTENT     , 1, 4, 0, 1, 4, 0, false },
                {  8, SC_CAT_INSERT_ROWS , 0, 5, 0, 0, 5, 0, true  },
                {  9, SC_CAT_CONTENT     , 1, 5, 0, 1, 5, 0, false },
                { 10, SC_CAT_INSERT_ROWS , 0, 6, 0, 0, 6, 0, true  },
                { 11, SC_CAT_CONTENT     , 1, 6, 0, 1, 6, 0, false },
                { 12, SC_CAT_INSERT_ROWS , 0, 7, 0, 0, 7, 0, true  },
                { 13, SC_CAT_CONTENT     , 1, 7, 0, 1, 7, 0, false },
            };

            ScChangeTrack* pCT = rDoc.GetChangeTrack();
            if (!pCT)
            {
                cerr << "Change track instance doesn't exist." << endl;
                return false;
            }

            sal_uLong nActionMax = pCT->GetActionMax();
            if (nActionMax != 13)
            {
                cerr << "Unexpected highest action ID value." << endl;
                return false;
            }

            for (size_t i = 0, n = SAL_N_ELEMENTS(aChecks); i < n; ++i)
            {
                sal_uInt16 nActId = aChecks[i].mnActionId;
                const ScChangeAction* pAction = pCT->GetAction(nActId);
                if (!pAction)
                {
                    cerr << "No action for action number " << nActId << " found." << endl;
                    return false;
                }

                if (pAction->GetType() != aChecks[i].meType)
                {
                    cerr << "Unexpected action type for action number " << nActId << "." << endl;
                    return false;
                }

                const ScBigRange& rRange = pAction->GetBigRange();
                ScBigRange aCheck(aChecks[i].mnStartCol, aChecks[i].mnStartRow, aChecks[i].mnStartTab,
                                  aChecks[i].mnEndCol, aChecks[i].mnEndRow, aChecks[i].mnEndTab);

                if (!checkRange(pAction->GetType(), aCheck, rRange))
                {
                    cerr << "Unexpected range for action number " << nActId
                        << ": expected=" << toString(aCheck) << " actual=" << toString(rRange) << endl;
                    return false;
                }

                switch (pAction->GetType())
                {
                    case SC_CAT_INSERT_ROWS:
                    {
                        const ScChangeActionIns* p = static_cast<const ScChangeActionIns*>(pAction);
                        if (p->IsEndOfList() != aChecks[i].mbRowInsertedAtBottom)
                        {
                            cerr << "Unexpected end-of-list flag for action number " << nActId << "." << endl;
                            return false;
                        }
                    }
                    break;
                    default:
                        ;
                }
            }

            return true;
        }

        bool checkRevisionUserAndTime( ScDocument& rDoc, const OUString& rOwnerName )
        {
            ScChangeTrack* pCT = rDoc.GetChangeTrack();
            if (!pCT)
            {
                cerr << "Change track instance doesn't exist." << endl;
                return false;
            }

            ScChangeAction* pAction = pCT->GetLast();
            if (pAction->GetUser() != "Kohei Yoshida")
            {
                cerr << "Wrong user name." << endl;
                return false;
            }

            DateTime aDT = pAction->GetDateTimeUTC();
            if (aDT.GetYear() != 2014 || aDT.GetMonth() != 7 || aDT.GetDay() != 11)
            {
                cerr << "Wrong time stamp." << endl;
                return false;
            }

            // Insert a new record to make sure the user and date-time are correct.
            rDoc.SetString(ScAddress(1,8,0), "New String");
            ScCellValue aEmpty;
            pCT->AppendContent(ScAddress(1,8,0), aEmpty);
            pAction = pCT->GetLast();
            if (!pAction)
            {
                cerr << "Failed to retrieve last revision." << endl;
                return false;
            }

            if (rOwnerName != pAction->GetUser())
            {
                cerr << "Wrong user name." << endl;
                return false;
            }

            DateTime aDTNew = pAction->GetDateTimeUTC();
            if (aDTNew <= aDT)
            {
                cerr << "Time stamp of the new revision should be more recent than that of the last revision." << endl;
                return false;
            }

            return true;
        }

    } aTest;

    SvtUserOptions& rUserOpt = SC_MOD()->GetUserOptions();
    rUserOpt.SetToken(USER_OPT_FIRSTNAME, "Export");
    rUserOpt.SetToken(USER_OPT_LASTNAME, "Test");

    OUString aOwnerName = rUserOpt.GetFirstName() + " " + rUserOpt.GetLastName();

    // First, test the xls variant.

    ScDocShellRef xDocSh = loadDoc("track-changes/simple-cell-changes.", XLS);
    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument* pDoc = &xDocSh->GetDocument();
    bool bGood = aTest.check(*pDoc);
    CPPUNIT_ASSERT_MESSAGE("Initial check failed (xls).", bGood);

    ScDocShellRef xDocSh2 = saveAndReload(xDocSh, XLS);
    xDocSh->DoClose();
    pDoc = &xDocSh2->GetDocument();
    bGood = aTest.check(*pDoc);
    CPPUNIT_ASSERT_MESSAGE("Check after reload failed (xls).", bGood);

    // fdo#81445 : Check the blank value string to make sure it's "<empty>".
    ScChangeTrack* pCT = pDoc->GetChangeTrack();
    CPPUNIT_ASSERT(pCT);
    ScChangeAction* pAction = pCT->GetAction(1);
    CPPUNIT_ASSERT(pAction);
    OUString aDesc;
    pAction->GetDescription(aDesc, pDoc);
    CPPUNIT_ASSERT_EQUAL(OUString("Cell B2 changed from '<empty>' to '1'"), aDesc);

    bGood = aTest.checkRevisionUserAndTime(*pDoc, aOwnerName);
    CPPUNIT_ASSERT_MESSAGE("Check revision and time failed after reload (xls).", bGood);

    xDocSh2->DoClose();

    // Now, test the xlsx variant the same way.

    xDocSh = loadDoc("track-changes/simple-cell-changes.", XLSX);
    CPPUNIT_ASSERT(xDocSh.Is());
    pDoc = &xDocSh->GetDocument();
    aTest.check(*pDoc);
    CPPUNIT_ASSERT_MESSAGE("Initial check failed (xlsx).", bGood);

    xDocSh2 = saveAndReload(xDocSh, XLSX);
    xDocSh->DoClose();
    pDoc = &xDocSh2->GetDocument();
    bGood = aTest.check(*pDoc);
    CPPUNIT_ASSERT_MESSAGE("Check after reload failed (xlsx).", bGood);

    bGood = aTest.checkRevisionUserAndTime(*pDoc, aOwnerName);
    CPPUNIT_ASSERT_MESSAGE("Check revision and time failed after reload (xlsx).", bGood);

    xDocSh2->DoClose();
}

void ScExportTest::testSheetTabColorsXLSX()
{
    struct
    {
        bool checkContent( ScDocument& rDoc )
        {

            std::vector<OUString> aTabNames = rDoc.GetAllTableNames();

            // green, red, blue, yellow (from left to right).
            if (aTabNames.size() != 4)
            {
                cerr << "There should be exactly 4 sheets." << endl;
                return false;
            }

            const char* pNames[] = { "Green", "Red", "Blue", "Yellow" };
            for (size_t i = 0, n = SAL_N_ELEMENTS(pNames); i < n; ++i)
            {
                OUString aExpected = OUString::createFromAscii(pNames[i]);
                if (aExpected != aTabNames[i])
                {
                    cerr << "incorrect sheet name: expected='" << aExpected <<"', actual='" << aTabNames[i] << "'" << endl;
                    return false;
                }
            }

            const ColorData aXclColors[] =
            {
                0x0000B050, // green
                0x00FF0000, // red
                0x000070C0, // blue
                0x00FFFF00, // yellow
            };

            for (size_t i = 0, n = SAL_N_ELEMENTS(aXclColors); i < n; ++i)
            {
                if (aXclColors[i] != rDoc.GetTabBgColor(i).GetColor())
                {
                    cerr << "wrong sheet color for sheet " << i << endl;
                    return false;
                }
            }

            return true;
        }

    } aTest;

    ScDocShellRef xDocSh = loadDoc("sheet-tab-color.", XLSX);
    {
        CPPUNIT_ASSERT_MESSAGE("Failed to load file.", xDocSh.Is());
        ScDocument& rDoc = xDocSh->GetDocument();
        bool bRes = aTest.checkContent(rDoc);
        CPPUNIT_ASSERT_MESSAGE("Failed on the initial content check.", bRes);
    }

    ScDocShellRef xDocSh2 = saveAndReload(xDocSh, XLSX);
    CPPUNIT_ASSERT_MESSAGE("Failed to reload file.", xDocSh2.Is());
    xDocSh->DoClose();
    ScDocument& rDoc = xDocSh2->GetDocument();
    bool bRes = aTest.checkContent(rDoc);
    CPPUNIT_ASSERT_MESSAGE("Failed on the content check after reload.", bRes);

    xDocSh2->DoClose();
}

void ScExportTest::testSharedFormulaExportXLS()
{
    struct
    {
        bool checkContent( ScDocument& rDoc )
        {
            formula::FormulaGrammar::Grammar eGram = formula::FormulaGrammar::GRAM_ENGLISH_XL_R1C1;
            rDoc.SetGrammar(eGram);
            sc::TokenStringContext aCxt(&rDoc, eGram);

            // Check the title row.

            OUString aActual = rDoc.GetString(0,1,0);
            OUString aExpected = "Response";
            if (aActual != aExpected)
            {
                cerr << "Wrong content in A2: expected='" << aExpected << "', actual='" << aActual << "'" << endl;
                return false;
            }

            aActual = rDoc.GetString(1,1,0);
            aExpected = "Response";
            if (aActual != aExpected)
            {
                cerr << "Wrong content in B2: expected='" << aExpected << "', actual='" << aActual << "'" << endl;
                return false;
            }

            // A3:A12 and B3:B12 are numbers from 1 to 10.
            for (SCROW i = 0; i <= 9; ++i)
            {
                double fExpected = i + 1.0;
                ScAddress aPos(0,i+2,0);
                double fActual = rDoc.GetValue(aPos);
                if (fExpected != fActual)
                {
                    cerr << "Wrong value in A" << (i+2) << ": expected=" << fExpected << ", actual=" << fActual << endl;
                    return false;
                }

                aPos.IncCol();
                ScFormulaCell* pFC = rDoc.GetFormulaCell(aPos);
                if (!pFC)
                {
                    cerr << "B" << (i+2) << " should be a formula cell." << endl;
                    return false;
                }

                OUString aFormula = pFC->GetCode()->CreateString(aCxt, aPos);
                aExpected = "Coefficients!RC[-1]";
                if (aFormula != aExpected)
                {
                    cerr << "Wrong formula in B" << (i+2) << ": expected='" << aExpected << "', actual='" << aFormula << "'" << endl;
                    return false;
                }

                fActual = rDoc.GetValue(aPos);
                if (fExpected != fActual)
                {
                    cerr << "Wrong value in B" << (i+2) << ": expected=" << fExpected << ", actual=" << fActual << endl;
                    return false;
                }
            }

            return true;
        }

    } aTest;

    ScDocShellRef xDocSh = loadDoc("shared-formula/3d-reference.", ODS);
    {
        CPPUNIT_ASSERT_MESSAGE("Failed to load file.", xDocSh.Is());
        ScDocument& rDoc = xDocSh->GetDocument();

        // Check the content of the original.
        bool bRes = aTest.checkContent(rDoc);
        CPPUNIT_ASSERT_MESSAGE("Content check on the original document failed.", bRes);
    }

    ScDocShellRef xDocSh2 = saveAndReload(xDocSh, XLS);
    xDocSh->DoClose();
    CPPUNIT_ASSERT_MESSAGE("Failed to reload file.", xDocSh2.Is());

    ScDocument& rDoc = xDocSh2->GetDocument();

    // Check the content of the reloaded. This should be identical.
    bool bRes = aTest.checkContent(rDoc);
    CPPUNIT_ASSERT_MESSAGE("Content check on the reloaded document failed.", bRes);

    xDocSh2->DoClose();
}

void ScExportTest::testSharedFormulaExportXLSX()
{
    struct
    {
        bool checkContent( ScDocument& rDoc )
        {
            // B2:B7 should show 1,2,3,4,5,6.
            double fExpected = 1.0;
            for (SCROW i = 1; i <= 6; ++i, ++fExpected)
            {
                ScAddress aPos(1,i,0);
                double fVal = rDoc.GetValue(aPos);
                if (fVal != fExpected)
                {
                    cerr << "Wrong value in B" << (i+1) << ": expected=" << fExpected << ", actual=" << fVal << endl;
                    return false;
                }
            }

            // C2:C7 should show 10,20,....,60.
            fExpected = 10.0;
            for (SCROW i = 1; i <= 6; ++i, fExpected+=10.0)
            {
                ScAddress aPos(2,i,0);
                double fVal = rDoc.GetValue(aPos);
                if (fVal != fExpected)
                {
                    cerr << "Wrong value in C" << (i+1) << ": expected=" << fExpected << ", actual=" << fVal << endl;
                    return false;
                }
            }

            // D2:D7 should show 1,2,...,6.
            fExpected = 1.0;
            for (SCROW i = 1; i <= 6; ++i, ++fExpected)
            {
                ScAddress aPos(3,i,0);
                double fVal = rDoc.GetValue(aPos);
                if (fVal != fExpected)
                {
                    cerr << "Wrong value in D" << (i+1) << ": expected=" << fExpected << ", actual=" << fVal << endl;
                    return false;
                }
            }

            return true;
        }

    } aTest;

    ScDocShellRef xDocSh = loadDoc("shared-formula/3d-reference.", XLSX);
    {
        CPPUNIT_ASSERT_MESSAGE("Failed to load file.", xDocSh.Is());
        ScDocument& rDoc = xDocSh->GetDocument();

        bool bRes = aTest.checkContent(rDoc);
        CPPUNIT_ASSERT_MESSAGE("Content check on the initial document failed.", bRes);

        rDoc.CalcAll(); // Recalculate to flush all cached results.
        bRes = aTest.checkContent(rDoc);
        CPPUNIT_ASSERT_MESSAGE("Content check on the initial recalculated document failed.", bRes);
    }

    // Save and reload, and check the content again.
    ScDocShellRef xDocSh2 = saveAndReload(xDocSh, XLSX);
    xDocSh->DoClose();

    CPPUNIT_ASSERT_MESSAGE("Failed to load file.", xDocSh2.Is());
    ScDocument& rDoc = xDocSh2->GetDocument();
    rDoc.CalcAll(); // Recalculate to flush all cached results.

    bool bRes = aTest.checkContent(rDoc);
    CPPUNIT_ASSERT_MESSAGE("Content check on the reloaded document failed.", bRes);

    xDocSh2->DoClose();
}

void ScExportTest::testSharedFormulaStringResultExportXLSX()
{
    struct
    {
        bool checkContent( ScDocument& rDoc )
        {
            {
                // B2:B7 should show A,B,....,F.
                const char* expected[] = { "A", "B", "C", "D", "E", "F" };
                for (SCROW i = 0; i <= 5; ++i)
                {
                    ScAddress aPos(1,i+1,0);
                    OUString aStr = rDoc.GetString(aPos);
                    OUString aExpected = OUString::createFromAscii(expected[i]);
                    if (aStr != aExpected)
                    {
                        cerr << "Wrong value in B" << (i+2) << ": expected='" << aExpected << "', actual='" << aStr << "'" << endl;
                        return false;
                    }
                }
            }

            {
                // C2:C7 should show AA,BB,....,FF.
                const char* expected[] = { "AA", "BB", "CC", "DD", "EE", "FF" };
                for (SCROW i = 0; i <= 5; ++i)
                {
                    ScAddress aPos(2,i+1,0);
                    OUString aStr = rDoc.GetString(aPos);
                    OUString aExpected = OUString::createFromAscii(expected[i]);
                    if (aStr != aExpected)
                    {
                        cerr << "Wrong value in C" << (i+2) << ": expected='" << aExpected << "', actual='" << aStr << "'" << endl;
                        return false;
                    }
                }
            }

            return true;
        }

    } aTest;

    ScDocShellRef xDocSh = loadDoc("shared-formula/text-results.", XLSX);
    {
        CPPUNIT_ASSERT_MESSAGE("Failed to load file.", xDocSh.Is());
        ScDocument& rDoc = xDocSh->GetDocument();

        // Check content without re-calculation, to test cached formula results.
        bool bRes = aTest.checkContent(rDoc);
        CPPUNIT_ASSERT_MESSAGE("Content check on the initial document failed.", bRes);

        // Now, re-calculate and check the results.
        rDoc.CalcAll();
        bRes = aTest.checkContent(rDoc);
        CPPUNIT_ASSERT_MESSAGE("Content check on the initial recalculated document failed.", bRes);
    }
    // Reload and check again.
    ScDocShellRef xDocSh2 = saveAndReload(xDocSh, XLSX);
    xDocSh->DoClose();
    CPPUNIT_ASSERT_MESSAGE("Failed to re-load file.", xDocSh2.Is());
    ScDocument& rDoc = xDocSh2->GetDocument();

    bool bRes = aTest.checkContent(rDoc);
    CPPUNIT_ASSERT_MESSAGE("Content check on the reloaded document failed.", bRes);

    xDocSh2->DoClose();
}

void ScExportTest::testFunctionsExcel2010( sal_uLong nFormatType )
{
    ScDocShellRef xShell = loadDoc("functions-excel-2010.", XLSX);
    CPPUNIT_ASSERT_MESSAGE("Failed to load the document.", xShell.Is());

    ScDocShellRef xDocSh = saveAndReload(xShell, nFormatType);
    ScDocument& rDoc = xDocSh->GetDocument();
    rDoc.CalcAll(); // perform hard re-calculation.

    testFunctionsExcel2010_Impl(rDoc);

    xDocSh->DoClose();
}

void ScExportTest::testFunctionsExcel2010XLSX()
{
    testFunctionsExcel2010(XLSX);
}

void ScExportTest::testFunctionsExcel2010XLS()
{
    testFunctionsExcel2010(XLS);
}

void ScExportTest::testRelativePaths()
{
    ScDocShellRef xDocSh = loadDoc("fdo79305.", ODS);
    CPPUNIT_ASSERT(xDocSh.Is());

    xmlDocPtr pDoc = XPathHelper::parseExport(&(*xDocSh), m_xSFactory, "content.xml", ODS);
    CPPUNIT_ASSERT(pDoc);
    OUString aURL = getXPath(pDoc,
            "/office:document-content/office:body/office:spreadsheet/table:table/table:table-row[2]/table:table-cell[2]/text:p/text:a", "href");
    // make sure that the URL is relative
    CPPUNIT_ASSERT(aURL.startsWith(".."));
}

#if 0
void ScExportTest::testFunctionsExcel2010ODS()
{
    testFunctionsExcel2010(ODS);
}
#endif

ScExportTest::ScExportTest()
      : ScBootstrapFixture("/sc/qa/unit/data")
{
}

void ScExportTest::setUp()
{
    test::BootstrapFixture::setUp();

    // This is a bit of a fudge, we do this to ensure that ScGlobals::ensure,
    // which is a private symbol to us, gets called
    m_xCalcComponent =
        getMultiServiceFactory()->createInstance("com.sun.star.comp.Calc.SpreadsheetDocument");
    CPPUNIT_ASSERT_MESSAGE("no calc component!", m_xCalcComponent.is());
}

void ScExportTest::tearDown()
{
    uno::Reference< lang::XComponent >( m_xCalcComponent, UNO_QUERY_THROW )->dispose();
    test::BootstrapFixture::tearDown();
}

CPPUNIT_TEST_SUITE_REGISTRATION(ScExportTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
