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

#include <vcl/bitmapfilter.hxx>

#ifndef INCLUDED_VCL_BITMAPSCALESUPER_HXX
#define INCLUDED_VCL_BITMAPSCALESUPER_HXX

class VCL_DLLPUBLIC BitmapScaleSuper : public BitmapFilter
{
    double mrScaleX;
    double mrScaleY;

public:
    BitmapScaleSuper(const double& rScaleX, const double& rScaleY);
    virtual ~BitmapScaleSuper();
    virtual bool filter(Bitmap& rBitmap) SAL_OVERRIDE;
};

#endif // INCLUDED_VCL_BITMAPSCALESUPER_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
