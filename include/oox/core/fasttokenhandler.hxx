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

#ifndef INCLUDED_OOX_CORE_FASTTOKENHANDLER_HXX
#define INCLUDED_OOX_CORE_FASTTOKENHANDLER_HXX

#include <oox/dllapi.h>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/xml/sax/XFastTokenHandler.hpp>
#include <cppuhelper/implbase2.hxx>
#include <sax/fastattribs.hxx>

namespace oox { class TokenMap; }

namespace oox {
namespace core {



typedef ::cppu::WeakImplHelper2< ::com::sun::star::lang::XServiceInfo, ::com::sun::star::xml::sax::XFastTokenHandler > FastTokenHandler_BASE;

/** Wrapper implementing the com.sun.star.xml.sax.XFastTokenHandler API interface
    that provides access to the tokens generated from the internal token name list.
 */
class OOX_DLLPUBLIC FastTokenHandler : public FastTokenHandler_BASE,
                         public sax_fastparser::FastTokenHandlerBase
{
public:
    explicit            FastTokenHandler();
    virtual             ~FastTokenHandler();

    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName() throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual sal_Bool SAL_CALL supportsService( const OUString& rServiceName ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual ::com::sun::star::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames() throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    // XFastTokenHandler
    virtual sal_Int32 SAL_CALL getToken( const OUString& rIdentifier ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual OUString SAL_CALL getIdentifier( sal_Int32 nToken ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual ::com::sun::star::uno::Sequence< sal_Int8 > SAL_CALL getUTF8Identifier( sal_Int32 nToken ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual sal_Int32 SAL_CALL getTokenFromUTF8( const ::com::sun::star::uno::Sequence< sal_Int8 >& Identifier ) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    // Much faster direct C++ shortcut to the method that matters
    virtual sal_Int32 getTokenDirect( const char *pToken, sal_Int32 nLength ) const SAL_OVERRIDE;

private:
    const TokenMap&     mrTokenMap;     ///< Reference to global token map singleton.
};



} // namespace core
} // namespace oox

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
