AM_CPPFLAGS = \
	-I$(top_srcdir)/src/libstrongswan \
	-I$(top_srcdir)/src/libhydra \
	-DRESOLV_CONF=\"${resolv_conf}\"

AM_CFLAGS = \
	-rdynamic

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-resolve.la
else
plugin_LTLIBRARIES = libstrongswan-resolve.la
endif

libstrongswan_resolve_la_SOURCES = \
	resolve_plugin.h resolve_plugin.c \
	resolve_handler.h resolve_handler.c

libstrongswan_resolve_la_LDFLAGS = -module -avoid-version
