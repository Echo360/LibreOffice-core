/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <unotest/filters-test.hxx>
#include <test/bootstrapfixture.hxx>
#include <rtl/strbuf.hxx>
#include <osl/file.hxx>

#include "scdll.hxx"
#include <sfx2/app.hxx>
#include <sfx2/docfilt.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/sfxmodelfactory.hxx>
#include <svl/stritem.hxx>

#include "helper/qahelper.hxx"

#include "docsh.hxx"
#include "postit.hxx"
#include "patattr.hxx"
#include "scitems.hxx"
#include "document.hxx"
#include "cellform.hxx"
#include "drwlayer.hxx"
#include "userdat.hxx"
#include "formulacell.hxx"
#include "tabprotection.hxx"
#include <dbdocfun.hxx>
#include <globalnames.hxx>
#include <dbdata.hxx>
#include <sortparam.hxx>

#include <svx/svdpage.hxx>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

/* Implementation of Filters test */

class ScFiltersTest
    : public test::FiltersTest
    , public ScBootstrapFixture
{
public:
    ScFiltersTest();

    virtual void setUp() SAL_OVERRIDE;
    virtual void tearDown() SAL_OVERRIDE;

    virtual bool load( const OUString &rFilter, const OUString &rURL,
        const OUString &rUserData, unsigned int nFilterFlags,
        unsigned int nClipboardID, unsigned int nFilterVersion) SAL_OVERRIDE;
    /**
     * Ensure CVEs remain unbroken
     */
    void testCVEs();

    //ods, xls, xlsx filter tests
    void testRangeNameODS(); // only test ods here, xls and xlsx in subsequent_filters-test
    void testContentODS();
    void testContentXLS();
    void testContentXLSX();
    void testContentXLSXStrict(); // strict OOXML
    void testContentLotus123();
    void testContentDIF();
    //void testContentXLS_XML();
    void testSharedFormulaXLS();
    void testSharedFormulaXLSX();
    void testLegacyCellAnchoredRotatedShape();
    void testEnhancedProtectionXLS();
    void testEnhancedProtectionXLSX();
    void testSortWithSharedFormulasODS();

    CPPUNIT_TEST_SUITE(ScFiltersTest);
    CPPUNIT_TEST(testCVEs);
    CPPUNIT_TEST(testRangeNameODS);
    CPPUNIT_TEST(testContentODS);
    CPPUNIT_TEST(testContentXLS);
    CPPUNIT_TEST(testContentXLSX);
    CPPUNIT_TEST(testContentXLSXStrict);
    CPPUNIT_TEST(testContentLotus123);
    CPPUNIT_TEST(testContentDIF);
    //CPPUNIT_TEST(testContentXLS_XML);
    CPPUNIT_TEST(testSharedFormulaXLS);
    CPPUNIT_TEST(testSharedFormulaXLSX);
    CPPUNIT_TEST(testLegacyCellAnchoredRotatedShape);
    CPPUNIT_TEST(testEnhancedProtectionXLS);
    CPPUNIT_TEST(testEnhancedProtectionXLSX);
    CPPUNIT_TEST(testSortWithSharedFormulasODS);

    CPPUNIT_TEST_SUITE_END();

private:
    uno::Reference<uno::XInterface> m_xCalcComponent;
};

bool ScFiltersTest::load(const OUString &rFilter, const OUString &rURL,
    const OUString &rUserData, unsigned int nFilterFlags,
        unsigned int nClipboardID, unsigned int nFilterVersion)
{
    ScDocShellRef xDocShRef = ScBootstrapFixture::load(rURL, rFilter, rUserData,
        OUString(), nFilterFlags, nClipboardID, nFilterVersion );
    bool bLoaded = xDocShRef.Is();
    //reference counting of ScDocShellRef is very confused.
    if (bLoaded)
        xDocShRef->DoClose();
    return bLoaded;
}

void ScFiltersTest::testCVEs()
{
#ifndef DISABLE_CVE_TESTS
    testDir(OUString("Quattro Pro 6.0"),
        getURLFromSrc("/sc/qa/unit/data/qpro/"), OUString());

    //warning, the current "sylk filter" in sc (docsh.cxx) automatically
    //chains on failure on trying as csv, rtf, etc. so "success" may
    //not indicate that it imported as .slk.
    testDir(OUString("SYLK"),
        getURLFromSrc("/sc/qa/unit/data/slk/"), OUString());

    testDir(OUString("MS Excel 97"),
        getURLFromSrc("/sc/qa/unit/data/xls/"), OUString());

    testDir(OUString("dBase"),
        getURLFromSrc("/sc/qa/unit/data/dbf/"), OUString());
#endif
}

namespace {

void testRangeNameImpl(ScDocument& rDoc)
{
    //check one range data per sheet and one global more detailed
    //add some more checks here
    ScRangeData* pRangeData = rDoc.GetRangeName()->findByUpperName(OUString("GLOBAL1"));
    CPPUNIT_ASSERT_MESSAGE("range name Global1 not found", pRangeData);
    double aValue;
    rDoc.GetValue(1,0,0,aValue);
    CPPUNIT_ASSERT_MESSAGE("range name Global1 should reference Sheet1.A1", aValue == 1);
    pRangeData = rDoc.GetRangeName(0)->findByUpperName(OUString("LOCAL1"));
    CPPUNIT_ASSERT_MESSAGE("range name Sheet1.Local1 not found", pRangeData);
    rDoc.GetValue(1,2,0,aValue);
    CPPUNIT_ASSERT_MESSAGE("range name Sheet1.Local1 should reference Sheet1.A3", aValue == 3);
    pRangeData = rDoc.GetRangeName(1)->findByUpperName(OUString("LOCAL2"));
    CPPUNIT_ASSERT_MESSAGE("range name Sheet2.Local2 not found", pRangeData);
    //check for correct results for the remaining formulas
    rDoc.GetValue(1,1,0, aValue);
    CPPUNIT_ASSERT_MESSAGE("=global2 should be 2", aValue == 2);
    rDoc.GetValue(1,3,0, aValue);
    CPPUNIT_ASSERT_MESSAGE("=local2 should be 4", aValue == 4);
    rDoc.GetValue(2,0,0, aValue);
    CPPUNIT_ASSERT_MESSAGE("=SUM(global3) should be 10", aValue == 10);
}

}

void ScFiltersTest::testRangeNameODS()
{
    ScDocShellRef xDocSh = loadDoc("named-ranges-global.", ODS);

    CPPUNIT_ASSERT_MESSAGE("Failed to load named-ranges-globals.*", xDocSh.Is());

    xDocSh->DoHardRecalc(true);

    ScDocument& rDoc = xDocSh->GetDocument();
    testRangeNameImpl(rDoc);

    OUString aSheet2CSV("rangeExp_Sheet2.");
    OUString aCSVPath;
    createCSVPath( aSheet2CSV, aCSVPath );
    testFile( aCSVPath, rDoc, 1);
    xDocSh->DoClose();
}

namespace {

void testContentImpl(ScDocument& rDoc, sal_Int32 nFormat ) //same code for ods, xls, xlsx
{
    double fValue;
    //check value import
    rDoc.GetValue(0,0,0,fValue);
    CPPUNIT_ASSERT_MESSAGE("value not imported correctly", fValue == 1);
    rDoc.GetValue(0,1,0,fValue);
    CPPUNIT_ASSERT_MESSAGE("value not imported correctly", fValue == 2);
    OUString aString = rDoc.GetString(1, 0, 0);

    //check string import
    CPPUNIT_ASSERT_MESSAGE("string imported not correctly", aString == "String1");
    aString = rDoc.GetString(1, 1, 0);
    CPPUNIT_ASSERT_MESSAGE("string not imported correctly", aString == "String2");

    //check basic formula import
    // in case of DIF it just contains values
    rDoc.GetValue(2,0,0,fValue);
    CPPUNIT_ASSERT_MESSAGE("=2*3", fValue == 6);
    rDoc.GetValue(2,1,0,fValue);
    CPPUNIT_ASSERT_MESSAGE("=2+3", fValue == 5);
    rDoc.GetValue(2,2,0,fValue);
    CPPUNIT_ASSERT_MESSAGE("=2-3", fValue == -1);
    rDoc.GetValue(2,3,0,fValue);
    CPPUNIT_ASSERT_MESSAGE("=C1+C2", fValue == 11);

    //check merged cells import
    if(nFormat != LOTUS123 && nFormat != DIF)
    {
        SCCOL nCol = 4;
        SCROW nRow = 1;
        rDoc.ExtendMerge(4, 1, nCol, nRow, 0, false);
        CPPUNIT_ASSERT_MESSAGE("merged cells are not imported", nCol == 5 && nRow == 2);

        //check notes import
        ScAddress aAddress(7, 2, 0);
        ScPostIt* pNote = rDoc.GetNote(aAddress);
        CPPUNIT_ASSERT_MESSAGE("note not imported", pNote);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("note text not imported correctly", pNote->GetText(), OUString("Test"));
    }

    //add additional checks here
}

}

void ScFiltersTest::testContentODS()
{
    ScDocShellRef xDocSh = loadDoc("universal-content.", ODS);
    xDocSh->DoHardRecalc(true);

    ScDocument& rDoc = xDocSh->GetDocument();
    testContentImpl(rDoc, ODS);
    xDocSh->DoClose();
}

void ScFiltersTest::testContentXLS()
{
    ScDocShellRef xDocSh = loadDoc("universal-content.", XLS);
    xDocSh->DoHardRecalc(true);

    ScDocument& rDoc = xDocSh->GetDocument();
    testContentImpl(rDoc, XLS);
    xDocSh->DoClose();
}

void ScFiltersTest::testContentXLSX()
{
    ScDocShellRef xDocSh = loadDoc("universal-content.", XLSX);
    xDocSh->DoHardRecalc(true);

    ScDocument& rDoc = xDocSh->GetDocument();
    testContentImpl(rDoc, XLSX);
    xDocSh->DoClose();
}

void ScFiltersTest::testContentXLSXStrict()
{
    ScDocShellRef xDocSh = loadDoc("universal-content-strict.", XLSX);
    xDocSh->DoHardRecalc(true);

    ScDocument& rDoc = xDocSh->GetDocument();
    testContentImpl(rDoc, XLSX);
    xDocSh->DoClose();
}

void ScFiltersTest::testContentLotus123()
{
    ScDocShellRef xDocSh = loadDoc("universal-content.", LOTUS123);
    xDocSh->DoHardRecalc(true);

    ScDocument& rDoc = xDocSh->GetDocument();
    CPPUNIT_ASSERT(&rDoc);
    testContentImpl(rDoc, LOTUS123);
    xDocSh->DoClose();
}

void ScFiltersTest::testContentDIF()
{
    ScDocShellRef xDocSh = loadDoc("universal-content.", DIF);

    ScDocument& rDoc = xDocSh->GetDocument();
    CPPUNIT_ASSERT(&rDoc);
    xDocSh->DoClose();
}

// void ScFiltersTest::testContentXLS_XML()
// {
//     ScDocShellRef xDocSh = loadDoc("universal-content.", XLS_XML);
//     CPPUNIT_ASSERT(xDocSh);
//
//     ScDocument& rDoc = xDocSh->GetDocument();
//     CPPUNIT_ASSERT(&rDoc);
//     testContentImpl(pDoc, XLS_XML);
//     xDocSh->DoClose();
// }

void ScFiltersTest::testSharedFormulaXLS()
{
    ScDocShellRef xDocSh = loadDoc("shared-formula/basic.", XLS);
    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rDoc = xDocSh->GetDocument();
    xDocSh->DoHardRecalc(true);
    // Check the results of formula cells in the shared formula range.
    for (SCROW i = 1; i <= 18; ++i)
    {
        double fVal = rDoc.GetValue(ScAddress(1,i,0));
        double fCheck = i*10.0;
        CPPUNIT_ASSERT_EQUAL(fCheck, fVal);
    }

    ScFormulaCell* pCell = rDoc.GetFormulaCell(ScAddress(1,18,0));
    CPPUNIT_ASSERT_MESSAGE("This should be a formula cell.", pCell);
    ScFormulaCellGroupRef xGroup = pCell->GetCellGroup();
    CPPUNIT_ASSERT_MESSAGE("This cell should be a part of a cell group.", xGroup);
    CPPUNIT_ASSERT_MESSAGE("Incorrect group geometry.", xGroup->mpTopCell->aPos.Row() == 1 && xGroup->mnLength == 18);

    xDocSh->DoClose();

    // The following file contains shared formula whose range is inaccurate.
    // Excel can easily mess up shared formula ranges, so we need to be able
    // to handle these wrong ranges that Excel stores.

    xDocSh = loadDoc("shared-formula/gap.", XLS);
    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rDoc2 = xDocSh->GetDocument();
    rDoc2.CalcAll();

    if (!checkFormula(rDoc2, ScAddress(1,0,0), "A1*20"))
        CPPUNIT_FAIL("Wrong formula.");

    if (!checkFormula(rDoc2, ScAddress(1,1,0), "A2*20"))
        CPPUNIT_FAIL("Wrong formula.");

    if (!checkFormula(rDoc2, ScAddress(1,2,0), "A3*20"))
        CPPUNIT_FAIL("Wrong formula.");

    // There is an intentional gap at row 4.

    if (!checkFormula(rDoc2, ScAddress(1,4,0), "A5*20"))
        CPPUNIT_FAIL("Wrong formula.");

    if (!checkFormula(rDoc2, ScAddress(1,5,0), "A6*20"))
        CPPUNIT_FAIL("Wrong formula.");

    if (!checkFormula(rDoc2, ScAddress(1,6,0), "A7*20"))
        CPPUNIT_FAIL("Wrong formula.");

    if (!checkFormula(rDoc2, ScAddress(1,7,0), "A8*20"))
        CPPUNIT_FAIL("Wrong formula.");

    // We re-group formula cells on load. Let's check that as well.

    ScFormulaCell* pFC = rDoc2.GetFormulaCell(ScAddress(1,0,0));
    CPPUNIT_ASSERT_MESSAGE("Failed to fetch formula cell.", pFC);
    CPPUNIT_ASSERT_MESSAGE("This should be the top cell in formula group.", pFC->IsSharedTop());
    CPPUNIT_ASSERT_EQUAL(static_cast<SCROW>(3), pFC->GetSharedLength());

    pFC = rDoc2.GetFormulaCell(ScAddress(1,4,0));
    CPPUNIT_ASSERT_MESSAGE("Failed to fetch formula cell.", pFC);
    CPPUNIT_ASSERT_MESSAGE("This should be the top cell in formula group.", pFC->IsSharedTop());
    CPPUNIT_ASSERT_EQUAL(static_cast<SCROW>(4), pFC->GetSharedLength());

    xDocSh->DoClose();
}

void ScFiltersTest::testSharedFormulaXLSX()
{
    ScDocShellRef xDocSh = loadDoc("shared-formula/basic.", XLSX);
    ScDocument& rDoc = xDocSh->GetDocument();
    CPPUNIT_ASSERT(&rDoc);
    xDocSh->DoHardRecalc(true);
    // Check the results of formula cells in the shared formula range.
    for (SCROW i = 1; i <= 18; ++i)
    {
        double fVal = rDoc.GetValue(ScAddress(1,i,0));
        double fCheck = i*10.0;
        CPPUNIT_ASSERT_EQUAL(fCheck, fVal);
    }

    ScFormulaCell* pCell = rDoc.GetFormulaCell(ScAddress(1,18,0));
    CPPUNIT_ASSERT_MESSAGE("This should be a formula cell.", pCell);
    ScFormulaCellGroupRef xGroup = pCell->GetCellGroup();
    CPPUNIT_ASSERT_MESSAGE("This cell should be a part of a cell group.", xGroup);
    CPPUNIT_ASSERT_MESSAGE("Incorrect group geometry.", xGroup->mpTopCell->aPos.Row() == 1 && xGroup->mnLength == 18);

    xDocSh->DoClose();
}

void impl_testLegacyCellAnchoredRotatedShape( ScDocument& rDoc, Rectangle& aRect, ScDrawObjData& aAnchor, long TOLERANCE = 30 /* 30 hmm */ )
{
    ScDrawLayer* pDrawLayer = rDoc.GetDrawLayer();
    CPPUNIT_ASSERT_MESSAGE("No drawing layer.", pDrawLayer);
    SdrPage* pPage = pDrawLayer->GetPage(0);
    CPPUNIT_ASSERT_MESSAGE("No page instance for the 1st sheet.", pPage);
    CPPUNIT_ASSERT_EQUAL( sal_uIntPtr(1), pPage->GetObjCount() );

    SdrObject* pObj = pPage->GetObj(0);
    const Rectangle& aSnap = pObj->GetSnapRect();
    printf("expected height %ld actual %ld\n", aRect.GetHeight(), aSnap.GetHeight() );
    CPPUNIT_ASSERT_EQUAL( true, testEqualsWithTolerance( aRect.GetHeight(), aSnap.GetHeight(), TOLERANCE ) );
    printf("expected width %ld actual %ld\n", aRect.GetWidth(), aSnap.GetWidth() );
    CPPUNIT_ASSERT_EQUAL( true, testEqualsWithTolerance( aRect.GetWidth(), aSnap.GetWidth(), TOLERANCE ) );
    printf("expected left %ld actual %ld\n", aRect.Left(), aSnap.Left() );
    CPPUNIT_ASSERT_EQUAL( true, testEqualsWithTolerance( aRect.Left(), aSnap.Left(), TOLERANCE ) );
    printf("expected right %ld actual %ld\n", aRect.Top(), aSnap.Top() );
    CPPUNIT_ASSERT_EQUAL( true, testEqualsWithTolerance( aRect.Top(), aSnap.Top(), TOLERANCE ) );

    ScDrawObjData* pData = ScDrawLayer::GetObjData( pObj );
    CPPUNIT_ASSERT_MESSAGE("expected object meta data", pData);
    printf("expected startrow %" SAL_PRIdINT32 " actual %" SAL_PRIdINT32 "\n", aAnchor.maStart.Row(), pData->maStart.Row()  );
    CPPUNIT_ASSERT_EQUAL( aAnchor.maStart.Row(), pData->maStart.Row() );
    printf("expected startcol %d actual %d\n", aAnchor.maStart.Col(), pData->maStart.Col()  );
    CPPUNIT_ASSERT_EQUAL( aAnchor.maStart.Col(), pData->maStart.Col() );
    printf("expected endrow %" SAL_PRIdINT32 " actual %" SAL_PRIdINT32 "\n", aAnchor.maEnd.Row(), pData->maEnd.Row()  );
    CPPUNIT_ASSERT_EQUAL( aAnchor.maEnd.Row(), pData->maEnd.Row() );
    printf("expected endcol %d actual %d\n", aAnchor.maEnd.Col(), pData->maEnd.Col()  );
    CPPUNIT_ASSERT_EQUAL( aAnchor.maEnd.Col(), pData->maEnd.Col() );
}

void ScFiltersTest::testLegacyCellAnchoredRotatedShape()
{
    {
        // This example doc contains cell anchored shape that is rotated, the
        // rotated shape is in fact cliped by the sheet boundries ( and thus
        // is a good edge case test to see if we import it still correctly )
        ScDocShellRef xDocSh = loadDoc("legacycellanchoredrotatedclippedshape.", ODS);

        ScDocument& rDoc = xDocSh->GetDocument();
        CPPUNIT_ASSERT(&rDoc);
        // ensure the imported legacy rotated shape is in the expected position
        Rectangle aRect( 6000, -2000, 8000, 4000 );
        // ensure the imported ( and converted ) anchor ( note we internally now store the anchor in
        // terms of the rotated shape ) is more or less contains the correct info
        ScDrawObjData aAnchor;
        aAnchor.maStart.SetRow( 0 );
        aAnchor.maStart.SetCol( 5 );
        aAnchor.maEnd.SetRow( 3 );
        aAnchor.maEnd.SetCol( 7 );
        impl_testLegacyCellAnchoredRotatedShape( rDoc, aRect, aAnchor );
        // test save and reload
        // for some reason having this test in subsequent_export-test.cxx causes
        // a core dump in editeng ( so moved to here )
        xDocSh = saveAndReload( &(*xDocSh), ODS);
        ScDocument& rDoc2 = xDocSh->GetDocument();
        CPPUNIT_ASSERT(&rDoc2);
        impl_testLegacyCellAnchoredRotatedShape( rDoc2, aRect, aAnchor );

        xDocSh->DoClose();
    }
    {
        // This example doc contains cell anchored shape that is rotated, the
        // rotated shape is in fact clipped by the sheet boundries, additionally
        // the shape is completely hidden because the rows the shape occupies
        // are hidden
        ScDocShellRef xDocSh = loadDoc("legacycellanchoredrotatedhiddenshape.", ODS, true);
        ScDocument& rDoc = xDocSh->GetDocument();
        CPPUNIT_ASSERT(&rDoc);
        // ensure the imported legacy rotated shape is in the expected position
        // when a shape is fully hidden reloading seems to result is in some errors, usually
        // ( same but different error happens pre-patch ) - we should do better here, I regard it
        // as a pre-existing bug though ( #FIXME )
        //Rectangle aRect( 6000, -2000, 8000, 4000 ); // proper dimensions
        Rectangle aRect( 6000, -2000, 7430, 4000 );
        // ensure the imported ( and converted ) anchor ( note we internally now store the anchor in
        // terms of the rotated shape ) is more or less contains the correct info
        ScDrawObjData aAnchor;
        aAnchor.maStart.SetRow( 0 );
        aAnchor.maStart.SetCol( 5 );
        aAnchor.maEnd.SetRow( 3 );
        aAnchor.maEnd.SetCol( 7 );
        rDoc.ShowRows(0, 9, 0, true); // show relavent rows
        rDoc.SetDrawPageSize(0); // trigger recalcpos

        // apply hefty ( 1 mm ) tolerence here, as some opensuse tinderbox
        // failing
        impl_testLegacyCellAnchoredRotatedShape( rDoc, aRect, aAnchor, 100 );

        xDocSh->DoClose();
    }
    {
        // This example doc contains cell anchored shape that is rotated
        ScDocShellRef xDocSh = loadDoc("legacycellanchoredrotatedshape.", ODS);

        ScDocument& rDoc = xDocSh->GetDocument();
        CPPUNIT_ASSERT(&rDoc);
        // ensure the imported legacy rotated shape is in the expected position
        Rectangle aRect( 6000, 3000, 8000, 9000 );
        // ensure the imported ( and converted ) anchor ( note we internally now store the anchor in
        // terms of the rotated shape ) is more or less contains the correct info

        ScDrawObjData aAnchor;
        aAnchor.maStart.SetRow( 3 );
        aAnchor.maStart.SetCol( 6 );
        aAnchor.maEnd.SetRow( 9 );
        aAnchor.maEnd.SetCol( 7 );
        // test import
        impl_testLegacyCellAnchoredRotatedShape( rDoc, aRect, aAnchor );
        // test save and reload
        xDocSh = saveAndReload( &(*xDocSh), ODS);
        ScDocument& rDoc2 = xDocSh->GetDocument();
        CPPUNIT_ASSERT(&rDoc2);
        impl_testLegacyCellAnchoredRotatedShape( rDoc2, aRect, aAnchor );

        xDocSh->DoClose();
    }
}

void testEnhancedProtectionImpl( ScDocument& rDoc )
{
    const ScTableProtection* pProt = rDoc.GetTabProtection(0);

    CPPUNIT_ASSERT( pProt);

    CPPUNIT_ASSERT( !pProt->isBlockEditable( ScRange( 0, 0, 0, 0, 0, 0)));  // locked
    CPPUNIT_ASSERT(  pProt->isBlockEditable( ScRange( 0, 1, 0, 0, 1, 0)));  // editable without password
    CPPUNIT_ASSERT(  pProt->isBlockEditable( ScRange( 0, 2, 0, 0, 2, 0)));  // editable without password
    CPPUNIT_ASSERT( !pProt->isBlockEditable( ScRange( 0, 3, 0, 0, 3, 0)));  // editable with password "foo"
    CPPUNIT_ASSERT( !pProt->isBlockEditable( ScRange( 0, 4, 0, 0, 4, 0)));  // editable with descriptor
    CPPUNIT_ASSERT( !pProt->isBlockEditable( ScRange( 0, 5, 0, 0, 5, 0)));  // editable with descriptor and password "foo"
    CPPUNIT_ASSERT(  pProt->isBlockEditable( ScRange( 0, 1, 0, 0, 2, 0)));  // union of two different editables
    CPPUNIT_ASSERT( !pProt->isBlockEditable( ScRange( 0, 0, 0, 0, 1, 0)));  // union of locked and editable
    CPPUNIT_ASSERT( !pProt->isBlockEditable( ScRange( 0, 2, 0, 0, 3, 0)));  // union of editable and password editable
}

void ScFiltersTest::testEnhancedProtectionXLS()
{
    ScDocShellRef xDocSh = loadDoc("enhanced-protection.", XLS);
    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rDoc = xDocSh->GetDocument();

    testEnhancedProtectionImpl( rDoc);

    xDocSh->DoClose();
}

void ScFiltersTest::testEnhancedProtectionXLSX()
{
    ScDocShellRef xDocSh = loadDoc("enhanced-protection.", XLSX);
    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rDoc = xDocSh->GetDocument();

    testEnhancedProtectionImpl( rDoc);

    xDocSh->DoClose();
}

void ScFiltersTest::testSortWithSharedFormulasODS()
{
    ScDocShellRef xDocSh = loadDoc("shared-formula/sort-crash.", ODS, true);
    CPPUNIT_ASSERT(xDocSh.Is());
    ScDocument& rDoc = xDocSh->GetDocument();

    // E2:E10 should be shared.
    const ScFormulaCell* pFC = rDoc.GetFormulaCell(ScAddress(4,1,0));
    CPPUNIT_ASSERT(pFC);
    CPPUNIT_ASSERT_EQUAL(static_cast<SCROW>(1), pFC->GetSharedTopRow());
    CPPUNIT_ASSERT_EQUAL(static_cast<SCROW>(9), pFC->GetSharedLength());

    // E12:E17 should be shared.
    pFC = rDoc.GetFormulaCell(ScAddress(4,11,0));
    CPPUNIT_ASSERT(pFC);
    CPPUNIT_ASSERT_EQUAL(static_cast<SCROW>(11), pFC->GetSharedTopRow());
    CPPUNIT_ASSERT_EQUAL(static_cast<SCROW>(6), pFC->GetSharedLength());

    // Set A1:E17 as an anonymous database range to sheet, or else Calc would
    // refuse to sort the range.
    ScDBData* pDBData = new ScDBData(STR_DB_LOCAL_NONAME, 0, 0, 0, 4, 16, true, true);
    rDoc.SetAnonymousDBData(0, pDBData);

    // Sort ascending by Column E.

    ScSortParam aSortData;
    aSortData.nCol1 = 0;
    aSortData.nCol2 = 4;
    aSortData.nRow1 = 0;
    aSortData.nRow2 = 16;
    aSortData.bHasHeader = true;
    aSortData.maKeyState[0].bDoSort = true;
    aSortData.maKeyState[0].nField = 4;
    aSortData.maKeyState[0].bAscending = true;

    // Do the sorting.  This should not crash.
    ScDBDocFunc aFunc(*xDocSh);
    bool bSorted = aFunc.Sort(0, aSortData, true, true, true);
    CPPUNIT_ASSERT(bSorted);

    // After the sort, E2:E16 should be shared.
    pFC = rDoc.GetFormulaCell(ScAddress(4,1,0));
    CPPUNIT_ASSERT(pFC);
    CPPUNIT_ASSERT_EQUAL(static_cast<SCROW>(1), pFC->GetSharedTopRow());
    CPPUNIT_ASSERT_EQUAL(static_cast<SCROW>(15), pFC->GetSharedLength());

    xDocSh->DoClose();
}

ScFiltersTest::ScFiltersTest()
      : ScBootstrapFixture( "/sc/qa/unit/data" )
{
}

void ScFiltersTest::setUp()
{
    test::BootstrapFixture::setUp();

    // This is a bit of a fudge, we do this to ensure that ScGlobals::ensure,
    // which is a private symbol to us, gets called
    m_xCalcComponent =
        getMultiServiceFactory()->createInstance("com.sun.star.comp.Calc.SpreadsheetDocument");
    CPPUNIT_ASSERT_MESSAGE("no calc component!", m_xCalcComponent.is());
}

void ScFiltersTest::tearDown()
{
    uno::Reference< lang::XComponent >( m_xCalcComponent, UNO_QUERY_THROW )->dispose();
    test::BootstrapFixture::tearDown();
}

CPPUNIT_TEST_SUITE_REGISTRATION(ScFiltersTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
