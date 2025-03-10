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


#include <svl/intitem.hxx>
#include <com/sun/star/uno/Any.hxx>
#include <tools/bigint.hxx>
#include <tools/stream.hxx>
#include <svl/metitem.hxx>


//  class SfxByteItem


TYPEINIT1_AUTOFACTORY(SfxByteItem, CntByteItem);

// virtual
SfxPoolItem * SfxByteItem::Create(SvStream & rStream, sal_uInt16) const
{
    short nValue = 0;
    rStream.ReadInt16( nValue );
    return new SfxByteItem(Which(), sal_uInt8(nValue));
}

TYPEINIT1_AUTOFACTORY(SfxInt16Item, SfxPoolItem);

SfxInt16Item::SfxInt16Item(sal_uInt16 which, SvStream & rStream):
    SfxPoolItem(which)
{
    short nTheValue = 0;
    rStream.ReadInt16( nTheValue );
    m_nValue = nTheValue;
}

// virtual
bool SfxInt16Item::operator ==(const SfxPoolItem & rItem) const
{
    DBG_ASSERT(SfxPoolItem::operator ==(rItem), "unequal type");
    return m_nValue == (static_cast< const SfxInt16Item * >(&rItem))->
                        m_nValue;
}

// virtual
int SfxInt16Item::Compare(const SfxPoolItem & rWith) const
{
    DBG_ASSERT(SfxPoolItem::operator ==(rWith), "unequal type");
    return (static_cast< const SfxInt16Item * >(&rWith))->m_nValue
             < m_nValue ?
            -1 :
           (static_cast< const SfxInt16Item * >(&rWith))->m_nValue
             == m_nValue ?
            0 : 1;
}

// virtual
bool SfxInt16Item::GetPresentation(SfxItemPresentation,
                                                  SfxMapUnit, SfxMapUnit,
                                                  OUString & rText,
                                                  const IntlWrapper *) const
{
    rText = OUString::number(m_nValue);
    return true;
}


// virtual
bool SfxInt16Item::QueryValue(com::sun::star::uno::Any& rVal, sal_uInt8) const
{
    sal_Int16 nValue = m_nValue;
    rVal <<= nValue;
    return true;
}

// virtual
bool SfxInt16Item::PutValue(const com::sun::star::uno::Any& rVal, sal_uInt8 )
{
    sal_Int16 nValue = sal_Int16();
    if (rVal >>= nValue)
    {
        m_nValue = nValue;
        return true;
    }

    OSL_FAIL( "SfxInt16Item::PutValue - Wrong type!" );
    return false;
}

// virtual
SfxPoolItem * SfxInt16Item::Create(SvStream & rStream, sal_uInt16) const
{
    return new SfxInt16Item(Which(), rStream);
}

// virtual
SvStream & SfxInt16Item::Store(SvStream & rStream, sal_uInt16) const
{
    rStream.WriteInt16( short(m_nValue) );
    return rStream;
}

SfxPoolItem * SfxInt16Item::Clone(SfxItemPool *) const
{
    return new SfxInt16Item(*this);
}

sal_Int16 SfxInt16Item::GetMin() const
{
    return -32768;
}

sal_Int16 SfxInt16Item::GetMax() const
{
    return 32767;
}

SfxFieldUnit SfxInt16Item::GetUnit() const
{
    return SFX_FUNIT_NONE;
}


//  class SfxUInt16Item


TYPEINIT1_AUTOFACTORY(SfxUInt16Item, CntUInt16Item);



//  class SfxInt32Item


TYPEINIT1_AUTOFACTORY(SfxInt32Item, CntInt32Item);



//  class SfxUInt32Item


TYPEINIT1_AUTOFACTORY(SfxUInt32Item, CntUInt32Item);

TYPEINIT1_AUTOFACTORY(SfxMetricItem, SfxInt32Item);

SfxMetricItem::SfxMetricItem(sal_uInt16 which, sal_uInt32 nValue):
    SfxInt32Item(which, nValue)
{
}

SfxMetricItem::SfxMetricItem(sal_uInt16 which, SvStream & rStream):
    SfxInt32Item(which, rStream)
{
}

SfxMetricItem::SfxMetricItem(const SfxMetricItem & rItem):
    SfxInt32Item(rItem)
{
}

// virtual
bool SfxMetricItem::ScaleMetrics(long nMult, long nDiv)
{
    BigInt aTheValue(GetValue());
    aTheValue *= nMult;
    aTheValue += nDiv / 2;
    aTheValue /= nDiv;
    SetValue(aTheValue);
    return true;
}

// virtual
bool SfxMetricItem::HasMetrics() const
{
    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
