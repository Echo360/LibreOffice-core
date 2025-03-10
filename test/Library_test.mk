# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_Library_Library,test))

$(eval $(call gb_Library_add_defs,test,\
    -DOOO_DLLIMPLEMENTATION_TEST \
))

$(eval $(call gb_Library_use_sdk_api,test))

$(eval $(call gb_Library_use_externals,test,\
	boost_headers \
	cppunit \
	libxml2 \
))

$(eval $(call gb_Library_use_libraries,test,\
    comphelper \
    cppu \
    cppuhelper \
	i18nlangtag \
    sal \
    svt \
	tl \
	utl \
	unotest \
	vcl \
	$(gb_UWINAPI) \
))

$(eval $(call gb_Library_add_exception_objects,test,\
    test/source/bootstrapfixture \
    test/source/diff/diff \
    test/source/xmltesttools \
    test/source/htmltesttools \
    test/source/mtfxmldump \
    test/source/xmlwriter \
))

# vim: set noet sw=4 ts=4:
