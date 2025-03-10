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

#ifndef INCLUDED_SC_SOURCE_FILTER_INC_HTMLEXP_HXX
#define INCLUDED_SC_SOURCE_FILTER_INC_HTMLEXP_HXX

#include "global.hxx"
#include <rtl/textenc.h>
#include <tools/gen.hxx>
#include <tools/color.hxx>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/scoped_ptr.hpp>

#include "expbase.hxx"

class ScDocument;
class SfxItemSet;
class SdrPage;
class Graphic;
class SdrObject;
class OutputDevice;
class ScDrawLayer;
class EditTextObject;

namespace editeng { class SvxBorderLine; }

struct ScHTMLStyle
{   // Defaults aus StyleSheet
    Color               aBackgroundColor;
    OUString            aFontFamilyName;
    sal_uInt32          nFontHeight;        // Item-Value
    sal_uInt16          nFontSizeNumber;    // HTML value 1-7
    sal_uInt8           nDefaultScriptType; // Font values are valid for the default script type
    bool                bInitialized;

    ScHTMLStyle() :
        nFontHeight(0),
        nFontSizeNumber(2),
        nDefaultScriptType(),
        bInitialized(false)
    {}

    const ScHTMLStyle& operator=( const ScHTMLStyle& rScHTMLStyle )
    {
        aBackgroundColor   = rScHTMLStyle.aBackgroundColor;
        aFontFamilyName    = rScHTMLStyle.aFontFamilyName;
        nFontHeight        = rScHTMLStyle.nFontHeight;
        nFontSizeNumber    = rScHTMLStyle.nFontSizeNumber;
        nDefaultScriptType = rScHTMLStyle.nDefaultScriptType;
        bInitialized       = rScHTMLStyle.bInitialized;
        return *this;
    }
};

struct ScHTMLGraphEntry
{
    ScRange             aRange;         // ueberlagerter Zellbereich
    Size                aSize;          // Groesse in Pixeln
    Size                aSpace;         // Spacing in Pixeln
    SdrObject*          pObject;
    bool                bInCell;        // ob in Zelle ausgegeben wird
    bool                bWritten;

    ScHTMLGraphEntry( SdrObject* pObj, const ScRange& rRange,
                      const Size& rSize,  bool bIn, const Size& rSpace ) :
        aRange( rRange ),
        aSize( rSize ),
        aSpace( rSpace ),
        pObject( pObj ),
        bInCell( bIn ),
        bWritten( false )
    {}
};

#define SC_HTML_FONTSIZES 7
const short nIndentMax = 23;

class ScHTMLExport : public ScExportBase
{
    // default HtmlFontSz[1-7]
    static const sal_uInt16 nDefaultFontSize[SC_HTML_FONTSIZES];
    // HtmlFontSz[1-7] in s*3.ini [user]
    static sal_uInt16       nFontSize[SC_HTML_FONTSIZES];
    static const char*  pFontSizeCss[SC_HTML_FONTSIZES];
    static const sal_uInt16 nCellSpacing;
    static const sal_Char sIndentSource[];

    typedef boost::scoped_ptr<std::map<OUString, OUString> > FileNameMapPtr;
    typedef boost::ptr_vector<ScHTMLGraphEntry> GraphEntryList;

    GraphEntryList   aGraphList;
    ScHTMLStyle      aHTMLStyle;
    OUString         aBaseURL;
    OUString         aStreamPath;
    OUString         aFilterOptions;
    OUString         aCId;           // Content-Id fuer Mail-Export
    OutputDevice*    pAppWin;        // fuer Pixelei
    FileNameMapPtr   pFileNameMap;        // fuer CopyLocalFileToINet
    OUString         aNonConvertibleChars;   // collect nonconvertible characters
    rtl_TextEncoding eDestEnc;
    SCTAB            nUsedTables;
    short            nIndent;
    sal_Char         sIndent[nIndentMax+1];
    bool             bAll;           // ganzes Dokument
    bool             bTabHasGraphics;
    bool             bTabAlignedLeft;
    bool             bCalcAsShown;
    bool             bCopyLocalFileToINet;
    bool             bTableDataWidth;
    bool             bTableDataHeight;
    bool             mbSkipImages;

    const SfxItemSet& PageDefaults( SCTAB nTab );

    void WriteBody();
    void WriteHeader();
    void WriteOverview();
    void WriteTables();
    void WriteCell( SCCOL nCol, SCROW nRow, SCTAB nTab );
    void WriteGraphEntry( ScHTMLGraphEntry* );
    void WriteImage( OUString& rLinkName,
                     const Graphic&, const OString& rImgOptions,
                     sal_uLong nXOutFlags = 0 );
            // nXOutFlags fuer XOutBitmap::WriteGraphic

    // write to stream if and only if URL fields in edit cell
    bool WriteFieldText( const EditTextObject* pData );

    // kopiere ggfs. eine lokale Datei ins Internet
    bool CopyLocalFileToINet( OUString& rFileNm, const OUString& rTargetNm, bool bFileToFile = false );
    bool HasCId()
    {
        return !aCId.isEmpty();
    }
    void MakeCIdURL( OUString& rURL );

    void PrepareGraphics( ScDrawLayer*, SCTAB nTab,
                          SCCOL nStartCol, SCROW nStartRow,
                          SCCOL nEndCol, SCROW nEndRow );

    void FillGraphList( const SdrPage*, SCTAB nTab,
                        SCCOL nStartCol, SCROW nStartRow,
                        SCCOL nEndCol, SCROW nEndRow );

    OString BorderToStyle(const char* pBorderName,
                          const editeng::SvxBorderLine* pLine,
                          bool& bInsertSemicolon);

    sal_uInt16  GetFontSizeNumber( sal_uInt16 nHeight );
    const char* GetFontSizeCss( sal_uInt16 nHeight );
    sal_uInt16  ToPixel( sal_uInt16 nTwips );
    Size        MMToPixel( const Size& r100thMMSize );
    void        IncIndent( short nVal );

    const sal_Char* GetIndentStr()
    {
        return sIndent;
    }

public:
                        ScHTMLExport( SvStream&, const OUString&, ScDocument*, const ScRange&,
                                      bool bAll, const OUString& aStreamPath, const OUString& rFilterOptions );
    virtual             ~ScHTMLExport();
    sal_uLong           Write();
    const OUString&     GetNonConvertibleChars() const
    {
        return aNonConvertibleChars;
    }
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
