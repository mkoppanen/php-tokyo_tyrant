/*
   +----------------------------------------------------------------------+
   | PHP Version 5 / Tokyo tyrant                                         |
   +----------------------------------------------------------------------+
   | Copyright (c) 2009-2010 Mikko Koppanen                               |
   +----------------------------------------------------------------------+
   | This source file is dual-licensed.                                   |
   | It is available under the terms of New BSD License that is bundled   |
   | with this package in the file LICENSE and available under the terms  |
   | of PHP license version 3.01. PHP license is bundled with this        |
   | package in the file LICENSE and can be obtained through the          |
   | world-wide-web at the following url:                                 |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Mikko Kopppanen <mkoppanen@php.net>                          |
   +----------------------------------------------------------------------+
*/
#ifndef _PHP_TOKYO_TYRANT_CONNECTION_H_
# define _PHP_TOKYO_TYRANT_CONNECTION_H_

#include "php_tokyo_tyrant_private.h"

php_tt_conn *php_tt_conn_init();

void php_tt_conn_deinit(php_tt_conn *conn);

zend_string *php_tt_hash_key(zend_string *host, int port, double timeout);

void php_tt_disconnect_ex(php_tt_conn *conn, zend_bool dealloc_rdb);

zend_bool php_tt_connect_ex(php_tt_conn *conn, zend_string *host, int port, double timeout, zend_bool persistent);

zend_bool php_tt_connect(php_tokyo_tyrant_object *intern, zend_string *host, int port, zval *params);

zend_bool php_tt_connect2(php_tokyo_tyrant_object *intern, php_url *url TSRMLS_DC);

#endif