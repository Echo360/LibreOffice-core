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

#ifndef INCLUDED_DESKTOP_INC_APP_HXX
#define INCLUDED_DESKTOP_INC_APP_HXX

#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <vcl/svapp.hxx>
#include <vcl/timer.hxx>
#include <tools/resmgr.hxx>
#include <unotools/bootstrap.hxx>
#include <com/sun/star/task/XStatusIndicator.hpp>
#include <com/sun/star/uno/Reference.h>
#include <osl/mutex.hxx>

namespace com { namespace sun { namespace star { namespace uno {
    class XComponentContext;
} } } }

namespace desktop
{

/*--------------------------------------------------------------------
    Description:    Application-class
 --------------------------------------------------------------------*/
class CommandLineArgs;
class Lockfile;
struct ConvertData;
class Desktop : public Application
{
    int doShutdown();

    public:
        enum BootstrapError
        {
            BE_OK,
            BE_UNO_SERVICEMANAGER,
            BE_UNO_SERVICE_CONFIG_MISSING,
            BE_PATHINFO_MISSING,
            BE_USERINSTALL_FAILED,
            BE_LANGUAGE_MISSING,
            BE_USERINSTALL_NOTENOUGHDISKSPACE,
            BE_USERINSTALL_NOWRITEACCESS,
            BE_OFFICECONFIG_BROKEN
        };
        enum BootstrapStatus
        {
            BS_OK,
            BS_TERMINATE
        };

                                Desktop();
                                virtual ~Desktop();
        virtual int             Main( ) SAL_OVERRIDE;
        virtual void            Init() SAL_OVERRIDE;
        virtual void            InitFinished() SAL_OVERRIDE;
        virtual void            DeInit() SAL_OVERRIDE;
        virtual bool        QueryExit() SAL_OVERRIDE;
        virtual sal_uInt16      Exception(sal_uInt16 nError) SAL_OVERRIDE;
        virtual void            OverrideSystemSettings( AllSettings& rSettings ) SAL_OVERRIDE;
        virtual void            AppEvent( const ApplicationEvent& rAppEvent ) SAL_OVERRIDE;

        DECL_LINK( OpenClients_Impl, void* );

        static void             OpenClients();
        static void             OpenDefault();

        DECL_LINK( EnableAcceptors_Impl, void*);

        static void             HandleAppEvent( const ApplicationEvent& rAppEvent );
        static ResMgr*          GetDesktopResManager();
        static CommandLineArgs& GetCommandLineArgs();

        void                    HandleBootstrapErrors(
                                    BootstrapError nError, OUString const & aMessage );
        void                    SetBootstrapError(
                                    BootstrapError nError, OUString const & aMessage )
        {
            if ( m_aBootstrapError == BE_OK )
            {
                m_aBootstrapError = nError;
                m_aBootstrapErrorMessage = aMessage;
            }
        }

        void                    SetBootstrapStatus( BootstrapStatus nStatus )
        {
            m_aBootstrapStatus = nStatus;
        }
        BootstrapStatus          GetBootstrapStatus() const
        {
            return m_aBootstrapStatus;
        }

        static bool            isCrashReporterEnabled();

        // first-start (ever) related methods
        static bool             CheckExtensionDependencies();

        void                    SynchronizeExtensionRepositories();
        void                    SetSplashScreenText( const OUString& rText );
        void                    SetSplashScreenProgress( sal_Int32 );

        // Bootstrap methods
        static void             InitApplicationServiceManager();
            // throws an exception upon failure

    private:
        void                    RegisterServices(
                                    css::uno::Reference< css::uno::XComponentContext > const & context);
        void                    DeregisterServices();

        void                    CreateTemporaryDirectory();
        void                    RemoveTemporaryDirectory();

        bool                    InitializeInstallation( const OUString& rAppFilename );
        bool                    InitializeConfiguration();
        void                    FlushConfiguration();
        bool                    InitializeQuickstartMode( const css::uno::Reference< css::uno::XComponentContext >& rxContext );

        void                    HandleBootstrapPathErrors( ::utl::Bootstrap::Status, const OUString& aMsg );
        void                    StartSetup( const OUString& aParameters );

        // Create a error message depending on bootstrap failure code and an optional file url
        OUString                CreateErrorMsgString( utl::Bootstrap::FailureCode nFailureCode,
                                                      const OUString& aFileURL );

        static void             PreloadModuleData( const CommandLineArgs& );
        static void             PreloadConfigurationData();

        css::uno::Reference<css::task::XStatusIndicator> m_rSplashScreen;
        void                    OpenSplashScreen();
        void                    CloseSplashScreen();

        void                    EnableOleAutomation();
                                DECL_LINK( ImplInitFilterHdl, ConvertData* );
        DECL_LINK( AsyncInitFirstRun, void* );
        /** checks if the office is run the first time
            <p>If so, <method>DoFirstRunInitializations</method> is called (asynchronously and delayed) and the
            respective flag in the configuration is reset.</p>
        */
        void                    CheckFirstRun( );

        /// does initializations which are necessary for the first run of the office
        void                    DoFirstRunInitializations();

        static void             ShowBackingComponent(Desktop * progress);

        static bool             SaveTasks();

        static bool             isUIOnSessionShutdownAllowed();

        // on-demand acceptors
        static void             createAcceptor(const OUString& aDescription);
        static void             enableAcceptors();
        static void             destroyAcceptor(const OUString& aDescription);

        bool                    m_bCleanedExtensionCache;
        bool                    m_bServicesRegistered;
        BootstrapError          m_aBootstrapError;
        OUString                m_aBootstrapErrorMessage;
        BootstrapStatus         m_aBootstrapStatus;

        boost::scoped_ptr<Lockfile> m_xLockfile;
        Timer                   m_firstRunTimer;

        static ResMgr*          pResMgr;
};

OUString GetURL_Impl(
    const OUString& rName, boost::optional< OUString > const & cwdUrl );

OUString ReplaceStringHookProc(const OUString& rStr);

}

#endif // INCLUDED_DESKTOP_INC_APP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
