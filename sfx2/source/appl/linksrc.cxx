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


#include <sfx2/linksrc.hxx>
#include <sfx2/lnkbase.hxx>
#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Sequence.hxx>

#include <vcl/timer.hxx>
#include <vector>
#include <algorithm>


using namespace ::com::sun::star::uno;

namespace sfx2
{

TYPEINIT0( SvLinkSource )

class SvLinkSourceTimer : public Timer
{
    SvLinkSource *  pOwner;
    virtual void    Timeout() SAL_OVERRIDE;
public:
            SvLinkSourceTimer( SvLinkSource * pOwn );
};

SvLinkSourceTimer::SvLinkSourceTimer( SvLinkSource * pOwn )
    : pOwner( pOwn )
{
}

void SvLinkSourceTimer::Timeout()
{
    // Secure against being destroyed in Handler
    SvLinkSourceRef aAdv( pOwner );
    pOwner->SendDataChanged();
}

static void StartTimer( SvLinkSourceTimer ** ppTimer, SvLinkSource * pOwner,
                        sal_uIntPtr nTimeout )
{
    if( !*ppTimer )
    {
        *ppTimer = new SvLinkSourceTimer( pOwner );
        (*ppTimer)->SetTimeout( nTimeout );
        (*ppTimer)->Start();
    }
}


struct SvLinkSource_Entry_Impl
{
    SvBaseLinkRef       xSink;
    OUString            aDataMimeType;
    sal_uInt16          nAdviseModes;
    bool            bIsDataSink;

    SvLinkSource_Entry_Impl( SvBaseLink* pLink, const OUString& rMimeType,
                                sal_uInt16 nAdvMode )
        : xSink( pLink ), aDataMimeType( rMimeType ),
            nAdviseModes( nAdvMode ), bIsDataSink( true )
    {}

    SvLinkSource_Entry_Impl( SvBaseLink* pLink )
        : xSink( pLink ), nAdviseModes( 0 ), bIsDataSink( false )
    {}

    ~SvLinkSource_Entry_Impl();
};

SvLinkSource_Entry_Impl::~SvLinkSource_Entry_Impl()
{
}

class SvLinkSource_Array_Impl : public std::vector<SvLinkSource_Entry_Impl*>
{
public:
    void DeleteAndDestroy(SvLinkSource_Entry_Impl* p)
    {
        iterator it = std::find(begin(), end(), p);
        if (it != end())
        {
            erase(it);
            delete p;
        }
    }

    ~SvLinkSource_Array_Impl()
    {
        for(const_iterator it = begin(); it != end(); ++it)
            delete *it;
    }
};

class SvLinkSource_EntryIter_Impl
{
    SvLinkSource_Array_Impl aArr;
    const SvLinkSource_Array_Impl& rOrigArr;
    sal_uInt16 nPos;
public:
    SvLinkSource_EntryIter_Impl( const SvLinkSource_Array_Impl& rArr );
    ~SvLinkSource_EntryIter_Impl();
    SvLinkSource_Entry_Impl* Curr()
                            { return nPos < aArr.size() ? aArr[ nPos ] : 0; }
    SvLinkSource_Entry_Impl* Next();
    bool IsValidCurrValue( SvLinkSource_Entry_Impl* pEntry );
};

SvLinkSource_EntryIter_Impl::SvLinkSource_EntryIter_Impl(
        const SvLinkSource_Array_Impl& rArr )
    : aArr( rArr ), rOrigArr( rArr ), nPos( 0 )
{
}
SvLinkSource_EntryIter_Impl::~SvLinkSource_EntryIter_Impl()
{
    aArr.clear();
}

bool SvLinkSource_EntryIter_Impl::IsValidCurrValue( SvLinkSource_Entry_Impl* pEntry )
{
    return ( nPos < aArr.size() && aArr[nPos] == pEntry
       && std::find( rOrigArr.begin(), rOrigArr.end(), pEntry ) != rOrigArr.end() );
}

SvLinkSource_Entry_Impl* SvLinkSource_EntryIter_Impl::Next()
{
    SvLinkSource_Entry_Impl* pRet = 0;
    if( nPos + 1 < (sal_uInt16)aArr.size() )
    {
        ++nPos;
        if( rOrigArr.size() == aArr.size() &&
            rOrigArr[ nPos ] == aArr[ nPos ] )
            pRet = aArr[ nPos ];
        else
        {
            // then we must search the current (or the next) in the orig
            do {
                pRet = aArr[ nPos ];
                if( std::find(rOrigArr.begin(), rOrigArr.end(), pRet ) != rOrigArr.end() )
                    break;
                pRet = 0;
                ++nPos;
            } while( nPos < aArr.size() );

            if( nPos >= aArr.size() )
                pRet = 0;
        }
    }
    return pRet;
}

struct SvLinkSource_Impl
{
    SvLinkSource_Array_Impl aArr;
    OUString                aDataMimeType;
    SvLinkSourceTimer *     pTimer;
    sal_uIntPtr             nTimeout;
    css::uno::Reference<css::io::XInputStream>
                           m_xInputStreamToLoadFrom;
    bool               m_bIsReadOnly;

    SvLinkSource_Impl()
        : pTimer(0)
        , nTimeout(3000)
        , m_bIsReadOnly(false)
    {
    }
    ~SvLinkSource_Impl();
};

SvLinkSource_Impl::~SvLinkSource_Impl()
{
    delete pTimer;
}

SvLinkSource::SvLinkSource()
     : pImpl( new SvLinkSource_Impl )
{
}

SvLinkSource::~SvLinkSource()
{
    delete pImpl;
}


SvLinkSource::StreamToLoadFrom SvLinkSource::getStreamToLoadFrom()
{
    return StreamToLoadFrom(
        pImpl->m_xInputStreamToLoadFrom,
        pImpl->m_bIsReadOnly);
}

void SvLinkSource::setStreamToLoadFrom(const com::sun::star::uno::Reference<com::sun::star::io::XInputStream>& xInputStream, bool bIsReadOnly )
{
    pImpl->m_xInputStreamToLoadFrom = xInputStream;
    pImpl->m_bIsReadOnly = bIsReadOnly;
}

// #i88291#
void SvLinkSource::clearStreamToLoadFrom()
{
    pImpl->m_xInputStreamToLoadFrom.clear();
}

void  SvLinkSource::Closed()
{
    SvLinkSource_EntryIter_Impl aIter( pImpl->aArr );
    for( SvLinkSource_Entry_Impl* p = aIter.Curr(); p; p = aIter.Next() )
        if( !p->bIsDataSink )
            p->xSink->Closed();
}

sal_uIntPtr SvLinkSource::GetUpdateTimeout() const
{
    return pImpl->nTimeout;
}

void SvLinkSource::SetUpdateTimeout( sal_uIntPtr nTimeout )
{
    pImpl->nTimeout = nTimeout;
    if( pImpl->pTimer )
        pImpl->pTimer->SetTimeout( nTimeout );
}

void SvLinkSource::SendDataChanged()
{
    SvLinkSource_EntryIter_Impl aIter( pImpl->aArr );
    for( SvLinkSource_Entry_Impl* p = aIter.Curr(); p; p = aIter.Next() )
    {
        if( p->bIsDataSink )
        {
            OUString sDataMimeType( pImpl->aDataMimeType );
            if( sDataMimeType.isEmpty() )
                sDataMimeType = p->aDataMimeType;

            Any aVal;
            if( ( p->nAdviseModes & ADVISEMODE_NODATA ) ||
                GetData( aVal, sDataMimeType, true ) )
            {
                p->xSink->DataChanged( sDataMimeType, aVal );

                if ( !aIter.IsValidCurrValue( p ) )
                    continue;

                if( p->nAdviseModes & ADVISEMODE_ONLYONCE )
                {
                    pImpl->aArr.DeleteAndDestroy( p );
                }

            }
        }
    }
    if( pImpl->pTimer )
    {
        delete pImpl->pTimer;
        pImpl->pTimer = NULL;
    }
    pImpl->aDataMimeType = "";
}

void SvLinkSource::NotifyDataChanged()
{
    if( pImpl->nTimeout )
        StartTimer( &pImpl->pTimer, this, pImpl->nTimeout ); // New timeout
    else
    {
        SvLinkSource_EntryIter_Impl aIter( pImpl->aArr );
        for( SvLinkSource_Entry_Impl* p = aIter.Curr(); p; p = aIter.Next() )
            if( p->bIsDataSink )
            {
                Any aVal;
                if( ( p->nAdviseModes & ADVISEMODE_NODATA ) ||
                    GetData( aVal, p->aDataMimeType, true ) )
                {
                    p->xSink->DataChanged( p->aDataMimeType, aVal );

                    if ( !aIter.IsValidCurrValue( p ) )
                        continue;

                    if( p->nAdviseModes & ADVISEMODE_ONLYONCE )
                    {
                        pImpl->aArr.DeleteAndDestroy( p );
                    }
                }
            }

        if( pImpl->pTimer )
        {
            delete pImpl->pTimer;
            pImpl->pTimer = NULL;
        }
    }
}

// notify the sink, the mime type is not
// a selection criterion
void SvLinkSource::DataChanged( const OUString & rMimeType,
                                const ::com::sun::star::uno::Any & rVal )
{
    if( pImpl->nTimeout && !rVal.hasValue() )
    {   // only when no data was included
        // fire all data to the sink, independent of the requested format
        pImpl->aDataMimeType = rMimeType;
        StartTimer( &pImpl->pTimer, this, pImpl->nTimeout ); // New timeout
    }
    else
    {
        SvLinkSource_EntryIter_Impl aIter( pImpl->aArr );
        for( SvLinkSource_Entry_Impl* p = aIter.Curr(); p; p = aIter.Next() )
        {
            if( p->bIsDataSink )
            {
                p->xSink->DataChanged( rMimeType, rVal );

                if ( !aIter.IsValidCurrValue( p ) )
                    continue;

                if( p->nAdviseModes & ADVISEMODE_ONLYONCE )
                {
                    pImpl->aArr.DeleteAndDestroy( p );
                }
            }
        }

        if( pImpl->pTimer )
        {
            delete pImpl->pTimer;
            pImpl->pTimer = NULL;
        }
    }
}


// only one link is correct
void SvLinkSource::AddDataAdvise( SvBaseLink * pLink, const OUString& rMimeType,
                                    sal_uInt16 nAdviseModes )
{
    SvLinkSource_Entry_Impl* pNew = new SvLinkSource_Entry_Impl(
                    pLink, rMimeType, nAdviseModes );
    pImpl->aArr.push_back( pNew );
}

void SvLinkSource::RemoveAllDataAdvise( SvBaseLink * pLink )
{
    SvLinkSource_EntryIter_Impl aIter( pImpl->aArr );
    for( SvLinkSource_Entry_Impl* p = aIter.Curr(); p; p = aIter.Next() )
        if( p->bIsDataSink && &p->xSink == pLink )
        {
            pImpl->aArr.DeleteAndDestroy( p );
        }
}

// only one link is correct
void SvLinkSource::AddConnectAdvise( SvBaseLink * pLink )
{
    SvLinkSource_Entry_Impl* pNew = new SvLinkSource_Entry_Impl( pLink );
    pImpl->aArr.push_back( pNew );
}

void SvLinkSource::RemoveConnectAdvise( SvBaseLink * pLink )
{
    SvLinkSource_EntryIter_Impl aIter( pImpl->aArr );
    for( SvLinkSource_Entry_Impl* p = aIter.Curr(); p; p = aIter.Next() )
        if( !p->bIsDataSink && &p->xSink == pLink )
        {
            pImpl->aArr.DeleteAndDestroy( p );
        }
}

bool SvLinkSource::HasDataLinks( const SvBaseLink* pLink ) const
{
    bool bRet = false;
    const SvLinkSource_Entry_Impl* p;
    for( sal_uInt16 n = 0, nEnd = pImpl->aArr.size(); n < nEnd; ++n )
        if( ( p = pImpl->aArr[ n ] )->bIsDataSink &&
            ( !pLink || &p->xSink == pLink ) )
        {
            bRet = true;
            break;
        }
    return bRet;
}

// sal_True => waitinmg for data
bool SvLinkSource::IsPending() const
{
    return false;
}

// sal_True => data complete loaded
bool SvLinkSource::IsDataComplete() const
{
    return true;
}

bool SvLinkSource::Connect( SvBaseLink* )
{
    return true;
}

bool SvLinkSource::GetData( ::com::sun::star::uno::Any &, const OUString &, bool )
{
    return false;
}

void SvLinkSource::Edit( Window *, SvBaseLink *, const Link& )
{
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
