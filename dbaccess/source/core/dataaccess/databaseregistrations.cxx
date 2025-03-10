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

#include <com/sun/star/sdb/XDatabaseRegistrations.hpp>

#include <cppuhelper/basemutex.hxx>
#include <cppuhelper/interfacecontainer.hxx>
#include <cppuhelper/implbase1.hxx>
#include <rtl/ustrbuf.hxx>
#include <unotools/pathoptions.hxx>
#include <tools/urlobj.hxx>
#include <unotools/confignode.hxx>

#include <databaseregistrations.hxx>

namespace dbaccess
{
    using ::com::sun::star::uno::Reference;
    using ::com::sun::star::uno::XInterface;
    using ::com::sun::star::uno::UNO_QUERY;
    using ::com::sun::star::uno::UNO_QUERY_THROW;
    using ::com::sun::star::uno::UNO_SET_THROW;
    using ::com::sun::star::uno::Exception;
    using ::com::sun::star::uno::RuntimeException;
    using ::com::sun::star::uno::Any;
    using ::com::sun::star::uno::makeAny;
    using ::com::sun::star::uno::Sequence;
    using ::com::sun::star::uno::Type;
    using ::com::sun::star::uno::XComponentContext;
    using ::com::sun::star::container::NoSuchElementException;
    using ::com::sun::star::lang::IllegalArgumentException;
    using ::com::sun::star::lang::IllegalAccessException;
    using ::com::sun::star::container::ElementExistException;
    using ::com::sun::star::sdb::XDatabaseRegistrations;
    using ::com::sun::star::sdb::XDatabaseRegistrationsListener;
    using ::com::sun::star::sdb::DatabaseRegistrationEvent;
    using ::com::sun::star::uno::XAggregation;

    static const OUString& getConfigurationRootPath()
    {
        static OUString s_sNodeName("org.openoffice.Office.DataAccess/RegisteredNames");
        return s_sNodeName;
    }

    const OUString& getLocationNodeName()
    {
        static OUString s_sNodeName("Location");
        return s_sNodeName;
    }

    const OUString& getNameNodeName()
    {
        static OUString s_sNodeName("Name");
        return s_sNodeName;
    }

    // DatabaseRegistrations - declaration
    typedef ::cppu::WeakAggImplHelper1  <   XDatabaseRegistrations
                                        >   DatabaseRegistrations_Base;
    class DatabaseRegistrations :public ::cppu::BaseMutex
                                ,public DatabaseRegistrations_Base
    {
    public:
        DatabaseRegistrations( const Reference<XComponentContext>& _rxContext );

    protected:
        virtual ~DatabaseRegistrations();

    public:
        virtual sal_Bool SAL_CALL hasRegisteredDatabase( const OUString& _Name ) throw (IllegalArgumentException, RuntimeException, std::exception) SAL_OVERRIDE;
        virtual Sequence< OUString > SAL_CALL getRegistrationNames() throw (RuntimeException, std::exception) SAL_OVERRIDE;
        virtual OUString SAL_CALL getDatabaseLocation( const OUString& _Name ) throw (IllegalArgumentException, NoSuchElementException, RuntimeException, std::exception) SAL_OVERRIDE;
        virtual void SAL_CALL registerDatabaseLocation( const OUString& _Name, const OUString& _Location ) throw (IllegalArgumentException, ElementExistException, RuntimeException, std::exception) SAL_OVERRIDE;
        virtual void SAL_CALL revokeDatabaseLocation( const OUString& _Name ) throw (IllegalArgumentException, NoSuchElementException, IllegalAccessException, RuntimeException, std::exception) SAL_OVERRIDE;
        virtual void SAL_CALL changeDatabaseLocation( const OUString& Name, const OUString& NewLocation ) throw (IllegalArgumentException, NoSuchElementException, IllegalAccessException, RuntimeException, std::exception) SAL_OVERRIDE;
        virtual sal_Bool SAL_CALL isDatabaseRegistrationReadOnly( const OUString& _Name ) throw (IllegalArgumentException, NoSuchElementException, RuntimeException, std::exception) SAL_OVERRIDE;
        virtual void SAL_CALL addDatabaseRegistrationsListener( const Reference< XDatabaseRegistrationsListener >& Listener ) throw (RuntimeException, std::exception) SAL_OVERRIDE;
        virtual void SAL_CALL removeDatabaseRegistrationsListener( const Reference< XDatabaseRegistrationsListener >& Listener ) throw (RuntimeException, std::exception) SAL_OVERRIDE;

    private:
        void
                impl_checkValidName_common(const OUString& _rName);
        ::utl::OConfigurationNode
                impl_checkValidName_throw_must_exist(const OUString& _rName);
        ::utl::OConfigurationNode
                impl_checkValidName_throw_must_not_exist(const OUString& _rName);

        void    impl_checkValidLocation_throw( const OUString& _rLocation );

        /** retrieves the configuration node whose "Name" sub node has the given value

            Since we separated the name of the registration node from the "Name" value of the registration, we cannot
            simply do a "getByName" (equivalent) when we want to retrieve the node for a given registration name.
            Instead, we must search all nodes.

            If a node with the given display name does not exist, then a NoSuchElementException is thrown.

            If no exception is thrown, then a valid node is returned: If the node existed it is returned.
        */
        ::utl::OConfigurationNode
                impl_getNodeForName_throw_must_exist(const OUString& _rName);

        /** retrieves the configuration node whose "Name" sub node has the given value

            Since we separated the name of the registration node from the "Name" value of the registration, we cannot
            simply do a "getByName" (equivalent) when we want to retrieve the node for a given registration name.
            Instead, we must search all nodes.

            If a node with the given name already exists, then a ElementExistException is thrown.

            If no exception is thrown, then a valid node is returned: If the node did not yet exist a new node is created,
            in this case the root node is not yet committed.
        */
        ::utl::OConfigurationNode
                impl_getNodeForName_throw_must_not_exist(const OUString& _rName);


        ::utl::OConfigurationNode
                impl_getNodeForName_nothrow(const OUString& _rName);

    private:
        Reference<XComponentContext>        m_aContext;
        ::utl::OConfigurationTreeRoot       m_aConfigurationRoot;
        ::cppu::OInterfaceContainerHelper   m_aRegistrationListeners;
    };

    // DatabaseRegistrations - implementation
    DatabaseRegistrations::DatabaseRegistrations( const Reference<XComponentContext> & _rxContext )
        :m_aContext( _rxContext )
        ,m_aConfigurationRoot()
        ,m_aRegistrationListeners( m_aMutex )
    {
        m_aConfigurationRoot = ::utl::OConfigurationTreeRoot::createWithComponentContext(
            m_aContext, getConfigurationRootPath(), -1, ::utl::OConfigurationTreeRoot::CM_UPDATABLE );
    }

    DatabaseRegistrations::~DatabaseRegistrations()
    {
    }

    ::utl::OConfigurationNode DatabaseRegistrations::impl_getNodeForName_nothrow( const OUString& _rName )
    {
        Sequence< OUString > aNames( m_aConfigurationRoot.getNodeNames() );
        for (   const OUString* pName = aNames.getConstArray();
                pName != aNames.getConstArray() + aNames.getLength();
                ++pName
            )
        {
            ::utl::OConfigurationNode aNodeForName = m_aConfigurationRoot.openNode( *pName );

            OUString sTestName;
            OSL_VERIFY( aNodeForName.getNodeValue( getNameNodeName() ) >>= sTestName );
            if ( sTestName == _rName )
                return aNodeForName;
        }
        return ::utl::OConfigurationNode();
    }

    ::utl::OConfigurationNode DatabaseRegistrations::impl_getNodeForName_throw_must_exist(const OUString& _rName)
    {
        ::utl::OConfigurationNode aNodeForName( impl_getNodeForName_nothrow( _rName ) );

        if (!aNodeForName.isValid())
        {
            throw NoSuchElementException( _rName, *this );
        }

        return aNodeForName;
    }

    ::utl::OConfigurationNode DatabaseRegistrations::impl_getNodeForName_throw_must_not_exist(const OUString& _rName)
    {
        ::utl::OConfigurationNode aNodeForName( impl_getNodeForName_nothrow( _rName ) );

        if (aNodeForName.isValid())
            throw ElementExistException( _rName, *this );

        OUString sNewNodeName;
        {
            OUStringBuffer aNewNodeName;
            aNewNodeName.appendAscii( "org.openoffice." );
            aNewNodeName.append( _rName );

            // make unique
            OUStringBuffer aReset( aNewNodeName );
            sNewNodeName = aNewNodeName.makeStringAndClear();
            sal_Int32 i=2;
            while ( m_aConfigurationRoot.hasByName( sNewNodeName ) )
            {
                aNewNodeName = aReset;
                aNewNodeName.appendAscii( " " );
                aNewNodeName.append( i );
                sNewNodeName = aNewNodeName.makeStringAndClear();
            }
        }

        ::utl::OConfigurationNode aNewNode( m_aConfigurationRoot.createNode( sNewNodeName ) );
        aNewNode.setNodeValue( getNameNodeName(), makeAny( _rName ) );
        return aNewNode;
    }

    void DatabaseRegistrations::impl_checkValidName_common(const OUString& _rName)
    {
        if ( !m_aConfigurationRoot.isValid() )
            throw RuntimeException( OUString(), *this );

        if ( _rName.isEmpty() )
            throw IllegalArgumentException( OUString(), *this, 1 );
    }

    ::utl::OConfigurationNode DatabaseRegistrations::impl_checkValidName_throw_must_exist(const OUString& _rName)
    {
        impl_checkValidName_common(_rName);
        return impl_getNodeForName_throw_must_exist(_rName);
    }

    ::utl::OConfigurationNode DatabaseRegistrations::impl_checkValidName_throw_must_not_exist(const OUString& _rName)
    {
        impl_checkValidName_common(_rName);
        return impl_getNodeForName_throw_must_not_exist(_rName);
    }

    void DatabaseRegistrations::impl_checkValidLocation_throw( const OUString& _rLocation )
    {
        if ( _rLocation.isEmpty() )
            throw IllegalArgumentException( OUString(), *this, 2 );

        INetURLObject aURL( _rLocation );
        if ( aURL.GetProtocol() == INET_PROT_NOT_VALID )
            throw IllegalArgumentException( OUString(), *this, 2 );
    }

    sal_Bool SAL_CALL DatabaseRegistrations::hasRegisteredDatabase( const OUString& _Name ) throw (IllegalArgumentException, RuntimeException, std::exception)
    {
        ::osl::MutexGuard aGuard( m_aMutex );
        ::utl::OConfigurationNode aNodeForName = impl_getNodeForName_nothrow( _Name );
        return aNodeForName.isValid();
    }

    Sequence< OUString > SAL_CALL DatabaseRegistrations::getRegistrationNames() throw (RuntimeException, std::exception)
    {
        ::osl::MutexGuard aGuard( m_aMutex );
        if ( !m_aConfigurationRoot.isValid() )
            throw RuntimeException( OUString(), *this );

        Sequence< OUString > aProgrammaticNames( m_aConfigurationRoot.getNodeNames() );
        Sequence< OUString > aDisplayNames( aProgrammaticNames.getLength() );
        OUString* pDisplayName = aDisplayNames.getArray();

        for (   const OUString* pName = aProgrammaticNames.getConstArray();
                pName != aProgrammaticNames.getConstArray() + aProgrammaticNames.getLength();
                ++pName, ++pDisplayName
            )
        {
            ::utl::OConfigurationNode aRegistrationNode = m_aConfigurationRoot.openNode( *pName );
            OSL_VERIFY( aRegistrationNode.getNodeValue( getNameNodeName() ) >>= *pDisplayName );
        }

        return aDisplayNames;
    }

    OUString SAL_CALL DatabaseRegistrations::getDatabaseLocation( const OUString& _Name ) throw (IllegalArgumentException, NoSuchElementException, RuntimeException, std::exception)
    {
        ::osl::MutexGuard aGuard( m_aMutex );

        ::utl::OConfigurationNode aNodeForName = impl_checkValidName_throw_must_exist(_Name);

        OUString sLocation;
        OSL_VERIFY( aNodeForName.getNodeValue( getLocationNodeName() ) >>= sLocation );
        sLocation = SvtPathOptions().SubstituteVariable( sLocation );

        return sLocation;
    }

    void SAL_CALL DatabaseRegistrations::registerDatabaseLocation( const OUString& _Name, const OUString& _Location ) throw (IllegalArgumentException, ElementExistException, RuntimeException, std::exception)
    {
        ::osl::ClearableMutexGuard aGuard( m_aMutex );

        // check
        impl_checkValidLocation_throw( _Location );
        ::utl::OConfigurationNode aDataSourceRegistration = impl_checkValidName_throw_must_not_exist(_Name);

        // register
        aDataSourceRegistration.setNodeValue( getLocationNodeName(), makeAny( _Location ) );
        m_aConfigurationRoot.commit();

        // notify
        DatabaseRegistrationEvent aEvent( *this, _Name, OUString(), _Location );
        aGuard.clear();
        m_aRegistrationListeners.notifyEach( &XDatabaseRegistrationsListener::registeredDatabaseLocation, aEvent );
    }

    void SAL_CALL DatabaseRegistrations::revokeDatabaseLocation( const OUString& _Name ) throw (IllegalArgumentException, NoSuchElementException, IllegalAccessException, RuntimeException, std::exception)
    {
        ::osl::ClearableMutexGuard aGuard( m_aMutex );

        // check
        ::utl::OConfigurationNode aNodeForName = impl_checkValidName_throw_must_exist(_Name);

        // obtain properties for notification
        OUString sLocation;
        OSL_VERIFY( aNodeForName.getNodeValue( getLocationNodeName() ) >>= sLocation );

        // revoke
        if  (   aNodeForName.isReadonly()
            ||  !m_aConfigurationRoot.removeNode( aNodeForName.getLocalName() )
            )
            throw IllegalAccessException( OUString(), *this );

        m_aConfigurationRoot.commit();

        // notify
        DatabaseRegistrationEvent aEvent( *this, _Name, sLocation, OUString() );
        aGuard.clear();
        m_aRegistrationListeners.notifyEach( &XDatabaseRegistrationsListener::revokedDatabaseLocation, aEvent );
    }

    void SAL_CALL DatabaseRegistrations::changeDatabaseLocation( const OUString& _Name, const OUString& _NewLocation ) throw (IllegalArgumentException, NoSuchElementException, IllegalAccessException, RuntimeException, std::exception)
    {
        ::osl::ClearableMutexGuard aGuard( m_aMutex );

        // check
        impl_checkValidLocation_throw( _NewLocation );
        ::utl::OConfigurationNode aDataSourceRegistration = impl_checkValidName_throw_must_exist(_Name);

        if  ( aDataSourceRegistration.isReadonly() )
            throw IllegalAccessException( OUString(), *this );

        // obtain properties for notification
        OUString sOldLocation;
        OSL_VERIFY( aDataSourceRegistration.getNodeValue( getLocationNodeName() ) >>= sOldLocation );

        // change
        aDataSourceRegistration.setNodeValue( getLocationNodeName(), makeAny( _NewLocation ) );
        m_aConfigurationRoot.commit();

        // notify
        DatabaseRegistrationEvent aEvent( *this, _Name, sOldLocation, _NewLocation );
        aGuard.clear();
        m_aRegistrationListeners.notifyEach( &XDatabaseRegistrationsListener::changedDatabaseLocation, aEvent );
    }

    sal_Bool SAL_CALL DatabaseRegistrations::isDatabaseRegistrationReadOnly( const OUString& _Name ) throw (IllegalArgumentException, NoSuchElementException, RuntimeException, std::exception)
    {
        ::osl::MutexGuard aGuard( m_aMutex );
        ::utl::OConfigurationNode aDataSourceRegistration = impl_checkValidName_throw_must_exist(_Name);
        return aDataSourceRegistration.isReadonly();
    }

    void SAL_CALL DatabaseRegistrations::addDatabaseRegistrationsListener( const Reference< XDatabaseRegistrationsListener >& _Listener ) throw (RuntimeException, std::exception)
    {
        if ( _Listener.is() )
            m_aRegistrationListeners.addInterface( _Listener );
    }

    void SAL_CALL DatabaseRegistrations::removeDatabaseRegistrationsListener( const Reference< XDatabaseRegistrationsListener >& _Listener ) throw (RuntimeException, std::exception)
    {
        if ( _Listener.is() )
            m_aRegistrationListeners.removeInterface( _Listener );
    }

    // DatabaseRegistrations - factory
    Reference< XAggregation > createDataSourceRegistrations( const Reference<XComponentContext> & _rxContext )
    {
        return new DatabaseRegistrations( _rxContext );
    }

} // namespace dbaccess

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
