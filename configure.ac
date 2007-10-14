AC_INIT(Gossip, 0.27, http://bugzilla.gnome.org/enter_bug.cgi?product=gossip)
AC_PREREQ(2.59)
AC_COPYRIGHT([Copyright (C) 2003-2007 Imendio AB])

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

IT_PROG_INTLTOOL([0.35.0])

AC_PATH_PROG(DBUSBINDINGTOOL, dbus-binding-tool)

IDT_COMPILE_WARNINGS

LIBXML_REQUIRED=2.6.16
GLIB_REQUIRED=2.12.0
GTK_REQUIRED=2.10.0
LIBGLADE_REQUIRED=2.0.0
LOUDMOUTH_REQUIRED=1.2.3
GCONF_REQUIRED=1.2.0
LIBGNOME_REQUIRED=2.0.0
LIBGNOMEVFS_REQUIRED=2.0.0
DESKTOP_FILE_UTILS_REQUIRED=0.2.90
LIBPANELAPPLET_REQUIRED=2.10.0
DBUS_REQUIRED=0.61


dnl -----------------------------------------------------------
dnl GTK+ Platform (X11, OS X or WIN32)
dnl -----------------------------------------------------------
IGE_PLATFORM_CHECK
if test "x$IGE_PLATFORM" = xx11; then
   gconf_pkgconfig=gconf-2.0
   gnome_vfs_pkgconfig=gnome-vfs-2.0
   libgnomeui_pkgconfig=libgnomeui-2.0
   PLATFORM_CFLAGS=
   PLATFORM_LDFLAGS=
elif text "x$IGE_PLATFORM" = xosx; then
   gconf_pkgconfig=
   gnome_vfs_pkgconfig=
   libgnomeui_pkgconfig=
   PLATFORM_CFLAGS="-xobjective-c"
   PLATFORM_LDFLAGS="-framework Carbon -framework AppKit -framework IOKit -lobjc"
elif text "x$IGE_PLATFORM" = xwin32; then
   gconf_pkgconfig=
   gnome_vfs_pkgconfig=
   libgnomeui_pkgconfig=
   PLATFORM_CFLAGS="-mms-bitfields"
   PLATFORM_LDFLAGS="-no-undefined -mwindows"
else
   gconf_pkgconfig=
   gnome_vfs_pkgconfig=
   libgnomeui_pkgconfig=
   PLATFORM_CFLAGS=
   PLATFORM_LDFLAGS=
fi
AM_CONDITIONAL(GCONF_SCHEMAS_INSTALL, test "x$IGE_PLATFORM" = xx11)
AC_SUBST(PLATFORM_CFLAGS)
AC_SUBST(PLATFORM_LDFLAGS)

dnl -----------------------------------------------------------
dnl Help
dnl -----------------------------------------------------------
AC_ARG_ENABLE(help,
              AS_HELP_STRING([--enable-help=@<:@no/yes/auto@:>@],
                             [compile with GNOME Help support]), ,
              enable_help=auto)

if test "x$enable_help" != "xno"; then
   if test $IGE_PLATFORM = x11; then
      GNOME_DOC_INIT
      have_help=yes
   else
      have_help=no
   fi

   if test x$have_dbus = xyes; then
      AC_DEFINE(HAVE_HELP, 1, [GNOME Help support])
   fi
else
   have_help=no
fi

if test "x$enable_help" = "xyes"; then
   if test "x$have_help" != "xyes"; then
      AC_MSG_ERROR([GNOME Help is not available on this platform.])
   fi
fi

AM_CONDITIONAL(HAVE_HELP, test x$have_help = xyes)


dnl -----------------------------------------------------------
dnl D-BUS
dnl -----------------------------------------------------------
AC_ARG_ENABLE(dbus,
              AS_HELP_STRING([--enable-dbus=@<:@no/yes/auto@:>@],
                             [compile with D-Bus support]), ,
              enable_dbus=auto)

if test "x$enable_dbus" != "xno"; then
   dbus_pkgconfig_tmp="dbus-1 >= $DBUS_REQUIRED dbus-glib-1"
   PKG_CHECK_MODULES(DBUS, $dbus_pkgconfig_tmp, have_dbus=yes, have_dbus=no)
   if test x$have_dbus = xyes; then
      AC_DEFINE(HAVE_DBUS, 1, [D-Bus support])
      dbus_pkgconfig=$dbus_pkgconfig_tmp
   fi
else
   have_dbus=no
   dbus_pkgconfig=
fi

if test "x$enable_dbus" = "xyes"; then
   if test "x$have_dbus" != "xyes"; then
      AC_MSG_ERROR([Couldn't find D-Bus.])
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
   galago_pkgconfig_tmp="libgalago >= 0.5.0"
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
   if test "x$have_galago" != "xyes"; then
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
   libnotify_pkgconfig_tmp="libnotify >= 0.4.1"
   PKG_CHECK_MODULES(LIBNOTIFY, $libnotify_pkgconfig_tmp, have_libnotify=yes, have_libnotify=no)
   if test x$have_libnotify = xyes; then
      AC_DEFINE(HAVE_LIBNOTIFY, 1, [libnotify support])
      libnotify_pkgconfig=$libnotify_pkgconfig_tmp
      if pkg-config --max-version=0.4.3.90 libnotify;then
	  AC_DEFINE(LIBNOTIFY_BUGGY_CLOSE, 1, [libnotify close bug see #395588 ])
      fi
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
dnl GNOME Keyring
dnl -----------------------------------------------------------
AC_ARG_ENABLE(gnome_keyring,
	      AS_HELP_STRING([--enable-gnome-keyring=@<:@no/yes/auto@:>@],
		             [compile with Gnome Keyring support]), ,
              enable_gnome_keyring=auto)

if test "x$enable_gnome_keyring" != "xno"; then
	gnome_keyring_pkgconfig_tmp="gnome-keyring-1"
	PKG_CHECK_MODULES(GNOME_KEYRING, $gnome_keyring_pkgconfig_tmp, have_gnome_keyring=yes, have_gnome_keyring=no)
	if test x$have_gnome_keyring = xyes; then
		AC_DEFINE(HAVE_GNOME_KEYRING, 1, [gnome keyring support])
		gnome_keyring_pkgconfig=$gnome_keyring_pkgconfig_tmp
	fi
else
	have_gnome_keyring=no
	gnome_keyring_pkgconfig=
fi

if text "x$enable_gnome_keyring" = "xyes"; then
	if test "x$have_gnome_keyring" != "xyes"; then
		AC_MSG_ERROR([Couldn't find GNOME Keyring.])
	fi
fi

AM_CONDITIONAL(HAVE_GNOME_KEYRING, test x$have_gnome_keyring = xyes)

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
   gtk+-2.0 >= $GTK_REQUIRED
   gobject-2.0
   $gconf_pkgconfig
   $dbus_pkgconfig
   $gnome_vfs_pkgconfig
   $gnome_keyring_pkgconfig
])

PKG_CHECK_MODULES(LIBLOUDERMOUTH, 
[
   loudmouth-1.0 >= $LOUDMOUTH_REQUIRED
   libxml-2.0 >= $LIBXML_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
])

PKG_CHECK_MODULES(GOSSIP,
[
   libxml-2.0 >= $LIBXML_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gtk+-2.0 >= $GTK_REQUIRED
   libglade-2.0 >= $LIBGLADE_REQUIRED 
   loudmouth-1.0 >= $LOUDMOUTH_REQUIRED
   $iso_codes_pkgconfig
   $dbus_pkgconfig
   $galago_pkgconfig
   $libnotify_pkgconfig
   $libgnomeui_pkgconfig
   $gnome_vfs_pkgconfig
])

PKG_CHECK_MODULES(GOSSIP_JABBER,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   loudmouth-1.0 >= $LOUDMOUTH_REQUIRED
   libxml-2.0 >= $LIBXML_REQUIRED
   $gnome_vfs_pkgconfig
])

if test x$have_dbus = xyes; then
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


dnl -----------------------------------------------------------
dnl Check for the XScreenSaver extension (used for autoaway)
dnl -----------------------------------------------------------
have_xss="no"

# Only check for XSS under GNOME, it doesn't make sense to find it
# when we're not using X (like on OSX).
if test $IGE_PLATFORM = x11; then
   x_libs="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"

   AC_CHECK_FUNC(XScreenSaverQueryInfo, [
      have_xss="yes"], [
      save_LIBS=$LIBS
      LIBS="$LIBS $X_PRE_LIBS $X_LIBS -lXss -lXext -lX11"
      AC_CHECK_LIB(Xss, XScreenSaverQueryInfo, 
      [
         have_xss="yes"
         XSS_LIBS="$X_PRE_LIBS $X_LIBS -lXss -lXext -lX11"
	 AC_DEFINE(HAVE_XSS, 1, [Define if we have xss])
      ], 
      [
         LIBS=$save_LIBS
         LIBS="$LIBS $X_PRE_LIBS $X_LIBS -lXext -lX11"
         AC_CHECK_LIB(Xext, XScreenSaverQueryInfo, 
         [
            have_xss="yes"
            XSS_LIBS="$X_PRE_LIBS $X_LIBS -lXext -lX11"],[])
	    AC_DEFINE(HAVE_XSS, 1, [Define if we have xss])
         ])
         LIBS=$save_LIBS
      ])

   AC_SUBST(XSS_LIBS)

   if test x$have_xss != xyes; then
      AC_MSG_ERROR([You must have xss installed])
   fi
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
dnl Platform specific details
dnl -----------------------------------------------------------
if test $IGE_PLATFORM = win32; then
   # Check for windres - which sets the icon for apps
   AC_ARG_VAR(WINDRES, [Windows Resource compiler tool path])
   AC_PATH_TOOL(WINDRES,windres,
	        [AC_MSG_WARN(You must have windres in your patch to compile)])
   AC_SUBST(WINDRES)

   AC_CHECK_HEADERS([windows.h])
   if test "$ac_cv_header_windows_h" = "yes"; then
      AC_DEFINE([WINDOWS_H], 1, [Define to 1 if you have windows.h])
   fi

   AC_CHECK_HEADERS([mmsystem.h])
   if test "$ac_cv_header_mmsystem_h" = "yes"; then
      LIBS="$LIBS -lwinmm"
      AC_DEFINE([MMSYSTEM_H], 1, [Define to 1 if you have mmsystem.h])
   fi

   AC_CHECK_HEADERS([shellapi.h])
   if test "$ac_cv_header_shellapi_h" = "yes"; then
      LIBS="$LIBS -lshell32"
      AC_DEFINE([SHELLAPI_H], 1, [Define to 1 if you have shellapi.h])
   fi
fi


AC_OUTPUT([
  Makefile
  ui/Makefile
  ui/pixmaps/Makefile
  ui/icons/Makefile
  libgossip/Makefile
  libloudermouth/Makefile
  src/Makefile
  src/sounds/Makefile
  po/Makefile.in
  help/Makefile
  peekaboo/Makefile
  peekaboo/src/Makefile
  peekaboo/data/Makefile
  gossip.desktop.in
])

echo
echo "Configuration:"
echo "  Prefix:        $prefix"
echo "  Platform:      $IGE_PLATFORM_NAME"
echo
echo "Extras:"
echo "  DBUS:          $have_dbus"
echo "  Galago:        $have_galago"
echo "  Notify:        $have_libnotify"
echo "  GNOME Keyring: $have_gnome_keyring (only with GNOME)"
echo "  XSS:           $have_xss (only with GNOME)"
echo "  Help           $have_help (only with GNOME Doc)"
echo "  ASpell:        $have_aspell"
echo
echo "Build:"
echo "  Peekaboo:      $have_peekaboo (only with DBUS)"
echo

