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


#ifndef INCLUDED_UCB_SOURCE_UCP_WEBDAV_DAVSESSIONFACTORY_HXX
#define INCLUDED_UCB_SOURCE_UCP_WEBDAV_DAVSESSIONFACTORY_HXX

#ifdef min
#undef min // GNU libstdc++ <memory> includes <limit> which defines methods called min...
#endif
#include <map>
#include <memory>
#include <osl/mutex.hxx>
#include <salhelper/simplereferenceobject.hxx>
#include <rtl/ref.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <ucbhelper/proxydecider.hxx>
#include "DAVException.hxx"

using namespace com::sun::star;

namespace com { namespace sun { namespace star { namespace lang {
    class XMultiServiceFactory;
} } } }

namespace http_dav_ucp
{

class DAVSession;

class DAVSessionFactory : public salhelper::SimpleReferenceObject
{
public:
    virtual ~DAVSessionFactory();

    rtl::Reference< DAVSession >
        createDAVSession( const OUString & inUri,
                          const ::com::sun::star::uno::Reference<
                            ::com::sun::star::uno::XComponentContext >&
                                rxContext )
            throw( DAVException );

private:
    typedef std::map< OUString, DAVSession * > Map;

    Map m_aMap;
    osl::Mutex m_aMutex;
    std::auto_ptr< ucbhelper::InternetProxyDecider > m_xProxyDecider;

    void releaseElement( DAVSession * pElement );

    friend class DAVSession;
};

} // namespace http_dav_ucp

#endif // INCLUDED_UCB_SOURCE_UCP_WEBDAV_DAVSESSIONFACTORY_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
