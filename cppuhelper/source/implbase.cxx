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

#include <cppuhelper/compbase_ex.hxx>
#include <osl/diagnose.h>
#include <rtl/instance.hxx>
#include <rtl/string.hxx>

#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/uno/RuntimeException.hpp>

using namespace ::osl;
using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

using rtl::OUString;
using rtl::OString;

namespace cppu
{

// WeakComponentImplHelperBase

WeakComponentImplHelperBase::WeakComponentImplHelperBase( Mutex & rMutex )
    : rBHelper( rMutex )
{
}

WeakComponentImplHelperBase::~WeakComponentImplHelperBase()
{
}

void WeakComponentImplHelperBase::disposing()
{
}

Any WeakComponentImplHelperBase::queryInterface( Type const & rType )
    throw (RuntimeException, std::exception)
{
    if (rType == ::getCppuType( (Reference< lang::XComponent > const *)0 ))
    {
        void * p = static_cast< lang::XComponent * >( this );
        return Any( &p, rType );
    }
    return OWeakObject::queryInterface( rType );
}

void WeakComponentImplHelperBase::acquire()
    throw ()
{
    OWeakObject::acquire();
}

void WeakComponentImplHelperBase::release()
    throw ()
{
    if (osl_atomic_decrement( &m_refCount ) == 0) {
        // ensure no other references are created, via the weak connection point, from now on
        disposeWeakConnectionPoint();
        // restore reference count:
        osl_atomic_increment( &m_refCount );
        if (! rBHelper.bDisposed) {
            try {
                dispose();
            }
            catch (RuntimeException const& exc) { // don't break throw ()
                OSL_FAIL(
                    OUStringToOString(
                        exc.Message, RTL_TEXTENCODING_ASCII_US ).getStr() );
                static_cast<void>(exc);
            }
            OSL_ASSERT( rBHelper.bDisposed );
        }
        OWeakObject::release();
    }
}

void WeakComponentImplHelperBase::dispose()
    throw (RuntimeException, std::exception)
{
    ClearableMutexGuard aGuard( rBHelper.rMutex );
    if (!rBHelper.bDisposed && !rBHelper.bInDispose)
    {
        rBHelper.bInDispose = sal_True;
        aGuard.clear();
        try
        {
            // side effect: keeping a reference to this
            lang::EventObject aEvt( static_cast< OWeakObject * >( this ) );
            try
            {
                rBHelper.aLC.disposeAndClear( aEvt );
                disposing();
            }
            catch (...)
            {
                MutexGuard aGuard2( rBHelper.rMutex );
                // bDisposed and bInDispose must be set in this order:
                rBHelper.bDisposed = sal_True;
                rBHelper.bInDispose = sal_False;
                throw;
            }
            MutexGuard aGuard2( rBHelper.rMutex );
            // bDisposed and bInDispose must be set in this order:
            rBHelper.bDisposed = sal_True;
            rBHelper.bInDispose = sal_False;
        }
        catch (RuntimeException &)
        {
            throw;
        }
        catch (Exception & exc)
        {
            throw RuntimeException(
                OUString("unexpected UNO exception caught: ") +
                exc.Message );
        }
    }
}

void WeakComponentImplHelperBase::addEventListener(
    Reference< lang::XEventListener > const & xListener )
    throw (RuntimeException, std::exception)
{
    ClearableMutexGuard aGuard( rBHelper.rMutex );
    if (rBHelper.bDisposed || rBHelper.bInDispose)
    {
        aGuard.clear();
        lang::EventObject aEvt( static_cast< OWeakObject * >( this ) );
        xListener->disposing( aEvt );
    }
    else
    {
        rBHelper.addListener( ::getCppuType( &xListener ), xListener );
    }
}

void WeakComponentImplHelperBase::removeEventListener(
    Reference< lang::XEventListener > const & xListener )
    throw (RuntimeException, std::exception)
{
    rBHelper.removeListener( ::getCppuType( &xListener ), xListener );
}

// WeakAggComponentImplHelperBase

WeakAggComponentImplHelperBase::WeakAggComponentImplHelperBase( Mutex & rMutex )
    : rBHelper( rMutex )
{
}

WeakAggComponentImplHelperBase::~WeakAggComponentImplHelperBase()
{
}

void WeakAggComponentImplHelperBase::disposing()
{
}

Any WeakAggComponentImplHelperBase::queryInterface( Type const & rType )
    throw (RuntimeException, std::exception)
{
    return OWeakAggObject::queryInterface( rType );
}

Any WeakAggComponentImplHelperBase::queryAggregation( Type const & rType )
    throw (RuntimeException, std::exception)
{
    if (rType == ::getCppuType( (Reference< lang::XComponent > const *)0 ))
    {
        void * p = static_cast< lang::XComponent * >( this );
        return Any( &p, rType );
    }
    return OWeakAggObject::queryAggregation( rType );
}

void WeakAggComponentImplHelperBase::acquire()
    throw ()
{
    OWeakAggObject::acquire();
}

void WeakAggComponentImplHelperBase::release()
    throw ()
{
    Reference<XInterface> const xDelegator_(xDelegator);
    if (xDelegator_.is()) {
        OWeakAggObject::release();
    }
    else if (osl_atomic_decrement( &m_refCount ) == 0) {
        // ensure no other references are created, via the weak connection point, from now on
        disposeWeakConnectionPoint();
        // restore reference count:
        osl_atomic_increment( &m_refCount );
        if (! rBHelper.bDisposed) {
            try {
                dispose();
            }
            catch (RuntimeException const& exc) { // don't break throw ()
                OSL_FAIL(
                    OUStringToOString(
                        exc.Message, RTL_TEXTENCODING_ASCII_US ).getStr() );
                static_cast<void>(exc);
            }
            OSL_ASSERT( rBHelper.bDisposed );
        }
        OWeakAggObject::release();
    }
}

void WeakAggComponentImplHelperBase::dispose()
    throw (RuntimeException, std::exception)
{
    ClearableMutexGuard aGuard( rBHelper.rMutex );
    if (!rBHelper.bDisposed && !rBHelper.bInDispose)
    {
        rBHelper.bInDispose = sal_True;
        aGuard.clear();
        try
        {
            // side effect: keeping a reference to this
            lang::EventObject aEvt( static_cast< OWeakObject * >( this ) );
            try
            {
                rBHelper.aLC.disposeAndClear( aEvt );
                disposing();
            }
            catch (...)
            {
                MutexGuard aGuard2( rBHelper.rMutex );
                // bDisposed and bInDispose must be set in this order:
                rBHelper.bDisposed = sal_True;
                rBHelper.bInDispose = sal_False;
                throw;
            }
            MutexGuard aGuard2( rBHelper.rMutex );
            // bDisposed and bInDispose must be set in this order:
            rBHelper.bDisposed = sal_True;
            rBHelper.bInDispose = sal_False;
        }
        catch (RuntimeException &)
        {
            throw;
        }
        catch (Exception & exc)
        {
            throw RuntimeException(
                OUString("unexpected UNO exception caught: ") +
                exc.Message );
        }
    }
}

void WeakAggComponentImplHelperBase::addEventListener(
    Reference< lang::XEventListener > const & xListener )
    throw (RuntimeException, std::exception)
{
    ClearableMutexGuard aGuard( rBHelper.rMutex );
    if (rBHelper.bDisposed || rBHelper.bInDispose)
    {
        aGuard.clear();
        lang::EventObject aEvt( static_cast< OWeakObject * >( this ) );
        xListener->disposing( aEvt );
    }
    else
    {
        rBHelper.addListener( ::getCppuType( &xListener ), xListener );
    }
}

void WeakAggComponentImplHelperBase::removeEventListener(
    Reference< lang::XEventListener > const & xListener )
    throw (RuntimeException, std::exception)
{
    rBHelper.removeListener( ::getCppuType( &xListener ), xListener );
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
