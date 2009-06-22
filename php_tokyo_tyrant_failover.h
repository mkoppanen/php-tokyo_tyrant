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
#ifndef _PHP_TOKYO_TYRANT_FAILOVER_H_
# define _PHP_TOKYO_TYRANT_FAILOVER_H_

#define PHP_TT_INCR 1
#define PHP_TT_DECR 2
#define PHP_TT_GET  3

long php_tt_server_fail(int op, char *host, int port TSRMLS_DC);

void php_tt_server_fail_incr(char *host, int port TSRMLS_DC);

void php_tt_server_fail_decr(char *host, int port TSRMLS_DC);

zend_bool php_tt_server_ok(char *host, int port TSRMLS_DC);

void php_tt_health_check(TSRMLS_D);

#endif