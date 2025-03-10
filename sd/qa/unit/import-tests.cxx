/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "sdmodeltestbase.hxx"

#include <svl/stritem.hxx>
#include <editeng/editobj.hxx>
#include <editeng/outlobj.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/escapementitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/wghtitem.hxx>
#include <editeng/numitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/postitem.hxx>
#include <rsc/rscsfx.hxx>

#include <svx/svdotext.hxx>
#include <svx/svdoashp.hxx>
#include <svx/svdograf.hxx>
#include <svx/svdogrp.hxx>
#include <svx/svdomedia.hxx>
#include <svx/svdoole2.hxx>
#include <svx/svdotable.hxx>
#include <svx/xflclit.hxx>
#include <animations/animationnodehelper.hxx>

#include <com/sun/star/drawing/XDrawPage.hpp>
#include <com/sun/star/drawing/XDrawPagesSupplier.hpp>
#include <com/sun/star/animations/XAnimationNodeSupplier.hpp>
#include <com/sun/star/animations/XAnimationNode.hpp>
#include <com/sun/star/animations/XAnimate.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/chart/XChartDocument.hpp>
#include <com/sun/star/chart2/XChartDocument.hpp>
#include <com/sun/star/chart2/XDataSeriesContainer.hpp>
#include <com/sun/star/chart2/XCoordinateSystemContainer.hpp>
#include <com/sun/star/chart2/XChartTypeContainer.hpp>
#include <com/sun/star/chart2/data/XLabeledDataSequence.hpp>
#include <com/sun/star/chart2/data/XDataSequence.hpp>
#include <com/sun/star/chart2/data/XNumericalDataSequence.hpp>

using namespace ::com::sun::star;

/// Impress import filters tests.
class SdFiltersTest : public SdModelTestBase
{
public:
    void testDocumentLayout();
    void testSmoketest();
    void testN759180();
    void testN778859();
    void testFdo64512();
    void testFdo71075();
    void testN828390_2();
    void testN828390_3();
    void testFdo68594();
    void testFdo72998();
    void testFdo77027();
    void testStrictOOXML();
    void testN862510_1();
    void testN862510_2();
    void testN862510_4();
    void testBnc870237();
    void testBnc887225();

    CPPUNIT_TEST_SUITE(SdFiltersTest);
    CPPUNIT_TEST(testDocumentLayout);
    CPPUNIT_TEST(testSmoketest);
    CPPUNIT_TEST(testN759180);
    CPPUNIT_TEST(testN778859);
    CPPUNIT_TEST(testFdo64512);
    CPPUNIT_TEST(testFdo71075);
    CPPUNIT_TEST(testN828390_2);
    CPPUNIT_TEST(testN828390_3);
    CPPUNIT_TEST(testFdo68594);
    CPPUNIT_TEST(testFdo72998);
    CPPUNIT_TEST(testFdo77027);
    CPPUNIT_TEST(testStrictOOXML);
    CPPUNIT_TEST(testN862510_1);
    CPPUNIT_TEST(testN862510_2);
    CPPUNIT_TEST(testN862510_4);
    CPPUNIT_TEST(testBnc870237);
    CPPUNIT_TEST(testBnc887225);

    CPPUNIT_TEST_SUITE_END();
};

/** Test document against a reference XML dump of shapes.

If you want to update one of these tests, or add a new one, set the nUpdateMe
to the index of the test, and the dump XML's will be created (or rewritten)
instead of checking. Use with care - when the test is failing, first find out
why, instead of just updating .xml's blindly.

Example: Let's say you are adding a test called fdoABCD.pptx.  You'll place it
to the data/ subdirectory, and will add an entry to aFilesToCompare below,
the 3rd parameter is for export test - can be -1 (don't export), ODP, PPT or PPTX
like:

        { "fdoABCD.pptx", "xml/fdoABCD_", PPTX },

and will count the index in the aFilesToCompare structure (1st is 0, 2nd is 1,
etc.)  Temporarily you'll set nUpdateMe to this index (instead of -1), and run

make sd

This will generate the sd/qa/unit/data/xml/fdoABCD_*.xml for you.  Now you
will change nUpdateMe back to -1, and commit your fdoABCD.pptx test, the
xml/fdoABCD_*.xml dumps, and the aFilesToCompare addition in one commit.

As the last step, you will revert your fix and do 'make sd' again, to check
that without your fix, the unit test breaks.  Then clean up, and push :-)

NOTE: This approach is suitable only for tests of fixes that actually change
the layout - best to check by reverting your fix locally after having added
the test, and re-running; it should break.
*/
void SdFiltersTest::testDocumentLayout()
{
    struct { const char *pInput, *pDump; sal_Int32 nFormat; sal_Int32 nExportType; } aFilesToCompare[] =
    {
        { "odp/shapes-test.odp", "xml/shapes-test_page", ODP, -1 },
        { "fdo47434.pptx", "xml/fdo47434_", PPTX, -1 },
        { "n758621.ppt", "xml/n758621_", PPT, -1 },
        { "fdo64586.ppt", "xml/fdo64586_", PPT, -1 },
        { "n819614.pptx", "xml/n819614_", PPTX, -1 },
        { "n820786.pptx", "xml/n820786_", PPTX, -1 },
        { "n762695.pptx", "xml/n762695_", PPTX, -1 },
        { "n593612.pptx", "xml/n593612_", PPTX, -1 },
        { "fdo71434.pptx", "xml/fdo71434_", PPTX, -1 },
        // { "pptx/n828390.pptx", "pptx/xml/n828390_", PPTX, PPTX }, // Example
    };

    for ( int i = 0; i < static_cast< int >( SAL_N_ELEMENTS( aFilesToCompare ) ); ++i )
    {
        int nUpdateMe = -1; // index of test we want to update; supposedly only when the test is created

        ::sd::DrawDocShellRef xDocShRef = loadURL( getURLFromSrc( "/sd/qa/unit/data/" ) + OUString::createFromAscii( aFilesToCompare[i].pInput ), aFilesToCompare[i].nFormat );
        if( aFilesToCompare[i].nExportType >= 0 )
            xDocShRef = saveAndReload( xDocShRef, aFilesToCompare[i].nExportType );
        compareWithShapesDump( xDocShRef,
                getPathFromSrc( "/sd/qa/unit/data/" ) + OUString::createFromAscii( aFilesToCompare[i].pDump ),
                i == nUpdateMe );
    }
}

void SdFiltersTest::testSmoketest()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/smoketest.pptx"), PPTX);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );

    // cf. SdrModel svx/svdmodel.hxx ...

    CPPUNIT_ASSERT_MESSAGE( "wrong page count", pDoc->GetPageCount() == 3);

    const SdrPage *pPage = pDoc->GetPage (1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );

    sal_uIntPtr nObjs = pPage->GetObjCount();
    for (sal_uIntPtr i = 0; i < nObjs; i++)
    {
        SdrObject *pObj = pPage->GetObj(i);
        SdrObjKind eKind = (SdrObjKind) pObj->GetObjIdentifier();
        SdrTextObj *pTxt = dynamic_cast<SdrTextObj *>( pObj );
        (void)pTxt; (void)eKind;
    }

    CPPUNIT_ASSERT_MESSAGE( "changed", !pDoc->IsChanged() );

    xDocShRef->DoClose();
}

void SdFiltersTest::testN759180()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/n759180.pptx"), PPTX);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage (1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );

    //sal_uIntPtr nObjs = pPage->GetObjCount();
    //for (sal_uIntPtr i = 0; i < nObjs; i++)
    {
        // Get the object
        SdrObject *pObj = pPage->GetObj(0);
        SdrTextObj *pTxtObj = dynamic_cast<SdrTextObj *>( pObj );
        CPPUNIT_ASSERT(pTxtObj);
        std::vector<EECharAttrib> rLst;
        const EditTextObject& aEdit = pTxtObj->GetOutlinerParaObject()->GetTextObject();
        const SvxULSpaceItem *pULSpace = dynamic_cast<const SvxULSpaceItem *>(aEdit.GetParaAttribs(0).GetItem(EE_PARA_ULSPACE));
        CPPUNIT_ASSERT(pULSpace);
        CPPUNIT_ASSERT_MESSAGE( "Para bottom spacing is wrong!", pULSpace->GetLower() == 0 );
        aEdit.GetCharAttribs(1, rLst);
        for( std::vector<EECharAttrib>::reverse_iterator it = rLst.rbegin(); it!=rLst.rend(); ++it)
        {
            const SvxFontHeightItem * pFontHeight = dynamic_cast<const SvxFontHeightItem *>((*it).pAttr);
            if(pFontHeight)
            {
                // nStart == 9
                // font height = 5 => 5*2540/72
                CPPUNIT_ASSERT_MESSAGE( "Font height is wrong", pFontHeight->GetHeight() == 176 );
                break;
            }
        }
    }

    xDocShRef->DoClose();
}

void SdFiltersTest::testN862510_1()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL( getURLFromSrc("/sd/qa/unit/data/pptx/n862510_1.pptx"), PPTX );

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage(1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );
    {
        std::vector<EECharAttrib> rLst;
        SdrObject *pObj = pPage->GetObj( 0 );
        SdrTextObj *pTxtObj = dynamic_cast<SdrTextObj *>( pObj );
        CPPUNIT_ASSERT( pTxtObj );
        const EditTextObject& aEdit = pTxtObj->GetOutlinerParaObject()->GetTextObject();
        aEdit.GetCharAttribs( 0, rLst );
        for( std::vector<EECharAttrib>::reverse_iterator it = rLst.rbegin(); it!=rLst.rend(); ++it)
        {
            const SvxEscapementItem *pFontEscapement = dynamic_cast<const SvxEscapementItem *>( (*it).pAttr );
            CPPUNIT_ASSERT_MESSAGE( "Baseline attribute not handled properly", !( pFontEscapement && pFontEscapement->GetProp() != 100 ) );
        }
    }

    xDocShRef->DoClose();
}

void SdFiltersTest::testN862510_2()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL( getURLFromSrc("/sd/qa/unit/data/pptx/n862510_2.pptx"), PPTX );

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage(1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );
    {
        SdrObjGroup *pGrpObj = dynamic_cast<SdrObjGroup *>( pPage->GetObj( 0 ) );
        CPPUNIT_ASSERT( pGrpObj );
        SdrObjCustomShape *pObj = dynamic_cast<SdrObjCustomShape *>( pGrpObj->GetSubList()->GetObj( 0 ) );
        CPPUNIT_ASSERT( pObj );
        CPPUNIT_ASSERT_MESSAGE( "Wrong Text Rotation!", pObj->GetExtraTextRotation( true ) == 90 );
    }

    xDocShRef->DoClose();
}

void SdFiltersTest::testN862510_4()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL( getURLFromSrc("/sd/qa/unit/data/pptx/n862510_4.pptx"), PPTX );

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage( 1 );
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );
    {
        std::vector<EECharAttrib> rLst;
        SdrObject *pObj = pPage->GetObj( 0 );
        SdrTextObj *pTxtObj = dynamic_cast<SdrTextObj *>( pObj );
        CPPUNIT_ASSERT( pTxtObj );
        const EditTextObject& aEdit = pTxtObj->GetOutlinerParaObject()->GetTextObject();
        aEdit.GetCharAttribs( 0, rLst );
        for( std::vector<EECharAttrib>::reverse_iterator it = rLst.rbegin(); it != rLst.rend(); ++it )
        {
            const SvxColorItem *pC = dynamic_cast<const SvxColorItem *>( (*it).pAttr );
            CPPUNIT_ASSERT_MESSAGE( "gradfill for text color not handled!", !( pC && pC->GetValue().GetColor() == 0 ) );
        }
    }

    xDocShRef->DoClose();
}

void SdFiltersTest::testN828390_2()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL( getURLFromSrc("/sd/qa/unit/data/pptx/n828390_2.pptx"), PPTX );

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage(1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );

    SdrObject *pObj = pPage->GetObj(0);
    SdrTextObj *pTxtObj = dynamic_cast<SdrTextObj *>( pObj );
    CPPUNIT_ASSERT( pTxtObj );
    const EditTextObject& aEdit = pTxtObj->GetOutlinerParaObject()->GetTextObject();
    CPPUNIT_ASSERT(aEdit.GetText(0) == OUString("Linux  "));
    CPPUNIT_ASSERT(aEdit.GetText(1) == OUString("Standard Platform"));

    xDocShRef->DoClose();
}

void SdFiltersTest::testN828390_3()
{
    bool bPassed = true;
    ::sd::DrawDocShellRef xDocShRef = loadURL( getURLFromSrc("/sd/qa/unit/data/pptx/n828390_3.pptx"), PPTX );

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage(1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );

    SdrObject *pObj = pPage->GetObj(0);
    SdrTextObj *pTxtObj = dynamic_cast<SdrTextObj *>( pObj );
    CPPUNIT_ASSERT( pTxtObj );
    const EditTextObject& aEdit = pTxtObj->GetOutlinerParaObject()->GetTextObject();
    std::vector<EECharAttrib> rLst;
    aEdit.GetCharAttribs(1, rLst);
    for( std::vector<EECharAttrib>::reverse_iterator it = rLst.rbegin(); it!=rLst.rend(); ++it)
    {
        const SvxEscapementItem *pFontEscapement = dynamic_cast<const SvxEscapementItem *>((*it).pAttr);
        if(pFontEscapement)
        {
            if( pFontEscapement->GetEsc() != 0 )
            {
                bPassed = false;
                break;
            }
        }
    }
    CPPUNIT_ASSERT_MESSAGE("CharEscapment not imported properly", bPassed);

    xDocShRef->DoClose();
}

void SdFiltersTest::testN778859()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/pptx/n778859.pptx"), PPTX);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage(1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );
    {
        // Get the object
        SdrObject *pObj = pPage->GetObj(1);
        SdrTextObj *pTxtObj = dynamic_cast<SdrTextObj *>( pObj );
        CPPUNIT_ASSERT_MESSAGE( "no text object", pTxtObj != NULL);
        CPPUNIT_ASSERT(!pTxtObj->IsAutoFit());
    }

    xDocShRef->DoClose();
}

void SdFiltersTest::testFdo68594()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/ppt/fdo68594.ppt"), PPT);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = &(pDoc->GetPage (1)->TRG_GetMasterPage());
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );
    SdrObject *pObj = pPage->GetObj(1);
    SdrTextObj *pTxtObj = dynamic_cast<SdrTextObj *>( pObj );
    CPPUNIT_ASSERT_MESSAGE( "no text object", pTxtObj != NULL);
    const SvxColorItem *pC = dynamic_cast<const SvxColorItem *>(&pTxtObj->GetMergedItem(EE_CHAR_COLOR));
    CPPUNIT_ASSERT_MESSAGE( "no color item", pC != NULL);
    // Color should be black
    CPPUNIT_ASSERT_MESSAGE( "Placeholder color mismatch", pC->GetValue().GetColor() == 0);

    xDocShRef->DoClose();
}

void SdFiltersTest::testFdo72998()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/pptx/cshapes.pptx"), PPTX);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage(1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );
    {
        SdrObjCustomShape *pObj = dynamic_cast<SdrObjCustomShape *>(pPage->GetObj(2));
        CPPUNIT_ASSERT( pObj );
        const SdrCustomShapeGeometryItem& rGeometryItem = (const SdrCustomShapeGeometryItem&)pObj->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY );
        const ::com::sun::star::uno::Any* pViewBox = ((SdrCustomShapeGeometryItem&)rGeometryItem).GetPropertyValueByName( OUString( "ViewBox" ) );
        CPPUNIT_ASSERT_MESSAGE( "Missing ViewBox", pViewBox );
        com::sun::star::awt::Rectangle aViewBox;
        CPPUNIT_ASSERT( (*pViewBox >>= aViewBox ) );
        CPPUNIT_ASSERT_MESSAGE( "Width should be zero - for forcing scale to 1", !aViewBox.Width );
        CPPUNIT_ASSERT_MESSAGE( "Height should be zero - for forcing scale to 1", !aViewBox.Height );
    }

    xDocShRef->DoClose();
}

// FIXME copypasta
std::ostream& operator<<(std::ostream& rStrm, const Color& rColor)
{
    rStrm << "Color: R:" << rColor.GetRed() << " G:" << rColor.GetGreen() << " B: " << rColor.GetBlue();
    return rStrm;
}

void SdFiltersTest::testFdo77027()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/odp/fdo77027.odp"), ODP);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage(1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );
    {
        SdrOle2Obj *const pObj = dynamic_cast<SdrOle2Obj*>(pPage->GetObj(0));
        CPPUNIT_ASSERT(pObj);

        // check that the fill style/color was actually imported
        const XFillStyleItem& rStyleItem = dynamic_cast<const XFillStyleItem&>(
                pObj->GetMergedItem(XATTR_FILLSTYLE));
        CPPUNIT_ASSERT_EQUAL(drawing::FillStyle_SOLID, rStyleItem.GetValue());
        const XFillColorItem& rColorItem = dynamic_cast<const XFillColorItem&>(
                pObj->GetMergedItem(XATTR_FILLCOLOR));
        CPPUNIT_ASSERT_EQUAL(Color(0xff6600), rColorItem.GetColorValue());
    }

    xDocShRef->DoClose();
}

void SdFiltersTest::testFdo64512()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/fdo64512.odp"), ODP);

    uno::Reference< drawing::XDrawPagesSupplier > xDoc(
        xDocShRef->GetDoc()->getUnoModel(), uno::UNO_QUERY_THROW );
    CPPUNIT_ASSERT_MESSAGE( "not exactly one page", xDoc->getDrawPages()->getCount() == 1 );

    uno::Reference< drawing::XDrawPage > xPage(
        xDoc->getDrawPages()->getByIndex(0), uno::UNO_QUERY_THROW );
    CPPUNIT_ASSERT_MESSAGE( "no exactly three shapes", xPage->getCount() == 3 );

    uno::Reference< beans::XPropertySet > xConnectorShape(
        xPage->getByIndex(2), uno::UNO_QUERY );
    CPPUNIT_ASSERT_MESSAGE( "no connector shape", xConnectorShape.is() );

    uno::Reference< beans::XPropertySet > xSvgShape(
        xConnectorShape->getPropertyValue("StartShape"), uno::UNO_QUERY );
    CPPUNIT_ASSERT_MESSAGE( "no start shape", xSvgShape.is() );

    uno::Reference< beans::XPropertySet > xCustomShape(
        xConnectorShape->getPropertyValue("EndShape"), uno::UNO_QUERY );
    CPPUNIT_ASSERT_MESSAGE( "no end shape", xCustomShape.is() );

    uno::Reference< animations::XAnimationNodeSupplier > xAnimNodeSupplier(
        xPage, uno::UNO_QUERY_THROW );
    uno::Reference< animations::XAnimationNode > xRootNode(
        xAnimNodeSupplier->getAnimationNode() );
    std::vector< uno::Reference< animations::XAnimationNode > > aAnimVector;
    anim::create_deep_vector(xRootNode, aAnimVector);
    CPPUNIT_ASSERT_MESSAGE( "not 8 animation nodes", aAnimVector.size() == 8 );

    uno::Reference< animations::XAnimate > xNode(
        aAnimVector[7], uno::UNO_QUERY_THROW );
    uno::Reference< drawing::XShape > xTargetShape(
        xNode->getTarget(), uno::UNO_QUERY_THROW );
    CPPUNIT_ASSERT_MESSAGE( "inner node not referencing svg shape",
                            xTargetShape != xSvgShape );

    xDocShRef->DoClose();
}

// Unit test for importing charts
void SdFiltersTest::testFdo71075()
{
    double values[] = { 12.0, 13.0, 14.0 };
    ::com::sun::star::uno::Any aAny;
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/fdo71075.odp"), ODP);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    uno::Reference< drawing::XDrawPagesSupplier > xDoc(xDocShRef->GetDoc()->getUnoModel(), uno::UNO_QUERY_THROW );
    uno::Reference< drawing::XDrawPage > xPage(xDoc->getDrawPages()->getByIndex(0), uno::UNO_QUERY_THROW );
    uno::Reference< drawing::XShape > xShape(xPage->getByIndex(0), uno::UNO_QUERY_THROW );
    CPPUNIT_ASSERT_MESSAGE( "failed to load shape", xShape.is() );

    uno::Reference< beans::XPropertySet > xPropSet( xShape, uno::UNO_QUERY );
    aAny = xPropSet->getPropertyValue( OUString("Model") );
    CPPUNIT_ASSERT_MESSAGE( "failed to load shape", aAny.hasValue() );

    uno::Reference< chart::XChartDocument > xChartDoc;
    aAny >>= xChartDoc;
    CPPUNIT_ASSERT_MESSAGE( "failed to load chart", xChartDoc.is() );
    uno::Reference< chart2::XChartDocument > xChart2Doc( xChartDoc, uno::UNO_QUERY );
    CPPUNIT_ASSERT_MESSAGE( "failed to load chart", xChart2Doc.is() );

    uno::Reference< chart2::XCoordinateSystemContainer > xBCooSysCnt( xChart2Doc->getFirstDiagram(), uno::UNO_QUERY );
    uno::Sequence< uno::Reference< chart2::XCoordinateSystem > > aCooSysSeq( xBCooSysCnt->getCoordinateSystems());
    uno::Reference< chart2::XChartTypeContainer > xCTCnt( aCooSysSeq[0], uno::UNO_QUERY );

    uno::Reference< chart2::XDataSeriesContainer > xDSCnt( xCTCnt->getChartTypes()[0], uno::UNO_QUERY );
    CPPUNIT_ASSERT_MESSAGE( "failed to load data series", xDSCnt.is() );
    uno::Sequence< uno::Reference< chart2::XDataSeries > > aSeriesSeq( xDSCnt->getDataSeries());
    CPPUNIT_ASSERT_MESSAGE( "Invalid Series count", aSeriesSeq.getLength() == 1);
    uno::Reference< chart2::data::XDataSource > xSource( aSeriesSeq[0], uno::UNO_QUERY );
    uno::Sequence< uno::Reference< chart2::data::XLabeledDataSequence > > aSeqCnt(xSource->getDataSequences());
    CPPUNIT_ASSERT_MESSAGE( "Invalid Series count", aSeqCnt.getLength() == 1);
    uno::Reference< chart2::data::XDataSequence > xValueSeq( aSeqCnt[0]->getValues() );
    CPPUNIT_ASSERT_MESSAGE( "Invalid Data count", xValueSeq->getData().getLength() == sizeof(values)/(sizeof(double)));
    uno::Reference< chart2::data::XNumericalDataSequence > xNumSeq( xValueSeq, uno::UNO_QUERY );
    uno::Sequence< double > aValues( xNumSeq->getNumericalData());
    for(sal_Int32 i=0;i<xValueSeq->getData().getLength();i++)
        CPPUNIT_ASSERT_MESSAGE( "Invalid Series count", aValues.getConstArray()[i] == values[i]);

    xDocShRef->DoClose();
}

void SdFiltersTest::testStrictOOXML()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/strict_ooxml.pptx"), PPTX);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    uno::Reference< drawing::XDrawPagesSupplier > xDoc(xDocShRef->GetDoc()->getUnoModel(), uno::UNO_QUERY_THROW );
    uno::Reference< drawing::XDrawPage > xPage(xDoc->getDrawPages()->getByIndex(0), uno::UNO_QUERY_THROW );
    uno::Reference< drawing::XShape > xShape(xPage->getByIndex(0), uno::UNO_QUERY_THROW );
    CPPUNIT_ASSERT_MESSAGE( "failed to load shape", xShape.is() );

    xDocShRef->DoClose();
}

void SdFiltersTest::testBnc870237()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL(getURLFromSrc("/sd/qa/unit/data/pptx/bnc870237.pptx"), PPTX);

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage (1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );

    // Simulate a:ext inside dsp:txXfrm with changing the lower distance
    const SdrObjGroup* pObj = dynamic_cast<SdrObjGroup*>( pPage->GetObj( 0 ) );
    CPPUNIT_ASSERT_MESSAGE( "no object", pObj != NULL);
    CPPUNIT_ASSERT_EQUAL( sal_Int32(0), (static_cast< const SdrMetricItem& >(pObj->GetMergedItem(SDRATTR_TEXT_UPPERDIST))).GetValue());
    CPPUNIT_ASSERT_EQUAL( sal_Int32(9919), (static_cast< const SdrMetricItem& >(pObj->GetMergedItem(SDRATTR_TEXT_LOWERDIST))).GetValue());
    CPPUNIT_ASSERT_EQUAL( sal_Int32(0), (static_cast< const SdrMetricItem& >(pObj->GetMergedItem(SDRATTR_TEXT_RIGHTDIST))).GetValue());
    CPPUNIT_ASSERT_EQUAL( sal_Int32(0), (static_cast< const SdrMetricItem& >(pObj->GetMergedItem(SDRATTR_TEXT_LEFTDIST))).GetValue());

    xDocShRef->DoClose();
}

void SdFiltersTest::testBnc887225()
{
    ::sd::DrawDocShellRef xDocShRef = loadURL( getURLFromSrc("/sd/qa/unit/data/pptx/bnc887225.pptx"), PPTX );
    // In the document, lastRow and lastCol table properties are used.
    // Make sure styles are set properly for individual cells.

    SdDrawDocument *pDoc = xDocShRef->GetDoc();
    CPPUNIT_ASSERT_MESSAGE( "no document", pDoc != NULL );
    const SdrPage *pPage = pDoc->GetPage(1);
    CPPUNIT_ASSERT_MESSAGE( "no page", pPage != NULL );

    sdr::table::SdrTableObj *pTableObj = dynamic_cast<sdr::table::SdrTableObj*>(pPage->GetObj(2));
    CPPUNIT_ASSERT( pTableObj );
    uno::Reference< table::XCellRange > xTable(pTableObj->getTable(), uno::UNO_QUERY_THROW);
    uno::Reference< beans::XPropertySet > xCell;
    sal_Int32 nColor;

    xCell.set(xTable->getCellByPosition(0, 0), uno::UNO_QUERY_THROW);
    xCell->getPropertyValue("FillColor") >>= nColor;
    CPPUNIT_ASSERT_EQUAL(sal_Int32(6003669), nColor);

    xCell.set(xTable->getCellByPosition(0, 1), uno::UNO_QUERY_THROW);
    xCell->getPropertyValue("FillColor") >>= nColor;
    CPPUNIT_ASSERT_EQUAL(sal_Int32(6003669), nColor);

    xCell.set(xTable->getCellByPosition(1, 1), uno::UNO_QUERY_THROW);
    xCell->getPropertyValue("FillColor") >>= nColor;
    CPPUNIT_ASSERT_EQUAL(sal_Int32(13754095), nColor);

    xCell.set(xTable->getCellByPosition(1, 2), uno::UNO_QUERY_THROW);
    xCell->getPropertyValue("FillColor") >>= nColor;
    CPPUNIT_ASSERT_EQUAL(sal_Int32(15331319), nColor);

    xCell.set(xTable->getCellByPosition(1, 4), uno::UNO_QUERY_THROW);
    xCell->getPropertyValue("FillColor") >>= nColor;
    CPPUNIT_ASSERT_EQUAL(sal_Int32(6003669), nColor);

    xCell.set(xTable->getCellByPosition(3, 2), uno::UNO_QUERY_THROW);
    xCell->getPropertyValue("FillColor") >>= nColor;
    CPPUNIT_ASSERT_EQUAL(sal_Int32(6003669), nColor);

    xCell.set(xTable->getCellByPosition(3, 4), uno::UNO_QUERY_THROW);
    xCell->getPropertyValue("FillColor") >>= nColor;
    CPPUNIT_ASSERT_EQUAL(sal_Int32(6003669), nColor);

    xDocShRef->DoClose();
}

CPPUNIT_TEST_SUITE_REGISTRATION(SdFiltersTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
