AC_INIT(Gossip, 0.12, gossip)
AC_PREREQ(2.59)
AC_COPYRIGHT([Copyright (C) 2003-2006 Imendio AB])

AC_CONFIG_SRCDIR(src/gossip-app.h)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(1.9 dist-bzip2 no-define)

AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC

AM_PROG_LIBTOOL
AM_PATH_GLIB_2_0 
AC_PATH_XTRA

AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2

GNOME_DOC_INIT
IT_PROG_INTLTOOL([0.35.0])

AC_PATH_PROG(GCONFTOOL, gconftool-2)
AC_PATH_PROG(DBUSBINDINGTOOL, dbus-binding-tool)

IDT_COMPILE_WARNINGS


dnl -----------------------------------------------------------
dnl Library Dependencies, uses pkg-config to detect correct version, etc
dnl -----------------------------------------------------------

dnl AC_SUBST is required to ensure that the versions number end up into the
dnl spec file.

AC_SUBST(LIBXML_REQUIRED, 2.6.16)
AC_SUBST(LIBXSLT_REQUIRED, 1.0.0)
AC_SUBST(LIBEXSLT_REQUIRED, 0.8.0)
AC_SUBST(GLIB_REQUIRED, 2.8.0)
AC_SUBST(GTK_REQUIRED, 2.8.0)
AC_SUBST(LIBGLADE_REQUIRED, 2.0.0)
AC_SUBST(LOUDMOUTH_REQUIRED, 1.0)
AC_SUBST(GCONF_REQUIRED, 1.2.0)
AC_SUBST(LIBGNOME_REQUIRED, 2.0.0)
AC_SUBST(LIBGNOMEVFS_REQUIRED, 2.0.0)
AC_SUBST(DESKTOP_FILE_UTILS_REQUIRED. 0.2.90)
AC_SUBST(LIBPANELAPPLET_REQUIRED, 2.8.0)


dnl -----------------------------------------------------------
dnl D-BUS
dnl -----------------------------------------------------------
AC_ARG_ENABLE(dbus,
              AS_HELP_STRING([--enable-dbus=@<:@no/yes/auto@:>@],
                             [compile with dbus support]), ,
              enable_dbus=no)

if test "x$enable_dbus" != "xno"; then
   dbus_pkgconfig_tmp="dbus-1 >= 0.31 dbus-glib-1"
   PKG_CHECK_MODULES(DBUS, $dbus_pkgconfig_tmp, have_dbus=yes, have_dbus=no)
   if test x$have_dbus = xyes; then
      AC_DEFINE(HAVE_DBUS, 1, [d-bus support])
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
dnl Galago
dnl -----------------------------------------------------------
AC_ARG_ENABLE(galago,
              AS_HELP_STRING([--enable-galago=@<:@no/yes/auto@:>@],
                             [compile with galago support]), ,
              enable_galago=no)

if test "x$enable_galago" != "xno"; then
   galago_pkgconfig_tmp="libgalago"
   PKG_CHECK_MODULES(GALAGO, $galago_pkgconfig_tmp, have_galago=yes, have_galago=no)
   if test x$have_galago = xyes; then
      AC_DEFINE(HAVE_GALAGO, 1, [Galago support])
      galago_pkgconfig=$galago_pkgconfig_tmp
  fi
else
   have_galago=no
   galago_pkgconfig=
fi

if test "x$enable_galago" = "xyes"; then
   if test "x$enable_galago" != "xyes"; then
      AC_MSG_ERROR([Couldn't find Galago])
   fi
fi

AM_CONDITIONAL(HAVE_GALAGO, test x$have_galago = xyes)


dnl -----------------------------------------------------------
dnl Notify
dnl -----------------------------------------------------------
AC_ARG_ENABLE(libnotify,
              AS_HELP_STRING([--enable-libnotify=@<:@no/yes/auto@:>@],
                             [compile with libnotify support]), ,
              enable_libnotify=auto)

if test "x$enable_libnotify" != "xno"; then
   libnotify_pkgconfig_tmp="libnotify >= 0.3.2"
   PKG_CHECK_MODULES(LIBNOTIFY, $libnotify_pkgconfig_tmp, have_libnotify=yes, have_libnotify=no)
   if test x$have_libnotify = xyes; then
      AC_DEFINE(HAVE_LIBNOTIFY, 1, [libnotify support])
      libnotify_pkgconfig=$libnotify_pkgconfig_tmp
  fi
else
   have_libnotify=no
   libnotify_pkgconfig=
fi

if test "x$enable_libnotify" = "xyes"; then
   if test "x$have_libnotify" != "xyes"; then
      AC_MSG_ERROR([Couldn't find libnotify.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBNOTIFY, test x$have_libnotify = xyes)


dnl -----------------------------------------------------------
dnl ISO codes, used for aspell support
dnl -----------------------------------------------------------
AC_MSG_CHECKING([whether iso-codes exists and has iso-639 domain])
if $PKG_CONFIG --variable=domains iso-codes | grep -q 639 ; then
   AC_MSG_RESULT([yes])
   iso_codes_prefix=`$PKG_CONFIG --variable=prefix iso-codes`
   iso_codes_pkgconfig=iso-codes
   have_iso_codes=yes
else
   AC_MSG_RESULT([no])
   iso_codes_prefix=
   iso_codes_pkgconfig=
   have_iso_codes=no
fi

AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["$iso_codes_prefix"],[ISO codes prefix])


dnl -----------------------------------------------------------
dnl Pkg-Config dependency checks
dnl -----------------------------------------------------------
PKG_CHECK_MODULES(LIBGOSSIP, 
[
   libxml-2.0 >= $LIBXML_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gnome-vfs-2.0 >= $LIBGNOMEVFS_REQUIRED
   gconf-2.0 >= $GCONF_REQUIRED
])

PKG_CHECK_MODULES(GOSSIP,
[
   libxml-2.0 >= $LIBXML_REQUIRED
   libxslt >= $LIBXSLT_REQUIRED
   libexslt >= $LIBEXSLT_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gtk+-2.0 >= $GTK_REQUIRED
   libglade-2.0 >= $LIBGLADE_REQUIRED 
   loudmouth-1.0 >= $LOUDMOUTH_REQUIRED
   gconf-2.0 >= $GCONF_REQUIRED
   libgnomeui-2.0 >= $LIBGNOME_REQUIRED
   $iso_codes_pkgconfig
   $dbus_pkgconfig
   $galago_pkgconfig
   $libnotify_pkgconfig
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

if test x$have_dbus = xyes -a x$have_galago = xyes; then
   PKG_CHECK_MODULES(PEEKABOO, 
   [
      libgnomeui-2.0 
      libpanelapplet-2.0 >= $LIBPANELAPPLET_REQUIRED
      gconf-2.0 >= $GCONF_REQUIRED
      glib-2.0 >= $GLIB_REQUIRED
      gtk+-2.0 >= $GTK_REQUIRED
      libglade-2.0 >= $LIBGLADE_REQUIRED 
      $dbus_pkgconfig
      $galago_pkgconfig
   ], have_peekaboo="yes", have_peekaboo="no")
else
  have_peekaboo="no"
fi

AM_CONDITIONAL(HAVE_PEEKABOO, test x$have_peekaboo = xyes)

GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

PKG_CHECK_MODULES(GTK_VER, gtk+-2.0 >= 2.7.3,
		  GTK_VER="-DNEW_GTK",
		  GTK_VER="-UNEW_GTK")
AC_SUBST(GTK_VER)

PKG_CHECK_MODULES(PANEL_VER, libpanelapplet-2.0 >= 2.9.4,
		  PANEL_VER="-UUSE_OLDER_PANEL_APPLET",
		  PANEL_VER="-DUSE_OLDER_PANEL_APPLET")
AC_SUBST(PANEL_VER)

dnl -----------------------------------------------------------
dnl Check for libgcrypt
dnl -----------------------------------------------------------
AC_PATH_PROG(LIBGCRYPT_CONFIG, libgcrypt-config, no)
if test x"$LIBGCRYPT_CONFIG" = xno; then
   AC_MSG_ERROR([gcrypt not found on system])
else
   LIBGCRYPT_CFLAGS=`$LIBGCRYPT_CONFIG --cflags`
   LIBGCRYPT_LIBS=`$LIBGCRYPT_CONFIG --libs`

   GOSSIP_JABBER_CFLAGS="$GOSSIP_JABBER_CFLAGS $LIBGCRYPT_CFLAGS"
   GOSSIP_JABBER_LDFLAGS="$GOSSIP_JABBER_LDFLAGS $LIBGCRYPT_LDFLAGS"
fi


dnl -----------------------------------------------------------
dnl Check for the XScreenSaver extension (used for autoaway)
dnl -----------------------------------------------------------
x_libs="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"
have_xss="no"

AC_CHECK_FUNC(XScreenSaverQueryInfo, [
   have_xss="yes"], [
   save_LIBS=$LIBS
   LIBS="$LIBS $X_PRE_LIBS $X_LIBS -lXss -lXext -lX11"
   AC_CHECK_LIB(Xss, XScreenSaverQueryInfo, 
   [
      have_xss="yes"
      XSS_LIBS="$X_PRE_LIBS $X_LIBS -lXss -lXext -lX11"
   ], 
   [
      LIBS=$save_LIBS
      LIBS="$LIBS $X_PRE_LIBS $X_LIBS -lXext -lX11"
      AC_CHECK_LIB(Xext, XScreenSaverQueryInfo, 
      [
         have_xss="yes"
         XSS_LIBS="$X_PRE_LIBS $X_LIBS -lXext -lX11"],[])
      ])
      LIBS=$save_LIBS
   ])

AC_SUBST(XSS_LIBS)

if test "x$have_xss" = "xno"; then
   AC_MSG_ERROR([Couldn't find XScreenSaver extension.])
fi


dnl -----------------------------------------------------------
dnl ASpell
dnl -----------------------------------------------------------
AC_ARG_ENABLE(aspell,
              AS_HELP_STRING([--enable-aspell=@<:@no/yes/auto@:>@],
                             [compile with aspell support]), ,
                             enable_aspell=auto)

if test "x$enable_aspell" = "xyes" -a "x$have_iso_codes" != "xyes"; then
   AC_MSG_ERROR([You must have iso-codes with the iso-639 domain to use aspell.])
fi

if test "x$enable_aspell" != "xno" -a "x$have_iso_codes" = "xyes"; then
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

if test "x$enable_aspell" = "xyes" -a "x$have_aspell" != "xyes"; then
   AC_MSG_ERROR([Couldn't find aspell.])
fi


dnl -----------------------------------------------------------
dnl Language Support
dnl -----------------------------------------------------------
GETTEXT_PACKAGE=gossip
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])

AM_GLIB_GNU_GETTEXT


dnl -----------------------------------------------------------
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
  help/Makefile
  peekaboo/Makefile
  peekaboo/src/Makefile
  peekaboo/data/Makefile
  gossip.desktop.in
])

echo
echo "Gossip prefix: $prefix"
echo 
echo "DBUS         : $have_dbus"
echo "Galago       : $have_galago"
echo "ASpell       : $have_aspell"
echo "Notify       : $have_libnotify"
echo
echo "Peekaboo     : $have_peekaboo (requires DBUS & Galago)"
echo