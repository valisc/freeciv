## Process this file with automake to produce Makefile.in

SUBDIRS = aicore scriptcore

lib_LTLIBRARIES = libfreeciv.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/utility \
	-I$(srcdir)/aicore \
	-I$(srcdir)/scriptcore \
	$(MAPIMG_WAND_CFLAGS)

libfreeciv_la_SOURCES = \
		achievements.c	\
		achievements.h	\
		actions.c	\
		actions.h	\
		ai.c		\
		ai.h		\
		base.c		\
		base.h		\
		borders.c	\
		borders.h	\
		capstr.c	\
		capstr.h	\
		citizens.c	\
		citizens.h	\
		city.c		\
		city.h		\
		combat.c	\
		combat.h	\
		connection.c	\
		connection.h	\
		dataio.c	\
		dataio.h	\
		diptreaty.c	\
		diptreaty.h	\
		disaster.c	\
		disaster.h	\
		effects.c	\
		effects.h	\
		events.c	\
		events.h	\
		extras.c	\
		extras.h	\
		featured_text.c	\
		featured_text.h	\
		fc_cmdhelp.c	\
		fc_cmdhelp.h	\
		fc_interface.c	\
		fc_interface.h	\
		fc_types.h	\
		game.c		\
		game.h		\
		generate_packets.py \
		government.c	\
		government.h	\
		idex.c		\
		idex.h		\
		improvement.c	\
		improvement.h	\
		map.c		\
		map.h		\
		mapimg.c	\
		mapimg.h	\
		metaknowledge.c \
		metaknowledge.h \
		movement.c	\
		movement.h	\
		name_translation.h \
		nation.c        \
		nation.h        \
		packets.c	\
		packets.h	\
		packets_gen.c	\
		packets_gen.h	\
		packets.def	\
		player.c	\
		player.h	\
		requirements.c	\
		requirements.h	\
		research.c	\
		research.h	\
		rgbcolor.c	\
		rgbcolor.h	\
		road.c		\
		road.h		\
		spaceship.c	\
		spaceship.h	\
		specialist.c	\
		specialist.h	\
		team.c		\
		team.h		\
		tech.c		\
		tech.h		\
		terrain.c	\
		terrain.h	\
		tile.c		\
		tile.h		\
		traderoutes.c	\
		traderoutes.h	\
		traits.h	\
		unit.c		\
		unit.h		\
		unitlist.c	\
		unitlist.h	\
		unittype.c	\
		unittype.h	\
		version.c	\
		version.h	\
		victory.c	\
		victory.h	\
		vision.c	\
		vision.h	\
		workertask.c	\
		workertask.h	\
		worklist.c	\
		worklist.h

libfreeciv_la_DEPENDENCIES = \
	$(top_builddir)/utility/libcivutility.la \
	$(top_builddir)/common/aicore/libaicore.la \
	$(top_builddir)/common/scriptcore/libscriptcore.la

libfreeciv_la_LIBADD = \
	$(top_builddir)/utility/libcivutility.la \
	$(top_builddir)/common/aicore/libaicore.la \
	$(top_builddir)/common/scriptcore/libscriptcore.la \
	$(COMMON_LIBS)

BUILT_SOURCES = packets_gen.c packets_gen.h

# ./generate_packets.py makes both packets_gen.[ch]
#
# Feed the dependence through a fake intermediate file to avoid
# a race condition in a parallel build (like make -j2).
#
# Currently we run the generation directly into the source directory.
# Ideally we'd generate the new files into the build directory but first we'd
# have to resolve conflicts (since there could then be 2 copies).
packets_gen.h packets_gen.c: packets_generate
.INTERMEDIATE: packets_generate
packets_generate: packets.def generate_packets.py
	cd $(srcdir) && ./generate_packets.py
	touch packets_generate

# These files are not generated to builddir, but to srcdir */
MAINTAINERCLEANFILES = \
	$(srcdir)/packets_gen.h \
	$(srcdir)/packets_gen.c

if SVNREV
BUILT_SOURCES += fc_svnrev_gen.h
endif

.PHONY : fc_svnrev_gen.h
fc_svnrev_gen.h :
	"$(top_srcdir)/bootstrap/generate_svnrev.sh" "$(top_srcdir)" "$(top_builddir)"
