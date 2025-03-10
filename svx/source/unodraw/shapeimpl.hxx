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
#ifndef INCLUDED_SVX_SOURCE_UNODRAW_SHAPEIMPL_HXX
#define INCLUDED_SVX_SOURCE_UNODRAW_SHAPEIMPL_HXX

#include <svx/unoshape.hxx>

/***********************************************************************
*                                                                      *
***********************************************************************/

class SvxShapeCaption : public SvxShapeText
{
public:
    SvxShapeCaption( SdrObject* pObj ) throw();
    virtual ~SvxShapeCaption() throw();
};

/***********************************************************************
*                                                                      *
***********************************************************************/

class SvxPluginShape : public SvxOle2Shape
{
protected:
    // overide these for special property handling in subcasses. Return true if property is handled
    virtual bool setPropertyValueImpl( const OUString& rName, const SfxItemPropertySimpleEntry* pProperty, const ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException) SAL_OVERRIDE;
    virtual bool getPropertyValueImpl( const OUString& rName, const SfxItemPropertySimpleEntry* pProperty, ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

public:
    SvxPluginShape( SdrObject* pObj ) throw();
    virtual ~SvxPluginShape() throw();

    virtual void SAL_CALL setPropertyValue( const OUString& aPropertyName, const ::com::sun::star::uno::Any& aValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    using SvxUnoTextRangeBase::setPropertyValue;

    virtual void SAL_CALL setPropertyValues( const ::com::sun::star::uno::Sequence< OUString >& aPropertyNames, const ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Any >& aValues ) throw (::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    virtual void Create( SdrObject* pNewOpj, SvxDrawPage* pNewPage = NULL ) SAL_OVERRIDE;
};

/***********************************************************************
*                                                                      *
***********************************************************************/

class SvxAppletShape : public SvxOle2Shape
{
protected:
    // overide these for special property handling in subcasses. Return true if property is handled
    virtual bool setPropertyValueImpl( const OUString& rName, const SfxItemPropertySimpleEntry* pProperty, const ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException) SAL_OVERRIDE;
    virtual bool getPropertyValueImpl( const OUString& rName, const SfxItemPropertySimpleEntry* pProperty, ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

public:
    SvxAppletShape( SdrObject* pObj ) throw();
    virtual ~SvxAppletShape() throw();

    virtual void SAL_CALL setPropertyValue( const OUString& aPropertyName, const ::com::sun::star::uno::Any& aValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    using SvxUnoTextRangeBase::setPropertyValue;

    virtual void SAL_CALL setPropertyValues( const ::com::sun::star::uno::Sequence< OUString >& aPropertyNames, const ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Any >& aValues ) throw (::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    virtual void Create( SdrObject* pNewOpj, SvxDrawPage* pNewPage = NULL ) SAL_OVERRIDE;
};

/***********************************************************************
*                                                                      *
***********************************************************************/

class SvxFrameShape : public SvxOle2Shape
{
protected:
    // overide these for special property handling in subcasses. Return true if property is handled
    virtual bool setPropertyValueImpl( const OUString& rName, const SfxItemPropertySimpleEntry* pProperty, const ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException) SAL_OVERRIDE;
    virtual bool getPropertyValueImpl(const OUString& rName, const SfxItemPropertySimpleEntry* pProperty,
        css::uno::Any& rValue)
            throw (css::beans::UnknownPropertyException,
                   css::lang::WrappedTargetException,
                   css::uno::RuntimeException,
                   std::exception) SAL_OVERRIDE;

public:
    SvxFrameShape( SdrObject* pObj ) throw();
    virtual ~SvxFrameShape() throw();

    virtual void SAL_CALL setPropertyValue( const OUString& aPropertyName, const ::com::sun::star::uno::Any& aValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    using SvxUnoTextRangeBase::setPropertyValue;

    virtual void SAL_CALL setPropertyValues( const ::com::sun::star::uno::Sequence< OUString >& aPropertyNames, const ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Any >& aValues ) throw (::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    virtual void Create( SdrObject* pNewOpj, SvxDrawPage* pNewPage = NULL ) throw (css::uno::RuntimeException) SAL_OVERRIDE;
};




class SvxTableShape : public SvxShape
{
protected:
    // overide these for special property handling in subcasses. Return true if property is handled
    virtual bool setPropertyValueImpl( const OUString& rName, const SfxItemPropertySimpleEntry* pProperty, const ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::beans::PropertyVetoException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException) SAL_OVERRIDE;
    virtual bool getPropertyValueImpl( const OUString& rName, const SfxItemPropertySimpleEntry* pProperty, ::com::sun::star::uno::Any& rValue ) throw(::com::sun::star::beans::UnknownPropertyException, ::com::sun::star::lang::WrappedTargetException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    virtual void lock() SAL_OVERRIDE;
    virtual void unlock() SAL_OVERRIDE;

public:

    SvxTableShape( SdrObject* pObj ) throw();
    virtual ~SvxTableShape() throw();
};

SvxUnoPropertyMapProvider& getSvxMapProvider();

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
