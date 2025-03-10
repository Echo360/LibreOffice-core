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

#include <com/sun/star/linguistic2/XAvailableLocales.hpp>
#include <com/sun/star/i18n/ScriptType.hpp>
#include <linguistic/misc.hxx>
#include <rtl/ustring.hxx>
#include <unotools/localedatawrapper.hxx>
#include <tools/urlobj.hxx>
#include <svtools/langtab.hxx>
#include <tools/shl.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <i18nlangtag/lang.h>
#include <editeng/scripttypeitem.hxx>
#include <editeng/unolingu.hxx>
#include <svx/langbox.hxx>
#include <svx/dialmgr.hxx>
#include <svx/dialogs.hrc>
#include <vcl/builder.hxx>
#include <vcl/i18nhelp.hxx>

using namespace ::com::sun::star::util;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::linguistic2;
using namespace ::com::sun::star::uno;


// If these ever dispersed we'd need a solution.
BOOST_STATIC_ASSERT((LISTBOX_APPEND == COMBOBOX_APPEND) && (LISTBOX_ENTRY_NOTFOUND == COMBOBOX_ENTRY_NOTFOUND));


OUString GetDicInfoStr( const OUString& rName, const sal_uInt16 nLang, bool bNeg )
{
    INetURLObject aURLObj;
    aURLObj.SetSmartProtocol( INET_PROT_FILE );
    aURLObj.SetSmartURL( rName, INetURLObject::ENCODE_ALL );
    OUString aTmp( aURLObj.GetBase() );
    aTmp += " ";

    if ( bNeg )
    {
        aTmp += " (-) ";
    }

    if ( LANGUAGE_NONE == nLang )
        aTmp += SVX_RESSTR(RID_SVXSTR_LANGUAGE_ALL);
    else
    {
        aTmp += "[";
        aTmp += SvtLanguageTable::GetLanguageString( (LanguageType)nLang );
        aTmp += "]";
    }

    return aTmp;
}


//  misc local helper functions


static Sequence< sal_Int16 > lcl_LocaleSeqToLangSeq( Sequence< Locale > &rSeq )
{
    const Locale *pLocale = rSeq.getConstArray();
    sal_Int32 nCount = rSeq.getLength();

    Sequence< sal_Int16 >   aLangs( nCount );
    sal_Int16 *pLang = aLangs.getArray();
    for (sal_Int32 i = 0;  i < nCount;  ++i)
    {
        pLang[i] = LanguageTag::convertToLanguageType( pLocale[i] );

    }

    return aLangs;
}


static bool lcl_SeqHasLang( const Sequence< sal_Int16 > & rLangSeq, sal_Int16 nLang )
{
    sal_Int32 i = -1;
    sal_Int32 nLen = rLangSeq.getLength();
    if (nLen)
    {
        const sal_Int16 *pLang = rLangSeq.getConstArray();
        for (i = 0;  i < nLen;  ++i)
        {
            if (nLang == pLang[i])
                break;
        }
    }
    return i >= 0  &&  i < nLen;
}


extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeSvxLanguageBox(Window *pParent, VclBuilder::stringmap &rMap)
{
    WinBits nBits = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_TABSTOP;
    bool bDropdown = VclBuilder::extractDropdown(rMap);
    if (bDropdown)
        nBits |= WB_DROPDOWN;
    else
        nBits |= WB_BORDER;
    SvxLanguageBox *pLanguageBox = new SvxLanguageBox(pParent, nBits);
    pLanguageBox->EnableAutoSize(true);
    return pLanguageBox;
}

extern "C" SAL_DLLPUBLIC_EXPORT Window* SAL_CALL makeSvxLanguageComboBox(Window *pParent, VclBuilder::stringmap &rMap)
{
    WinBits nBits = WB_LEFT|WB_VCENTER|WB_3DLOOK|WB_TABSTOP;
    bool bDropdown = VclBuilder::extractDropdown(rMap);
    if (bDropdown)
        nBits |= WB_DROPDOWN;
    else
        nBits |= WB_BORDER;
    SvxLanguageComboBox *pLanguageBox = new SvxLanguageComboBox(pParent, nBits);
    pLanguageBox->EnableAutoSize(true);
    return pLanguageBox;
}


SvxLanguageBoxBase::SvxLanguageBoxBase( bool bCheck )
    : m_pSpellUsedLang(NULL)
    , m_nLangList(LANG_LIST_EMPTY)
    , m_bHasLangNone(false)
    , m_bLangNoneIsLangAll(false)
    , m_bWithCheckmark(bCheck)
{
}

void SvxLanguageBoxBase::ImplLanguageBoxBaseInit()
{
    m_aNotCheckedImage = Image( SVX_RES( RID_SVXIMG_NOTCHECKED ) );
    m_aCheckedImage = Image( SVX_RES( RID_SVXIMG_CHECKED ) );
    m_aAllString            = SVX_RESSTR( RID_SVXSTR_LANGUAGE_ALL );
    m_nLangList             = LANG_LIST_EMPTY;
    m_bHasLangNone          = false;
    m_bLangNoneIsLangAll    = false;

    if ( m_bWithCheckmark )
    {
        sal_uInt32 nCount = SvtLanguageTable::GetLanguageEntryCount();
        for ( sal_uInt32 i = 0; i < nCount; i++ )
        {
            LanguageType nLangType = SvtLanguageTable::GetLanguageTypeAtIndex( i );

            bool bInsert = true;
            if ((LANGUAGE_DONTKNOW == nLangType)  ||
                (LANGUAGE_SYSTEM   == nLangType))
            {
                bInsert = false;
            }

            if ( bInsert )
                InsertLanguage( nLangType );
        }
        m_nLangList = LANG_LIST_ALL;
    }
}


SvxLanguageBoxBase::~SvxLanguageBoxBase()
{
    delete m_pSpellUsedLang;
}


void SvxLanguageBoxBase::SetLanguageList( sal_Int16 nLangList,
        bool bHasLangNone, bool bLangNoneIsLangAll, bool bCheckSpellAvail )
{
    ImplClear();

    m_nLangList             = nLangList;
    m_bHasLangNone          = bHasLangNone;
    m_bLangNoneIsLangAll    = bLangNoneIsLangAll;
    m_bWithCheckmark        = bCheckSpellAvail;

    if ( LANG_LIST_EMPTY != nLangList )
    {
        Sequence< sal_Int16 > aSpellAvailLang;
        Sequence< sal_Int16 > aHyphAvailLang;
        Sequence< sal_Int16 > aThesAvailLang;
        Sequence< sal_Int16 > aSpellUsedLang;
        Sequence< sal_Int16 > aHyphUsedLang;
        Sequence< sal_Int16 > aThesUsedLang;
        Reference< XAvailableLocales > xAvail( LinguMgr::GetLngSvcMgr(), UNO_QUERY );
        if (xAvail.is())
        {
            Sequence< Locale > aTmp;

            if (LANG_LIST_SPELL_AVAIL & nLangList)
            {
                aTmp = xAvail->getAvailableLocales( SN_SPELLCHECKER );
                aSpellAvailLang = lcl_LocaleSeqToLangSeq( aTmp );
            }
            if (LANG_LIST_HYPH_AVAIL  & nLangList)
            {
                aTmp = xAvail->getAvailableLocales( SN_HYPHENATOR );
                aHyphAvailLang = lcl_LocaleSeqToLangSeq( aTmp );
            }
            if (LANG_LIST_THES_AVAIL  & nLangList)
            {
                aTmp = xAvail->getAvailableLocales( SN_THESAURUS );
                aThesAvailLang = lcl_LocaleSeqToLangSeq( aTmp );
            }
        }
        if (LANG_LIST_SPELL_USED & nLangList)
        {
            Reference< XSpellChecker1 > xTmp1( SvxGetSpellChecker(), UNO_QUERY );
            if (xTmp1.is())
                aSpellUsedLang = xTmp1->getLanguages();
        }
        if (LANG_LIST_HYPH_USED  & nLangList)
        {
            Reference< XHyphenator > xTmp( SvxGetHyphenator() );
            if (xTmp.is()) {
                Sequence < Locale > aLocaleSequence( xTmp->getLocales() );
                aHyphUsedLang = lcl_LocaleSeqToLangSeq( aLocaleSequence );
            }
        }
        if (LANG_LIST_THES_USED  & nLangList)
        {
            Reference< XThesaurus > xTmp( SvxGetThesaurus() );
            if (xTmp.is()) {
                Sequence < Locale > aLocaleSequence( xTmp->getLocales() );
                aThesUsedLang = lcl_LocaleSeqToLangSeq( aLocaleSequence );
            }
        }

        ::com::sun::star::uno::Sequence< sal_uInt16 > xKnown;
        const sal_uInt16* pKnown;
        sal_uInt32 nCount;
        if ( nLangList & LANG_LIST_ONLY_KNOWN )
        {
            xKnown = LocaleDataWrapper::getInstalledLanguageTypes();
            pKnown = xKnown.getConstArray();
            nCount = xKnown.getLength();
        }
        else
        {
            nCount = SvtLanguageTable::GetLanguageEntryCount();
            pKnown = NULL;
        }
        for ( sal_uInt32 i = 0; i < nCount; i++ )
        {
            LanguageType nLangType;
            if ( nLangList & LANG_LIST_ONLY_KNOWN )
                nLangType = pKnown[i];
            else
                nLangType = SvtLanguageTable::GetLanguageTypeAtIndex( i );
            if ( nLangType != LANGUAGE_DONTKNOW &&
                 nLangType != LANGUAGE_SYSTEM &&
                 nLangType != LANGUAGE_NONE &&
                 !MsLangId::isLegacy( nLangType) &&
                 (MsLangId::getSubLanguage( nLangType) != 0 ||
                  (nLangList & LANG_LIST_ALSO_PRIMARY_ONLY)) &&
                 ((nLangList & LANG_LIST_ALL) != 0 ||
                  ((nLangList & LANG_LIST_WESTERN) != 0 &&
                   (SvtLanguageOptions::GetScriptTypeOfLanguage(nLangType) ==
                    SCRIPTTYPE_LATIN)) ||
                  ((nLangList & LANG_LIST_CTL) != 0 &&
                   (SvtLanguageOptions::GetScriptTypeOfLanguage(nLangType) ==
                    SCRIPTTYPE_COMPLEX)) ||
                  ((nLangList & LANG_LIST_CJK) != 0 &&
                   (SvtLanguageOptions::GetScriptTypeOfLanguage(nLangType) ==
                    SCRIPTTYPE_ASIAN)) ||
                  ((nLangList & LANG_LIST_FBD_CHARS) != 0 &&
                   MsLangId::hasForbiddenCharacters(nLangType)) ||
                  ((nLangList & LANG_LIST_SPELL_AVAIL) != 0 &&
                   lcl_SeqHasLang(aSpellAvailLang, nLangType)) ||
                  ((nLangList & LANG_LIST_HYPH_AVAIL) != 0 &&
                   lcl_SeqHasLang(aHyphAvailLang, nLangType)) ||
                  ((nLangList & LANG_LIST_THES_AVAIL) != 0 &&
                   lcl_SeqHasLang(aThesAvailLang, nLangType)) ||
                  ((nLangList & LANG_LIST_SPELL_USED) != 0 &&
                   lcl_SeqHasLang(aSpellUsedLang, nLangType)) ||
                  ((nLangList & LANG_LIST_HYPH_USED) != 0 &&
                   lcl_SeqHasLang(aHyphUsedLang, nLangType)) ||
                  ((nLangList & LANG_LIST_THES_USED) != 0 &&
                   lcl_SeqHasLang(aThesUsedLang, nLangType))) )
                InsertLanguage( nLangType );
        }

        if (bHasLangNone)
            InsertLanguage( LANGUAGE_NONE );
    }
}


sal_Int32 SvxLanguageBoxBase::InsertLanguage( const LanguageType nLangType, sal_Int32 nPos )
{
    return ImplInsertLanguage( nLangType, nPos, ::com::sun::star::i18n::ScriptType::WEAK );
}


sal_Int32 SvxLanguageBoxBase::ImplInsertLanguage( const LanguageType nLangType, sal_Int32 nPos, sal_Int16 nType )
{
    LanguageType nLang = MsLangId::getReplacementForObsoleteLanguage( nLangType);
    // For obsolete and to be replaced languages check whether an entry of the
    // replacement already exists and if so don't add an entry with identical
    // string as would be returned by SvtLanguageTable::GetString().
    if (nLang != nLangType)
    {
        sal_Int32 nAt = ImplTypeToPos( nLang );
        if ( nAt != LISTBOX_ENTRY_NOTFOUND )
            return nAt;
    }

    OUString aStrEntry = SvtLanguageTable::GetLanguageString( nLang );
    if (LANGUAGE_NONE == nLang && m_bHasLangNone && m_bLangNoneIsLangAll)
        aStrEntry = m_aAllString;

    LanguageType nRealLang = nLang;
    if (nRealLang == LANGUAGE_SYSTEM)
    {
        nRealLang = MsLangId::resolveSystemLanguageByScriptType(nRealLang, nType);
        aStrEntry += " - ";
        aStrEntry += SvtLanguageTable::GetLanguageString( nRealLang );
    } else if (nRealLang == LANGUAGE_USER_SYSTEM_CONFIG) {
        nRealLang = MsLangId::getSystemLanguage();
        aStrEntry += " - ";
        aStrEntry += SvtLanguageTable::GetLanguageString( nRealLang );
    }

    aStrEntry = ApplyLreOrRleEmbedding( aStrEntry );

    sal_Int32 nAt = 0;
    if ( m_bWithCheckmark )
    {
        bool bFound = false;

        if (!m_pSpellUsedLang)
        {
            Reference< XSpellChecker1 > xSpell( SvxGetSpellChecker(), UNO_QUERY );
            if ( xSpell.is() )
                m_pSpellUsedLang = new Sequence< sal_Int16 >( xSpell->getLanguages() );
        }
        bFound = m_pSpellUsedLang ?
            lcl_SeqHasLang( *m_pSpellUsedLang, nRealLang ) : false;

        nAt = ImplInsertImgEntry( aStrEntry, nPos, bFound );
    }
    else
        nAt = ImplInsertEntry( aStrEntry, nPos );

    ImplSetEntryData( nAt, (void*)(sal_uIntPtr)nLangType );
    return nAt;
}


sal_Int32 SvxLanguageBoxBase::InsertDefaultLanguage( sal_Int16 nType, sal_Int32 nPos )
{
    return ImplInsertLanguage( LANGUAGE_SYSTEM, nPos, nType );
}


sal_Int32 SvxLanguageBoxBase::InsertSystemLanguage( sal_Int32 nPos )
{
    return ImplInsertLanguage( LANGUAGE_USER_SYSTEM_CONFIG, nPos, ::com::sun::star::i18n::ScriptType::WEAK );
}


sal_Int32 SvxLanguageBoxBase::InsertLanguage( const LanguageType nLangType,
        bool bCheckEntry, sal_Int32 nPos )
{
    LanguageType nLang = MsLangId::getReplacementForObsoleteLanguage( nLangType);
    // For obsolete and to be replaced languages check whether an entry of the
    // replacement already exists and if so don't add an entry with identical
    // string as would be returned by SvtLanguageTable::GetString().
    if (nLang != nLangType)
    {
        sal_Int32 nAt = ImplTypeToPos( nLang );
        if ( nAt != LISTBOX_ENTRY_NOTFOUND )
            return nAt;
    }

    OUString aStrEntry = SvtLanguageTable::GetLanguageString( nLang );
    if (LANGUAGE_NONE == nLang && m_bHasLangNone && m_bLangNoneIsLangAll)
        aStrEntry = m_aAllString;

    sal_Int32 nAt = ImplInsertImgEntry( aStrEntry, nPos, bCheckEntry );
    ImplSetEntryData( nAt, (void*)(sal_uIntPtr)nLang );

    return nAt;
}


void SvxLanguageBoxBase::RemoveLanguage( const LanguageType eLangType )
{
    sal_Int32 nAt = ImplTypeToPos( eLangType );

    if ( nAt != LISTBOX_ENTRY_NOTFOUND )
        ImplRemoveEntryAt( nAt );
}


LanguageType SvxLanguageBoxBase::GetSelectLanguage() const
{
    sal_Int32     nPos   = ImplGetSelectEntryPos();

    if ( nPos != LISTBOX_ENTRY_NOTFOUND )
        return LanguageType( (sal_uIntPtr)ImplGetEntryData(nPos) );
    else
        return LanguageType( LANGUAGE_DONTKNOW );
}


void SvxLanguageBoxBase::SelectLanguage( const LanguageType eLangType, bool bSelect )
{
    // If the core uses a LangID of an imported MS document and wants to select
    // a language that is replaced, we need to select the replacement instead.
    LanguageType nLang = MsLangId::getReplacementForObsoleteLanguage( eLangType);

    sal_Int32 nAt = ImplTypeToPos( nLang );

    if ( nAt == LISTBOX_ENTRY_NOTFOUND )
        nAt = InsertLanguage( nLang );      // on-the-fly-ID

    if ( nAt != LISTBOX_ENTRY_NOTFOUND )
        ImplSelectEntryPos( nAt, bSelect );
}


bool SvxLanguageBoxBase::IsLanguageSelected( const LanguageType eLangType ) const
{
    // Same here, work on the replacement if applicable.
    LanguageType nLang = MsLangId::getReplacementForObsoleteLanguage( eLangType);

    sal_Int32 nAt = ImplTypeToPos( nLang );

    if ( nAt != LISTBOX_ENTRY_NOTFOUND )
        return ImplIsEntryPosSelected( nAt );
    else
        return false;
}


sal_Int32 SvxLanguageBoxBase::ImplTypeToPos( LanguageType eType ) const
{
    return ImplGetEntryPos( (void*)(sal_uIntPtr)eType);
}


void SvxLanguageBoxBase::SetNoSelectionLBB()
{
    ImplSetNoSelection();
}

void SvxLanguageBoxBase::HideLBB()
{
    ImplHide();
}

void SvxLanguageBoxBase::DisableLBB()
{
    ImplDisable();
}

void SvxLanguageBoxBase::SaveValueLBB()
{
    ImplSaveValue();
}

sal_Int32 SvxLanguageBoxBase::GetSelectEntryPosLBB( sal_Int32 nSelIndex ) const
{
    return ImplGetSelectEntryPos( nSelIndex);
}

void* SvxLanguageBoxBase::GetEntryDataLBB( sal_Int32  nPos ) const
{
    return ImplGetEntryData( nPos);
}

sal_Int32 SvxLanguageBoxBase::GetSavedValueLBB() const
{
    return ImplGetSavedValue();
}


SvxLanguageBox::SvxLanguageBox( Window* pParent, WinBits nBits, bool bCheck )
    : ListBox( pParent, nBits )
    , SvxLanguageBoxBase( bCheck )
{
    // display entries sorted
    SetStyle( GetStyle() | WB_SORT );

    ImplLanguageBoxBaseInit();
}

SvxLanguageBox::~SvxLanguageBox()
{
}


SvxLanguageComboBox::SvxLanguageComboBox( Window* pParent, WinBits nBits, bool bCheck )
    : ComboBox( pParent, nBits )
    , SvxLanguageBoxBase( bCheck )
    , mnSavedValuePos( COMBOBOX_ENTRY_NOTFOUND )
    , meEditedAndValid( EDITED_NO )
{
    // display entries sorted
    SetStyle( GetStyle() | WB_SORT );

    EnableMultiSelection( false );

    ImplLanguageBoxBaseInit();

    SetModifyHdl( LINK( this, SvxLanguageComboBox, EditModifyHdl ) );
}

SvxLanguageComboBox::~SvxLanguageComboBox()
{
}


sal_Int32 SvxLanguageBox::ImplInsertImgEntry( const OUString& rEntry, sal_Int32 nPos, bool bChecked )
{
    return InsertEntry( rEntry, (bChecked ? m_aCheckedImage : m_aNotCheckedImage), nPos );
}

sal_Int32 SvxLanguageComboBox::ImplInsertImgEntry( const OUString& rEntry, sal_Int32 nPos, bool bChecked )
{
    return InsertEntryWithImage( rEntry, (bChecked ? m_aCheckedImage : m_aNotCheckedImage), nPos );
}


void SvxLanguageBox::ImplRemoveEntryAt( sal_Int32 nPos )
{
    RemoveEntry( nPos);
}

void SvxLanguageComboBox::ImplRemoveEntryAt( sal_Int32 nPos )
{
    RemoveEntryAt( nPos);
}


void SvxLanguageBox::ImplClear()
{
    Clear();
}

void SvxLanguageComboBox::ImplClear()
{
    Clear();
}


sal_Int32 SvxLanguageBox::ImplInsertEntry( const OUString& rEntry, sal_Int32 nPos )
{
    return InsertEntry( rEntry, nPos);
}

sal_Int32 SvxLanguageComboBox::ImplInsertEntry( const OUString& rEntry, sal_Int32 nPos )
{
    return InsertEntry( rEntry, nPos);
}


void SvxLanguageBox::ImplSetEntryData( sal_Int32 nPos, void* pData )
{
    SetEntryData( nPos, pData);
}

void SvxLanguageComboBox::ImplSetEntryData( sal_Int32 nPos, void* pData )
{
    SetEntryData( nPos, pData);
}


sal_Int32 SvxLanguageBox::ImplGetSelectEntryPos( sal_Int32 nSelIndex ) const
{
    return GetSelectEntryPos( nSelIndex);
}

sal_Int32 SvxLanguageComboBox::ImplGetSelectEntryPos( sal_Int32 nSelIndex ) const
{
    return GetSelectEntryPos( nSelIndex);
}


void* SvxLanguageBox::ImplGetEntryData( sal_Int32 nPos ) const
{
    return GetEntryData( nPos);
}

void* SvxLanguageComboBox::ImplGetEntryData( sal_Int32 nPos ) const
{
    return GetEntryData( nPos);
}


void SvxLanguageBox::ImplSelectEntryPos( sal_Int32 nPos, bool bSelect )
{
    SelectEntryPos( nPos, bSelect);
}

void SvxLanguageComboBox::ImplSelectEntryPos( sal_Int32 nPos, bool bSelect )
{
    SelectEntryPos( nPos, bSelect);
}


bool SvxLanguageBox::ImplIsEntryPosSelected( sal_Int32 nPos ) const
{
    return IsEntryPosSelected( nPos);
}

bool SvxLanguageComboBox::ImplIsEntryPosSelected( sal_Int32 nPos ) const
{
    return IsEntryPosSelected( nPos);
}


sal_Int32 SvxLanguageBox::ImplGetEntryPos( const void* pData ) const
{
    return GetEntryPos( pData);
}

sal_Int32 SvxLanguageComboBox::ImplGetEntryPos( const void* pData ) const
{
    return GetEntryPos( pData);
}


sal_Int32 SvxLanguageBox::ImplGetEntryCount() const
{
    return GetEntryCount();
}

sal_Int32 SvxLanguageComboBox::ImplGetEntryCount() const
{
    return GetEntryCount();
}


void SvxLanguageBox::ImplSetNoSelection()
{
    SetNoSelection();
}

void SvxLanguageComboBox::ImplSetNoSelection()
{
    SetNoSelection();
}


void SvxLanguageBox::ImplHide()
{
    Hide();
}

void SvxLanguageComboBox::ImplHide()
{
    Hide();
}


void SvxLanguageBox::ImplDisable()
{
    Disable();
}

void SvxLanguageComboBox::ImplDisable()
{
    Disable();
}


void SvxLanguageBox::ImplSaveValue()
{
    SaveValue();
}

void SvxLanguageComboBox::ImplSaveValue()
{
    // Emulate the ListBox behavior.
    mnSavedValuePos = GetSelectEntryPos();
}


sal_Int32 SvxLanguageBox::ImplGetSavedValue() const
{
    return GetSavedValue();
}

sal_Int32 SvxLanguageComboBox::ImplGetSavedValue() const
{
    return mnSavedValuePos;
}


IMPL_LINK( SvxLanguageComboBox, EditModifyHdl, SvxLanguageComboBox*, /*pEd*/ )
{
    EditedAndValid eOldState = meEditedAndValid;
    OUString aStr( vcl::I18nHelper::filterFormattingChars( GetText()));
    if (aStr.isEmpty())
        meEditedAndValid = EDITED_INVALID;
    else
    {
        const sal_Int32 nPos = GetEntryPos( aStr);
        if (nPos != COMBOBOX_ENTRY_NOTFOUND)
        {
            // Advance start of full selection by one so the next character
            // will already continue the string instead of having to type the
            // same character again to start a new string. The selection
            // includes formatting characters and is reverse when obtained from
            // the Edit control.
            Selection aSel( GetSelection());
            if (aSel.Max() == 1)
            {
                OUString aText( GetText());
                if (aSel.Min() == aText.getLength())
                {
                    ++aSel.Max();
                    SetSelection( aSel);
                }
            }
            meEditedAndValid = EDITED_NO;
        }
        else
        {
            OUString aCanonicalized;
            bool bValid = LanguageTag::isValidBcp47( aStr, &aCanonicalized, true);
            meEditedAndValid = (bValid ? EDITED_VALID : EDITED_INVALID);
            if (bValid && aCanonicalized != aStr)
            {
                SetText( aCanonicalized);
                SetSelection( Selection( aCanonicalized.getLength()));
            }
        }
    }
    if (eOldState != meEditedAndValid)
    {
        if (meEditedAndValid == EDITED_INVALID)
        {
#if 0
            //! Gives white on white!?! instead of white on reddish.
            SetControlBackground( ::Color( RGB_COLORDATA( 0xff, 0x65, 0x63)));
            SetControlForeground( ::Color( COL_WHITE));
#else
            SetControlForeground( ::Color( RGB_COLORDATA( 0xf0, 0, 0)));
#endif
        }
        else
        {
            SetControlForeground();
            SetControlBackground();
        }
    }
    return 0;
}




sal_Int32 SvxLanguageComboBox::SaveEditedAsEntry()
{
    if (meEditedAndValid != EDITED_VALID)
        return COMBOBOX_ENTRY_NOTFOUND;

    LanguageTag aLanguageTag( vcl::I18nHelper::filterFormattingChars( GetText()));
    LanguageType nLang = aLanguageTag.getLanguageType();
    if (nLang == LANGUAGE_DONTKNOW)
    {
        SAL_WARN( "svx.dialog", "SvxLanguageComboBox::SaveEditedAsEntry: unknown tag");
        return COMBOBOX_ENTRY_NOTFOUND;
    }

    sal_Int32 nPos = ImplTypeToPos( nLang);
    if (nPos != COMBOBOX_ENTRY_NOTFOUND)
        return nPos;    // Already present but with a different string.

    if (SvtLanguageTable::HasLanguageType( nLang))
    {
        // In SvtLanguageTable but not in SvxLanguageComboBox. On purpose? This
        // may be an entry with different settings or CTL instead of Western or
        // ... all things we don't handle yet.
        SAL_WARN( "svx.dialog", "SvxLanguageComboBox::SaveEditedAsEntry: already in SvtLanguageTable: " <<
                SvtLanguageTable::GetLanguageString( nLang) << ", " << nLang);
    }
    else
    {
        // Add to both, SvtLanguageTable and SvxLanguageComboBox.
        /* TODO: a descriptive user comment would be a nice to have here. */
        SvtLanguageTable::AddLanguageTag( aLanguageTag, OUString());
    }

    nPos = InsertLanguage( nLang);

    return nPos;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
