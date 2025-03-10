# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_ExternalProject_ExternalProject,lcms2))

$(eval $(call gb_ExternalProject_register_targets,lcms2,\
	build \
))

ifeq ($(COM),MSC)

$(call gb_ExternalProject_get_state_target,lcms2,build):
	$(call gb_ExternalProject_run,build,\
		MSBuild.exe lcms2_DLL.vcxproj \
			/p:Configuration=$(if $(MSVC_USE_DEBUG_RUNTIME),Debug,Release) \
			/p:Platform=Win32 /p:TargetName=lcms2 \
			$(if $(filter 110,$(VCVER)),/p:PlatformToolset=$(if $(filter 80,$(WINDOWS_SDK_VERSION)),v110,v110_xp) /p:VisualStudioVersion=11.0) \
	,$(if $(filter 120,$(VCVER)),Projects/VC2013/lcms2_DLL,Projects/VC2010/lcms2_DLL))
else
$(call gb_ExternalProject_get_state_target,lcms2,build):
	$(call gb_ExternalProject_run,build,\
		./configure --without-jpeg --without-tiff --with-pic \
			$(if $(CROSS_COMPILING),--build=$(BUILD_PLATFORM) --host=$(HOST_PLATFORM)) \
			CPPFLAGS=" $(SOLARINC)" \
			$(if $(filter-out WNTGCC,$(OS)$(COM)),,CPPFLAGS=" -DCMS_DLL_BUILD") \
			$(if $(filter IOS ANDROID,$(OS)), --disable-shared --enable-static, --enable-shared --disable-static) \
			$(if $(filter MACOSX,$(OS)),--prefix=/@.__________________________________________________OOO) \
		&& cd src \
		&& $(MAKE) \
	)
endif
# vim: set noet sw=4 ts=4:
