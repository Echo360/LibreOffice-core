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
#ifndef INCLUDED_SW_INC_VIEWSH_HXX
#define INCLUDED_SW_INC_VIEWSH_HXX

#include <com/sun/star/embed/XClassifiedObject.hpp>
#include <com/sun/star/embed/XEmbeddedObject.hpp>
#include <tools/rtti.hxx>
#include "swdllapi.h"
#include <swtypes.hxx>
#include <ring.hxx>
#include <swrect.hxx>
#include <boost/shared_ptr.hpp>
#include <stack>
#include <vcl/mapmod.hxx>
#include <vcl/print.hxx>

namespace com { namespace sun { namespace star { namespace accessibility {
           class XAccessible; } } } }

class SwDoc;
class IDocumentSettingAccess;
class IDocumentDeviceAccess;
class IDocumentMarkAccess;
class IDocumentDrawModelAccess;
class IDocumentRedlineAccess;
class IDocumentLayoutAccess;
class IDocumentFieldsAccess;
class IDocumentContentOperations;
class IDocumentStylePoolAccess;
class IDocumentStatistics;
class IDocumentUndoRedo;
class IDocumentListItems;
class IDocumentOutlineNodes;
class SfxPrinter;
class SfxProgress;
class SwRootFrm;
class SwNodes;
class SdrView;
class SfxItemPool;
class SfxViewShell;
class SwViewOption;
class SwViewImp;
class SwPrintData;
class SwPagePreviewPrtData;
class Window;
class OutputDevice;
class SwLayIdle;
struct ShellResource;
class SwRegionRects;
class SwFrm;
class SvtAccessibilityOptions;
class SwPagePreviewLayout;
class SwTxtFrm;
class BitmapEx;

struct SwAccessibilityOptions;
class Region;
class SwPostItMgr;
class SdrPaintWindow;
class SwAccessibleMap;

namespace vcl
{
    class OldStylePrintAdaptor;
}

enum FrameControlType
{
    PageBreak,
    Header,
    Footer
};

// Define for flags needed in ctor or layers below.
// Currently the Preview flag is needed for DrawPage.
#define VSHELLFLAG_ISPREVIEW            ((long)0x1)
#define VSHELLFLAG_SHARELAYOUT          ((long)0x2)
typedef boost::shared_ptr<SwRootFrm> SwRootFrmPtr;

class SW_DLLPUBLIC SwViewShell : public Ring
{
    friend void SetOutDev( SwViewShell *pSh, OutputDevice *pOut );
    friend void SetOutDevAndWin( SwViewShell *pSh, OutputDevice *pOut,
                                 Window *pWin, sal_uInt16 nZoom );

    friend class SwViewImp;
    friend class SwLayIdle;

    // For setting visible area for page preview paint.
    friend class SwPagePreviewLayout;

    // Set SwVisArea in order to enable clean formatting before printing.
    friend void SetSwVisArea( SwViewShell *pSh, const SwRect & );

    static BitmapEx*    mpReplaceBmp;    ///< replaced display of still loaded images
    static BitmapEx*    mpErrorBmp;      ///< error display of missed images

    static bool mbLstAct;        // sal_True if EndAction of last Shell
                                    // i.e. if the EndActions of the other
                                    // Shells on the document are through.

    Point         maPrtOffst;         // Ofst for Printer,
                                     // non-printable margin.
     Size         maBrowseBorder;     // Border for frame documents.
    SwRect        maInvalidRect;

    SfxViewShell *mpSfxViewShell;
    SwViewImp    *mpImp;             // Core-internals of SwViewShell.
                                    // The pointer is never 0.

    Window       *mpWin;              ///< = 0 during printing or pdf export
    OutputDevice *mpOut;              ///< Window, Printer, VirtDev, ...
    OutputDevice* mpTmpRef;           // Temporariy reference device. Is used
                                     // during (printer depending) prospect
                                     // and page preview printing
                                     // (because a scaling has to be set at
                                     // the original printer)

    SwViewOption *mpOpt;
    SwAccessibilityOptions* mpAccOptions;

    bool  mbDocSizeChgd     :1;  // For DocChgNotify(): Announce new DocSize
                                    // at EndAction to DocMDI.
    bool  mbPaintWorks      :1;  // Normal Painting if sal_True,
                                    // remember Paint if sal_False.
    bool  mbPaintInProgress :1;  // Block any double paint.
    bool  mbViewLocked      :1;  // Lock visible range;
                                    // in this case MakeVisible is ineffectual.
    bool  mbInEndAction     :1;  // Avoid problems, cf. viewsh.cxx.
    bool  mbPreview         :1;  // If sal_True it is a Preview-SwViewShell.
    bool  mbFrameView       :1;  // If sal_True it is a  (HTML-)Frame.
    bool  mbEnableSmooth    :1;  // Disable SmoothScroll, e.g. for drag
                                    // of scrollbars.
    bool  mbEndActionByVirDev:1; // Paints from EndAction always via virtual device
                                    // (e.g. when browsing).
    bool      mbShowHeaderSeparator:1; ///< Flag to say that we are showing the header control
    bool      mbShowFooterSeparator:1; ///< Flag to say that we are showing the footer control
    bool      mbHeaderFooterEdit:1;  ///< Flag to say that we are editing header or footer (according to the bShow(Header|Footer)Separator above)

    // boolean, indicating that class in in constructor.
    bool mbInConstructor:1;
    bool mbTiledRendering:1; ///< Set if we are doing the tiled rendering (using PaintTile()).

    SdrPaintWindow*         mpTargetPaintWindow;
    OutputDevice*           mpBufferedOut;

    SwRootFrmPtr            mpLayout;

    // Initialization; called by the diverse constructors.
    SAL_DLLPRIVATE void Init( const SwViewOption *pNewOpt );

    inline void ResetInvalidRect();

    SAL_DLLPRIVATE void Reformat();          // Invalidates complete Layout (ApplyViewOption).

    SAL_DLLPRIVATE void PaintDesktop( const SwRect & );  // Collect values for painting of desktop
                                                        // and calling.
    // PaintDesktop split. This pars is also used by PreviewPage.
    SAL_DLLPRIVATE void _PaintDesktop( const SwRegionRects &rRegion );

    SAL_DLLPRIVATE bool CheckInvalidForPaint( const SwRect & );  // Direct Paint or rather
                                                                    // trigger an action.

    SAL_DLLPRIVATE void PrepareForPrint( const SwPrintData &rOptions );

    SAL_DLLPRIVATE void ImplApplyViewOptions( const SwViewOption &rOpt );

protected:
    static ShellResource*   mpShellRes;      ///< Resources for the Shell.
    static Window*          mpCareWindow;    ///< Avoid this window.

    SwRect                  maVisArea;       ///< The modern version of VisArea.
    SwDoc                   *mpDoc;          ///< The document; never 0.

    sal_uInt16 mnStartAction; ///< != 0 if at least one Action is active.
    sal_uInt16 mnLockPaint;   ///< != 0 if Paint is locked.
    bool      mbSelectAll; ///< Special select all mode: whole document selected, even if doc starts with table.

public:
    TYPEINFO();

          SwViewImp *Imp() { return mpImp; }
    const SwViewImp *Imp() const { return mpImp; }

    const SwNodes& GetNodes() const;

    // After change of printer; by Doc.
    void            InitPrt( OutputDevice *pOutDev );

    // Bracketing of actions belonging together.
    inline void StartAction();
           void ImplStartAction();
    inline void EndAction( const bool bIdleEnd = false );
           void ImplEndAction( const bool bIdleEnd = false );
    sal_uInt16 ActionCount() const { return mnStartAction; }
    bool ActionPend() const { return mnStartAction != 0; }
    bool IsInEndAction() const { return mbInEndAction; }

    void SetEndActionByVirDev( bool b ) { mbEndActionByVirDev = b; }
    bool IsEndActionByVirDev()          { return mbEndActionByVirDev; }

    // The ActionCount for all Shells is temporarily set to zero and then
    // restored at the RootFrame via UNO.
    void    SetRestoreActions(sal_uInt16 nSet);
    sal_uInt16  GetRestoreActions() const;

    inline bool HasInvalidRect() const { return maInvalidRect.HasArea(); }
    void ChgHyphenation() { Reformat(); }
    void ChgNumberDigits();

    bool AddPaintRect( const SwRect &rRect );

    void InvalidateWindows( const SwRect &rRect );

    // #i72754# set of Pre/PostPaints with lock counter and initial target OutDev
protected:
    std::stack<Region>          mPrePostPaintRegions; // acts also as a lock counter (empty == not locked)
    OutputDevice*           mpPrePostOutDev;
    MapMode                 maPrePostMapMode;
public:
    void PrePaint();
    void DLPrePaint2(const Region& rRegion);
    void DLPostPaint2(bool bPaintFormLayer);
    const MapMode& getPrePostMapMode() const { return maPrePostMapMode; }

    virtual void Paint(const Rectangle &rRect);

    /** Paint tile.

        Sets the pOut so that the rRect is always painted over the entire
        pOut, ie. starts in 0,0 and ends in width/height.
    */
    void PaintTile(VirtualDevice &rDevice, int contextWidth, int contextHeight, int tilePosX, int tilePosY, long tileWidth, long tileHeight);

    bool IsPaintInProgress() const { return mbPaintInProgress; }
    bool IsDrawingLayerPaintInProgress() const { return !mPrePostPaintRegions.empty(); }

    // Notification that visible area has been changed.
    // VisArea is reset, after that scrolling takes place.
    // The passed rect is situated on pixel borders
    // in order to avoid pixel errors when scrolling.
    virtual void VisPortChgd( const SwRect & );
    bool SmoothScroll( long lXDiff, long lYDiff, const Rectangle* );//Browser
    void EnableSmooth( bool b ) { mbEnableSmooth = b; }

    const SwRect& VisArea() const { return maVisArea; }

    // If necessary scroll until passed Rect is situated in visible sector.
    void MakeVisible( const SwRect & );

    // At nearest occasion pass new document size to UI.
    void SizeChgNotify();
    void UISizeNotify();            // Passing of current size.

    Point GetPagePos( sal_uInt16 nPageNum ) const;

    sal_uInt16 GetNumPages();   // Ask count of current pages from layout.
    bool   IsDummyPage( sal_uInt16 nPageNum ) const;  // An empty page?

    // Invalidate first visible page for all Shells in ring.
    void SetFirstVisPageInvalid();

    SwRootFrm   *GetLayout() const;
    bool         IsNewLayout() const; // Has Layout been loaded or created?

     Size GetDocSize() const;   // Get document size.

    virtual void CalcLayout();  // Force complete formatting of layout.

    sal_uInt16 GetPageCount() const;

    const Size GetPageSize( sal_uInt16 nPageNum, bool bSkipEmptyPages ) const;

    inline SwDoc *GetDoc()  const { return mpDoc; }  //Never 0.

    /** Provides access to the document setting interface
     */
    const IDocumentSettingAccess* getIDocumentSettingAccess() const;
          IDocumentSettingAccess* getIDocumentSettingAccess();

    /** Provides access to the document device interface
     */
    const IDocumentDeviceAccess* getIDocumentDeviceAccess() const;
          IDocumentDeviceAccess* getIDocumentDeviceAccess();

    /** Provides access to the document bookmark interface
     */
    const IDocumentMarkAccess* getIDocumentMarkAccess() const;
          IDocumentMarkAccess* getIDocumentMarkAccess();

    /** Provides access to the document draw model interface
     */
    const IDocumentDrawModelAccess* getIDocumentDrawModelAccess() const;
          IDocumentDrawModelAccess* getIDocumentDrawModelAccess();

    /** Provides access to the document redline interface
     */
    const IDocumentRedlineAccess* getIDocumentRedlineAccess() const;
          IDocumentRedlineAccess* getIDocumentRedlineAccess();

    /** Provides access to the document layout interface
     */
    const IDocumentLayoutAccess* getIDocumentLayoutAccess() const;
          IDocumentLayoutAccess* getIDocumentLayoutAccess();

    /** Provides access to the content operations interface
     */
    IDocumentContentOperations* getIDocumentContentOperations();

    /** Provides access to the document style pool interface
     */
    IDocumentStylePoolAccess* getIDocumentStylePoolAccess();

    /** Provides access to the document statistics interface
     */
    const IDocumentStatistics* getIDocumentStatistics() const;

    /** Provides access to the document undo/redo interface
     */
    IDocumentUndoRedo const& GetIDocumentUndoRedo() const;
    IDocumentUndoRedo      & GetIDocumentUndoRedo();

    const IDocumentListItems* getIDocumentListItemsAccess() const;
    const IDocumentOutlineNodes* getIDocumentOutlineNodesAccess() const;

    // 1. GetRefDev:   Either the printer or the virtual device from the doc
    // 2. GetWin:      Available if we not printing
    // 3. GetOut:      Printer, Window or Virtual device
    OutputDevice& GetRefDev() const;
    inline Window* GetWin()    const { return mpWin; }
    inline OutputDevice* GetOut()     const { return mpOut; }

    void SetWin(Window* win) { mpWin = win; }
    static inline bool IsLstEndAction() { return SwViewShell::mbLstAct; }

    // Change of all page descriptors.
    void   ChgAllPageOrientation( Orientation eOri );
    void   ChgAllPageSize( Size &rSz );

    // Printing of one page.
    // bIsPDFExport == true is: do PDF Export (no printing!)
    bool PrintOrPDFExport( OutputDevice *pOutDev,
            SwPrintData const& rPrintData,
            sal_Int32 nRenderer /* offset in vector of pages to print */ );

    // Printing of one brochure page.
    void PrintProspect( OutputDevice *pOutDev, const SwPrintData &rPrintData,
            sal_Int32 nRenderer /* offset in vector of page pairs for prospect printing */ );

    // Printing for OLE 2.0.
    static void PrtOle2( SwDoc *pDoc, const SwViewOption *pOpt, const SwPrintData& rOptions,
                         OutputDevice* pOleOut, const Rectangle& rRect );

    // Fill temporary doc with selected text for Print or PDF export.
    SwDoc * FillPrtDoc( SwDoc* pPrtDoc, const SfxPrinter* pPrt );

    // Called internally for Shell. Formats pages.
    void CalcPagesForPrint( sal_uInt16 nMax );

    // All about fields.
    void UpdateFlds(bool bCloseDB = false);
    bool IsAnyFieldInDoc() const;
    // Update all charts, for that exists any table.
    void UpdateAllCharts();
    bool HasCharts() const;

    // DOCUMENT COMPATIBILITY FLAGS START

    // Add or maximize paragraph spacing?
    void SetParaSpaceMax( bool bNew );

    // Add or maximize paragraph spacing?
    void SetParaSpaceMaxAtPages( bool bNew );

    // Compatible behaviour of tabs.
    void SetTabCompat( bool bNew );

    // Font metric attribute "External Leading" should be considered.
    void SetAddExtLeading( bool bNew );

    // Formatting by virtual device or printer.
    void SetUseVirDev( bool nNew );

    // Adding paragraph and table spacing at bottom
    // of table cells.
    void SetAddParaSpacingToTableCells( bool _bAddParaSpacingToTableCells );

    // Former formatting of text lines with
    // proportional line spacing or not.
    void SetUseFormerLineSpacing( bool _bUseFormerLineSpacing );

    // Former object positioning.
    void SetUseFormerObjectPositioning( bool _bUseFormerObjPos );

    void SetConsiderWrapOnObjPos( bool _bConsiderWrapOnObjPos );

    void SetUseFormerTextWrapping( bool _bUseFormerTextWrapping );

    void SetDoNotJustifyLinesWithManualBreak( bool _bDoNotJustifyLinesWithManualBreak );

    // DOCUMENT COMPATIBILITY FLAGS END

    // Calls Idle-formatter of Layout.
    void LayoutIdle();

    inline const SwViewOption *GetViewOptions() const { return mpOpt; }
    virtual void  ApplyViewOptions( const SwViewOption &rOpt );
           void  SetUIOptions( const SwViewOption &rOpt );
    virtual void  SetReadonlyOption(bool bSet);          // Set readonly-bit of ViewOptions.
           void  SetPDFExportOption(bool bSet);         // Set/reset PDF export mode.
           void  SetPrtFormatOption(bool bSet);         // Set PrtFormat-Bit of ViewOptions.
           void  SetReadonlySelectionOption(bool bSet); // Change the selection mode in readonly docs.

    const SwAccessibilityOptions* GetAccessibilityOptions() const { return mpAccOptions;}

    static void           SetShellRes( ShellResource* pRes ) { mpShellRes = pRes; }
    static ShellResource* GetShellRes();

    static void           SetCareWin( Window* pNew );
    static Window*        GetCareWin(SwViewShell& rVSh)
                          { return mpCareWindow ? mpCareWindow : CareChildWin(rVSh); }
    static Window*        CareChildWin(SwViewShell& rVSh);

    inline SfxViewShell   *GetSfxViewShell() { return mpSfxViewShell; }
    inline void           SetSfxViewShell(SfxViewShell *pNew) { mpSfxViewShell = pNew; }

    // Selection of  Draw Engine has been changed.
    virtual void DrawSelChanged();

    SwPagePreviewLayout* PagePreviewLayout();

    /** adjust view options for page preview

        Because page preview should show the document as it is printed -
        page preview is print preview -, the view options are adjusted to the
        same as for printing.

        @param _rPrintOptions
        input parameter - constant reference to print options, to which the
        view option will be adjusted.
    */
    void AdjustOptionsForPagePreview( SwPrintData const& rPrintOptions );

    bool IsViewLocked() const { return mbViewLocked; }
    void LockView( bool b )   { mbViewLocked = b;    }

    inline void LockPaint();
           void ImplLockPaint();
    inline void UnlockPaint( bool bVirDev = false );
           void ImplUnlockPaint( bool bVirDev );
           bool IsPaintLocked() const { return mnLockPaint != 0; }

    // Get/set DrawView and PageView.
    bool HasDrawView() const;
    void MakeDrawView();

    // DrawView may be used at UI.
          SdrView *GetDrawView();
    const SdrView *GetDrawView() const { return ((SwViewShell*)this)->GetDrawView(); }

    // Take care that MarkList is up-to-date in any case (Bug 57153).
    SdrView *GetDrawViewWithValidMarkList();

    // Query attribute pool.
    inline const SfxItemPool& GetAttrPool() const;
                 SfxItemPool& GetAttrPool();

    bool IsPreview() const { return mbPreview; }

    bool IsFrameView()  const { return mbFrameView; }
    void SetFrameView( const Size& rBrowseBorder )
           { mbFrameView = true; maBrowseBorder = rBrowseBorder; }

    // Makes the necessary invalidations:
    // If BrowseMode has changed bBrowseChgd == sal_True
    // or bBrowseChgd == sal_False in BrowseMode
    // if proportions have changed.
    void CheckBrowseView( bool bBrowseChgd );

    const Size& GetBrowseBorder() const;
    sal_Int32 GetBrowseWidth() const;
    void SetBrowseBorder( const Size& rNew );

    ::com::sun::star::uno::Reference< ::com::sun::star::accessibility::XAccessible > CreateAccessible();

    ::com::sun::star::uno::Reference<
        ::com::sun::star::accessibility::XAccessible >
            CreateAccessiblePreview();

    void ShowPreviewSelection( sal_uInt16 nSelPage );
    void InvalidateAccessibleFocus();

    // Apply Accessiblity options.
    void ApplyAccessiblityOptions(SvtAccessibilityOptions& rAccessibilityOptions);

    /** invalidate CONTENT_FLOWS_FROM/_TO relation for paragraphs

        @author OD

        @param _pFromTxtFrm
        input parameter - paragraph frame, for which the relation CONTENT_FLOWS_FROM
        has to be invalidated.
        If NULL, no CONTENT_FLOWS_FROM relation has to be invalidated

        @param _pToTxtFrm
        input parameter - paragraph frame, for which the relation CONTENT_FLOWS_TO
        has to be invalidated.
        If NULL, no CONTENT_FLOWS_TO relation has to be invalidated
    */
    void InvalidateAccessibleParaFlowRelation( const SwTxtFrm* _pFromTxtFrm,
                                               const SwTxtFrm* _pToTxtFrm );

    /** invalidate text selection for paragraphs

        @author OD
    */
    void InvalidateAccessibleParaTextSelection();

    /** invalidate attributes for paragraphs and paragraph's characters

        usage also for changes of the attributes of
        paragraph's characters.

        @author OD

        @param rTxtFrm
        input parameter - paragraph frame, whose attributes have changed
    */
    void InvalidateAccessibleParaAttrs( const SwTxtFrm& rTxtFrm );

    SwAccessibleMap* GetAccessibleMap();

    SwViewShell( SwViewShell&, Window *pWin = 0, OutputDevice *pOut = 0,
                long nFlags = 0 );
    SwViewShell( SwDoc& rDoc, Window *pWin,
               const SwViewOption *pOpt = 0, OutputDevice *pOut = 0,
               long nFlags = 0 );
    virtual ~SwViewShell();

    sal_Int32 GetPageNumAndSetOffsetForPDF( OutputDevice& rOut, const SwRect& rRect ) const;

    inline bool IsInConstructor() const { return mbInConstructor; }

    static const BitmapEx& GetReplacementBitmap( bool bIsErrorState );
    static void DeleteReplacementBitmaps();

    const SwPostItMgr* GetPostItMgr() const { return (const_cast<SwViewShell*>(this))->GetPostItMgr(); }
    SwPostItMgr* GetPostItMgr();

    /// Acts both for headers / footers, depending on the bShow(Header|Footer)Separator flags
    void ToggleHeaderFooterEdit();
    /// Acts both for headers / footers, depending on the bShow(Header|Footer)Separator flags
    bool IsHeaderFooterEdit() const { return mbHeaderFooterEdit; }
    bool IsShowHeaderFooterSeparator( FrameControlType eControl ) { return (eControl == Header)? mbShowHeaderSeparator: mbShowFooterSeparator; }
    virtual void SetShowHeaderFooterSeparator( FrameControlType eControl, bool bShow ) { if ( eControl == Header ) mbShowHeaderSeparator = bShow; else mbShowFooterSeparator = bShow; }
    bool IsSelectAll() { return mbSelectAll; }
};

// manages global ShellPointer
class CurrShell
{
public:
    SwViewShell *pPrev;
    SwRootFrm *pRoot;

    CurrShell( SwViewShell *pNew );
    ~CurrShell();
};

inline void SwViewShell::ResetInvalidRect()
{
   maInvalidRect.Clear();
}

inline void SwViewShell::StartAction()
{
    if ( !mnStartAction++ )
        ImplStartAction();
}
inline void SwViewShell::EndAction( const bool bIdleEnd )
{
    if( 0 == (mnStartAction - 1) )
        ImplEndAction( bIdleEnd );
    --mnStartAction;
}

inline void SwViewShell::LockPaint()
{
    if ( !mnLockPaint++ )
        ImplLockPaint();
}
inline void SwViewShell::UnlockPaint( bool bVirDev )
{
    if ( 0 == --mnLockPaint )
        ImplUnlockPaint( bVirDev );
}
inline const SfxItemPool& SwViewShell::GetAttrPool() const
{
    return ((SwViewShell*)this)->GetAttrPool();
}

#endif // INCLUDED_SW_INC_VIEWSH_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
