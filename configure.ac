AC_INIT(Imendio Gossip, 0.8.90 , gossip)
AC_PREREQ(2.59)
AC_COPYRIGHT([Copyright (C) 2003-2005 Imendio AB])

AC_CONFIG_SRCDIR(src/gossip-app.h)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE(1.9 -Wall check-news dist-bzip2 no-define)

AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC

AM_PROG_LIBTOOL
AM_PATH_GLIB_2_0 
AC_PATH_X

AC_PROG_INTLTOOL
AC_PATH_PROG(GCONFTOOL, gconftool-2)

IMENDIO_COMPILE_WARNINGS

AM_GCONF_SOURCE_2

dnl -----------------------------------------------------------
dnl D-BUS
dnl -----------------------------------------------------------

AC_ARG_ENABLE(dbus,
              AS_HELP_STRING([--enable-dbus=@<:@no/yes/auto@:>@],
                             [compile with d-bus support]), ,
              enable_dbus=no)

if test "x$enable_dbus" != "xno"; then
   dbus_pkgconfig_tmp="dbus-1 >= 0.31 dbus-glib-1"
   PKG_CHECK_MODULES(DBUS, $dbus_pkgconfig_tmp, have_dbus=yes, have_dbus=no)
   if test x$have_dbus = xyes; then
      AC_DEFINE(HAVE_DBUS, 1, d-bus support)
      dbus_pkgconfig=$dbus_pkgconfig_tmp
  fi
else
   have_dbus=no
   dbus_pkgconfig=
fi

if test "x$enable_dbus" = "xyes"; then
   if test "x$have_dbus" != "xyes"; then
      AC_MSG_ERROR([Couldn't find d-bus.])
   fi
fi

AM_CONDITIONAL(HAVE_DBUS, test x$have_dbus = xyes)

dnl -----------------------------------------------------------
dnl
dnl Library Dependencies, uses pkg-config to detect correct version, etc
dnl
dnl -----------------------------------------------------------

dnl AC_SUBST is required to ensure that the versions number end up into the
dnl spec file.

AC_SUBST(LIBXML_REQUIRED, 2.4.7)
AC_SUBST(LIBXSLT_REQUIRED, 1.0.0)
AC_SUBST(GLIB_REQUIRED, 2.6.0)
AC_SUBST(GTK_REQUIRED, 2.6.0)
AC_SUBST(LIBGLADE_REQUIRED, 2.0.0)
AC_SUBST(LOUDMOUTH_REQUIRED, 0.17)
AC_SUBST(GCONF_REQUIRED, 1.2.0)
AC_SUBST(LIBGNOME_REQUIRED, 2.0.0)
AC_SUBST(LIBGNOMEVFS_REQUIRED, 2.0.0)
AC_SUBST(DESKTOP_FILE_UTILS_REQUIRED.0.2.90)

dnl To make the egg stuff build properly, should have a real test...
AC_DEFINE(HAVE_GTK_MULTIHEAD,0,[gtk+ with multihead support found])


dnl -----------------------------------------------------------
dnl Pkg-Config dependency checks
dnl -----------------------------------------------------------

PKG_CHECK_MODULES(LIBGOSSIP, [
	glib-2.0 >= $GLIB_REQUIRED
	gobject-2.0
	libgnome-2.0 >= $LIBGNOME_REQUIRED
	gconf-2.0 >= $GCONF_REQUIRED
])

PKG_CHECK_MODULES(GOSSIP,
[
	libxml-2.0 >= $LIBXML_REQUIRED
	libxslt >= $LIBXSLT_REQUIRED
	glib-2.0 >= $GLIB_REQUIRED
	gobject-2.0
	gtk+-2.0 >= $GTK_REQUIRED
	libglade-2.0 >= $LIBGLADE_REQUIRED 
	loudmouth-1.0 >= $LOUDMOUTH_REQUIRED
	gconf-2.0 >= $GCONF_REQUIRED
	libgnomeui-2.0 >= $LIBGNOME_REQUIRED
	gnome-vfs-2.0 >= $LIBGNOMEVFS_REQUIRED
	$dbus_pkgconfig
])

PKG_CHECK_MODULES(GOSSIP_JABBER,
[
        glib-2.0 >= $GLIB_REQUIRED
	gobject-2.0
	loudmouth-1.0 >= $LOUDMOUTH_REQUIRED
	gconf-2.0 >= $GCONF_REQUIRED
	gnome-vfs-2.0 >= $LIBGNOMEVFS_REQUIRED
	libxml-2.0 >= $LIBXML_REQUIRED
])
# Sucks that we need to do this
GOSSIP_LIBS="$GOSSIP_LIBS -lexslt"

dnl -----------------------------------------------------------
dnl Check for autoaway
dnl -----------------------------------------------------------

dnl Check for XScreenSaver
old_LIBS="$LIBS"
old_CFLAGS="$CFLAGS"
LIBS="$LIBS -L$x_libraries $GOSSIP_LIBS"
CFLAGS="$CFLAGS -I$x_includes $GOSSIP_CFLAGS"
XSS_LIBS="no"
XSS_HEADERS="no"
AC_CHECK_LIB(Xext, XScreenSaverRegister,[XSS_LIBS="-L$x_libraries"],[],[-lX11 -lXext -lm])
AC_CHECK_LIB(Xss, XScreenSaverRegister,[XSS_LIBS="-L$x_libraries -lXss"],[],[-lX11 -lXext -lm])
if test "x$XSS_LIBS" != "xno"; then
   AC_TRY_COMPILE([
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>
		],[],[
		AC_DEFINE(USE_SCREENSAVER, 1, [Define if we're using XScreenSaver.])
		], [enable_xss=no])
else
    XSS_LIBS=""
    enable_xss=no
fi
LIBS="$old_LIBS"
CFLAGS="$old_CFLAGS"

if test "x$enable_xss" = "xno"; then
  AC_MSG_ERROR([Couldn't find XScreenSaver extension.])
fi

GOSSIP_CFLAGS="$GOSSIP_CFLAGS $XSS_CFLAGS"
GOSSIP_LIBS="$GOSSIP_LIBS $XSS_LIBS"

AC_SUBST(GOSSIP_CFLAGS)
AC_SUBST(GOSSIP_LIBS)

dnl -----------------------------------------------------------
dnl Language Support
dnl -----------------------------------------------------------
GETTEXT_PACKAGE=gossip
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])

ALL_LINGUAS="ar az bg ca cs cy da de en_CA en_GB eo es eu fr ga hr it lt ml ms nb nl no pl pt pt_BR ru sq sr sr@Latn sv tr uk zh_CN zh_TW"

AM_GLIB_GNU_GETTEXT


dnl -----------------------------------------------------------
dnl ASpell
dnl -----------------------------------------------------------

AC_ARG_ENABLE(aspell,
              AS_HELP_STRING([--enable-aspell=@<:@no/yes/auto@:>@],
                             [compile with aspell support]), ,
			     enable_aspell=auto)

if test "x$enable_aspell" != "xno"; then

	AC_CHECK_LIB(aspell, aspell_speller_check,have_aspell=yes,have_aspell=no,)
	
	if test "x$have_aspell" = "xyes"; then
  		AC_DEFINE(HAVE_ASPELL, 1, [Define if we have aspell])
		GOSSIP_LIBS="$GOSSIP_LIBS -laspell"
	else
   		have_aspell=no
	fi
else
	have_aspell=no
fi

if test "x$enable_aspell" = "xyes"; then
   if test "x$have_aspell" != "xyes"; then
      AC_MSG_ERROR([Couldn't find aspell.])
   fi
fi

AM_CONDITIONAL(HAVE_DBUS, test x$have_dbus = xyes)

dnl -----------------------------------------------------------


CFLAGS="$CFLAGS $WARN_CFLAGS"

AC_OUTPUT([
Makefile
ui/Makefile
ui/pixmaps/Makefile
libgossip/Makefile
src/Makefile
src/sounds/Makefile
protocols/Makefile
protocols/jabber/Makefile
po/Makefile.in
gossip.desktop.in
])

echo
echo "Gossip prefix: $prefix"
echo "D-BUS        : $have_dbus"
echo "ASpell       : $have_aspell"
echo
