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
#include "php_tokyo_tyrant_session.h"
#include "ext/standard/php_rand.h"

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

zend_bool php_tt_server_ok(char *host, int port TSRMLS_DC)
{
	long fail_count = php_tt_server_fail(PHP_TT_GET, host, port TSRMLS_CC);
	
	/* Server is always ok if failover is disabled */
	if (!TOKYO_G(allow_failover)) {
		return 1;
	}

	/* If this matches, execute a health check on servers and restore them to pool if they are fine */
	if ((php_rand(TSRMLS_C) % TOKYO_G(health_check_divisor)) == (php_rand(TSRMLS_C) % TOKYO_G(health_check_divisor))) {
		php_tt_health_check(TSRMLS_C); 
	}
	
	if (fail_count < TOKYO_G(fail_threshold)) {
		return 1;
	} else {
		return 0;
	}
}

static int _php_tt_check_servers(zval **ppzval TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key)
{
	char *key, host[4096];
	double timeout;
	int port;

	if (hash_key->nKeyLength == 0 || hash_key->nKeyLength >= 4096) {
		return ZEND_HASH_APPLY_REMOVE;
	}

	key = estrdup(hash_key->arKey);

	memset(host, '\0', 4096);
	if (sscanf(key, "%s %d %lf", host, &port, &timeout) == 3) {
		int value;
		uint_fast64_t index;
		TCRDB *rdb = tcrdbnew();
		
		value = tcrdbopen(rdb, host, port);
		
		if (!value) {
			efree(key);
			tcrdbdel(rdb);
			return ZEND_HASH_APPLY_KEEP;
		}
		
		index = tcrdbtblgenuid(rdb);
		tcrdbdel(rdb);
		
		if (index == -1) {
			efree(key);
			return ZEND_HASH_APPLY_KEEP;
		}
	}
	efree(key);
	return ZEND_HASH_APPLY_REMOVE;
}

void php_tt_health_check(TSRMLS_D) 
{
	if (TOKYO_G(failures) && zend_hash_num_elements(TOKYO_G(failures)) > 0) {
		zend_hash_apply_with_arguments(TOKYO_G(failures) TSRMLS_CC, (apply_func_args_t)_php_tt_check_servers, 0);
	}
}




