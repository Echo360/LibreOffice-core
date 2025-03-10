/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <com/sun/star/awt/Size.hpp>
#include <com/sun/star/awt/XControlModel.hpp>
#include <com/sun/star/drawing/XControlShape.hpp>
#include <com/sun/star/text/VertOrientation.hpp>

#include <comphelper/sequenceashashmap.hxx>
#include <editeng/unoprnms.hxx>
#include <vcl/outdev.hxx>
#include <vcl/svapp.hxx>
#include <unotools/datetime.hxx>

#include <DomainMapper_Impl.hxx>
#include <StyleSheetTable.hxx>
#include <SdtHelper.hxx>

namespace writerfilter
{
namespace dmapper
{

using namespace ::com::sun::star;

/// w:sdt's w:dropDownList doesn't have width, so guess the size based on the longest string.
awt::Size lcl_getOptimalWidth(StyleSheetTablePtr pStyleSheet, OUString& rDefault, std::vector<OUString>& rItems)
{
    OUString aLongest = rDefault;
    sal_Int32 nHeight = 0;
    for (size_t i = 0; i < rItems.size(); ++i)
        if (rItems[i].getLength() > aLongest.getLength())
            aLongest = rItems[i];

    MapMode aMap(MAP_100TH_MM);
    OutputDevice* pOut = Application::GetDefaultDevice();
    pOut->Push(PUSH_FONT | PUSH_MAPMODE);

    PropertyMapPtr pDefaultCharProps = pStyleSheet->GetDefaultCharProps();
    Font aFont(pOut->GetFont());
    boost::optional<PropertyMap::Property> aFontName = pDefaultCharProps->getProperty(PROP_CHAR_FONT_NAME);
    if (aFontName)
        aFont.SetName(aFontName->second.get<OUString>());
    boost::optional<PropertyMap::Property> aHeight = pDefaultCharProps->getProperty(PROP_CHAR_HEIGHT);
    if (aHeight)
    {
        nHeight = aHeight->second.get<double>() * 35; // points -> mm100
        aFont.SetSize(Size(0, nHeight));
    }
    pOut->SetFont(aFont);
    pOut->SetMapMode(aMap);
    sal_Int32 nWidth = pOut->GetTextWidth(aLongest);

    pOut->Pop();

    // Border: see PDFWriterImpl::drawFieldBorder(), border size is font height / 4,
    // so additional width / height needed is height / 2.
    sal_Int32 nBorder = nHeight / 2;

    // Width: space for the text + the square having the dropdown arrow.
    return awt::Size(nWidth + nBorder + nHeight, nHeight + nBorder);
}

SdtHelper::SdtHelper(DomainMapper_Impl& rDM_Impl)
    : m_rDM_Impl(rDM_Impl)
    , m_bHasElements(false)
    , m_bOutsideAParagraph(false)
{
}

SdtHelper::~SdtHelper()
{
}

void SdtHelper::createDropDownControl()
{
    OUString aDefaultText = m_aSdtTexts.makeStringAndClear();
    uno::Reference<awt::XControlModel> xControlModel(m_rDM_Impl.GetTextFactory()->createInstance("com.sun.star.form.component.ComboBox"), uno::UNO_QUERY);
    uno::Reference<beans::XPropertySet> xPropertySet(xControlModel, uno::UNO_QUERY);
    xPropertySet->setPropertyValue("DefaultText", uno::makeAny(aDefaultText));
    xPropertySet->setPropertyValue("Dropdown", uno::makeAny(sal_True));
    uno::Sequence<OUString> aItems(m_aDropDownItems.size());
    for (size_t i = 0; i < m_aDropDownItems.size(); ++i)
        aItems[i] = m_aDropDownItems[i];
    xPropertySet->setPropertyValue("StringItemList", uno::makeAny(aItems));

    createControlShape(lcl_getOptimalWidth(m_rDM_Impl.GetStyleSheetTable(), aDefaultText, m_aDropDownItems), xControlModel);
    m_aDropDownItems.clear();
}

void SdtHelper::createDateControl(OUString& rContentText, beans::PropertyValue aCharFormat)
{
    uno::Reference<awt::XControlModel> xControlModel(m_rDM_Impl.GetTextFactory()->createInstance("com.sun.star.form.component.DateField"), uno::UNO_QUERY);
    uno::Reference<beans::XPropertySet> xPropertySet(xControlModel, uno::UNO_QUERY);

    xPropertySet->setPropertyValue("Dropdown", uno::makeAny(sal_True));

    // See com/sun/star/awt/UnoControlDateFieldModel.idl, DateFormat; sadly there are no constants
    sal_Int16 nDateFormat = 0;
    OUString sDateFormat = m_sDateFormat.makeStringAndClear();
    if (sDateFormat == "M/d/yyyy" || sDateFormat == "M.d.yyyy")
        // Approximate with MM.dd.yyy
        nDateFormat = 8;
    else
        // Set default format, so at least the date picker is created.
        SAL_WARN("writerfilter", "unhandled w:dateFormat value");
    xPropertySet->setPropertyValue("DateFormat", uno::makeAny(nDateFormat));

    util::Date aDate;
    util::DateTime aDateTime;
    if (utl::ISO8601parseDateTime(m_sDate.makeStringAndClear(), aDateTime))
    {
        utl::extractDate(aDateTime, aDate);
        xPropertySet->setPropertyValue("Date", uno::makeAny(aDate));
    }
    else
        xPropertySet->setPropertyValue("HelpText", uno::makeAny(rContentText));

    // append date format to grab bag
    comphelper::SequenceAsHashMap aGrabBag;
    aGrabBag["OriginalDate"] <<= aDate;
    aGrabBag["OriginalContent"] <<= rContentText;
    aGrabBag["DateFormat"] <<= sDateFormat;
    aGrabBag["Locale"] <<= m_sLocale.makeStringAndClear();
    aGrabBag["CharFormat"] <<= aCharFormat.Value;
    // merge in properties like ooxml:CT_SdtPr_alias and friends.
    aGrabBag.update(comphelper::SequenceAsHashMap(m_aGrabBag));
    // and empty the property list, so they won't end up on the next sdt as well
    m_aGrabBag.realloc(0);

    std::vector<OUString> aItems;
    createControlShape(lcl_getOptimalWidth(m_rDM_Impl.GetStyleSheetTable(), rContentText, aItems), xControlModel, aGrabBag.getAsConstPropertyValueList());
}

void SdtHelper::createControlShape(awt::Size aSize, uno::Reference<awt::XControlModel> const& xControlModel)
{
    createControlShape(aSize, xControlModel, uno::Sequence<beans::PropertyValue>());
}

void SdtHelper::createControlShape(awt::Size aSize, uno::Reference<awt::XControlModel> const& xControlModel, const uno::Sequence<beans::PropertyValue>& rGrabBag)
{
    uno::Reference<drawing::XControlShape> xControlShape(m_rDM_Impl.GetTextFactory()->createInstance("com.sun.star.drawing.ControlShape"), uno::UNO_QUERY);
    xControlShape->setSize(aSize);
    xControlShape->setControl(xControlModel);

    uno::Reference<beans::XPropertySet> xPropertySet(xControlShape, uno::UNO_QUERY);
    xPropertySet->setPropertyValue("VertOrient", uno::makeAny(text::VertOrientation::CENTER));

    if (rGrabBag.hasElements())
        xPropertySet->setPropertyValue(UNO_NAME_MISC_OBJ_INTEROPGRABBAG, uno::makeAny(rGrabBag));

    uno::Reference<text::XTextContent> xTextContent(xControlShape, uno::UNO_QUERY);
    m_rDM_Impl.appendTextContent(xTextContent, uno::Sequence< beans::PropertyValue >());
    m_bHasElements = true;
}







void SdtHelper::appendToInteropGrabBag(com::sun::star::beans::PropertyValue rValue)
{
    sal_Int32 nLength = m_aGrabBag.getLength();
    m_aGrabBag.realloc(nLength + 1);
    m_aGrabBag[nLength] = rValue;
}

com::sun::star::uno::Sequence<com::sun::star::beans::PropertyValue> SdtHelper::getInteropGrabBagAndClear()
{
    com::sun::star::uno::Sequence<com::sun::star::beans::PropertyValue> aRet = m_aGrabBag;
    m_aGrabBag.realloc(0);
    return aRet;
}

bool SdtHelper::isInteropGrabBagEmpty()
{
    return m_aGrabBag.getLength() == 0;
}

sal_Int32 SdtHelper::getInteropGrabBagSize()
{
    return m_aGrabBag.getLength();
}

bool SdtHelper::containedInInteropGrabBag(const OUString& rValueName)
{
    for (sal_Int32 i=0; i < m_aGrabBag.getLength(); ++i)
        if (m_aGrabBag[i].Name == rValueName)
            return true;

    return false;
}

} // namespace dmapper
} // namespace writerfilter

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
