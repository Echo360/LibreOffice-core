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
#ifndef INCLUDED_SVX_SOURCE_INC_FMSHIMP_HXX
#define INCLUDED_SVX_SOURCE_INC_FMSHIMP_HXX

#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/sdbc/XResultSet.hpp>
#include <com/sun/star/sdb/XSQLQueryComposer.hpp>
#include <com/sun/star/frame/XStatusListener.hpp>
#include <com/sun/star/container/ContainerEvent.hpp>
#include <com/sun/star/container/XContainerListener.hpp>
#include <com/sun/star/awt/XControl.hpp>
#include <com/sun/star/awt/XControlContainer.hpp>
#include <com/sun/star/util/XModifyListener.hpp>
#include <com/sun/star/form/XForm.hpp>
#include <com/sun/star/form/runtime/XFormController.hpp>
#include <com/sun/star/form/XFormComponent.hpp>
#include <com/sun/star/form/NavigationBarMode.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/view/XSelectionChangeListener.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XFastPropertySet.hpp>
#include <com/sun/star/beans/XPropertyChangeListener.hpp>
#include <com/sun/star/beans/PropertyChangeEvent.hpp>
#include <com/sun/star/form/runtime/FeatureState.hpp>
#include <vcl/timer.hxx>
#include <sfx2/app.hxx>
#include <svx/svdmark.hxx>
#include <svx/fmsearch.hxx>
#include <svx/svxids.hrc>
#include <svl/lstner.hxx>

#include <sfx2/mnuitem.hxx>
#include "svx/fmtools.hxx"
#include "svx/fmsrccfg.hxx"
#include <osl/mutex.hxx>
#include <cppuhelper/component.hxx>
#include <comphelper/container.hxx>
#include <cppuhelper/compbase4.hxx>
#include <cppuhelper/compbase6.hxx>
#include <unotools/configitem.hxx>
#include "svx/dbtoolsclient.hxx"
#include "formcontrolling.hxx"
#include "fmdocumentclassification.hxx"

#include <queue>
#include <set>
#include <vector>
#include <boost/ptr_container/ptr_vector.hpp>

typedef std::vector<SdrObject*> SdrObjArray;
typedef std::vector< ::com::sun::star::uno::Reference< ::com::sun::star::form::XForm > > FmFormArray;

// catch database exceptions if they occur
#define DO_SAFE(statement) try { statement; } catch( const Exception& ) { OSL_FAIL("unhandled exception (I tried to move a cursor (or something like that).)"); }

#define GA_DISABLE_SYNC     1
#define GA_FORCE_SYNC       2
#define GA_ENABLE_SYNC      3
#define GA_SYNC_MASK        3
#define GA_DISABLE_ROCTRLR  4
#define GA_ENABLE_ROCTRLR   8


// flags for controlling the behaviour when calling loadForms
#define FORMS_LOAD          0x0000      // default: simply load
#define FORMS_SYNC          0x0000      // default: do in synchronous

#define FORMS_UNLOAD        0x0001      // unload
#define FORMS_ASYNC         0x0002      // do this async


// a class iterating through all fields of a form which are bound to a field
// sub forms are ignored, grid columns (where the grid is a direct child of the form) are included
class FmXBoundFormFieldIterator : public ::comphelper::IndexAccessIterator
{
public:
    FmXBoundFormFieldIterator(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& _rStartingPoint) : ::comphelper::IndexAccessIterator(_rStartingPoint) { }

protected:
    virtual bool ShouldHandleElement(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& _rElement) SAL_OVERRIDE;
    virtual bool ShouldStepInto(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& _rContainer) const SAL_OVERRIDE;
};

class FmFormPage;

struct FmLoadAction
{
    FmFormPage* pPage;
    ImplSVEvent * nEventId;
    sal_uInt16  nFlags;

    FmLoadAction( ) : pPage( NULL ), nEventId( 0 ), nFlags( 0 ) { }
    FmLoadAction( FmFormPage* _pPage, sal_uInt16 _nFlags, ImplSVEvent * _nEventId )
        :pPage( _pPage ), nEventId( _nEventId ), nFlags( _nFlags )
    {
    }
};


class SfxViewFrame;
typedef ::cppu::WeakComponentImplHelper4<   ::com::sun::star::beans::XPropertyChangeListener
                                        ,   ::com::sun::star::container::XContainerListener
                                        ,   ::com::sun::star::view::XSelectionChangeListener
                                        ,   ::com::sun::star::form::XFormControllerListener
                                        >   FmXFormShell_BD_BASE;


class FmXFormShell_Base_Disambiguation : public FmXFormShell_BD_BASE
{
    using ::com::sun::star::beans::XPropertyChangeListener::disposing;
protected:
    FmXFormShell_Base_Disambiguation( ::osl::Mutex& _rMutex );
    virtual void SAL_CALL disposing() SAL_OVERRIDE;
};


namespace svx
{
    class FmTextControlShell;
}


typedef FmXFormShell_Base_Disambiguation    FmXFormShell_BASE;
typedef ::utl::ConfigItem                   FmXFormShell_CFGBASE;

struct SdrViewEvent;
class FmFormShell;
class FmFormView;
class FmFormObj;
class SVX_DLLPUBLIC FmXFormShell   : public FmXFormShell_BASE
                                    ,public FmXFormShell_CFGBASE
                                    ,public ::svxform::OStaticDataAccessTools
                                    ,public ::svx::IControllerFeatureInvalidation
{
    friend class FmFormView;
    friend class FmXFormView;

    class SuspendPropertyTracking;
    friend class SuspendPropertyTracking;

    // Timer um verzoegerte Markierung vorzunehmen
    Timer               m_aMarkTimer;
    SdrObjArray         m_arrSearchedControls;
        // We enable a permanent cursor for the grid we found a searched text, it's disabled in the next "found" event.
    FmFormArray         m_aSearchForms;

    struct SAL_DLLPRIVATE InvalidSlotInfo {
        sal_uInt16 id;
        sal_uInt8   flags;
        inline InvalidSlotInfo(sal_uInt16 slotId, sal_uInt8 flgs) : id(slotId), flags(flgs) {};
    };
    std::vector<InvalidSlotInfo> m_arrInvalidSlots;
        // we explicitly switch off the propbrw before leaving the design mode
        // this flag tells us if we have to switch it on again when reentering

    ::osl::Mutex    m_aAsyncSafety;
        // secure the access to our thread related members
    ::osl::Mutex    m_aInvalidationSafety;
        // secure the access to all our slot invalidation related members

    ::com::sun::star::form::NavigationBarMode   m_eNavigate;                // Art der Navigation

        // da ich beim Suchen fuer die Behandlung des "gefunden" ein SdrObject markieren will, besorge ich mir vor dem
        // Hochreissen des Suchen-Dialoges alle relevanten Objekte
        // (das Array ist damit auch nur waehrend des Suchvorganges gueltig)
    std::vector<long> m_arrRelativeGridColumn;

    ::osl::Mutex    m_aMutex;
    ImplSVEvent *   m_nInvalidationEvent;
    ImplSVEvent *   m_nActivationEvent;
    ::std::queue< FmLoadAction >
                    m_aLoadingPages;

    FmFormShell*                m_pShell;
    ::svx::FmTextControlShell*  m_pTextShell;

    ::svx::ControllerFeatures   m_aActiveControllerFeatures;
    ::svx::ControllerFeatures   m_aNavControllerFeatures;

    // aktuelle Form, Controller
    // nur im alive mode verfuegbar
    ::com::sun::star::uno::Reference< ::com::sun::star::form::runtime::XFormController >    m_xActiveController;
    ::com::sun::star::uno::Reference< ::com::sun::star::form::runtime::XFormController >    m_xNavigationController;
    ::com::sun::star::uno::Reference< ::com::sun::star::form::XForm >                       m_xActiveForm;

    // Aktueller container einer Page
    // nur im designmode verfuegbar
    ::com::sun::star::uno::Reference< ::com::sun::star::container::XIndexAccess> m_xForms;

    // the currently selected objects, as to be displayed in the property browser
    InterfaceBag                                                                m_aCurrentSelection;
    /// the currently selected form, or the form which all currently selected controls belong to, or <NULL/>
    ::com::sun::star::uno::Reference< ::com::sun::star::form::XForm >           m_xCurrentForm;
    /// the last selection/marking of controls only. Necessary to implement the "Control properties" slot
    InterfaceBag                                                                m_aLastKnownMarkedControls;


        // und das ist ebenfalls fuer's 'gefunden' : Beim Finden in GridControls brauche ich die Spalte, bekomme aber
        // nur die Nummer des Feldes, die entspricht der Nummer der Spalte + <offset>, wobei der Offset von der Position
        // des GridControls im Formular abhaengt. Also hier eine Umrechnung.
    ::com::sun::star::uno::Reference< ::com::sun::star::awt::XControlModel>         m_xLastGridFound;
     // the frame we live in
    ::com::sun::star::uno::Reference< ::com::sun::star::frame::XFrame>              m_xAttachedFrame;
    // Administration of external form views (see the SID_FM_VIEW_AS_GRID-slot)
    ::com::sun::star::uno::Reference< ::com::sun::star::frame::XController >                m_xExternalViewController;      // the controller for the external form view
    ::com::sun::star::uno::Reference< ::com::sun::star::form::runtime::XFormController >    m_xExtViewTriggerController;    // the nav controller at the time the external display was triggered
    ::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XResultSet >                  m_xExternalDisplayedForm;       // the form which the external view is based on

    mutable ::svxform::DocumentType
                    m_eDocumentType;        /// the type of document we're living in
    sal_Int16       m_nLockSlotInvalidation;
    bool        m_bHadPropertyBrowserInDesignMode : 1;

    bool        m_bTrackProperties  : 1;
        // soll ich (bzw. der Owner diese Impl-Klasse) mich um die Aktualisierung des ::com::sun::star::beans::Property-Browsers kuemmern ?

    bool        m_bUseWizards : 1;

    bool        m_bDatabaseBar      : 1;    // Gibt es eine Datenbankleiste
    bool        m_bInActivate       : 1;    // Wird ein Controller aktiviert
    bool        m_bSetFocus         : 1;    // Darf der Focus umgesetzt werden
    bool        m_bFilterMode       : 1;    // Wird gerade ein Filter auf die Controls angesetzt
    bool        m_bChangingDesignMode:1;    // sal_True within SetDesignMode
    bool        m_bPreparedClose    : 1;    // for the current modification state of the current form
                                                //  PrepareClose had been called and the user denied to save changes
    bool        m_bFirstActivation  : 1;    // has the shell ever been activated?

public:
    // attribute access
    SAL_DLLPRIVATE inline const ::com::sun::star::uno::Reference< ::com::sun::star::frame::XFrame >&
                getHostFrame() const { return m_xAttachedFrame; }
    SAL_DLLPRIVATE inline const ::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XResultSet >&
                getExternallyDisplayedForm() const { return m_xExternalDisplayedForm; }

    SAL_DLLPRIVATE inline bool
                didPrepareClose() const { return m_bPreparedClose; }
    SAL_DLLPRIVATE inline void
                didPrepareClose( bool _bDid ) { m_bPreparedClose = _bDid; }

public:
    SAL_DLLPRIVATE FmXFormShell(FmFormShell& _rShell, SfxViewFrame* _pViewFrame);

    // UNO Anbindung
    DECLARE_UNO3_DEFAULTS(FmXFormShell, FmXFormShell_BASE)
    SAL_DLLPRIVATE virtual ::com::sun::star::uno::Any SAL_CALL queryInterface( const ::com::sun::star::uno::Type& type) throw ( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;

protected:
    SAL_DLLPRIVATE virtual ~FmXFormShell();

// XTypeProvider
    SAL_DLLPRIVATE virtual ::com::sun::star::uno::Sequence< sal_Int8 > SAL_CALL getImplementationId() throw(::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;
    SAL_DLLPRIVATE ::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type > SAL_CALL getTypes(  ) throw(::com::sun::star::uno::RuntimeException, std::exception) SAL_OVERRIDE;

// EventListener
    SAL_DLLPRIVATE virtual void SAL_CALL disposing(const ::com::sun::star::lang::EventObject& Source) throw( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;

// ::com::sun::star::container::XContainerListener
    SAL_DLLPRIVATE virtual void SAL_CALL elementInserted(const ::com::sun::star::container::ContainerEvent& rEvent) throw( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;
    SAL_DLLPRIVATE virtual void SAL_CALL elementReplaced(const ::com::sun::star::container::ContainerEvent& rEvent) throw( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;
    SAL_DLLPRIVATE virtual void SAL_CALL elementRemoved(const ::com::sun::star::container::ContainerEvent& rEvent) throw( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;

// XSelectionChangeListener
    SAL_DLLPRIVATE virtual void SAL_CALL selectionChanged(const ::com::sun::star::lang::EventObject& rEvent) throw( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;

// ::com::sun::star::beans::XPropertyChangeListener
    SAL_DLLPRIVATE virtual void SAL_CALL propertyChange(const ::com::sun::star::beans::PropertyChangeEvent& evt) throw( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;

// ::com::sun::star::form::XFormControllerListener
    SAL_DLLPRIVATE virtual void SAL_CALL formActivated(const ::com::sun::star::lang::EventObject& rEvent) throw( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;
    SAL_DLLPRIVATE virtual void SAL_CALL formDeactivated(const ::com::sun::star::lang::EventObject& rEvent) throw( ::com::sun::star::uno::RuntimeException, std::exception ) SAL_OVERRIDE;

// OComponentHelper
    SAL_DLLPRIVATE virtual void SAL_CALL disposing() SAL_OVERRIDE;

public:
    SAL_DLLPRIVATE void EnableTrackProperties( bool bEnable) { m_bTrackProperties = bEnable; }
    SAL_DLLPRIVATE bool IsTrackPropertiesEnabled() {return m_bTrackProperties;}

    // activation handling
    SAL_DLLPRIVATE void        viewActivated( FmFormView& _rCurrentView, bool _bSyncAction = false );
    SAL_DLLPRIVATE void        viewDeactivated( FmFormView& _rCurrentView, bool _bDeactivateController = true );

    // IControllerFeatureInvalidation
    SAL_DLLPRIVATE virtual void invalidateFeatures( const ::std::vector< sal_Int32 >& _rFeatures ) SAL_OVERRIDE;

    SAL_DLLPRIVATE void ExecuteTabOrderDialog(         // execute SID_FM_TAB_DIALOG
        const ::com::sun::star::uno::Reference< ::com::sun::star::awt::XTabControllerModel >& _rxForForm
    );

    // stuff
    SAL_DLLPRIVATE void AddElement(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& Element);
    SAL_DLLPRIVATE void RemoveElement(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& Element);

    /** updates m_xForms, to be either <NULL/>, if we're in alive mode, or our current page's forms collection,
        if in design mode
    */
    SAL_DLLPRIVATE void UpdateForms( bool _bInvalidate );

    SAL_DLLPRIVATE void ExecuteSearch();               // execute SID_FM_SEARCH
    SAL_DLLPRIVATE void CreateExternalView();          // execute SID_FM_VIEW_AS_GRID

    SAL_DLLPRIVATE bool        GetY2KState(sal_uInt16& n);
    SAL_DLLPRIVATE void        SetY2KState(sal_uInt16 n);

protected:
    // activation handling
    SAL_DLLPRIVATE inline  bool    hasEverBeenActivated( ) const { return !m_bFirstActivation; }
    SAL_DLLPRIVATE inline  void        setHasBeenActivated( ) { m_bFirstActivation = false; }

    // form handling
    /// load or unload the forms on a page
    SAL_DLLPRIVATE         void        loadForms( FmFormPage* _pPage, const sal_uInt16 _nBehaviour = FORMS_LOAD | FORMS_SYNC );
    SAL_DLLPRIVATE         void        smartControlReset( const ::com::sun::star::uno::Reference< ::com::sun::star::container::XIndexAccess >& _rxModels );


    SAL_DLLPRIVATE void startListening();
    SAL_DLLPRIVATE void stopListening();

    SAL_DLLPRIVATE ::com::sun::star::uno::Reference< ::com::sun::star::awt::XControl >
        impl_getControl(
            const ::com::sun::star::uno::Reference< ::com::sun::star::awt::XControlModel>& i_rxModel,
            const FmFormObj& i_rKnownFormObj
        );

    // sammelt in strNames die Namen aller Formulare
    SAL_DLLPRIVATE static void impl_collectFormSearchContexts_nothrow(
        const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& _rxStartingPoint,
        const OUString& _rCurrentLevelPrefix,
        FmFormArray& _out_rForms,
        ::std::vector< OUString >& _out_rNames );

    /** checks whether the instance is already disposed, if so, this is reported as assertion error (debug
        builds only) and <TRUE/> is returned.
    */
    SAL_DLLPRIVATE bool    impl_checkDisposed() const;

public:
    // methode fuer nicht designmode (alive mode)
    SAL_DLLPRIVATE void setActiveController( const ::com::sun::star::uno::Reference< ::com::sun::star::form::runtime::XFormController>& _xController, bool _bNoSaveOldContent = false );
    SAL_DLLPRIVATE const ::com::sun::star::uno::Reference< ::com::sun::star::form::runtime::XFormController>& getActiveController() const {return m_xActiveController;}
    SAL_DLLPRIVATE const ::com::sun::star::uno::Reference< ::com::sun::star::form::runtime::XFormController>& getActiveInternalController() const { return m_xActiveController == m_xExternalViewController ? m_xExtViewTriggerController : m_xActiveController; }
    SAL_DLLPRIVATE const ::com::sun::star::uno::Reference< ::com::sun::star::form::XForm>& getActiveForm() const {return m_xActiveForm;}
    SAL_DLLPRIVATE const ::com::sun::star::uno::Reference< ::com::sun::star::form::runtime::XFormController>& getNavController() const {return m_xNavigationController;}

    SAL_DLLPRIVATE inline const ::svx::ControllerFeatures& getActiveControllerFeatures() const
        { return m_aActiveControllerFeatures; }
    SAL_DLLPRIVATE inline const ::svx::ControllerFeatures& getNavControllerFeatures() const
        { return m_aNavControllerFeatures.isAssigned() ? m_aNavControllerFeatures : m_aActiveControllerFeatures; }

    /** announces a new "current selection"
        @return
            <TRUE/> if and only if the to-bet-set selection was different from the previous selection
    */
    SAL_DLLPRIVATE bool    setCurrentSelection( const InterfaceBag& _rSelection );

    /** sets the new selection to the last known marked controls
    */
    SAL_DLLPRIVATE bool    selectLastMarkedControls();

    /** retrieves the current selection
    */
    void    getCurrentSelection( InterfaceBag& /* [out] */ _rSelection ) const;

    /** sets a new current selection as indicated by a mark list
        @return
            <TRUE/> if and only if the to-bet-set selection was different from the previous selection
    */
    SAL_DLLPRIVATE bool    setCurrentSelectionFromMark(const SdrMarkList& rMarkList);

    /// returns the currently selected form, or the form which all currently selected controls belong to, or <NULL/>
    SAL_DLLPRIVATE ::com::sun::star::uno::Reference< ::com::sun::star::form::XForm >
                getCurrentForm() const { return m_xCurrentForm; }
    SAL_DLLPRIVATE void        forgetCurrentForm();
    /// returns whether the last known marking contained only controls
    SAL_DLLPRIVATE bool    onlyControlsAreMarked() const { return !m_aLastKnownMarkedControls.empty(); }

    /// determines whether the current selection consists of exactly the given object
    SAL_DLLPRIVATE bool    isSolelySelected(
                const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface >& _rxObject
            );

    /// handles a MouseButtonDown event of the FmFormView
    SAL_DLLPRIVATE void handleMouseButtonDown( const SdrViewEvent& _rViewEvent );
    /// handles the request for showing the "Properties"
    SAL_DLLPRIVATE void handleShowPropertiesRequest();

    SAL_DLLPRIVATE bool hasForms() const {return m_xForms.is() && m_xForms->getCount() != 0;}
    SAL_DLLPRIVATE bool hasDatabaseBar() const {return m_bDatabaseBar;}
    SAL_DLLPRIVATE bool canNavigate() const    {return m_xNavigationController.is();}

    SAL_DLLPRIVATE void ShowSelectionProperties( bool bShow );
    SAL_DLLPRIVATE bool IsPropBrwOpen() const;

    SAL_DLLPRIVATE void DetermineSelection(const SdrMarkList& rMarkList);
    SAL_DLLPRIVATE void SetSelection(const SdrMarkList& rMarkList);
    SAL_DLLPRIVATE void SetSelectionDelayed();

    SAL_DLLPRIVATE void SetDesignMode(bool bDesign);

    SAL_DLLPRIVATE bool    GetWizardUsing() const { return m_bUseWizards; }
    SAL_DLLPRIVATE void    SetWizardUsing(bool _bUseThem);

        // Setzen des Filtermodus
    SAL_DLLPRIVATE bool isInFilterMode() const {return m_bFilterMode;}
    SAL_DLLPRIVATE void startFiltering();
    SAL_DLLPRIVATE void stopFiltering(bool bSave);

    SAL_DLLPRIVATE static PopupMenu* GetConversionMenu();
        // ein Menue, das alle ControlConversion-Eintraege enthaelt

    /// checks whethere a given control conversion slot can be applied to the current selection
    SAL_DLLPRIVATE        bool canConvertCurrentSelectionToControl( sal_Int16 nConversionSlot );
    /// enables or disables all conversion slots in a menu, according to the current selection
    SAL_DLLPRIVATE        void checkControlConversionSlotsForCurrentSelection( Menu& rMenu );
    /// executes a control conversion slot for a given object
    SAL_DLLPRIVATE        bool executeControlConversionSlot( const ::com::sun::star::uno::Reference< ::com::sun::star::form::XFormComponent >& _rxObject, sal_uInt16 _nSlotId );
    /** executes a control conversion slot for the current selection
        @precond canConvertCurrentSelectionToControl( <arg>_nSlotId</arg> ) must return <TRUE/>
    */
    SAL_DLLPRIVATE        bool executeControlConversionSlot( sal_uInt16 _nSlotId );
    /// checks whether the given slot id denotes a control conversion slot
    SAL_DLLPRIVATE static bool isControlConversionSlot( sal_uInt16 _nSlotId );

    SAL_DLLPRIVATE void    ExecuteTextAttribute( SfxRequest& _rReq );
    SAL_DLLPRIVATE void    GetTextAttributeState( SfxItemSet& _rSet );
    SAL_DLLPRIVATE bool    IsActiveControl( bool _bCountRichTextOnly = false ) const;
    SAL_DLLPRIVATE void    ForgetActiveControl();
    SAL_DLLPRIVATE void    SetControlActivationHandler( const Link& _rHdl );

    /// classifies our host document
    SAL_DLLPRIVATE ::svxform::DocumentType
            getDocumentType() const;
    SAL_DLLPRIVATE bool    isEnhancedForm() const;

    /// determines whether our host document is currently read-only
    SAL_DLLPRIVATE bool    IsReadonlyDoc() const;

    // das Setzen des curObject/selObject/curForm erfolgt verzoegert (SetSelectionDelayed), mit den folgenden
    // Funktionen laesst sich das abfragen/erzwingen
    SAL_DLLPRIVATE inline bool IsSelectionUpdatePending();
    SAL_DLLPRIVATE void        ForceUpdateSelection(bool bLockInvalidation);

    SAL_DLLPRIVATE ::com::sun::star::uno::Reference< ::com::sun::star::frame::XModel>          getContextDocument() const;
    SAL_DLLPRIVATE ::com::sun::star::uno::Reference< ::com::sun::star::form::XForm>            getInternalForm(const ::com::sun::star::uno::Reference< ::com::sun::star::form::XForm>& _xForm) const;
    SAL_DLLPRIVATE ::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XResultSet>       getInternalForm(const ::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XResultSet>& _xForm) const;
        // if the form belongs to the controller (extern) displaying a grid, the according internal form will
        // be displayed, _xForm else

    // check if the current control of the active controller has the focus
    SAL_DLLPRIVATE bool    HasControlFocus() const;

private:
    DECL_DLLPRIVATE_LINK(OnFoundData, FmFoundRecordInformation*);
    DECL_DLLPRIVATE_LINK(OnCanceledNotFound, FmFoundRecordInformation*);
    DECL_DLLPRIVATE_LINK(OnSearchContextRequest, FmSearchContext*);
    DECL_DLLPRIVATE_LINK(OnTimeOut, void*);
    DECL_DLLPRIVATE_LINK(OnFirstTimeActivation, void*);
    DECL_DLLPRIVATE_LINK(OnFormsCreated, FmFormPage*);

    SAL_DLLPRIVATE void LoopGrids(sal_Int16 nWhat);

    // Invalidierung von Slots
    SAL_DLLPRIVATE void    InvalidateSlot( sal_Int16 nId, bool bWithId );
    SAL_DLLPRIVATE void    UpdateSlot( sal_Int16 nId );
    // Locking der Invalidierung - wenn der interne Locking-Counter auf 0 geht, werden alle aufgelaufenen Slots
    // (asynchron) invalidiert
    SAL_DLLPRIVATE void    LockSlotInvalidation(bool bLock);

    DECL_DLLPRIVATE_LINK(OnInvalidateSlots, void*);

    SAL_DLLPRIVATE void    CloseExternalFormViewer();
        // closes the task-local beamer displaying a grid view for a form

    // ConfigItem related stuff
    SAL_DLLPRIVATE virtual void Notify( const com::sun::star::uno::Sequence< OUString >& _rPropertyNames) SAL_OVERRIDE;
    SAL_DLLPRIVATE virtual void Commit() SAL_OVERRIDE;
    SAL_DLLPRIVATE void implAdjustConfigCache();

    SAL_DLLPRIVATE ::com::sun::star::uno::Reference< ::com::sun::star::awt::XControlContainer >
            getControlContainerForView();

    /** finds and sets a default for m_xCurrentForm, if it is currently NULL
    */
    SAL_DLLPRIVATE void    impl_defaultCurrentForm_nothrow();

    /** sets m_xCurrentForm to the provided form, and udpates everything which
        depends on the current form
    */
    SAL_DLLPRIVATE void    impl_updateCurrentForm( const ::com::sun::star::uno::Reference< ::com::sun::star::form::XForm >& _rxNewCurForm );

    /** adds or removes ourself as XEventListener at m_xActiveController
    */
    SAL_DLLPRIVATE void    impl_switchActiveControllerListening( const bool _bListen );

    /** add an element
    */
    SAL_DLLPRIVATE void    impl_AddElement_nothrow(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& Element);

    /** remove an element
    */
    SAL_DLLPRIVATE void    impl_RemoveElement_nothrow(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& Element);


    // asyncronous cursor actions/navigation slot handling

public:
    /** execute the given form slot
        <p>Warning. Only a small set of slots implemented currently.</p>
        @param _nSlot
            the slot to execute
    */
    SAL_DLLPRIVATE void    ExecuteFormSlot( sal_Int32 _nSlot );

    /** determines whether the current form slot is currently enabled
    */
    SAL_DLLPRIVATE bool    IsFormSlotEnabled( sal_Int32 _nSlot, ::com::sun::star::form::runtime::FeatureState* _pCompleteState = NULL );

protected:
    DECL_DLLPRIVATE_LINK( OnLoadForms, FmFormPage* );
};


inline bool FmXFormShell::IsSelectionUpdatePending()
{
    return m_aMarkTimer.IsActive();
}


// = ein Iterator, der ausgehend von einem Interface ein Objekt sucht, dessen
// = ::com::sun::star::beans::Property-Set eine ControlSource- sowie eine BoundField-Eigenschaft hat,
// = wobei letztere einen Wert ungleich NULL haben muss.
// = Wenn das Interface selber diese Bedingung nicht erfuellt, wird getestet,
// = ob es ein Container ist (also ueber eine ::com::sun::star::container::XIndexAccess verfuegt), dann
// = wird dort abgestiegen und fuer jedes Element des Containers das selbe
// = versucht (wiederum eventuell mit Abstieg).
// = Wenn irgendein Objekt dabei die geforderte Eigenschaft hat, entfaellt
// = der Teil mit dem Container-Test fuer dieses Objekt.
// =

class SearchableControlIterator : public ::comphelper::IndexAccessIterator
{
    OUString         m_sCurrentValue;
        // der aktuelle Wert der ControlSource-::com::sun::star::beans::Property

public:
    OUString     getCurrentValue() const { return m_sCurrentValue; }

public:
    SearchableControlIterator(::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface> xStartingPoint);

    virtual bool ShouldHandleElement(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& rElement) SAL_OVERRIDE;
    virtual bool ShouldStepInto(const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface>& xContainer) const SAL_OVERRIDE;
    virtual void Invalidate() SAL_OVERRIDE { IndexAccessIterator::Invalidate(); m_sCurrentValue = OUString(); }
};


typedef boost::ptr_vector<SfxStatusForwarder> StatusForwarderArray;
class SVX_DLLPUBLIC ControlConversionMenuController : public SfxMenuControl
{
protected:
    StatusForwarderArray    m_aStatusForwarders;
    Menu*                   m_pMainMenu;
    PopupMenu*              m_pConversionMenu;

public:
    SVX_DLLPRIVATE ControlConversionMenuController(sal_uInt16 nId, Menu& rMenu, SfxBindings& rBindings);
    SVX_DLLPRIVATE virtual ~ControlConversionMenuController();
    SFX_DECL_MENU_CONTROL();

    SVX_DLLPRIVATE virtual void StateChanged(sal_uInt16 nSID, SfxItemState eState, const SfxPoolItem* pState) SAL_OVERRIDE;
};

#endif // INCLUDED_SVX_SOURCE_INC_FMSHIMP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
