PHP_ARG_WITH(tokyo-tyrant, whether to enable tokyo tyrant support,
[  --with-tokyo-tyrant[=DIR]       Enable tokyo tyrant support. DIR is the prefix to Tokyo Tyrant installation directory.], yes)

PHP_ARG_WITH(tokyo-cabinet-dir, directory of the Tokyo Cabinet installation,
[  --with-tokyo-cabinet-dir[=DIR]       DIR is the prefix to Tokyo Cabinet installation directory.], yes)

PHP_ARG_ENABLE(tokyo-tyrant-session, whether to enable tokyo tyrant session handler support,
[  --disable-tokyo-tyrant-session    Disables tokyo tyrant session handler support], yes, no)

if test "$PHP_TOKYO_TYRANT" != "no"; then

dnl Check PHP version

dnl Get PHP version depending on shared/static build

  AC_MSG_CHECKING([PHP version is at least 5.2.0])

  if test -z "${PHP_VERSION_ID}"; then
    if test -z "${PHP_CONFIG}"; then
      AC_MSG_ERROR([php-config not found])
    fi
    PHP_TT_FOUND_VERNUM=`${PHP_CONFIG} --vernum`;
    PHP_TT_FOUND_VERSION=`${PHP_CONFIG} --version`
  else
    PHP_TT_FOUND_VERNUM="${PHP_VERSION_ID}"
    PHP_TT_FOUND_VERSION="${PHP_VERSION}"
  fi

  if test "$PHP_TT_FOUND_VERNUM" -ge "50200"; then
    AC_MSG_RESULT(yes. found $PHP_TT_FOUND_VERSION)
  else 
    AC_MSG_ERROR(no. found $PHP_TT_FOUND_VERSION)
  fi

  
dnl Add dependency to date extension if PHP >= 5.3.0 is used
  if test $PHP_TT_FOUND_VERNUM -ge 50300; then
    PHP_ADD_EXTENSION_DEP(tokyo_tyrant, date)
  fi

dnl Tokyo Tyrant parts
  
  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  if test "x$PKG_CONFIG" = "xno"; then
    AC_MSG_RESULT([pkg-config not found])
    AC_MSG_ERROR([Please reinstall the pkg-config distribution])
  fi

  ORIG_PKG_CONFIG_PATH=$PKG_CONFIG_PATH

  dnl Tokyo Tyrant PKG_CONFIG
  if test "$PHP_TOKYO_TYRANT" = "yes"; then
    export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/opt/lib/pkgconfig:/opt/local/lib/pkgconfig
  else
    export PKG_CONFIG_PATH=$PHP_TOKYO_TYRANT:$PHP_TOKYO_TYRANT/lib/pkgconfig
  fi

  AC_MSG_CHECKING([for Tokyo Tyrant])
  if $PKG_CONFIG --exists tokyotyrant; then
    PHP_TYRANT_INCS=`$PKG_CONFIG tokyotyrant --cflags`
    PHP_TYRANT_LIBS=`$PKG_CONFIG tokyotyrant --libs`
    PHP_TYRANT_VERSION_STRING=`$PKG_CONFIG tokyotyrant --modversion`

    PHP_EVAL_LIBLINE($PHP_TYRANT_LIBS, TOKYO_TYRANT_SHARED_LIBADD)
    PHP_EVAL_INCLINE($PHP_TYRANT_INCS)
    AC_MSG_RESULT([yes, ${PHP_TYRANT_VERSION_STRING}])
  else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the Tokyo Tyrant distribution])
  fi

  AC_MSG_CHECKING([that Tokyo Tyrant is at least version 1.1.24])
  `$PKG_CONFIG --atleast-version=1.1.24 tokyotyrant`
 
  if test $? != 0; then
    AC_MSG_ERROR(no)
  fi
  AC_MSG_RESULT(yes)

  PHP_TYRANT_VERSION_MASK=`echo ${PHP_TYRANT_VERSION_STRING} | awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`
  AC_DEFINE_UNQUOTED(PHP_TOKYO_TYRANT_VERSION, $PHP_TYRANT_VERSION_MASK, [ ])

dnl Tokyo Cabinet header

  dnl Tokyo Tyrant PKG_CONFIG
  if test "$PHP_TOKYO_CABINET_DIR" = "yes"; then
    export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/opt/lib/pkgconfig:/opt/local/lib/pkgconfig
  else
    export PKG_CONFIG_PATH=$PHP_TOKYO_CABINET_DIR:$PHP_TOKYO_CABINET_DIR/lib/pkgconfig
  fi

  AC_MSG_CHECKING([for Tokyo Cabinet])
  if test -x "$PKG_CONFIG" && $PKG_CONFIG --exists tokyocabinet; then
    PHP_CABINET_INCS=`$PKG_CONFIG tokyocabinet --cflags`
    PHP_CABINET_LIBS=`$PKG_CONFIG tokyocabinet --libs`
    PHP_CABINET_VERSION=`$PKG_CONFIG tokyocabinet --modversion`

    PHP_EVAL_LIBLINE($PHP_CABINET_LIBS, TOKYO_TYRANT_SHARED_LIBADD)
    PHP_EVAL_INCLINE($PHP_CABINET_INCS)
    AC_MSG_RESULT([yes, ${PHP_CABINET_VERSION}])
  else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the Tokyo Cabinet distribution])
  fi

  TOKYO_EXT_FILES="tokyo_tyrant.c tokyo_tyrant_funcs.c connection.c"

  if test "$PHP_TOKYO_TYRANT_SESSION" != "no"; then
    AC_DEFINE(HAVE_PHP_TOKYO_TYRANT_SESSION,1,[ ])
    TOKYO_EXT_FILES="${TOKYO_EXT_FILES} session_funcs.c server_pool.c failover.c session.c"
  fi

  PHP_NEW_EXTENSION(tokyo_tyrant, $TOKYO_EXT_FILES, $ext_shared)
  AC_DEFINE(HAVE_PHP_TOKYO_TYRANT,1,[ ])

  PHP_SUBST(TOKYO_TYRANT_SHARED_LIBADD)
  export PKG_CONFIG_PATH="$ORIG_PKG_CONFIG_PATH"
fi

