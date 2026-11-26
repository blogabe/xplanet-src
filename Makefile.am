SUBDIRS = src

man_MANS = xplanet.1

nobase_dist_data_DATA = \
	xplanet/rgb.txt			\
	xplanet/arcs/README		\
	xplanet/arcs/constellations	\
	xplanet/config/README		\
	xplanet/config/default		\
	xplanet/config/earth_markers	\
	xplanet/config/moon_orbit	\
	xplanet/config/overlay_clouds	\
	xplanet/ephemeris/README	\
	xplanet/fonts/README		\
	xplanet/fonts/FreeMonoBold.ttf	\
	xplanet/images/README		\
	xplanet/images/earth.jpg	\
	xplanet/images/hubble.png       \
	xplanet/images/iss.png          \
	xplanet/images/mgs.png		\
	xplanet/images/night.jpg	\
	xplanet/images/odyssey.png	\
	xplanet/images/shuttle.png      \
	xplanet/images/smile.png        \
	xplanet/images/sublunar.png	\
	xplanet/images/subsolar.png	\
	xplanet/images/sun.jpg		\
	xplanet/markers/README		\
	xplanet/markers/brightStars	\
	xplanet/markers/earth		\
	xplanet/markers/mars		\
	xplanet/markers/moon		\
	xplanet/origin/README		\
	xplanet/origin/cassini		\
	xplanet/origin/galileo		\
	xplanet/satellites/README	\
	xplanet/satellites/iss		\
	xplanet/satellites/iss.tle	\
	xplanet/scattering/README	\
	xplanet/scattering/earthRayleigh \
	xplanet/spice/README		\
	xplanet/spice/asteroids		\
	xplanet/spice/asteroids.krn	\
	xplanet/spice/cassini		\
	xplanet/spice/cassini.krn	\
	xplanet/spice/mgs		\
	xplanet/spice/mgs.krn		\
	xplanet/spice/voyager		\
	xplanet/spice/voyager.krn	\
	xplanet/stars/BSC

EXTRA_DIST = xplanet.1 $(sources) LICENSE README.md prepare-build.sh tests/README.md tests/tle_catalog.c tests/check-renderer.sh

check-local:
	@set -e; \
	  tmp=$$(mktemp -d "$${TMPDIR:-/tmp}/xplanet-catalog.XXXXXX"); \
	  trap 'rm -rf -- "$$tmp"' EXIT; \
	  trap 'exit 1' HUP INT TERM; \
	  $(CC) $(CPPFLAGS) $(CFLAGS) -I"$(abs_srcdir)/src/libsgp4sdp4" \
	    "$(abs_srcdir)/tests/tle_catalog.c" \
	    "$(abs_builddir)/src/libsgp4sdp4/libsgp4sdp4.a" -lm -o "$$tmp/tle-catalog"; \
	  "$$tmp/tle-catalog"
	$(SHELL) "$(abs_srcdir)/tests/check-renderer.sh" "$(abs_builddir)/src/xplanet" "$(abs_srcdir)/xplanet"
