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
#ifndef INCLUDED_DBACCESS_SOURCE_UI_APP_APPVIEW_HXX
#define INCLUDED_DBACCESS_SOURCE_UI_APP_APPVIEW_HXX

#include <dbaccess/dataview.hxx>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/sdbc/XDatabaseMetaData.hpp>
#include <com/sun/star/ucb/XContent.hpp>
#include <com/sun/star/sdb/application/NamedDatabaseObject.hpp>
#include <vcl/fixed.hxx>
#include <unotools/eventlisteneradapter.hxx>
#include "IClipBoardTest.hxx"
#include "AppElementType.hxx"

namespace com{ namespace sun { namespace star { namespace beans    { class XPropertySet; } } } }

class Control;
class SvTreeListEntry;
class MnemonicGenerator;

namespace dbaui
{
    class IApplicationController;
    class OApplicationView;
    class OApplicationDetailView;
    class OApplicationSwapWindow;
    class OTitleWindow;
    class OAppBorderWindow : public Window
    {
        OTitleWindow*                       m_pPanel;
        OApplicationDetailView*             m_pDetailView;
        OApplicationView*                   m_pView;

        void ImplInitSettings();
    protected:
        // Window
        virtual void DataChanged( const DataChangedEvent& rDCEvt ) SAL_OVERRIDE;
    public:
        OAppBorderWindow(OApplicationView* _pParent,PreviewMode _ePreviewMode);
        virtual ~OAppBorderWindow();

        // window overloads
        virtual void GetFocus() SAL_OVERRIDE;
        virtual void Resize() SAL_OVERRIDE;

        OApplicationView*       getView() const { return m_pView;}
        OApplicationSwapWindow* getPanel() const;
        OApplicationDetailView* getDetailView() const { return m_pDetailView;}
    };

    class OApplicationView : public ODataView
                            ,public IClipboardTest
                            ,public ::utl::OEventListenerAdapter
    {
        enum ChildFocusState
        {
            PANELSWAP,
            DETAIL,
            NONE
        };
    private:
        ::com::sun::star::lang::Locale      m_aLocale;
        ::com::sun::star::uno::Reference< ::com::sun::star::lang::XComponent >
                                            m_xObject;
        OAppBorderWindow*                   m_pWin;
        IApplicationController&             m_rAppController;
        ChildFocusState                     m_eChildFocus;

        IClipboardTest* getActiveChild() const;

        void ImplInitSettings();
    protected:

        // return the Rectangle where I can paint myself
        virtual void resizeDocumentView(Rectangle& rRect) SAL_OVERRIDE;

        // OEventListenerAdapter
        virtual void _disposing( const ::com::sun::star::lang::EventObject& _rSource ) SAL_OVERRIDE;

        // Window
        virtual void DataChanged( const DataChangedEvent& rDCEvt ) SAL_OVERRIDE;
    public:
        OApplicationView(   Window* pParent
                            ,const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XComponentContext >&
                            ,IApplicationController&            _rAppController
                            ,PreviewMode _ePreviewMode
                            );
        virtual ~OApplicationView();

        /// automatically creates mnemonics for the icon/texts in our left hand side panel
        void    createIconAutoMnemonics( MnemonicGenerator& _rMnemonics );

        /// automatically creates mnemonics for the texts in our task pane
        void    setTaskExternalMnemonics( MnemonicGenerator& _rMnemonics );

        // window overloads
        virtual bool PreNotify( NotifyEvent& rNEvt ) SAL_OVERRIDE;
        virtual void GetFocus() SAL_OVERRIDE;

        inline IApplicationController&                  getAppController() const { return m_rAppController; }
        inline const ::com::sun::star::lang::Locale&    getLocale() const { return m_aLocale;}

        // IClipboardTest
        virtual bool isCutAllowed() SAL_OVERRIDE;
        virtual bool isCopyAllowed() SAL_OVERRIDE;
        virtual bool isPasteAllowed() SAL_OVERRIDE;
        virtual bool hasChildPathFocus() SAL_OVERRIDE { return HasChildPathFocus(); }
        virtual void copy() SAL_OVERRIDE;
        virtual void cut() SAL_OVERRIDE;
        virtual void paste() SAL_OVERRIDE;

        /// get the left panel
        inline OApplicationSwapWindow*  getPanel()      const { return m_pWin->getPanel(); }
        /// get the detail page
        inline OApplicationDetailView*  getDetailView() const { return m_pWin->getDetailView(); }

        /** return the qualified name.
            @param  _pEntry
                The entry of a table, or query, form, report to get the qualified name.
                If the entry is <NULL/>, the first selected is chosen.
            @return
                the qualified name
        */
        OUString getQualifiedName( SvTreeListEntry* _pEntry ) const;

        /** returns if an entry is a leaf
            @param _pEntry
                The entry to check
            @return
                <TRUE/> if the entry is a leaf, otherwise <FALSE/>
        */
        bool isLeaf(SvTreeListEntry* _pEntry) const;

        /** returns if one of the selected entries is a leaf
            @return
                <TRUE/> if the entry is a leaf, otherwise <FALSE/>
        */
        bool isALeafSelected() const;

        /** select all entries in the detail page
        */
        void selectAll();

        /// returns <TRUE/> if it sorts ascending
        bool isSortUp() const;

        /// sort the entries in the detail page down
        void sortDown();

        /// sort the entries in the detail page up
        void sortUp();

        /// returns <TRUE/> when a detail page was filled
        bool isFilled() const;

        /// return the element of currently select entry
        ElementType getElementType() const;

        /// returns the count of entries
        sal_Int32 getElementCount();

        /// returns the count of selected entries
        sal_Int32 getSelectionCount();

        /** clears the detail page and the selection on the left side.
            @param  _bTaskAlso
                If <TRUE/> the task window will also be cleared.
        */
        void clearPages(bool _bTaskAlso = true);

        /** returns the element names which are selected
            @param  _rNames
                The list will be filled.
        */
        void getSelectionElementNames( ::std::vector< OUString>& _rNames ) const;

        /** describes the current selection for the given control
        */
        void    describeCurrentSelectionForControl(
                    const Control& _rControl,
                    ::com::sun::star::uno::Sequence< ::com::sun::star::sdb::application::NamedDatabaseObject >& _out_rSelectedObjects
                );

        /** describes the current selection for the given ElementType
        */
        void    describeCurrentSelectionForType(
                    const ElementType _eType,
                    ::com::sun::star::uno::Sequence< ::com::sun::star::sdb::application::NamedDatabaseObject >& _out_rSelectedObjects
                );

        /** select all names on the currently selected container. Non existence names where ignored.
        *
        * \param _aNames the element names
        */
        void selectElements(const ::com::sun::star::uno::Sequence< OUString>& _aNames);

        /** adds a new object to the detail page.
            @param  _eType
                The type where the entry shold be appended.
            @param  _rName
                The name of the object to be inserted
            @param  _rObject
                The object to add.
            @param  _rxConn
                If we insert a table, the connection must be set.
        */
        SvTreeListEntry* elementAdded(ElementType _eType
                        ,const OUString& _rName
                        ,const ::com::sun::star::uno::Any& _rObject );

        /** replaces a objects name with a new one
            @param  _eType
                The type where the entry shold be appended.
            @param  _rOldName
                The old name of the object to be replaced
            @param  _rNewName
                The new name of the object to be replaced
            @param  _rxConn
                If we insert a table, the connection must be set.
            @param  _xObject
                The object which was replaced
        */
        void elementReplaced(ElementType eType
                        ,const OUString& _rOldName
                        ,const OUString& _rNewName );

        /** removes an element from the detail page.
            @param  _eType
                The type where the entry shold be appended.
            @param  _rName
                The name of the element to be removed.
            @param  _rxConn
                If we remove a table, the connection must be set.
        */
        void elementRemoved(ElementType _eType
                            ,const OUString& _rName );

        /** changes the container which should be displayed. The select handler will also be called.
            @param  _eType
                Which container to show.
        */
        void selectContainer(ElementType _eType);

        /// returns the preview mode
        PreviewMode getPreviewMode();

        /// <TRUE/> if the preview is enabled
        bool isPreviewEnabled();

        /** switches to the given preview mode
            @param  _eMode
                the mode to set for the preview
        */
        void switchPreview(PreviewMode _eMode);

        /** shows the Preview of the content when it is enabled.
            @param  _xContent
                The content which must support the "preview" command.
        */
        void showPreview(const ::com::sun::star::uno::Reference< ::com::sun::star::ucb::XContent >& _xContent);

        /** shows the Preview of a table or query
            @param  _sDataSourceName
                the name of the data source
            @param  _xConnection
                the connection which will be shared
            @param  _sName
                the name of table or query
            @param  _bTable
                <TRUE/> if it is a table, otherwise <FALSE/>
            @return void
        */
        void showPreview(   const OUString& _sDataSourceName,
                            const ::com::sun::star::uno::Reference< ::com::sun::star::sdbc::XConnection>& _xConnection,
                            const OUString& _sName,
                            bool _bTable);

        SvTreeListEntry* getEntry( const Point& _aPosPixel ) const;
    };
}
#endif // INCLUDED_DBACCESS_SOURCE_UI_APP_APPVIEW_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
