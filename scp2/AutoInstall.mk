# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_AutoInstall_AutoInstall))

$(eval $(call gb_AutoInstall_add_module,activexbinarytable,LIBO_LIB_FILE_BINARYTABLE))
$(eval $(call gb_AutoInstall_add_module,activex,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,activexwin64,LIBO_LIB_FILE_COMPONENTCONDITION,,,"VersionNT64"))
$(eval $(call gb_AutoInstall_add_module,base,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,calc,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,extensions_bsh,,,LIBO_JAR_FILE))
$(eval $(call gb_AutoInstall_add_module,extensions_rhino,,,LIBO_JAR_FILE))
$(eval $(call gb_AutoInstall_add_module,gnome,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,graphicfilter,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,impress,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,kde,LIBO_LIB_FILE,LIBO_EXECUTABLE))
$(eval $(call gb_AutoInstall_add_module,math,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,ogltrans,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,onlineupdate,LIBO_LIB_FILE_COMPONENTCONDITION,,,"ISCHECKFORPRODUCTUPDATES=1"))
$(eval $(call gb_AutoInstall_add_module,ooo,LIBO_LIB_FILE,LIBO_EXECUTABLE,LIBO_JAR_FILE))
$(eval $(call gb_AutoInstall_add_module,ooobinarytable,LIBO_LIB_FILE_BINARYTABLE))
$(eval $(call gb_AutoInstall_add_module,python,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,postgresqlsdbc,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,pdfimport,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,reportbuilder,LIBO_LIB_FILE,,LIBO_JAR_FILE))
$(eval $(call gb_AutoInstall_add_module,sdk,,SDK_EXECUTABLE))
$(eval $(call gb_AutoInstall_add_module,tde,LIBO_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,ure,URE_PRIVATE_LIB,URE_EXECUTABLE,URE_JAR_FILE))
$(eval $(call gb_AutoInstall_add_module,winexplorerextbinarytable,LIBO_LIB_FILE_BINARYTABLE))
$(eval $(call gb_AutoInstall_add_module,winexplorerext,SHLXTHDL_LIB_FILE))
$(eval $(call gb_AutoInstall_add_module,winexplorerextnt6,SHLXTHDL_LIB_FILE_COMPONENTCONDITION,,,"VersionNT >= 600"))
$(eval $(call gb_AutoInstall_add_module,winexplorerextwin64,SHLXTHDL_LIB_FILE_COMPONENTCONDITION,,,"VersionNT64"))
$(eval $(call gb_AutoInstall_add_module,winexplorerextwin64nt6,SHLXTHDL_LIB_FILE_COMPONENTCONDITION,,,"VersionNT64 >= 600"))
$(eval $(call gb_AutoInstall_add_module,writer,LIBO_LIB_FILE))

# vim: set noet sw=4 ts=4:
