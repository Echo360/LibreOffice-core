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

#include <properties.h>
#include <stdtypes.h>
#include <helper/mischelper.hxx>

#include <com/sun/star/beans/Property.hpp>
#include <com/sun/star/beans/XProperty.hpp>
#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/util/XChangesNotifier.hpp>
#include <com/sun/star/util/PathSubstitution.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/util/XStringSubstitution.hpp>
#include <com/sun/star/util/XChangesListener.hpp>
#include <com/sun/star/util/XPathSettings.hpp>

#include <tools/urlobj.hxx>
#include <rtl/ustrbuf.hxx>

#include <cppuhelper/basemutex.hxx>
#include <cppuhelper/propshlp.hxx>
#include <cppuhelper/compbase3.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <comphelper/sequence.hxx>
#include <comphelper/configurationhelper.hxx>
#include <unotools/configitem.hxx>
#include <unotools/configpaths.hxx>

using namespace framework;

#define CFGPROP_USERPATHS "UserPaths"
#define CFGPROP_WRITEPATH "WritePath"

/*
    0 : old style              "Template"              string using ";" as separator
    1 : internal paths         "Template_internal"     string list
    2 : user paths             "Template_user"         string list
    3 : write path             "Template_write"        string
 */

#define POSTFIX_INTERNAL_PATHS "_internal"
#define POSTFIX_USER_PATHS "_user"
#define POSTFIX_WRITE_PATH "_writable"

namespace {

const sal_Int32 IDGROUP_OLDSTYLE        = 0;
const sal_Int32 IDGROUP_INTERNAL_PATHS = 1;
const sal_Int32 IDGROUP_USER_PATHS     = 2;
const sal_Int32 IDGROUP_WRITE_PATH      = 3;

const sal_Int32 IDGROUP_COUNT           = 4;

sal_Int32 impl_getPropGroup(sal_Int32 nID)
{
    return (nID % IDGROUP_COUNT);
}

/* enable it if you wish to migrate old user settings (using the old cfg schema) on demand ....
   disable it in case only the new schema must be used.
 */

typedef ::cppu::WeakComponentImplHelper3<
            css::lang::XServiceInfo,
            css::util::XChangesListener,    // => XEventListener
            css::util::XPathSettings>       // => XPropertySet
                PathSettings_BASE;

class PathSettings : private cppu::BaseMutex
                   , public  PathSettings_BASE
                   , public  ::cppu::OPropertySetHelper
{
    struct PathInfo
    {
        public:

            PathInfo()
                : sPathName     ()
                , lInternalPaths()
                , lUserPaths    ()
                , sWritePath    ()
                , bIsSinglePath (false)
                , bIsReadonly   (false)
            {}

            PathInfo(const PathInfo& rCopy)
            {
                takeOver(rCopy);
            }

            void takeOver(const PathInfo& rCopy)
            {
                sPathName      = rCopy.sPathName;
                lInternalPaths = rCopy.lInternalPaths;
                lUserPaths     = rCopy.lUserPaths;
                sWritePath     = rCopy.sWritePath;
                bIsSinglePath  = rCopy.bIsSinglePath;
                bIsReadonly    = rCopy.bIsReadonly;
            }

            /// an internal name describing this path
            OUString sPathName;

            /// contains all paths, which are used internally - but are not visible for the user.
            OUStringList lInternalPaths;

            /// contains all paths configured by the user
            OUStringList lUserPaths;

            /// this special path is used to generate feature depending content there
            OUString sWritePath;

            /// indicates real single paths, which uses WritePath property only
            bool bIsSinglePath;

            /// simple handling of finalized/mandatory states ... => we know one state READONLY only .-)
            bool bIsReadonly;
    };

    typedef BaseHash< PathSettings::PathInfo > PathHash;

    enum EChangeOp
    {
        E_UNDEFINED,
        E_ADDED,
        E_CHANGED,
        E_REMOVED
    };

private:

    /** reference to factory, which has create this instance. */
    css::uno::Reference< css::uno::XComponentContext > m_xContext;

    /** list of all path variables and her corresponding values. */
    PathSettings::PathHash m_lPaths;

    /** describes all properties available on our interface.
        Will be generated on demand based on our path list m_lPaths. */
    css::uno::Sequence< css::beans::Property > m_lPropDesc;

    /** helper needed to (re-)substitute all internal save path values. */
    css::uno::Reference< css::util::XStringSubstitution > m_xSubstitution;

    /** provides access to the old configuration schema (which will be migrated on demand). */
    css::uno::Reference< css::container::XNameAccess > m_xCfgOld;

    /** provides access to the new configuration schema. */
    css::uno::Reference< css::container::XNameAccess > m_xCfgNew;

    /** helper to listen for configuration changes without ownership cycle problems */
    css::uno::Reference< css::util::XChangesListener > m_xCfgNewListener;

    ::cppu::OPropertyArrayHelper* m_pPropHelp;

    bool m_bIgnoreEvents;

public:

    /** initialize a new instance of this class.
        Attention: It's necessary for right function of this class, that the order of base
        classes is the right one. Because we transfer information from one base to another
        during this ctor runs! */
    PathSettings(const css::uno::Reference< css::uno::XComponentContext >& xContext);

    /** free all used resources ... if it was not already done. */
    virtual ~PathSettings();

    virtual OUString SAL_CALL getImplementationName()
        throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
    {
        return OUString("com.sun.star.comp.framework.PathSettings");
    }

    virtual sal_Bool SAL_CALL supportsService(OUString const & ServiceName)
        throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
    {
        return cppu::supportsService(this, ServiceName);
    }

    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames()
        throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
    {
        css::uno::Sequence< OUString > aSeq(1);
        aSeq[0] = OUString("com.sun.star.util.PathSettings");
        return aSeq;
    }

    // XInterface
    virtual css::uno::Any SAL_CALL queryInterface( const css::uno::Type& type) throw ( css::uno::RuntimeException, std::exception ) SAL_OVERRIDE;
    virtual void SAL_CALL acquire() throw () SAL_OVERRIDE
        { OWeakObject::acquire(); }
    virtual void SAL_CALL release() throw () SAL_OVERRIDE
        { OWeakObject::release(); }

    // XTypeProvider
    virtual css::uno::Sequence< css::uno::Type > SAL_CALL getTypes(  ) throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    // css::util::XChangesListener
    virtual void SAL_CALL changesOccurred(const css::util::ChangesEvent& aEvent) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    // css::lang::XEventListener
    virtual void SAL_CALL disposing(const css::lang::EventObject& aSource)
        throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    /**
     * XPathSettings attribute methods
     */
    virtual OUString SAL_CALL getAddin() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Addin"); }
    virtual void SAL_CALL setAddin(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Addin", p1); }
    virtual OUString SAL_CALL getAutoCorrect() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("AutoCorrect"); }
    virtual void SAL_CALL setAutoCorrect(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("AutoCorrect", p1); }
    virtual OUString SAL_CALL getAutoText() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("AutoText"); }
    virtual void SAL_CALL setAutoText(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("AutoText", p1); }
    virtual OUString SAL_CALL getBackup() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Backup"); }
    virtual void SAL_CALL setBackup(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Backup", p1); }
    virtual OUString SAL_CALL getBasic() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Basic"); }
    virtual void SAL_CALL setBasic(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Basic", p1); }
    virtual OUString SAL_CALL getBitmap() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Bitmap"); }
    virtual void SAL_CALL setBitmap(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Bitmap", p1); }
    virtual OUString SAL_CALL getConfig() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Config"); }
    virtual void SAL_CALL setConfig(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Config", p1); }
    virtual OUString SAL_CALL getDictionary() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Dictionary"); }
    virtual void SAL_CALL setDictionary(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Dictionary", p1); }
    virtual OUString SAL_CALL getFavorite() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Favorite"); }
    virtual void SAL_CALL setFavorite(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Favorite", p1); }
    virtual OUString SAL_CALL getFilter() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Filter"); }
    virtual void SAL_CALL setFilter(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Filter", p1); }
    virtual OUString SAL_CALL getGallery() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Gallery"); }
    virtual void SAL_CALL setGallery(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Gallery", p1); }
    virtual OUString SAL_CALL getGraphic() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Graphic"); }
    virtual void SAL_CALL setGraphic(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Graphic", p1); }
    virtual OUString SAL_CALL getHelp() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Help"); }
    virtual void SAL_CALL setHelp(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Help", p1); }
    virtual OUString SAL_CALL getLinguistic() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Linguistic"); }
    virtual void SAL_CALL setLinguistic(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Linguistic", p1); }
    virtual OUString SAL_CALL getModule() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Module"); }
    virtual void SAL_CALL setModule(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Module", p1); }
    virtual OUString SAL_CALL getPalette() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Palette"); }
    virtual void SAL_CALL setPalette(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Palette", p1); }
    virtual OUString SAL_CALL getPlugin() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Plugin"); }
    virtual void SAL_CALL setPlugin(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Plugin", p1); }
    virtual OUString SAL_CALL getStorage() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Storage"); }
    virtual void SAL_CALL setStorage(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Storage", p1); }
    virtual OUString SAL_CALL getTemp() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Temp"); }
    virtual void SAL_CALL setTemp(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Temp", p1); }
    virtual OUString SAL_CALL getTemplate() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Template"); }
    virtual void SAL_CALL setTemplate(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Template", p1); }
    virtual OUString SAL_CALL getUIConfig() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("UIConfig"); }
    virtual void SAL_CALL setUIConfig(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("UIConfig", p1); }
    virtual OUString SAL_CALL getUserConfig() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("UserConfig"); }
    virtual void SAL_CALL setUserConfig(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("UserConfig", p1); }
    virtual OUString SAL_CALL getUserDictionary() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("UserDictionary"); }
    virtual void SAL_CALL setUserDictionary(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("UserDictionary", p1); }
    virtual OUString SAL_CALL getWork() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("Work"); }
    virtual void SAL_CALL setWork(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("Work", p1); }
    virtual OUString SAL_CALL getBasePathShareLayer() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("UIConfig"); }
    virtual void SAL_CALL setBasePathShareLayer(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("UIConfig", p1); }
    virtual OUString SAL_CALL getBasePathUserLayer() throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return getStringProperty("UserConfig"); }
    virtual void SAL_CALL setBasePathUserLayer(const OUString& p1) throw (css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { setStringProperty("UserConfig", p1); }

    /**
     * overrides to resolve inheritance ambiguity
     */
    virtual void SAL_CALL setPropertyValue(const OUString& p1, const css::uno::Any& p2)
        throw (css::beans::UnknownPropertyException, css::beans::PropertyVetoException, css::lang::IllegalArgumentException, css::lang::WrappedTargetException, css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { ::cppu::OPropertySetHelper::setPropertyValue(p1, p2); }
    virtual css::uno::Any SAL_CALL getPropertyValue(const OUString& p1)
        throw (css::beans::UnknownPropertyException, css::lang::WrappedTargetException, css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { return ::cppu::OPropertySetHelper::getPropertyValue(p1); }
    virtual void SAL_CALL addPropertyChangeListener(const OUString& p1, const css::uno::Reference<css::beans::XPropertyChangeListener>& p2)
        throw (css::beans::UnknownPropertyException, css::lang::WrappedTargetException, css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { ::cppu::OPropertySetHelper::addPropertyChangeListener(p1, p2); }
    virtual void SAL_CALL removePropertyChangeListener(const OUString& p1, const css::uno::Reference<css::beans::XPropertyChangeListener>& p2)
        throw (css::beans::UnknownPropertyException, css::lang::WrappedTargetException, css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { ::cppu::OPropertySetHelper::removePropertyChangeListener(p1, p2); }
    virtual void SAL_CALL addVetoableChangeListener(const OUString& p1, const css::uno::Reference<css::beans::XVetoableChangeListener>& p2)
        throw (css::beans::UnknownPropertyException, css::lang::WrappedTargetException, css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { ::cppu::OPropertySetHelper::addVetoableChangeListener(p1, p2); }
    virtual void SAL_CALL removeVetoableChangeListener(const OUString& p1, const css::uno::Reference<css::beans::XVetoableChangeListener>& p2)
        throw (css::beans::UnknownPropertyException, css::lang::WrappedTargetException, css::uno::RuntimeException, std::exception) SAL_OVERRIDE
        { ::cppu::OPropertySetHelper::removeVetoableChangeListener(p1, p2); }
    /** read all configured paths and create all needed internal structures. */
    void impl_readAll();

private:
    virtual void SAL_CALL disposing() SAL_OVERRIDE;

    OUString getStringProperty(const OUString& p1)
        throw(css::uno::RuntimeException);

    void setStringProperty(const OUString& p1, const OUString& p2)
        throw(css::uno::RuntimeException);

    /** read a path info using the old cfg schema.
        This is needed for "migration on demand" reasons only.
        Can be removed for next major release .-) */
    OUStringList impl_readOldFormat(const OUString& sPath);

    /** read a path info using the new cfg schema. */
    PathSettings::PathInfo impl_readNewFormat(const OUString& sPath);

    /** filter "real user defined paths" from the old configuration schema
        and set it as UserPaths on the new schema.
        Can be removed with new major release ... */

    void impl_mergeOldUserPaths(      PathSettings::PathInfo& rPath,
                                 const OUStringList&           lOld );

    /** reload one path directly from the new configuration schema (because
        it was updated by any external code) */
    PathSettings::EChangeOp impl_updatePath(const OUString& sPath          ,
                                                  bool         bNotifyListener);

    /** replace all might existing placeholder variables inside the given path ...
        or check if the given path value uses paths, which can be replaced with predefined
        placeholder variables ...
     */
    void impl_subst(      OUStringList&                                          lVals   ,
                    const css::uno::Reference< css::util::XStringSubstitution >& xSubst  ,
                          bool                                               bReSubst);

    void impl_subst(PathSettings::PathInfo& aPath   ,
                    bool                bReSubst);

    /** converts our new string list schema to the old ";" separated schema ... */
    OUString impl_convertPath2OldStyle(const PathSettings::PathInfo& rPath        ) const;
    OUStringList    impl_convertOldStyle2Path(const OUString&        sOldStylePath) const;

    /** remove still known paths from the given lList argument.
        So real user defined paths can be extracted from the list of
        fix internal paths !
     */
    void impl_purgeKnownPaths(const PathSettings::PathInfo& rPath,
                                     OUStringList&           lList);

    /** rebuild the member m_lPropDesc using the path list m_lPaths. */
    void impl_rebuildPropertyDescriptor();

    /** provides direct access to the list of path values
        using it's internal property id.
     */
    css::uno::Any impl_getPathValue(      sal_Int32      nID ) const;
    void          impl_setPathValue(      sal_Int32      nID ,
                                    const css::uno::Any& aVal);

    /** check the given handle and return the corresponding PathInfo reference.
        These reference can be used then directly to manipulate these path. */
          PathSettings::PathInfo* impl_getPathAccess     (sal_Int32 nHandle);
    const PathSettings::PathInfo* impl_getPathAccessConst(sal_Int32 nHandle) const;

    /** it checks, if the given path value seems to be a valid URL or system path. */
    bool impl_isValidPath(const OUString& sPath) const;
    bool impl_isValidPath(const OUStringList&    lPath) const;

    void impl_storePath(const PathSettings::PathInfo& aPath);

    css::uno::Sequence< sal_Int32 > impl_mapPathName2IDList(const OUString& sPath);

    void impl_notifyPropListener(      PathSettings::EChangeOp eOp     ,
                                       const OUString&        sPath   ,
                                       const PathSettings::PathInfo* pPathOld,
                                       const PathSettings::PathInfo* pPathNew);

    //  OPropertySetHelper
    virtual sal_Bool SAL_CALL convertFastPropertyValue( css::uno::Any&  aConvertedValue,
            css::uno::Any& aOldValue,
            sal_Int32 nHandle,
            const css::uno::Any& aValue ) throw(css::lang::IllegalArgumentException) SAL_OVERRIDE;
    virtual void SAL_CALL setFastPropertyValue_NoBroadcast( sal_Int32 nHandle,
            const css::uno::Any&  aValue ) throw(css::uno::Exception, std::exception) SAL_OVERRIDE;
    virtual void SAL_CALL getFastPropertyValue( css::uno::Any&  aValue,
            sal_Int32 nHandle ) const SAL_OVERRIDE;
    // Avoid:
    // warning: ‘virtual com::sun::star::uno::Any cppu::OPropertySetHelper::getFastPropertyValue(sal_Int32)’ was hidden [-Woverloaded-virtual]
    // warning:   by ‘virtual void {anonymous}::PathSettings::getFastPropertyValue(com::sun::star::uno::Any&, sal_Int32) const’ [-Woverloaded-virtual]
    using cppu::OPropertySetHelper::getFastPropertyValue;
    virtual ::cppu::IPropertyArrayHelper& SAL_CALL getInfoHelper() SAL_OVERRIDE;
    virtual css::uno::Reference< css::beans::XPropertySetInfo > SAL_CALL getPropertySetInfo() throw(css::uno::RuntimeException, std::exception) SAL_OVERRIDE;

    /** factory methods to guarantee right (but on demand) initialized members ... */
    css::uno::Reference< css::util::XStringSubstitution > fa_getSubstitution();
    css::uno::Reference< css::container::XNameAccess >    fa_getCfgOld();
    css::uno::Reference< css::container::XNameAccess >    fa_getCfgNew();
};

PathSettings::PathSettings( const css::uno::Reference< css::uno::XComponentContext >& xContext )
    : PathSettings_BASE(m_aMutex)
    , ::cppu::OPropertySetHelper(cppu::WeakComponentImplHelperBase::rBHelper)
    ,   m_xContext (xContext)
    ,   m_pPropHelp(0    )
    ,  m_bIgnoreEvents(false)
{
}

PathSettings::~PathSettings()
{
    disposing();
}

void SAL_CALL PathSettings::disposing()
{
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);

    css::uno::Reference< css::util::XChangesNotifier >
        xBroadcaster(m_xCfgNew, css::uno::UNO_QUERY);
    if (xBroadcaster.is())
        xBroadcaster->removeChangesListener(m_xCfgNewListener);

    m_xSubstitution.clear();
    m_xCfgOld.clear();
    m_xCfgNew.clear();
    m_xCfgNewListener.clear();

    delete m_pPropHelp;
    m_pPropHelp = 0;
}

css::uno::Any SAL_CALL PathSettings::queryInterface( const css::uno::Type& _rType )
    throw(css::uno::RuntimeException, std::exception)
{
    css::uno::Any aRet = PathSettings_BASE::queryInterface( _rType );
    if ( !aRet.hasValue() )
        aRet = ::cppu::OPropertySetHelper::queryInterface( _rType );
    return aRet;
}

css::uno::Sequence< css::uno::Type > SAL_CALL PathSettings::getTypes(  )
    throw(css::uno::RuntimeException, std::exception)
{
    return comphelper::concatSequences(
        PathSettings_BASE::getTypes(),
        ::cppu::OPropertySetHelper::getTypes()
    );
}

void SAL_CALL PathSettings::changesOccurred(const css::util::ChangesEvent& aEvent)
    throw (css::uno::RuntimeException, std::exception)
{
    sal_Int32 c                 = aEvent.Changes.getLength();
    sal_Int32 i                 = 0;
    bool  bUpdateDescriptor = false;

    for (i=0; i<c; ++i)
    {
        const css::util::ElementChange& aChange = aEvent.Changes[i];

        OUString sChanged;
        aChange.Accessor >>= sChanged;

        OUString sPath = ::utl::extractFirstFromConfigurationPath(sChanged);
        if (!sPath.isEmpty())
        {
            PathSettings::EChangeOp eOp = impl_updatePath(sPath, true);
            if (
                (eOp == PathSettings::E_ADDED  ) ||
                (eOp == PathSettings::E_REMOVED)
               )
                bUpdateDescriptor = true;
        }
    }

    if (bUpdateDescriptor)
        impl_rebuildPropertyDescriptor();
}

void SAL_CALL PathSettings::disposing(const css::lang::EventObject& aSource)
    throw(css::uno::RuntimeException, std::exception)
{
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);

    if (aSource.Source == m_xCfgNew)
        m_xCfgNew.clear();
}

OUString PathSettings::getStringProperty(const OUString& p1)
    throw(css::uno::RuntimeException)
{
    css::uno::Any a = ::cppu::OPropertySetHelper::getPropertyValue(p1);
    OUString s;
    a >>= s;
    return s;
}

void PathSettings::setStringProperty(const OUString& p1, const OUString& p2)
    throw(css::uno::RuntimeException)
{
    ::cppu::OPropertySetHelper::setPropertyValue(p1, css::uno::Any(p2));
}

void PathSettings::impl_readAll()
{
    try
    {
        // TODO think about me
        css::uno::Reference< css::container::XNameAccess > xCfg    = fa_getCfgNew();
        css::uno::Sequence< OUString >              lPaths = xCfg->getElementNames();

        sal_Int32 c = lPaths.getLength();
        for (sal_Int32 i = 0; i < c; ++i)
        {
            const OUString& sPath = lPaths[i];
            impl_updatePath(sPath, false);
        }
    }
    catch(const css::uno::RuntimeException& )
    {
    }

    impl_rebuildPropertyDescriptor();
}

// NO substitution here ! It's done outside ...
OUStringList PathSettings::impl_readOldFormat(const OUString& sPath)
{
    css::uno::Reference< css::container::XNameAccess > xCfg( fa_getCfgOld() );
    OUStringList aPathVal;

    if( xCfg->hasByName(sPath) )
    {
        css::uno::Any aVal( xCfg->getByName(sPath) );

        OUString                       sStringVal;
        css::uno::Sequence< OUString > lStringListVal;

        if (aVal >>= sStringVal)
        {
            aPathVal.push_back(sStringVal);
        }
        else if (aVal >>= lStringListVal)
        {
            aPathVal << lStringListVal;
        }
    }

    return aPathVal;
}

// NO substitution here ! It's done outside ...
PathSettings::PathInfo PathSettings::impl_readNewFormat(const OUString& sPath)
{
    const OUString CFGPROP_INTERNALPATHS("InternalPaths");
    const OUString CFGPROP_ISSINGLEPATH("IsSinglePath");

    css::uno::Reference< css::container::XNameAccess > xCfg = fa_getCfgNew();

    // get access to the "queried" path
    css::uno::Reference< css::container::XNameAccess > xPath;
    xCfg->getByName(sPath) >>= xPath;

    PathSettings::PathInfo aPathVal;

    // read internal path list
    css::uno::Reference< css::container::XNameAccess > xIPath;
    xPath->getByName(CFGPROP_INTERNALPATHS) >>= xIPath;
    aPathVal.lInternalPaths << xIPath->getElementNames();

    // read user defined path list
    aPathVal.lUserPaths << xPath->getByName(CFGPROP_USERPATHS);

    // read the writeable path
    xPath->getByName(CFGPROP_WRITEPATH) >>= aPathVal.sWritePath;

    // avoid duplicates, by removing the writeable path from
    // the user defined path list if it happens to be there too
    OUStringList::iterator aI = aPathVal.lUserPaths.find(aPathVal.sWritePath);
    if (aI != aPathVal.lUserPaths.end())
        aPathVal.lUserPaths.erase(aI);

    // read state props
    xPath->getByName(CFGPROP_ISSINGLEPATH) >>= aPathVal.bIsSinglePath;

    // analyze finalized/mandatory states
    aPathVal.bIsReadonly = false;
    css::uno::Reference< css::beans::XProperty > xInfo(xPath, css::uno::UNO_QUERY);
    if (xInfo.is())
    {
        css::beans::Property aInfo = xInfo->getAsProperty();
        bool bFinalized = ((aInfo.Attributes & css::beans::PropertyAttribute::READONLY  ) == css::beans::PropertyAttribute::READONLY  );

        // Note: Till we support finalized / mandatory on our API more in detail we handle
        // all states simple as READONLY ! But because all really needed paths are "mandatory" by default
        // we have to handle "finalized" as the real "readonly" indicator .
        aPathVal.bIsReadonly = bFinalized;
    }

    return aPathVal;
}

void PathSettings::impl_storePath(const PathSettings::PathInfo& aPath)
{
    m_bIgnoreEvents = true;

    css::uno::Reference< css::container::XNameAccess > xCfgNew = fa_getCfgNew();
    css::uno::Reference< css::container::XNameAccess > xCfgOld = fa_getCfgOld();

    // try to replace path-parts with well known and uspported variables.
    // So an office can be moved easialy to another location without loosing
    // it's related paths.
    PathInfo aResubstPath(aPath);
    impl_subst(aResubstPath, true);

    // update new configuration
    if (! aResubstPath.bIsSinglePath)
    {
        ::comphelper::ConfigurationHelper::writeRelativeKey(xCfgNew,
                                                            aResubstPath.sPathName,
                                                            CFGPROP_USERPATHS,
                                                            css::uno::makeAny(aResubstPath.lUserPaths.getAsConstList()));
    }

    ::comphelper::ConfigurationHelper::writeRelativeKey(xCfgNew,
                                                        aResubstPath.sPathName,
                                                        CFGPROP_WRITEPATH,
                                                        css::uno::makeAny(aResubstPath.sWritePath));

    ::comphelper::ConfigurationHelper::flush(xCfgNew);

    // remove the whole path from the old configuration !
    // Otherwise we can't make sure that the diff between new and old configuration
    // on loading time really represent an user setting !!!

    // Check if the given path exists inside the old configuration.
    // Because our new configuration knows more than the list of old paths ... !
    if (xCfgOld->hasByName(aResubstPath.sPathName))
    {
        css::uno::Reference< css::beans::XPropertySet > xProps(xCfgOld, css::uno::UNO_QUERY_THROW);
        xProps->setPropertyValue(aResubstPath.sPathName, css::uno::Any());
        ::comphelper::ConfigurationHelper::flush(xCfgOld);
    }

    m_bIgnoreEvents = false;
}

void PathSettings::impl_mergeOldUserPaths(      PathSettings::PathInfo& rPath,
                                          const OUStringList&           lOld )
{
    OUStringList::const_iterator pIt;
    for (  pIt  = lOld.begin();
           pIt != lOld.end();
         ++pIt                )
    {
        const OUString& sOld = *pIt;

        if (rPath.bIsSinglePath)
        {
            SAL_WARN_IF(lOld.size()>1, "fwk", "PathSettings::impl_mergeOldUserPaths(): Single path has more than one path value inside old configuration (Common.xcu)!");
            if (! rPath.sWritePath.equals(sOld))
               rPath.sWritePath = sOld;
        }
        else
        {
            if (
                (  rPath.lInternalPaths.findConst(sOld) == rPath.lInternalPaths.end()) &&
                (  rPath.lUserPaths.findConst(sOld)     == rPath.lUserPaths.end()    ) &&
                (! rPath.sWritePath.equals(sOld)                                     )
               )
               rPath.lUserPaths.push_back(sOld);
        }
    }
}

PathSettings::EChangeOp PathSettings::impl_updatePath(const OUString& sPath          ,
                                                            bool         bNotifyListener)
{
    // SAFE ->
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);

    PathSettings::PathInfo* pPathOld = 0;
    PathSettings::PathInfo* pPathNew = 0;
    PathSettings::EChangeOp eOp      = PathSettings::E_UNDEFINED;
    PathSettings::PathInfo  aPath;

    try
    {
        aPath = impl_readNewFormat(sPath);
        aPath.sPathName = sPath;
        // replace all might existing variables with real values
        // Do it before these old paths will be compared against the
        // new path configuration. Otherwise some striungs uses different variables ... but substitution
        // will produce strings with same content (because some variables are redundant!)
        impl_subst(aPath, false);
    }
    catch(const css::uno::RuntimeException&)
        { throw; }
    catch(const css::container::NoSuchElementException&)
        { eOp = PathSettings::E_REMOVED; }
    catch(const css::uno::Exception&)
        { throw; }

    try
    {
        // migration of old user defined values on demand
        // can be disabled for a new major
        OUStringList lOldVals = impl_readOldFormat(sPath);
        // replace all might existing variables with real values
        // Do it before these old paths will be compared against the
        // new path configuration. Otherwise some striungs uses different variables ... but substitution
        // will produce strings with same content (because some variables are redundant!)
        impl_subst(lOldVals, fa_getSubstitution(), false);
        impl_mergeOldUserPaths(aPath, lOldVals);
    }
    catch(const css::uno::RuntimeException&)
        { throw; }
    // Normal(!) exceptions can be ignored!
    // E.g. in case an addon installs a new path, which was not well known for an OOo 1.x installation
    // we can't find a value for it inside the "old" configuration. So a NoSuchElementException
    // will be normal .-)
    catch(const css::uno::Exception&)
        {}

    PathSettings::PathHash::iterator pPath = m_lPaths.find(sPath);
    if (eOp == PathSettings::E_UNDEFINED)
    {
        if (pPath != m_lPaths.end())
            eOp = PathSettings::E_CHANGED;
        else
            eOp = PathSettings::E_ADDED;
    }

    switch(eOp)
    {
        case PathSettings::E_ADDED :
             {
                if (bNotifyListener)
                {
                    pPathOld = 0;
                    pPathNew = &aPath;
                    impl_notifyPropListener(eOp, sPath, pPathOld, pPathNew);
                }
                m_lPaths[sPath] = aPath;
             }
             break;

        case PathSettings::E_CHANGED :
             {
                if (bNotifyListener)
                {
                    pPathOld = &(pPath->second);
                    pPathNew = &aPath;
                    impl_notifyPropListener(eOp, sPath, pPathOld, pPathNew);
                }
                m_lPaths[sPath] = aPath;
             }
             break;

        case PathSettings::E_REMOVED :
             {
                if (pPath != m_lPaths.end())
                {
                    if (bNotifyListener)
                    {
                        pPathOld = &(pPath->second);
                        pPathNew = 0;
                        impl_notifyPropListener(eOp, sPath, pPathOld, pPathNew);
                    }
                    m_lPaths.erase(pPath);
                }
             }
             break;

        default: // to let compiler be happy
             break;
    }

    return eOp;
}

css::uno::Sequence< sal_Int32 > PathSettings::impl_mapPathName2IDList(const OUString& sPath)
{
    OUString sOldStyleProp = sPath;
    OUString sInternalProp = sPath+POSTFIX_INTERNAL_PATHS;
    OUString sUserProp     = sPath+POSTFIX_USER_PATHS;
    OUString sWriteProp    = sPath+POSTFIX_WRITE_PATH;

    // Attention: The default set of IDs is fix and must follow these schema.
    // Otherwhise the outside code ant work for new added properties.
    // Why ?
    // The outside code must fire N events for every changed property.
    // And the knowing about packaging of variables of the structure PathInfo
    // follow these group IDs ! But if such ID isnt in the range of [0..IDGROUP_COUNT]
    // the outside can't determine the right group ... and cant fire the right events .-)

    css::uno::Sequence< sal_Int32 > lIDs(IDGROUP_COUNT);
    lIDs[0] = IDGROUP_OLDSTYLE;
    lIDs[1] = IDGROUP_INTERNAL_PATHS;
    lIDs[2] = IDGROUP_USER_PATHS;
    lIDs[3] = IDGROUP_WRITE_PATH;

    sal_Int32 c = m_lPropDesc.getLength();
    sal_Int32 i = 0;
    for (i=0; i<c; ++i)
    {
        const css::beans::Property& rProp = m_lPropDesc[i];

        if (rProp.Name.equals(sOldStyleProp))
            lIDs[IDGROUP_OLDSTYLE] = rProp.Handle;
        else
        if (rProp.Name.equals(sInternalProp))
            lIDs[IDGROUP_INTERNAL_PATHS] = rProp.Handle;
        else
        if (rProp.Name.equals(sUserProp))
            lIDs[IDGROUP_USER_PATHS] = rProp.Handle;
        else
        if (rProp.Name.equals(sWriteProp))
            lIDs[IDGROUP_WRITE_PATH] = rProp.Handle;
    }

    return lIDs;
}

void PathSettings::impl_notifyPropListener(      PathSettings::EChangeOp /*eOp*/     ,
                                           const OUString&        sPath   ,
                                           const PathSettings::PathInfo* pPathOld,
                                           const PathSettings::PathInfo* pPathNew)
{
    css::uno::Sequence< sal_Int32 >     lHandles(1);
    css::uno::Sequence< css::uno::Any > lOldVals(1);
    css::uno::Sequence< css::uno::Any > lNewVals(1);

    css::uno::Sequence< sal_Int32 > lIDs   = impl_mapPathName2IDList(sPath);
    sal_Int32                       c      = lIDs.getLength();
    sal_Int32                       i      = 0;
    sal_Int32                       nMaxID = m_lPropDesc.getLength()-1;
    for (i=0; i<c; ++i)
    {
        sal_Int32 nID = lIDs[i];

        if (
            (nID < 0     ) ||
            (nID > nMaxID)
           )
           continue;

        lHandles[0] = nID;
        switch(impl_getPropGroup(nID))
        {
            case IDGROUP_OLDSTYLE :
                 {
                    if (pPathOld)
                    {
                        OUString sVal = impl_convertPath2OldStyle(*pPathOld);
                        lOldVals[0] <<= sVal;
                    }
                    if (pPathNew)
                    {
                        OUString sVal = impl_convertPath2OldStyle(*pPathNew);
                        lNewVals[0] <<= sVal;
                    }
                 }
                 break;

            case IDGROUP_INTERNAL_PATHS :
                 {
                    if (pPathOld)
                        lOldVals[0] <<= pPathOld->lInternalPaths.getAsConstList();
                    if (pPathNew)
                        lNewVals[0] <<= pPathNew->lInternalPaths.getAsConstList();
                 }
                 break;

            case IDGROUP_USER_PATHS :
                 {
                    if (pPathOld)
                        lOldVals[0] <<= pPathOld->lUserPaths.getAsConstList();
                    if (pPathNew)
                        lNewVals[0] <<= pPathNew->lUserPaths.getAsConstList();
                 }
                 break;

            case IDGROUP_WRITE_PATH :
                 {
                    if (pPathOld)
                        lOldVals[0] <<= pPathOld->sWritePath;
                    if (pPathNew)
                        lNewVals[0] <<= pPathNew->sWritePath;
                 }
                 break;
        }

        fire(lHandles.getArray(),
             lNewVals.getArray(),
             lOldVals.getArray(),
             1,
             sal_False);
    }
}

void PathSettings::impl_subst(      OUStringList&                                          lVals   ,
                              const css::uno::Reference< css::util::XStringSubstitution >& xSubst  ,
                                    bool                                               bReSubst)
{
    OUStringList::iterator pIt;

    for (  pIt  = lVals.begin();
           pIt != lVals.end();
         ++pIt                 )
    {
        const OUString& sOld = *pIt;
              OUString  sNew;
        if (bReSubst)
            sNew = xSubst->reSubstituteVariables(sOld);
        else
            sNew = xSubst->substituteVariables(sOld, sal_False);

        *pIt = sNew;
    }
}

void PathSettings::impl_subst(PathSettings::PathInfo& aPath   ,
                              bool                bReSubst)
{
    css::uno::Reference< css::util::XStringSubstitution > xSubst = fa_getSubstitution();

    impl_subst(aPath.lInternalPaths, xSubst, bReSubst);
    impl_subst(aPath.lUserPaths    , xSubst, bReSubst);
    if (bReSubst)
        aPath.sWritePath = xSubst->reSubstituteVariables(aPath.sWritePath);
    else
        aPath.sWritePath = xSubst->substituteVariables(aPath.sWritePath, sal_False);
}

OUString PathSettings::impl_convertPath2OldStyle(const PathSettings::PathInfo& rPath) const
{
    OUStringList::const_iterator pIt;
    OUStringList                 lTemp;
    lTemp.reserve(rPath.lInternalPaths.size() + rPath.lUserPaths.size() + 1);

    for (  pIt  = rPath.lInternalPaths.begin();
           pIt != rPath.lInternalPaths.end();
         ++pIt                                 )
    {
        lTemp.push_back(*pIt);
    }
    for (  pIt  = rPath.lUserPaths.begin();
           pIt != rPath.lUserPaths.end();
         ++pIt                             )
    {
        lTemp.push_back(*pIt);
    }

    if (!rPath.sWritePath.isEmpty())
        lTemp.push_back(rPath.sWritePath);

    OUStringBuffer sPathVal(256);
    for (  pIt  = lTemp.begin();
           pIt != lTemp.end();
                               )
    {
        sPathVal.append(*pIt);
        ++pIt;
        if (pIt != lTemp.end())
            sPathVal.appendAscii(";");
    }

    return sPathVal.makeStringAndClear();
}

OUStringList PathSettings::impl_convertOldStyle2Path(const OUString& sOldStylePath) const
{
    OUStringList lList;
    sal_Int32    nToken = 0;
    do
    {
        OUString sToken = sOldStylePath.getToken(0, ';', nToken);
        if (!sToken.isEmpty())
            lList.push_back(sToken);
    }
    while(nToken >= 0);

    return lList;
}

void PathSettings::impl_purgeKnownPaths(const PathSettings::PathInfo& rPath,
                                               OUStringList&           lList)
{
    OUStringList::const_iterator pIt;
    for (  pIt  = rPath.lInternalPaths.begin();
           pIt != rPath.lInternalPaths.end();
         ++pIt                                 )
    {
        const OUString& rItem = *pIt;
        OUStringList::iterator pItem = lList.find(rItem);
        if (pItem != lList.end())
            lList.erase(pItem);
    }
    for (  pIt  = rPath.lUserPaths.begin();
           pIt != rPath.lUserPaths.end();
         ++pIt                             )
    {
        const OUString& rItem = *pIt;
        OUStringList::iterator pItem = lList.find(rItem);
        if (pItem != lList.end())
            lList.erase(pItem);
    }

    OUStringList::iterator pItem = lList.find(rPath.sWritePath);
    if (pItem != lList.end())
        lList.erase(pItem);
}

void PathSettings::impl_rebuildPropertyDescriptor()
{
    // SAFE ->
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);

    sal_Int32 c = (sal_Int32)m_lPaths.size();
    sal_Int32 i = 0;
    m_lPropDesc.realloc(c*IDGROUP_COUNT);

    PathHash::const_iterator pIt;
    for (  pIt  = m_lPaths.begin();
           pIt != m_lPaths.end();
         ++pIt                     )
    {
        const PathSettings::PathInfo& rPath = pIt->second;
              css::beans::Property*   pProp = 0;

        pProp             = &(m_lPropDesc[i]);
        pProp->Name       = rPath.sPathName;
        pProp->Handle     = i;
        pProp->Type       = cppu::UnoType<OUString>::get();
        pProp->Attributes = css::beans::PropertyAttribute::BOUND;
        if (rPath.bIsReadonly)
            pProp->Attributes |= css::beans::PropertyAttribute::READONLY;
        ++i;

        pProp             = &(m_lPropDesc[i]);
        pProp->Name       = rPath.sPathName+POSTFIX_INTERNAL_PATHS;
        pProp->Handle     = i;
        pProp->Type       = ::getCppuType((css::uno::Sequence< OUString >*)0);
        pProp->Attributes = css::beans::PropertyAttribute::BOUND   |
                            css::beans::PropertyAttribute::READONLY;
        ++i;

        pProp             = &(m_lPropDesc[i]);
        pProp->Name       = rPath.sPathName+POSTFIX_USER_PATHS;
        pProp->Handle     = i;
        pProp->Type       = ::getCppuType((css::uno::Sequence< OUString >*)0);
        pProp->Attributes = css::beans::PropertyAttribute::BOUND;
        if (rPath.bIsReadonly)
            pProp->Attributes |= css::beans::PropertyAttribute::READONLY;
        ++i;

        pProp             = &(m_lPropDesc[i]);
        pProp->Name       = rPath.sPathName+POSTFIX_WRITE_PATH;
        pProp->Handle     = i;
        pProp->Type       = cppu::UnoType<OUString>::get();
        pProp->Attributes = css::beans::PropertyAttribute::BOUND;
        if (rPath.bIsReadonly)
            pProp->Attributes |= css::beans::PropertyAttribute::READONLY;
        ++i;
    }

    delete m_pPropHelp;
    m_pPropHelp = new ::cppu::OPropertyArrayHelper(m_lPropDesc, sal_False); // false => not sorted ... must be done inside helper

    // <- SAFE
}

css::uno::Any PathSettings::impl_getPathValue(sal_Int32 nID) const
{
    const PathSettings::PathInfo* pPath = impl_getPathAccessConst(nID);
    if (! pPath)
        throw css::lang::IllegalArgumentException();

    css::uno::Any aVal;
    switch(impl_getPropGroup(nID))
    {
        case IDGROUP_OLDSTYLE :
             {
                OUString sVal = impl_convertPath2OldStyle(*pPath);
                aVal <<= sVal;
             }
             break;

        case IDGROUP_INTERNAL_PATHS :
             {
                aVal <<= pPath->lInternalPaths.getAsConstList();
             }
             break;

        case IDGROUP_USER_PATHS :
             {
                aVal <<= pPath->lUserPaths.getAsConstList();
             }
             break;

        case IDGROUP_WRITE_PATH :
             {
                aVal <<= pPath->sWritePath;
             }
             break;
    }

    return aVal;
}

void PathSettings::impl_setPathValue(      sal_Int32      nID ,
                                     const css::uno::Any& aVal)
{
    PathSettings::PathInfo* pOrgPath = impl_getPathAccess(nID);
    if (! pOrgPath)
        throw css::container::NoSuchElementException();

    // We work on a copied path ... so we can be sure that errors during this operation
    // does not make our internal cache invalid  .-)
    PathSettings::PathInfo aChangePath(*pOrgPath);

    switch(impl_getPropGroup(nID))
    {
        case IDGROUP_OLDSTYLE :
             {
                OUString sVal;
                aVal >>= sVal;
                OUStringList lList = impl_convertOldStyle2Path(sVal);
                impl_subst(lList, fa_getSubstitution(), false);
                impl_purgeKnownPaths(aChangePath, lList);
                if (! impl_isValidPath(lList))
                    throw css::lang::IllegalArgumentException();

                if (aChangePath.bIsSinglePath)
                {
                    SAL_WARN_IF(lList.size()>1, "fwk", "PathSettings::impl_setPathValue(): You try to set more than path value for a defined SINGLE_PATH!");
                    if ( !lList.empty() )
                        aChangePath.sWritePath = *(lList.begin());
                    else
                        aChangePath.sWritePath = OUString();
                }
                else
                {
                    OUStringList::const_iterator pIt;
                    for (  pIt  = lList.begin();
                           pIt != lList.end();
                         ++pIt                 )
                    {
                        aChangePath.lUserPaths.push_back(*pIt);
                    }
                }
             }
             break;

        case IDGROUP_INTERNAL_PATHS :
             {
                if (aChangePath.bIsSinglePath)
                {
                    OUStringBuffer sMsg(256);
                    sMsg.appendAscii("The path '"    );
                    sMsg.append     (aChangePath.sPathName);
                    sMsg.appendAscii("' is defined as SINGLE_PATH. It's sub set of internal paths can't be set.");
                    throw css::uno::Exception(sMsg.makeStringAndClear(),
                                              static_cast< ::cppu::OWeakObject* >(this));
                }

                OUStringList lList;
                lList << aVal;
                if (! impl_isValidPath(lList))
                    throw css::lang::IllegalArgumentException();
                aChangePath.lInternalPaths = lList;
             }
             break;

        case IDGROUP_USER_PATHS :
             {
                if (aChangePath.bIsSinglePath)
                {
                    OUStringBuffer sMsg(256);
                    sMsg.appendAscii("The path '"    );
                    sMsg.append     (aChangePath.sPathName);
                    sMsg.appendAscii("' is defined as SINGLE_PATH. It's sub set of internal paths can't be set.");
                    throw css::uno::Exception(sMsg.makeStringAndClear(),
                                              static_cast< ::cppu::OWeakObject* >(this));
                }

                OUStringList lList;
                lList << aVal;
                if (! impl_isValidPath(lList))
                    throw css::lang::IllegalArgumentException();
                aChangePath.lUserPaths = lList;
             }
             break;

        case IDGROUP_WRITE_PATH :
             {
                OUString sVal;
                aVal >>= sVal;
                if (! impl_isValidPath(sVal))
                    throw css::lang::IllegalArgumentException();
                aChangePath.sWritePath = sVal;
             }
             break;
    }

    // TODO check if path has at least one path value set
    // At least it depends from the feature using this path, if an empty path list is allowed.

    // first we should try to store the changed (copied!) path ...
    // In case an error occurs on saving time an exception is thrown ...
    // If no exception occurs we can update our internal cache (means
    // we can overwrite pOrgPath !
    impl_storePath(aChangePath);
    pOrgPath->takeOver(aChangePath);
}

bool PathSettings::impl_isValidPath(const OUStringList& lPath) const
{
    OUStringList::const_iterator pIt;
    for (  pIt  = lPath.begin();
           pIt != lPath.end();
         ++pIt                 )
    {
        const OUString& rVal = *pIt;
        if (! impl_isValidPath(rVal))
            return false;
    }

    return true;
}

bool PathSettings::impl_isValidPath(const OUString& sPath) const
{
    // allow empty path to reset a path.
// idea by LLA to support empty paths
//    if (sPath.getLength() == 0)
//    {
//        return sal_True;
//    }

    return (! INetURLObject(sPath).HasError());
}

OUString impl_extractBaseFromPropName(const OUString& sPropName)
{
    sal_Int32 i = sPropName.indexOf(POSTFIX_INTERNAL_PATHS);
    if (i > -1)
        return sPropName.copy(0, i);
    i = sPropName.indexOf(POSTFIX_USER_PATHS);
    if (i > -1)
        return sPropName.copy(0, i);
    i = sPropName.indexOf(POSTFIX_WRITE_PATH);
    if (i > -1)
        return sPropName.copy(0, i);

    return sPropName;
}

PathSettings::PathInfo* PathSettings::impl_getPathAccess(sal_Int32 nHandle)
{
    // SAFE ->
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);

    if (nHandle > (m_lPropDesc.getLength()-1))
        return 0;

    const css::beans::Property&            rProp = m_lPropDesc[nHandle];
          OUString                  sProp = impl_extractBaseFromPropName(rProp.Name);
          PathSettings::PathHash::iterator rPath = m_lPaths.find(sProp);

    if (rPath != m_lPaths.end())
       return &(rPath->second);

    return 0;
    // <- SAFE
}

const PathSettings::PathInfo* PathSettings::impl_getPathAccessConst(sal_Int32 nHandle) const
{
    // SAFE ->
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);

    if (nHandle > (m_lPropDesc.getLength()-1))
        return 0;

    const css::beans::Property&                  rProp = m_lPropDesc[nHandle];
          OUString                        sProp = impl_extractBaseFromPropName(rProp.Name);
          PathSettings::PathHash::const_iterator rPath = m_lPaths.find(sProp);

    if (rPath != m_lPaths.end())
       return &(rPath->second);

    return 0;
    // <- SAFE
}

sal_Bool SAL_CALL PathSettings::convertFastPropertyValue(      css::uno::Any& aConvertedValue,
                                                               css::uno::Any& aOldValue      ,
                                                               sal_Int32      nHandle        ,
                                                         const css::uno::Any& aValue         )
    throw(css::lang::IllegalArgumentException)
{
    // throws NoSuchElementException !
    css::uno::Any aCurrentVal = impl_getPathValue(nHandle);

    return PropHelper::willPropertyBeChanged(
                aCurrentVal,
                aValue,
                aOldValue,
                aConvertedValue);
}

void SAL_CALL PathSettings::setFastPropertyValue_NoBroadcast(      sal_Int32      nHandle,
                                                             const css::uno::Any& aValue )
    throw(css::uno::Exception, std::exception)
{
    // throws NoSuchElement- and IllegalArgumentException !
    impl_setPathValue(nHandle, aValue);
}

void SAL_CALL PathSettings::getFastPropertyValue(css::uno::Any& aValue ,
                                                 sal_Int32      nHandle) const
{
    aValue = impl_getPathValue(nHandle);
}

::cppu::IPropertyArrayHelper& SAL_CALL PathSettings::getInfoHelper()
{
    return *m_pPropHelp;
}

css::uno::Reference< css::beans::XPropertySetInfo > SAL_CALL PathSettings::getPropertySetInfo()
    throw(css::uno::RuntimeException, std::exception)
{
    return css::uno::Reference< css::beans::XPropertySetInfo >(
            ::cppu::OPropertySetHelper::createPropertySetInfo(getInfoHelper()));
}

css::uno::Reference< css::util::XStringSubstitution > PathSettings::fa_getSubstitution()
{
    css::uno::Reference< css::util::XStringSubstitution > xSubst;
    { // SAFE ->
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);
    xSubst = m_xSubstitution;
    }

    if (! xSubst.is())
    {
        // create the needed substitution service.
        // We must replace all used variables inside readed path values.
        // In case we can't do so ... the whole office can't work really.
        // That's why it seems to be OK to throw a RuntimeException then.
        xSubst = css::util::PathSubstitution::create(m_xContext);

        { // SAFE ->
        osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);
        m_xSubstitution = xSubst;
        }
    }

    return xSubst;
}

css::uno::Reference< css::container::XNameAccess > PathSettings::fa_getCfgOld()
{
    const OUString CFG_NODE_OLD("org.openoffice.Office.Common/Path/Current");

    css::uno::Reference< css::container::XNameAccess > xCfg;
    { // SAFE ->
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);
    xCfg = m_xCfgOld;
    } // <- SAFE

    if (! xCfg.is())
    {
        xCfg = css::uno::Reference< css::container::XNameAccess >(
                   ::comphelper::ConfigurationHelper::openConfig(
                        m_xContext,
                        CFG_NODE_OLD,
                        ::comphelper::ConfigurationHelper::E_STANDARD), // not readonly! Sometimes we need write access there !!!
                   css::uno::UNO_QUERY_THROW);

        { // SAFE ->
        osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);
        m_xCfgOld = xCfg;
        }
    }

    return xCfg;
}

css::uno::Reference< css::container::XNameAccess > PathSettings::fa_getCfgNew()
{
    const OUString CFG_NODE_NEW("org.openoffice.Office.Paths/Paths");

    css::uno::Reference< css::container::XNameAccess > xCfg;
    { // SAFE ->
    osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);
    xCfg = m_xCfgNew;
    } // <- SAFE

    if (! xCfg.is())
    {
        xCfg = css::uno::Reference< css::container::XNameAccess >(
                   ::comphelper::ConfigurationHelper::openConfig(
                        m_xContext,
                        CFG_NODE_NEW,
                        ::comphelper::ConfigurationHelper::E_STANDARD),
                   css::uno::UNO_QUERY_THROW);

        { // SAFE ->
        osl::MutexGuard g(cppu::WeakComponentImplHelperBase::rBHelper.rMutex);
        m_xCfgNew = xCfg;
        m_xCfgNewListener = new WeakChangesListener(this);
        }

        css::uno::Reference< css::util::XChangesNotifier > xBroadcaster(xCfg, css::uno::UNO_QUERY_THROW);
        xBroadcaster->addChangesListener(m_xCfgNewListener);
    }

    return xCfg;
}

struct Instance {
    explicit Instance(
        css::uno::Reference<css::uno::XComponentContext> const & context):
        instance(
            static_cast<cppu::OWeakObject *>(new PathSettings(context)))
    {
        // fill cache
        static_cast<PathSettings *>(static_cast<cppu::OWeakObject *>
                (instance.get()))->impl_readAll();
    }

    css::uno::Reference<css::uno::XInterface> instance;
};

struct Singleton:
    public rtl::StaticWithArg<
        Instance, css::uno::Reference<css::uno::XComponentContext>, Singleton>
{};

}

extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface * SAL_CALL
com_sun_star_comp_framework_PathSettings_get_implementation(
    css::uno::XComponentContext *context,
    css::uno::Sequence<css::uno::Any> const &)
{
    return cppu::acquire(static_cast<cppu::OWeakObject *>(
                Singleton::get(context).instance.get()));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
