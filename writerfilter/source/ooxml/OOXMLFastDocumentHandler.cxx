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

#include <iostream>
#include <boost/shared_ptr.hpp>
#include "OOXMLFastDocumentHandler.hxx"
#include "OOXMLFastContextHandler.hxx"
#include "oox/token/tokens.hxx"
#include "OOXMLFactory.hxx"

namespace writerfilter {
namespace ooxml
{
using namespace ::com::sun::star;
using namespace ::std;


OOXMLFastDocumentHandler::OOXMLFastDocumentHandler(
    uno::Reference< uno::XComponentContext > const & context,
    Stream* pStream,
    OOXMLDocumentImpl* pDocument,
    sal_Int32 nXNoteId )
    : m_xContext(context)
    , mpStream( pStream )
#ifdef DEBUG_DOMAINMAPPER
    , mpTmpStream()
#endif
    , mpDocument( pDocument )
    , mnXNoteId( nXNoteId )
    , mpContextHandler()
{
}

// ::com::sun::star::xml::sax::XFastContextHandler:
void SAL_CALL OOXMLFastDocumentHandler::startFastElement
(::sal_Int32
#ifdef DEBUG_DOMAINMAPPER
Element
#endif
, const uno::Reference< xml::sax::XFastAttributeList > & /*Attribs*/)
    throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
#ifdef DEBUG_DOMAINMAPPER
    clog << this << ":start element:"
         << fastTokenToId(Element)
         << endl;
#endif
}

void SAL_CALL OOXMLFastDocumentHandler::startUnknownElement
(const OUString &
#ifdef DEBUG_DOMAINMAPPER
Namespace
#endif
, const OUString &
#ifdef DEBUG_DOMAINMAPPER
Name
#endif
,
 const uno::Reference< xml::sax::XFastAttributeList > & /*Attribs*/)
throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
#ifdef DEBUG_DOMAINMAPPER
    clog << this << ":start unknown element:"
         << OUStringToOString(Namespace, RTL_TEXTENCODING_ASCII_US).getStr()
         << ":"
         << OUStringToOString(Name, RTL_TEXTENCODING_ASCII_US).getStr()
         << endl;
#endif
}

void SAL_CALL OOXMLFastDocumentHandler::endFastElement(::sal_Int32
#ifdef DEBUG_DOMAINMAPPER
Element
#endif
)
throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
#ifdef DEBUG_DOMAINMAPPER
    clog << this << ":end element:"
         << fastTokenToId(Element)
         << endl;
#endif
}

void SAL_CALL OOXMLFastDocumentHandler::endUnknownElement
(const OUString &
#ifdef DEBUG_DOMAINMAPPER
Namespace
#endif
, const OUString &
#ifdef DEBUG_DOMAINMAPPER
Name
#endif
)
throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
#ifdef DEBUG_DOMAINMAPPER
    clog << this << ":end unknown element:"
         << OUStringToOString(Namespace, RTL_TEXTENCODING_ASCII_US).getStr()
         << ":"
         << OUStringToOString(Name, RTL_TEXTENCODING_ASCII_US).getStr()
         << endl;
#endif
}

OOXMLFastContextHandler::Pointer_t
OOXMLFastDocumentHandler::getContextHandler() const
{
    if (mpContextHandler == OOXMLFastContextHandler::Pointer_t())
    {
        mpContextHandler.reset
        (new OOXMLFastContextHandler(m_xContext));
        mpContextHandler->setStream(mpStream);
        mpContextHandler->setDocument(mpDocument);
        mpContextHandler->setXNoteId(mnXNoteId);
        mpContextHandler->setForwardEvents(true);
    }

    return mpContextHandler;
}

uno::Reference< xml::sax::XFastContextHandler > SAL_CALL
 OOXMLFastDocumentHandler::createFastChildContext
(::sal_Int32 Element,
 const uno::Reference< xml::sax::XFastAttributeList > & /*Attribs*/)
    throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
#ifdef DEBUG_DOMAINMAPPER
    clog << this << ":createFastChildContext:"
         << fastTokenToId(Element)
         << endl;
#endif

    if ( mpStream == 0 && mpDocument == 0 )
    {
        // document handler has been created as unknown child - see <OOXMLFastDocumentHandler::createUnknownChildContext(..)>
        // --> do not provide a child context
        return NULL;
    }

    return OOXMLFactory::getInstance()->createFastChildContextFromStart(getContextHandler().get(), Element);
}

uno::Reference< xml::sax::XFastContextHandler > SAL_CALL
OOXMLFastDocumentHandler::createUnknownChildContext
(const OUString &
#ifdef DEBUG_DOMAINMAPPER
Namespace
#endif
,
 const OUString &
#ifdef DEBUG_DOMAINMAPPER
Name
#endif
, const uno::Reference< xml::sax::XFastAttributeList > & /*Attribs*/)
    throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
#ifdef DEBUG_DOMAINMAPPER
    clog << this << ":createUnknownChildContext:"
         << OUStringToOString(Namespace, RTL_TEXTENCODING_ASCII_US).getStr()
         << ":"
         << OUStringToOString(Name, RTL_TEXTENCODING_ASCII_US).getStr()
         << endl;
#endif

    return uno::Reference< xml::sax::XFastContextHandler >
        ( new OOXMLFastDocumentHandler( m_xContext, 0, 0, 0 ) );
}

void SAL_CALL OOXMLFastDocumentHandler::characters(const OUString & /*aChars*/)
    throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
}

// ::com::sun::star::xml::sax::XFastDocumentHandler:
void SAL_CALL OOXMLFastDocumentHandler::startDocument()
    throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
}

void SAL_CALL OOXMLFastDocumentHandler::endDocument()
    throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
}

void SAL_CALL OOXMLFastDocumentHandler::setDocumentLocator
(const uno::Reference< xml::sax::XLocator > & /*xLocator*/)
    throw (uno::RuntimeException, xml::sax::SAXException, std::exception)
{
}

void OOXMLFastDocumentHandler::setIsSubstream( bool bSubstream )
{
    if ( mpStream != 0 && mpDocument != 0 )
    {
        getContextHandler( )->getParserState( )->setInSectionGroup( bSubstream );
    }
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
