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

#ifndef INCLUDED_XMLOFF_TXTPARAE_HXX
#define INCLUDED_XMLOFF_TXTPARAE_HXX

#include <sal/config.h>
#include <xmloff/dllapi.h>
#include <rtl/ustring.hxx>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/uno/Sequence.h>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <xmloff/xmlexppr.hxx>
#include <xmloff/styleexp.hxx>
#include <xmloff/xmltoken.hxx>
#include <xmloff/SinglePropertySetInfoCache.hxx>
#include <vector>
#include <boost/scoped_ptr.hpp>

class XMLTextListsHelper;
class SvXMLExport;
class SvXMLAutoStylePoolP;
class XMLTextFieldExport;
class XMLTextNumRuleInfo;
class XMLTextListAutoStylePool;
class XMLSectionExport;
class XMLIndexMarkExport;
class XMLRedlineExport;
struct XMLPropertyState;
class MultiPropertySetHelper;

namespace com { namespace sun { namespace star
{
    namespace beans { class XPropertySet; class XPropertyState;
                      class XPropertySetInfo; }
    namespace container { class XEnumerationAccess; class XEnumeration; class XIndexAccess; }
    namespace text { class XTextContent; class XTextRange; class XText;
                     class XFootnote; class XTextFrame; class XTextSection;
                     class XTextField;
                     class XDocumentIndex; class XTextShapesSupplier; }
} } }

namespace xmloff
{
    class OFormLayerXMLExport;
    class BoundFrameSets;
}

class XMLOFF_DLLPUBLIC XMLTextParagraphExport : public XMLStyleExport
{
    struct Impl;
    ::boost::scoped_ptr<Impl> m_pImpl;

//  SvXMLExport& rExport;
    SvXMLAutoStylePoolP& rAutoStylePool;
    rtl::Reference < SvXMLExportPropertyMapper > xParaPropMapper;
    rtl::Reference < SvXMLExportPropertyMapper > xTextPropMapper;
    rtl::Reference < SvXMLExportPropertyMapper > xFramePropMapper;
    rtl::Reference < SvXMLExportPropertyMapper > xAutoFramePropMapper;
    rtl::Reference < SvXMLExportPropertyMapper > xSectionPropMapper;
    rtl::Reference < SvXMLExportPropertyMapper > xRubyPropMapper;

    const ::std::auto_ptr< ::xmloff::BoundFrameSets > pBoundFrameSets;
    XMLTextFieldExport          *pFieldExport;
    std::vector<OUString>  *pListElements;
    XMLTextListAutoStylePool    *pListAutoPool;
    XMLSectionExport            *pSectionExport;
    XMLIndexMarkExport          *pIndexMarkExport;

    /// may be NULL (if no redlines should be exported; e.g. in block mode)
    XMLRedlineExport            *pRedlineExport;
    std::vector<OUString>       *pHeadingStyles;

    bool                        bProgress;

    bool                        bBlock;

    // keep track of open rubies
    OUString                    sOpenRubyText;
    OUString                    sOpenRubyCharStyle;
    bool                        bOpenRuby;

    XMLTextListsHelper* mpTextListsHelper;
    ::std::vector< XMLTextListsHelper* > maTextListsHelperStack;

    enum FrameType { FT_TEXT, FT_GRAPHIC, FT_EMBEDDED, FT_SHAPE };
public:

    enum FieldmarkType { NONE, TEXT, CHECK }; // Used for simulating fieldmarks in OpenDocument 1.n Strict (for n <= 2). CHECK currently ignored.


    void exportTextRangeSpan(
            const ::com::sun::star::uno::Reference< com::sun::star::text::XTextRange > & rTextRange,
            ::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertySet > & xPropSet,
            ::com::sun::star::uno::Reference < ::com::sun::star::beans::XPropertySetInfo > & xPropSetInfo,
            const bool bIsUICharStyle,
            const bool bHasAutoStyle,
            const OUString& sStyle,
            bool& rPrevCharIsSpace,
            FieldmarkType& openFieldMark);

protected:

    const OUString sActualSize;
    // Implement Title/Description Elements UI (#i73249#)
    const OUString sTitle;
    const OUString sDescription;
    const OUString sAnchorCharStyleName;
    const OUString sAnchorPageNo;
    const OUString sAnchorType;
    const OUString sBeginNotice;
    const OUString sBookmark;
    const OUString sCategory;
    const OUString sChainNextName;
    const OUString sCharStyleName;
    const OUString sCharStyleNames;
    const OUString sContourPolyPolygon;
    const OUString sDocumentIndex;
    const OUString sDocumentIndexMark;
    const OUString sEndNotice;
    const OUString sFootnote;
    const OUString sFootnoteCounting;
    const OUString sFrame;
    const OUString sFrameHeightAbsolute;
    const OUString sFrameHeightPercent;
    const OUString sFrameStyleName;
    const OUString sFrameWidthAbsolute;
    const OUString sFrameWidthPercent;
    const OUString sGraphicFilter;
    const OUString sGraphicRotation;
    const OUString sGraphicURL;
    const OUString sReplacementGraphicURL;
    const OUString sHeight;
    const OUString sHoriOrient;
    const OUString sHoriOrientPosition;
    const OUString sHyperLinkName;
    const OUString sHyperLinkTarget;
    const OUString sHyperLinkURL;
    const OUString sIsAutomaticContour;
    const OUString sIsCollapsed;
    const OUString sIsPixelContour;
    const OUString sIsStart;
    const OUString sIsSyncHeightToWidth;
    const OUString sIsSyncWidthToHeight;
    const OUString sNumberingRules;
    const OUString sNumberingType;
    const OUString sPageDescName;
    const OUString sPageStyleName;
    const OUString sParaChapterNumberingLevel;
    const OUString sParaConditionalStyleName;
    const OUString sParagraphService;
    const OUString sParaStyleName;
    const OUString sPositionEndOfDoc;
    const OUString sPrefix;
    const OUString sRedline;
    const OUString sReferenceId;
    const OUString sReferenceMark;
    const OUString sRelativeHeight;
    const OUString sRelativeWidth;
    const OUString sRuby;
    const OUString sRubyAdjust;
    const OUString sRubyCharStyleName;
    const OUString sRubyText;
    const OUString sServerMap;
    const OUString sShapeService;
    const OUString sSizeType;
    const OUString sSoftPageBreak;
    const OUString sStartAt;
    const OUString sSuffix;
    const OUString sTableService;
    const OUString sText;
    const OUString sTextContentService;
    const OUString sTextEmbeddedService;
    const OUString sTextEndnoteService;
    const OUString sTextField;
    const OUString sTextFieldService;
    const OUString sTextFrameService;
    const OUString sTextGraphicService;
    const OUString sTextPortionType;
    const OUString sTextSection;
    const OUString sUnvisitedCharStyleName;
    const OUString sVertOrient;
    const OUString sVertOrientPosition;
    const OUString sVisitedCharStyleName;
    const OUString sWidth;
    const OUString sWidthType;
    const OUString sTextFieldStart;
    const OUString sTextFieldEnd;
    const OUString sTextFieldStartEnd;

    SinglePropertySetInfoCache aCharStyleNamesPropInfoCache;

    SvXMLAutoStylePoolP& GetAutoStylePool() { return rAutoStylePool; }
    const SvXMLAutoStylePoolP& GetAutoStylePool() const { return rAutoStylePool; }

public:
    rtl::Reference < SvXMLExportPropertyMapper > GetParaPropMapper() const
    {
        return xParaPropMapper;
    }

    rtl::Reference < SvXMLExportPropertyMapper > GetTextPropMapper() const
    {
        return xTextPropMapper;
    }

    rtl::Reference < SvXMLExportPropertyMapper > GetFramePropMapper() const
    {
        return xFramePropMapper;
    }
    rtl::Reference < SvXMLExportPropertyMapper > GetAutoFramePropMapper() const
    {
        return xAutoFramePropMapper;
    }
    rtl::Reference < SvXMLExportPropertyMapper > GetSectionPropMapper() const
    {
        return xSectionPropMapper;
    }
    rtl::Reference < SvXMLExportPropertyMapper > GetRubyPropMapper() const
    {
        return xRubyPropMapper;
    }

    OUString FindTextStyleAndHyperlink(
            const ::com::sun::star::uno::Reference <
                ::com::sun::star::beans::XPropertySet > & rPropSet,
            bool& rbHyperlink,
            bool& rbHasCharStyle,
            bool& rbHasAutoStyle,
            const XMLPropertyState** pAddState = NULL) const;
    bool addHyperlinkAttributes(
        const ::com::sun::star::uno::Reference <
                ::com::sun::star::beans::XPropertySet > & rPropSet,
        const ::com::sun::star::uno::Reference <
                ::com::sun::star::beans::XPropertyState > & rPropState,
        const ::com::sun::star::uno::Reference <
                ::com::sun::star::beans::XPropertySetInfo > & rPropSetInfo );

    void exportTextRangeEnumeration(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::container::XEnumeration > & rRangeEnum,
        bool bAutoStyles, bool bProgress,
        bool bPrvChrIsSpc = true );

protected:

    sal_Int32 addTextFrameAttributes(
        const ::com::sun::star::uno::Reference <
                ::com::sun::star::beans::XPropertySet >& rPropSet,
        bool bShape,
        OUString *pMinHeightValue = 0,
        OUString *pMinWidthValue = 0 );

    virtual void exportStyleAttributes(
        const ::com::sun::star::uno::Reference<
                ::com::sun::star::style::XStyle > & rStyle ) SAL_OVERRIDE;

    void exportPageFrames( bool bAutoStyles, bool bProgress );
    void exportFrameFrames( bool bAutoStyles, bool bProgress,
            const ::com::sun::star::uno::Reference <
                    ::com::sun::star::text::XTextFrame > *pParentTxtFrame = 0 );

    void exportNumStyles( bool bUsed );

    void exportText(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XText > & rText,
        bool bAutoStyles, bool bProgress, bool bExportParagraph );

    void exportText(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XText > & rText,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextSection > & rBaseSection,
        bool bAutoStyles, bool bProgress, bool bExportParagraph );

    bool exportTextContentEnumeration(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::container::XEnumeration > & rContentEnum,
        bool bAutoStyles,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextSection > & rBaseSection,
        bool bProgress,
        bool bExportParagraph = true,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet = 0,
        bool bExportLevels = true );
    void exportParagraph(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles, bool bProgress,
        bool bExportParagraph,
        MultiPropertySetHelper& rPropSetHelper);
    virtual void exportTable(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles, bool bProgress );

    void exportTextField(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextRange > & rTextRange,
        bool bAutoStyles, bool bProgress );

    void exportTextField(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextField> & xTextField,
        const bool bAutoStyles, const bool bProgress,
        const bool bRecursive );

    void exportAnyTextFrame(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        FrameType eTxpe,
        bool bAutoStyles, bool bProgress, bool bExportContent,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet = 0 );
    void _exportTextFrame(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySetInfo > & rPropSetInfo,
        bool bProgress );
    inline void exportTextFrame(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles, bool bProgress, bool bExportContent,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet = 0 );
    inline void exportShape(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet = 0  );

    void exportContour(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySetInfo > & rPropSetInfo );
    void _exportTextGraphic(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySetInfo > & rPropSetInfo );
    inline void exportTextGraphic(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet = 0  );

    virtual void _collectTextEmbeddedAutoStyles(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet );
    virtual void _exportTextEmbedded(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySetInfo > & rPropSetInfo );
    inline void exportTextEmbedded(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet = 0  );
    virtual void setTextEmbeddedGraphicURL(
        const ::com::sun::star::uno::Reference <
                ::com::sun::star::beans::XPropertySet >& rPropSet,
        OUString& rStreamName ) const;

    /// export a footnote and styles
    void exportTextFootnote(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        const OUString& sString,
        bool bAutoStyles, bool bProgress );

    /// helper for exportTextFootnote
    void exportTextFootnoteHelper(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::text::XFootnote > & rPropSet,
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::text::XText> & rText,
        const OUString& sString,
        bool bAutoStyles,
        bool bIsEndnote, bool bProgress );

    /// export footnote and endnote configuration elements
    void exportTextFootnoteConfiguration();

    void exportTextFootnoteConfigurationHelper(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::beans::XPropertySet> & rFootnoteSupplier,
        bool bIsEndnote);

    void exportTextMark(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::beans::XPropertySet> & xPropSet,
        const OUString& rProperty,
        const enum ::xmloff::token::XMLTokenEnum pElements[],
        bool bAutoStyles);

    void exportIndexMark(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::beans::XPropertySet> & rPropSet,
        bool bAutoStyles);

    void exportSoftPageBreak(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::beans::XPropertySet> & rPropSet,
        bool bAutoStyles);

    void exportTextRange(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextRange > & rTextRange,
        bool bAutoStyles,
        bool& rPrevCharWasSpace,
        FieldmarkType& openFieldmarkType );

    void exportListChange( const XMLTextNumRuleInfo& rPrvInfo,
                           const XMLTextNumRuleInfo& rNextInfo );

    /// check if current section or current list has changed;
    /// calls exortListChange as appropriate
    void exportListAndSectionChange(
        ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextSection > & rOldSection,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextSection > & rNewSection,
        const XMLTextNumRuleInfo& rOldList,
        const XMLTextNumRuleInfo& rNewList,
        bool bAutoStyles );

    /// overload for exportListAndSectionChange;
    /// takes new content rather than new section.
    void exportListAndSectionChange(
        ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextSection > & rOldSection,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rNewContent,
        const XMLTextNumRuleInfo& rOldList,
        const XMLTextNumRuleInfo& rNewList,
        bool bAutoStyles );
    void exportListAndSectionChange(
        ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextSection > & rOldSection,
        MultiPropertySetHelper& rPropSetHelper,
        sal_Int16 nTextSectionId,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rNewContent,
        const XMLTextNumRuleInfo& rOldList,
        const XMLTextNumRuleInfo& rNewList,
        bool bAutoStyles );

    /// export a redline text portion
    void exportChange(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        bool bAutoStyle);

    /// export a ruby
    void exportRuby(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::beans::XPropertySet> & rPortionPropSet,
        bool bAutoStyles );

    /// export a text:meta
    void exportMeta(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::beans::XPropertySet> & i_xPortion,
        bool i_bAutoStyles, bool i_isProgress );

public:

    XMLTextParagraphExport(
            SvXMLExport& rExp,
               SvXMLAutoStylePoolP & rASP
                          );
    virtual ~XMLTextParagraphExport();

    /// add autostyle for specified family
    void Add(
        sal_uInt16 nFamily,
        MultiPropertySetHelper& rPropSetHelper,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        const XMLPropertyState** pAddState = NULL );
    void Add(
        sal_uInt16 nFamily,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        const XMLPropertyState** pAddState = NULL, bool bDontSeek = false );

    /// find style name for specified family and parent
    OUString Find(
        sal_uInt16 nFamily,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > & rPropSet,
        const OUString& rParent,
        const XMLPropertyState** pAddState = NULL ) const;

    static SvXMLExportPropertyMapper *CreateShapeExtPropMapper(
                                                SvXMLExport& rExport );
    static SvXMLExportPropertyMapper *CreateCharExtPropMapper(
                                                SvXMLExport& rExport);
    static SvXMLExportPropertyMapper *CreateParaExtPropMapper(
                                                SvXMLExport& rExport);
    static SvXMLExportPropertyMapper *CreateParaDefaultExtPropMapper(
                                                SvXMLExport& rExport);

    // This methods exports all (or all used) styles
    void exportTextStyles( bool bUsed
                           , bool bProg = false
                         );

    /// This method exports (text field) declarations etc.
    void exportTextDeclarations();

    /// export the (text field) declarations for a particular XText
    void exportTextDeclarations(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XText > & rText );

    /// true: export only those declarations that are used;
    /// false: export all declarations
    void exportUsedDeclarations( bool bOnlyUsed );

    /// Export the list of change information (enclosed by <tracked-changes>)
    /// (or the necessary automatic styles)
    void exportTrackedChanges(bool bAutoStyle);

    /// Export the list of change information (enclosed by <tracked-changes>)
    /// (or the necessary automatic styles)
    void exportTrackedChanges(const ::com::sun::star::uno::Reference <
                                  ::com::sun::star::text::XText > & rText,
                              bool bAutoStyle );

    /// Record tracked changes for this particular XText
    /// (empty reference stop recording)
    /// This should be used if tracked changes for e.g. footers are to
    /// be exported separately via the exportTrackedChanges(bool,
    /// Reference<XText>) method.
    void recordTrackedChangesForXText(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XText > & rText );


    /// Stop recording tracked changes.
    /// This is the same as calling recordTrackedChanges(...) with an
    /// empty reference.
    void recordTrackedChangesNoXText();


    // This method exports the given OUString
    void exportText(
        const OUString& rText,
        bool& rPrevCharWasSpace );

    // This method collects all automatic styles for the given XText
    void collectTextAutoStyles(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XText > & rText,
        bool bIsProgress = false,
        bool bExportParagraph = true )
    {
        exportText( rText, true, bIsProgress, bExportParagraph );
    }

    void collectTextAutoStyles(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XText > & rText,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextSection > & rBaseSection,
        bool bIsProgress = false,
        bool bExportParagraph = true )
    {
        exportText( rText, rBaseSection, true, bIsProgress, bExportParagraph );
    }

    // It the model implements the xAutoStylesSupplier interface, the automatic
    // styles can exported without iterating over the text portions
    bool collectTextAutoStylesOptimized(
        bool bIsProgress = false );

    // This method exports all automatic styles that have been collected.
    virtual void exportTextAutoStyles();

    void exportEvents( const ::com::sun::star::uno::Reference < com::sun::star::beans::XPropertySet > & rPropSet );

    // Implement Title/Description Elements UI (#i73249#)
    void exportTitleAndDescription( const ::com::sun::star::uno::Reference < ::com::sun::star::beans::XPropertySet > & rPropSet,
                                    const ::com::sun::star::uno::Reference < ::com::sun::star::beans::XPropertySetInfo > & rPropSetInfo );

    // This method exports the given XText
    void exportText(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XText > & rText,
        bool bIsProgress = false,
        bool bExportParagraph = true)
    {
        exportText( rText, false, bIsProgress, bExportParagraph );
    }

    void exportText(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XText > & rText,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextSection > & rBaseSection,
        bool bIsProgress = false,
        bool bExportParagraph = true)
    {
        exportText( rText, rBaseSection, false, bIsProgress, bExportParagraph );
    }

    void exportFramesBoundToPage( bool bIsProgress = false )
    {
        exportPageFrames( false, bIsProgress );
    }
    void exportFramesBoundToFrame(
            const ::com::sun::star::uno::Reference <
                    ::com::sun::star::text::XTextFrame >& rParentTxtFrame,
            bool bIsProgress = false )
    {
        exportFrameFrames( false, bIsProgress, &rParentTxtFrame );
    }
    inline const XMLTextListAutoStylePool& GetListAutoStylePool() const;

    void SetBlockMode( bool bSet ) { bBlock = bSet; }
    bool IsBlockMode() const { return bBlock; }


    rtl::Reference < SvXMLExportPropertyMapper > GetParagraphPropertyMapper() const
    {
        return xParaPropMapper;
    }


    /** exclude form controls which are in mute sections.
     *
     * This method is necessary to prevent the form layer export from exporting
     * control models whose controls are not represented in the document.  To
     * achieve this, this method iterates over all shapes, checks to see if
     * they are control shapes, and if so, whether they should be exported or
     * not. If not, the form layer export will be notified accordingly.
     *
     * The reason this method is located here is tha it needs to access the
     * XMLSectionExport, which is only available here.
     */
    void PreventExportOfControlsInMuteSections(
        const ::com::sun::star::uno::Reference<
            ::com::sun::star::container::XIndexAccess> & rShapes,
        rtl::Reference<xmloff::OFormLayerXMLExport> xFormExport );

    SinglePropertySetInfoCache& GetCharStyleNamesPropInfoCache() { return aCharStyleNamesPropInfoCache; }

    void PushNewTextListsHelper();

    void PopTextListsHelper();

private:
        XMLTextParagraphExport(XMLTextParagraphExport &); // private copy-ctor because of explicit copy-ctor of auto_ptr
};

inline const XMLTextListAutoStylePool&
    XMLTextParagraphExport::GetListAutoStylePool() const
{
    return *pListAutoPool;
}

inline void XMLTextParagraphExport::exportTextFrame(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles, bool bIsProgress, bool bExportContent,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet)
{
    exportAnyTextFrame( rTextContent, FT_TEXT, bAutoStyles, bIsProgress,
                        bExportContent, pRangePropSet );
}

inline void XMLTextParagraphExport::exportTextGraphic(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet )
{
    exportAnyTextFrame( rTextContent, FT_GRAPHIC, bAutoStyles, false,
                        true, pRangePropSet );
}

inline void XMLTextParagraphExport::exportTextEmbedded(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet )
{
    exportAnyTextFrame( rTextContent, FT_EMBEDDED, bAutoStyles, false,
                        true, pRangePropSet );
}

inline void XMLTextParagraphExport::exportShape(
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::text::XTextContent > & rTextContent,
        bool bAutoStyles,
        const ::com::sun::star::uno::Reference <
            ::com::sun::star::beans::XPropertySet > *pRangePropSet )
{
    exportAnyTextFrame( rTextContent, FT_SHAPE, bAutoStyles, false,
                        true, pRangePropSet );
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
