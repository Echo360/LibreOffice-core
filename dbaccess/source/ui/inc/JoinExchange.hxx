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
#ifndef INCLUDED_DBACCESS_SOURCE_UI_INC_JOINEXCHANGE_HXX
#define INCLUDED_DBACCESS_SOURCE_UI_INC_JOINEXCHANGE_HXX

#include "dbexchange.hxx"
#include "TableWindowListBox.hxx"

#include <svtools/transfer.hxx>
#include <com/sun/star/lang/XUnoTunnel.hpp>
#include <cppuhelper/implbase1.hxx>

namespace dbaui
{
    // OJoinExchObj: Additional data to create Joins in the JoinShell

    typedef ::cppu::ImplHelper1< ::com::sun::star::lang::XUnoTunnel > OJoinExchObj_Base;
    class OJoinExchObj : public TransferableHelper, public OJoinExchObj_Base
    {
        static OUString         m_sJoinFormat;
        bool                m_bFirstEntry;

    protected:
        OJoinExchangeData           m_jxdSourceDescription;
        IDragTransferableListener*  m_pDragListener;

        virtual ~OJoinExchObj();

    public:
        OJoinExchObj(const OJoinExchangeData& jxdSource,bool _bFirstEntry=false);


        // XInterface
        virtual ::com::sun::star::uno::Any SAL_CALL queryInterface( const ::com::sun::star::uno::Type& aType ) throw(::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
        virtual void SAL_CALL acquire(  ) throw() SAL_OVERRIDE;
        virtual void SAL_CALL release(  ) throw() SAL_OVERRIDE;

        // XUnoTunnel
        virtual sal_Int64 SAL_CALL getSomething( const ::com::sun::star::uno::Sequence< sal_Int8 >& _rIdentifier ) throw(::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

        void StartDrag( Window* pWindow, sal_Int8 nDragSourceActions, IDragTransferableListener* _pListener );

        static OJoinExchangeData    GetSourceDescription(const ::com::sun::star::uno::Reference< ::com::sun::star::datatransfer::XTransferable >& _rxObject);
        static bool             isFormatAvailable( const DataFlavorExVector& _rFormats ,SotFormatStringId _nSlotID=SOT_FORMATSTR_ID_SBA_JOIN);

    protected:
        virtual void                AddSupportedFormats() SAL_OVERRIDE;
        virtual bool GetData( const css::datatransfer::DataFlavor& rFlavor, const OUString& rDestDoc ) SAL_OVERRIDE;
        virtual void                DragFinished( sal_Int8 nDropAction ) SAL_OVERRIDE;

        static ::com::sun::star::uno::Sequence< sal_Int8 > getUnoTunnelImplementationId();

    private:
        using TransferableHelper::StartDrag;
    };
}
#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
