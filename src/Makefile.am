## Process this file with automake to produce Makefile.in

INCLUDES = \
	@GTK_CFLAGS@ @LIBXML2_CFLAGS@ -I$(top_srcdir)/pixmaps -I$(top_srcdir)/intl
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
AM_YFLAGS = -d

BUILT_SOURCES = mapconfig.h osm_tagpresets.h

.l.c:
	$(LEX) $(LFLAGS) -o$@ $<
bin_PROGRAMS = mumpot get_maprect simplify-gps download_osm osm2places
noinst_PROGRAMS = pngviewer cur_pos

pngviewer_SOURCES = pngviewer.c png_io.c png_io.h 
mumpot_SOURCES = \
	main.c mapdrawing.c gui_common.c gui_common.h mapconfig.h mapdrawing.h png2ps.c png2ps.h png_io.c png_io.h strasse.c strasse.h findpath.c findpath.h mapconfig.y mapconfig_lex.l mapconfig_data.h printdlg.c printdlg.h create_connection.c create_connection.h osm_view.c osm_view.h osm_parse.h osm_parse.c myintl.h geometry.h geometry.c gps.c gps.h osm_tagpresets.y osm_tagpresets.h osm_tagpresets_lex.l osm_tagpresets_data.h osm_tagpresets_gui.h osm_tagpresets_gui.c
get_maprect_SOURCES= \
	get_maprect.c geometry.c geometry.h mapdrawing.c mapdrawing.h png_io.c png_io.h strasse.c mapconfig.h strasse.h mapconfig_lex.l mapconfig.y mapconfig_data.h png2ps.c png2ps.h gps.c gps.h
simplify_gps_SOURCES= \
	simplify-gps.c gps.c gps.h geometry.c geometry.h
cur_pos_SOURCES = gps.c gps.h cur_pos.c
download_osm_SOURCES= \
	download_osm.c gps.c gps.h
pngviewer_LDADD = @GTK_LIBS@ -lpng -lz -ljpeg
#png2cfxga_LDADD = -lpng -lz -ljpeg
mumpot_LDADD = @GTK_LIBS@  -lpng -lz -ljpeg @LIBXML2_LIBS@ @INTLLIBS@ @BTLIBS@ 
osm2places_LDADD = @LIBXML2_LIBS@
get_maprect_LDADD = @GTK_LIBS@ -lpng -lz -ljpeg @LIBXML2_LIBS@
simplify_gps_LDADD = @GTK_LIBS@ -lz @LIBXML2_LIBS@
download_osm_LDADD = @GTK_LIBS@ -lz @LIBXML2_LIBS@
cur_pos_LDADD = @GTK_LIBS@ -lz @LIBXML2_LIBS@
