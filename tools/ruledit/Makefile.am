## Process this file with automake to produce Makefile.in

MOCCMD=@MOCCMD@

include $(top_srcdir)/bootstrap/Makerules.mk

bin_PROGRAMS = freeciv-ruledit

AM_CPPFLAGS = \
	$(ruledit_cppflags) \
	-I$(top_srcdir)/utility \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/server

freeciv_ruledit_CXXFLAGS = $(ruledit_cxxflags)

MOC_FILES = meta_ruledit_qt.cpp \
            meta_tab_misc.cpp	\
            meta_tab_nation.cpp	\
            meta_tab_tech.cpp

freeciv_ruledit_SOURCES =	\
		tab_misc.cpp	\
		tab_misc.h	\
		tab_nation.cpp	\
		tab_nation.h	\
                tab_tech.cpp	\
		tab_tech.h	\
		ruledit.cpp	\
		ruledit.h	\
		ruledit_qt.cpp	\
		ruledit_qt.h	\
		rulesave.c	\
		rulesave.h	\
		validity.c	\
		validity.h

nodist_freeciv_ruledit_SOURCES = $(MOC_FILES)

freeciv_ruledit_LDADD = \
 $(ruledit_libs) \
 $(top_builddir)/server/libfreeciv-srv.la \
 $(top_builddir)/common/libfreeciv.la \
 $(MAPIMG_WAND_LIBS) $(SERVER_LIBS)

CLEANFILES = $(MOC_FILES)
