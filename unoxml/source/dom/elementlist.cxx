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

#include "elementlist.hxx"

#include <string.h>

#include <element.hxx>
#include <document.hxx>

using namespace css::uno;
using namespace css::xml::dom;
using namespace css::xml::dom::events;

namespace
{
    class WeakEventListener : public ::cppu::WeakImplHelper1<css::xml::dom::events::XEventListener>
    {
    private:
        css::uno::WeakReference<css::xml::dom::events::XEventListener> mxOwner;

    public:
        WeakEventListener(const css::uno::Reference<css::xml::dom::events::XEventListener>& rOwner)
            : mxOwner(rOwner)
        {
        }

        virtual ~WeakEventListener()
        {
        }

        virtual void SAL_CALL handleEvent(const css::uno::Reference<css::xml::dom::events::XEvent>& rEvent)
            throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        {
            css::uno::Reference<css::xml::dom::events::XEventListener> xOwner(mxOwner.get(),
                css::uno::UNO_QUERY);
            if (xOwner.is())
                xOwner->handleEvent(rEvent);
        }
    };
}

namespace DOM
{

    static xmlChar* lcl_initXmlString(OUString const& rString)
    {
        OString const os =
            OUStringToOString(rString, RTL_TEXTENCODING_UTF8);
        xmlChar *const pRet = new xmlChar[os.getLength() + 1];
        strcpy(reinterpret_cast<char*>(pRet), os.getStr());
        return pRet;
    }

    CElementList::CElementList(::rtl::Reference<CElement> const& pElement,
            ::osl::Mutex & rMutex,
            OUString const& rName, OUString const*const pURI)
        : m_xImpl(new CElementListImpl(pElement, rMutex, rName, pURI))
    {
        if (pElement.is()) {
            m_xImpl->registerListener(*pElement);
        }
    }

    CElementListImpl::CElementListImpl(::rtl::Reference<CElement> const& pElement,
            ::osl::Mutex & rMutex,
            OUString const& rName, OUString const*const pURI)
        : m_pElement(pElement)
        , m_rMutex(rMutex)
        , m_pName(lcl_initXmlString(rName))
        , m_pURI((pURI) ? lcl_initXmlString(*pURI) : 0)
        , m_bRebuild(true)
    {
    }

    CElementListImpl::~CElementListImpl()
    {
        if (m_xEventListener.is() && m_pElement.is())
        {
            Reference< XEventTarget > xTarget(static_cast<XElement*>(m_pElement.get()), UNO_QUERY);
            assert(xTarget.is());
            if (!xTarget.is())
                return;
            bool capture = false;
            xTarget->removeEventListener("DOMSubtreeModified", m_xEventListener, capture);
        }
    }

    void CElementListImpl::registerListener(CElement & rElement)
    {
        try {
            Reference< XEventTarget > const xTarget(
                    static_cast<XElement*>(& rElement), UNO_QUERY_THROW);
            bool capture = false;
            m_xEventListener = new WeakEventListener(this);
            xTarget->addEventListener("DOMSubtreeModified",
                    m_xEventListener, capture);
        } catch (const Exception &e){
            OString aMsg("Exception caught while registering NodeList as listener:\n");
            aMsg += OUStringToOString(e.Message, RTL_TEXTENCODING_ASCII_US);
            OSL_FAIL(aMsg.getStr());
        }
    }

    void CElementListImpl::buildlist(xmlNodePtr pNode, bool start)
    {
        // bail out if no rebuild is needed
        if (start) {
            if (!m_bRebuild)
            {
                return;
            } else {
                m_nodevector.erase(m_nodevector.begin(), m_nodevector.end());
                m_bRebuild = false; // don't rebuild until tree is mutated
            }
        }

        while (pNode != NULL )
        {
            if (pNode->type == XML_ELEMENT_NODE &&
                (strcmp((char*)pNode->name, (char*)m_pName.get()) == 0))
            {
                if (!m_pURI) {
                    m_nodevector.push_back(pNode);
                } else {
                    if (pNode->ns != NULL && (0 ==
                         strcmp((char*)pNode->ns->href, (char*)m_pURI.get())))
                    {
                        m_nodevector.push_back(pNode);
                    }
                }
            }
            if (pNode->children != NULL) buildlist(pNode->children, false);

            if (!start) pNode = pNode->next;
            else break; // fold back
        }
    }

    /**
    The number of nodes in the list.
    */
    sal_Int32 SAL_CALL CElementListImpl::getLength() throw (RuntimeException, std::exception)
    {
        ::osl::MutexGuard const g(m_rMutex);

        if (!m_pElement.is()) { return 0; }

        // this has to be 'live'
        buildlist(m_pElement->GetNodePtr());
        return m_nodevector.size();
    }
    /**
    Returns the indexth item in the collection.
    */
    Reference< XNode > SAL_CALL CElementListImpl::item(sal_Int32 index)
        throw (RuntimeException, std::exception)
    {
        if (index < 0) throw RuntimeException();

        ::osl::MutexGuard const g(m_rMutex);

        if (!m_pElement.is()) { return 0; }

        buildlist(m_pElement->GetNodePtr());
        if (m_nodevector.size() <= static_cast<size_t>(index)) {
            throw RuntimeException();
        }
        Reference< XNode > const xRet(
            m_pElement->GetOwnerDocument().GetCNode(m_nodevector[index]).get());
        return xRet;
    }

    // tree mutations can change the list
    void SAL_CALL CElementListImpl::handleEvent(Reference< XEvent > const&)
        throw (RuntimeException, std::exception)
    {
        ::osl::MutexGuard const g(m_rMutex);

        m_bRebuild = true;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
