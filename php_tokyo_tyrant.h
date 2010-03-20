/*
   +----------------------------------------------------------------------+
   | PHP Version 5 / Tokyo tyrant                                         |
   +----------------------------------------------------------------------+
   | Copyright (c) 2009-2010 Mikko Koppanen                               |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Mikko Kopppanen <mkoppanen@php.net>                          |
   +----------------------------------------------------------------------+
*/
#ifndef _PHP_TOKYO_TYRANT_H_
# define _PHP_TOKYO_TYRANT_H_

#define PHP_TOKYO_TYRANT_EXTNAME "tokyo_tyrant"
#define PHP_TOKYO_TYRANT_EXTVER "@PACKAGE_VERSION@"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef ZTS
# include "TSRM.h"
#endif

#include "php.h"

extern zend_module_entry tokyo_tyrant_module_entry;
#define phpext_tokyo_tyrant_ptr &tokyo_tyrant_module_entry

#ifdef HAVE_PHP_TOKYO_TYRANT_SESSION
#include "ext/session/php_session.h"

extern ps_module ps_mod_tokyo_tyrant;
#define ps_tokyo_tyrant_ptr &ps_mod_tokyo_tyrant

PS_FUNCS(tokyo_tyrant);
#endif

#endif /* _PHP_TOKYO_TYRANT_H_ */
