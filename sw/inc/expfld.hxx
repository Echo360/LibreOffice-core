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
#ifndef INCLUDED_SW_INC_EXPFLD_HXX
#define INCLUDED_SW_INC_EXPFLD_HXX

#include "swdllapi.h"
#include <fldbas.hxx>
#include <cellfml.hxx>
#include <set>
#include <vector>

class SfxPoolItem;
class SwTxtNode;
class SwFrm;
struct SwPosition;
class SwTxtFld;
class SwDoc;
class SwFmtFld;
class _SetGetExpFlds;
class SwEditShell;

/// Forward declaration: get "BodyTxtNode" for exp.fld in Fly's headers/footers/footnotes.
const SwTxtNode* GetBodyTxtNode( const SwDoc& pDoc, SwPosition& rPos,
                                 const SwFrm& rFrm );

OUString ReplacePoint(const OUString& sTmpName, bool bWithCommandType = false);

struct _SeqFldLstElem
{
    OUString sDlgEntry;
    sal_uInt16 nSeqNo;

    _SeqFldLstElem( const OUString& rStr, sal_uInt16 nNo )
        : sDlgEntry( rStr ), nSeqNo( nNo )
    {}
};

class SW_DLLPUBLIC SwSeqFldList
{
    std::vector<_SeqFldLstElem*> maData;
public:
    ~SwSeqFldList()
    {
        for( std::vector<_SeqFldLstElem*>::const_iterator it = maData.begin(); it != maData.end(); ++it )
            delete *it;
    }

    bool InsertSort(_SeqFldLstElem* pNew);
    bool SeekEntry(const _SeqFldLstElem& rNew, sal_uInt16* pPos) const;

    sal_uInt16 Count() { return maData.size(); }
    _SeqFldLstElem* operator[](sal_uInt16 nIndex) { return maData[nIndex]; }
    const _SeqFldLstElem* operator[](sal_uInt16 nIndex) const { return maData[nIndex]; }
    void Clear() { maData.clear(); }
};

class SwGetExpFieldType : public SwValueFieldType
{
public:
        SwGetExpFieldType(SwDoc* pDoc);
        virtual SwFieldType*    Copy() const SAL_OVERRIDE;

        /** Overlay, because get-field cannot be changed and therefore
         does not need to be updated. Update at changing of set-values! */
protected:
   virtual void Modify( const SfxPoolItem* pOld, const SfxPoolItem *pNew ) SAL_OVERRIDE;
};

class SW_DLLPUBLIC SwGetExpField : public SwFormulaField
{
    OUString        sExpand;
    bool            bIsInBodyTxt;
    sal_uInt16          nSubType;

    bool            bLateInitialization; // #i82544#

    virtual OUString            Expand() const SAL_OVERRIDE;
    virtual SwField*            Copy() const SAL_OVERRIDE;

public:
    SwGetExpField( SwGetExpFieldType*, const OUString& rFormel,
                   sal_uInt16 nSubType = nsSwGetSetExpType::GSE_EXPR, sal_uLong nFmt = 0);

    virtual void                SetValue( const double& rVal ) SAL_OVERRIDE;
    virtual void                SetLanguage(sal_uInt16 nLng) SAL_OVERRIDE;

    inline OUString             GetExpStr() const;
    inline void                 ChgExpStr(const OUString& rExpand);

    /// Called by formatting.
    inline bool                 IsInBodyTxt() const;

    /// Set by UpdateExpFlds where node position is known.
    inline void                 ChgBodyTxtFlag( bool bIsInBody );

    /** For fields in header/footer/footnotes/flys:
     Only called by formatting!! */
    void                        ChangeExpansion( const SwFrm&, const SwTxtFld& );

    virtual OUString    GetFieldName() const SAL_OVERRIDE;

    /// Change formula.
    virtual OUString GetPar2() const SAL_OVERRIDE;
    virtual void        SetPar2(const OUString& rStr) SAL_OVERRIDE;

    virtual sal_uInt16  GetSubType() const SAL_OVERRIDE;
    virtual void        SetSubType(sal_uInt16 nType) SAL_OVERRIDE;
    virtual bool        QueryValue( com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) const SAL_OVERRIDE;
    virtual bool        PutValue( const com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) SAL_OVERRIDE;

    static sal_Int32    GetReferenceTextPos( const SwFmtFld& rFmt, SwDoc& rDoc, sal_Int32 nHint = 0);
    // #i82544#
    void                SetLateInitialization() { bLateInitialization = true;}
};

inline void SwGetExpField::ChgExpStr(const OUString& rExpand)
    { sExpand = rExpand;}

inline OUString SwGetExpField::GetExpStr() const
    { return sExpand;   }

 /// Called by formatting.
inline bool SwGetExpField::IsInBodyTxt() const
    { return bIsInBodyTxt; }

 /// Set by UpdateExpFlds where node position is known.
inline void SwGetExpField::ChgBodyTxtFlag( bool bIsInBody )
    { bIsInBodyTxt = bIsInBody; }

class SwSetExpField;

class SW_DLLPUBLIC SwSetExpFieldType : public SwValueFieldType
{
    OUString sName;
    const SwNode* pOutlChgNd;
    OUString      sDelim;
    sal_uInt16      nType;
    sal_uInt8       nLevel;
    bool        bDeleted;

protected:
   virtual void Modify( const SfxPoolItem* pOld, const SfxPoolItem *pNew ) SAL_OVERRIDE;

public:
    SwSetExpFieldType( SwDoc* pDoc, const OUString& rName,
                        sal_uInt16 nType = nsSwGetSetExpType::GSE_EXPR );
    virtual SwFieldType*    Copy() const SAL_OVERRIDE;
    virtual OUString        GetName() const SAL_OVERRIDE;

    inline void             SetType(sal_uInt16 nTyp);
    inline sal_uInt16       GetType() const;

    void                    SetSeqFormat(sal_uLong nFormat);
    sal_uLong               GetSeqFormat();

    bool                IsDeleted() const       { return bDeleted; }
    void                    SetDeleted( bool b )    { bDeleted = b; }

    /// Overlay, because set-field takes care for its being updated by itself.
    inline OUString         GetSetRefName() const;

    sal_uInt16 SetSeqRefNo( SwSetExpField& rFld );

    sal_uInt16 GetSeqFldList( SwSeqFldList& rList );
    OUString MakeSeqName( sal_uInt16 nSeqNo );

    /// Number sequence fields chapterwise if required.
    OUString GetDelimiter() const             { return sDelim; }
    void SetDelimiter( const OUString& s )    { sDelim = s; }
    sal_uInt8 GetOutlineLvl() const                 { return nLevel; }
    void SetOutlineLvl( sal_uInt8 n )           { nLevel = n; }
    void SetChapter( SwSetExpField& rFld, const SwNode& rNd );

    /** Member only for SwDoc::UpdateExpFld.
     It is needed only at runtime of sequence field types! */
    const SwNode* GetOutlineChgNd() const   { return pOutlChgNd; }
    void SetOutlineChgNd( const SwNode* p ) { pOutlChgNd = p; }

    virtual bool        QueryValue( com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) const SAL_OVERRIDE;
    virtual bool        PutValue( const com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) SAL_OVERRIDE;
};

inline void SwSetExpFieldType::SetType( sal_uInt16 nTyp )
{
        nType = nTyp;
        EnableFormat( !(nType & (nsSwGetSetExpType::GSE_SEQ|nsSwGetSetExpType::GSE_STRING)));
}

inline sal_uInt16 SwSetExpFieldType::GetType() const
    { return nType;   }

inline OUString SwSetExpFieldType::GetSetRefName() const
    { return sName; }

class SW_DLLPUBLIC SwSetExpField : public SwFormulaField
{
    OUString        sExpand;
    OUString        aPText;
    OUString        aSeqText;
    bool            bInput;
    sal_uInt16          nSeqNo;
    sal_uInt16          nSubType;

    virtual OUString            Expand() const SAL_OVERRIDE;
    virtual SwField*            Copy() const SAL_OVERRIDE;

public:
    SwSetExpField(SwSetExpFieldType*, const OUString& rFormel, sal_uLong nFmt = 0);

    virtual void                SetValue( const double& rVal ) SAL_OVERRIDE;

    inline OUString             GetExpStr() const;

    inline void                 ChgExpStr( const OUString& rExpand );

    inline void                 SetPromptText(const OUString& rStr);
    inline OUString             GetPromptText() const;

    inline void                 SetInputFlag(bool bInp);
    inline bool                 GetInputFlag() const;

    virtual OUString            GetFieldName() const SAL_OVERRIDE;

    virtual sal_uInt16              GetSubType() const SAL_OVERRIDE;
    virtual void                SetSubType(sal_uInt16 nType) SAL_OVERRIDE;

    inline bool                 IsSequenceFld() const;

    /// Logical number, sequence fields.
    inline void                 SetSeqNumber( sal_uInt16 n )    { nSeqNo = n; }
    inline sal_uInt16           GetSeqNumber() const        { return nSeqNo; }

    /// Query name only.
    virtual OUString       GetPar1()   const SAL_OVERRIDE;

    /// Query formula.
    virtual OUString       GetPar2()   const SAL_OVERRIDE;
    virtual void                SetPar2(const OUString& rStr) SAL_OVERRIDE;
    virtual bool        QueryValue( com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) const SAL_OVERRIDE;
    virtual bool        PutValue( const com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) SAL_OVERRIDE;
};

inline OUString SwSetExpField::GetExpStr() const
    { return sExpand;       }

inline void SwSetExpField::ChgExpStr( const OUString& rExpand )
    { sExpand = rExpand;    }

inline void  SwSetExpField::SetPromptText(const OUString& rStr)
    { aPText = rStr;        }

inline OUString SwSetExpField::GetPromptText() const
    { return aPText;        }

inline void SwSetExpField::SetInputFlag(bool bInp)
    { bInput = bInp; }

inline bool SwSetExpField::GetInputFlag() const
    { return bInput; }

inline bool SwSetExpField::IsSequenceFld() const
    { return 0 != (nsSwGetSetExpType::GSE_SEQ & ((SwSetExpFieldType*)GetTyp())->GetType()); }

class SwInputFieldType : public SwFieldType
{
    SwDoc* pDoc;
public:
    SwInputFieldType( SwDoc* pDoc );

    virtual SwFieldType* Copy() const SAL_OVERRIDE;

    SwDoc* GetDoc() const { return pDoc; }
};

class SW_DLLPUBLIC SwInputField : public SwField
{
    mutable OUString aContent;
    OUString aPText;
    OUString aHelp;
    OUString aToolTip;
    sal_uInt16 nSubType;
    bool mbIsFormField;

    SwFmtFld* mpFmtFld; // attribute to which the <SwInputField> belongs to

    virtual OUString        Expand() const SAL_OVERRIDE;
    virtual SwField*        Copy() const SAL_OVERRIDE;

    // Accessing Input Field's content
    const OUString& getContent() const { return aContent;}

    void LockNotifyContentChange();
    void UnlockNotifyContentChange();

public:
    /// Direct input via dialog; delete old value.
    SwInputField(
        SwInputFieldType* pFieldType,
        const OUString& rContent,
        const OUString& rPrompt,
        sal_uInt16 nSubType = 0,
        sal_uLong nFmt = 0,
        bool bIsFormField = true );
    virtual ~SwInputField();

    void SetFmtFld( SwFmtFld& rFmtFld );
    SwFmtFld* GetFmtFld() { return mpFmtFld;}

    // Providing new Input Field's content:
    // Fill Input Field's content depending on <nSupType>.
    void applyFieldContent( const OUString& rNewFieldContent );

    bool isFormField() const;

    virtual OUString        GetFieldName() const SAL_OVERRIDE;

    /// Content
    virtual OUString        GetPar1() const SAL_OVERRIDE;
    virtual void            SetPar1(const OUString& rStr) SAL_OVERRIDE;

    /// aPromptText
    virtual OUString   GetPar2() const SAL_OVERRIDE;
    virtual void            SetPar2(const OUString& rStr) SAL_OVERRIDE;

    virtual OUString        GetHelp() const;
    virtual void            SetHelp(const OUString & rStr);

    virtual OUString        GetToolTip() const;
    virtual void            SetToolTip(const OUString & rStr);

    virtual sal_uInt16      GetSubType() const SAL_OVERRIDE;
    virtual void            SetSubType(sal_uInt16 nSub) SAL_OVERRIDE;
    virtual bool        QueryValue( com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) const SAL_OVERRIDE;
    virtual bool        PutValue( const com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) SAL_OVERRIDE;
};

// Sorted list of input fields and DropDown fields
class SwInputFieldList
{
public:
    SwInputFieldList( SwEditShell* pShell, bool bBuildTmpLst = false );
    ~SwInputFieldList();

    size_t      Count() const;
    SwField*    GetField(size_t nId);

    void        GotoFieldPos(size_t nId);
    void        PushCrsr();
    void        PopCrsr();

    /** Put all that are new into SortLst for updating. @return true if not empty.
     (For Glossary: only update its input-fields).
     Compare TmpLst with current fields. */
    bool        BuildSortLst();

private:
    SwEditShell*              pSh;
    _SetGetExpFlds*           pSrtLst;
    std::set<const SwTxtFld*> aTmpLst;
};

 /// Implementation in tblcalc.cxx.
class SwTblFieldType : public SwValueFieldType
{
public:
    SwTblFieldType(SwDoc* pDocPtr);
    virtual SwFieldType* Copy() const SAL_OVERRIDE;
};

class SwTblField : public SwValueField, public SwTableFormula
{
    OUString      sExpand;
    sal_uInt16      nSubType;

    virtual OUString    Expand() const SAL_OVERRIDE;
    virtual SwField*    Copy() const SAL_OVERRIDE;

    /// Search TextNode containing the field.
    virtual const SwNode* GetNodeOfFormula() const SAL_OVERRIDE;

    OUString GetCommand();

public:
    SwTblField( SwTblFieldType*, const OUString& rFormel,
                sal_uInt16 nSubType = 0, sal_uLong nFmt = 0);

    virtual void        SetValue( const double& rVal ) SAL_OVERRIDE;
    virtual sal_uInt16      GetSubType() const SAL_OVERRIDE;
    virtual void        SetSubType(sal_uInt16 nType) SAL_OVERRIDE;

    OUString            GetExpStr() const               { return sExpand; }
    void                ChgExpStr(const OUString& rStr) { sExpand = rStr; }

    void                CalcField( SwTblCalcPara& rCalcPara );

    virtual OUString    GetFieldName() const SAL_OVERRIDE;

    /// The formula.
    virtual OUString GetPar2()   const SAL_OVERRIDE;
    virtual void        SetPar2(const OUString& rStr) SAL_OVERRIDE;
    virtual bool        QueryValue( com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) const SAL_OVERRIDE;
    virtual bool        PutValue( const com::sun::star::uno::Any& rVal, sal_uInt16 nWhich ) SAL_OVERRIDE;
};

#endif // INCLUDED_SW_INC_EXPFLD_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
