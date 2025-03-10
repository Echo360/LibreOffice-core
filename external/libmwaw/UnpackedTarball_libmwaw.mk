# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_UnpackedTarball_UnpackedTarball,libmwaw))

$(eval $(call gb_UnpackedTarball_set_tarball,libmwaw,$(MWAW_TARBALL)))

$(eval $(call gb_UnpackedTarball_set_patchlevel,libmwaw,1))

$(eval $(call gb_UnpackedTarball_add_patches,libmwaw,\
	external/libmwaw/0001-librevenge-stream-is-optional-don-t-depend-on-it.patch \
	external/libmwaw/0002-librevenge-stream-is-optional-don-t-depend-on-it.patch \
	external/libmwaw/0001-msvc2013-does-not-like-this.patch \
))

# vim: set noet sw=4 ts=4:
