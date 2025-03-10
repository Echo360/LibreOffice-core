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
#ifndef INCLUDED_SD_SOURCE_UI_SIDEBAR_PANELFACTORY_HXX
#define INCLUDED_SD_SOURCE_UI_SIDEBAR_PANELFACTORY_HXX

#include <cppuhelper/compbase1.hxx>
#include <cppuhelper/basemutex.hxx>
#include <rtl/ref.hxx>
#include "framework/Pane.hxx"

#include <com/sun/star/ui/XUIElementFactory.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/lang/XInitialization.hpp>

#include <map>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace cssu = ::com::sun::star::uno;

namespace sd {
    class ViewShellBase;
}

namespace sd { namespace sidebar {

namespace
{
    typedef ::cppu::WeakComponentImplHelper1 <
        css::ui::XUIElementFactory
        > PanelFactoryInterfaceBase;
}

class PanelFactory
    : private ::boost::noncopyable,
      private ::cppu::BaseMutex,
      public PanelFactoryInterfaceBase
{
public:
    static ::rtl::OUString SAL_CALL getImplementationName (void);
    static cssu::Reference<cssu::XInterface> SAL_CALL createInstance (
        const cssu::Reference<css::lang::XMultiServiceFactory>& rxFactory);
    static cssu::Sequence<rtl::OUString> SAL_CALL getSupportedServiceNames (void);

    PanelFactory (const cssu::Reference<cssu::XComponentContext>& rxContext);
    virtual ~PanelFactory (void);

    virtual void SAL_CALL disposing (void) SAL_OVERRIDE;

    // XUIElementFactory

    cssu::Reference<css::ui::XUIElement> SAL_CALL createUIElement (
        const ::rtl::OUString& rsResourceURL,
        const ::cssu::Sequence<css::beans::PropertyValue>& rArguments)
        throw(
            css::container::NoSuchElementException,
            css::lang::IllegalArgumentException,
            cssu::RuntimeException, std::exception) SAL_OVERRIDE;
};

} } // end of namespace sd::sidebar

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
