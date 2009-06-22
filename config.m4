PHP_ARG_WITH(tokyo-tyrant, whether to enable tokyo tyrant support,
[  --with-tokyo-tyrant[=DIR]       Enable tokyo tyrant support], yes)

PHP_ARG_ENABLE(tokyo-tyrant-session, whether to enable tokyo tyrant session handler support,
[  --disable-tokyo-tyrant-session    Disables tokyo tyrant session handler support], yes, no)

if test "$PHP_TOKYO_TYRANT" != "no"; then

  AC_MSG_CHECKING(for tcrdb.h)

  for i in $PHP_TOKYO_TYRANT /usr /usr/local; do
    test -r $i/include/tcrdb.h && TYRANT_PREFIX=$i && break 
  done

  if test -z $TYRANT_PREFIX; then
    AC_MSG_ERROR(unable to find tcrdb.h)
  fi

  AC_MSG_RESULT(found in $TYRANT_PREFIX)

  dnl Check functionality
  PHP_CHECK_LIBRARY(tokyotyrant, tcrdbsetecode,
  [],[
    AC_MSG_ERROR([Tokyo Tyrant version >= 1.1.24 not found])
  ],[
    -L$TYRANT_PREFIX/lib
  ])

  PHP_ADD_LIBRARY_WITH_PATH(tokyocabinet, $TYRANT_PREFIX/lib, TOKYO_TYRANT_SHARED_LIBADD)
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

