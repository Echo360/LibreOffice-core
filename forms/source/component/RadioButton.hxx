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

#ifndef INCLUDED_FORMS_SOURCE_COMPONENT_RADIOBUTTON_HXX
#define INCLUDED_FORMS_SOURCE_COMPONENT_RADIOBUTTON_HXX

#include "refvaluecomponent.hxx"


namespace frm
{

class ORadioButtonModel     :public OReferenceValueComponent
{
public:
    DECLARE_DEFAULT_LEAF_XTOR( ORadioButtonModel );

    // XServiceInfo
    IMPLEMENTATION_NAME(ORadioButtonModel);
    virtual StringSequence SAL_CALL getSupportedServiceNames() throw(::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    // OPropertySetHelper
    virtual void SAL_CALL setFastPropertyValue_NoBroadcast( sal_Int32 nHandle, const ::com::sun::star::uno::Any& rValue )
                throw (::com::sun::star::uno::Exception, std::exception) SAL_OVERRIDE;

    // XPersistObject
    virtual OUString SAL_CALL    getServiceName() throw(::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual void SAL_CALL
        write(const ::com::sun::star::uno::Reference< ::com::sun::star::io::XObjectOutputStream>& _rxOutStream) throw(::com::sun::star::io::IOException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    virtual void SAL_CALL
        read(const ::com::sun::star::uno::Reference< ::com::sun::star::io::XObjectInputStream>& _rxInStream) throw(::com::sun::star::io::IOException, ::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    // OPropertyChangeListener
    virtual void _propertyChanged(const ::com::sun::star::beans::PropertyChangeEvent& evt) throw(::com::sun::star::uno::RuntimeException) SAL_OVERRIDE;

    // OControlModel's property handling
    virtual void describeFixedProperties(
        ::com::sun::star::uno::Sequence< ::com::sun::star::beans::Property >& /* [out] */ _rProps
    ) const SAL_OVERRIDE;

protected:
    // OBoundControlModel overridables
    virtual ::com::sun::star::uno::Any
                            translateDbColumnToControlValue( ) SAL_OVERRIDE;
    virtual bool            commitControlValueToDbColumn( bool _bPostReset ) SAL_OVERRIDE;
    virtual ::com::sun::star::uno::Any
                            translateExternalValueToControlValue( const ::com::sun::star::uno::Any& _rExternalValue ) const SAL_OVERRIDE;

protected:
    void SetSiblingPropsTo(const OUString& rPropName, const ::com::sun::star::uno::Any& rValue);

    virtual css::uno::Reference< css::util::XCloneable > SAL_CALL createClone(  ) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

private:
    /** sets the given value as new State at the aggregate
        @precond
            our mutex is acquired exactly once
    */
    void    setNewAggregateState( const ::com::sun::star::uno::Any& _rValue );

    void setControlSource();
};

class ORadioButtonControl: public OBoundControl
{
public:
    ORadioButtonControl(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XComponentContext>& _rxFactory);

    // XServiceInfo
    IMPLEMENTATION_NAME(ORadioButtonControl);
    virtual StringSequence SAL_CALL getSupportedServiceNames() throw(::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

protected:
    // XControl
    virtual void SAL_CALL createPeer(const ::com::sun::star::uno::Reference<starawt::XToolkit>& Toolkit, const ::com::sun::star::uno::Reference<starawt::XWindowPeer>& Parent) throw (::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
};


}


#endif // INCLUDED_FORMS_SOURCE_COMPONENT_RADIOBUTTON_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
