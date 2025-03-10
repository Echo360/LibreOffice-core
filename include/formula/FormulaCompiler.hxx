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

#ifndef INCLUDED_FORMULA_FORMULACOMPILER_HXX
#define INCLUDED_FORMULA_FORMULACOMPILER_HXX

#include <formula/formuladllapi.h>
#include <rtl/ustrbuf.hxx>
#include <rtl/ustring.hxx>
#include <tools/debug.hxx>

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/noncopyable.hpp>

#include <com/sun/star/uno/Sequence.hxx>

#include <formula/opcode.hxx>
#include <formula/grammar.hxx>
#include <formula/token.hxx>
#include <formula/ExternalReferenceHelper.hxx>

#define FORMULA_MAXJUMPCOUNT    32  /* maximum number of jumps (ocChose) */
#define FORMULA_MAXTOKENS     8192  /* maximum number of tokens in formula */


namespace com { namespace sun { namespace star {
    namespace sheet {
        struct FormulaOpCodeMapEntry;
        struct FormulaToken;
    }
}}}


namespace formula
{
    class FormulaTokenArray;

struct FormulaArrayStack
{
    FormulaArrayStack*  pNext;
    FormulaTokenArray*  pArr;
    bool bTemp;
};


typedef ::boost::unordered_map< OUString, OpCode, OUStringHash, ::std::equal_to< OUString > > OpCodeHashMap;
typedef ::boost::unordered_map< OUString, OUString, OUStringHash, ::std::equal_to< OUString > > ExternalHashMap;

class FORMULA_DLLPUBLIC FormulaCompiler : boost::noncopyable
{
public:
    FormulaCompiler();
    FormulaCompiler(FormulaTokenArray& _rArr);
    virtual ~FormulaCompiler();

    // SUNWS8 needs a forward declared friend, otherwise members of the outer
    // class are not accessible.
    class OpCodeMap;
    friend class FormulaCompiler::OpCodeMap;

    /** Mappings from strings to OpCodes and vice versa. */
    class FORMULA_DLLPUBLIC OpCodeMap
    {
        OpCodeHashMap         * mpHashMap;                 /// Hash map of symbols, OUString -> OpCode
        OUString              * mpTable;                   /// Array of symbols, OpCode -> OUString, offset==OpCode
        ExternalHashMap       * mpExternalHashMap;         /// Hash map of ocExternal, Filter String -> AddIn String
        ExternalHashMap       * mpReverseExternalHashMap;  /// Hash map of ocExternal, AddIn String -> Filter String
        FormulaGrammar::Grammar meGrammar;                  /// Grammar, language and reference convention
        sal_uInt16              mnSymbols;                  /// Count of OpCode symbols
        bool                    mbCore      : 1;            /// If mapping was setup by core, not filters
        bool                    mbEnglish   : 1;            /// If English symbols and external names

        OpCodeMap();                              // prevent usage
        OpCodeMap( const OpCodeMap& );            // prevent usage
        OpCodeMap& operator=( const OpCodeMap& ); // prevent usage

    public:

        OpCodeMap(sal_uInt16 nSymbols, bool bCore, FormulaGrammar::Grammar eGrammar ) :
            mpHashMap( new OpCodeHashMap( nSymbols)),
            mpTable( new OUString[ nSymbols ]),
            mpExternalHashMap( new ExternalHashMap),
            mpReverseExternalHashMap( new ExternalHashMap),
            meGrammar( eGrammar),
            mnSymbols( nSymbols),
            mbCore( bCore)
        {
            mbEnglish = FormulaGrammar::isEnglish( meGrammar);
        }
        virtual ~OpCodeMap();

        /** Copy mappings from r into this map, effectively replacing this map.

            @param  bOverrideKnownBad
                    If TRUE, override known legacy bad function names with
                    correct ones if the conditions can be derived from the
                    current maps.
         */
        void copyFrom( const OpCodeMap& r, bool bOverrideKnownBad );

        /// Get the symbol String -> OpCode hash map for finds.
        inline const OpCodeHashMap* getHashMap() const { return mpHashMap; }

        /// Get the symbol String -> AddIn String hash map for finds.
        inline const ExternalHashMap* getExternalHashMap() const { return mpExternalHashMap; }

        /// Get the AddIn String -> symbol String hash map for finds.
        inline const ExternalHashMap* getReverseExternalHashMap() const { return mpReverseExternalHashMap; }

        /// Get the symbol string matching an OpCode.
        inline const OUString& getSymbol( const OpCode eOp ) const
        {
            DBG_ASSERT( sal_uInt16(eOp) < mnSymbols, "OpCodeMap::getSymbol: OpCode out of range");
            if (sal_uInt16(eOp) < mnSymbols)
                return mpTable[ eOp ];
            static OUString s_sEmpty;
            return s_sEmpty;
        }

        /// Get the first character of the symbol string matching an OpCode.
        inline sal_Unicode getSymbolChar( const OpCode eOp ) const {  return getSymbol(eOp)[0]; };

        /// Get the grammar.
        inline FormulaGrammar::Grammar getGrammar() const { return meGrammar; }

        /// Get the symbol count.
        inline sal_uInt16 getSymbolCount() const { return mnSymbols; }

        /** Are these English symbols, as opposed to native language (which may
            be English as well)? */
        inline bool isEnglish() const { return mbEnglish; }

        /// Is it an internal core mapping, or setup by filters?
        inline bool isCore() const { return mbCore; }

        /// Is it an ODF 1.1 compatibility mapping?
        inline bool isPODF() const { return FormulaGrammar::isPODF( meGrammar); }

        /// Is it an ODFF / ODF 1.2 mapping?
        inline bool isODFF() const { return FormulaGrammar::isODFF( meGrammar); }

        /// Is it an OOXML mapping?
        inline bool isOOXML() const { return FormulaGrammar::isOOXML( meGrammar); }

        /// Does it have external symbol/name mappings?
        inline bool hasExternals() const { return !mpExternalHashMap->empty(); }

        /// Put entry of symbol String and OpCode pair.
        void putOpCode( const OUString & rStr, const OpCode eOp );

        /// Put entry of symbol String and AddIn international String pair.
        void putExternal( const OUString & rSymbol, const OUString & rAddIn );

        /** Put entry of symbol String and AddIn international String pair,
            failing silently if rAddIn name already exists. */
        void putExternalSoftly( const OUString & rSymbol, const OUString & rAddIn );

        /// Core implementation of XFormulaOpCodeMapper::getMappings()
        ::com::sun::star::uno::Sequence< ::com::sun::star::sheet::FormulaToken >
            createSequenceOfFormulaTokens(const FormulaCompiler& _rCompiler,
                    const ::com::sun::star::uno::Sequence< OUString >& rNames ) const;

        /// Core implementation of XFormulaOpCodeMapper::getAvailableMappings()
        ::com::sun::star::uno::Sequence<
            ::com::sun::star::sheet::FormulaOpCodeMapEntry >
            createSequenceOfAvailableMappings( const FormulaCompiler& _rCompiler,const sal_Int32 nGroup ) const;

        /** The value used in createSequenceOfAvailableMappings() and thus in
            XFormulaOpCodeMapper::getMappings() for an unknown symbol. */
        static sal_Int32 getOpCodeUnknown();

    private:

        /** Conditionally put a mapping in copyFrom() context.

            Does NOT check eOp range!
         */
        void putCopyOpCode( const OUString& rSymbol, OpCode eOp );
    };

public:
    typedef ::boost::shared_ptr< const OpCodeMap >  OpCodeMapPtr;
    typedef ::boost::shared_ptr< OpCodeMap >        NonConstOpCodeMapPtr;

    /** Get OpCodeMap for formula language.
        @param nLanguage
            One of ::com::sun::star::sheet::FormulaLanguage constants.
        @return Map for nLanguage. If nLanguage is unknown, a NULL map is returned.
     */
    OpCodeMapPtr GetOpCodeMap( const sal_Int32 nLanguage ) const;

    /** Create an internal symbol map from API mapping.
        @param bEnglish
            Use English number parser / formatter instead of native.
     */
    OpCodeMapPtr CreateOpCodeMap(
            const ::com::sun::star::uno::Sequence<
            const ::com::sun::star::sheet::FormulaOpCodeMapEntry > & rMapping,
            bool bEnglish );

    /** Get current OpCodeMap in effect. */
    inline OpCodeMapPtr GetCurrentOpCodeMap() const { return mxSymbols; }

    /** Get OpCode for English symbol.
        Used in XFunctionAccess to create token array.
        @param rName
            Symbol to lookup. MUST be upper case.
     */
    OpCode GetEnglishOpCode( const OUString& rName ) const;

    sal_uInt16 GetErrorConstant( const OUString& rName ) const;

    void EnableJumpCommandReorder( bool bEnable );
    void EnableStopOnError( bool bEnable );

    static bool IsOpCodeVolatile( OpCode eOp );
    static bool IsOpCodeJumpCommand( OpCode eOp );

    static bool DeQuote( OUString& rStr );


    static const OUString&  GetNativeSymbol( OpCode eOp );
    static sal_Unicode      GetNativeSymbolChar( OpCode eOp );
    static  bool            IsMatrixFunction(OpCode _eOpCode);   // if a function _always_ returns a Matrix

    short GetNumFormatType() const { return nNumFmt; }
    bool  CompileTokenArray();

    void CreateStringFromTokenArray( OUString& rFormula );
    void CreateStringFromTokenArray( OUStringBuffer& rBuffer );
    FormulaToken* CreateStringFromToken( OUString& rFormula, FormulaToken* pToken,
                                    bool bAllowArrAdvance = false );
    FormulaToken* CreateStringFromToken( OUStringBuffer& rBuffer, FormulaToken* pToken,
                                    bool bAllowArrAdvance = false );

    void AppendBoolean( OUStringBuffer& rBuffer, bool bVal ) const;
    void AppendDouble( OUStringBuffer& rBuffer, double fVal ) const;
    void AppendString( OUStringBuffer& rBuffer, const OUString & rStr ) const;

    /** Set symbol map corresponding to one of predefined formula::FormulaGrammar::Grammar,
        including an address reference convention. */
    inline  FormulaGrammar::Grammar   GetGrammar() const { return meGrammar; }

    static void UpdateSeparatorsNative( const OUString& rSep, const OUString& rArrayColSep, const OUString& rArrayRowSep );
    static void ResetNativeSymbols();
    static void SetNativeSymbols( const OpCodeMapPtr& xMap );

    /** Separators mapped when loading opcodes from the resource, values other
        than RESOURCE_BASE may override the resource strings. Used by OpCodeList
        implementation via loadSymbols().
     */
    enum SeparatorType
    {
        RESOURCE_BASE,
        SEMICOLON_BASE,
        COMMA_BASE
    };

protected:
    virtual OUString FindAddInFunction( const OUString& rUpperName, bool bLocalFirst ) const;
    virtual void fillFromAddInCollectionUpperName( NonConstOpCodeMapPtr xMap ) const;
    virtual void fillFromAddInMap( NonConstOpCodeMapPtr xMap, FormulaGrammar::Grammar _eGrammar ) const;
    virtual void fillFromAddInCollectionEnglishName( NonConstOpCodeMapPtr xMap ) const;
    virtual void fillAddInToken(::std::vector< ::com::sun::star::sheet::FormulaOpCodeMapEntry >& _rVec,bool _bIsEnglish) const;

    virtual void SetError(sal_uInt16 nError);
    virtual FormulaTokenRef ExtendRangeReference( FormulaToken & rTok1, FormulaToken & rTok2, bool bReuseDoubleRef );
    virtual bool HandleExternalReference(const FormulaToken& _aToken);
    virtual bool HandleRange();
    virtual bool HandleSingleRef();
    virtual bool HandleDbData();

    virtual void CreateStringFromExternal(OUStringBuffer& rBuffer, FormulaToken* pTokenP) const;
    virtual void CreateStringFromSingleRef(OUStringBuffer& rBuffer,FormulaToken* pTokenP) const;
    virtual void CreateStringFromDoubleRef(OUStringBuffer& rBuffer,FormulaToken* pTokenP) const;
    virtual void CreateStringFromMatrix(OUStringBuffer& rBuffer,FormulaToken* pTokenP) const;
    virtual void CreateStringFromIndex(OUStringBuffer& rBuffer,FormulaToken* pTokenP) const;
    virtual void LocalizeString( OUString& rName ) const;   // modify rName - input: exact name

    void AppendErrorConstant( OUStringBuffer& rBuffer, sal_uInt16 nError ) const;

    bool   GetToken();
    OpCode NextToken();
    void PutCode( FormulaTokenRef& );
    void Factor();
    void RangeLine();
    void UnionLine();
    void IntersectionLine();
    void UnaryLine();
    void PostOpLine();
    void PowLine();
    void MulDivLine();
    void AddSubLine();
    void ConcatLine();
    void CompareLine();
    void NotLine();
    OpCode Expression();
    void PopTokenArray();
    void PushTokenArray( FormulaTokenArray*, bool = false );

    bool MergeRangeReference( FormulaToken * * const pCode1, FormulaToken * const * const pCode2 );

    OUString            aCorrectedFormula;      // autocorrected Formula
    OUString            aCorrectedSymbol;       // autocorrected Symbol

    OpCodeMapPtr        mxSymbols;              // which symbols are used

    FormulaTokenRef     mpToken;                // current token
    FormulaTokenRef     pCurrentFactorToken;    // current factor token (of Factor() method)
    FormulaTokenArray*  pArr;

    FormulaToken**      pCode;
    FormulaArrayStack*  pStack;

    OpCode              eLastOp;
    short               nRecursion;             // GetToken() recursions
    short               nNumFmt;                // set during CompileTokenArray()
    sal_uInt16          pc;                     // program counter

    FormulaGrammar::Grammar meGrammar;          // The grammar used, language plus convention.

    bool                bAutoCorrect;           // whether to apply AutoCorrection
    bool                bCorrected;             // AutoCorrection was applied
    bool                glSubTotal;             // if code contains one or more subtotal functions

    bool mbJumpCommandReorder; /// Whether or not to reorder RPN for jump commands.
    bool mbStopOnError;        /// Whether to stop compilation on first encountered error.

private:
    void InitSymbolsNative() const;    /// only SymbolsNative, on first document creation
    void InitSymbolsEnglish() const;   /// only SymbolsEnglish, maybe later
    void InitSymbolsPODF() const;      /// only SymbolsPODF, on demand
    void InitSymbolsODFF() const;      /// only SymbolsODFF, on demand
    void InitSymbolsEnglishXL() const; /// only SymbolsEnglishXL, on demand
    void InitSymbolsOOXML() const;     /// only SymbolsOOXML, on demand

    void loadSymbols( sal_uInt16 nSymbols, FormulaGrammar::Grammar eGrammar, NonConstOpCodeMapPtr& rxMap,
            SeparatorType eSepType = SEMICOLON_BASE ) const;

    static inline void ForceArrayOperator( FormulaTokenRef& rCurr, const FormulaTokenRef& rPrev )
        {
            if ( rPrev && rPrev->HasForceArray() && rCurr->GetOpCode() != ocPush &&
                    (rCurr->GetType() == svByte || rCurr->GetType() == svJump) &&
                    !rCurr->HasForceArray() )
                rCurr->SetForceArray( true);
        }

    // SUNWS7 needs a forward declared friend, otherwise members of the outer
    // class are not accessible.
    class CurrentFactor;
    friend class FormulaCompiler::CurrentFactor;
    class CurrentFactor
    {
        FormulaTokenRef  pPrevFac;
        FormulaCompiler* pCompiler;
        // not implemented
        CurrentFactor( const CurrentFactor& );
        CurrentFactor& operator=( const CurrentFactor& );
    public:
        explicit CurrentFactor( FormulaCompiler* pComp )
            : pPrevFac( pComp->pCurrentFactorToken )
            , pCompiler( pComp )
            {}
        ~CurrentFactor()
            { pCompiler->pCurrentFactorToken = pPrevFac; }
        // yes, this operator= may modify the RValue
        void operator=( FormulaTokenRef& r )
            {
                ForceArrayOperator( r, pPrevFac);
                pCompiler->pCurrentFactorToken = r;
            }
        void operator=( FormulaToken* p )
            {
                FormulaTokenRef xTemp( p );
                *this = xTemp;
            }
        operator FormulaTokenRef&()
            { return pCompiler->pCurrentFactorToken; }
        FormulaToken* operator->()
            { return pCompiler->pCurrentFactorToken.operator->(); }
        operator FormulaToken*()
            { return operator->(); }
    };


    mutable NonConstOpCodeMapPtr  mxSymbolsODFF;      // ODFF symbols
    mutable NonConstOpCodeMapPtr  mxSymbolsPODF;      // ODF 1.1 symbols
    mutable NonConstOpCodeMapPtr  mxSymbolsNative;    // native symbols
    mutable NonConstOpCodeMapPtr  mxSymbolsEnglish;   // English symbols
    mutable NonConstOpCodeMapPtr  mxSymbolsEnglishXL; // English Excel symbols (for VBA formula parsing)
    mutable NonConstOpCodeMapPtr  mxSymbolsOOXML;     // Excel OOXML symbols
};

} // formula


#endif // INCLUDED_FORMULA_FORMULACOMPILER_HXX


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
