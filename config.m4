PHP_ARG_WITH(tokyo-tyrant, whether to enable tokyo tyrant handler support,
[  --with-tokyo-tyrant[=DIR]       Enable tokyo tyrant handler support], yes)

if test "$PHP_TOKYO_TYRANT" != "no"; then

  for i in $PHP_TOKYO_TYRANT /usr /usr/local; do
    test -r $i/include/tcrdb.h && TYRANT_PREFIX=$i && break 
  done

  if test -z $TYRANT_PREFIX; then
    AC_MSG_ERROR(unable to find tcrdb.h)
  fi

  PHP_ADD_INCLUDE($TYRANT_PREFIX/include)
  PHP_ADD_LIBRARY_WITH_PATH(tokyocabinet, $TYRANT_PREFIX/lib, TOKYO_TYRANT_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(tokyotyrant, $TYRANT_PREFIX/lib, TOKYO_TYRANT_SHARED_LIBADD)

  PHP_SUBST(TOKYO_TYRANT_SHARED_LIBADD)
  PHP_NEW_EXTENSION(tokyo_tyrant, tokyo_tyrant.c tokyo_tyrant_funcs.c, $ext_shared)
  AC_DEFINE(HAVE_PHP_TOKYO_TYRANT,1,[ ])
fi

