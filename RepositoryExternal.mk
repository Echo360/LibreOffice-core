# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This file incorporates work covered by the following license notice:
#
#   Licensed to the Apache Software Foundation (ASF) under one or more
#   contributor license agreements. See the NOTICE file distributed
#   with this work for additional information regarding copyright
#   ownership. The ASF licenses this file to you under the Apache
#   License, Version 2.0 (the "License"); you may not use this file
#   except in compliance with the License. You may obtain a copy of
#   the License at http://www.apache.org/licenses/LICENSE-2.0 .
#

# this file describes all the external libraries
# depending on the configure options these may be taken from the system,
# or the internal/bundled copy may be built.

# for every external, a function gb_LinkTarget__use_FOO is defined,
# once for the system case, once for the internal case.

# in the system case, no libraries should be registered, but the target-local
# variable LIBS should be set to FOO_LIBS, and INCLUDES to FOO_CFLAGS.

# External headers

ifneq ($(SYSTEM_MESA_HEADERS),)

gb_LinkTarget__use_mesa_headers :=

else # !SYSTEM_MESA_HEADERS

define gb_LinkTarget__use_mesa_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(SRCDIR)/external/Mesa/inc \
	$$(INCLUDE) \
)

endef

endif # SYSTEM_MESA_HEADERS

ifneq ($(SYSTEM_NPAPI_HEADERS),)

# yes this uses internal headers too...
# they are split across 2 dirs for this reason
define gb_LinkTarget__use_npapi_headers
$(call gb_LinkTarget_set_include,$(1),\
	$(NPAPI_HEADERS_CFLAGS) \
	-I$(SRCDIR)/external/np_sdk \
	$$(INCLUDE) \
)

endef

else #!SYSTEM_NPAPI_HEADERS

define gb_LinkTarget__use_npapi_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(SRCDIR)/external/np_sdk/inc \
	-I$(SRCDIR)/external/np_sdk \
	$$(INCLUDE) \
)

endef

endif #SYSTEM_NPAPI_HEADERS

ifneq ($(SYSTEM_ODBC_HEADERS),)

define gb_LinkTarget__use_odbc_headers
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_ODBC_HEADERS \
)

endef

else # !SYSTEM_ODBC_HEADERS

define gb_LinkTarget__use_odbc_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(SRCDIR)/external/unixODBC/inc \
	$$(INCLUDE) \
)

endef

endif # SYSTEM_ODBC_HEADERS

ifneq ($(SYSTEM_VIGRA),)

gb_LinkTarget__use_vigra_headers :=

else

define gb_LinkTarget__use_vigra_headers
$(call gb_LinkTarget_use_unpacked,$(1),vigra)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,vigra/include) \
	$$(INCLUDE) \
)

endef

endif

ifneq ($(SYSTEM_MDDS),)

gb_ExternalProject__use_mdds_headers :=

define gb_LinkTarget__use_mdds_headers
$(call gb_LinkTarget_set_include,$(1),\
	$(MDDS_CFLAGS) \
	$$(INCLUDE) \
)

endef

else # !SYSTEM_MDDS

define gb_ExternalProject__use_mdds_headers
$(call gb_ExternalProject_use_external_project,$(1),mdds)

endef

define gb_LinkTarget__use_mdds_headers
$(call gb_LinkTarget_use_unpacked,$(1),mdds)
$(call gb_LinkTarget_set_include,$(1),\
	$(MDDS_CFLAGS) \
	$$(INCLUDE) \
)

endef

endif # SYSTEM_MDDS

ifneq ($(SYSTEM_GLM),)

gb_LinkTarget__use_glm_headers :=
gb_ExternalProject__use_glm_headers :=

else

define gb_LinkTarget__use_glm_headers
$(call gb_LinkTarget_use_unpacked,$(1),glm)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,glm) \
	$$(INCLUDE) \
)

endef

define gb_ExternalProject__use_glm_headers
$(call gb_ExternalProject_get_preparation_target,$(1)) :| $(call gb_UnpackedTarball_get_final_target,glm)

endef

endif

ifeq (SANE,$(filter SANE,$(BUILD_TYPE)))

define gb_LinkTarget__use_sane_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(SRCDIR)/external/$(if $(filter WNT,$(OS)),twain,sane)/inc \
	$$(INCLUDE) \
)

endef

else

gb_LinkTarget__use_sane_headers :=

endif

ifneq ($(SYSTEM_BLUEZ),)

gb_LinkTarget__use_bluez_bluetooth_headers :=

else # !SYSTEM_BLUEZ

define gb_LinkTarget__use_bluez_bluetooth_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(SRCDIR)/external/bluez_bluetooth/inc \
	$$(INCLUDE) \
)

endef

endif # SYSTEM_BLUEZ

# External libraries

ifneq ($(SYSTEM_CPPUNIT),)

define gb_LinkTarget__use_cppunit
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(CPPUNIT_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
    $(CPPUNIT_LIBS) \
)

endef

else # !SYSTEM_CPPUNIT

define gb_LinkTarget__use_cppunit
$(call gb_LinkTarget_use_external_project,$(1),cppunit)

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,cppunit/include)\
	$$(INCLUDE) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,cppunit)/src/cppunit/$(if $(MSVC_USE_DEBUG_RUNTIME),DebugDll/cppunitd_dll.lib,ReleaseDll/cppunit_dll.lib) \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,cppunit)/src/cppunit/.libs -lcppunit \
)
endif

endef

endif # SYSTEM_CPPUNIT

ifneq ($(SYSTEM_GLEW),)

define gb_LinkTarget__use_glew
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(GLEW_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(GLEW_LIBS))

endef

gb_ExternalProject__use_glew :=

else # !SYSTEM_GLEW

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	glew \
))

define gb_LinkTarget__use_glew
$(call gb_LinkTarget_use_package,$(1),glew)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,glew/include) \
	$$(INCLUDE) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,glew)/lib/$(if $(MSVC_USE_DEBUG_RUNTIME),Debug/Win32/glew32d.lib,Release/Win32/glew32.lib) \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,glew)/lib/ -lGLEW \
)
endif

endef

define gb_ExternalProject__use_glew
$(call gb_ExternalProject_use_external_project,$(1),glew)

endef

endif # SYSTEM_GLEW

define gb_LinkTarget__use_iconv
$(call gb_LinkTarget_add_libs,$(1),-liconv)

endef

ifneq ($(SYSTEM_MARIADB),)

define gb_LinkTarget__use_mariadb
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(MARIADB_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(MARIADB_LIBS))

endef
gb_ExternalProject__use_mariadb :=

else # !SYSTEM_MARIADB

define gb_LinkTarget__use_mariadb
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(MARIADB_CFLAGS) \
)
$(call gb_LinkTarget_use_static_libraries,$(1),\
	mariadblib \
)

endef
define gb_ExternalProject__use_mariadb
$(call gb_ExternalProject_use_static_libraries,$(1),mariadblib)

endef

endif # SYSTEM_MARIADB


ifneq ($(SYSTEM_MARIADB),)

define gb_LinkTarget__use_mysql

$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_MARIADB \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(MARIADB_LIBS) \
)

$(call gb_LinkTarget_set_include,$(1),\
	$(MARIADB_CFLAGS) \
	$$(INCLUDE) \
)
endef

else

define gb_LinkTarget__use_mysql

$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
)

endef

endif

ifneq ($(SYSTEM_MYSQL_CPPCONN),)

define gb_LinkTarget__use_mysqlcppconn
$(call gb_LinkTarget_add_libs,$(1),\
	-lmysqlcppconn \
)

$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_MYSQL_CPPCONN \
)
endef

else

$(eval $(call gb_Helper_register_libraries,PLAINLIBS_NONE,\
	mysqlcppconn \
))

# note: this does not link mysqlcppconn, it is loaded via osl_loadModuleRelative
define gb_LinkTarget__use_mysqlcppconn

$(call gb_LinkTarget_use_unpacked,$(1),mysqlcppconn)

$(call gb_LinkTarget_add_defs,$(1),\
	-DCPPCONN_LIB_BUILD \
)

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,mysqlcppconn) \
	-I$(call gb_UnpackedTarball_get_dir,mysqlcppconn)/cppconn \
	$$(INCLUDE) \
)

endef

endif

ifneq ($(SYSTEM_ZLIB),)

define gb_LinkTarget__use_zlib
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_ZLIB \
)
$(call gb_LinkTarget_add_libs,$(1),-lz)

endef

# nothing on system
define gb_LinkTarget__use_zlib_x64

endef

gb_ExternalProject__use_zlib :=

else # !SYSTEM_ZLIB

define gb_LinkTarget__use_zlib_multiarch
$(if $(2),,$(call gb_Output_error,gb_LinkTarget__use_zlib_multiarch needs two arguments))

$(call gb_LinkTarget_set_include,$(1),\
	$(ZLIB_CFLAGS) \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	$(2) \
)

endef

define gb_LinkTarget__use_zlib
$(call gb_LinkTarget__use_zlib_multiarch,$(1),zlib)

endef

define gb_LinkTarget__use_zlib_x64
$(call gb_LinkTarget__use_zlib_multiarch,$(1),zlib_x64)

endef

define gb_ExternalProject__use_zlib
$(call gb_ExternalProject_use_static_libraries,$(1),zlib)

endef

endif # SYSTEM_ZLIB


ifneq ($(SYSTEM_JPEG),)

define gb_LinkTarget__use_jpeg
$(call gb_LinkTarget_add_libs,$(1),-ljpeg)
$(call gb_LinkTarget_set_ldflags,$(1),\
	$$(filter-out -L/usr/lib/jvm%,$$(T_LDFLAGS)) \
)

endef

else # !SYSTEM_JPEG

define gb_LinkTarget__use_jpeg
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,jpeg) \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	jpeg \
)

endef

endif # SYSTEM_JPEG

ifneq ($(SYSTEM_MYTHES),)

define gb_LinkTarget__use_mythes
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(MYTHES_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(MYTHES_LIBS))

endef

else # !SYSTEM_MYTHES

define gb_LinkTarget__use_mythes
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,mythes) \
	$$(INCLUDE) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_use_static_libraries,$(1),\
	mythes \
)
else
$(call gb_LinkTarget_add_libs,$(1),$(MYTHES_LIBS))
$(call gb_LinkTarget_use_external_project,$(1),mythes)
endif

endef

endif # SYSTEM_MYTHES


ifneq ($(SYSTEM_EXPAT),)

define gb_LinkTarget__use_expat_impl
$(if $(2),,$(error gb_LinkTarget__use_expat_impl needs additional parameter))

$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_EXPAT \
)

$(call gb_LinkTarget_add_libs,$(1),-lexpat)

endef

gb_ExternalProject__use_expat :=

else # !SYSTEM_EXPAT

define gb_LinkTarget__use_expat_impl
$(if $(2),,$(error gb_LinkTarget__use_expat_impl needs additional parameter))

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,expat)/lib \
    $$(INCLUDE) \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	$(2) \
)

endef

define gb_ExternalProject__use_expat
$(call gb_ExternalProject_use_static_libraries,$(1),expat)

endef

endif # SYSTEM_EXPAT

define gb_LinkTarget__use_expat
$(call gb_LinkTarget__use_expat_impl,$(1),expat)

endef

define gb_LinkTarget__use_expat_x64
$(call gb_LinkTarget__use_expat_impl,$(1),expat_x64)

endef

ifneq ($(SYSTEM_HYPH),)

define gb_LinkTarget__use_hyphen
$(call gb_LinkTarget_add_libs,$(1),$(HYPHEN_LIB))

endef

else # !SYSTEM_HYPH

define gb_LinkTarget__use_hyphen
$(call gb_LinkTarget_use_unpacked,$(1),hyphen)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,hyphen)\
	$$(INCLUDE) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_use_static_libraries,$(1),\
	hyphen \
)
else
$(call gb_LinkTarget_add_libs,$(1),$(HYPHEN_LIB))
$(call gb_LinkTarget_use_external_project,$(1),hyphen)
endif

endef

endif # SYSTEM_HYPH

ifneq ($(SYSTEM_HUNSPELL),)

define gb_LinkTarget__use_hunspell
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(HUNSPELL_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(HUNSPELL_LIBS))

endef

gb_ExternalProject__use_hunspell :=

else # !SYSTEM_HUNSPELL

define gb_LinkTarget__use_hunspell
$(call gb_LinkTarget_add_defs,$(1),\
	-DHUNSPELL_STATIC \
)
$(call gb_LinkTarget_use_unpacked,$(1),hunspell)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,hunspell/src/hunspell)\
	$$(INCLUDE) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_use_static_libraries,$(1),\
	hunspell \
)
else
$(call gb_LinkTarget_add_libs,$(1),$(HUNSPELL_LIBS))
$(call gb_LinkTarget_use_external_project,$(1),hunspell)
endif

endef

define gb_ExternalProject__use_hunspell
$(call gb_ExternalProject_use_external_project,$(1),hunspell)

endef

endif # SYSTEM_HUNSPELL


ifneq ($(SYSTEM_BOOST),)

define gb_LinkTarget__use_boostdatetime
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(BOOST_CPPFLAGS) \
)

$(call gb_LinkTarget_add_ldflags,$(1),\
	$(BOOST_LDFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(BOOST_DATE_TIME_LIB) \
)

endef

define gb_LinkTarget__use_boost_iostreams
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(BOOST_CPPFLAGS) \
)

$(call gb_LinkTarget_add_ldflags,$(1),\
	$(BOOST_LDFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(BOOST_IOSTREAMS_LIB) \
)

endef

gb_ExternalProject__use_boost_iostreams :=

define gb_LinkTarget__use_boost_system
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(BOOST_CPPFLAGS) \
)

$(call gb_LinkTarget_add_ldflags,$(1),\
	$(BOOST_LDFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(BOOST_SYSTEM_LIB) \
)

endef

gb_ExternalProject__use_boost_system :=

define gb_LinkTarget__use_boost_headers
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(BOOST_CPPFLAGS) \
)

endef

gb_ExternalProject__use_boost_headers:=

else # !SYSTEM_BOOST

ifeq ($(OS),WNT)
define gb_LinkTarget__use_boostthread
$(call gb_LinkTarget_add_defs,$(1),\
	-DBOOST_ALL_NO_LIB \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	boostthread \
)
endef
endif

define gb_LinkTarget__use_boostdatetime
$(call gb_LinkTarget_add_defs,$(1),\
	-DBOOST_ALL_NO_LIB \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	boostdatetime \
)

endef

define gb_LinkTarget__use_boost_iostreams
$(call gb_LinkTarget_add_defs,$(1),\
	-DBOOST_ALL_NO_LIB \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	boost_iostreams \
)

endef

define gb_ExternalProject__use_boost_iostreams
$(call gb_ExternalProject_use_static_libraries,$(1),boost_iostreams)
endef

define gb_LinkTarget__use_boost_system
$(call gb_LinkTarget_add_defs,$(1),\
	-DBOOST_ALL_NO_LIB \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	boost_system \
)

endef

define gb_ExternalProject__use_boost_system
$(call gb_ExternalProject_use_static_libraries,$(1),boost_system)
endef

define gb_LinkTarget__use_boost_headers
$(call gb_LinkTarget_use_unpacked,$(1),boost)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,boost) \
	$$(INCLUDE) \
)

endef

define gb_ExternalProject__use_boost_headers
$(call gb_ExternalProject_get_preparation_target,$(1)) :| $(call gb_UnpackedTarball_get_final_target,boost)

endef
endif # SYSTEM_BOOST


ifneq ($(SYSTEM_CMIS),)

define gb_LinkTarget__use_cmis
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(CMIS_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(CMIS_LIBS))

endef

else # !SYSTEM_CMIS

define gb_LinkTarget__use_cmis
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,cmis)/src \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_use_static_libraries,$(1),\
	cmislib \
)

endef

endif # SYSTEM_CMIS

ifeq ($(ENABLE_JAVA),TRUE)

ifeq ($(OS)$(COM),WNTGCC)

define gb_LinkTarget__use_jawt
$(call gb_LinkTarget_use_custom_headers,$(1),external/jawt)

$(call gb_LinkTarget_add_ldflags,$(1),\
	-L$(call gb_CustomTarget_get_workdir,external/jawt) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(JAWTLIB) \
)

endef

else # $(OS)$(COM) != WNTGCC

define gb_LinkTarget__use_jawt
$(call gb_LinkTarget_add_libs,$(1),\
	$(JAWTLIB) \
)

endef

endif # $(OS)$(COM) = WNTGCC

else # !ENABLE_JAVA

gb_LinkTarget__use_jawt :=

endif # ENABLE_JAVA

ifneq ($(SYSTEM_LIBATOMIC_OPS),)

define gb_LinkTarget__use_libatomic_ops
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBATOMIC_OPS_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1), $(LIBATOMIC_OPS_LIBS))

endef
gb_ExternalProject__use_libatomic_ops :=

else # !SYSTEM_LIBATOMIC_OPS

define gb_LinkTarget__use_libatomic_ops
$(call gb_LinkTarget_set_include,$(1),\
$(LIBATOMIC_OPS_CFLAGS) \
	$$(INCLUDE) \
	$(LIBATOMIC_OPS_CFLAGS) \
)
$(call gb_LinkTarget_use_external_project,$(1),\
	libatomic_ops \
)

$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,libatomic_ops)/src/lib -latomic_ops \
)

endef

define gb_ExternalProject__use_libatomic_ops
$(call gb_ExternalProject_use_external_project,$(1),libatomic_ops)

endef

endif # SYSTEM_LIBATOMIC_OPS


ifneq ($(SYSTEM_LIBEXTTEXTCAT),)

define gb_LinkTarget__use_libexttextcat
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBEXTTEXTCAT_CFLAGS) \
)
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_LIBEXTTEXTCAT \
)
$(call gb_LinkTarget_add_libs,$(1),$(LIBEXTTEXTCAT_LIBS))

endef

else # !SYSTEM_LIBEXTTEXTCAT

define gb_LinkTarget__use_libexttextcat
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,exttextcat/src) \
	$$(INCLUDE) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_use_static_libraries,$(1),\
	exttextcat \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,exttextcat)/src/.libs/libexttextcat-2.0.a\
)
$(call gb_LinkTarget_use_external_project,$(1),exttextcat)
endif


endef

endif # SYSTEM_LIBEXTTEXTCAT


ifneq ($(SYSTEM_LIBXML),)

define gb_LinkTarget__use_libxml2
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_LIBXML \
)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBXML_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(LIBXML_LIBS))

endef
gb_ExternalProject__use_libxml2:=

else # !SYSTEM_LIBXML

$(eval $(call gb_Helper_register_packages_for_install,ure,\
	xml2 \
))

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	xml2_win32 \
))

define gb_LinkTarget__use_libxml2
$(call gb_LinkTarget_use_package,$(1),xml2)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBXML_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(LIBXML_LIBS) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_use_external,$(1),icu_headers)
endif

endef
define gb_ExternalProject__use_libxml2
$(call gb_ExternalProject_use_package,$(1),xml2)

ifeq ($(COM),MSC)
$(call gb_ExternalProject_use_external_project,$(1),icu)
endif

endef

endif # SYSTEM_LIBXML


ifneq ($(SYSTEM_LIBXSLT),)

define gb_LinkTarget__use_libxslt
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBXSLT_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(LIBXSLT_LIBS))

endef

define gb_LinkTarget__use_libexslt
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBEXSLT_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),$(LIBEXSLT_LIBS))

endef

else # !SYSTEM_LIBXSLT

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	xslt \
))

define gb_LinkTarget__use_libxslt
$(call gb_LinkTarget_use_package,$(1),xslt)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	-I$(call gb_UnpackedTarball_get_dir,xslt) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,xslt)/win32/bin.msvc/libxslt.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,xslt)/libxslt/.libs -lxslt \
)
endif

endef

define gb_LinkTarget__use_libexslt
$(call gb_LinkTarget_use_package,$(1),xslt)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	-I$(call gb_UnpackedTarball_get_dir,xslt) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,xslt)/win32/bin.msvc/libexslt.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,xslt)/libexslt/.libs -lexslt \
)
endif

endef

endif # SYSTEM_LIBXSLT

ifeq ($(ENABLE_LIBLANGTAG),TRUE)

ifneq ($(SYSTEM_LIBLANGTAG),)

define gb_LinkTarget__use_liblangtag
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBLANGTAG_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),$(LIBLANGTAG_LIBS))

endef

else # !SYSTEM_LIBLANGTAG

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	liblangtag_data \
))

define gb_LinkTarget__use_liblangtag
$(call gb_LinkTarget_use_unpacked,$(1),langtag)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,langtag) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),$(LIBLANGTAG_LIBS))
$(call gb_LinkTarget_use_external_project,$(1),langtag)

endef

endif # SYSTEM_LIBLANGTAG

else

gb_LinkTarget__use_liblangtag :=

endif # ENABLE_LIBLANGTAG

gb_ExternalProject__use_apr :=

ifeq ($(WITH_WEBDAV),serf)

define gb_LinkTarget__use_apr
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(APR_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(APR_LIBS) \
	$(if $(filter $(OS),LINUX),-lpthread) \
	$(if $(filter $(OS),MACOSX),-liconv) \
)

ifeq ($(SYSTEM_APR),)
$(call gb_LinkTarget_use_system_win32_libs,$(1),\
	mswsock \
	rpcrt4 \
	shell32 \
)
$(call gb_LinkTarget_add_defs,$(1),\
	-DAPR_DECLARE_STATIC \
	-DAPU_DECLARE_STATIC \
)
$(call gb_LinkTarget_use_external_project,$(1),apr_util)
endif

endef

define gb_ExternalProject__use_apr
ifeq ($(SYSTEM_APR),)
$(call gb_ExternalProject_use_external_project,$(1),apr_util)
endif

endef

define gb_LinkTarget__use_serf
$(call gb_LinkTarget_set_include,$(1),\
	$(SERF_CFLAGS) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(SERF_LIBS) \
)

ifeq ($(SYSTEM_SERF),)
$(call gb_LinkTarget_use_external_project,$(1),serf)
endif

endef

else ifeq ($(WITH_WEBDAV),neon)

ifneq ($(SYSTEM_NEON),)

define gb_LinkTarget__use_neon
$(call gb_LinkTarget_add_defs,$(1),\
	-DNEON_VERSION=0x$(NEON_VERSION) \
	-DSYSTEM_NEON \
)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(NEON_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),$(NEON_LIBS))

endef

else # !SYSTEM_NEON

$(eval $(call gb_Helper_register_libraries_for_install,PLAINLIBS_OOO,ooo,\
	neon \
))

define gb_LinkTarget__use_neon
$(call gb_LinkTarget_use_unpacked,$(1),neon)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,neon/src) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_use_libraries,$(1),\
	neon \
)

endef

endif # SYSTEM_NEON

endif # WITH_WEBDAV

ifneq ($(SYSTEM_REDLAND),)

define gb_LinkTarget__use_librdf
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_REDLAND \
)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(REDLAND_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(REDLAND_LIBS))

endef

gb_LinkTarget__use_redland_headers:=

gb_LinkTarget__use_raptor_headers:=

gb_LinkTarget__use_rasqal_headers:=

else # !SYSTEM_REDLAND

define gb_LinkTarget__use_redland_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,redland)/src \
	$$(INCLUDE) \
)

endef

define gb_LinkTarget__use_raptor_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,raptor)/src \
	$$(INCLUDE) \
)

endef

define gb_LinkTarget__use_rasqal_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,rasqal)/src \
	$$(INCLUDE) \
)

endef

ifneq ($(OS),ANDROID)

ifeq ($(COM),MSC)
$(eval $(call gb_Helper_register_libraries_for_install,PLAINLIBS_OOO,ooo, \
	raptor2 \
	rasqal \
	rdf \
))
else
$(eval $(call gb_Helper_register_packages_for_install,ooo, \
	raptor \
	rasqal \
	redland \
))
endif

define gb_LinkTarget__use_librdf
ifeq ($(COM),MSC)
$(call gb_LinkTarget_use_libraries,$(1),\
	raptor2 \
	rdf \
)
else
$(call gb_LinkTarget_use_packages,$(1),raptor rasqal redland)

$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,raptor)/src/.libs -lraptor2 \
	-L$(call gb_UnpackedTarball_get_dir,redland)/src/.libs -lrdf \
)
endif

endef

else # ANDROID

define gb_LinkTarget__use_librdf
$(call gb_LinkTarget_use_packages,$(1),raptor rasqal redland)

endef

endif # ANDROID

endif # SYSTEM_REDLAND


ifneq ($(filter unx,$(GUIBASE))$(ENABLE_CAIRO_CANVAS),) # or

ifneq ($(SYSTEM_CAIRO),)

define gb_LinkTarget__use_cairo
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(CAIRO_CFLAGS) \
)
$(call gb_LinkTarget_use_external,$(1),freetype_headers)
$(call gb_LinkTarget_add_libs,$(1),$(CAIRO_LIBS))

endef

else # !SYSTEM_CAIRO

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	cairo \
	$(if $(filter $(OS),WNT),,pixman) \
))

define gb_LinkTarget__use_cairo
$(call gb_LinkTarget_use_package,$(1),cairo)
$(call gb_LinkTarget_use_package,$(1),pixman)
$(call gb_LinkTarget_use_external,$(1),freetype_headers)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,cairo) \
	-I$(call gb_UnpackedTarball_get_dir,cairo)/src \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,cairo)/src/.libs -lcairo \
	$(if $(filter-out MACOSX WNT,$(OS)), \
		-L$(call gb_UnpackedTarball_get_dir,pixman)/pixman/.libs -lpixman-1 \
	) \
)

endef

endif # SYSTEM_CAIRO

endif # CAIRO

ifneq ($(SYSTEM_FREETYPE),)

define gb_LinkTarget__use_freetype_headers
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(FREETYPE_CFLAGS) \
)

endef

define gb_LinkTarget__use_freetype
$(call gb_LinkTarget_use_external,$(1),freetype_headers)
$(call gb_LinkTarget_add_libs,$(1),$(FREETYPE_LIBS))

endef

gb_ExternalProject__use_freetype :=

else ifeq ($(OS),ANDROID)

define gb_LinkTarget__use_freetype_headers
$(call gb_LinkTarget_use_external_project,$(1),freetype)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,freetype)/include \
	$$(INCLUDE) \
)

endef

define gb_LinkTarget__use_freetype
$(call gb_LinkTarget_use_external,$(1),freetype_headers)
$(call gb_LinkTarget_add_libs,$(1),$(FREETYPE_LIBS))

endef

define gb_ExternalProject__use_freetype
$(call gb_ExternalProject_use_external_project,$(1),freetype)

endef

endif # SYSTEM_FREETYPE

ifneq ($(SYSTEM_FONTCONFIG),)

define gb_LinkTarget__use_fontconfig
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(FONTCONFIG_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),$(FONTCONFIG_LIBS))

endef

else ifeq ($(OS),ANDROID)

define gb_LinkTarget__use_fontconfig
$(call gb_LinkTarget_use_external_project,$(1),fontconfig)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,fontconfig) \
	$$(INCLUDE) \
)

endef

endif # SYSTEM_FONTCONFIG

ifeq ($(ENABLE_GRAPHITE),TRUE)

ifneq ($(SYSTEM_GRAPHITE),)

define gb_LinkTarget__use_graphite
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(GRAPHITE_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(GRAPHITE_LIBS))

endef

else # !SYSTEM_GRAPHITE

define gb_LinkTarget__use_graphite
$(call gb_LinkTarget_use_unpacked,$(1),graphite)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,graphite/include) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_use_static_libraries,$(1),\
    graphite \
)

endef

endif # SYSTEM_GRAPHITE

else # !ENABLE_GRAPHITE

gb_LinkTarget__use_graphite :=

endif # ENABLE_GRAPHITE

ifneq ($(SYSTEM_ICU),)

gb_LinkTarget__use_icu_headers:=
gb_ExternalProject__use_icu:=

define gb_LinkTarget__use_icudata
$(call gb_LinkTarget_add_libs,$(1),-licudata)

endef
define gb_LinkTarget__use_icui18n
$(call gb_LinkTarget_add_libs,$(1),-licui18n)

endef
define gb_LinkTarget__use_icuuc
$(call gb_LinkTarget_add_libs,$(1),-licuuc)

endef

else # !SYSTEM_ICU

$(eval $(call gb_Helper_register_packages_for_install,ure, \
	icu_ure \
))

$(eval $(call gb_Helper_register_packages_for_install,ooo, \
	icu \
))

ifeq ($(OS)$(DISABLE_DYNLOADING),ANDROID)
gb_ICU_suffix:=lo
else
gb_ICU_suffix:=
endif

define gb_LinkTarget__use_icu_headers
$(call gb_LinkTarget_use_unpacked,$(1),icu)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,icu)/source \
	-I$(call gb_UnpackedTarball_get_dir,icu)/source/i18n \
	-I$(call gb_UnpackedTarball_get_dir,icu)/source/common \
	$$(INCLUDE) \
)

endef

define gb_ExternalProject__use_icu
$(call gb_ExternalProject_use_package,$(1),icu)

endef

# icudata and icui18n is called icudt and icuin when built with MSVC :-/
define gb_LinkTarget__use_icudata
$(call gb_LinkTarget_use_package,$(1),icu)

ifeq ($(OS),WNT)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,icu)/source/lib/icudt$(if $(MSVC_USE_DEBUG_RUNTIME),d).lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,icu)/source/lib -licudata$(gb_ICU_suffix) \
)
endif

endef

define gb_LinkTarget__use_icui18n
$(call gb_LinkTarget_use_package,$(1),icu)

ifeq ($(OS),WNT)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,icu)/source/lib/icuin$(if $(MSVC_USE_DEBUG_RUNTIME),d).lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,icu)/source/lib -licui18n$(gb_ICU_suffix) \
)
endif

endef

define gb_LinkTarget__use_icuuc
$(call gb_LinkTarget_use_package,$(1),icu)

ifeq ($(OS),WNT)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,icu)/source/lib/icuuc$(if $(MSVC_USE_DEBUG_RUNTIME),d).lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,icu)/source/lib -licuuc$(gb_ICU_suffix) \
)
endif

endef

endif # SYSTEM_ICU

ifeq ($(ENABLE_HARFBUZZ),TRUE)
ifneq ($(SYSTEM_HARFBUZZ),)

define gb_LinkTarget__use_harfbuzz
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(HARFBUZZ_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(HARFBUZZ_LIBS))

endef

else # SYSTEM_HARFBUZZ != TRUE

define gb_LinkTarget__use_harfbuzz
$(call gb_LinkTarget_set_include,$(1),\
	$(HARFBUZZ_CFLAGS) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),$(HARFBUZZ_LIBS))
$(call gb_LinkTarget_use_external_project,$(1),harfbuzz)

endef

endif # SYSTEM_HARFBUZZ
else # ENABLE_HARFBUZZ != YES

gb_LinkTarget__use_harfbuzz :=

endif # ENABLE_HARFBUZZ

ifeq ($(DISABLE_OPENSSL),TRUE)

gb_ExternalProject__use_openssl:=
gb_LinkTarget__use_openssl_headers:=
gb_LinkTarget__use_openssl:=

else # !DISABLE_OPENSSL

ifneq ($(SYSTEM_OPENSSL),)

gb_LinkTarget__use_openssl_headers:=
gb_ExternalProject__use_openssl:=

define gb_LinkTarget__use_openssl
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(OPENSSL_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(OPENSSL_LIBS))

endef

else # !SYSTEM_OPENSSL

$(eval $(call gb_Helper_register_packages_for_install,ooo, \
	openssl \
))

define gb_ExternalProject__use_openssl
$(call gb_ExternalProject_use_package,$(1),openssl)

endef

define gb_LinkTarget__use_openssl_headers
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,openssl)/include \
	$$(INCLUDE) \
)

endef

define gb_LinkTarget__use_openssl
$(call gb_LinkTarget_use_package,$(1),openssl)
ifeq ($(OS),WNT)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,openssl)/out32dll/ssleay32.lib \
	$(call gb_UnpackedTarball_get_dir,openssl)/out32dll/libeay32.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,openssl)/libssl.a \
	$(call gb_UnpackedTarball_get_dir,openssl)/libcrypto.a \
)
$(call gb_LinkTarget_use_external_project,$(1),openssl)
ifeq ($(OS),SOLARIS)
$(call gb_LinkTarget_add_libs,$(1),\
	-lnsl \
	-lsocket \
)
endif
endif

endef

endif # SYSTEM_OPENSSL
endif # DISABLE_OPENSSL


ifeq ($(DISABLE_OPENSSL),TRUE)

define gb_LinkTarget__use_gnutls
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(GNUTLS_CFLAGS) \
)

$(call gb_LinkTarget_add_defs,$(1),\
    -DDISABLE_OPENSSL \
)

$(call gb_LinkTarget_add_libs,$(1),$(GNUTLS_LIBS))

endef

define gb_LinkTarget__use_libgcrypt
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBGCRYPT_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),$(LIBGCRYPT_LIBS))

endef

else # !DISABLE_OPENSSL

gb_LinkTarget__use_gnutls:=
gb_LinkTarget__use_libgcrypt:=

endif # DISABLE_OPENSSL


ifneq ($(SYSTEM_CDR),)

define gb_LinkTarget__use_cdr
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(CDR_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(CDR_LIBS))

endef

else # !SYSTEM_CDR

define gb_LinkTarget__use_cdr
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libcdr)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libcdr)/src/lib/.libs/libcdr-0.1$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libcdr)
endef

endif # SYSTEM_CDR


ifneq ($(SYSTEM_EBOOK),)

define gb_LinkTarget__use_ebook
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(EBOOK_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(EBOOK_LIBS))

endef

gb_ExternalProject__use_ebook :=

else # !SYSTEM_EBOOK

define gb_LinkTarget__use_ebook
$(call gb_LinkTarget_set_include,$(1),\
	-I${WORKDIR}/UnpackedTarball/libebook/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libebook)/src/lib/.libs/libe-book-0.1$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libebook)

endef

define gb_ExternalProject__use_ebook
$(call gb_ExternalProject_use_external_project,$(1),libebook)

endef

endif # SYSTEM_EBOOK


ifneq ($(SYSTEM_ETONYEK),)

define gb_LinkTarget__use_etonyek
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(ETONYEK_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(ETONYEK_LIBS))

endef

gb_ExternalProject__use_etonyek :=

else # !SYSTEM_ETONYEK

define gb_LinkTarget__use_etonyek
$(call gb_LinkTarget_set_include,$(1),\
	-I${WORKDIR}/UnpackedTarball/libetonyek/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libetonyek)/src/lib/.libs/libetonyek-0.1$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libetonyek)

endef

define gb_ExternalProject__use_etonyek
$(call gb_ExternalProject_use_external_project,$(1),libetonyek)

endef

endif # SYSTEM_ETONYEK


ifneq ($(SYSTEM_FREEHAND),)

define gb_LinkTarget__use_freehand
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(FREEHAND_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(FREEHAND_LIBS))

endef

gb_ExternalProject__use_freehand :=

else # !SYSTEM_FREEHAND

define gb_LinkTarget__use_freehand
$(call gb_LinkTarget_set_include,$(1),\
	-I${WORKDIR}/UnpackedTarball/libfreehand/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libfreehand)/src/lib/.libs/libfreehand-0.1$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libfreehand)

endef

define gb_ExternalProject__use_freehand
$(call gb_ExternalProject_use_external_project,$(1),libfreehand)

endef

endif # SYSTEM_FREEHAND


ifneq ($(SYSTEM_ODFGEN),)

define gb_LinkTarget__use_odfgen
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(ODFGEN_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(ODFGEN_LIBS))

endef

else # !SYSTEM_ODFGEN

ifeq ($(COM),MSC)

$(eval $(call gb_Helper_register_libraries_for_install,PLAINLIBS_OOO,ooo,\
	odfgen \
))

define gb_LinkTarget__use_odfgen
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libodfgen)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_use_libraries,$(1),\
	odfgen \
)

endef

else # !MSC

$(eval $(call gb_Helper_register_packages_for_install,ooo, \
	libodfgen \
))

define gb_LinkTarget__use_odfgen
$(call gb_LinkTarget_use_package,$(1),libodfgen)

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libodfgen)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,libodfgen)/src/.libs -lodfgen-0.1 \
)

endef

endif

endif # SYSTEM_ODFGEN


ifneq ($(SYSTEM_REVENGE),)

define gb_LinkTarget__use_revenge
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(REVENGE_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(REVENGE_LIBS))

endef

gb_ExternalProject__use_revenge :=

else # !SYSTEM_REVENGE

ifeq ($(COM),MSC)

$(eval $(call gb_Helper_register_libraries_for_install,PLAINLIBS_OOO,ooo,\
	revenge \
))

define gb_LinkTarget__use_revenge
$(call gb_LinkTarget_set_include,$(1),\
	$(REVENGE_CFLAGS) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_use_libraries,$(1),\
	revenge \
)

endef

define gb_ExternalProject__use_revenge
$(call gb_ExternalProject_get_preparation_target,$(1)) : $(call gb_Library_get_target,revenge)

endef

else # !MSC

$(eval $(call gb_Helper_register_packages_for_install,ooo, \
	librevenge \
))

define gb_LinkTarget__use_revenge
$(call gb_LinkTarget_use_package,$(1),librevenge)

$(call gb_LinkTarget_set_include,$(1),\
	$(REVENGE_CFLAGS) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(REVENGE_LIBS) \
)
endef

define gb_ExternalProject__use_revenge
$(call gb_ExternalProject_use_package,$(1),librevenge)

endef

endif # MSC

endif # SYSTEM_REVENGE


ifneq ($(SYSTEM_ABW),)

define gb_LinkTarget__use_abw
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(ABW_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(ABW_LIBS))

endef
gb_ExternalProject__use_abw :=

else # !SYSTEM_ABW

define gb_LinkTarget__use_abw
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libabw)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libabw)/src/lib/.libs/libabw-0.1$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libabw)

endef
define gb_ExternalProject__use_abw
$(call gb_ExternalProject_use_external_project,$(1),libabw)

endef

endif # SYSTEM_ABW


ifneq ($(SYSTEM_MSPUB),)

define gb_LinkTarget__use_mspub
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(MSPUB_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(MSPUB_LIBS))

endef

else # !SYSTEM_MSPUB

define gb_LinkTarget__use_mspub
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libmspub)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libmspub)/src/lib/.libs/libmspub-0.1$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libmspub)

endef

endif # SYSTEM_MSPUB


ifneq ($(SYSTEM_PAGEMAKER),)

define gb_LinkTarget__use_pagemaker
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(PAGEMAKER_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(PAGEMAKER_LIBS))

endef
gb_ExternalProject__use_pagemaker :=

else # !SYSTEM_PAGEMAKER

define gb_LinkTarget__use_pagemaker
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libpagemaker)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libpagemaker)/src/lib/.libs/libpagemaker-0.0$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libpagemaker)

endef
define gb_ExternalProject__use_pagemaker
$(call gb_ExternalProject_use_external_project,$(1),libpagemaker)

endef

endif # SYSTEM_PAGEMAKER


ifneq ($(SYSTEM_VISIO),)

define gb_LinkTarget__use_visio
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(VISIO_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(VISIO_LIBS))

endef

else # !SYSTEM_VISIO

define gb_LinkTarget__use_visio
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libvisio)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libvisio)/src/lib/.libs/libvisio-0.1$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libvisio)

endef

endif # SYSTEM_VISIO


ifneq ($(SYSTEM_WPD),)

define gb_LinkTarget__use_wpd
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(WPD_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(WPD_LIBS))

endef
gb_ExternalProject__use_wpd :=

else # !SYSTEM_WPD

define gb_LinkTarget__use_wpd
$(call gb_LinkTarget_set_include,$(1),\
	$(WPD_CFLAGS) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libwpd)/src/lib/.libs/libwpd-0.10$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libwpd)

endef
define gb_ExternalProject__use_wpd
$(call gb_ExternalProject_use_external_project,$(1),libwpd)

endef

endif # SYSTEM_WPD


ifneq ($(SYSTEM_WPG),)

define gb_LinkTarget__use_wpg
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(WPG_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(WPG_LIBS))

endef
gb_ExternalProject__use_wpg :=

else # !SYSTEM_WPG

define gb_LinkTarget__use_wpg
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libwpg)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libwpg)/src/lib/.libs/libwpg-0.3$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libwpg)

endef
define gb_ExternalProject__use_wpg
$(call gb_ExternalProject_use_external_project,$(1),libwpg)

endef

endif # SYSTEM_WPG


ifneq ($(SYSTEM_WPS),)

define gb_LinkTarget__use_wps
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(WPS_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(WPS_LIBS))

endef
gb_ExternalProject__use_wps :=

else # !SYSTEM_WPS

ifeq ($(COM),MSC)

$(eval $(call gb_Helper_register_libraries_for_install,PLAINLIBS_OOO,ooo,\
	wps \
))

define gb_LinkTarget__use_wps
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libwps)/inc \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_use_libraries,$(1),\
	wps \
)

endef

else # !MSC

$(eval $(call gb_Helper_register_packages_for_install,ooo, \
	libwps \
))

define gb_LinkTarget__use_wps
$(call gb_LinkTarget_use_package,$(1),libwps)

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libwps)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,libwps)/src/lib/.libs -lwps-0.3 \
)

endef

endif # MSC

endif # SYSTEM_WPS


ifneq ($(SYSTEM_MWAW),)

define gb_LinkTarget__use_mwaw
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(MWAW_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(MWAW_LIBS))

endef

else # !SYSTEM_MWAW

ifeq ($(COM),MSC)

$(eval $(call gb_Helper_register_libraries_for_install,PLAINLIBS_OOO,ooo,\
	mwaw \
))

define gb_LinkTarget__use_mwaw
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libmwaw)/inc \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_use_libraries,$(1),\
	mwaw \
)

endef

else # !MSC

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	libmwaw \
))

define gb_LinkTarget__use_mwaw
$(call gb_LinkTarget_use_package,$(1),libmwaw)

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libmwaw)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,libmwaw)/src/lib/.libs -lmwaw-0.3 \
)

endef

endif # MSC

endif # SYSTEM_MWAW


ifneq ($(SYSTEM_LCMS2),)

define gb_LinkTarget__use_lcms2
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LCMS2_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(LCMS2_LIBS))

endef

gb_ExternalProject__use_lcms2 :=

else # !SYSTEM_LCMS2

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	lcms2 \
))

define gb_ExternalProject__use_lcms2
$(call gb_ExternalProject_use_package,$(1),lcms2)

endef

ifeq ($(OS),ANDROID)

define gb_LinkTarget__use_lcms2
$(call gb_LinkTarget_use_package,$(1),lcms2)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,lcms2/include) \
	$$(INCLUDE) \
)

endef

else

define gb_LinkTarget__use_lcms2
$(call gb_LinkTarget_use_package,$(1),lcms2)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,lcms2/include) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),$(LCMS2_LIBS))

endef

endif # ANDROID
endif # SYSTEM_LCMS2

ifneq ($(ENABLE_LPSOLVE),)

ifneq ($(SYSTEM_LPSOLVE),)

define gb_LinkTarget__use_lpsolve
$(call gb_LinkTarget_add_libs,$(1),-llpsolve55)
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_LPSOLVE \
)

endef

else # !SYSTEM_LPSOLVE

define gb_LinkTarget__use_lpsolve
$(call gb_LinkTarget_use_unpacked,$(1),lpsolve)
ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,lpsolve)/lpsolve55/lpsolve55.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,lpsolve)/lpsolve55 -llpsolve55 \
)
endif
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,lpsolve) \
	$$(INCLUDE) \
)

endef

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	lpsolve \
))

endif # SYSTEM_LPSOLVE

else

gb_LinkTarget__use_lpsolve :=

endif # ENABLE_LPSOLVE

ifneq ($(ENABLE_COINMP),)

ifneq ($(SYSTEM_COINMP),TRUE)

define gb_LinkTarget__use_coinmp
$(call gb_LinkTarget_use_package,$(1),coinmp)
ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,coinmp)/CoinMP/MSVisualStudio/v9/$(if $(MSVC_USE_DEBUG_RUNTIME),Debug,Release)/CoinMP.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,coinmp)/Cbc/src/.libs -lCbc -lCbcSolver \
	-L$(call gb_UnpackedTarball_get_dir,coinmp)/Cgl/src/.libs -lCgl \
	-L$(call gb_UnpackedTarball_get_dir,coinmp)/Clp/src/.libs -lClp \
	-L$(call gb_UnpackedTarball_get_dir,coinmp)/Clp/src/OsiClp/.libs -lOsiClp \
	-L$(call gb_UnpackedTarball_get_dir,coinmp)/CoinMP/src/.libs -lCoinMP \
	-L$(call gb_UnpackedTarball_get_dir,coinmp)/CoinUtils/src/.libs -lCoinUtils \
	-L$(call gb_UnpackedTarball_get_dir,coinmp)/Osi/src/Osi/.libs -lOsi \
)
endif
$(call gb_LinkTarget_set_include,$(1),\
   -I$(call gb_UnpackedTarball_get_dir,coinmp)/CoinMP/src \
   -I$(call gb_UnpackedTarball_get_dir,coinmp)/CoinUtils/src \
   $$(INCLUDE) \
)

endef

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	coinmp \
))

else # SYSTEM_COINMP

define gb_LinkTarget__use_coinmp
$(call gb_LinkTarget_set_include,$(1),\
       $$(INCLUDE) \
       $(COINMP_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(COINMP_LIBS))

endef

endif

else

gb_LinkTarget__use_coinmp :=

endif # ENABLE_COINMP

ifeq ($(ENABLE_GIO),TRUE)

define gb_LinkTarget__use_gio
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(GIO_CFLAGS) \
)

$(call gb_LinkTarget_add_defs,$(1),\
	-DENABLE_GIO \
)

$(call gb_LinkTarget_add_libs,$(1),$(GIO_LIBS))

endef

else # ENABLE_GIO

define gb_LinkTarget__use_gio

endef

endif # ENABLE_GIO

ifeq ($(ENABLE_AVAHI),TRUE)

define gb_LinkTarget__use_avahi
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(AVAHI_CFLAGS) \
)

$(call gb_LinkTarget_add_defs,$(1),\
    -DENABLE_AVAHI \
)

$(call gb_LinkTarget_add_libs,$(1),$(AVAHI_LIBS))

endef

else # ENABLE_AVAHI

gb_LinkTarget__use_avahi :=

endif # ENABLE_AVAHI


define gb_LinkTarget__use_gtk
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(GTK_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),$(GTK_LIBS))

ifeq ($(ENABLE_GTK_PRINT),TRUE)

$(call gb_LinkTarget_add_defs,$(1),-DENABLE_GTK_PRINT)

$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(GTK_PRINT_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),$(GTK_PRINT_LIBS))

endif

endef

define gb_LinkTarget__use_gthread
$(call gb_LinkTarget_add_libs,$(1),$(GTHREAD_LIBS))
endef

ifeq ($(ENABLE_CUPS),TRUE)

define gb_LinkTarget__use_cups
$(call gb_LinkTarget_add_defs,$(1),\
    -DENABLE_CUPS \
)

$(call gb_LinkTarget_add_libs,$(1),\
	-lcups \
)

endef

else # ENABLE_CUPS

define gb_LinkTarget__use_cups

endef

endif # ENABLE_DBUS

ifeq ($(ENABLE_DBUS),TRUE)

define gb_LinkTarget__use_dbus
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(DBUS_CFLAGS) \
)

$(call gb_LinkTarget_add_defs,$(1),\
    -DENABLE_DBUS \
)

ifeq ($(ENABLE_PACKAGEKIT),TRUE)
$(call gb_LinkTarget_add_defs,$(1),\
    -DENABLE_PACKAGEKIT \
)
endif # ENABLE_PACKAGEKIT

$(call gb_LinkTarget_add_libs,$(1),\
	$(DBUS_LIBS) \
)

endef

else # ENABLE_DBUS

define gb_LinkTarget__use_dbus

endef

endif # ENABLE_DBUS


define gb_LinkTarget__use_dbusmenugtk
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(DBUSMENUGTK_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),$(DBUSMENUGTK_LIBS))

endef


ifeq ($(ENABLE_TELEPATHY),TRUE)

define gb_LinkTarget__use_telepathy
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(TELEPATHY_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(TELEPATHY_LIBS) \
)

endef

else # !ENABLE_TELEPATHY

gb_LinkTarget__use_telepathy :=

endif # ENABLE_TELEPATHY

ifneq ($(SYSTEM_LIBPNG),)

define gb_LinkTarget__use_png
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBPNG_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(LIBPNG_LIBS) \
)

endef

else # !SYSTEM_LIBPNG

define gb_LinkTarget__use_png
$(call gb_LinkTarget_set_include,$(1),\
	$(LIBPNG_CFLAGS) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_use_static_libraries,$(1),\
	png \
)
$(call gb_LinkTarget__use_zlib,$(1))

endef

endif # !SYSTEM_LIBPNG


ifneq ($(SYSTEM_CURL),)

define gb_LinkTarget__use_curl
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_CURL \
)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(CURL_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(CURL_LIBS))

endef

else # !SYSTEM_CURL

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	curl \
))

define gb_LinkTarget__use_curl
$(call gb_LinkTarget_use_package,$(1),curl)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,curl/include) \
	$$(INCLUDE) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,curl)/lib/$(if $(MSVC_USE_DEBUG_RUNTIME),debug-dll,release-dll)/libcurl$(if $(MSVC_USE_DEBUG_RUNTIME),d)_imp.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,curl)/lib/.libs -lcurl \
)
endif

endef

endif # SYSTEM_CURL

ifeq ($(ENABLE_VALGRIND),TRUE)

define gb_LinkTarget__use_valgrind
$(call gb_LinkTarget_add_defs,$(1),\
	-DHAVE_VALGRIND_HEADERS \
)

$(call gb_LinkTarget_set_include,$(1),\
    $$(INCLUDE) \
    $(VALGRIND_CFLAGS) \
)

endef

else # !ENABLE_VALGRIND

define gb_LinkTarget__use_valgrind

endef

endif # ENABLE_VALGRIND

ifneq ($(SYSTEM_POPPLER),)

define gb_LinkTarget__use_poppler
$(call gb_LinkTarget_set_include,$(1),\
	$(POPPLER_CFLAGS) \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(POPPLER_LIBS) \
)

endef

else # !SYSTEM_POPPLER

define gb_LinkTarget__use_poppler
$(call gb_LinkTarget_use_external_project,$(1),poppler)

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,poppler) \
	-I$(call gb_UnpackedTarball_get_dir,poppler)/poppler \
	-I$(call gb_UnpackedTarball_get_dir,poppler)/goo \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,poppler)/fofi/.libs/libfofi$(gb_StaticLibrary_PLAINEXT) \
	$(call gb_UnpackedTarball_get_dir,poppler)/goo/.libs/libgoo$(gb_StaticLibrary_PLAINEXT) \
	$(call gb_UnpackedTarball_get_dir,poppler)/poppler/.libs/libpoppler$(gb_StaticLibrary_PLAINEXT) \
)

ifeq ($(OS),MACOSX)
$(call gb_LinkTarget_add_libs,$(1),\
	-lobjc \
)
else ifeq ($(OS),WNT)
$(call gb_LinkTarget_use_system_win32_libs,$(1),\
	advapi32 \
	gdi32 \
)
endif

endef

endif # SYSTEM_POPPLER


ifneq ($(SYSTEM_CLUCENE),)

define gb_LinkTarget__use_clucene
$(call gb_LinkTarget_add_defs,$(1),\
	$(filter-out -I% -isystem%,$(subst -isystem /,-isystem/,$(CLUCENE_CFLAGS))) \
)

$(call gb_LinkTarget_set_include,$(1),\
	$(subst -isystem/,-isystem /,$(filter -I% -isystem%,$(subst -isystem /,-isystem/,$(CLUCENE_CFLAGS)))) \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(CLUCENE_LIBS) \
)

endef

else # !SYSTEM_CLUCENE

define gb_LinkTarget__use_clucene
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,clucene)/src/core \
	-I$(call gb_UnpackedTarball_get_dir,clucene)/src/shared \
	-I$(call gb_UnpackedTarball_get_dir,clucene)/src/contribs-lib \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_use_libraries,$(1),\
	clucene \
)

endef

$(eval $(call gb_Helper_register_libraries_for_install,PLAINLIBS_OOO,ooo,\
	clucene \
))

endif # SYSTEM_CLUCENE

define gb_LinkTarget__use_gobject
$(call gb_LinkTarget_add_libs,$(1),\
       $(GOBJECT_LIBS) \
)

$(call gb_LinkTarget_set_include,$(1),\
       $$(INCLUDE) \
       $(GOBJECT_CFLAGS) \
)
endef

ifneq ($(SYSTEM_HSQLDB),)

define gb_LinkTarget__use_hsqldb

$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_HSQLDB \
	-DHSQLDB_JAR=\""file://$(HSQLDB_JAR)"\" \
)

endef

else # !SYSTEM_HSQLDB

define gb_LinkTarget__use_hsqldb

endef

endif # SYSTEM_HSQLDB


ifneq ($(SYSTEM_OPENLDAP),)

define gb_LinkTarget__use_openldap

$(call gb_LinkTarget_add_libs,$(1),\
	-lldap \
	-llber \
)

endef

gb_ExternalProject__use_openldap :=

else # !SYSTEM_OPENLDAP

define gb_LinkTarget__use_openldap
$(call gb_LinkTarget_use_unpacked,$(1),openldap)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,openldap/include) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_use_external_project,$(1),openldap)
$(call gb_LinkTarget_add_libs,$(1), \
	$(call gb_UnpackedTarball_get_dir,openldap)/libraries/libldap/.libs/libldap.a \
	$(call gb_UnpackedTarball_get_dir,openldap)/libraries/liblber/.libs/liblber.a \
)

endef

define gb_ExternalProject__use_openldap
$(call gb_ExternalProject_use_external_project,$(1),openldap)

endef

endif # SYSTEM_OPENLDAP


ifeq ($(ENABLE_FIREBIRD_SDBC),TRUE)

ifneq ($(SYSTEM_FIREBIRD),)

define gb_LinkTarget__use_libfbembed
$(call gb_LinkTarget_set_include,$(1),\
	$(FIREBIRD_CFLAGS) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),$(FIREBIRD_LIBS))

endef

# gb_LinkTarget__use_atomic_ops :=
# gb_LinkTarget__use_tommath :=

else # !SYSTEM_FIREBIRD

#$(call gb_LinkTarget__use_libatomic_ops,$(1))

define gb_LinkTarget__use_libfbembed
$(call gb_LinkTarget_use_package,$(1),firebird)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	-I$(call gb_UnpackedTarball_get_dir,firebird)/gen/firebird/include \
)
ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,firebird)/gen/firebird/bin/ifbembed.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,firebird)/gen/firebird/lib -lfbembed \
)
endif

endef

# define gb_LinkTarget__use_tommath
# $(call gb_LinkTarget_set_include,$(1),\
# 	$(TOMMATH_CFLAGS) \
# 	$$(INCLUDE) \
# )

# $(call gb_LinkTarget_add_libs,$(1),$(TOMMATH_LIBS))

# endef

endif # SYSTEM_FIREBIRD

else # !ENABLE_FIREBIRD_SDBC

gb_LinkTarget__use_firebird :=
# gb_LinkTarget__use_atomic_ops :=
# gb_LinkTarget__use_tommath :=

endif # ENABLE_FIREBIRD_SDBC


ifneq ($(SYSTEM_POSTGRESQL),)

define gb_LinkTarget__use_postgresql

$(call gb_LinkTarget_set_include,$(1),\
	$(POSTGRESQL_INC) \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	-lpq \
)

$(call gb_LinkTarget_add_ldflags,$(1),\
	$(POSTGRESQL_LIB) \
)

endef

else # !SYSTEM_POSTGRESQL

define gb_LinkTarget__use_postgresql

$(call gb_LinkTarget_use_external_project,$(1),postgresql)

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,postgresql)/src/include \
	-I$(call gb_UnpackedTarball_get_dir,postgresql)/src/interfaces/libpq \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,postgresql)/src/interfaces/libpq/libpq$(gb_StaticLibrary_PLAINEXT) \
)

ifeq ($(OS)$(COM),WNTMSC)
$(call gb_LinkTarget_use_external,$(1),openssl)

$(call gb_LinkTarget_use_system_win32_libs,$(1),\
	secur32 \
	ws2_32 \
)

endif

endef

endif # SYSTEM_POSTGRESQL

ifneq ($(WITH_MOZAB4WIN),)

$(eval $(call gb_Helper_register_packages_for_install,ooo,\
	moz_runtime \
))

define gb_LinkTarget__use_mozilla

$(call gb_LinkTarget_use_unpacked,$(1),moz_lib)
$(call gb_LinkTarget_use_unpacked,$(1),moz_inc)

$(call gb_LinkTarget_add_defs,$(1),\
	-DMOZILLA_INTERNAL_API \
)

$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,moz_inc) \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/addrbook \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/content \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/embed_base \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/intl \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/mime \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/mozldap \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/msgbase \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/necko \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/pref \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/profile \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/rdf \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/string \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/uconv \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/xpcom \
	-I$(call gb_UnpackedTarball_get_dir,moz_inc)/xpcom_obsolete \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib/nspr4.lib \
	$(call gb_UnpackedTarball_get_dir,moz_lib)/xpcom.lib \
	$(call gb_UnpackedTarball_get_dir,moz_lib)/xpcom_core.lib \
)

$(call gb_LinkTarget_add_defs,$(1),\
	-DMOZILLA_CLIENT \
	-DMOZ_REFLOW_PERF \
	-DMOZ_REFLOW_PERF_DSP \
	-DMOZ_XUL \
	-DOJI \
	-DWIN32 \
	-DXP_PC \
	-DXP_WIN \
	-DXP_WIN32 \
	-D_WINDOWS \
)

ifeq ($(COM),GCC)

$(call gb_LinkTarget_add_cxxflags,$(1),\
	-Wall \
	-Wcast-align \
	-Wconversion \
	-Wno-long-long \
	-Woverloaded-virtual \
	-Wpointer-arith \
	-Wsynth \
	-fno-rtti \
)

else

ifneq ($(DBG_LEVEL),0)
$(call gb_LinkTarget_add_defs,$(1),\
	-D_STL_NOFORCE_MANIFEST \
)
endif

$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,moz_lib)/embed_base_s.lib \
	$(call gb_UnpackedTarball_get_dir,moz_lib)/mozreg_s.lib \
)

endif # !GCC

endef

endif # WITH_MOZAB4WIN

ifeq ($(ENABLE_KDE),TRUE)

define gb_LinkTarget__use_kde
$(call gb_LinkTarget_set_include,$(1),\
	$(subst -isystem/,-isystem /,$(filter -I% -isystem%,$(subst -isystem /,-isystem/,$(KDE_CFLAGS)))) \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_defs,$(1),\
	$(filter-out -I% -isystem%,$(subst -isystem /,-isystem/,$(KDE_CFLAGS))) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(KDE_LIBS) \
)

ifeq ($(COM),GCC)
$(call gb_LinkTarget_add_cxxflags,$(1),\
	-Wno-shadow \
)
endif

endef

else # !ENABLE_KDE

define gb_LinkTarget__use_kde

endef

endif # ENABLE_KDE


ifeq ($(ENABLE_KDE4),TRUE)

define gb_LinkTarget__use_kde4
$(call gb_LinkTarget_set_include,$(1),\
	$(subst -isystem/,-isystem /,$(filter -I% -isystem%,$(subst -isystem /,-isystem/,$(KDE4_CFLAGS)))) \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_defs,$(1),\
	$(filter-out -I% -isystem%,$(subst -isystem /,-isystem/,$(KDE4_CFLAGS))) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(KDE4_LIBS) \
)

ifeq ($(COM),GCC)
$(call gb_LinkTarget_add_cxxflags,$(1),\
	-Wno-shadow \
)
endif

endef

else # !ENABLE_KDE4

define gb_LinkTarget__use_kde4

endef

endif # ENABLE_KDE4


ifeq ($(ENABLE_TDE),TRUE)

define gb_LinkTarget__use_tde
$(call gb_LinkTarget_add_libs,$(1),\
	$(TDE_LIBS) \
)

$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(TDE_CFLAGS) \
)
endef

else # ! ENABLE_TDE

define gb_LinkTarget__use_tde

endef

endif # ENABLE_TDE


ifeq ($(ENABLE_GCONF),TRUE)

define gb_LinkTarget__use_gconf
$(call gb_LinkTarget_set_include,$(1),\
	$(subst -isystem/,-isystem /,$(filter -I% -isystem%,$(subst -isystem /,-isystem/,$(GCONF_CFLAGS)))) \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_add_defs,$(1),\
	$(filter-out -I% -isystem%,$(subst -isystem /,-isystem/,$(GCONF_CFLAGS))) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(GCONF_LIBS) \
)

endef

else # !ENABLE_GCONF

define gb_LinkTarget__use_gconf

endef

endif # ENABLE_GCONF


# PYTHON
# extra python_headers external because pyuno wrapper must not link python
ifneq ($(SYSTEM_PYTHON),)

define gb_LinkTarget__use_python_headers
$(call gb_LinkTarget_add_defs,$(1),\
	$(filter-out -I% -isystem%,$(subst -isystem /,-isystem/,$(PYHTON_CFLAGS)))) \
)

$(call gb_LinkTarget_set_include,$(1),\
	$(subst -isystem/,-isystem /,$(filter -I% -isystem%,$(subst -isystem /,-isystem/,$(PYTHON_CFLAGS)))) \
	$$(INCLUDE) \
)

endef

define gb_LinkTarget__use_python
$(call gb_LinkTarget__use_python_headers,$(1))

$(call gb_LinkTarget_add_libs,$(1),\
	$(PYTHON_LIBS) \
)

endef

else # !SYSTEM_PYTHON

$(eval $(call gb_Helper_register_packages_for_install,python,\
	python3 \
))

# depend on external project because on MACOSX the Package is disabled...
define gb_LinkTarget__use_python_headers
$(call gb_LinkTarget_use_external_project,$(1),python3)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,python3) \
	-I$(call gb_UnpackedTarball_get_dir,python3)/PC \
	-I$(call gb_UnpackedTarball_get_dir,python3)/Include \
	$$(INCLUDE) \
)

endef

define gb_LinkTarget__use_python
$(call gb_LinkTarget__use_python_headers,$(1))

ifeq ($(OS),WNT)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,python3)/PCbuild$(if $(filter X86_64,$(CPUNAME)),/amd64)/python$(PYTHON_VERSION_MAJOR)$(PYTHON_VERSION_MINOR)$(if $(MSVC_USE_DEBUG_RUNTIME),_d).lib \
)
else ifeq ($(OS),MACOSX)
$(call gb_LinkTarget_add_libs,$(1),\
	-F$(call gb_UnpackedTarball_get_dir,python3)/python-inst/@__________________________________________________OOO -framework LibreOfficePython \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,python3) \
	-lpython$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)m \
)
endif

endef

endif # SYSTEM_PYTHON

# ORCUS
ifeq ($(ENABLE_ORCUS),TRUE)

ifneq ($(SYSTEM_LIBORCUS),)

define gb_LinkTarget__use_orcus
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(ORCUS_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(ORCUS_LIBS))
endef

define gb_LinkTarget__use_orcus-parser

endef

else # !SYSTEM_LIBORCUS

define gb_LinkTarget__use_orcus
$(call gb_LinkTarget_use_external_project,$(1),liborcus)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,liborcus/include) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,liborcus)/src/liborcus/.libs/liborcus-0.8$(gb_StaticLibrary_PLAINEXT) \
)

$(if $(SYSTEM_BOOST), \
    $(call gb_LinkTarget_add_ldflags,$(1),$(BOOST_LDFLAGS)) \
    $(call gb_LinkTarget_add_libs,$(1),$(BOOST_SYSTEM_LIB)) \
)

endef

define gb_LinkTarget__use_orcus-parser
$(call gb_LinkTarget_use_external_project,$(1),liborcus)
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,liborcus/include) \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,liborcus)/src/parser/.libs/liborcus-parser-0.8$(gb_StaticLibrary_PLAINEXT) \
)

endef

endif # SYSTEM_LIBORCUS

else # ENABLE_ORCUS != TRUE

gb_LinkTarget__use_orcus :=
gb_LinkTarget__use_orcus-parser :=

endif

ifeq ($(ENABLE_EOT),TRUE)

ifneq ($(SYSTEM_LIBEOT),)

define gb_LinkTarget__use_libeot
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
    $(LIBEOT_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(LIBEOT_LIBS))

endef

gb_ExternalProject__use_libeot :=

else # !SYSTEM_LIBEOT

define gb_LinkTarget__use_libeot
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,libeot)/inc \
	$$(INCLUDE) \
)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libeot)/.libs/libeot$(gb_StaticLibrary_PLAINEXT) \
)
$(call gb_LinkTarget_use_external_project,$(1),libeot)

endef

define gb_ExternalProject__use_libeot
$(call gb_ExternalProject_use_external_project,$(1),libeot)

endef

endif # SYSTEM_LIBEOT

else # !ENABLE_EOT

gb_LinkTarget__use_libeot :=
gb_ExternalProject__use_libeot :=

endif # ENABLE_EOT

### X11 stuff ###

ifeq ($(GUIBASE),unx)

ifeq ($(XRANDR_DLOPEN),FALSE)

define gb_LinkTarget__use_Xrandr
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(XRANDR_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(XRANDR_LIBS) \
)
endef

else # XRANDR_DLOPEN

define gb_LinkTarget__use_Xrandr
$(call gb_LinkTarget_add_defs,$(1),\
	-DXRANDR_DLOPEN \
)
endef

endif # XRANDR_DLOPEN

define gb_LinkTarget__use_Xrender
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(XRENDER_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
	$(XRENDER_LIBS) \
)
endef

endif # GUIBASE=unx


gb_ExternalProject__use_nss3:=

ifeq (,$(filter DESKTOP,$(BUILD_TYPE)))

gb_LinkTarget__use_nss3:=

else

ifneq ($(SYSTEM_NSS),)

define gb_LinkTarget__use_nss3
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_NSS \
)

$(call gb_LinkTarget_set_include,$(1),\
    $$(INCLUDE) \
    $(NSS_CFLAGS) \
)

$(call gb_LinkTarget_add_libs,$(1),\
    $(NSS_LIBS) \
)

endef

define gb_LinkTarget__use_plc4
$(call gb_LinkTarget__use_nss3,$(1))

endef

define gb_LinkTarget__use_ssl3
$(call gb_LinkTarget__use_nss3,$(1))

endef

else # !SYSTEM_NSS

$(eval $(call gb_Helper_register_packages_for_install,ooo, \
	nss \
))

define gb_LinkTarget__use_nss3
$(call gb_LinkTarget_use_package,$(1),nss)
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	-I$(call gb_UnpackedTarball_get_dir,nss)/dist/public/nss \
	-I$(call gb_UnpackedTarball_get_dir,nss)/dist/out/include \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib/nspr4.lib \
	$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib/nss3.lib \
	$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib/smime3.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib \
	-lnspr4 \
	-lnss3 \
	-lsmime3 \
)
endif

endef

define gb_LinkTarget__use_plc4
$(call gb_LinkTarget_use_package,$(1),nss)
ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib/plc4.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
    -L$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib -lplc4 \
)
endif

endef

define gb_LinkTarget__use_ssl3
$(call gb_LinkTarget_use_package,$(1),nss)
ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib/ssl3.lib \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	-L$(call gb_UnpackedTarball_get_dir,nss)/dist/out/lib -lssl3 \
)
endif

endef

define gb_ExternalProject__use_nss3
$(call gb_ExternalProject_use_package,$(1),nss)

endef

endif # SYSTEM_NSS

endif # DESKTOP

ifeq ($(ENABLE_GLTF),TRUE)

ifneq ($(SYSTEM_LIBGLTF),TRUE)

define gb_LinkTarget__use_libgltf
$(call gb_LinkTarget_set_include,$(1),\
    -I$(call gb_UnpackedTarball_get_dir,libgltf)/inc \
    $$(INCLUDE) \
)

ifeq ($(COM),MSC)
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libgltf)/build/win32/$(if $(MSVC_USE_DEBUG_RUNTIME),Debug/libgltf.lib,Release/libgltf.lib) \
)
else
$(call gb_LinkTarget_add_libs,$(1),\
	$(call gb_UnpackedTarball_get_dir,libgltf)/src/.libs/libgltf-0.0$(gb_StaticLibrary_PLAINEXT) \
)
endif

$(call gb_LinkTarget_use_external_project,$(1),libgltf)

endef

define gb_ExternalProject__use_libgltf
$(call gb_ExternalProject_use_external_project,$(1),libgltf)

endef

else # SYSTEM_LIBGLTF

define gb_LinkTarget__use_libgltf
$(call gb_LinkTarget_set_include,$(1),\
	$$(INCLUDE) \
	$(LIBGLTF_CFLAGS) \
)
$(call gb_LinkTarget_add_libs,$(1),$(LIBGLTF_LIBS))
$(call gb_LinkTarget_add_defs,$(1),\
	-DSYSTEM_LIBGLTF \
)

endef

endif # SYSTEN_LIBGLTF

ifeq ($(ENABLE_COLLADA),TRUE)

define gb_LinkTarget__use_opencollada_parser
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,opencollada)/COLLADABaseUtils/include \
	-I$(call gb_UnpackedTarball_get_dir,opencollada)/COLLADAFramework/include \
	-I$(call gb_UnpackedTarball_get_dir,opencollada)/COLLADASaxFrameworkLoader/include \
	-I$(call gb_UnpackedTarball_get_dir,opencollada)/GeneratedSaxParser/include \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	opencollada_parser \
)
endef

define gb_LinkTarget__use_collada2gltf
$(call gb_LinkTarget_set_include,$(1),\
	-I$(call gb_UnpackedTarball_get_dir,collada2gltf) \
	-I$(call gb_UnpackedTarball_get_dir,collada2gltf)/assetModifiers \
	-I$(call gb_UnpackedTarball_get_dir,collada2gltf)/GLTF \
	-I$(call gb_UnpackedTarball_get_dir,collada2gltf)/helpers \
	-I$(call gb_UnpackedTarball_get_dir,collada2gltf)/JSON \
	-I$(call gb_UnpackedTarball_get_dir,collada2gltf)/dependencies/json/include/rapidjson/ \
	$$(INCLUDE) \
)

$(call gb_LinkTarget_use_static_libraries,$(1),\
	collada2gltf \
)
endef

endif # ENABLE_COLLADA

endif # ENABLE_GLTF

### Jars ############################################################

ifneq ($(SYSTEM_HSQLDB),)

define gb_Jar__use_hsqldb
$(call gb_Jar_use_system_jar,$(1),$(HSQLDB_JAR))
endef
define gb_JunitTest__use_hsqldb
$(call gb_JunitTest_use_system_jar,$(1),$(HSQLDB_JAR))
endef

else # !SYSTEM_HSQLDB

ifeq ($(ENABLE_JAVA),TRUE)
$(eval $(call gb_Helper_register_jars_for_install,OOO,ooo, \
	hsqldb \
))
endif

define gb_Jar__use_hsqldb
$(call gb_Jar_use_jar,$(1),hsqldb)
endef
define gb_JunitTest__use_hsqldb
$(call gb_JunitTest_use_jar,$(1),hsqldb)
endef

endif # SYSTEM_HSQLDB


ifeq ($(ENABLE_SCRIPTING_BEANSHELL),TRUE)

ifneq ($(SYSTEM_BSH),)

define gb_Jar__use_bsh
$(call gb_Jar_use_system_jar,$(1),$(BSH_JAR))
endef

else # !SYSTEM_BSH

ifeq ($(ENABLE_JAVA),TRUE)
$(eval $(call gb_Helper_register_jars_for_install,OOO,extensions_bsh, \
	bsh \
))
endif

define gb_Jar__use_bsh
$(call gb_Jar_use_jar,$(1),bsh)
endef

endif # SYSTEM_BSH

endif

ifeq ($(ENABLE_SCRIPTING_JAVASCRIPT),TRUE)

ifneq ($(SYSTEM_RHINO),)

define gb_Jar__use_rhino
$(call gb_Jar_use_system_jar,$(1),$(RHINO_JAR))
endef

else # !SYSTEM_RHINO

ifeq ($(ENABLE_JAVA),TRUE)
$(eval $(call gb_Helper_register_jars_for_install,OOO,extensions_rhino, \
	js \
))
endif

define gb_Jar__use_rhino
$(call gb_Jar_use_jar,$(1),js)
endef

endif # SYSTEM_RHINO

endif

ifneq ($(SYSTEM_APACHE_COMMONS),)

define gb_Jar__use_commons-codec
$(call gb_Jar_use_system_jar,$(1),$(COMMONS_CODEC_JAR))
endef
gb_ExternalProject__use_commons-codec :=

define gb_Jar__use_commons-httpclient
$(call gb_Jar_use_system_jar,$(1),$(COMMONS_HTTPCLIENT_JAR))
endef
gb_ExternalProject__use_commons-httpclient :=

define gb_Jar__use_commons-lang
$(call gb_Jar_usadd_linked_libse_system_jar,$(1),$(COMMONS_LANG_JAR))
endef
gb_ExternalProject__use_commons-lang :=

define gb_Jar__use_commons-logging
$(call gb_Jar_use_system_jar,$(1),$(COMMONS_LOGGING_JAR))
endef
gb_ExternalProject__use_commons-logging :=

else # !SYSTEM_APACHE_COMMONS

ifeq ($(ENABLE_JAVA),TRUE)
$(eval $(call gb_Helper_register_jars_for_install,OOO,reportbuilder,\
	commons-logging-1.1.3 \
))
endif
$(eval $(call gb_Helper_register_jars,OXT,\
	$(if $(filter TRUE,$(HAVE_JAVA6)),commons-codec-1.9,commons-codec-1.6) \
	commons-httpclient-3.1 \
	$(if $(filter TRUE,$(HAVE_JAVA6)),commons-lang3-3.3.1.jar,commons-lang-2.4) \
))

define gb_Jar__use_commons-codec
$(call gb_Jar_use_external_project,$(1),apache_commons_codec)
$(call gb_Jar_use_external_jar,$(1),$(call gb_UnpackedTarball_get_dir,apache_commons_codec)\
$(if $(filter TRUE,$(HAVE_JAVA6)),/dist/commons-codec-1.9.jar,/dist/commons-codec-1.6.jar),\
$(if $(filter TRUE,$(HAVE_JAVA6)),commons-codec-1.9.jar,commons-codec-1.6.jar)\
)
endef
define gb_ExternalProject__use_commons-codec
$(call gb_ExternalProject_use_external_project,$(1),apache_commons_codec)
endef

define gb_Jar__use_commons-httpclient
$(call gb_Jar_use_external_project,$(1),apache_commons_httpclient)
$(call gb_Jar_use_external_jar,$(1),$(call gb_UnpackedTarball_get_dir,apache_commons_httpclient)/dist/commons-httpclient.jar,commons-httpclient-3.1.jar)
endef
define gb_ExternalProject__use_commons-httpclient
$(call gb_ExternalProject_use_external_project,$(1),apache_commons_httpclient)
endef

define gb_Jar__use_commons-lang
$(call gb_Jar_use_external_project,$(1),apache_commons_lang)
$(call gb_Jar_use_external_jar,$(1),$(call gb_UnpackedTarball_get_dir,apache_commons_lang)\
$(if $(filter TRUE,$(HAVE_JAVA6)),/target/commons-lang3-3.3.1.jar,/dist/commons-lang-2.4.jar),\
$(if $(filter TRUE,$(HAVE_JAVA6)),commons-lang3-3.3.1.jar,commons-lang-2.4.jar)\
)
endef
define gb_ExternalProject__use_commons-lang
$(call gb_ExternalProject_use_external_project,$(1),apache_commons_lang)
endef

define gb_Jar__use_commons-logging
$(call gb_Jar_use_external_project,$(1),apache_commons_logging)
$(call gb_Jar_use_jar,$(1),commons-logging-1.1.3)
endef
define gb_ExternalProject__use_commons-logging
$(call gb_ExternalProject_use_external_project,$(1),apache_commons_logging)
endef

endif # SYSTEM_APACHE_COMMONS


ifneq ($(SYSTEM_JFREEREPORT),)

define gb_Jar__use_flow-engine
$(call gb_Jar_use_system_jar,$(1),$(JFREEREPORT_JAR))
endef

define gb_Jar__use_flute
$(call gb_Jar_use_system_jar,$(1),$(FLUTE_JAR))
endef

define gb_Jar__use_libbase
$(call gb_Jar_use_system_jar,$(1),$(LIBBASE_JAR))
endef

define gb_Jar__use_libfonts
$(call gb_Jar_use_system_jar,$(1),$(LIBFONTS_JAR))
endef

define gb_Jar__use_libformula
$(call gb_Jar_use_system_jar,$(1),$(LIBFORMULA_JAR))
endef

define gb_Jar__use_liblayout
$(call gb_Jar_use_system_jar,$(1),$(LIBLAYOUT_JAR))
endef

define gb_Jar__use_libloader
$(call gb_Jar_use_system_jar,$(1),$(LIBLOADER_JAR))
endef

define gb_Jar__use_librepository
$(call gb_Jar_use_system_jar,$(1),$(LIBREPOSITORY_JAR))
endef

define gb_Jar__use_libserializer
$(call gb_Jar_use_system_jar,$(1),$(LIBSERIALIZER_JAR))
endef

define gb_Jar__use_libxml
$(call gb_Jar_use_system_jar,$(1),$(LIBXML_JAR))
endef

define gb_Jar__use_sac
$(call gb_Jar_use_system_jar,$(1),$(SAC_JAR))
endef

else # !SYSTEM_JFREEREPORT

ifeq ($(ENABLE_JAVA),TRUE)
$(eval $(call gb_Helper_register_jars_for_install,OOO,reportbuilder,\
	flow-engine \
	flute-1.1.6 \
	libbase-1.1.6 \
	libfonts-1.1.6 \
	libformula-1.1.7 \
	liblayout \
	libloader-1.1.6 \
	librepository-1.1.6 \
	libserializer-1.1.6 \
	libxml-1.1.7 \
	sac \
))
endif

define gb_Jar__use_flow-engine
$(call gb_Jar_use_jar,$(1),flow-engine)
endef

define gb_Jar__use_flute
$(call gb_Jar_use_jar,$(1),flute-1.1.6)
endef

define gb_Jar__use_libbase
$(call gb_Jar_use_jar,$(1),libbase-1.1.6)
endef

define gb_Jar__use_libfonts
$(call gb_Jar_use_jar,$(1),libfonts-1.1.6)
endef

define gb_Jar__use_libformula
$(call gb_Jar_use_jar,$(1),libformula-1.1.7)
endef

define gb_Jar__use_liblayout
$(call gb_Jar_use_jar,$(1),liblayout)
endef

define gb_Jar__use_libloader
$(call gb_Jar_use_jar,$(1),libloader-1.1.6)
endef

define gb_Jar__use_librepository
$(call gb_Jar_use_jar,$(1),librepository-1.1.6)
endef

define gb_Jar__use_libserializer
$(call gb_Jar_use_jar,$(1),libserializer-1.1.6)
endef

define gb_Jar__use_libxml
$(call gb_Jar_use_jar,$(1),libxml-1.1.7)
endef

define gb_Jar__use_sac
$(call gb_Jar_use_jar,$(1),sac)
endef

endif # SYSTEM_JFREEREPORT


# Executables

# FIXME: the library target should be for build too
define gb_Executable__register_bestreversemap
$(call gb_Executable_add_runtime_dependencies,bestreversemap,\
	$(if $(filter-out ANDROID,$(OS)),$(call gb_Library_get_target,sal_textenc)) \
)
endef

# TODO depending on the whole URE might be overkill, but I do not have a
# Windows machine to debug it...
# FIXME: the library target should be for build too
define gb_Executable__register_climaker
$(call gb_Executable_add_runtime_dependencies,climaker,\
	$(call gb_Library_get_target,$(gb_CPPU_ENV)_uno) \
	$(call gb_Rdb_get_target_for_build,ure/services) \
	$(INSTROOT)/$(LIBO_URE_SHARE_FOLDER)/misc/services.rdb \
	$(call gb_UnoApi_get_target,udkapi) \
)
endef

define gb_Executable__register_cppumaker
$(call gb_Executable_add_runtime_dependencies,cppumaker,\
	$(if $(filter-out ANDROID,$(OS)),$(call gb_Library_get_target,sal_textenc)) \
)
endef

gb_Gallery__UNO_COMPONENTS := \
	comphelper/util/comphelp \
	configmgr/source/configmgr \
	drawinglayer/drawinglayer \
	framework/util/fwk \
	i18npool/util/i18npool \
	package/source/xstor/xstor \
	package/util/package2 \
	sax/source/expatwrap/expwrap \
	sfx2/util/sfx \
	svgio/svgio \
	svx/util/svx \
	svx/util/svxcore \
	ucb/source/core/ucb1 \
	ucb/source/ucp/file/ucpfile1 \
	unoxml/source/service/unoxml

# This is used to determine what we need for 'build' platform.
define gb_Executable__register_gengal
$(call gb_Executable_add_runtime_dependencies,gengal,\
	$(foreach component,$(gb_Gallery__UNO_COMPONENTS) \
	,$(call gb_ComponentTarget_get_target_for_build,$(component))) \
	$(call gb_AllLangResTarget_get_target,ofa) \
	$(call gb_Library_get_target,$(gb_CPPU_ENV)_uno) \
	$(if $(filter-out MACOSX WNT,$(OS_FOR_BUILD)),$(if $(ENABLE_HEADLESS),, \
		$(call gb_Library_get_target,vclplug_svp) \
	)) \
	$(call gb_Package_get_target_for_build,postprocess_images) \
	$(call gb_Package_get_target_for_build,postprocess_registry) \
	$(call gb_Rdb_get_target_for_build,ure/services) \
	$(INSTROOT)/$(LIBO_URE_SHARE_FOLDER)/misc/services.rdb \
	$(call gb_UnoApi_get_target,offapi) \
	$(call gb_UnoApi_get_target,udkapi) \
)
endef

ifeq ($(SYSTEM_ICU),)

define gb_Executable__register_gendict
$(call gb_Executable_add_runtime_dependencies,gendict,\
	$(call gb_Package_get_target_for_build,icu) \
)
endef

endif

define gb_Executable__register_idlc
$(call gb_Executable_add_runtime_dependencies,idlc,\
	$(call gb_ExternalExecutable_get_dependencies,ucpp) \
)
endef

define gb_Executable__register_localize
$(call gb_Executable_add_runtime_dependencies,localize,\
	$(foreach exec,cfgex helpex propex transex3 treex uiex ulfex xrmex,\
		$(call gb_Executable_get_runtime_dependencies,$(exec)) \
	) \
)
endef

# FIXME ure/services.rdb needs cleanup
# The dependencies on ure/services.rdb and udkapi.rdb are implicitly required
# due to the settings for URE_SERVICES and URE_TYPES in cppuhelper/source/unorc:
# FIXME: the library target should be for build too
define gb_Executable__register_saxparser
$(call gb_Executable_add_runtime_dependencies,saxparser,\
	$(call gb_Library_get_target,$(gb_CPPU_ENV)_uno) \
	$(call gb_Rdb_get_target_for_build,saxparser) \
	$(call gb_Rdb_get_target_for_build,ure/services) \
	$(INSTROOT)/$(LIBO_URE_SHARE_FOLDER)/misc/services.rdb \
	$(call gb_UnoApi_get_target,udkapi) \
)
endef

# NOTE: the dependencies on ure/services.rdb and udkapi.rdb are implicitly
# required due to the settings for URE_SERVICES and URE_TYPES in
# cppuhelper/source/unorc
# FIXME: the library target should be for build too
define gb_Executable__register_uno
$(call gb_Executable_add_runtime_dependencies,uno,\
	$(call gb_Library_get_target,$(gb_CPPU_ENV)_uno) \
	$(call gb_Rdb_get_target_for_build,ure/services) \
	$(INSTROOT)/$(LIBO_URE_SHARE_FOLDER)/misc/services.rdb \
	$(call gb_UnoApi_get_target,udkapi) \
)
endef


# External executables

ifneq ($(SYSTEM_LIBXML_FOR_BUILD),)

gb_ExternalExecutable__register_xmllint :=

else # ! SYSTEM_LIBXML_FOR_BUILD

define gb_ExternalExecutable__register_xmllint
$(call gb_ExternalExecutable_set_internal,xmllint,$(WORKDIR_FOR_BUILD)/UnpackedTarball/xml2/$(if $(filter MSC,$(COM)),win32/bin.msvc)/xmllint$(gb_Executable_EXT_for_build),xml2)
$(call gb_ExternalExecutable_add_dependencies,xmllint,\
	$(if $(filter WNT,$(OS)),$(call gb_Package_get_target,icu_ure)) \
	$(call gb_Package_get_target,xml2) \
)

endef

endif # SYSTEM_LIBXML_FOR_BUILD

ifneq ($(SYSTEM_LIBXSLT_FOR_BUILD),)

gb_ExternalExecutable__register_xsltproc :=

else # ! SYSTEM_LIBXSLT_FOR_BUILD

define gb_ExternalExecutable__register_xsltproc
$(call gb_ExternalExecutable_set_internal,xsltproc,$(WORKDIR_FOR_BUILD)/UnpackedTarball/xslt/$(if $(filter MSC,$(COM)),win32/bin.msvc,xsltproc)/xsltproc$(gb_Executable_EXT_for_build),xslt)
$(call gb_ExternalExecutable_add_dependencies,xsltproc,\
	$(if $(filter WNT,$(OS)),$(call gb_Package_get_target,icu_ure)) \
	$(call gb_Package_get_target,xslt) \
)

endef

endif # SYSTEM_LIBXSLT_FOR_BUILD

ifneq (,$(SYSTEM_UCPP))

gb_ExternalExecutable__register_ucpp :=

else # ! SYSTEM_UCPP

define gb_ExternalExecutable__register_ucpp
$(call gb_ExternalExecutable_set_internal,ucpp,$(INSTDIR_FOR_BUILD)/$(SDKDIRNAME)/bin/ucpp$(gb_Executable_EXT_for_build))

endef

endif # SYSTEM_UCPP

ifeq (,$(PYTHON_FOR_BUILD))

define gb_ExternalExecutable__register_python
ifeq ($(OS),MACOSX)

# use set_external, to avoid having the command added as prerequisite for the
# targets that make use of it. (Otherwise make will choke as it doesn't have a
# matching rule to build that specific file)
$(call gb_ExternalExecutable_set_external,python,$(call gb_UnpackedTarball_get_dir,python3)/python-inst/@__________________________________________________OOO/LibreOfficePython.framework/Versions/$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR)/bin/python$(PYTHON_VERSION_MAJOR).$(PYTHON_VERSION_MINOR))
# the Zip ensures that internal python has been built (cannot use the Package
# target, as that is not used on Mac)
$(call gb_ExternalExecutable_add_dependencies,python,$(call gb_GeneratedPackage_get_target_for_build,python3))

else

$(call gb_ExternalExecutable_set_internal,python,$(INSTROOT)/$(LIBO_BIN_FOLDER)/$(if $(filter WNT,$(OS)),python-core-$(PYTHON_VERSION)/bin/python.exe,python.bin))
$(call gb_ExternalExecutable_set_precommand,python,$(subst $$,$$$$,$(gb_Python_PRECOMMAND)))
$(call gb_ExternalExecutable_add_dependencies,python,$(call gb_Package_get_target_for_build,python3))

endif

endef

else

define gb_ExternalExecutable__register_python
$(call gb_ExternalExecutable_set_external,python,$(PYTHON_FOR_BUILD))

endef

endif # PYTHON_FOR_BUILD

ifneq ($(SYSTEM_GENBRK),)

define gb_ExternalExecutable__register_genbrk
$(call gb_ExternalExecutable_set_external,genbrk,$(SYSTEM_GENBRK))

endef

else # ! SYSTEM_GENBRK

define gb_ExternalExecutable__register_genbrk
$(call gb_ExternalExecutable_set_internal,genbrk,$(WORKDIR_FOR_BUILD)/UnpackedTarball/icu/source/bin/genbrk$(gb_Executable_EXT_for_build),icu)
$(call gb_ExternalExecutable_set_precommand,genbrk,$(subst $$,$$$$,$(gb_ICU_PRECOMMAND)))
$(call gb_ExternalExecutable_add_dependencies,genbrk,\
	$(call gb_Package_get_target_for_build,icu) \
)

endef

endif

ifneq ($(SYSTEM_GENCCODE),)

define gb_ExternalExecutable__register_genccode
$(call gb_ExternalExecutable_set_external,genccode,$(SYSTEM_GENCCODE))

endef

else # ! SYSTEM_GENCCODE

define gb_ExternalExecutable__register_genccode
$(call gb_ExternalExecutable_set_internal,genccode,$(WORKDIR_FOR_BUILD)/UnpackedTarball/icu/source/bin/genccode$(gb_Executable_EXT_for_build),icu)
$(call gb_ExternalExecutable_set_precommand,genccode,$(subst $$,$$$$,$(gb_ICU_PRECOMMAND)))
$(call gb_ExternalExecutable_add_dependencies,genccode,\
	$(call gb_Package_get_target_for_build,icu) \
)

endef

endif

ifneq ($(SYSTEM_GENCMN),)

define gb_ExternalExecutable__register_gencmn
$(call gb_ExternalExecutable_set_external,gencmn,$(SYSTEM_GENCMN))

endef

else # ! SYSTEM_GENCMN

define gb_ExternalExecutable__register_gencmn
$(call gb_ExternalExecutable_set_internal,gencmn,$(WORKDIR_FOR_BUILD)/UnpackedTarball/icu/source/bin/gencmn$(gb_Executable_EXT_for_build),icu)
$(call gb_ExternalExecutable_set_precommand,gencmn,$(subst $$,$$$$,$(gb_ICU_PRECOMMAND)))
$(call gb_ExternalExecutable_add_dependencies,gencmn,\
	$(call gb_Package_get_target_for_build,icu) \
)

endef

endif

# vim: set noet sw=4 ts=4:
