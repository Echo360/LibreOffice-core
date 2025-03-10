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

#ifndef INCLUDED_DBACCESS_SOURCE_FILTER_XML_XMLSTYLEIMPORT_HXX
#define INCLUDED_DBACCESS_SOURCE_FILTER_XML_XMLSTYLEIMPORT_HXX

#include <rtl/ustring.hxx>
#include <xmloff/xmlimp.hxx>
#include <xmloff/xmlictxt.hxx>
#include <xmloff/maptype.hxx>
#include <xmloff/prstylei.hxx>
#include <xmloff/xmlimppr.hxx>
#include <xmloff/XMLTextMasterPageContext.hxx>
#include <xmloff/XMLTextMasterStylesContext.hxx>
#include <xmloff/contextid.hxx>
#include <xmloff/controlpropertyhdl.hxx>
#include <vector>

namespace dbaxml
{
    class ODBFilter;

    class OTableStyleContext : public XMLPropStyleContext
    {
        OUString             m_sDataStyleName;
        OUString             sPageStyle;
        const OUString         sNumberFormat;
        SvXMLStylesContext*         pStyles;
        com::sun::star::uno::Any    aConditionalFormat;
        sal_Int32                   m_nNumberFormat;

        ODBFilter& GetOwnImport();

    protected:

        virtual void SetAttribute( sal_uInt16 nPrefixKey,
                                const OUString& rLocalName,
                                const OUString& rValue ) SAL_OVERRIDE;

    public:

        TYPEINFO_OVERRIDE();

        OTableStyleContext( ODBFilter& rImport, sal_uInt16 nPrfx,
                const OUString& rLName,
                const ::com::sun::star::uno::Reference< ::com::sun::star::xml::sax::XAttributeList > & xAttrList,
                SvXMLStylesContext& rStyles, sal_uInt16 nFamily, bool bDefaultStyle = false );

        virtual ~OTableStyleContext();

        virtual void FillPropertySet(const ::com::sun::star::uno::Reference<
                    ::com::sun::star::beans::XPropertySet > & rPropSet ) SAL_OVERRIDE;

        virtual void SetDefaults() SAL_OVERRIDE;

          void AddProperty(sal_Int16 nContextID, const com::sun::star::uno::Any& aValue);

        sal_Int32 GetNumberFormat() { return m_nNumberFormat; }
    };

    class OTableStylesContext : public SvXMLStylesContext
    {
        const OUString sTableStyleServiceName;
        const OUString sColumnStyleServiceName;
        const OUString sCellStyleServiceName;
        sal_Int32 m_nNumberFormatIndex;
        sal_Int32 m_nMasterPageNameIndex;
        bool bAutoStyles : 1;

        mutable rtl::Reference < SvXMLImportPropertyMapper > m_xTableImpPropMapper;
        mutable rtl::Reference < SvXMLImportPropertyMapper > m_xColumnImpPropMapper;
        mutable rtl::Reference < SvXMLImportPropertyMapper > m_xCellImpPropMapper;

        ODBFilter& GetOwnImport();

    protected:

        // Create a style context.
        virtual SvXMLStyleContext *CreateStyleStyleChildContext(
                sal_uInt16 nFamily,
                sal_uInt16 nPrefix,
                const OUString& rLocalName,
                const ::com::sun::star::uno::Reference< ::com::sun::star::xml::sax::XAttributeList > & xAttrList ) SAL_OVERRIDE;

    public:

        TYPEINFO_OVERRIDE();

        OTableStylesContext( SvXMLImport& rImport, sal_uInt16 nPrfx ,
                const OUString& rLName ,
                const ::com::sun::star::uno::Reference< ::com::sun::star::xml::sax::XAttributeList > & xAttrList,
                const bool bAutoStyles );
        virtual ~OTableStylesContext();

        virtual void EndElement() SAL_OVERRIDE;

        virtual rtl::Reference < SvXMLImportPropertyMapper > GetImportPropertyMapper(
                            sal_uInt16 nFamily ) const SAL_OVERRIDE;
        virtual ::com::sun::star::uno::Reference <
                        ::com::sun::star::container::XNameContainer >
            GetStylesContainer( sal_uInt16 nFamily ) const SAL_OVERRIDE;
        virtual OUString GetServiceName( sal_uInt16 nFamily ) const SAL_OVERRIDE;

        sal_Int32 GetIndex(const sal_Int16 nContextID);
    };
} // dbaxml
#endif // INCLUDED_DBACCESS_SOURCE_FILTER_XML_XMLSTYLEIMPORT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
