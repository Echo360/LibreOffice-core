/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SW_INC_TEXTBOXHELPER_HXX
#define INCLUDED_SW_INC_TEXTBOXHELPER_HXX

#include <map>
#include <set>
#include <vector>

#include <com/sun/star/drawing/XShape.hpp>
#include <com/sun/star/uno/Any.h>
#include <com/sun/star/uno/Type.h>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>

#include <swdllapi.h>

class SdrPage;
class SfxItemSet;
class SwFrmFmt;
class SwFrmFmts;
class SwFmtCntnt;
class SwDoc;
class Rectangle;
class _ZSortFly;

/**
 * A TextBox is a TextFrame, that is tied to a drawinglayer shape.
 *
 * This class provides helper methods to create, query and maintain such
 * TextBoxes.
 */
class SW_DLLPUBLIC SwTextBoxHelper
{
public:
    /// Maps a draw format to a fly format.
    typedef std::map<const SwFrmFmt*, const SwFrmFmt*> SavedLink;
    /// Maps a draw format to content.
    typedef std::map<const SwFrmFmt*, SwFmtCntnt> SavedContent;
    /// Create a TextBox for a shape.
    static void create(SwFrmFmt* pShape);
    /// Destroy a TextBox for a shape.
    static void destroy(SwFrmFmt* pShape);
    /// Get interface of a shape's TextBox, if there is any.
    static css::uno::Any queryInterface(SwFrmFmt* pShape, const css::uno::Type& rType);

    /// Sync property of TextBox with the one of the shape.
    static void syncProperty(SwFrmFmt* pShape, sal_uInt16 nWID, sal_uInt8 nMemberID, const css::uno::Any& rValue);
    /// Does the same, but works on properties which lack an sw-specific WID / MemberID.
    static void syncProperty(SwFrmFmt* pShape, const OUString& rPropertyName, const css::uno::Any& rValue);
    /// Get a property of the underlying TextFrame.
    static void getProperty(SwFrmFmt* pShape, sal_uInt16 nWID, sal_uInt8 nMemberID, css::uno::Any& rValue);

    /// Similar to syncProperty(), but used by the internal API (e.g. for UI purposes).
    static void syncFlyFrmAttr(SwFrmFmt& rShape, SfxItemSet& rSet);

    /// If we have an associated TextFrame, then return that.
    static SwFrmFmt* findTextBox(SwFrmFmt* pShape);
    static SwFrmFmt* findTextBox(css::uno::Reference<css::drawing::XShape> xShape);
    /// Return the textbox rectangle of a draw shape (in twips).
    static Rectangle getTextRectangle(SwFrmFmt* pShape, bool bAbsolute = true);

    /// Look up TextFrames in a document, which are in fact TextBoxes.
    static std::set<SwFrmFmt*> findTextBoxes(const SwDoc* pDoc);
    /// Build a textbox -> shape format map.
    static std::map<SwFrmFmt*, SwFrmFmt*> findShapes(const SwDoc* pDoc);
    /// Count number of shapes in the document, excluding TextBoxes.
    static sal_Int32 getCount(SdrPage* pPage, std::set<SwFrmFmt*>& rTextBoxes);
    /// Get a shape by index, excluding TextBoxes.
    static css::uno::Any getByIndex(SdrPage* pPage, sal_Int32 nIndex, std::set<SwFrmFmt*>& rTextBoxes) throw(css::lang::IndexOutOfBoundsException);

    /// Saves the current shape -> textbox links in a map, so they can be restored later.
    static void saveLinks(const SwFrmFmts& rFormats, std::map<const SwFrmFmt*, const SwFrmFmt*>& rLinks);
    /// Reset the shape -> textbox link on the shape, and save it to the map, so it can be restored later.
    static void resetLink(SwFrmFmt* pShape, std::map<const SwFrmFmt*, SwFmtCntnt>& rOldContent);
    /// Undo the effect of saveLinks() + individual resetLink() calls.
    static void restoreLinks(std::set<_ZSortFly>& rOld, std::vector<SwFrmFmt*>& rNew, SavedLink& rSavedLinks, SavedContent& rResetContent);
};

#endif // INCLUDED_SW_INC_TEXTBOXHELPER_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
