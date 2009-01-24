EXTRA_DIST = osm2places.1 get_maprect.1 download_osm.1 simplify-gps.1 mumpot.1 mumpot-tah.1 mumpot-mapnik.1 mumpot-cyclemap.1

man_MANS = osm2places.1 get_maprect.1 download_osm.1 simplify-gps.1 mumpot.1 mumpot-tah.1 mumpot-mapnik.1 mumpot-cyclemap.1

osm2places.1: ../src/osm2places.c
	PATH=`pwd`/../src:$$PATH help2man -N -n "create a place list for mumpot" -o osm2places.1 osm2places
get_maprect.1: ../src/get_maprect.c
	PATH=`pwd`/../src:$$PATH help2man -N -n "save a rectanlge of the map" -o get_maprect.1 get_maprect
download_osm.1: ../src/download_osm.c
	PATH=`pwd`/../src:$$PATH help2man -N -n "downloads osm data" -o download_osm.1 download_osm
simplify-gps.1: ../src/simplify-gps.c
	PATH=`pwd`/../src:$$PATH help2man -N -n "remove useless points"  -o simplify-gps.1 simplify-gps
