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

#include <stdio.h>
#include <stdlib.h>
#include <tools/config.hxx>
#include <vcl/dibtools.hxx>
#include <vcl/layout.hxx>
#include <vcl/msgbox.hxx>
#include <sanedlg.hxx>
#include <grid.hxx>
#include <math.h>
#include <sal/macros.h>
#include <rtl/strbuf.hxx>
#include <boost/scoped_array.hpp>

#define PREVIEW_WIDTH       113
#define PREVIEW_HEIGHT      160

class ScanPreview : public Window
{
private:
    enum DragDirection { TopLeft, Top, TopRight, Right, BottomRight, Bottom,
                         BottomLeft, Left };

    Bitmap    maPreviewBitmap;
    Rectangle maPreviewRect;
    Point     maTopLeft, maBottomRight;
    Point     maMinTopLeft, maMaxBottomRight;
    SaneDlg*  mpParentDialog;
    DragDirection meDragDirection;
    bool      mbDragEnable;
    bool      mbDragDrawn;
    bool      mbIsDragging;

    void DrawRectangles(Point& rUL, Point& rBR);
public:
    ScanPreview(Window* pParent, WinBits nStyle)
        : Window(pParent, nStyle)
        , maMaxBottomRight(PREVIEW_WIDTH,  PREVIEW_HEIGHT)
        , mpParentDialog(NULL)
        , meDragDirection(TopLeft)
        , mbDragEnable(false)
        , mbDragDrawn(false)
        , mbIsDragging(false)
    {
    }
    void Init(SaneDlg *pParent)
    {
        mpParentDialog = pParent;
    }
    void ResetForNewScanner()
    {
        maTopLeft = Point();
        maBottomRight = Point();
        maMinTopLeft = Point();
        maMaxBottomRight = Point(PREVIEW_WIDTH,  PREVIEW_HEIGHT);
    }
    void EnableDrag() { mbDragEnable = true; }
    void DisableDrag() { mbDragEnable = false; }
    bool IsDragEnabled() { return mbDragEnable; }
    virtual void Paint(const Rectangle& rRect) SAL_OVERRIDE;
    virtual void MouseButtonDown(const MouseEvent& rMEvt) SAL_OVERRIDE;
    virtual void MouseMove(const MouseEvent& rMEvt) SAL_OVERRIDE;
    virtual void MouseButtonUp(const MouseEvent& rMEvt) SAL_OVERRIDE;
    Point GetPixelPos(const Point& rIn) const;
    Point GetLogicPos(const Point& rIn) const;
    void GetPreviewLogicRect(Point& rTopLeft, Point &rBottomRight) const
    {
        rTopLeft = GetLogicPos(maTopLeft);
        rBottomRight = GetLogicPos(maBottomRight);
    }
    void GetMaxLogicRect(Point& rTopLeft, Point &rBottomRight) const
    {
        rTopLeft = maMinTopLeft;
        rBottomRight = maMaxBottomRight;

    }
    void ChangePreviewLogicTopLeftY(long Y)
    {
        Point aPoint(0, Y);
        aPoint = GetPixelPos(aPoint);
        maTopLeft.Y() = aPoint.Y();
    }
    void ChangePreviewLogicTopLeftX(long X)
    {
        Point aPoint(X, 0);
        aPoint = GetPixelPos(aPoint);
        maTopLeft.X() = aPoint.X();
    }
    void ChangePreviewLogicBottomRightY(long Y)
    {
        Point aPoint(0, Y);
        aPoint = GetPixelPos(aPoint);
        maBottomRight.Y() = aPoint.Y();
    }
    void ChangePreviewLogicBottomRightX(long X)
    {
        Point aPoint(X, 0);
        aPoint = GetPixelPos(aPoint);
        maBottomRight.X() = aPoint.X();
    }
    void SetPreviewLogicRect(const Point& rTopLeft, const Point &rBottomRight)
    {
        maTopLeft = GetPixelPos(rTopLeft);
        maBottomRight = GetPixelPos(rBottomRight);
        maPreviewRect = Rectangle( maTopLeft,
                                   Size( maBottomRight.X() - maTopLeft.X(),
                                         maBottomRight.Y() - maTopLeft.Y() )
                                   );
    }
    void SetPreviewMaxRect(const Point& rTopLeft, const Point &rBottomRight)
    {
        maMinTopLeft = rTopLeft;
        maMaxBottomRight = rBottomRight;
    }
    void DrawDrag();
    void UpdatePreviewBounds();
    void SetBitmap(SvStream &rStream)
    {
        ReadDIB(maPreviewBitmap, rStream, true);
    }
    virtual Size GetOptimalSize() const SAL_OVERRIDE
    {
        Size aSize(LogicToPixel(Size(PREVIEW_WIDTH, PREVIEW_HEIGHT), MAP_APPFONT));
        aSize.setWidth(aSize.getWidth()+1);
        aSize.setHeight(aSize.getHeight()+1);
        return aSize;
    }
};

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeScanPreview(Window *pParent, VclBuilder::stringmap &rMap)
{
    WinBits nWinStyle = 0;
    OString sBorder = VclBuilder::extractCustomProperty(rMap);
    if (!sBorder.isEmpty())
        nWinStyle |= WB_BORDER;
    ScanPreview *pWindow = new ScanPreview(pParent, nWinStyle);
    return pWindow;
}

SaneDlg::SaneDlg( Window* pParent, Sane& rSane, bool bScanEnabled ) :
        ModalDialog(pParent, "SaneDialog", "modules/scanner/ui/sanedialog.ui"),
        mrSane( rSane ),
        mbScanEnabled( bScanEnabled ),
        mnCurrentOption(0),
        mnCurrentElement(0),
        mpRange(0),
        mfMin(0.0),
        mfMax(0.0),
        doScan(false)
{
    get(mpOKButton, "ok");
    get(mpCancelButton, "cancel");
    get(mpDeviceInfoButton, "deviceInfoButton");
    get(mpPreviewButton, "previewButton");
    get(mpScanButton, "scanButton");
    get(mpButtonOption, "optionsButton");
    get(mpOptionTitle, "optionTitleLabel");
    Size aSize(LogicToPixel(Size(130, 102), MAP_APPFONT));
    mpOptionTitle->set_width_request(aSize.Width());
    mpOptionTitle->set_height_request(aSize.Height() / 2);
    get(mpOptionDescTxt, "optionsDescLabel");
    get(mpVectorTxt, "vectorLabel");
    get(mpLeftField, "leftSpinbutton");
    get(mpTopField, "topSpinbutton");
    get(mpRightField, "rightSpinbutton");
    get(mpBottomField, "bottomSpinbutton");
    get(mpDeviceBox, "deviceCombobox");
    get(mpReslBox, "reslCombobox");
    get(mpAdvancedBox, "advancedCheckbutton");
    get(mpVectorBox, "vectorSpinbutton-nospin");
    get(mpQuantumRangeBox, "quantumRangeCombobox");
    get(mpStringRangeBox, "stringRangeCombobox");
    get(mpStringEdit, "stringEntry");
    get(mpNumericEdit, "numericEntry");
    get(mpOptionBox, "optionSvTreeListBox");
    mpOptionBox->set_width_request(aSize.Width());
    mpOptionBox->set_height_request(aSize.Height());
    get(mpBoolCheckBox, "boolCheckbutton");
    get(mpPreview, "preview");
    mpPreview->Init(this);
    if( Sane::IsSane() )
    {
        InitDevices(); // opens first sane device
        DisableOption();
        InitFields();
    }

    mpDeviceInfoButton->SetClickHdl( LINK( this, SaneDlg, ClickBtnHdl ) );
    mpPreviewButton->SetClickHdl( LINK( this, SaneDlg, ClickBtnHdl ) );
    mpScanButton->SetClickHdl( LINK( this, SaneDlg, ClickBtnHdl ) );
    mpButtonOption->SetClickHdl( LINK( this, SaneDlg, ClickBtnHdl ) );
    mpDeviceBox->SetSelectHdl( LINK( this, SaneDlg, SelectHdl ) );
    mpOptionBox->SetSelectHdl( LINK( this, SaneDlg, OptionsBoxSelectHdl ) );
    mpOKButton->SetClickHdl( LINK( this, SaneDlg, ClickBtnHdl ) );
    mpCancelButton->SetClickHdl( LINK( this, SaneDlg, ClickBtnHdl ) );
    mpBoolCheckBox->SetClickHdl( LINK( this, SaneDlg, ClickBtnHdl ) );
    mpStringEdit->SetModifyHdl( LINK( this, SaneDlg, ModifyHdl ) );
    mpNumericEdit->SetModifyHdl( LINK( this, SaneDlg, ModifyHdl ) );
    mpVectorBox->SetModifyHdl( LINK( this, SaneDlg, ModifyHdl ) );
    mpReslBox->SetModifyHdl( LINK( this, SaneDlg, ModifyHdl ) );
    mpStringRangeBox->SetSelectHdl( LINK( this, SaneDlg, SelectHdl ) );
    mpQuantumRangeBox->SetSelectHdl( LINK( this, SaneDlg, SelectHdl ) );
    mpLeftField->SetModifyHdl( LINK( this, SaneDlg, ModifyHdl ) );
    mpRightField->SetModifyHdl( LINK( this, SaneDlg, ModifyHdl ) );
    mpTopField->SetModifyHdl( LINK( this, SaneDlg, ModifyHdl ) );
    mpBottomField->SetModifyHdl( LINK( this, SaneDlg, ModifyHdl ) );
    mpAdvancedBox->SetClickHdl( LINK( this, SaneDlg, ClickBtnHdl ) );

    maOldLink = mrSane.SetReloadOptionsHdl( LINK( this, SaneDlg, ReloadSaneOptionsHdl ) );

    mpOptionBox->SetNodeBitmaps(get<FixedImage>("plus")->GetImage(),
        get<FixedImage>("minus")->GetImage());
    mpOptionBox->SetStyle( mpOptionBox->GetStyle()|
                          WB_HASLINES           |
                          WB_HASBUTTONS         |
                          WB_NOINITIALSELECTION |
                          WB_HASBUTTONSATROOT   |
                          WB_HASLINESATROOT
                        );
}

SaneDlg::~SaneDlg()
{
    mrSane.SetReloadOptionsHdl( maOldLink );
}

short SaneDlg::Execute()
{
    if( ! Sane::IsSane() )
    {
        ErrorBox aErrorBox( NULL, WB_OK | WB_DEF_OK,
                            "The SANE interface could not be initialized. Scanning is not possible." );
        aErrorBox.Execute();
        return sal_False;
    }
    LoadState();
    return ModalDialog::Execute();
}

void SaneDlg::InitDevices()
{
    if( ! Sane::IsSane() )
        return;

    if( mrSane.IsOpen() )
        mrSane.Close();
    mrSane.ReloadDevices();
    mpDeviceBox->Clear();
    for (int i = 0; i < Sane::CountDevices(); ++i)
        mpDeviceBox->InsertEntry(Sane::GetName(i));
    if( Sane::CountDevices() )
    {
        mrSane.Open(0);
        mpDeviceBox->SelectEntryPos(0);
    }
}

void SaneDlg::InitFields()
{
    if( ! Sane::IsSane() )
        return;

    int nOption, i, nValue;
    double fValue;
    bool bSuccess = false;
    const char *ppSpecialOptions[] = {
        "resolution",
        "tl-x",
        "tl-y",
        "br-x",
        "br-y",
        "preview"
    };

    mpPreview->EnableDrag();
    mpReslBox->Clear();
    Point aTopLeft, aBottomRight;
    mpPreview->GetPreviewLogicRect(aTopLeft, aBottomRight);
    Point aMinTopLeft, aMaxBottomRight;
    mpPreview->GetMaxLogicRect(aMinTopLeft, aMaxBottomRight);
    mpScanButton->Show( mbScanEnabled );

    if( ! mrSane.IsOpen() )
        return;

    // set Resolution
    nOption = mrSane.GetOptionByName( "resolution" );
    if( nOption != -1 )
    {
        double fRes;

        bSuccess = mrSane.GetOptionValue( nOption, fRes );
        if( bSuccess )
        {
            mpReslBox->Enable( true );

            mpReslBox->SetValue( (long)fRes );
            double *pDouble = NULL;
            nValue = mrSane.GetRange( nOption, pDouble );
            if( nValue > -1 )
            {
                if( nValue )
                {
                    mpReslBox->SetMin( (long)pDouble[0] );
                    mpReslBox->SetMax( (long)pDouble[ nValue-1 ] );
                    for( i=0; i<nValue; i++ )
                    {
                        if( i == 0 || i == nValue-1 || ! ( ((int)pDouble[i]) % 20) )
                            mpReslBox->InsertValue( (long)pDouble[i] );
                    }
                }
                else
                {
                    mpReslBox->SetMin( (long)pDouble[0] );
                    mpReslBox->SetMax( (long)pDouble[1] );
                    mpReslBox->InsertValue( (long)pDouble[0] );
                    // Can only select 75 and 2400 dpi in Scanner dialogue
                    // scanner allows random setting of dpi resolution, a slider might be useful
                    // support that
                    // workaround: offer at least some more standard dpi resolution between
                    // min and max value
                    int bGot300 = 0;
                    for ( int nRes = (long) pDouble[0] * 2; nRes < (long) pDouble[1]; nRes = nRes * 2 )
                    {
                        if ( !bGot300 && nRes > 300 ) {
                            nRes = 300; bGot300 = 1;
                        }
                        mpReslBox->InsertValue(nRes);
                    }
                    mpReslBox->InsertValue( (long)pDouble[1] );
                }
            }
            else
                mpReslBox->Enable( false );
            delete [] pDouble;
        }
    }
    else
        mpReslBox->Enable( false );

    // set scan area
    for( i = 0; i < 4; i++ )
    {
        char const *pOptionName = NULL;
        MetricField* pField = NULL;
        switch( i )
        {
            case 0:
                pOptionName = "tl-x";
                pField = mpLeftField;
                break;
            case 1:
                pOptionName = "tl-y";
                pField = mpTopField;
                break;
            case 2:
                pOptionName = "br-x";
                pField = mpRightField;
                break;
            case 3:
                pOptionName = "br-y";
                pField = mpBottomField;
        }
        nOption = pOptionName ? mrSane.GetOptionByName( pOptionName ) : -1;
        bSuccess = false;
        if( nOption != -1 )
        {
            bSuccess = mrSane.GetOptionValue( nOption, fValue, 0 );
            if( bSuccess )
            {
                if( mrSane.GetOptionUnit( nOption ) == SANE_UNIT_MM )
                {
                    pField->SetUnit( FUNIT_MM );
                    pField->SetValue( (int)fValue, FUNIT_MM );
                }
                else // SANE_UNIT_PIXEL
                {
                    pField->SetValue( (int)fValue, FUNIT_CUSTOM );
                    pField->SetCustomUnitText(OUString("Pixel"));
                }
                switch( i ) {
                    case 0: aTopLeft.X() = (int)fValue;break;
                    case 1: aTopLeft.Y() = (int)fValue;break;
                    case 2: aBottomRight.X() = (int)fValue;break;
                    case 3: aBottomRight.Y() = (int)fValue;break;
                }
            }
            double *pDouble = NULL;
            nValue = mrSane.GetRange( nOption, pDouble );
            if( nValue > -1 )
            {
                if( pDouble )
                {
                    pField->SetMin( (long)pDouble[0] );
                    if( nValue )
                        pField->SetMax( (long)pDouble[ nValue-1 ] );
                    else
                        pField->SetMax( (long)pDouble[ 1 ] );
                    delete [] pDouble;
                }
                switch( i ) {
                    case 0: aMinTopLeft.X() = pField->GetMin();break;
                    case 1: aMinTopLeft.Y() = pField->GetMin();break;
                    case 2: aMaxBottomRight.X() = pField->GetMax();break;
                    case 3: aMaxBottomRight.Y() = pField->GetMax();break;
                }
            }
            else
            {
                switch( i ) {
                    case 0: aMinTopLeft.X() = (int)fValue;break;
                    case 1: aMinTopLeft.Y() = (int)fValue;break;
                    case 2: aMaxBottomRight.X() = (int)fValue;break;
                    case 3: aMaxBottomRight.Y() = (int)fValue;break;
                }
            }
            pField->Enable( true );
        }
        else
        {
            mpPreview->DisableDrag();
            pField->SetMin( 0 );
            switch( i ) {
                case 0:
                    aMinTopLeft.X() = 0;
                    aTopLeft.X() = 0;
                    pField->SetMax( PREVIEW_WIDTH );
                    pField->SetValue( 0 );
                    break;
                case 1:
                    aMinTopLeft.Y() = 0;
                    aTopLeft.Y() = 0;
                    pField->SetMax( PREVIEW_HEIGHT );
                    pField->SetValue( 0 );
                    break;
                case 2:
                    aMaxBottomRight.X() = PREVIEW_WIDTH;
                    aBottomRight.X() = PREVIEW_WIDTH;
                    pField->SetMax( PREVIEW_WIDTH );
                    pField->SetValue( PREVIEW_WIDTH );
                    break;
                case 3:
                    aMaxBottomRight.Y() = PREVIEW_HEIGHT;
                    aBottomRight.Y() = PREVIEW_HEIGHT;
                    pField->SetMax( PREVIEW_HEIGHT );
                    pField->SetValue( PREVIEW_HEIGHT );
                    break;
            }
            pField->Enable( false );
        }
    }

    mpPreview->SetPreviewMaxRect(aMinTopLeft, aMaxBottomRight);
    mpPreview->SetPreviewLogicRect(aTopLeft, aBottomRight);
    mpPreview->Invalidate();

    // fill OptionBox
    mpOptionBox->Clear();
    SvTreeListEntry* pParentEntry = 0;
    bool bGroupRejected = false;
    for( i = 1; i < mrSane.CountOptions(); i++ )
    {
        OUString aOption=mrSane.GetOptionName( i );
        bool bInsertAdvanced =
            mrSane.GetOptionCap( i ) & SANE_CAP_ADVANCED &&
            ! mpAdvancedBox->IsChecked() ? sal_False : sal_True;
        if( mrSane.GetOptionType( i ) == SANE_TYPE_GROUP )
        {
            if( bInsertAdvanced )
            {
                aOption = mrSane.GetOptionTitle( i );
                pParentEntry = mpOptionBox->InsertEntry( aOption );
                bGroupRejected = false;
            }
            else
                bGroupRejected = true;
        }
        else if( !aOption.isEmpty() &&
                 ! ( mrSane.GetOptionCap( i ) &
                     (
                         SANE_CAP_HARD_SELECT |
                         SANE_CAP_INACTIVE
                         ) ) &&
                 bInsertAdvanced && ! bGroupRejected )
        {
            bool bIsSpecial = false;
            for( size_t n = 0; !bIsSpecial &&
                     n < SAL_N_ELEMENTS(ppSpecialOptions); n++ )
            {
                if( aOption == OUString::createFromAscii(ppSpecialOptions[n]) )
                    bIsSpecial=true;
            }
            if( ! bIsSpecial )
            {
                if( pParentEntry )
                    mpOptionBox->InsertEntry( aOption, pParentEntry );
                else
                    mpOptionBox->InsertEntry( aOption );
            }
        }
    }
}

IMPL_LINK( SaneDlg, ClickBtnHdl, Button*, pButton )
{
    if( mrSane.IsOpen() )
    {
        if( pButton == mpDeviceInfoButton )
        {
            OUString aString("Device: %s\nVendor: %s\nModel: %s\nType: %s");
            aString = aString.replaceFirst( "%s", Sane::GetName( mrSane.GetDeviceNumber() ) );
            aString = aString.replaceFirst( "%s", Sane::GetVendor( mrSane.GetDeviceNumber() ) );
            aString = aString.replaceFirst( "%s", Sane::GetModel( mrSane.GetDeviceNumber() ) );
            aString = aString.replaceFirst( "%s", Sane::GetType( mrSane.GetDeviceNumber() ) );
            InfoBox aInfoBox( this, aString );
            aInfoBox.Execute();
        }
        else if( pButton == mpPreviewButton )
            AcquirePreview();
        else if( pButton == mpBoolCheckBox )
        {
            mrSane.SetOptionValue( mnCurrentOption,
                                   mpBoolCheckBox->IsChecked() );
        }
        else if( pButton == mpButtonOption )
        {

            SANE_Value_Type nType = mrSane.GetOptionType( mnCurrentOption );
            switch( nType )
            {
                case SANE_TYPE_BUTTON:
                    mrSane.ActivateButtonOption( mnCurrentOption );
                    break;
                case SANE_TYPE_FIXED:
                case SANE_TYPE_INT:
                {
                    int nElements = mrSane.GetOptionElements( mnCurrentOption );
                    boost::scoped_array<double> x(new double[ nElements ]);
                    boost::scoped_array<double> y(new double[ nElements ]);
                    for( int i = 0; i < nElements; i++ )
                        x[ i ] = (double)i;
                    mrSane.GetOptionValue( mnCurrentOption, y.get() );

                    GridDialog aGrid( x.get(), y.get(), nElements, this );
                    aGrid.SetText( mrSane.GetOptionName( mnCurrentOption ) );
                    aGrid.setBoundings( 0, mfMin, nElements, mfMax );
                    if( aGrid.Execute() && aGrid.getNewYValues() )
                        mrSane.SetOptionValue( mnCurrentOption, aGrid.getNewYValues() );
                }
                break;
                case SANE_TYPE_BOOL:
                case SANE_TYPE_STRING:
                case SANE_TYPE_GROUP:
                    break;
            }
        }
        else if( pButton == mpAdvancedBox )
        {
            ReloadSaneOptionsHdl( NULL );
        }
    }
    if( pButton == mpOKButton || pButton == mpScanButton )
    {
        double fRes = (double)mpReslBox->GetValue();
        SetAdjustedNumericalValue( "resolution", fRes );
        UpdateScanArea(true);
        SaveState();
        EndDialog( mrSane.IsOpen() ? 1 : 0 );
        doScan = (pButton == mpScanButton);
    }
    else if( pButton == mpCancelButton )
    {
        mrSane.Close();
        EndDialog( 0 );
    }
    return 0;
}

IMPL_LINK( SaneDlg, SelectHdl, ListBox*, pListBox )
{
    if( pListBox == mpDeviceBox && Sane::IsSane() && Sane::CountDevices() )
    {
        int nNewNumber = mpDeviceBox->GetSelectEntryPos();
        int nOldNumber = mrSane.GetDeviceNumber();
        if (nNewNumber != nOldNumber)
        {
            mrSane.Close();
            mrSane.Open(nNewNumber);
            mpPreview->ResetForNewScanner();
            InitFields();
        }
    }
    if( mrSane.IsOpen() )
    {
        if( pListBox == mpQuantumRangeBox )
        {
            OString aValue(OUStringToOString(mpQuantumRangeBox->GetSelectEntry(),
                osl_getThreadTextEncoding()));
            double fValue = atof(aValue.getStr());
            mrSane.SetOptionValue( mnCurrentOption, fValue, mnCurrentElement );
        }
        else if( pListBox == mpStringRangeBox )
        {
            mrSane.SetOptionValue( mnCurrentOption, mpStringRangeBox->GetSelectEntry() );
        }
    }
    return 0;
}

IMPL_LINK( SaneDlg, OptionsBoxSelectHdl, SvTreeListBox*, pBox )
{
    if( pBox == mpOptionBox && Sane::IsSane() )
    {
        OUString aOption =
            mpOptionBox->GetEntryText( mpOptionBox->FirstSelected() );
        int nOption = mrSane.GetOptionByName(OUStringToOString(aOption,
            osl_getThreadTextEncoding()).getStr());
        if( nOption != -1 && nOption != mnCurrentOption )
        {
            DisableOption();
            mnCurrentOption = nOption;
            mpOptionTitle->SetText( mrSane.GetOptionTitle( mnCurrentOption ) );
            SANE_Value_Type nType = mrSane.GetOptionType( mnCurrentOption );
            SANE_Constraint_Type nConstraint;
            switch( nType )
            {
                case SANE_TYPE_BOOL:    EstablishBoolOption();break;
                case SANE_TYPE_STRING:
                    nConstraint = mrSane.GetOptionConstraintType( mnCurrentOption );
                    if( nConstraint == SANE_CONSTRAINT_STRING_LIST )
                        EstablishStringRange();
                    else
                        EstablishStringOption();
                    break;
                case SANE_TYPE_FIXED:
                case SANE_TYPE_INT:
                {
                    nConstraint = mrSane.GetOptionConstraintType( mnCurrentOption );
                    int nElements = mrSane.GetOptionElements( mnCurrentOption );
                    mnCurrentElement = 0;
                    if( nConstraint == SANE_CONSTRAINT_RANGE ||
                        nConstraint == SANE_CONSTRAINT_WORD_LIST )
                        EstablishQuantumRange();
                    else
                    {
                        mfMin = mfMax = 0.0;
                        EstablishNumericOption();
                    }
                    if( nElements > 1 )
                    {
                        if( nElements <= 10 )
                        {
                            mpVectorBox->SetValue( 1 );
                            mpVectorBox->SetMin( 1 );
                            mpVectorBox->SetMax(
                                mrSane.GetOptionElements( mnCurrentOption ) );
                            mpVectorBox->Show( true );
                            mpVectorTxt->Show( true );
                        }
                        else
                        {
                            DisableOption();
                            // bring up dialog only on button click
                            EstablishButtonOption();
                        }
                    }
                }
                break;
                case SANE_TYPE_BUTTON:
                    EstablishButtonOption();
                    break;
                default: break;
            }
        }
    }
    return 0;
}

IMPL_LINK( SaneDlg, ModifyHdl, Edit*, pEdit )
{
    if( mrSane.IsOpen() )
    {
        if( pEdit == mpStringEdit )
        {
            mrSane.SetOptionValue( mnCurrentOption, mpStringEdit->GetText() );
        }
        else if( pEdit == mpReslBox )
        {
            double fRes = (double)mpReslBox->GetValue();
            int nOption = mrSane.GetOptionByName( "resolution" );
            if( nOption != -1 )
            {
                double* pDouble = NULL;
                int nValues = mrSane.GetRange( nOption, pDouble );
                if( nValues > 0 )
                {
                    int i;
                    for( i = 0; i < nValues; i++ )
                    {
                        if( fRes == pDouble[i] )
                            break;
                    }
                    if( i >= nValues )
                        fRes = pDouble[0];
                }
                else if( nValues == 0 )
                {
                    if( fRes < pDouble[ 0 ] )
                        fRes = pDouble[ 0 ];
                    if( fRes > pDouble[ 1 ] )
                        fRes = pDouble[ 1 ];
                }
                delete[] pDouble;
                mpReslBox->SetValue( (sal_uLong)fRes );
            }
        }
        else if( pEdit == mpNumericEdit )
        {
            double fValue;
            OString aContents(OUStringToOString(mpNumericEdit->GetText(),
                osl_getThreadTextEncoding()));
            fValue = atof(aContents.getStr());
            if( mfMin != mfMax && ( fValue < mfMin || fValue > mfMax ) )
            {
                char pBuf[256];
                if( fValue < mfMin )
                    fValue = mfMin;
                else if( fValue > mfMax )
                    fValue = mfMax;
                sprintf( pBuf, "%g", fValue );
                mpNumericEdit->SetText( OUString( pBuf, strlen(pBuf), osl_getThreadTextEncoding() ) );
            }
            mrSane.SetOptionValue( mnCurrentOption, fValue, mnCurrentElement );
        }
        else if( pEdit == mpVectorBox )
        {
            char pBuf[256];
            mnCurrentElement = mpVectorBox->GetValue()-1;
            double fValue;
            if( mrSane.GetOptionValue( mnCurrentOption, fValue, mnCurrentElement ))
            {
                sprintf( pBuf, "%g", fValue );
                OUString aValue( pBuf, strlen(pBuf), osl_getThreadTextEncoding() );
                mpNumericEdit->SetText( aValue );
                mpQuantumRangeBox->SelectEntry( aValue );
            }
        }
        else if( pEdit == mpTopField )
        {
            mpPreview->ChangePreviewLogicTopLeftY(mpTopField->GetValue());
            mpPreview->DrawDrag();
        }
        else if( pEdit == mpLeftField )
        {
            mpPreview->ChangePreviewLogicTopLeftX(mpLeftField->GetValue());
            mpPreview->DrawDrag();
        }
        else if( pEdit == mpBottomField )
        {
            mpPreview->ChangePreviewLogicBottomRightY(mpBottomField->GetValue());
            mpPreview->DrawDrag();
        }
        else if( pEdit == mpRightField )
        {
            mpPreview->ChangePreviewLogicBottomRightX(mpRightField->GetValue());
            mpPreview->DrawDrag();
        }
    }
    return 0;
}

IMPL_LINK( SaneDlg, ReloadSaneOptionsHdl, Sane*, /*pSane*/ )
{
    mnCurrentOption = -1;
    mnCurrentElement = 0;
    DisableOption();
    InitFields();
    mpPreview->Invalidate();
    return 0;
}

void SaneDlg::AcquirePreview()
{
    if( ! mrSane.IsOpen() )
        return;

    UpdateScanArea( true );
    // set small resolution for preview
    double fResl = (double)mpReslBox->GetValue();
    SetAdjustedNumericalValue( "resolution", 30.0 );

    int nOption = mrSane.GetOptionByName( "preview" );
    if( nOption == -1 )
    {
        OUString aString("The device does not offer a preview option. Therefore, a normal scan will be used as a preview instead. This may take a considerable amount of time." );
        WarningBox aBox( this, WB_OK_CANCEL | WB_DEF_OK, aString );
        if( aBox.Execute() == RET_CANCEL )
            return;
    }
    else
        mrSane.SetOptionValue( nOption, true );

    BitmapTransporter aTransporter;
    if( ! mrSane.Start( aTransporter ) )
    {
        ErrorBox aErrorBox( this, WB_OK | WB_DEF_OK,
                            "An error occurred while scanning." );
        aErrorBox.Execute();
    }
    else
    {
#if OSL_DEBUG_LEVEL > 1
        aTransporter.getStream().Seek( STREAM_SEEK_TO_END );
        fprintf( stderr, "Previewbitmapstream contains %d bytes\n", (int)aTransporter.getStream().Tell() );
#endif
        aTransporter.getStream().Seek( STREAM_SEEK_TO_BEGIN );
        mpPreview->SetBitmap(aTransporter.getStream());
    }

    SetAdjustedNumericalValue( "resolution", fResl );
    mpReslBox->SetValue( (sal_uLong)fResl );

    mpPreview->UpdatePreviewBounds();
    mpPreview->Invalidate();
}

void ScanPreview::UpdatePreviewBounds()
{
    if( mbDragEnable )
    {
        maPreviewRect = Rectangle( maTopLeft,
                                   Size( maBottomRight.X() - maTopLeft.X(),
                                         maBottomRight.Y() - maTopLeft.Y() )
                                   );
    }
    else
    {
        Size aBMSize( maPreviewBitmap.GetSizePixel() );
        if( aBMSize.Width() > aBMSize.Height() && aBMSize.Width() )
        {
            int nVHeight = (maBottomRight.X() - maTopLeft.X()) * aBMSize.Height() / aBMSize.Width();
            maPreviewRect = Rectangle( Point( maTopLeft.X(), ( maTopLeft.Y() + maBottomRight.Y() )/2 - nVHeight/2 ),
                                       Size( maBottomRight.X() - maTopLeft.X(),
                                             nVHeight ) );
        }
        else if (aBMSize.Height())
        {
            int nVWidth = (maBottomRight.Y() - maTopLeft.Y()) * aBMSize.Width() / aBMSize.Height();
            maPreviewRect = Rectangle( Point( ( maTopLeft.X() + maBottomRight.X() )/2 - nVWidth/2, maTopLeft.Y() ),
                                       Size( nVWidth,
                                             maBottomRight.Y() - maTopLeft.Y() ) );
        }
    }
}

void ScanPreview::Paint(const Rectangle& rRect)
{
    Window::Paint(rRect);
    SetMapMode(MAP_APPFONT);
    SetFillColor( Color( COL_WHITE ) );
    SetLineColor( Color( COL_WHITE ) );
    DrawRect( Rectangle( Point( 0, 0 ),
                         Size( PREVIEW_WIDTH, PREVIEW_HEIGHT ) ) );
    SetMapMode( MapMode( MAP_PIXEL ) );
    // check for sane values
    DrawBitmap( maPreviewRect.TopLeft(), maPreviewRect.GetSize(),
                maPreviewBitmap );

    mbDragDrawn = false;
    DrawDrag();
}

void SaneDlg::DisableOption()
{
    mpBoolCheckBox->Show( false );
    mpStringEdit->Show( false );
    mpNumericEdit->Show( false );
    mpQuantumRangeBox->Show( false );
    mpStringRangeBox->Show( false );
    mpButtonOption->Show( false );
    mpVectorBox->Show( false );
    mpVectorTxt->Show( false );
    mpOptionDescTxt->Show( false );
}

void SaneDlg::EstablishBoolOption()
{
    bool bSuccess, bValue;

    bSuccess = mrSane.GetOptionValue( mnCurrentOption, bValue );
    if( bSuccess )
    {
        mpBoolCheckBox->SetText( mrSane.GetOptionName( mnCurrentOption ) );
        mpBoolCheckBox->Check( bValue );
        mpBoolCheckBox->Show( true );
    }
}

void SaneDlg::EstablishStringOption()
{
    bool bSuccess;
    OString aValue;

    bSuccess = mrSane.GetOptionValue( mnCurrentOption, aValue );
    if( bSuccess )
    {
        mpOptionDescTxt->SetText( mrSane.GetOptionName( mnCurrentOption ) );
        mpOptionDescTxt->Show( true );
        mpStringEdit->SetText(OStringToOUString(aValue, osl_getThreadTextEncoding()));
        mpStringEdit->Show( true );
    }
}

void SaneDlg::EstablishStringRange()
{
    const char** ppStrings = mrSane.GetStringConstraint( mnCurrentOption );
    mpStringRangeBox->Clear();
    for( int i = 0; ppStrings[i] != 0; i++ )
        mpStringRangeBox->InsertEntry( OUString( ppStrings[i], strlen(ppStrings[i]), osl_getThreadTextEncoding() ) );
    OString aValue;
    mrSane.GetOptionValue( mnCurrentOption, aValue );
    mpStringRangeBox->SelectEntry(OStringToOUString(aValue, osl_getThreadTextEncoding()));
    mpStringRangeBox->Show( true );
    mpOptionDescTxt->SetText( mrSane.GetOptionName( mnCurrentOption ) );
    mpOptionDescTxt->Show( true );
}

void SaneDlg::EstablishQuantumRange()
{
    if( mpRange )
    {
        delete [] mpRange;
        mpRange = 0;
    }
    int nValues = mrSane.GetRange( mnCurrentOption, mpRange );
    if( nValues == 0 )
    {
        mfMin = mpRange[ 0 ];
        mfMax = mpRange[ 1 ];
        delete [] mpRange;
        mpRange = 0;
        EstablishNumericOption();
    }
    else if( nValues > 0 )
    {
        char pBuf[ 256 ];
        mpQuantumRangeBox->Clear();
        mfMin = mpRange[ 0 ];
        mfMax = mpRange[ nValues-1 ];
        for( int i = 0; i < nValues; i++ )
        {
            sprintf( pBuf, "%g", mpRange[ i ] );
            mpQuantumRangeBox->InsertEntry( OUString( pBuf, strlen(pBuf), osl_getThreadTextEncoding() ) );
        }
        double fValue;
        if( mrSane.GetOptionValue( mnCurrentOption, fValue, mnCurrentElement ) )
        {
            sprintf( pBuf, "%g", fValue );
            mpQuantumRangeBox->SelectEntry( OUString( pBuf, strlen(pBuf), osl_getThreadTextEncoding() ) );
        }
        mpQuantumRangeBox->Show( true );
        OUString aText( mrSane.GetOptionName( mnCurrentOption ) );
        aText += " ";
        aText += mrSane.GetOptionUnitName( mnCurrentOption );
        mpOptionDescTxt->SetText( aText );
        mpOptionDescTxt->Show( true );
    }
}

void SaneDlg::EstablishNumericOption()
{
    bool bSuccess;
    double fValue;

    bSuccess = mrSane.GetOptionValue( mnCurrentOption, fValue );
    if( ! bSuccess )
        return;

    char pBuf[256];
    OUString aText( mrSane.GetOptionName( mnCurrentOption ) );
    aText += " ";
    aText += mrSane.GetOptionUnitName( mnCurrentOption );
    if( mfMin != mfMax )
    {
        sprintf( pBuf, " < %g ; %g >", mfMin, mfMax );
        aText += OUString( pBuf, strlen(pBuf), osl_getThreadTextEncoding() );
    }
    mpOptionDescTxt->SetText( aText );
    mpOptionDescTxt->Show( true );
    sprintf( pBuf, "%g", fValue );
    mpNumericEdit->SetText( OUString( pBuf, strlen(pBuf), osl_getThreadTextEncoding() ) );
    mpNumericEdit->Show( true );
}

void SaneDlg::EstablishButtonOption()
{
    mpOptionDescTxt->SetText( mrSane.GetOptionName( mnCurrentOption ) );
    mpOptionDescTxt->Show( true );
    mpButtonOption->Show( true );
}

#define RECT_SIZE_PIX 7

void ScanPreview::MouseMove(const MouseEvent& rMEvt)
{
    if( mbIsDragging )
    {
        Point aMousePos = rMEvt.GetPosPixel();
        // move into valid area
        Point aLogicPos = GetLogicPos( aMousePos );
        aMousePos = GetPixelPos( aLogicPos );
        switch( meDragDirection )
        {
            case TopLeft:       maTopLeft = aMousePos; break;
            case Top:           maTopLeft.Y() = aMousePos.Y(); break;
            case TopRight:
                maTopLeft.Y() = aMousePos.Y();
                maBottomRight.X() = aMousePos.X();
                break;
            case Right:         maBottomRight.X() = aMousePos.X(); break;
            case BottomRight:   maBottomRight = aMousePos; break;
            case Bottom:        maBottomRight.Y() = aMousePos.Y(); break;
            case BottomLeft:
                maTopLeft.X() = aMousePos.X();
                maBottomRight.Y() = aMousePos.Y();
                break;
            case Left:          maTopLeft.X() = aMousePos.X(); break;
            default: break;
        }
        int nSwap;
        if( maTopLeft.X() > maBottomRight.X() )
        {
            nSwap = maTopLeft.X();
            maTopLeft.X() = maBottomRight.X();
            maBottomRight.X() = nSwap;
        }
        if( maTopLeft.Y() > maBottomRight.Y() )
        {
            nSwap = maTopLeft.Y();
            maTopLeft.Y() = maBottomRight.Y();
            maBottomRight.Y() = nSwap;
        }
        DrawDrag();
        mpParentDialog->UpdateScanArea(false);
    }
    Window::MouseMove( rMEvt );
}

void ScanPreview::MouseButtonDown( const MouseEvent& rMEvt )
{
    Point aMousePixel = rMEvt.GetPosPixel();

    if( ! mbIsDragging  && mbDragEnable )
    {
        int nMiddleX = ( maBottomRight.X() - maTopLeft.X() ) / 2 - RECT_SIZE_PIX/2 + maTopLeft.X();
        int nMiddleY = ( maBottomRight.Y() - maTopLeft.Y() ) / 2 - RECT_SIZE_PIX/2 + maTopLeft.Y();
        if( aMousePixel.Y() >= maTopLeft.Y() &&
            aMousePixel.Y() < maTopLeft.Y() + RECT_SIZE_PIX )
        {
            if( aMousePixel.X() >= maTopLeft.X() &&
                aMousePixel.X() < maTopLeft.X() + RECT_SIZE_PIX )
            {
                meDragDirection = TopLeft;
                aMousePixel = maTopLeft;
                mbIsDragging = true;
            }
            else if( aMousePixel.X() >= nMiddleX &&
                     aMousePixel.X() < nMiddleX + RECT_SIZE_PIX )
            {
                meDragDirection = Top;
                aMousePixel.Y() = maTopLeft.Y();
                mbIsDragging = true;
            }
            else if( aMousePixel.X() > maBottomRight.X() - RECT_SIZE_PIX &&
                     aMousePixel.X() <= maBottomRight.X() )
            {
                meDragDirection = TopRight;
                aMousePixel = Point( maBottomRight.X(), maTopLeft.Y() );
                mbIsDragging = true;
            }
        }
        else if( aMousePixel.Y() >= nMiddleY &&
                 aMousePixel.Y() < nMiddleY + RECT_SIZE_PIX )
        {
            if( aMousePixel.X() >= maTopLeft.X() &&
                aMousePixel.X() < maTopLeft.X() + RECT_SIZE_PIX )
            {
                meDragDirection = Left;
                aMousePixel.X() = maTopLeft.X();
                mbIsDragging = true;
            }
            else if( aMousePixel.X() > maBottomRight.X() - RECT_SIZE_PIX &&
                     aMousePixel.X() <= maBottomRight.X() )
            {
                meDragDirection = Right;
                aMousePixel.X() = maBottomRight.X();
                mbIsDragging = true;
            }
        }
        else if( aMousePixel.Y() <= maBottomRight.Y() &&
                 aMousePixel.Y() > maBottomRight.Y() - RECT_SIZE_PIX )
        {
            if( aMousePixel.X() >= maTopLeft.X() &&
                aMousePixel.X() < maTopLeft.X() + RECT_SIZE_PIX )
            {
                meDragDirection = BottomLeft;
                aMousePixel = Point( maTopLeft.X(), maBottomRight.Y() );
                mbIsDragging = true;
            }
            else if( aMousePixel.X() >= nMiddleX &&
                     aMousePixel.X() < nMiddleX + RECT_SIZE_PIX )
            {
                meDragDirection = Bottom;
                aMousePixel.Y() = maBottomRight.Y();
                mbIsDragging = true;
            }
            else if( aMousePixel.X() > maBottomRight.X() - RECT_SIZE_PIX &&
                     aMousePixel.X() <= maBottomRight.X() )
            {
                meDragDirection = BottomRight;
                aMousePixel = maBottomRight;
                mbIsDragging = true;
            }
        }
    }
    if( mbIsDragging )
    {
        SetPointerPosPixel( aMousePixel );
        DrawDrag();
    }
    Window::MouseButtonDown( rMEvt );
}

void ScanPreview::MouseButtonUp( const MouseEvent& rMEvt )
{
    if( mbIsDragging )
    {
        mpParentDialog->UpdateScanArea(true);
    }
    mbIsDragging = false;

    Window::MouseButtonUp( rMEvt );
}

void ScanPreview::DrawRectangles( Point& rUL, Point& rBR )
{
    int nMiddleX, nMiddleY;
    Point aBL, aUR;

    aUR = Point( rBR.X(), rUL.Y() );
    aBL = Point( rUL.X(), rBR.Y() );
    nMiddleX = ( rBR.X() - rUL.X() ) / 2 + rUL.X();
    nMiddleY = ( rBR.Y() - rUL.Y() ) / 2 + rUL.Y();

    DrawLine( rUL, aBL );
    DrawLine( aBL, rBR );
    DrawLine( rBR, aUR );
    DrawLine( aUR, rUL );
    DrawRect( Rectangle( rUL, Size( RECT_SIZE_PIX,RECT_SIZE_PIX ) ) );
    DrawRect( Rectangle( aBL, Size( RECT_SIZE_PIX, -RECT_SIZE_PIX ) ) );
    DrawRect( Rectangle( rBR, Size( -RECT_SIZE_PIX, -RECT_SIZE_PIX ) ) );
    DrawRect( Rectangle( aUR, Size( -RECT_SIZE_PIX, RECT_SIZE_PIX ) ) );
    DrawRect( Rectangle( Point( nMiddleX - RECT_SIZE_PIX/2, rUL.Y() ), Size( RECT_SIZE_PIX, RECT_SIZE_PIX ) ) );
    DrawRect( Rectangle( Point( nMiddleX - RECT_SIZE_PIX/2, rBR.Y() ), Size( RECT_SIZE_PIX, -RECT_SIZE_PIX ) ) );
    DrawRect( Rectangle( Point( rUL.X(), nMiddleY - RECT_SIZE_PIX/2 ), Size( RECT_SIZE_PIX, RECT_SIZE_PIX ) ) );
    DrawRect( Rectangle( Point( rBR.X(), nMiddleY - RECT_SIZE_PIX/2 ), Size( -RECT_SIZE_PIX, RECT_SIZE_PIX ) ) );
}

void ScanPreview::DrawDrag()
{
    static Point aLastUL, aLastBR;

    if( ! mbDragEnable )
        return;

    RasterOp eROP = GetRasterOp();
    SetRasterOp( ROP_INVERT );
    SetMapMode( MapMode( MAP_PIXEL ) );

    if( mbDragDrawn )
        DrawRectangles( aLastUL, aLastBR );

    aLastUL = maTopLeft;
    aLastBR = maBottomRight;
    DrawRectangles( maTopLeft, maBottomRight );

    mbDragDrawn = true;
    SetRasterOp( eROP );
    SetMapMode(MAP_APPFONT);
}

Point ScanPreview::GetPixelPos( const Point& rIn) const
{
    Point aConvert(
        ( ( rIn.X() * PREVIEW_WIDTH ) /
          ( maMaxBottomRight.X() - maMinTopLeft.X() ) )
        ,
        ( ( rIn.Y() * PREVIEW_HEIGHT )
          / ( maMaxBottomRight.Y() - maMinTopLeft.Y() ) )
        );

    return LogicToPixel(aConvert, MAP_APPFONT);
}

Point ScanPreview::GetLogicPos(const Point& rIn) const
{
    Point aConvert = PixelToLogic(rIn, MAP_APPFONT);
    if( aConvert.X() < 0 )
        aConvert.X() = 0;
    if( aConvert.X() >= PREVIEW_WIDTH )
        aConvert.X() = PREVIEW_WIDTH-1;
    if( aConvert.Y() < 0 )
        aConvert.Y() = 0;
    if( aConvert.Y() >= PREVIEW_HEIGHT )
        aConvert.Y() = PREVIEW_HEIGHT-1;

    aConvert.X() *= ( maMaxBottomRight.X() - maMinTopLeft.X() );
    aConvert.X() /= PREVIEW_WIDTH;
    aConvert.Y() *= ( maMaxBottomRight.Y() - maMinTopLeft.Y() );
    aConvert.Y() /= PREVIEW_HEIGHT;
    return aConvert;
}

void SaneDlg::UpdateScanArea(bool bSend)
{
    if (!mpPreview->IsDragEnabled())
        return;

    Point aUL, aBR;
    mpPreview->GetPreviewLogicRect(aUL, aBR);

    mpLeftField->SetValue( aUL.X() );
    mpTopField->SetValue( aUL.Y() );
    mpRightField->SetValue( aBR.X() );
    mpBottomField->SetValue( aBR.Y() );

    if (!bSend)
        return;

    if( mrSane.IsOpen() )
    {
        SetAdjustedNumericalValue( "tl-x", (double)aUL.X() );
        SetAdjustedNumericalValue( "tl-y", (double)aUL.Y() );
        SetAdjustedNumericalValue( "br-x", (double)aBR.X() );
        SetAdjustedNumericalValue( "br-y", (double)aBR.Y() );
    }
}

bool SaneDlg::LoadState()
{
    int i;

    if( ! Sane::IsSane() )
        return false;

    const char* pEnv = getenv("HOME");
    OUString aFileName = pEnv ? OUString(pEnv, strlen(pEnv), osl_getThreadTextEncoding() ) : OUString();
    aFileName += "/.so_sane_state";
    Config aConfig( aFileName );
    if( ! aConfig.HasGroup( "SANE" ) )
        return false;

    aConfig.SetGroup( "SANE" );
    OString aString = aConfig.ReadKey( "SO_LastSaneDevice" );
    for( i = 0; i < Sane::CountDevices() && !aString.equals(OUStringToOString(Sane::GetName(i), osl_getThreadTextEncoding())); i++ ) ;
    if( i == Sane::CountDevices() )
        return false;

    mrSane.Close();
    mrSane.Open( aString.getStr() );

    DisableOption();
    InitFields();

    if( mrSane.IsOpen() )
    {
        int iMax = aConfig.GetKeyCount();
        for (i = 0; i < iMax; ++i)
        {
            aString = aConfig.GetKeyName( i );
            OString aValue = aConfig.ReadKey( i );
            int nOption = mrSane.GetOptionByName( aString.getStr() );
            if( nOption == -1 )
                continue;

            if (aValue.startsWith("BOOL="))
            {
                aValue = aValue.copy(RTL_CONSTASCII_LENGTH("BOOL="));
                bool aBOOL = aValue.toInt32() != 0;
                mrSane.SetOptionValue( nOption, aBOOL );
            }
            else if (aValue.startsWith("STRING="))
            {
                aValue = aValue.copy(RTL_CONSTASCII_LENGTH("STRING="));
                mrSane.SetOptionValue(nOption,OStringToOUString(aValue, osl_getThreadTextEncoding()) );
            }
            else if (aValue.startsWith("NUMERIC="))
            {
                aValue = aValue.copy(RTL_CONSTASCII_LENGTH("NUMERIC="));

                sal_Int32 nIndex = 0;
                int n = 0;
                do
                {
                    OString aSub = aValue.getToken(0, ':', nIndex);
                    double fValue=0.0;
                    sscanf(aSub.getStr(), "%lg", &fValue);
                    SetAdjustedNumericalValue(aString.getStr(), fValue, n++);
                }
                while ( nIndex >= 0 );
            }
        }
    }

    DisableOption();
    InitFields();

    return true;
}

void SaneDlg::SaveState()
{
    if( ! Sane::IsSane() )
        return;

    const char* pEnv = getenv( "HOME" );
    OUString aFileName;

    if( pEnv )
        aFileName = OUString::createFromAscii(pEnv) + "/.so_sane_state";
    else
        aFileName = OStringToOUString("", osl_getThreadTextEncoding()) + "/.so_sane_state";

    Config aConfig( aFileName );
    aConfig.DeleteGroup( "SANE" );
    aConfig.SetGroup( "SANE" );
    aConfig.WriteKey( "SO_LastSANEDevice",
        OUStringToOString(mpDeviceBox->GetSelectEntry(), RTL_TEXTENCODING_UTF8) );

    static char const* pSaveOptions[] = {
        "resolution",
        "tl-x",
        "tl-y",
        "br-x",
        "br-y"
    };
    for( size_t i = 0; i < SAL_N_ELEMENTS(pSaveOptions); ++i )
    {
        OString aOption = pSaveOptions[i];
        int nOption = mrSane.GetOptionByName( pSaveOptions[i] );
        if( nOption > -1 )
        {
            SANE_Value_Type nType = mrSane.GetOptionType( nOption );
            switch( nType )
            {
                case SANE_TYPE_BOOL:
                {
                    bool bValue;
                    if( mrSane.GetOptionValue( nOption, bValue ) )
                    {
                        OStringBuffer aString("BOOL=");
                        aString.append(static_cast<sal_Int32>(bValue));
                        aConfig.WriteKey(aOption, aString.makeStringAndClear());
                    }
                }
                break;
                case SANE_TYPE_STRING:
                {
                    OString aValue;
                    if( mrSane.GetOptionValue( nOption, aValue ) )
                    {
                        OStringBuffer aString("STRING=");
                        aString.append(aValue);
                        aConfig.WriteKey( aOption, aString.makeStringAndClear() );
                    }
                }
                break;
                case SANE_TYPE_FIXED:
                case SANE_TYPE_INT:
                {
                    OStringBuffer aString("NUMERIC=");
                    double fValue;
                    char buf[256];
                    int n;

                    for( n = 0; n < mrSane.GetOptionElements( nOption ); n++ )
                    {
                        if( ! mrSane.GetOptionValue( nOption, fValue, n ) )
                            break;
                        if( n > 0 )
                            aString.append(':');
                        sprintf( buf, "%lg", fValue );
                        aString.append(buf);
                    }
                    if( n >= mrSane.GetOptionElements( nOption ) )
                        aConfig.WriteKey( aOption, aString.makeStringAndClear() );
                }
                break;
                default:
                    break;
            }
        }
    }
}

bool SaneDlg::SetAdjustedNumericalValue(
    const char* pOption,
    double fValue,
    int nElement )
{
    int nOption;
    if( ! Sane::IsSane() || ! mrSane.IsOpen() || ( nOption = mrSane.GetOptionByName( pOption ) ) == -1 )
        return false;

    if( nElement < 0 || nElement >= mrSane.GetOptionElements( nOption ) )
        return false;

    double* pValues = NULL;
    int nValues;
    if( ( nValues = mrSane.GetRange( nOption, pValues ) ) < 0 )
    {
        delete [] pValues;
        return false;
    }

#if OSL_DEBUG_LEVEL > 1
    fprintf( stderr, "SaneDlg::SetAdjustedNumericalValue( \"%s\", %lg ) ",
             pOption, fValue );
#endif

    if( nValues )
    {
        int nNearest = 0;
        double fNearest = 1e6;
        for( int i = 0; i < nValues; i++ )
        {
            if( fabs( fValue - pValues[ i ] ) < fNearest )
            {
                fNearest = fabs( fValue - pValues[ i ] );
                nNearest = i;
            }
        }
        fValue = pValues[ nNearest ];
    }
    else
    {
        if( fValue < pValues[0] )
            fValue = pValues[0];
        if( fValue > pValues[1] )
            fValue = pValues[1];
    }
    delete [] pValues;
    mrSane.SetOptionValue( nOption, fValue, nElement );
#if OSL_DEBUG_LEVEL > 1
    fprintf( stderr, "yields %lg\n", fValue );
#endif


    return true;
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
