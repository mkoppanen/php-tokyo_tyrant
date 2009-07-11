PHP_ARG_WITH(tokyo-tyrant, whether to enable tokyo tyrant support,
[  --with-tokyo-tyrant[=DIR]       Enable tokyo tyrant support. DIR is the prefix to Tokyo Tyrant installation directory.], yes)

PHP_ARG_WITH(tokyo-cabinet-dir, directory of the Tokyo Cabinet installation,
[  --with-tokyo-cabinet-dir[=DIR]       DIR is the prefix to Tokyo Cabinet installation directory.], yes)

PHP_ARG_ENABLE(tokyo-tyrant-session, whether to enable tokyo tyrant session handler support,
[  --disable-tokyo-tyrant-session    Disables tokyo tyrant session handler support], yes, no)

if test "$PHP_TOKYO_TYRANT" != "no"; then

dnl Tokyo Tyrant parts

  AC_MSG_CHECKING(for tcrdb.h)

  for i in $PHP_TOKYO_TYRANT /usr /usr/local; do
    test -r $i/include/tcrdb.h && TYRANT_PREFIX=$i && break 
  done

  if test -z $TYRANT_PREFIX; then
    AC_MSG_ERROR(unable to find tcrdb.h)
  fi

  AC_MSG_RESULT(found in $TYRANT_PREFIX/include/tcrdb.h)

  AC_MSG_CHECKING(for tcrmgr binary)

  if test -r $TYRANT_PREFIX/bin/tcrmgr; then
    TYRANT_MGR_BIN=$TYRANT_PREFIX/bin/tcrmgr
  fi

  if test -z $TYRANT_MGR_BIN; then
    AC_MSG_ERROR(unable to find tcrmgr)
  fi

  AC_MSG_RESULT(found in $TYRANT_MGR_BIN)

  AC_MSG_CHECKING(for Tokyo Tyrant version)

  dnl Get the version, a bit of a hack
  TT_PHP_VERSION_STRING=`$TYRANT_MGR_BIN version | grep version | cut -d ' ' -f 4` 
  
  if test -z "$TT_PHP_VERSION_STRING"; then
    AC_MSG_ERROR(Unable to get tokyo tyrant version)
  fi

  TT_PHP_VERSION_MASK=`echo ${TT_PHP_VERSION_STRING} | awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2) * 1000 + $3;}'`

  if test $TT_PHP_VERSION_MASK -ge 1001024; then
    AC_MSG_RESULT(found version $TT_PHP_VERSION_STRING)
  else
    AC_MSG_ERROR(no. You need Tokyo Tyrant version >= 1.2.24)
  fi

  AC_DEFINE_UNQUOTED(PHP_TOKYO_TYRANT_VERSION, ${TT_PHP_VERSION_MASK}, [ ])

dnl Tokyo Cabinet header

  AC_MSG_CHECKING(for tcbdb.h)

  for i in $PHP_TOKYO_CABINET_DIR /usr /usr/local; do
    test -r $i/include/tcbdb.h && CABINET_PREFIX=$i && break 
  done

  if test -z $CABINET_PREFIX; then
    AC_MSG_ERROR(unable to find tcbdb.h)
  fi

  AC_MSG_RESULT(found in $CABINET_PREFIX/include/tcbdb.h)

  PHP_ADD_INCLUDE($TYRANT_PREFIX/include)
  PHP_ADD_INCLUDE($CABINET_PREFIX/include)

  PHP_ADD_LIBRARY_WITH_PATH(tokyocabinet, $CABINET_PREFIX/lib, TOKYO_TYRANT_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(tokyotyrant, $TYRANT_PREFIX/lib, TOKYO_TYRANT_SHARED_LIBADD)
  PHP_SUBST(TOKYO_TYRANT_SHARED_LIBADD)

  TOKYO_EXT_FILES="tokyo_tyrant.c tokyo_tyrant_funcs.c connection.c"

  if test "$PHP_TOKYO_TYRANT_SESSION" != "no"; then
    AC_DEFINE(HAVE_PHP_TOKYO_TYRANT_SESSION,1,[ ])
    TOKYO_EXT_FILES="${TOKYO_EXT_FILES} session_funcs.c server_pool.c failover.c session.c"
  fi
 
  PHP_NEW_EXTENSION(tokyo_tyrant, $TOKYO_EXT_FILES, $ext_shared)
  AC_DEFINE(HAVE_PHP_TOKYO_TYRANT,1,[ ])
fi

