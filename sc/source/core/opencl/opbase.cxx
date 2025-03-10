/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "opbase.hxx"

using namespace formula;

namespace sc { namespace opencl {

DynamicKernelArgument::DynamicKernelArgument(const std::string &s,
   FormulaTreeNodeRef ft):
    mSymName(s), mFormulaTree(ft) {}

/// Generate use/references to the argument
void DynamicKernelArgument::GenDeclRef(std::stringstream &ss) const
{
    ss << mSymName;
}

FormulaToken* DynamicKernelArgument::GetFormulaToken(void) const
{
    return mFormulaTree->GetFormulaToken();
}

VectorRef::VectorRef(const std::string &s, FormulaTreeNodeRef ft, int idx):
    DynamicKernelArgument(s, ft), mpClmem(NULL), mnIndex(idx)
{
    if (mnIndex)
    {
        std::stringstream ss;
        ss << mSymName << "s" << mnIndex;
        mSymName = ss.str();
    }
}

VectorRef::~VectorRef()
{
    if (mpClmem) {
        clReleaseMemObject(mpClmem);
    }
}

/// Generate declaration
void VectorRef::GenDecl(std::stringstream &ss) const
{
    ss << "__global double *"<<mSymName;
}

/// When declared as input to a sliding window function
void VectorRef::GenSlidingWindowDecl(std::stringstream &ss) const
{
    VectorRef::GenDecl(ss);
}

/// When referenced in a sliding window function
std::string VectorRef::GenSlidingWindowDeclRef(bool nested) const
{
    std::stringstream ss;
    formula::SingleVectorRefToken *pSVR =
        dynamic_cast<formula::SingleVectorRefToken*>(DynamicKernelArgument::GetFormulaToken());
    if (pSVR&&!nested)
        ss << "(gid0 < " << pSVR->GetArrayLength() << "?";
    ss << mSymName << "[gid0]";
    if (pSVR&&!nested)
        ss << ":NAN)";
    return ss.str();
}

size_t VectorRef::GetWindowSize(void) const
{
    FormulaToken *pCur = mFormulaTree->GetFormulaToken();
    assert(pCur);
    if (const formula::DoubleVectorRefToken* pCurDVR =
        dynamic_cast<const formula::DoubleVectorRefToken *>(pCur))
    {
        return pCurDVR->GetRefRowSize();
    }
    else if (dynamic_cast<const formula::SingleVectorRefToken *>(pCur))
    {
        // Prepare intermediate results (on CPU for now)
        return 1;
    }
    else
    {
        throw Unhandled();
    }
}

void Normal::GenSlidingWindowFunction(
    std::stringstream &ss, const std::string &sSymName, SubArguments &vSubArguments)
{
    ArgVector argVector;
    ss << "\ndouble " << sSymName;
    ss << "_"<< BinFuncName() <<"(";
    for (unsigned i = 0; i < vSubArguments.size(); i++)
    {
        if (i)
            ss << ",";
        vSubArguments[i]->GenSlidingWindowDecl(ss);
        argVector.push_back(vSubArguments[i]->GenSlidingWindowDeclRef());
    }
    ss << ") {\n\t";
    ss << "double tmp = " << GetBottom() <<";\n\t";
    ss << "int gid0 = get_global_id(0);\n\t";
    ss << "tmp = ";
    ss << Gen(argVector);
    ss << ";\n\t";
    ss << "return tmp;\n";
    ss << "}";
}

void CheckVariables::GenTmpVariables(
    std::stringstream & ss, SubArguments & vSubArguments)
{
    for(unsigned i=0;i<vSubArguments.size();i++)
    {
         ss << "    double tmp";
         ss << i;
         ss <<";\n";
    }
}

void CheckVariables::CheckSubArgumentIsNan( std::stringstream & ss,
    SubArguments &vSubArguments,  int argumentNum)
{
    int i = argumentNum;
     if(vSubArguments[i]->GetFormulaToken()->GetType() ==
     formula::svSingleVectorRef)
     {
         const formula::SingleVectorRefToken*pTmpDVR1= static_cast<const
         formula::SingleVectorRefToken *>(vSubArguments[i]->GetFormulaToken());
         ss<< "    if(singleIndex>=";
         ss<< pTmpDVR1->GetArrayLength();
         ss<<" ||";
         ss<< "isNan(";
         ss<< vSubArguments[i]->GenSlidingWindowDeclRef(true);
         ss<<"))\n";
         ss<< "        tmp";
         ss<< i;
         ss <<"=0;\n    else \n";
         ss <<"        tmp";
         ss <<i;
         ss << "=";
         ss << vSubArguments[i]->GenSlidingWindowDeclRef(true);
         ss<<";\n";
     }
     if(vSubArguments[i]->GetFormulaToken()->GetType() ==
     formula::svDoubleVectorRef)
     {
         const formula::DoubleVectorRefToken*pTmpDVR2= static_cast<const
         formula::DoubleVectorRefToken *>(vSubArguments[i]->GetFormulaToken());
         ss<< "    if(doubleIndex>=";
         ss<< pTmpDVR2->GetArrayLength();
         ss<<" ||";
         ss<< "isNan(";
         ss<< vSubArguments[i]->GenSlidingWindowDeclRef(false);
         ss<<"))\n";
         ss<< "        tmp";
         ss<< i;
         ss <<"=0;\n    else \n";
         ss <<"        tmp";
         ss <<i;
         ss << "=";
         ss << vSubArguments[i]->GenSlidingWindowDeclRef(false);
         ss<<";\n";
     }
     if(vSubArguments[i]->GetFormulaToken()->GetType() == formula::svDouble ||
     vSubArguments[i]->GetFormulaToken()->GetOpCode() != ocPush)
     {
         ss<< "    if(";
         ss<< "isNan(";
         ss<< vSubArguments[i]->GenSlidingWindowDeclRef();
         ss<<"))\n";
         ss<< "        tmp";
         ss<< i;
         ss <<"=0;\n    else \n";
         ss <<"        tmp";
         ss <<i;
         ss << "=";
         ss << vSubArguments[i]->GenSlidingWindowDeclRef();
         ss<<";\n";

     }

}

void CheckVariables::CheckSubArgumentIsNan2( std::stringstream & ss,
    SubArguments &vSubArguments,  int argumentNum, std::string p)
{
    int i = argumentNum;
    if(vSubArguments[i]->GetFormulaToken()->GetType() == formula::svDouble)
    {
        ss <<"    tmp";
        ss <<i;
        ss << "=";
        vSubArguments[i]->GenDeclRef(ss);
        ss<<";\n";
        return;
    }

#ifdef ISNAN
    ss<< "    tmp";
    ss<< i;
    ss<< "= fsum(";
    vSubArguments[i]->GenDeclRef(ss);
    if(vSubArguments[i]->GetFormulaToken()->GetType() ==
     formula::svDoubleVectorRef)
        ss<<"["<< p.c_str()<< "]";
    else  if(vSubArguments[i]->GetFormulaToken()->GetType() ==
     formula::svSingleVectorRef)
        ss<<"[get_group_id(1)]";
    ss<<", 0);\n";
#else
    ss <<"    tmp";
    ss <<i;
    ss << "=";
    vSubArguments[i]->GenDeclRef(ss);
    if(vSubArguments[i]->GetFormulaToken()->GetType() ==
            formula::svDoubleVectorRef)
        ss<<"["<< p.c_str()<< "]";
    else  if(vSubArguments[i]->GetFormulaToken()->GetType() ==
            formula::svSingleVectorRef)
        ss<<"[get_group_id(1)]";

    ss<<";\n";
#endif
}

void CheckVariables::CheckAllSubArgumentIsNan(
    std::stringstream & ss, SubArguments & vSubArguments)
{
    ss<<"    int k = gid0;\n";
     for(unsigned i=0;i<vSubArguments.size();i++)
    {
        CheckSubArgumentIsNan(ss,vSubArguments,i);
    }
}

void CheckVariables::UnrollDoubleVector( std::stringstream & ss,
std::stringstream & unrollstr,const formula::DoubleVectorRefToken* pCurDVR,
int nCurWindowSize)
{
    int unrollSize = 16;
    if (!pCurDVR->IsStartFixed() && pCurDVR->IsEndFixed()) {
        ss << "    loop = ("<<nCurWindowSize<<" - gid0)/";
        ss << unrollSize<<";\n";
    } else if (pCurDVR->IsStartFixed() && !pCurDVR->IsEndFixed()) {
        ss << "    loop = ("<<nCurWindowSize<<" + gid0)/";
        ss << unrollSize<<";\n";

    } else {
        ss << "    loop = "<<nCurWindowSize<<"/"<< unrollSize<<";\n";
    }

    ss << "    for ( int j = 0;j< loop; j++)\n";
    ss << "    {\n";
    ss << "        int i = ";
    if (!pCurDVR->IsStartFixed()&& pCurDVR->IsEndFixed()) {
       ss << "gid0 + j * "<< unrollSize <<";\n";
    }else {
       ss << "j * "<< unrollSize <<";\n";
    }

    if(!pCurDVR->IsStartFixed() && !pCurDVR->IsEndFixed())
    {
       ss << "        int doubleIndex = i+gid0;\n";
    }else
    {
       ss << "        int doubleIndex = i;\n";
    }

    for(int j =0;j < unrollSize;j++)
    {
        ss << unrollstr.str();
        ss << "i++;\n";
        ss << "doubleIndex++;\n";
    }
     ss << "    }\n";
     ss << "    for (int i = ";
     if (!pCurDVR->IsStartFixed() && pCurDVR->IsEndFixed()) {
        ss << "gid0 + loop *"<<unrollSize<<"; i < ";
        ss << nCurWindowSize <<"; i++)\n";
     } else if (pCurDVR->IsStartFixed() && !pCurDVR->IsEndFixed()) {
        ss << "0 + loop *"<<unrollSize<<"; i < gid0+";
        ss << nCurWindowSize <<"; i++)\n";
     } else {
        ss << "0 + loop *"<<unrollSize<<"; i < ";
        ss << nCurWindowSize <<"; i++)\n";
     }
     ss << "    {\n";
     if(!pCurDVR->IsStartFixed() && !pCurDVR->IsEndFixed())
     {
        ss << "        int doubleIndex = i+gid0;\n";
     }else
     {
        ss << "        int doubleIndex = i;\n";
     }
     ss << unrollstr.str();
     ss << "    }\n";
}

}}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
