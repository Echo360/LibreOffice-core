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

#ifndef INCLUDED_CONNECTIVITY_SOURCE_SIMPLEDBT_DBTFACTORY_HXX
#define INCLUDED_CONNECTIVITY_SOURCE_SIMPLEDBT_DBTFACTORY_HXX

#include <connectivity/virtualdbtools.hxx>


namespace connectivity
{



    //= ODataAccessToolsFactory

    class ODataAccessToolsFactory : public simple::IDataAccessToolsFactory
    {
    protected:
        ::rtl::Reference< simple::IDataAccessTypeConversion >   m_xTypeConversionHelper;
        ::rtl::Reference< simple::IDataAccessTools >            m_xToolsHelper;

    public:
        ODataAccessToolsFactory();
        virtual ~ODataAccessToolsFactory() {}

        // IDataAccessToolsFactory
        virtual ::rtl::Reference< simple::ISQLParser >  createSQLParser(
            const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XComponentContext >& _rxContext,
            const IParseContext* _pContext
        ) const SAL_OVERRIDE;

        virtual ::rtl::Reference< simple::IDataAccessCharSet > createCharsetHelper( ) const SAL_OVERRIDE;

        virtual ::rtl::Reference< simple::IDataAccessTypeConversion > getTypeConversionHelper() SAL_OVERRIDE;

        virtual ::rtl::Reference< simple::IDataAccessTools > getDataAccessTools() SAL_OVERRIDE;

        virtual ::std::auto_ptr< ::dbtools::FormattedColumnValue >  createFormattedColumnValue(
            const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XComponentContext >& _rxContext,
            const ::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XRowSet >& _rxRowSet,
            const ::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertySet >& _rxColumn
        ) SAL_OVERRIDE;

    };


}   // namespace connectivity


#endif // INCLUDED_CONNECTIVITY_SOURCE_SIMPLEDBT_DBTFACTORY_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
