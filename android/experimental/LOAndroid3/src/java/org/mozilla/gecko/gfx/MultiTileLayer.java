/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is Mozilla Android code.
*
* The Initial Developer of the Original Code is Mozilla Foundation.
* Portions created by the Initial Developer are Copyright (C) 2011-2012
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*   Chris Lord <chrislord.net@gmail.com>
*   Arkady Blyakher <rkadyb@mit.edu>
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.gfx;

import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.util.Log;

import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Encapsulates the logic needed to draw a layer made of multiple tiles.
 */
public class MultiTileLayer extends Layer {
    private static final String LOGTAG = "GeckoMultiTileLayer";

    private final List<SubTile> mTiles;
    private IntSize mTileSize;
    private IntSize mSize;

    public MultiTileLayer(IntSize tileSize) {
        super();
        mTileSize = tileSize;
        mTiles = new CopyOnWriteArrayList<SubTile>();
        mSize = new IntSize(0,0);
    }

    public void invalidate(Rect dirtyRect) {
        if (!inTransaction()) {
            throw new RuntimeException("invalidate() is only valid inside a transaction");
        }

        for (SubTile layer : mTiles) {
            IntSize tileSize = layer.getSize();
            Rect tileRect = new Rect(layer.x, layer.y, layer.x + tileSize.width, layer.y + tileSize.height);

            if (tileRect.intersect(dirtyRect)) {
                tileRect.offset(-layer.x, -layer.y);
                layer.invalidate(tileRect);
            }
        }
    }

    public void invalidate() {
        for (SubTile layer : mTiles) {
            layer.invalidate();
        }
    }

    public void setSize(IntSize size) {
        mSize = size;
    }

    @Override
    public IntSize getSize() {
        return mSize;
    }

    private void validateTiles() {
        Log.i(LOGTAG, "validateTiles()");

        // Set tile origins and resolution
        refreshTileMetrics(getOrigin(), getResolution(), false);
    }

    @Override
    protected boolean performUpdates(RenderContext context) {
        super.performUpdates(context);

        validateTiles();

        // Iterate over the tiles and decide which ones we'll be drawing
        int dirtyTiles = 0;
        boolean screenUpdateDone = false;
        SubTile firstDirtyTile = null;
        for (SubTile layer : mTiles) {
            // First do a non-texture update to make sure coordinates are
            // up-to-date.
            boolean invalid = layer.getSkipTextureUpdate();
            layer.setSkipTextureUpdate(true);
            layer.performUpdates(context);

            RectF layerBounds = layer.getBounds(context, new FloatSize(layer.getSize()));
            boolean isDirty = layer.isDirty();

            if (isDirty) {
                if (!RectF.intersects(layerBounds, context.viewport)) {
                    if (firstDirtyTile == null)
                        firstDirtyTile = layer;
                    dirtyTiles++;
                    invalid = true;
                } else {
                    // This tile intersects with the screen and is dirty,
                    // update it immediately.
                    layer.setSkipTextureUpdate(false);
                    screenUpdateDone = true;
                    layer.performUpdates(context);
                    invalid = false;
                }
            }

            // We use the SkipTextureUpdate flag as a marker of a tile's
            // validity. This is required, as sometimes layers are drawn
            // without updating first, and we mustn't draw tiles that have
            // been marked as invalid that we haven't updated.
            layer.setSkipTextureUpdate(invalid);
        }

        // Now if no tiles that intersect with the screen were updated, update
        // a single tile that doesn't (if there are any). This has the effect
        // of spreading out non-critical texture upload over time, and smoothing
        // upload-related hitches.
        if (!screenUpdateDone && firstDirtyTile != null) {
            firstDirtyTile.setSkipTextureUpdate(false);
            firstDirtyTile.performUpdates(context);
            dirtyTiles--;
        }

        return (dirtyTiles == 0);
    }

    private void refreshTileMetrics(Point origin, float resolution, boolean inTransaction) {
        IntSize size = getSize();
        for (SubTile layer : mTiles) {
            if (!inTransaction) {
                layer.beginTransaction();
            }

            if (origin != null) {
                layer.setOrigin(new Point(origin.x + layer.x, origin.y + layer.y));
            }
            if (resolution >= 0.0f) {
                layer.setResolution(resolution);
            }

            if (!inTransaction) {
                layer.endTransaction();
            }
        }
    }

    @Override
    public void setOrigin(Point newOrigin) {
        super.setOrigin(newOrigin);
        refreshTileMetrics(newOrigin, -1, true);
    }

    @Override
    public void setResolution(float newResolution) {
        super.setResolution(newResolution);
        refreshTileMetrics(null, newResolution, true);
    }

    @Override
    public void beginTransaction() {
        super.beginTransaction();

        for (SubTile layer : mTiles) {
            layer.beginTransaction();
        }
    }

    @Override
    public void endTransaction() {
        for (SubTile layer : mTiles) {
            layer.endTransaction();
        }

        super.endTransaction();
    }

    @Override
    public void draw(RenderContext context) {
        for (SubTile layer : mTiles) {
            // We use the SkipTextureUpdate flag as a validity flag. If it's false,
            // the contents of this tile are invalid and we shouldn't draw it.
            if (layer.getSkipTextureUpdate())
                continue;

            // Avoid work, only draw tiles that intersect with the viewport
            RectF layerBounds = layer.getBounds(context, new FloatSize(layer.getSize()));
            if (RectF.intersects(layerBounds, context.viewport))
                layer.draw(context);
        }
    }

    @Override
    public Region getValidRegion(RenderContext context) {
        Region validRegion = new Region();
        for (SubTile tile : mTiles) {
            if (tile.getSkipTextureUpdate())
                continue;
            validRegion.op(tile.getValidRegion(context), Region.Op.UNION);
        }

        return validRegion;
    }

    public void addTile(SubTile tile) {
        mTiles.add(tile);
    }

    public List<SubTile> getTiles() {
        return mTiles;
    }
}

