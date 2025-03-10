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

#ifndef INCLUDED_SD_SOURCE_UI_TABLE_TABLEDESIGNPANE_HXX
#define INCLUDED_SD_SOURCE_UI_TABLE_TABLEDESIGNPANE_HXX

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/ui/XUIElement.hpp>
#include <com/sun/star/ui/LayoutSize.hpp>
#include <com/sun/star/drawing/XDrawView.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>

#include <svtools/valueset.hxx>
#include <vcl/dialog.hxx>
#include <vcl/fixed.hxx>
#include <vcl/button.hxx>
#include <svx/sidebar/PanelLayout.hxx>

#include <boost/scoped_ptr.hpp>

namespace sd
{

namespace tools {
class EventMultiplexerEvent;
}

class ViewShellBase;

#define CB_HEADER_ROW       0
#define CB_TOTAL_ROW        1
#define CB_BANDED_ROWS      2
#define CB_FIRST_COLUMN     3
#define CB_LAST_COLUMN      4
#define CB_BANDED_COLUMNS   5
#define CB_COUNT CB_BANDED_COLUMNS-CB_HEADER_ROW+1

class TableValueSet : public ValueSet
{
private:
    bool m_bModal;
public:
    TableValueSet(Window *pParent, WinBits nStyle);
    virtual void Resize() SAL_OVERRIDE;
    virtual void DataChanged( const DataChangedEvent& rDCEvt ) SAL_OVERRIDE;
    void updateSettings();
    void setModal(bool bModal) { m_bModal = bModal; }
};

class TableDesignWidget
{
public:
    TableDesignWidget( VclBuilderContainer* pParent, ViewShellBase& rBase, bool bModal );
    virtual ~TableDesignWidget();

    // callbacks
    void onSelectionChanged();

    void ApplyOptions();
    void ApplyStyle();

    bool isStyleChanged() const { return mbStyleSelected; }
    bool isOptionsChanged() const { return mbOptionsChanged; }

private:
    void addListener();
    void removeListener();
    void updateControls();

    void FillDesignPreviewControl();

    DECL_LINK(EventMultiplexerListener, tools::EventMultiplexerEvent*);
    DECL_LINK(implValueSetHdl, void *);
    DECL_LINK(implCheckBoxHdl, void *);

private:
    ViewShellBase& mrBase;
    const OUString msTableTemplate;

    TableValueSet* m_pValueSet;
    CheckBox* m_aCheckBoxes[CB_COUNT];

    ::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertySet > mxSelectedTable;
    ::com::sun::star::uno::Reference< ::com::sun::star::drawing::XDrawView > mxView;
    ::com::sun::star::uno::Reference< ::com::sun::star::container::XIndexAccess > mxTableFamily;
    ::com::sun::star::uno::Reference< ::com::sun::star::ui::XUIElement > mxToolbar;

    bool mbModal;
    bool mbStyleSelected;
    bool mbOptionsChanged;
};

class TableDesignPane : public PanelLayout
{
private:
    TableDesignWidget aImpl;
public:
    TableDesignPane( ::Window* pParent, ViewShellBase& rBase )
        : PanelLayout(pParent, "TableDesignPanel",
        "modules/simpress/ui/tabledesignpanel.ui", com::sun::star::uno::Reference<css::frame::XFrame>())
        , aImpl(this, rBase, false)
    {
    }
};

class TableDesignDialog : public ModalDialog
{
private:
    TableDesignWidget aImpl;
public:
    TableDesignDialog( ::Window* pParent, ViewShellBase& rBase )
        : ModalDialog(pParent, "TableDesignDialog",
        "modules/sdraw/ui/tabledesigndialog.ui")
        , aImpl(this, rBase, true)
    {
    }
    virtual short Execute() SAL_OVERRIDE;
};

void showTableDesignDialog( ::Window*, ViewShellBase& );

}

#endif // _SD_TABLEFORMATPANE_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
