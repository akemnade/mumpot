## Process this file with automake to produce Makefile.in

SUBDIRS = intl po src config-examples doc

EXTRA_DIST = config.rpath m4/ChangeLog  config.rpath m4/ChangeLog  \
	autogen.sh CONFIGFORMAT mumpot.desktop

dist-hook:
	if test -d pixmaps; then \
	mkdir $(distdir)/pixmaps; \
	for pixmap in pixmaps/*; do \
	if test -f $$pixmap; then \
	  cp -p $$pixmap $(distdir)/pixmaps; \
	fi \
	done \
	 fi; \
	mkdir $(distdir)/m4; \
	for macro in m4/*; do \
	if test -f $$macro; then \
	  cp -p $$macro $(distdir)/m4; \
	fi \
	done


appsdir = $(datadir)/applications
apps_DATA = mumpot.desktop

ACLOCAL_AMFLAGS = -I m4
