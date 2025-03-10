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

#ifndef INCLUDED_SW_INC_SECTION_HXX
#define INCLUDED_SW_INC_SECTION_HXX

#include <com/sun/star/uno/Sequence.h>

#include <tools/rtti.hxx>
#include <tools/ref.hxx>
#include <svl/smplhint.hxx>
#include <sfx2/lnkbase.hxx>
#include <sfx2/Metadatable.hxx>

#include <frmfmt.hxx>
#include <vector>

namespace com { namespace sun { namespace star {
    namespace text { class XTextSection; }
} } }

class SwSectionFmt;
class SwDoc;
class SwSection;
class SwSectionNode;
class SwTOXBase;
class SwServerObject;

typedef std::vector<SwSection*> SwSections;

enum SectionType { CONTENT_SECTION,
                    TOX_HEADER_SECTION,
                    TOX_CONTENT_SECTION,
                    DDE_LINK_SECTION    = OBJECT_CLIENT_DDE,
                    FILE_LINK_SECTION   = OBJECT_CLIENT_FILE
                    };

enum LinkCreateType
{
    CREATE_NONE,            // Do nothing.
    CREATE_CONNECT,         // Connect created link.
    CREATE_UPDATE           // Connect created link and update it.
};

class SW_DLLPUBLIC SwSectionData
{
private:
    SectionType m_eType;

    OUString m_sSectionName;
    OUString m_sCondition;
    OUString m_sLinkFileName;
    OUString m_sLinkFilePassword; // Must be changed to Sequence.
    ::com::sun::star::uno::Sequence <sal_Int8> m_Password;

    /// It seems this flag caches the current final "hidden" state.
    bool m_bHiddenFlag          : 1;
    /// Flags that correspond to attributes in the format:
    /// may have different value than format attribute:
    /// format attr has value for this section, while flag is
    /// effectively ORed with parent sections!
    bool m_bProtectFlag         : 1;
    // Edit in readonly sections.
    bool m_bEditInReadonlyFlag  : 1;

    bool m_bHidden              : 1; // All paragraphs hidden?
    bool m_bCondHiddenFlag      : 1; // Hiddenflag for condition.
    bool m_bConnectFlag         : 1; // Connected to server?

public:

    SwSectionData(SectionType const eType, OUString const& rName);
    explicit SwSectionData(SwSection const&);
    SwSectionData(SwSectionData const&);
    SwSectionData & operator=(SwSectionData const&);
    bool operator==(SwSectionData const&) const;

    OUString GetSectionName() const         { return m_sSectionName; }
    void SetSectionName(OUString const& rName){ m_sSectionName = rName; }
    SectionType GetType() const             { return m_eType; }
    void SetType(SectionType const eNew)    { m_eType = eNew; }

    bool IsHidden() const { return m_bHidden; }
    void SetHidden(bool const bFlag = true) { m_bHidden = bFlag; }

    bool IsHiddenFlag() const { return m_bHiddenFlag; }
    SAL_DLLPRIVATE void
        SetHiddenFlag(bool const bFlag) { m_bHiddenFlag = bFlag; }
    bool IsProtectFlag() const { return m_bProtectFlag; }
    SAL_DLLPRIVATE void
        SetProtectFlag(bool const bFlag) { m_bProtectFlag = bFlag; }
    bool IsEditInReadonlyFlag() const { return m_bEditInReadonlyFlag; }
    void SetEditInReadonlyFlag(bool const bFlag)
        { m_bEditInReadonlyFlag = bFlag; }

    void SetCondHidden(bool const bFlag = true) { m_bCondHiddenFlag = bFlag; }
    bool IsCondHidden() const { return m_bCondHiddenFlag; }

    OUString GetCondition() const           { return m_sCondition; }
    void SetCondition(OUString const& rNew) { m_sCondition = rNew; }

    OUString GetLinkFileName() const        { return m_sLinkFileName; }
    void SetLinkFileName(OUString const& rNew, OUString const* pPassWd = 0)
    {
        m_sLinkFileName = rNew;
        if (pPassWd) { SetLinkFilePassword(*pPassWd); }
    }

    OUString GetLinkFilePassword() const        { return m_sLinkFilePassword; }
    void SetLinkFilePassword(OUString const& rS){ m_sLinkFilePassword = rS; }

    ::com::sun::star::uno::Sequence<sal_Int8> const& GetPassword() const
                                            { return m_Password; }
    void SetPassword(::com::sun::star::uno::Sequence<sal_Int8> const& rNew)
                                            { m_Password = rNew; }
    bool IsLinkType() const
    { return (DDE_LINK_SECTION == m_eType) || (FILE_LINK_SECTION == m_eType); }

    bool IsConnectFlag() const                  { return m_bConnectFlag; }
    void SetConnectFlag(bool const bFlag = true){ m_bConnectFlag = bFlag; }

    static OUString CollapseWhiteSpaces(const OUString& sName);
};

class SW_DLLPUBLIC SwSection
    : public SwClient
{
    // In order to correctly maintain the flag when creating/deleting frames.
    friend class SwSectionNode;
    // The "read CTOR" of SwSectionFrm have to change the Hiddenflag.
    friend class SwSectionFrm;

private:
    mutable SwSectionData m_Data;

    tools::SvRef<SwServerObject> m_RefObj; // Set if DataServer.
    ::sfx2::SvBaseLinkRef m_RefLink;

    SAL_DLLPRIVATE void ImplSetHiddenFlag(
            bool const bHidden, bool const bCondition);

protected:
    virtual void Modify( const SfxPoolItem* pOld, const SfxPoolItem* pNew ) SAL_OVERRIDE;

public:
    TYPEINFO_OVERRIDE();     // rtti

    SwSection(SectionType const eType, OUString const& rName,
                SwSectionFmt & rFormat);
    virtual ~SwSection();

    bool DataEquals(SwSectionData const& rCmp) const;

    void SetSectionData(SwSectionData const& rData);

    OUString GetSectionName() const         { return m_Data.GetSectionName(); }
    void SetSectionName(OUString const& rName){ m_Data.SetSectionName(rName); }
    SectionType GetType() const             { return m_Data.GetType(); }
    void SetType(SectionType const eType)   { return m_Data.SetType(eType); }

    SwSectionFmt* GetFmt()          { return (SwSectionFmt*)GetRegisteredIn(); }
    SwSectionFmt* GetFmt() const    { return (SwSectionFmt*)GetRegisteredIn(); }

    // Set hidden/protected -> update the whole tree!
    // (Attributes/flags are set/get.)
    bool IsHidden()  const { return m_Data.IsHidden(); }
    void SetHidden (bool const bFlag = true);
    bool IsProtect() const;
    void SetProtect(bool const bFlag = true);
    bool IsEditInReadonly() const;
    void SetEditInReadonly(bool const bFlag = true);

    // Get internal flags (state including parents, not what is
    // currently set at section!).
    bool IsHiddenFlag()  const { return m_Data.IsHiddenFlag(); }
    bool IsProtectFlag() const { return m_Data.IsProtectFlag(); }
    bool IsEditInReadonlyFlag() const { return m_Data.IsEditInReadonlyFlag(); }

    void SetCondHidden(bool const bFlag = true);
    bool IsCondHidden() const { return m_Data.IsCondHidden(); }
    // Query (also for parents) if this section is to be hidden.
    bool CalcHiddenFlag() const;

    inline SwSection* GetParent() const;

    OUString GetCondition() const           { return m_Data.GetCondition(); }
    void SetCondition(OUString const& rNew) { m_Data.SetCondition(rNew); }

    OUString GetLinkFileName() const;
    void SetLinkFileName(OUString const& rNew, OUString const*const pPassWd = 0);
    // Password of linked file (only valid during runtime!)
    OUString GetLinkFilePassword() const
        { return m_Data.GetLinkFilePassword(); }
    void SetLinkFilePassword(OUString const& rS)
        { m_Data.SetLinkFilePassword(rS); }

    // Get / set password of this section
    ::com::sun::star::uno::Sequence<sal_Int8> const& GetPassword() const
                                            { return m_Data.GetPassword(); }
    void SetPassword(::com::sun::star::uno::Sequence <sal_Int8> const& rNew)
                                            { m_Data.SetPassword(rNew); }

    // Data server methods.
    void SetRefObject( SwServerObject* pObj );
    const SwServerObject* GetObject() const {  return & m_RefObj; }
          SwServerObject* GetObject()       {  return & m_RefObj; }
    bool IsServer() const                   {  return m_RefObj.Is(); }

    // Methods for linked ranges.
    sal_uInt16 GetUpdateType() const    { return m_RefLink->GetUpdateMode(); }
    void SetUpdateType(sal_uInt16 const nType )
        { m_RefLink->SetUpdateMode(nType); }

    bool IsConnected() const        { return m_RefLink.Is(); }
    void UpdateNow()                { m_RefLink->Update(); }
    void Disconnect()               { m_RefLink->Disconnect(); }

    const ::sfx2::SvBaseLink& GetBaseLink() const    { return *m_RefLink; }
          ::sfx2::SvBaseLink& GetBaseLink()          { return *m_RefLink; }

    void CreateLink( LinkCreateType eType );

    void MakeChildLinksVisible( const SwSectionNode& rSectNd );

    bool IsLinkType() const { return m_Data.IsLinkType(); }

    // Flags for UI. Did connection work?
    bool IsConnectFlag() const      { return m_Data.IsConnectFlag(); }
    void SetConnectFlag(bool const bFlag = true)
                                    { m_Data.SetConnectFlag(bFlag); }

    // Return the TOX base class if the section is a TOX section
    const SwTOXBase* GetTOXBase() const;

    void BreakLink();

};

// #i117863#
class SwSectionFrmMoveAndDeleteHint : public SfxSimpleHint
{
    public:
        SwSectionFrmMoveAndDeleteHint( const bool bSaveCntnt )
            : SfxSimpleHint( SFX_HINT_DYING )
            , mbSaveCntnt( bSaveCntnt )
        {}

        virtual ~SwSectionFrmMoveAndDeleteHint()
        {}

        bool IsSaveCntnt() const
        {
            return mbSaveCntnt;
        }

    private:
        const bool mbSaveCntnt;
};

enum SectionSort { SORTSECT_NOT, SORTSECT_NAME, SORTSECT_POS };

class SW_DLLPUBLIC SwSectionFmt
    : public SwFrmFmt
    , public ::sfx2::Metadatable
{
    friend class SwDoc;

    /** Why does this exist in addition to the m_wXObject in SwFrmFmt?
        in case of an index, both a SwXDocumentIndex and a SwXTextSection
        register at this SwSectionFmt, so we need to have two refs.
     */
    ::com::sun::star::uno::WeakReference<
        ::com::sun::star::text::XTextSection> m_wXTextSection;

    SAL_DLLPRIVATE void UpdateParent();      // Parent has been changed.

protected:
    SwSectionFmt( SwFrmFmt* pDrvdFrm, SwDoc *pDoc );
   virtual void Modify( const SfxPoolItem* pOld, const SfxPoolItem* pNew ) SAL_OVERRIDE;

public:
    TYPEINFO_OVERRIDE();     // Already contained in base class client.
    virtual ~SwSectionFmt();

    // Deletes all Frms in aDepend (Frms are recognized via PTR_CAST).
    virtual void DelFrms() SAL_OVERRIDE;

    // Creates views.
    virtual void MakeFrms() SAL_OVERRIDE;

    // Get information from Format.
    virtual bool GetInfo( SfxPoolItem& ) const SAL_OVERRIDE;

    SwSection* GetSection() const;
    inline SwSectionFmt* GetParent() const;
    inline SwSection* GetParentSection() const;

    //  All sections that are derived from this one:
    //  - sorted according to name or position or unsorted
    //  - all of them or only those that are in the normal Nodes-array.
    sal_uInt16 GetChildSections( SwSections& rArr,
                            SectionSort eSort = SORTSECT_NOT,
                            bool bAllSections = true ) const;

    // Query whether section is in Nodes-array or in UndoNodes-array.
    bool IsInNodesArr() const;

          SwSectionNode* GetSectionNode(bool const bEvenIfInUndo = false);
    const SwSectionNode* GetSectionNode(bool const bEvenIfInUndo = false) const
        { return const_cast<SwSectionFmt *>(this)
                ->GetSectionNode(bEvenIfInUndo); }

    // Is section a valid one for global document?
    const SwSection* GetGlobalDocSection() const;

    SAL_DLLPRIVATE ::com::sun::star::uno::WeakReference<
        ::com::sun::star::text::XTextSection> const& GetXTextSection() const
            { return m_wXTextSection; }
    SAL_DLLPRIVATE void SetXTextSection(::com::sun::star::uno::Reference<
                    ::com::sun::star::text::XTextSection> const& xTextSection)
            { m_wXTextSection = xTextSection; }

    // sfx2::Metadatable
    virtual ::sfx2::IXmlIdRegistry& GetRegistry() SAL_OVERRIDE;
    virtual bool IsInClipboard() const SAL_OVERRIDE;
    virtual bool IsInUndo() const SAL_OVERRIDE;
    virtual bool IsInContent() const SAL_OVERRIDE;
    virtual ::com::sun::star::uno::Reference<
        ::com::sun::star::rdf::XMetadatable > MakeUnoObject() SAL_OVERRIDE;

};

inline SwSection* SwSection::GetParent() const
{
    SwSectionFmt* pFmt = GetFmt();
    SwSection* pRet = 0;
    if( pFmt )
        pRet = pFmt->GetParentSection();
    return pRet;
}

inline SwSectionFmt* SwSectionFmt::GetParent() const
{
    SwSectionFmt* pRet = 0;
    if( GetRegisteredIn() )
        pRet = PTR_CAST( SwSectionFmt, GetRegisteredIn() );
    return pRet;
}

inline SwSection* SwSectionFmt::GetParentSection() const
{
    SwSectionFmt* pParent = GetParent();
    SwSection* pRet = 0;
    if( pParent )
    {
        pRet = pParent->GetSection();
    }
    return pRet;
}

#endif /* _ INCLUDED_SW_INC_SECTION_HXX */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
