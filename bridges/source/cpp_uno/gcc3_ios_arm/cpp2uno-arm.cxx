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

#ifdef __arm

// For iOS devices (ARM). Basically a copy of
// ../gcc3_linux_arm/cpp2uno.cxx with some cleanups and necessary
// changes: No dynamic code generation as that is prohibited for apps
// in the App Store. Instead we use a set of pre-generated snippets.

// No attempts at factoring out the large amounts of more or less
// common code in this and cpp2uno-i386.cxx have been done. Which is
// sad. But then the whole bridges/source/cpp_uno is full of
// copy/paste. So I continue in that tradition...

// Note that for iOS, neither __ARM_EABI__ or __ARM_PCS_VFP are
// defined.  The ifdefs for those have been left in place to keep this
// file as close to ../gcc3_linux_arm/cpp2uno.cxx as possible, to make
// future unification easier.

#include <com/sun/star/uno/RuntimeException.hpp>
#include <uno/data.h>
#include <typelib/typedescription.hxx>

#include "bridges/cpp_uno/shared/bridge.hxx"
#include "bridges/cpp_uno/shared/cppinterfaceproxy.hxx"
#include "bridges/cpp_uno/shared/types.hxx"
#include "bridges/cpp_uno/shared/vtablefactory.hxx"

#include "share.hxx"

extern "C" {
    extern int nFunIndexes, nVtableOffsets;
    extern int codeSnippets[];
}

using namespace ::com::sun::star::uno;

namespace
{

    static typelib_TypeClass cpp2uno_call(
        bridges::cpp_uno::shared::CppInterfaceProxy* pThis,
        const typelib_TypeDescription * pMemberTypeDescr,
        typelib_TypeDescriptionReference * pReturnTypeRef,
        sal_Int32 nParams, typelib_MethodParameter * pParams,
        void ** pCallStack,
        sal_Int64 * pRegisterReturn /* space for register return */ )
    {
        // pCallStack: ret, [return ptr], this, params
        char * pTopStack = (char *)(pCallStack + 0);
        char * pCppStack = pTopStack;

#ifdef __ARM_PCS_VFP
        int dc = 0;
        char * pFloatArgs = (char *)(pCppStack - 64);
#endif
        // return
        typelib_TypeDescription * pReturnTypeDescr = 0;
        if (pReturnTypeRef)
            TYPELIB_DANGER_GET( &pReturnTypeDescr, pReturnTypeRef );

        void * pUnoReturn = 0;
        // complex return ptr: if != 0 && != pUnoReturn, reconversion need
        void * pCppReturn = 0;

        if (pReturnTypeDescr)
        {
            if (!arm::return_in_hidden_param(pReturnTypeRef))
                pUnoReturn = pRegisterReturn; // direct way for simple types
            else // complex return via ptr (pCppReturn)
            {
                pCppReturn = *(void **)pCppStack;
                pCppStack += sizeof(void *);

                pUnoReturn = (bridges::cpp_uno::shared::relatesToInterfaceType(
                    pReturnTypeDescr )
                        ? alloca( pReturnTypeDescr->nSize )
                        : pCppReturn); // direct way
            }
        }
        // pop this
        pCppStack += sizeof( void* );

        // stack space
        OSL_ENSURE( sizeof(void *) == sizeof(sal_Int32),
            "### unexpected size!" );
        // parameters
        void ** pUnoArgs = (void **)alloca( 4 * sizeof(void *) * nParams );
        void ** pCppArgs = pUnoArgs + nParams;
        // indices of values this have to be converted (interface conversion
        // cpp<=>uno)
        sal_Int32 * pTempIndices = (sal_Int32 *)(pUnoArgs + (2 * nParams));
        // type descriptions for reconversions
        typelib_TypeDescription ** ppTempParamTypeDescr =
            (typelib_TypeDescription **)(pUnoArgs + (3 * nParams));

        sal_Int32 nTempIndices   = 0;

        for ( sal_Int32 nPos = 0; nPos < nParams; ++nPos )
        {
            const typelib_MethodParameter & rParam = pParams[nPos];
            typelib_TypeDescription * pParamTypeDescr = 0;
            TYPELIB_DANGER_GET( &pParamTypeDescr, rParam.pTypeRef );

            if (!rParam.bOut &&
                bridges::cpp_uno::shared::isSimpleType( pParamTypeDescr ))
            {
#ifdef __ARM_EABI__
                switch (pParamTypeDescr->eTypeClass)
                {
                    case typelib_TypeClass_HYPER:
                    case typelib_TypeClass_UNSIGNED_HYPER:
#ifndef __ARM_PCS_VFP
                    case typelib_TypeClass_DOUBLE:
#endif
                        if ((pCppStack - pTopStack) % 8) pCppStack+=sizeof(sal_Int32); //align to 8
                        break;
                    default:
                        break;
                }
#endif

// For armhf we get the floating point arguments from a different area of the stack
#ifdef __ARM_PCS_VFP
                if (pParamTypeDescr->eTypeClass == typelib_TypeClass_FLOAT)
                {
                    pCppArgs[nPos] =  pUnoArgs[nPos] = pFloatArgs;
                    pFloatArgs += sizeof(float);
                } else
                if (pParamTypeDescr->eTypeClass == typelib_TypeClass_DOUBLE)
                {
                    if ((pFloatArgs - pTopStack) % 8) pFloatArgs+=sizeof(float); //align to 8
                    pCppArgs[nPos] = pUnoArgs[nPos] = pFloatArgs;
                    pFloatArgs += sizeof(double);
                    if (++dc == arm::MAX_FPR_REGS) {
                        if (pCppStack - pTopStack < 16)
                            pCppStack = pTopStack + 16;
                        pFloatArgs = pCppStack;
                    }
                } else
#endif
                    pCppArgs[nPos] = pUnoArgs[nPos] = pCppStack;

                switch (pParamTypeDescr->eTypeClass)
                {
                    case typelib_TypeClass_HYPER:
                    case typelib_TypeClass_UNSIGNED_HYPER:
#ifndef __ARM_PCS_VFP
                    case typelib_TypeClass_DOUBLE:
#endif
                        pCppStack += sizeof(sal_Int32); // extra long
                        break;
                    default:
                        break;
                }
                // no longer needed
                TYPELIB_DANGER_RELEASE( pParamTypeDescr );
            }
            else // ptr to complex value | ref
            {
                pCppArgs[nPos] = *(void **)pCppStack;

                if (! rParam.bIn) // is pure out
                {
                    // uno out is unconstructed mem!
                    pUnoArgs[nPos] = alloca( pParamTypeDescr->nSize );
                    pTempIndices[nTempIndices] = nPos;
                    // will be released at reconversion
                    ppTempParamTypeDescr[nTempIndices++] = pParamTypeDescr;
                }
                // is in/inout
                else if (bridges::cpp_uno::shared::relatesToInterfaceType(
                    pParamTypeDescr ))
                {
                    uno_copyAndConvertData( pUnoArgs[nPos] =
                        alloca( pParamTypeDescr->nSize ),
                        *(void **)pCppStack, pParamTypeDescr,
                        pThis->getBridge()->getCpp2Uno() );
                    pTempIndices[nTempIndices] = nPos; // has to be reconverted
                    // will be released at reconversion
                    ppTempParamTypeDescr[nTempIndices++] = pParamTypeDescr;
                }
                else // direct way
                {
                    pUnoArgs[nPos] = *(void **)pCppStack;
                    // no longer needed
                    TYPELIB_DANGER_RELEASE( pParamTypeDescr );
                }
            }
#ifdef __ARM_PCS_VFP
            // use the stack for output parameters or non floating point values
                if (rParam.bOut ||
                        ((pParamTypeDescr->eTypeClass != typelib_TypeClass_DOUBLE)
                         && (pParamTypeDescr->eTypeClass != typelib_TypeClass_FLOAT))
                    )
#endif
            pCppStack += sizeof(sal_Int32); // standard parameter length
        }

        // ExceptionHolder
        uno_Any aUnoExc; // Any will be constructed by callee
        uno_Any * pUnoExc = &aUnoExc;

        // invoke uno dispatch call
        (*pThis->getUnoI()->pDispatcher)(
          pThis->getUnoI(), pMemberTypeDescr, pUnoReturn, pUnoArgs, &pUnoExc );

        // in case an exception occurred...
        if (pUnoExc)
        {
            // destruct temporary in/inout params
            for ( ; nTempIndices--; )
            {
                sal_Int32 nIndex = pTempIndices[nTempIndices];

                if (pParams[nIndex].bIn) // is in/inout => was constructed
                    uno_destructData( pUnoArgs[nIndex],
                        ppTempParamTypeDescr[nTempIndices], 0 );
                TYPELIB_DANGER_RELEASE( ppTempParamTypeDescr[nTempIndices] );
            }
            if (pReturnTypeDescr)
                TYPELIB_DANGER_RELEASE( pReturnTypeDescr );

            CPPU_CURRENT_NAMESPACE::raiseException( &aUnoExc,
                pThis->getBridge()->getUno2Cpp() ); // has to destruct the any
            // is here for dummy
            return typelib_TypeClass_VOID;
        }
        else // else no exception occurred...
        {
            // temporary params
            for ( ; nTempIndices--; )
            {
                sal_Int32 nIndex = pTempIndices[nTempIndices];
                typelib_TypeDescription * pParamTypeDescr =
                    ppTempParamTypeDescr[nTempIndices];

                if (pParams[nIndex].bOut) // inout/out
                {
                    // convert and assign
                    uno_destructData( pCppArgs[nIndex], pParamTypeDescr,
                        cpp_release );
                    uno_copyAndConvertData( pCppArgs[nIndex], pUnoArgs[nIndex],
                        pParamTypeDescr, pThis->getBridge()->getUno2Cpp() );
                }
                // destroy temp uno param
                uno_destructData( pUnoArgs[nIndex], pParamTypeDescr, 0 );

                TYPELIB_DANGER_RELEASE( pParamTypeDescr );
            }
            // return
            if (pCppReturn) // has complex return
            {
                if (pUnoReturn != pCppReturn) // needs reconversion
                {
                    uno_copyAndConvertData( pCppReturn, pUnoReturn,
                        pReturnTypeDescr, pThis->getBridge()->getUno2Cpp() );
                    // destroy temp uno return
                    uno_destructData( pUnoReturn, pReturnTypeDescr, 0 );
                }
                // complex return ptr is set to eax
                *(void **)pRegisterReturn = pCppReturn;
            }
            if (pReturnTypeDescr)
            {
                typelib_TypeClass eRet =
                    (typelib_TypeClass)pReturnTypeDescr->eTypeClass;
                TYPELIB_DANGER_RELEASE( pReturnTypeDescr );
                return eRet;
            }
            else
                return typelib_TypeClass_VOID;
        }
    }


    static typelib_TypeClass cpp_mediate(
        sal_Int32 nFunctionIndex, sal_Int32 nVtableOffset,
        void ** pCallStack,
        sal_Int64 * pRegisterReturn /* space for register return */ )
    {
        OSL_ENSURE( sizeof(sal_Int32)==sizeof(void *), "### unexpected!" );

        // pCallStack: [ret *], this, params
        // _this_ ptr is patched cppu_XInterfaceProxy object
        void *pThis;
        if( nFunctionIndex & 0x80000000 )
        {
            nFunctionIndex &= 0x7fffffff;
            pThis = pCallStack[1];
        }
        else
        {
            pThis = pCallStack[0];
        }

        pThis = static_cast< char * >(pThis) - nVtableOffset;
        bridges::cpp_uno::shared::CppInterfaceProxy * pCppI =
            bridges::cpp_uno::shared::CppInterfaceProxy::castInterfaceToProxy(
                pThis);

        typelib_InterfaceTypeDescription * pTypeDescr = pCppI->getTypeDescr();

        OSL_ENSURE( nFunctionIndex < pTypeDescr->nMapFunctionIndexToMemberIndex,
            "### illegal vtable index!" );
        if (nFunctionIndex >= pTypeDescr->nMapFunctionIndexToMemberIndex)
        {
            throw RuntimeException( "illegal vtable index!", (XInterface *)pCppI );
        }

        // determine called method
        OSL_ENSURE( nFunctionIndex < pTypeDescr->nMapFunctionIndexToMemberIndex,
            "### illegal vtable index!" );
        sal_Int32 nMemberPos =
            pTypeDescr->pMapFunctionIndexToMemberIndex[nFunctionIndex];
        OSL_ENSURE( nMemberPos < pTypeDescr->nAllMembers,
            "### illegal member index!" );

        TypeDescription aMemberDescr( pTypeDescr->ppAllMembers[nMemberPos] );

        typelib_TypeClass eRet;
        switch (aMemberDescr.get()->eTypeClass)
        {
        case typelib_TypeClass_INTERFACE_ATTRIBUTE:
        {
            if (pTypeDescr->pMapMemberIndexToFunctionIndex[nMemberPos] ==
                nFunctionIndex)
            {
                // is GET method
                eRet = cpp2uno_call(
                    pCppI, aMemberDescr.get(),
                    ((typelib_InterfaceAttributeTypeDescription *)aMemberDescr.get())->pAttributeTypeRef,
                    0, 0, // no params
                    pCallStack, pRegisterReturn );
            }
            else
            {
                // is SET method
                typelib_MethodParameter aParam;
                aParam.pTypeRef =
                    ((typelib_InterfaceAttributeTypeDescription *)aMemberDescr.get())->pAttributeTypeRef;
                aParam.bIn      = sal_True;
                aParam.bOut     = sal_False;

                eRet = cpp2uno_call(
                    pCppI, aMemberDescr.get(),
                    0, // indicates void return
                    1, &aParam,
                    pCallStack, pRegisterReturn );
            }
            break;
        }
        case typelib_TypeClass_INTERFACE_METHOD:
        {
            // is METHOD
            switch (nFunctionIndex)
            {
            case 1: // acquire()
                pCppI->acquireProxy(); // non virtual call!
                eRet = typelib_TypeClass_VOID;
                break;
            case 2: // release()
                pCppI->releaseProxy(); // non virtual call!
                eRet = typelib_TypeClass_VOID;
                break;
            case 0: // queryInterface() opt
            {
                typelib_TypeDescription * pTD = 0;
                TYPELIB_DANGER_GET(&pTD,
                    reinterpret_cast<Type *>(pCallStack[2])->getTypeLibType());
                if (pTD)
                {
                    XInterface * pInterface = 0;
                    (*pCppI->getBridge()->getCppEnv()->getRegisteredInterface)(
                        pCppI->getBridge()->getCppEnv(),
                        (void **)&pInterface, pCppI->getOid().pData,
                        (typelib_InterfaceTypeDescription *)pTD );

                    if (pInterface)
                    {
                        ::uno_any_construct(
                            reinterpret_cast< uno_Any * >( pCallStack[0] ),
                            &pInterface, pTD, cpp_acquire );
                        pInterface->release();
                        TYPELIB_DANGER_RELEASE( pTD );
                        *(void **)pRegisterReturn = pCallStack[0];
                        eRet = typelib_TypeClass_ANY;
                        break;
                    }
                    TYPELIB_DANGER_RELEASE( pTD );
                }
            } // else perform queryInterface()
            default:
                eRet = cpp2uno_call(
                    pCppI, aMemberDescr.get(),
                    ((typelib_InterfaceMethodTypeDescription *)aMemberDescr.get())->pReturnTypeRef,
                    ((typelib_InterfaceMethodTypeDescription *)aMemberDescr.get())->nParams,
                    ((typelib_InterfaceMethodTypeDescription *)aMemberDescr.get())->pParams,
                    pCallStack, pRegisterReturn );
            }
            break;
        }
        default:
        {
            throw RuntimeException( "no member description found!", (XInterface *)pCppI );
        }
        }

        return eRet;
    }
}

/**
 * is called on incoming vtable calls
 * (called by asm snippets)
 */

extern "C" sal_Int64 cpp_vtable_call( long *pFunctionAndOffset,
    void **pCallStack )
{
    sal_Int64 nRegReturn;
    typelib_TypeClass aType = cpp_mediate( pFunctionAndOffset[0], pFunctionAndOffset[1], pCallStack,
        &nRegReturn );

    switch( aType )
    {
        case typelib_TypeClass_BOOLEAN:
        case typelib_TypeClass_BYTE:
            nRegReturn = (unsigned long)(*(unsigned char *)&nRegReturn);
            break;
        case typelib_TypeClass_CHAR:
        case typelib_TypeClass_UNSIGNED_SHORT:
        case typelib_TypeClass_SHORT:
            nRegReturn = (unsigned long)(*(unsigned short *)&nRegReturn);
            break;
        case typelib_TypeClass_ENUM:
        case typelib_TypeClass_UNSIGNED_LONG:
        case typelib_TypeClass_LONG:
            nRegReturn = (unsigned long)(*(unsigned int *)&nRegReturn);
            break;
        case typelib_TypeClass_VOID:
        default:
            break;
    }

    return nRegReturn;
}

namespace
{
    unsigned char *codeSnippet(const typelib_InterfaceTypeDescription *type,
                               const typelib_TypeDescription *member,
                               sal_Int32 functionIndex,
                               sal_Int32 vtableOffset,
                               bool bHasHiddenParam)
    {
        assert(functionIndex < nFunIndexes);
        if (!(functionIndex < nFunIndexes))
            return NULL;

        assert(vtableOffset < nVtableOffsets);
        if (!(vtableOffset < nVtableOffsets))
            return NULL;

        // The codeSnippets table is indexed by functionIndex,
        // vtableOffset, and the has-hidden-param flag

        int index = functionIndex*nVtableOffsets*2 + vtableOffset*2 + bHasHiddenParam;
        unsigned char *result = ((unsigned char *) &codeSnippets) + codeSnippets[index];

        SAL_INFO( "bridges.ios", "codeSnippet(" << OUString(type->aBase.pTypeName) << "::" << OUString(member->pTypeName) << "): [" << functionIndex << "," << vtableOffset << "," << (int)bHasHiddenParam << "]=" << (void *) result << " (" << std::hex << ((int*)result)[0] << "," << ((int*)result)[1] << "," << ((int*)result)[2] << "," << ((int*)result)[3] << ")");

        return result;
    }
}

struct bridges::cpp_uno::shared::VtableFactory::Slot { void * fn; };

bridges::cpp_uno::shared::VtableFactory::Slot *
bridges::cpp_uno::shared::VtableFactory::mapBlockToVtable(void * block)
{
    return static_cast< Slot * >(block) + 2;
}

sal_Size bridges::cpp_uno::shared::VtableFactory::getBlockSize(
    sal_Int32 slotCount)
{
    return (slotCount + 2) * sizeof (Slot);
}

bridges::cpp_uno::shared::VtableFactory::Slot *
bridges::cpp_uno::shared::VtableFactory::initializeBlock(
    void * block, sal_Int32 slotCount, sal_Int32,
    typelib_InterfaceTypeDescription *)
{
    Slot * slots = mapBlockToVtable(block);
    slots[-2].fn = 0;
    slots[-1].fn = 0;
    return slots + slotCount;
}

unsigned char * bridges::cpp_uno::shared::VtableFactory::addLocalFunctions(
    Slot ** slots, unsigned char * code,
    typelib_InterfaceTypeDescription const * type, sal_Int32 functionOffset,
    sal_Int32 functionCount, sal_Int32 vtableOffset)
{
    (*slots) -= functionCount;
    Slot * s = *slots;
    for (sal_Int32 i = 0; i < type->nMembers; ++i)
    {
        typelib_TypeDescription * member = 0;
        TYPELIB_DANGER_GET(&member, type->ppMembers[i]);
        OSL_ASSERT(member != 0);
        switch (member->eTypeClass)
        {
            case typelib_TypeClass_INTERFACE_ATTRIBUTE:
            {
                typelib_InterfaceAttributeTypeDescription *pAttrTD =
                    reinterpret_cast<typelib_InterfaceAttributeTypeDescription *>( member );

                // Getter:
                (s++)->fn = codeSnippet(type, member,
                                        functionOffset++, vtableOffset,
                                        arm::return_in_hidden_param( pAttrTD->pAttributeTypeRef ));

                // Setter:
                if (!pAttrTD->bReadOnly)
                {
                    (s++)->fn = codeSnippet(type, member,
                                            functionOffset++, vtableOffset,
                                            false);
                }
                break;
            }
            case typelib_TypeClass_INTERFACE_METHOD:
            {
                typelib_InterfaceMethodTypeDescription *pMethodTD =
                    reinterpret_cast<
                        typelib_InterfaceMethodTypeDescription * >(member);

                (s++)->fn = codeSnippet(type, member,
                                        functionOffset++, vtableOffset,
                                        arm::return_in_hidden_param(pMethodTD->pReturnTypeRef));
                break;
            }
        default:
            OSL_ASSERT(false);
            break;
        }
        TYPELIB_DANGER_RELEASE(member);
    }
    return code;
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
