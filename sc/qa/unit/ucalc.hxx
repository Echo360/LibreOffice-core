/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SC_QA_UNIT_UCALC_HXX
#define INCLUDED_SC_QA_UNIT_UCALC_HXX

#include "helper/qahelper.hxx"
#include "document.hxx"

struct TestImpl;
class ScUndoPaste;

/**
 * Temporarily set formula grammar.
 */
class FormulaGrammarSwitch
{
    ScDocument* mpDoc;
    formula::FormulaGrammar::Grammar meOldGrammar;
public:
    FormulaGrammarSwitch(ScDocument* pDoc, formula::FormulaGrammar::Grammar eGrammar);
    ~FormulaGrammarSwitch();
};

class Test : public test::BootstrapFixture
{
public:
    struct RangeNameDef
    {
        const char* mpName;
        const char* mpExpr;
        sal_uInt16 mnIndex;
    };

    static ScDocShell* findLoadedDocShellByName(const OUString& rName);
    static bool insertRangeNames(ScDocument* pDoc, ScRangeName* pNames, const RangeNameDef* p, const RangeNameDef* pEnd);
    static void printRange(ScDocument* pDoc, const ScRange& rRange, const char* pCaption);
    static void clearRange(ScDocument* pDoc, const ScRange& rRange);
    static void clearSheet(ScDocument* pDoc, SCTAB nTab);
    static void copyToClip(ScDocument* pSrcDoc, const ScRange& rRange, ScDocument* pClipDoc);
    static void pasteFromClip(ScDocument* pDestDoc, const ScRange& rDestRange, ScDocument* pClipDoc);
    static ScUndoPaste* createUndoPaste(ScDocShell& rDocSh, const ScRange& rRange, ScDocument* pUndoDoc);

    /**
     * Enable or disable expand reference options which controls how
     * references in formula are expanded when inserting rows or columns.
     */
    static void setExpandRefs(bool bExpand);

    static void setCalcAsShown(ScDocument* pDoc, bool bCalcAsShown);

    template<size_t _Size>
    static ScRange insertRangeData(ScDocument* pDoc, const ScAddress& rPos, const char* aData[][_Size], size_t nRowCount)
    {
        ScRange aRange(rPos);
        aRange.aEnd.SetCol(rPos.Col()+_Size-1);
        aRange.aEnd.SetRow(rPos.Row()+nRowCount-1);

        clearRange(pDoc, aRange);

        for (size_t i = 0; i < _Size; ++i)
        {
            for (size_t j = 0; j < nRowCount; ++j)
            {
                if (!aData[j][i])
                    continue;

                SCCOL nCol = i + rPos.Col();
                SCROW nRow = j + rPos.Row();
                pDoc->SetString(nCol, nRow, rPos.Tab(), OUString(aData[j][i], strlen(aData[j][i]), RTL_TEXTENCODING_UTF8));
            }
        }

        printRange(pDoc, aRange, "Range data content");
        return aRange;
    }

    Test();
    virtual ~Test();

    ScDocShell& getDocShell();

    virtual void setUp() SAL_OVERRIDE;
    virtual void tearDown() SAL_OVERRIDE;

    /**
     * Basic performance regression test. Pick some actions that *should* take
     * only a fraction of a second to complete, and make sure they stay that
     * way. We set the threshold to 1 second for each action which should be
     * large enough to accommodate slower machines or machines with high load.
     */
    void testPerf();
    void testCollator();
    void testSharedStringPool();
    void testSharedStringPoolUndoDoc();
    void testRangeList();
    void testMarkData();
    void testInput();
    void testDocStatistics();

    /**
     * The 'data entries' data is a list of strings used for suggestions as
     * the user types in new cell value.
     */
    void testDataEntries();

    /**
     * Selection function is responsible for displaying quick calculation
     * results in the status bar.
     */
    void testSelectionFunction();

    void testFormulaCreateStringFromTokens();
    void testFormulaParseReference();
    void testFetchVectorRefArray();
    void testFormulaHashAndTag();
    void testFormulaTokenEquality();
    void testFormulaRefData();
    void testFormulaCompiler();
    void testFormulaCompilerJumpReordering();
    void testFormulaRefUpdate();
    void testFormulaRefUpdateRange();
    void testFormulaRefUpdateSheets();
    void testFormulaRefUpdateInsertRows();
    void testFormulaRefUpdateInsertColumns();
    void testFormulaRefUpdateMove();
    void testFormulaRefUpdateMoveUndo();
    void testFormulaRefUpdateDeleteContent();
    void testFormulaRefUpdateNamedExpression();
    void testFormulaRefUpdateNamedExpressionMove();
    void testFormulaRefUpdateNamedExpressionExpandRef();
    void testFormulaRefUpdateValidity();
    void testMultipleOperations();
    void testFuncCOLUMN();
    void testFuncCOUNT();
    void testFuncROW();
    void testFuncSUM();
    void testFuncPRODUCT();
    void testFuncSUMPRODUCT();
    void testFuncMIN();
    void testFuncN();
    void testFuncCOUNTIF();
    void testFuncNUMBERVALUE();
    void testFuncLEN();
    void testFuncLOOKUP();
    void testFuncVLOOKUP();
    void testFuncMATCH();
    void testFuncCELL();
    void testFuncDATEDIF();
    void testFuncINDIRECT();
    void testFuncIF();
    void testFuncCHOOSE();
    void testFuncIFERROR();
    void testFuncSHEET();
    void testFuncNOW();
    void testFuncGETPIVOTDATA();
    void testFuncGETPIVOTDATALeafAccess();

    void testExternalRef();
    void testExternalRefFunctions();

    void testCopyToDocument();

    void testHorizontalIterator();
    void testValueIterator();

    /**
     * Basic test for formula dependency tracking.
     */
    void testFormulaDepTracking();

    /**
     * Another test for formula dependency tracking, inspired by fdo#56278.
     */
    void testFormulaDepTracking2();

    void testFormulaDepTrackingDeleteRow();

    void testFormulaMatrixResultUpdate();

    /**
     * More direct test for cell broadcaster management, used to track formula
     * dependencies.
     */
    void testCellBroadcaster();

    void testFuncParam();
    void testNamedRange();
    void testInsertNameList();
    void testCSV();
    void testMatrix();
    void testEnterMixedMatrix();
    void testMatrixEditable();

    /**
     * Basic test for pivot tables.
     */
    void testPivotTable();

    /**
     * Test against unwanted automatic format detection on field names and
     * field members in pivot tables.
     */
    void testPivotTableLabels();

    /**
     * Make sure that we set cells displaying date values numeric cells,
     * rather than text cells.  Grouping by date or number functionality
     * depends on this.
     */
    void testPivotTableDateLabels();

    /**
     * Test for pivot table's filtering functionality by page fields.
     */
    void testPivotTableFilters();

    /**
     * Test for pivot table's named source range.
     */
    void testPivotTableNamedSource();

    /**
     * Test for pivot table cache.  Each dimension in the pivot cache stores
     * only unique values that are sorted in ascending order.
     */
    void testPivotTableCache();

    /**
     * Test for pivot table containing data fields that reference the same
     * source field but different functions.
     */
    void testPivotTableDuplicateDataFields();

    void testPivotTableNormalGrouping();
    void testPivotTableNumberGrouping();
    void testPivotTableDateGrouping();
    void testPivotTableEmptyRows();
    void testPivotTableTextNumber();

    /**
     * Test for checking that pivot table treats strings in a case insensitive
     * manner.
     */
    void testPivotTableCaseInsensitiveStrings();

    /**
     * Test for pivot table's handling of double-precision numbers that are
     * very close together.
     */
    void testPivotTableNumStability();

    /**
     * Test for pivot table that include field with various non-default field
     * refrences.
     */
    void testPivotTableFieldReference();

    /**
     * Test pivot table functionality performed via ScDBDocFunc.
     */
    void testPivotTableDocFunc();

    void testCellCopy();
    void testSheetCopy();
    void testSheetMove();
    void testDataArea();
    void testAutofilter();
    void testCopyPaste();
    void testCopyPasteAsLink();
    void testCopyPasteTranspose();
    void testCopyPasteMultiRange();
    void testCopyPasteSkipEmpty();
    void testCopyPasteSkipEmpty2();
    void testCopyPasteSkipEmptyConditionalFormatting();
    void testCutPasteRefUndo();
    void testMoveRefBetweenSheets();
    void testUndoCut();
    void testMoveBlock();
    void testCopyPasteRelativeFormula();
    void testMergedCells();
    void testUpdateReference();
    void testSearchCells();
    void testSharedFormulas();
    void testSharedFormulasRefUpdate();
    void testSharedFormulasRefUpdateMove();
    void testSharedFormulasRefUpdateMove2();
    void testSharedFormulasRefUpdateRange();
    void testSharedFormulasRefUpdateExternal();
    void testSharedFormulasInsertRow();
    void testSharedFormulasDeleteRows();
    void testSharedFormulasDeleteColumns();
    void testSharedFormulasRefUpdateMoveSheets();
    void testSharedFormulasRefUpdateCopySheets();
    void testSharedFormulasRefUpdateDeleteSheets();
    void testSharedFormulasCopyPaste();
    void testSharedFormulaInsertColumn();
    void testSharedFormulaMoveBlock();
    void testSharedFormulaUpdateOnNamedRangeChange();
    void testSharedFormulaUpdateOnDBChange();
    void testFormulaPosition();

    void testMixData();

    /**
     * Make sure the sheet streams are invalidated properly.
     */
    void testStreamValid();

    /**
     * Test built-in cell functions to make sure their categories and order
     * are correct.
     */
    void testFunctionLists();

    void testGraphicsInGroup();
    void testGraphicsOnSheetMove();

    /**
     * Test toggling relative/absolute flag of cell and cell range references.
     * This corresponds with hitting Shift-F4 while the cursor is on a formula
     * cell.
     */
    void testToggleRefFlag();

    /**
     * Test to make sure correct precedent / dependent cells are obtained when
     * preparing to jump to them.
     */
    void testJumpToPrecedentsDependents();

    void testSetBackgroundColor();
    void testRenameTable();

    void testAutoFill();
    void testCopyPasteFormulas();
    void testCopyPasteFormulasExternalDoc();

    void testFindAreaPosVertical();
    void testFindAreaPosColRight();
    void testSort();
    void testSortHorizontal();
    void testSortSingleRow();
    void testSortWithFormulaRefs();
    void testSortWithStrings();
    void testSortInFormulaGroup();
    void testSortWithCellFormats();
    void testSortRefUpdate();
    void testSortRefUpdate2();
    void testSortRefUpdate3();
    void testSortOutOfPlaceResult();
    void testSortPartialFormulaGroup();
    void testShiftCells();

    void testNoteBasic();
    void testNoteDeleteRow();
    void testNoteDeleteCol();
    void testNoteLifeCycle();
    void testNoteCopyPaste();
    void testAreasWithNotes();
    void testAnchoredRotatedShape();
    void testCellTextWidth();
    void testEditTextIterator();

    void testCondFormatINSDEL();
    void testCondFormatInsertRow();
    void testCondFormatInsertCol();
    void testCondFormatInsertDeleteSheets();
    void testCondCopyPaste();
    void testIconSet();

    void testImportStream();
    void testDeleteContents();
    void testTransliterateText();

    void testColumnFindEditCells();

    CPPUNIT_TEST_SUITE(Test);
#if CALC_TEST_PERF
    CPPUNIT_TEST(testPerf);
#endif
    CPPUNIT_TEST(testCollator);
    CPPUNIT_TEST(testSharedStringPool);
    CPPUNIT_TEST(testSharedStringPoolUndoDoc);
    CPPUNIT_TEST(testRangeList);
    CPPUNIT_TEST(testMarkData);
    CPPUNIT_TEST(testInput);
    CPPUNIT_TEST(testDocStatistics);
    CPPUNIT_TEST(testDataEntries);
    CPPUNIT_TEST(testSelectionFunction);
    CPPUNIT_TEST(testFormulaCreateStringFromTokens);
    CPPUNIT_TEST(testFormulaParseReference);
    CPPUNIT_TEST(testFetchVectorRefArray);
    CPPUNIT_TEST(testFormulaHashAndTag);
    CPPUNIT_TEST(testFormulaTokenEquality);
    CPPUNIT_TEST(testFormulaRefData);
    CPPUNIT_TEST(testFormulaCompiler);
    CPPUNIT_TEST(testFormulaCompilerJumpReordering);
    CPPUNIT_TEST(testFormulaRefUpdate);
    CPPUNIT_TEST(testFormulaRefUpdateRange);
    CPPUNIT_TEST(testFormulaRefUpdateSheets);
    CPPUNIT_TEST(testFormulaRefUpdateInsertRows);
    CPPUNIT_TEST(testFormulaRefUpdateInsertColumns);
    CPPUNIT_TEST(testFormulaRefUpdateMove);
    CPPUNIT_TEST(testFormulaRefUpdateMoveUndo);
    CPPUNIT_TEST(testFormulaRefUpdateDeleteContent);
    CPPUNIT_TEST(testFormulaRefUpdateNamedExpression);
    CPPUNIT_TEST(testFormulaRefUpdateNamedExpressionMove);
    CPPUNIT_TEST(testFormulaRefUpdateNamedExpressionExpandRef);
    CPPUNIT_TEST(testFormulaRefUpdateValidity);
    CPPUNIT_TEST(testMultipleOperations);
    CPPUNIT_TEST(testFuncCOLUMN);
    CPPUNIT_TEST(testFuncCOUNT);
    CPPUNIT_TEST(testFuncROW);
    CPPUNIT_TEST(testFuncSUM);
    CPPUNIT_TEST(testFuncPRODUCT);
    CPPUNIT_TEST(testFuncSUMPRODUCT);
    CPPUNIT_TEST(testFuncMIN);
    CPPUNIT_TEST(testFuncN);
    CPPUNIT_TEST(testFuncCOUNTIF);
    CPPUNIT_TEST(testFuncNUMBERVALUE);
    CPPUNIT_TEST(testFuncLEN);
    CPPUNIT_TEST(testFuncLOOKUP);
    CPPUNIT_TEST(testFuncVLOOKUP);
    CPPUNIT_TEST(testFuncMATCH);
    CPPUNIT_TEST(testFuncCELL);
    CPPUNIT_TEST(testFuncDATEDIF);
    CPPUNIT_TEST(testFuncINDIRECT);
    CPPUNIT_TEST(testFuncIF);
    CPPUNIT_TEST(testFuncCHOOSE);
    CPPUNIT_TEST(testFuncIFERROR);
    CPPUNIT_TEST(testFuncGETPIVOTDATA);
    CPPUNIT_TEST(testFuncGETPIVOTDATALeafAccess);
    CPPUNIT_TEST(testExternalRef);
    CPPUNIT_TEST(testExternalRefFunctions);
    CPPUNIT_TEST(testCopyToDocument);
    CPPUNIT_TEST(testFuncSHEET);
    CPPUNIT_TEST(testFuncNOW);
    CPPUNIT_TEST(testHorizontalIterator);
    CPPUNIT_TEST(testValueIterator);
    CPPUNIT_TEST(testFormulaDepTracking);
    CPPUNIT_TEST(testFormulaDepTracking2);
    CPPUNIT_TEST(testFormulaDepTrackingDeleteRow);
    CPPUNIT_TEST(testFormulaMatrixResultUpdate);
    CPPUNIT_TEST(testCellBroadcaster);
    CPPUNIT_TEST(testFuncParam);
    CPPUNIT_TEST(testNamedRange);
    CPPUNIT_TEST(testInsertNameList);
    CPPUNIT_TEST(testCSV);
    CPPUNIT_TEST(testMatrix);
    CPPUNIT_TEST(testEnterMixedMatrix);
    CPPUNIT_TEST(testMatrixEditable);
    CPPUNIT_TEST(testPivotTable);
    CPPUNIT_TEST(testPivotTableLabels);
    CPPUNIT_TEST(testPivotTableDateLabels);
    CPPUNIT_TEST(testPivotTableFilters);
    CPPUNIT_TEST(testPivotTableNamedSource);
    CPPUNIT_TEST(testPivotTableCache);
    CPPUNIT_TEST(testPivotTableDuplicateDataFields);
    CPPUNIT_TEST(testPivotTableNormalGrouping);
    CPPUNIT_TEST(testPivotTableNumberGrouping);
    CPPUNIT_TEST(testPivotTableDateGrouping);
    CPPUNIT_TEST(testPivotTableEmptyRows);
    CPPUNIT_TEST(testPivotTableTextNumber);
    CPPUNIT_TEST(testPivotTableCaseInsensitiveStrings);
    CPPUNIT_TEST(testPivotTableNumStability);
    CPPUNIT_TEST(testPivotTableFieldReference);
    CPPUNIT_TEST(testPivotTableDocFunc);
    CPPUNIT_TEST(testCellCopy);
    CPPUNIT_TEST(testSheetCopy);
    CPPUNIT_TEST(testSheetMove);
    CPPUNIT_TEST(testDataArea);
    CPPUNIT_TEST(testGraphicsInGroup);
    CPPUNIT_TEST(testGraphicsOnSheetMove);
    CPPUNIT_TEST(testStreamValid);
    CPPUNIT_TEST(testFunctionLists);
    CPPUNIT_TEST(testToggleRefFlag);
    CPPUNIT_TEST(testAutofilter);
    CPPUNIT_TEST(testCopyPaste);
    CPPUNIT_TEST(testCopyPasteAsLink);
    CPPUNIT_TEST(testCopyPasteTranspose);
    CPPUNIT_TEST(testCopyPasteMultiRange);
    CPPUNIT_TEST(testCopyPasteSkipEmpty);
    CPPUNIT_TEST(testCopyPasteSkipEmpty2);
    //CPPUNIT_TEST(testCopyPasteSkipEmptyConditionalFormatting);
    CPPUNIT_TEST(testCutPasteRefUndo);
    CPPUNIT_TEST(testMoveRefBetweenSheets);
    CPPUNIT_TEST(testUndoCut);
    CPPUNIT_TEST(testMoveBlock);
    CPPUNIT_TEST(testCopyPasteRelativeFormula);
    CPPUNIT_TEST(testMergedCells);
    CPPUNIT_TEST(testUpdateReference);
    CPPUNIT_TEST(testSearchCells);
    CPPUNIT_TEST(testSharedFormulas);
    CPPUNIT_TEST(testSharedFormulasRefUpdate);
    CPPUNIT_TEST(testSharedFormulasRefUpdateMove);
    CPPUNIT_TEST(testSharedFormulasRefUpdateMove2);
    CPPUNIT_TEST(testSharedFormulasRefUpdateRange);
    CPPUNIT_TEST(testSharedFormulasRefUpdateExternal);
    CPPUNIT_TEST(testSharedFormulasInsertRow);
    CPPUNIT_TEST(testSharedFormulasDeleteRows);
    CPPUNIT_TEST(testSharedFormulasDeleteColumns);
    CPPUNIT_TEST(testSharedFormulasRefUpdateMoveSheets);
    CPPUNIT_TEST(testSharedFormulasRefUpdateCopySheets);
    CPPUNIT_TEST(testSharedFormulasRefUpdateDeleteSheets);
    CPPUNIT_TEST(testSharedFormulasCopyPaste);
    CPPUNIT_TEST(testSharedFormulaInsertColumn);
    CPPUNIT_TEST(testSharedFormulaUpdateOnNamedRangeChange);
    CPPUNIT_TEST(testSharedFormulaUpdateOnDBChange);
    CPPUNIT_TEST(testFormulaPosition);
    CPPUNIT_TEST(testMixData);
    CPPUNIT_TEST(testJumpToPrecedentsDependents);
    CPPUNIT_TEST(testSetBackgroundColor);
    CPPUNIT_TEST(testRenameTable);
    CPPUNIT_TEST(testAutoFill);
    CPPUNIT_TEST(testCopyPasteFormulas);
    CPPUNIT_TEST(testCopyPasteFormulasExternalDoc);
    CPPUNIT_TEST(testFindAreaPosVertical);
    CPPUNIT_TEST(testFindAreaPosColRight);
    CPPUNIT_TEST(testSort);
    CPPUNIT_TEST(testSortHorizontal);
    CPPUNIT_TEST(testSortSingleRow);
    CPPUNIT_TEST(testSortWithFormulaRefs);
    CPPUNIT_TEST(testSortWithStrings);
    CPPUNIT_TEST(testSortInFormulaGroup);
    CPPUNIT_TEST(testSortWithCellFormats);
    CPPUNIT_TEST(testSortRefUpdate);
    CPPUNIT_TEST(testSortRefUpdate2);
    CPPUNIT_TEST(testSortRefUpdate3);
    CPPUNIT_TEST(testSortOutOfPlaceResult);
    CPPUNIT_TEST(testSortPartialFormulaGroup);
    CPPUNIT_TEST(testShiftCells);
    CPPUNIT_TEST(testNoteBasic);
    CPPUNIT_TEST(testNoteDeleteRow);
    CPPUNIT_TEST(testNoteDeleteCol);
    CPPUNIT_TEST(testNoteLifeCycle);
    CPPUNIT_TEST(testNoteCopyPaste);
    CPPUNIT_TEST(testAreasWithNotes);
    CPPUNIT_TEST(testAnchoredRotatedShape);
    CPPUNIT_TEST(testCellTextWidth);
    CPPUNIT_TEST(testEditTextIterator);
    CPPUNIT_TEST(testCondFormatINSDEL);
    CPPUNIT_TEST(testCondFormatInsertRow);
    CPPUNIT_TEST(testCondFormatInsertCol);
    CPPUNIT_TEST(testCondFormatInsertDeleteSheets);
    CPPUNIT_TEST(testCondCopyPaste);
    CPPUNIT_TEST(testIconSet);
    CPPUNIT_TEST(testImportStream);
    CPPUNIT_TEST(testDeleteContents);
    CPPUNIT_TEST(testTransliterateText);
    CPPUNIT_TEST(testColumnFindEditCells);
    CPPUNIT_TEST_SUITE_END();

private:
    TestImpl* m_pImpl;
    ScDocument *m_pDoc;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
