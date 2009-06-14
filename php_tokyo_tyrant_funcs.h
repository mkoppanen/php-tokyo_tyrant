/*
  +----------------------------------------------------------------------+
  | PHP Version 5 / Tokyo tyrant                                         |
  +----------------------------------------------------------------------+
  | Copyright (c) 2009 Mikko Koppanen                                    |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Mikko Koppanen <mkoppanen@php.net>                          |
  +----------------------------------------------------------------------+
*/

#ifndef _TOKYO_TYRANT_FUNCS_H_
# define _TOKYO_TYRANT_FUNCS_H_

#include "php_tokyo_tyrant.h"
#include "php_tokyo_tyrant_private.h"

void php_tokyo_tyrant_init_tt_object(php_tokyo_tyrant_object *intern TSRMLS_DC);

zend_bool php_tokyo_tyrant_init_tt_query_object(php_tokyo_tyrant_query_object *intern, zval *parent TSRMLS_DC);

zend_bool php_tokyo_tyrant_disconnect(php_tokyo_tyrant_conn *conn TSRMLS_DC);

zend_bool php_tokyo_tyrant_connect(php_tokyo_tyrant_object *intern, char *host, int port, zval *args TSRMLS_DC);

zend_bool php_tokyo_tyrant_connect_ex(php_tokyo_tyrant_object *intern, char *host, int port, double timeout, int options, zend_bool persistent TSRMLS_DC);

TCMAP *php_tokyo_tyrant_zval_to_tcmap(zval *array, zend_bool value_as_key TSRMLS_DC);

void php_tokyo_tyrant_tcmap_to_zval(TCMAP *map, zval *array TSRMLS_DC);

zend_bool php_tokyo_tyrant_is_connected(php_tokyo_tyrant_object *intern TSRMLS_DC);

void php_tokyo_tyrant_tclist_to_array(TCRDB *rdb, TCLIST *res, zval *container TSRMLS_DC);

int php_tokyo_tyrant_connection_from_uri(php_tokyo_tyrant_object *intern, php_url *url TSRMLS_DC);

#endif