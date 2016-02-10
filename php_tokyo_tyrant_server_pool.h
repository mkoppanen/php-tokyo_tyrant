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
#ifndef _PHP_TOKYO_TYRANT_SERVER_POOL_H_
# define _PHP_TOKYO_TYRANT_SERVER_POOL_H_

typedef struct _php_tt_server {
	zend_string *host;
	int port;
} php_tt_server;

typedef struct _php_tt_server_pool {
   HashTable servers;
} php_tt_server_pool;

php_tt_server_pool *php_tt_pool_init();

void php_tt_pool_append(php_tt_server_pool *pool, php_tt_server *server);

void php_tt_pool_append2(php_tt_server_pool *pool, char *host, int port);

void php_tt_pool_deinit(php_tt_server_pool *pool);

php_tt_server_pool *php_tt_pool_init2(const char *save_path);

int php_tt_pool_map(php_tt_server_pool *pool, char *key);

php_tt_server *php_tt_pool_get_server(php_tt_server_pool *pool, int idx);

#endif