/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "charttest.hxx"

#include <test/xmltesttools.hxx>

#include <com/sun/star/chart/ErrorBarStyle.hpp>
#include <com/sun/star/chart2/XRegressionCurveContainer.hpp>
#include <com/sun/star/lang/XServiceName.hpp>
#include <com/sun/star/packages/zip/ZipFileAccess.hpp>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/text/XTextFramesSupplier.hpp>
#include <com/sun/star/drawing/LineStyle.hpp>

#include <unotools/ucbstreamhelper.hxx>
#include <rtl/strbuf.hxx>

#include <libxml/xpathInternals.h>
#include <libxml/parserInternals.h>

#include <algorithm>

using uno::Reference;
using beans::XPropertySet;

class Chart2ExportTest : public ChartTest, public XmlTestTools
{
protected:

    virtual void registerNamespaces(xmlXPathContextPtr& pXmlXPathCtx) SAL_OVERRIDE;
public:
    Chart2ExportTest() : ChartTest() {}
    void test();
    void testErrorBarXLSX();
    void testTrendline();
    void testStockChart();
    void testBarChart();
    void testCrosses();
    void testScatterChartTextXValues();
    void testChartDataTable();
    void testChartExternalData();
    void testEmbeddingsGrabBag();
    void testAreaChartLoad();
    void testUpDownBars();
    void testDoughnutChart();
    void testDisplayUnits();
    // void testFdo74115WallGradientFill();
    void testFdo74115WallBitmapFill();
    void testBarChartRotation();
    void testShapeFollowedByChart();
    void testPieChartDataLabels();
    void testSeriesIdxOrder();
    void testScatterPlotLabels();
    void testErrorBarDataRangeODS();
    void testChartCrash();
    void testPieChartRotation();
    void testEmbeddingsOleObjectGrabBag();
    void testGapWidthXLSX();
    void testSmoothedLines();
    void testLabelStringODS();
    void testFdo78290LineChartMarkerX();
    void testFdo78290ScatterChartMarkerX();
    void testFdo78290CombinationChartMarkerX();
    void testAxisNumberFormatODS();
    void testDataLabelBordersDOCX();
    void testDataLabel3DChartDOCX();
    void testDataLabelBarChartDOCX();
    void testDataLabelRadarChartDOCX();
    void testDataLabelDoughnutChartDOCX();
    void testDataLabelAreaChartDOCX();
    void testDataLabelDefaultLineChartDOCX();

    CPPUNIT_TEST_SUITE(Chart2ExportTest);
    CPPUNIT_TEST(test);
    CPPUNIT_TEST(testErrorBarXLSX);
    CPPUNIT_TEST(testTrendline);
    CPPUNIT_TEST(testStockChart);
    CPPUNIT_TEST(testBarChart);
    CPPUNIT_TEST(testCrosses);
    CPPUNIT_TEST(testScatterChartTextXValues);
    CPPUNIT_TEST(testChartDataTable);
    CPPUNIT_TEST(testChartExternalData);
    CPPUNIT_TEST(testEmbeddingsGrabBag);
    CPPUNIT_TEST(testAreaChartLoad);
    CPPUNIT_TEST(testUpDownBars);
    CPPUNIT_TEST(testDoughnutChart);
    CPPUNIT_TEST(testDisplayUnits);
    // CPPUNIT_TEST(testFdo74115WallGradientFill);
    CPPUNIT_TEST(testFdo74115WallBitmapFill);
    CPPUNIT_TEST(testBarChartRotation);
    CPPUNIT_TEST(testShapeFollowedByChart);
    CPPUNIT_TEST(testPieChartDataLabels);
    CPPUNIT_TEST(testSeriesIdxOrder);
    CPPUNIT_TEST(testScatterPlotLabels);
    CPPUNIT_TEST(testErrorBarDataRangeODS);
    CPPUNIT_TEST(testChartCrash);
    CPPUNIT_TEST(testPieChartRotation);
    CPPUNIT_TEST(testEmbeddingsOleObjectGrabBag);
    CPPUNIT_TEST(testGapWidthXLSX);
    CPPUNIT_TEST(testSmoothedLines);
    CPPUNIT_TEST(testLabelStringODS);
    CPPUNIT_TEST(testFdo78290LineChartMarkerX);
    CPPUNIT_TEST(testFdo78290ScatterChartMarkerX);
    CPPUNIT_TEST(testFdo78290CombinationChartMarkerX);
    CPPUNIT_TEST(testAxisNumberFormatODS);
    CPPUNIT_TEST(testDataLabelBordersDOCX);
    CPPUNIT_TEST(testDataLabel3DChartDOCX);
    CPPUNIT_TEST(testDataLabelBarChartDOCX);
    CPPUNIT_TEST(testDataLabelRadarChartDOCX);
    CPPUNIT_TEST(testDataLabelDoughnutChartDOCX);
    CPPUNIT_TEST(testDataLabelAreaChartDOCX);
    CPPUNIT_TEST(testDataLabelDefaultLineChartDOCX);
    CPPUNIT_TEST_SUITE_END();

protected:
    /**
     * Given that some problem doesn't affect the result in the importer, we
     * test the resulting file directly, by opening the zip file, parsing an
     * xml stream, and asserting an XPath expression. This method returns the
     * xml stream, so that you can do the asserting.
     */
    xmlDocPtr parseExport(const OUString& rDir, const OUString& rFilterFormat);

};

void Chart2ExportTest::test()
{
    load("/chart2/qa/extras/data/ods/", "simple_export_chart.ods");
    reload("Calc Office Open XML");
}

struct CheckForChartName
{
private:
    OUString aDir;

public:
    CheckForChartName( const OUString& rDir ):
        aDir(rDir) {}

    bool operator()(const OUString& rName)
    {
        if(!rName.startsWith(aDir))
            return false;

        if(!rName.endsWith(".xml"))
            return false;

        return true;
    }
};

OUString findChartFile(const OUString& rDir, uno::Reference< container::XNameAccess > xNames )
{
    uno::Sequence<OUString> rNames = xNames->getElementNames();
    OUString* pElement = std::find_if(rNames.begin(), rNames.end(), CheckForChartName(rDir));

    CPPUNIT_ASSERT(pElement);
    CPPUNIT_ASSERT(pElement != rNames.end());
    return *pElement;
}

xmlDocPtr Chart2ExportTest::parseExport(const OUString& rDir, const OUString& rFilterFormat)
{
    boost::shared_ptr<utl::TempFile> pTempFile = reload(rFilterFormat);

    // Read the XML stream we're interested in.
    uno::Reference<packages::zip::XZipFileAccess2> xNameAccess = packages::zip::ZipFileAccess::createWithURL(comphelper::getComponentContext(m_xSFactory), pTempFile->GetURL());
    uno::Reference<io::XInputStream> xInputStream(xNameAccess->getByName(findChartFile(rDir, xNameAccess)), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xInputStream.is());
    boost::shared_ptr<SvStream> pStream(utl::UcbStreamHelper::CreateStream(xInputStream, true));

    return parseXmlStream(pStream.get());
}

void Chart2ExportTest::registerNamespaces(xmlXPathContextPtr& pXmlXPathCtx)
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

namespace {

void testErrorBar( Reference< XPropertySet > xErrorBar )
{
    sal_Int32 nErrorBarStyle;
    CPPUNIT_ASSERT(
        xErrorBar->getPropertyValue("ErrorBarStyle") >>= nErrorBarStyle);
    CPPUNIT_ASSERT_EQUAL(nErrorBarStyle, chart::ErrorBarStyle::RELATIVE);
    bool bShowPositive = bool(), bShowNegative = bool();
    CPPUNIT_ASSERT(
        xErrorBar->getPropertyValue("ShowPositiveError") >>= bShowPositive);
    CPPUNIT_ASSERT(bShowPositive);
    CPPUNIT_ASSERT(
        xErrorBar->getPropertyValue("ShowNegativeError") >>= bShowNegative);
    CPPUNIT_ASSERT(bShowNegative);
    double nVal = 0.0;
    CPPUNIT_ASSERT(xErrorBar->getPropertyValue("PositiveError") >>= nVal);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(nVal, 10.0, 1e-10);
}

void checkCommonTrendline(
        Reference<chart2::XRegressionCurve> xCurve,
        double aExpectedExtrapolateForward, double aExpectedExtrapolateBackward,
        bool aExpectedForceIntercept, double aExpectedInterceptValue,
        bool aExpectedShowEquation, bool aExpectedR2)
{
    Reference<XPropertySet> xProperties( xCurve , uno::UNO_QUERY );
    CPPUNIT_ASSERT(xProperties.is());

    double aExtrapolateForward = 0.0;
    CPPUNIT_ASSERT(xProperties->getPropertyValue("ExtrapolateForward") >>= aExtrapolateForward);
    CPPUNIT_ASSERT_EQUAL(aExpectedExtrapolateForward, aExtrapolateForward);

    double aExtrapolateBackward = 0.0;
    CPPUNIT_ASSERT(xProperties->getPropertyValue("ExtrapolateBackward") >>= aExtrapolateBackward);
    CPPUNIT_ASSERT_EQUAL(aExpectedExtrapolateBackward, aExtrapolateBackward);

    bool aForceIntercept = false;
    CPPUNIT_ASSERT(xProperties->getPropertyValue("ForceIntercept") >>= aForceIntercept);
    CPPUNIT_ASSERT_EQUAL(aExpectedForceIntercept, aForceIntercept);

    if (aForceIntercept)
    {
        double aInterceptValue = 0.0;
        CPPUNIT_ASSERT(xProperties->getPropertyValue("InterceptValue") >>= aInterceptValue);
        CPPUNIT_ASSERT_EQUAL(aExpectedInterceptValue, aInterceptValue);
    }

    Reference< XPropertySet > xEquationProperties( xCurve->getEquationProperties() );
    CPPUNIT_ASSERT(xEquationProperties.is());

    bool aShowEquation = false;
    CPPUNIT_ASSERT(xEquationProperties->getPropertyValue("ShowEquation") >>= aShowEquation);
    CPPUNIT_ASSERT_EQUAL(aExpectedShowEquation, aShowEquation);

    bool aShowCorrelationCoefficient = false;
    CPPUNIT_ASSERT(xEquationProperties->getPropertyValue("ShowCorrelationCoefficient") >>= aShowCorrelationCoefficient);
    CPPUNIT_ASSERT_EQUAL(aExpectedR2, aShowCorrelationCoefficient);
}

void checkNameAndType(Reference<XPropertySet> xProperties, const OUString& aExpectedName, const OUString& aExpectedServiceName)
{
    Reference< lang::XServiceName > xServiceName( xProperties, UNO_QUERY );
    CPPUNIT_ASSERT(xServiceName.is());

    OUString aServiceName = xServiceName->getServiceName();
    CPPUNIT_ASSERT_EQUAL(aExpectedServiceName, aServiceName);

    OUString aCurveName;
    CPPUNIT_ASSERT(xProperties->getPropertyValue("CurveName") >>= aCurveName);
    CPPUNIT_ASSERT_EQUAL(aExpectedName, aCurveName);
}

void checkLinearTrendline(
        Reference<chart2::XRegressionCurve> xCurve, const OUString& aExpectedName,
        double aExpectedExtrapolateForward, double aExpectedExtrapolateBackward,
        bool aExpectedForceIntercept, double aExpectedInterceptValue,
        bool aExpectedShowEquation, bool aExpectedR2)
{
    Reference<XPropertySet> xProperties( xCurve , uno::UNO_QUERY );
    CPPUNIT_ASSERT(xProperties.is());

    checkNameAndType(xProperties, aExpectedName, "com.sun.star.chart2.LinearRegressionCurve");

    checkCommonTrendline(
        xCurve,
        aExpectedExtrapolateForward, aExpectedExtrapolateBackward,
        aExpectedForceIntercept, aExpectedInterceptValue,
        aExpectedShowEquation, aExpectedR2);
}

void checkPolynomialTrendline(
        Reference<chart2::XRegressionCurve> xCurve, const OUString& aExpectedName,
        sal_Int32 aExpectedDegree,
        double aExpectedExtrapolateForward, double aExpectedExtrapolateBackward,
        bool aExpectedForceIntercept, double aExpectedInterceptValue,
        bool aExpectedShowEquation, bool aExpectedR2)
{
    Reference<XPropertySet> xProperties( xCurve , uno::UNO_QUERY );
    CPPUNIT_ASSERT(xProperties.is());

    checkNameAndType(xProperties, aExpectedName, "com.sun.star.chart2.PolynomialRegressionCurve");

    sal_Int32 aDegree = 2;
    CPPUNIT_ASSERT(xProperties->getPropertyValue("PolynomialDegree") >>= aDegree);
    CPPUNIT_ASSERT_EQUAL(aExpectedDegree, aDegree);

    checkCommonTrendline(
        xCurve,
        aExpectedExtrapolateForward, aExpectedExtrapolateBackward,
        aExpectedForceIntercept, aExpectedInterceptValue,
        aExpectedShowEquation, aExpectedR2);
}

void checkMovingAverageTrendline(
        Reference<chart2::XRegressionCurve> xCurve, const OUString& aExpectedName, sal_Int32 aExpectedPeriod)
{
    Reference<XPropertySet> xProperties( xCurve , uno::UNO_QUERY );
    CPPUNIT_ASSERT(xProperties.is());

    checkNameAndType(xProperties, aExpectedName, "com.sun.star.chart2.MovingAverageRegressionCurve");

    sal_Int32 aPeriod = 2;
    CPPUNIT_ASSERT(xProperties->getPropertyValue("MovingAveragePeriod") >>= aPeriod);
    CPPUNIT_ASSERT_EQUAL(aExpectedPeriod, aPeriod);
}

void checkTrendlinesInChart(uno::Reference< chart2::XChartDocument > xChartDoc)
{
    CPPUNIT_ASSERT(xChartDoc.is());

    Reference< chart2::XDataSeries > xDataSeries = getDataSeriesFromDoc( xChartDoc, 0 );
    CPPUNIT_ASSERT( xDataSeries.is() );

    Reference< chart2::XRegressionCurveContainer > xRegressionCurveContainer( xDataSeries, UNO_QUERY );
    CPPUNIT_ASSERT( xRegressionCurveContainer.is() );

    Sequence< Reference< chart2::XRegressionCurve > > xRegressionCurveSequence = xRegressionCurveContainer->getRegressionCurves();
    CPPUNIT_ASSERT_EQUAL((sal_Int32) 3, xRegressionCurveSequence.getLength());

    Reference<chart2::XRegressionCurve> xCurve;

    xCurve = xRegressionCurveSequence[0];
    CPPUNIT_ASSERT(xCurve.is());
    checkPolynomialTrendline(xCurve, "col2_poly", 3, 0.1, -0.1, true, -1.0, true, true);

    xCurve = xRegressionCurveSequence[1];
    CPPUNIT_ASSERT(xCurve.is());
    checkLinearTrendline(xCurve, "col2_linear", -0.5, -0.5, false, 0.0, true, false);

    xCurve = xRegressionCurveSequence[2];
    CPPUNIT_ASSERT(xCurve.is());
    checkMovingAverageTrendline(xCurve, "col2_moving_avg", 3);
}

}

// improve the test
void Chart2ExportTest::testErrorBarXLSX()
{
    load("/chart2/qa/extras/data/ods/", "error_bar.ods");
    {
        // make sure the ODS import was successful
        uno::Reference< chart2::XChartDocument > xChartDoc = getChartDocFromSheet( 0, mxComponent );
        CPPUNIT_ASSERT(xChartDoc.is());

        Reference< chart2::XDataSeries > xDataSeries = getDataSeriesFromDoc( xChartDoc, 0 );
        CPPUNIT_ASSERT( xDataSeries.is() );

        Reference< beans::XPropertySet > xPropSet( xDataSeries, UNO_QUERY_THROW );
        CPPUNIT_ASSERT( xPropSet.is() );

        // test that y error bars are there
        Reference< beans::XPropertySet > xErrorBarYProps;
        xPropSet->getPropertyValue(CHART_UNONAME_ERRORBAR_Y) >>= xErrorBarYProps;
        testErrorBar(xErrorBarYProps);
    }

    reload("Calc Office Open XML");
    {
        uno::Reference< chart2::XChartDocument > xChartDoc = getChartDocFromSheet( 0, mxComponent );
        CPPUNIT_ASSERT(xChartDoc.is());

        Reference< chart2::XDataSeries > xDataSeries = getDataSeriesFromDoc( xChartDoc, 0 );
        CPPUNIT_ASSERT( xDataSeries.is() );

        Reference< beans::XPropertySet > xPropSet( xDataSeries, UNO_QUERY_THROW );
        CPPUNIT_ASSERT( xPropSet.is() );

        // test that y error bars are there
        Reference< beans::XPropertySet > xErrorBarYProps;
        xPropSet->getPropertyValue(CHART_UNONAME_ERRORBAR_Y) >>= xErrorBarYProps;
        testErrorBar(xErrorBarYProps);
    }
}

// This method tests the preservation of properties for trendlines / regression curves
// in an export -> import cycle using different file formats - ODS, XLS and XLSX.
void Chart2ExportTest::testTrendline()
{
    mbSkipValidation = true;
    load("/chart2/qa/extras/data/ods/", "trendline.ods");
    checkTrendlinesInChart(getChartDocFromSheet( 0, mxComponent));
    reload("calc8");
    checkTrendlinesInChart(getChartDocFromSheet( 0, mxComponent));

    load("/chart2/qa/extras/data/ods/", "trendline.ods");
    checkTrendlinesInChart(getChartDocFromSheet( 0, mxComponent));
    reload("Calc Office Open XML");
    checkTrendlinesInChart(getChartDocFromSheet( 0, mxComponent));

    load("/chart2/qa/extras/data/ods/", "trendline.ods");
    checkTrendlinesInChart(getChartDocFromSheet( 0, mxComponent));
    reload("MS Excel 97");
    checkTrendlinesInChart(getChartDocFromSheet( 0, mxComponent));

}

void Chart2ExportTest::testStockChart()
{
    /*  For attached file Stock_Chart.docx, in chart1.xml,
     * <c:stockChart>, there are four types of series as
     * Open,Low,High and Close.
     * For Open series, in <c:idx val="0" />
     * an attribute val of index should start from 1 and not from 0.
     * Which was problem area.
     */
    load("/chart2/qa/extras/data/docx/", "testStockChart.docx");

    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    if (!pXmlDoc)
       return;

    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:stockChart/c:ser[1]/c:idx", "val", "1");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:stockChart/c:ser[1]/c:order", "val", "1");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:stockChart/c:ser[1]/c:tx/c:strRef/c:strCache/c:pt/c:v", "Open");
}

void Chart2ExportTest::testBarChart()
{
    load("/chart2/qa/extras/data/docx/", "testBarChart.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    if (!pXmlDoc)
       return;

    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:barChart/c:barDir", "val", "col");
}

void Chart2ExportTest::testCrosses()
{
    load("/chart2/qa/extras/data/docx/", "Bar_horizontal_cone.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");

    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:catAx/c:crosses", "val", "autoZero");
}

void Chart2ExportTest::testScatterChartTextXValues()
{
    load("/chart2/qa/extras/data/docx/", "scatter-chart-text-x-values.docx");

    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    Reference<chart2::XChartType> xCT = getChartTypeFromDoc(xChartDoc, 0, 0);
    CPPUNIT_ASSERT(xCT.is());

    // Make sure we have exactly 3 data series.
    std::vector<uno::Sequence<uno::Any> > aLabels = getDataSeriesLabelsFromChartType(xCT);
    CPPUNIT_ASSERT_EQUAL(size_t(3), aLabels.size());
    CPPUNIT_ASSERT_EQUAL(OUString("Series 1"), aLabels[0][0].get<OUString>());
    CPPUNIT_ASSERT_EQUAL(OUString("Series 2"), aLabels[1][0].get<OUString>());
    CPPUNIT_ASSERT_EQUAL(OUString("Series 3"), aLabels[2][0].get<OUString>());

    std::vector<std::vector<double> > aYValues = getDataSeriesYValuesFromChartType(xCT);
    CPPUNIT_ASSERT_EQUAL(size_t(3), aYValues.size());

    // Check the Y values of "Series 1".
    CPPUNIT_ASSERT_EQUAL(size_t(4), aYValues[0].size());
    CPPUNIT_ASSERT_EQUAL(4.3, aYValues[0][0]);
    CPPUNIT_ASSERT_EQUAL(2.5, aYValues[0][1]);
    CPPUNIT_ASSERT_EQUAL(3.5, aYValues[0][2]);
    CPPUNIT_ASSERT_EQUAL(4.5, aYValues[0][3]);

    // And "Series 2".
    CPPUNIT_ASSERT_EQUAL(size_t(4), aYValues[1].size());
    CPPUNIT_ASSERT_EQUAL(2.4, aYValues[1][0]);
    CPPUNIT_ASSERT_EQUAL(4.4, aYValues[1][1]);
    CPPUNIT_ASSERT_EQUAL(1.8, aYValues[1][2]);
    CPPUNIT_ASSERT_EQUAL(2.8, aYValues[1][3]);

    // And "Series 3".
    CPPUNIT_ASSERT_EQUAL(size_t(4), aYValues[2].size());
    CPPUNIT_ASSERT_EQUAL(2.0, aYValues[2][0]);
    CPPUNIT_ASSERT_EQUAL(2.0, aYValues[2][1]);
    CPPUNIT_ASSERT_EQUAL(3.0, aYValues[2][2]);
    CPPUNIT_ASSERT_EQUAL(5.0, aYValues[2][3]);

    // Test the export.
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    if (!pXmlDoc)
       return;

    assertXPathContent(pXmlDoc, "//c:scatterChart/c:ser[1]/c:xVal[1]/c:numRef[1]/c:numCache[1]/c:pt[1]/c:v[1]", "1");
}

void Chart2ExportTest::testChartDataTable()
{
    load("/chart2/qa/extras/data/docx/", "testChartDataTable.docx");

    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:dTable/c:showHorzBorder", "val", "1");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:dTable/c:showVertBorder", "val", "1");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:dTable/c:showOutline", "val", "1");
}

void Chart2ExportTest::testChartExternalData()
{
    load("/chart2/qa/extras/data/docx/", "testMultipleChart.docx");

    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    xmlXPathObjectPtr pXmlPathObj = getXPathNode(pXmlDoc, "/c:chartSpace/c:externalData");
    xmlNodeSetPtr pXmlNodes = pXmlPathObj->nodesetval;
    CPPUNIT_ASSERT(pXmlNodes);
}

void Chart2ExportTest::testEmbeddingsGrabBag()
{
   // The problem was that .xlsx files were missing from docx file from embeddings folder
   // after saving file.
   // This test case tests whether embeddings files grabbagged properly in correct object.

   load("/chart2/qa/extras/data/docx/", "testMultiplechartembeddings.docx" );
   uno::Reference<text::XTextDocument> xTextDocument(mxComponent, uno::UNO_QUERY);
   uno::Reference<beans::XPropertySet> xTextDocumentPropertySet(xTextDocument, uno::UNO_QUERY);
   uno::Sequence<beans::PropertyValue> aGrabBag(0);
   xTextDocumentPropertySet->getPropertyValue(OUString("InteropGrabBag")) >>= aGrabBag;
   CPPUNIT_ASSERT(aGrabBag.hasElements()); // Grab Bag not empty
   bool bEmbeddings = false;
   const char* testEmbeddedFileNames[3] = {"word/embeddings/Microsoft_Excel_Worksheet3.xlsx",
                                        "word/embeddings/Microsoft_Excel_Worksheet2.xlsx",
                                        "word/embeddings/Microsoft_Excel_Worksheet1.xlsx"};
   for(int i = 0; i < aGrabBag.getLength(); ++i)
   {
       if (aGrabBag[i].Name == "OOXEmbeddings")
       {
           bEmbeddings = true;
           uno::Sequence<beans::PropertyValue> aEmbeddingsList(0);
           uno::Reference<io::XInputStream> aEmbeddingXlsxStream;
           OUString aEmbeddedfileName;
           CPPUNIT_ASSERT(aGrabBag[i].Value >>= aEmbeddingsList); // PropertyValue of proper type
           sal_Int32 length = aEmbeddingsList.getLength();
           CPPUNIT_ASSERT_EQUAL(sal_Int32(3), length);
           for(int j = 0; j < length; ++j)
           {
               aEmbeddingsList[j].Value >>= aEmbeddingXlsxStream;
               aEmbeddedfileName = aEmbeddingsList[j].Name;
               CPPUNIT_ASSERT(aEmbeddingXlsxStream.get()); // Reference not empty
               CPPUNIT_ASSERT_EQUAL(OUString::createFromAscii(testEmbeddedFileNames[j]),aEmbeddedfileName);
           }
       }
   }
   CPPUNIT_ASSERT(bEmbeddings); // Grab Bag has all the expected elements
}

void Chart2ExportTest::testAreaChartLoad()
{
    load ("/chart2/qa/extras/data/docx/", "testAreaChartLoad.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart","Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:areaChart/c:ser/c:dLbls/c:showVal", "val", "1");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:areaChart/c:ser/c:dLbls/c:dLbl", 0);
}

void Chart2ExportTest::testUpDownBars()
{
    load("/chart2/qa/extras/data/docx/", "UpDownBars.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:lineChart/c:upDownBars");
}

void Chart2ExportTest::testDoughnutChart()
{
    load("/chart2/qa/extras/data/docx/", "doughnutChart.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);

    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:doughnutChart", "1");
}

void Chart2ExportTest::testDisplayUnits()
{
    load("/chart2/qa/extras/data/docx/", "DisplayUnits.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);

    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:valAx/c:dispUnits/c:builtInUnit", "val", "billions");
}

// void Chart2ExportTest::testFdo74115WallGradientFill()
// {
//     load("/chart2/qa/extras/data/docx/", "fdo74115_WallGradientFill.docx");
//     xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
//     CPPUNIT_ASSERT(pXmlDoc);
//
//     assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:spPr/a:gradFill");
// }

void Chart2ExportTest::testFdo74115WallBitmapFill()
{
    load("/chart2/qa/extras/data/docx/", "fdo74115_WallBitmapFill.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:spPr/a:blipFill");
}

//The below test case tests the built in marker 'x' for Office 2010 in Line charts

void Chart2ExportTest::testFdo78290LineChartMarkerX()
{
    load("/chart2/qa/extras/data/docx/", "fdo78290_Line_Chart_Marker_x.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace[1]/c:chart[1]/c:plotArea[1]/c:lineChart[1]/c:ser[1]/c:marker[1]/c:symbol[1]","val","x");
    assertXPath(pXmlDoc, "/c:chartSpace[1]/c:chart[1]/c:plotArea[1]/c:lineChart[1]/c:ser[1]/c:marker[1]/c:size[1]","val","7");
}

// We can also use the built in marker 'x' in scatter chart, hence writing the test case for the same.

void Chart2ExportTest::testFdo78290ScatterChartMarkerX()
{
    load("/chart2/qa/extras/data/docx/", "fdo78290_Scatter_Chart_Marker_x.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace[1]/c:chart[1]/c:plotArea[1]/c:scatterChart[1]/c:ser[1]/c:marker[1]/c:symbol[1]","val","x");
    assertXPath(pXmlDoc, "/c:chartSpace[1]/c:chart[1]/c:plotArea[1]/c:scatterChart[1]/c:ser[1]/c:marker[1]/c:size[1]","val","7");
}

// Also in a combination of charts like a column chart and line chart, we can use the built in marker 'x'
// for the line chart too. hence put a test case for the combination chart also.

void Chart2ExportTest::testFdo78290CombinationChartMarkerX()
{
    load("/chart2/qa/extras/data/docx/", "fdo78290_Combination_Chart_Marker_x.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace[1]/c:chart[1]/c:plotArea[1]/c:lineChart[1]/c:ser[1]/c:marker[1]/c:symbol[1]","val","x");
    assertXPath(pXmlDoc, "/c:chartSpace[1]/c:chart[1]/c:plotArea[1]/c:lineChart[1]/c:ser[1]/c:marker[1]/c:size[1]","val","7");
}

void Chart2ExportTest::testAxisNumberFormatODS()
{
    struct
    {
        void check( const Reference<chart2::XChartDocument>& xChartDoc )
        {
            Reference<chart2::XAxis> xAxisX = getAxisFromDoc(xChartDoc, 0, 0, 0);
            Reference<chart2::XTitled> xTitle(xAxisX, UNO_QUERY_THROW);
            OUString aTitleText = getTitleString(xTitle);
            CPPUNIT_ASSERT_EQUAL(OUString("Linked To Source"), aTitleText);

            sal_Int32 nNumFmt = getNumberFormatFromAxis(xAxisX);
            sal_Int16 nType = getNumberFormatType(xChartDoc, nNumFmt);
            CPPUNIT_ASSERT_MESSAGE("X axis should be percentage format.", (nType & util::NumberFormat::PERCENT));

            bool bNumFmtLinked = false;
            Reference<beans::XPropertySet> xPS(xAxisX, uno::UNO_QUERY_THROW);
            xPS->getPropertyValue("LinkNumberFormatToSource") >>= bNumFmtLinked;
            CPPUNIT_ASSERT_MESSAGE("X axis should have its number format linked to source.", bNumFmtLinked);

            Reference<chart2::XAxis> xAxisY = getAxisFromDoc(xChartDoc, 0, 1, 0);
            xTitle.set(xAxisY, UNO_QUERY_THROW);
            aTitleText = getTitleString(xTitle);
            CPPUNIT_ASSERT_EQUAL(OUString("Not Linked"), aTitleText);

            nNumFmt = getNumberFormatFromAxis(xAxisY);
            nType = getNumberFormatType(xChartDoc, nNumFmt);
            CPPUNIT_ASSERT_MESSAGE("Y axis should be a normal number format.", (nType & util::NumberFormat::NUMBER));

            bNumFmtLinked = true;
            xPS.set(xAxisY, uno::UNO_QUERY_THROW);
            xPS->getPropertyValue("LinkNumberFormatToSource") >>= bNumFmtLinked;
            CPPUNIT_ASSERT_MESSAGE("Y axis should not have its number format linked to source.", !bNumFmtLinked);
        }

    } aTest;

    load("/chart2/qa/extras/data/ods/", "axis-numformats-linked.ods");

    Reference<chart2::XChartDocument> xChartDoc = getChartDocFromSheet(0, mxComponent);
    aTest.check(xChartDoc);

    // Reload the document and make sure everything remains intact.
    reload("calc8");
    xChartDoc = getChartDocFromSheet(0, mxComponent);
    aTest.check(xChartDoc);
}

void Chart2ExportTest::testDataLabelBordersDOCX()
{
    struct Check
    {
        sal_Int32 mnIndex;
        css::drawing::LineStyle meStyle;
        sal_Int32 mnColor;
    };

    struct
    {
        /**
         * Chart 1 has 4 bars of which 1st and 3rd have labels with borders
         * around them.
         */
        void checkObject1( const Reference<chart2::XChartDocument>& xChartDoc )
        {
            CPPUNIT_ASSERT(xChartDoc.is());

            Reference<chart2::XDataSeries> xDataSeries = getDataSeriesFromDoc(xChartDoc, 0);
            CPPUNIT_ASSERT(xDataSeries.is());

            // Check to make sure that data points 0 and 2 have local properties.
            Reference<beans::XPropertySet> xPropSet(xDataSeries, uno::UNO_QUERY);
            CPPUNIT_ASSERT(xPropSet.is());

            Sequence<sal_Int32> aIndices;
            xPropSet->getPropertyValue("AttributedDataPoints") >>= aIndices;
            CPPUNIT_ASSERT_MESSAGE("There should be 2 data points with local properties.", aIndices.getLength() == 2);
            CPPUNIT_ASSERT(aIndices[0] == 0);
            CPPUNIT_ASSERT(aIndices[1] == 2);

            const Check aDataPoints[] =
            {
                { 0, css::drawing::LineStyle_SOLID, 0x00FFFF00 }, // solid yellow
                { 2, css::drawing::LineStyle_SOLID, 0x00FF0000 }  // solid red
            };

            for (size_t i = 0, n = SAL_N_ELEMENTS(aDataPoints); i < n; ++i)
            {
                xPropSet = xDataSeries->getDataPointByIndex(aDataPoints[i].mnIndex);
                CPPUNIT_ASSERT(xPropSet.is());

                css::drawing::LineStyle eLineStyle = css::drawing::LineStyle_NONE;
                xPropSet->getPropertyValue(CHART_UNONAME_LABEL_BORDER_STYLE) >>= eLineStyle;
                CPPUNIT_ASSERT(eLineStyle == aDataPoints[i].meStyle);

                sal_Int32 nWidth = -1;
                xPropSet->getPropertyValue(CHART_UNONAME_LABEL_BORDER_WIDTH) >>= nWidth;
                CPPUNIT_ASSERT(nWidth > 0);

                sal_Int32 nColor = -1;
                xPropSet->getPropertyValue(CHART_UNONAME_LABEL_BORDER_COLOR) >>= nColor;
                CPPUNIT_ASSERT_MESSAGE("Border color is wrong.", nColor == aDataPoints[i].mnColor);
            }
        }

        /**
         * Chart 2 has all its data labels with identical borders.
         */
        void checkObject2( const Reference<chart2::XChartDocument>& xChartDoc )
        {
            CPPUNIT_ASSERT(xChartDoc.is());

            Reference<chart2::XDataSeries> xDataSeries = getDataSeriesFromDoc(xChartDoc, 0);
            CPPUNIT_ASSERT(xDataSeries.is());

            Reference<beans::XPropertySet> xPropSet(xDataSeries, uno::UNO_QUERY);
            CPPUNIT_ASSERT(xPropSet.is());

            css::drawing::LineStyle eLineStyle = css::drawing::LineStyle_NONE;
            xPropSet->getPropertyValue(CHART_UNONAME_LABEL_BORDER_STYLE) >>= eLineStyle;
            CPPUNIT_ASSERT(eLineStyle == css::drawing::LineStyle_SOLID);

            sal_Int32 nWidth = -1;
            xPropSet->getPropertyValue(CHART_UNONAME_LABEL_BORDER_WIDTH) >>= nWidth;
            CPPUNIT_ASSERT(nWidth > 0);

            sal_Int32 nColor = -1;
            xPropSet->getPropertyValue(CHART_UNONAME_LABEL_BORDER_COLOR) >>= nColor;
            CPPUNIT_ASSERT_MESSAGE("Border color should be green.", nColor == 0x0000FF00);
        }

    } aTest;

    load("/chart2/qa/extras/data/docx/", "data-label-borders.docx");

    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);

    // "Automatic" chart background fill in docx should be loaded as solid white.
    Reference<beans::XPropertySet> xPropSet = xChartDoc->getPageBackground();
    CPPUNIT_ASSERT(xPropSet.is());
    drawing::FillStyle eStyle = xPropSet->getPropertyValue("FillStyle").get<drawing::FillStyle>();
    sal_Int32 nColor = xPropSet->getPropertyValue("FillColor").get<sal_Int32>();
    CPPUNIT_ASSERT_MESSAGE("'Automatic' chart background fill in docx should be loaded as solid fill.",
        eStyle == drawing::FillStyle_SOLID);
    CPPUNIT_ASSERT_MESSAGE("'Automatic' chart background fill in docx should be loaded as solid white.",
        (nColor & 0x00FFFFFF) == 0x00FFFFFF); // highest 2 bytes are transparency which we ignore here.

    aTest.checkObject1(xChartDoc);
    xChartDoc.set(getChartDocFromWriter(1), uno::UNO_QUERY);
    aTest.checkObject2(xChartDoc);

    reload("Office Open XML Text");

    xChartDoc.set(getChartDocFromWriter(0), uno::UNO_QUERY);
    aTest.checkObject1(xChartDoc);
    xChartDoc.set(getChartDocFromWriter(1), uno::UNO_QUERY);
    aTest.checkObject2(xChartDoc);
}

void Chart2ExportTest::testDataLabel3DChartDOCX()
{
    load("/chart2/qa/extras/data/docx/", "3d-bar-label.docx");

    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    xmlDocPtr pXmlDoc = parseExport("word/charts/chart","Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);

    // We must not export label position attributes for 3D bar charts. The
    // same rule also applies to several other 3D charts, apparently.
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:bar3DChart/c:ser/c:dLbls/c:dLblPos", 0);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:bar3DChart/c:ser/c:dLbls/c:dLbl/c:dLblPos", 0);
}

void Chart2ExportTest::testDataLabelBarChartDOCX()
{
    load("/chart2/qa/extras/data/docx/", "bar-chart-labels.docx");

    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    xmlDocPtr pXmlDoc = parseExport("word/charts/chart","Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);

    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:barChart/c:ser[1]/c:dLbls/c:dLblPos", "val", "ctr");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:barChart/c:ser[2]/c:dLbls/c:dLblPos", "val", "inEnd");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:barChart/c:ser[3]/c:dLbls/c:dLblPos", "val", "inBase");
}

void Chart2ExportTest::testDataLabelRadarChartDOCX()
{
    load("/chart2/qa/extras/data/docx/", "radar-chart-labels.docx");

    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    xmlDocPtr pXmlDoc = parseExport("word/charts/chart","Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);

    // We must not export label position attributes for radar charts.
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:radarChart/c:ser/c:dLbls/c:dLblPos", 0);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:radarChart/c:ser/c:dLbls/c:dLbl/c:dLblPos", 0);
}

void Chart2ExportTest::testDataLabelDoughnutChartDOCX()
{
    load("/chart2/qa/extras/data/docx/", "doughnut-chart-labels.docx");

    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    xmlDocPtr pXmlDoc = parseExport("word/charts/chart","Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);

    // We must not export label position attributes for doughnut charts.
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:doughnutChart/c:ser/c:dLbls/c:dLblPos", 0);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:doughnutChart/c:ser/c:dLbls/c:dLbl/c:dLblPos", 0);
}

void Chart2ExportTest::testDataLabelAreaChartDOCX()
{
    load("/chart2/qa/extras/data/docx/", "area-chart-labels.docx");

    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    xmlDocPtr pXmlDoc = parseExport("word/charts/chart","Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);

    // We must not export label position attributes for area charts.
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:areaChart/c:ser/c:dLbls/c:dLblPos", 0);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:areaChart/c:ser/c:dLbls/c:dLbl/c:dLblPos", 0);
}

void Chart2ExportTest::testDataLabelDefaultLineChartDOCX()
{
    // This file was created by Word 2007, which doesn't provide default data
    // label position (2010 does).  Make sure its default data label position
    // is RIGHT when exporting.

    load("/chart2/qa/extras/data/docx/", "line-chart-label-default-placement.docx");

    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    reload("Office Open XML Text");

    xChartDoc.set(getChartDocFromWriter(0), uno::UNO_QUERY);
    Reference<chart2::XDataSeries> xDataSeries = getDataSeriesFromDoc(xChartDoc, 0);
    Reference<beans::XPropertySet> xPropSet(xDataSeries, uno::UNO_QUERY);
    CPPUNIT_ASSERT(xPropSet.is());
    sal_Int32 nLabelPlacement = -1;
    if (xPropSet->getPropertyValue("LabelPlacement") >>= nLabelPlacement)
        // This option may not be set.  Check its value only when it's set.
        CPPUNIT_ASSERT_MESSAGE("Line chart's default label placement should be 'right'.", nLabelPlacement == chart::DataLabelPlacement::RIGHT);
}

void Chart2ExportTest::testBarChartRotation()
{
    load ("/chart2/qa/extras/data/docx/", "barChartRotation.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart","Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);

    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:view3D/c:rotX", "val", "30");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:view3D/c:rotY", "val", "50");
}

void Chart2ExportTest::testShapeFollowedByChart()
{
    /* If there is a scenario where a chart is followed by a shape
       which is being exported as an alternate content then, the
       docPr Id is being repeated, ECMA 20.4.2.5 says that the
       docPr Id should be unique, ensuring the same here.
    */
    load("/chart2/qa/extras/data/docx/", "FDO74430.docx");
    xmlDocPtr pXmlDoc = parseExport("word/document", "Office Open XML Text" );
    CPPUNIT_ASSERT(pXmlDoc);

    OUString aValueOfFirstDocPR = getXPath(pXmlDoc, "/w:document/w:body/w:p[3]/w:r[1]/w:drawing[1]/wp:inline[1]/wp:docPr[1]", "id");
    OUString aValueOfSecondDocPR;

    aValueOfSecondDocPR = getXPath(pXmlDoc, "/w:document/w:body/w:p[3]/w:r[1]/mc:AlternateContent[1]/mc:Choice[1]/w:drawing[1]/wp:anchor[1]/wp:docPr[1]", "id");

    CPPUNIT_ASSERT( aValueOfFirstDocPR != aValueOfSecondDocPR );
}

void Chart2ExportTest::testPieChartDataLabels()
{
    load("/chart2/qa/extras/data/docx/", "PieChartDataLabels.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:pie3DChart/c:ser[1]/c:dLbls/c:dLbl[1]/c:dLblPos", "val", "bestFit");
}

void Chart2ExportTest::testSeriesIdxOrder()
{
    load("/chart2/qa/extras/data/docx/", "testSeriesIdxOrder.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace[1]/c:chart[1]/c:plotArea[1]/c:lineChart[1]/c:ser[1]/c:idx[1]", "val", "1");
    assertXPath(pXmlDoc, "/c:chartSpace[1]/c:chart[1]/c:plotArea[1]/c:lineChart[1]/c:ser[1]/c:order[1]", "val", "1");
}

void Chart2ExportTest::testScatterPlotLabels()
{
    load("/chart2/qa/extras/data/odt/", "scatter-plot-labels.odt");
    Reference<chart2::XChartDocument> xChartDoc(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    Reference<chart2::XChartType> xCT = getChartTypeFromDoc(xChartDoc, 0, 0);
    CPPUNIT_ASSERT(xCT.is());

    // Make sure the original chart has 'a', 'b', 'c' as its data labels.
    std::vector<uno::Sequence<uno::Any> > aLabels = getDataSeriesLabelsFromChartType(xCT);
    CPPUNIT_ASSERT_EQUAL(size_t(3), aLabels.size());
    CPPUNIT_ASSERT_EQUAL(OUString("a"), aLabels[0][0].get<OUString>());
    CPPUNIT_ASSERT_EQUAL(OUString("b"), aLabels[1][0].get<OUString>());
    CPPUNIT_ASSERT_EQUAL(OUString("c"), aLabels[2][0].get<OUString>());

    // Reload the doc and check again.  The labels should not change.
    reload("writer8");

    xChartDoc.set(getChartDocFromWriter(0), uno::UNO_QUERY);
    CPPUNIT_ASSERT(xChartDoc.is());

    xCT = getChartTypeFromDoc(xChartDoc, 0, 0);
    CPPUNIT_ASSERT(xCT.is());

    aLabels = getDataSeriesLabelsFromChartType(xCT);
    CPPUNIT_ASSERT_EQUAL(size_t(3), aLabels.size());
    CPPUNIT_ASSERT_EQUAL(OUString("a"), aLabels[0][0].get<OUString>());
    CPPUNIT_ASSERT_EQUAL(OUString("b"), aLabels[1][0].get<OUString>());
    CPPUNIT_ASSERT_EQUAL(OUString("c"), aLabels[2][0].get<OUString>());
}

void Chart2ExportTest::testErrorBarDataRangeODS()
{
    load("/chart2/qa/extras/data/ods/", "ErrorBarRange.ods");
    reload("calc8");

    uno::Reference< chart2::XChartDocument > xChartDoc = getChartDocFromSheet( 0, mxComponent );
    CPPUNIT_ASSERT(xChartDoc.is());

    Reference< chart2::XDataSeries > xDataSeries = getDataSeriesFromDoc( xChartDoc, 0 );
    CPPUNIT_ASSERT( xDataSeries.is() );

    Reference< beans::XPropertySet > xPropSet( xDataSeries, UNO_QUERY_THROW );
    CPPUNIT_ASSERT( xPropSet.is() );

    // test that y error bars are there
    Reference< beans::XPropertySet > xErrorBarYProps;
    xPropSet->getPropertyValue(CHART_UNONAME_ERRORBAR_Y) >>= xErrorBarYProps;
    uno::Any aAny = xErrorBarYProps->getPropertyValue("ErrorBarRangePositive");
    CPPUNIT_ASSERT(aAny.hasValue());
    OUString aPosRange;
    aAny >>= aPosRange;
    CPPUNIT_ASSERT_EQUAL(OUString("$Sheet1.$B$1:$B$3"), aPosRange);

    aAny = xErrorBarYProps->getPropertyValue("ErrorBarRangeNegative");
    CPPUNIT_ASSERT(aAny.hasValue());
    OUString aNegRange;
    aAny >>= aNegRange;
    CPPUNIT_ASSERT_EQUAL(OUString("$Sheet1.$C$1:$C$3"), aNegRange);
}

void Chart2ExportTest::testChartCrash()
{
    load("/chart2/qa/extras/data/docx/", "FDO75975.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart", "Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
}

void Chart2ExportTest::testPieChartRotation()
{
    load ("/chart2/qa/extras/data/docx/", "pieChartRotation.docx");
    xmlDocPtr pXmlDoc = parseExport("word/charts/chart","Office Open XML Text");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:view3D/c:rotX", "val", "40");
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:view3D/c:rotY", "val", "30");
}

void Chart2ExportTest::testEmbeddingsOleObjectGrabBag()
{
   // The problem was that .bin files were missing from docx file from embeddings folder
   // after saving file.
   // This test case tests whether embeddings files grabbagged properly in correct object.

   load("/chart2/qa/extras/data/docx/", "testchartoleobjectembeddings.docx" );
   uno::Reference<text::XTextDocument> xTextDocument(mxComponent, uno::UNO_QUERY);
   uno::Reference<beans::XPropertySet> xTextDocumentPropertySet(xTextDocument, uno::UNO_QUERY);
   uno::Sequence<beans::PropertyValue> aGrabBag(0);
   xTextDocumentPropertySet->getPropertyValue(OUString("InteropGrabBag")) >>= aGrabBag;
   CPPUNIT_ASSERT(aGrabBag.hasElements()); // Grab Bag not empty
   bool bEmbeddings = false;
   const char* testEmbeddedFileNames[1] = {"word/embeddings/oleObject1.bin"};
   for(int i = 0; i < aGrabBag.getLength(); ++i)
   {
       if (aGrabBag[i].Name == "OOXEmbeddings")
       {
           bEmbeddings = true;
           uno::Sequence<beans::PropertyValue> aEmbeddingsList(0);
           uno::Reference<io::XInputStream> aEmbeddingXlsxStream;
           OUString aEmbeddedfileName;
           CPPUNIT_ASSERT(aGrabBag[i].Value >>= aEmbeddingsList); // PropertyValue of proper type
           sal_Int32 length = aEmbeddingsList.getLength();
           CPPUNIT_ASSERT_EQUAL(sal_Int32(1), length);
           for(int j = 0; j < length; ++j)
           {
               aEmbeddingsList[j].Value >>= aEmbeddingXlsxStream;
               aEmbeddedfileName = aEmbeddingsList[j].Name;
               CPPUNIT_ASSERT(aEmbeddingXlsxStream.get()); // Reference not empty
               CPPUNIT_ASSERT_EQUAL(OUString::createFromAscii(testEmbeddedFileNames[j]),aEmbeddedfileName);
           }
       }
   }
   CPPUNIT_ASSERT(bEmbeddings); // Grab Bag has all the expected elements
}

namespace {

void checkGapWidth(Reference<beans::XPropertySet> xPropSet, sal_Int32 nValue)
{
    uno::Any aAny = xPropSet->getPropertyValue("GapwidthSequence");
    CPPUNIT_ASSERT(aAny.hasValue());
    uno::Sequence< sal_Int32 > aSequence;
    aAny >>= aSequence;
    CPPUNIT_ASSERT(aSequence.getLength());
    CPPUNIT_ASSERT_EQUAL(nValue, aSequence[0]);
}

void checkOverlap(Reference<beans::XPropertySet> xPropSet, sal_Int32 nValue)
{
    uno::Any aAny = xPropSet->getPropertyValue("OverlapSequence");
    CPPUNIT_ASSERT(aAny.hasValue());
    uno::Sequence< sal_Int32 > aSequence;
    aAny >>= aSequence;
    CPPUNIT_ASSERT(aSequence.getLength());
    CPPUNIT_ASSERT_EQUAL(nValue, aSequence[0]);
}

void checkSheetForGapWidthAndOverlap(uno::Reference< chart2::XChartDocument > xChartDoc,
        sal_Int32 nExpectedGapWidth, sal_Int32 nExpectedOverlap)
{
    CPPUNIT_ASSERT(xChartDoc.is());

    Reference< chart2::XChartType > xChartType = getChartTypeFromDoc( xChartDoc, 0, 0 );
    CPPUNIT_ASSERT(xChartType.is());

    Reference< beans::XPropertySet > xPropSet( xChartType, uno::UNO_QUERY_THROW );
    checkGapWidth(xPropSet, nExpectedGapWidth);
    checkOverlap(xPropSet, nExpectedOverlap);

}

}

void Chart2ExportTest::testGapWidthXLSX()
{
    load("/chart2/qa/extras/data/xlsx/", "gapWidth.xlsx");

    uno::Reference< chart2::XChartDocument > xChartDoc = getChartDocFromSheet( 0, mxComponent );
    checkSheetForGapWidthAndOverlap(xChartDoc, 120, -60);

    xChartDoc = getChartDocFromSheet( 1, mxComponent );
    checkSheetForGapWidthAndOverlap(xChartDoc, 50, 30);

    reload("Calc Office Open XML");

    xChartDoc = getChartDocFromSheet( 0, mxComponent );
    checkSheetForGapWidthAndOverlap(xChartDoc, 120, -60);

    xChartDoc = getChartDocFromSheet( 1, mxComponent );
    checkSheetForGapWidthAndOverlap(xChartDoc, 50, 30);
}

void Chart2ExportTest::testSmoothedLines()
{
    load("/chart2/qa/extras/data/ods/", "smoothedLines.ods");
    xmlDocPtr pXmlDoc = parseExport("xl/charts/chart", "Calc Office Open XML");
    CPPUNIT_ASSERT(pXmlDoc);
    assertXPath(pXmlDoc, "/c:chartSpace/c:chart/c:plotArea/c:lineChart/c:ser[1]/c:smooth", "val", "0");
}

void Chart2ExportTest::testLabelStringODS()
{
    load("/chart2/qa/extras/data/ods/", "labelString.ods");

    uno::Reference< chart2::XChartDocument > xChartDoc = getChartDocFromSheet( 0, mxComponent );
    Reference< chart2::data::XDataSequence > xLabelSeq =
        getLabelDataSequenceFromDoc(xChartDoc);
    CPPUNIT_ASSERT(xLabelSeq.is());

    OUString aLabelString = xLabelSeq->getSourceRangeRepresentation();
    CPPUNIT_ASSERT_EQUAL(OUString("\"LabelName\""), aLabelString);

    reload("calc8");

    xChartDoc = getChartDocFromSheet( 0, mxComponent );
    xLabelSeq = getLabelDataSequenceFromDoc(xChartDoc);
    CPPUNIT_ASSERT(xLabelSeq.is());

    aLabelString = xLabelSeq->getSourceRangeRepresentation();
    CPPUNIT_ASSERT_EQUAL(OUString("\"LabelName\""), aLabelString);
}

CPPUNIT_TEST_SUITE_REGISTRATION(Chart2ExportTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
