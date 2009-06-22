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
#include "php_tokyo_tyrant_session.h"

long php_tt_server_fail(int op, char *host, int port TSRMLS_DC)
{
    char *key = NULL;
    zend_bool result;
    int key_len;
    zval *fail_count, **ptrptr;

    if (!TOKYO_G(failures)) {
		TOKYO_G(failures) = malloc(sizeof(HashTable));

		if (!TOKYO_G(failures)) {
			return 0;
		}
		zend_hash_init(TOKYO_G(failures), 5, NULL, ZVAL_INTERNAL_PTR_DTOR, 1);
    }

    key = php_tt_hash_key(host, port, 0.0, &key_len TSRMLS_CC);

    if (zend_hash_find(TOKYO_G(failures), key, key_len + 1, (void **)&ptrptr) == SUCCESS) {
		fail_count = *ptrptr;

		if (op == PHP_TT_GET) {
			efree(key);
			return Z_LVAL_P(fail_count);
		} else if (op == PHP_TT_INCR) {
			Z_LVAL_P(fail_count)++;
		} else {
			Z_LVAL_P(fail_count)--;
		}
	} else {
		if (op == PHP_TT_GET) {
			efree(key);
			return 0;
		}
		
		fail_count = malloc(sizeof(zval));
		INIT_PZVAL(fail_count);
		ZVAL_LONG(fail_count, (op == PHP_TT_INCR) ? 1 : 0);
    }
    result = (zend_hash_update(TOKYO_G(failures), key, key_len + 1, (void *)&fail_count, sizeof(zval *), NULL) == SUCCESS);

    efree(key);
    return Z_LVAL_P(fail_count);
}

void php_tt_server_fail_incr(char *host, int port TSRMLS_DC)
{
	php_tt_server_fail(PHP_TT_INCR, host, port TSRMLS_CC);
}

void php_tt_server_fail_decr(char *host, int port TSRMLS_DC)
{
	php_tt_server_fail(PHP_TT_DECR, host, port TSRMLS_CC);
}

int php_tt_server_color(char *host, int port TSRMLS_DC)
{
	long fail_count = php_tt_server_fail(PHP_TT_GET, host, port TSRMLS_CC);
	
	if (fail_count <= 1) {
		return PHP_TT_COLOR_GREEN;
	} else if (fail_count > 1 && fail_count < 5) {
		return PHP_TT_COLOR_AMBER;
	} else {
		return PHP_TT_COLOR_RED;
	}
}