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

#ifndef INCLUDED_WRITERFILTER_SOURCE_DMAPPER_SETTINGSTABLE_HXX
#define INCLUDED_WRITERFILTER_SOURCE_DMAPPER_SETTINGSTABLE_HXX

#include <resourcemodel/LoggedResources.hxx>
#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/text/XTextDocument.hpp>
#include <map>

namespace com{ namespace sun{ namespace star{
namespace lang{
class XMultiServiceFactory;
struct Locale;
}
}}}

namespace writerfilter {
namespace dmapper
{
class DomainMapper;

struct SettingsTable_Impl;

class SettingsTable : public LoggedProperties, public LoggedTable
{
    SettingsTable_Impl *m_pImpl;

 public:
    SettingsTable(  DomainMapper& rDMapper,
            const ::com::sun::star::uno::Reference< ::com::sun::star::lang::XMultiServiceFactory > & xTextFactory
            );
    virtual ~SettingsTable();

    //returns default TabStop in 1/100th mm
    int GetDefaultTabStop() const;

    /// Automatically update styles from document template?
    bool GetLinkStyles() const;

    /// What's the zoom factor set in percents?
    sal_Int16 GetZoomFactor() const;

    /// What's the requested view? E.g. "web".
    Id GetView() const;

    bool GetEvenAndOddHeaders() const;

    bool GetUsePrinterMetrics() const;

    bool GetEmbedTrueTypeFonts() const;
    bool GetEmbedSystemFonts() const;

    bool GetDoNotUseHTMLParagraphAutoSpacing() const;
    bool GetSplitPgBreakAndParaMark() const;
    bool GetMirrorMarginSettings() const;
    bool GetNoColumnBalance() const;

    css::uno::Sequence<css::beans::PropertyValue> GetThemeFontLangProperties() const;

    css::uno::Sequence<css::beans::PropertyValue> GetCompatSettings() const;

    void ApplyProperties(css::uno::Reference<css::text::XTextDocument> const& xDoc);

 private:
    // Properties
    virtual void lcl_attribute(Id Name, Value & val) SAL_OVERRIDE;
    virtual void lcl_sprm(Sprm & sprm) SAL_OVERRIDE;

    // Table
    virtual void lcl_entry(int pos, writerfilter::Reference<Properties>::Pointer_t ref) SAL_OVERRIDE;

};
typedef boost::shared_ptr< SettingsTable >          SettingsTablePtr;
}}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
