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

#include "php_tokyo_tyrant.h"
#include "php_tokyo_tyrant_private.h"

#include "php_tokyo_tyrant_connection.h"

php_tt_conn *php_tt_conn_init(TSRMLS_D) 
{	
	php_tt_conn *conn = emalloc(sizeof(php_tt_conn));

	conn->connected  = 0;
	conn->persistent = 0;
	conn->rdb        = NULL;
	
	return conn;
}

void php_tt_conn_deinit(php_tt_conn *conn TSRMLS_DC) 
{	
	if (!conn->persistent && conn->rdb) {
		tcrdbdel(conn->rdb);
	}
	efree(conn);
}

char *php_tt_hash_key(char *host, int port, double timeout, int *key_len TSRMLS_DC)
{
	char *buffer = NULL;
	*key_len = spprintf(&buffer, strlen(host) + 256, "%s|%d|%f", host, port, timeout);
	return buffer;	
}

static void php_tt_alloc_pool(TSRMLS_D) 
{
	TOKYO_G(connections) = malloc(sizeof(HashTable)); 
	
	if (!TOKYO_G(connections))
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to allocate memory for the connection pool");
	
	zend_hash_init(TOKYO_G(connections), 0, NULL, NULL, 1);
}

static TCRDB *php_tt_get_persistent(char *host, int port, double timeout TSRMLS_DC)
{
	TCRDB **conn;
	int key_len;
	char *key = php_tt_hash_key(host, port, timeout, &key_len TSRMLS_CC);
	
	/* Make sure that connection pool is allocated */
	if (!TOKYO_G(connections))
		php_tt_alloc_pool(TSRMLS_C);

	if (zend_hash_find(TOKYO_G(connections), key, key_len + 1, (void **)&conn) == SUCCESS) {
		efree(key);
		return *conn;
	}
	
	efree(key);
	return NULL;
}

static zend_bool php_tt_set_persistent(char *host, int port, double timeout, TCRDB *conn TSRMLS_DC)
{
	int key_len;
	char *key = php_tt_hash_key(host, port, timeout, &key_len TSRMLS_CC);

	if (!TOKYO_G(connections))
		php_tt_alloc_pool(TSRMLS_C);

	if (zend_hash_update(TOKYO_G(connections), key, key_len + 1, (void *)&conn, sizeof(TCRDB *), NULL) == SUCCESS) {
		efree(key);
		return 0;
	}
	efree(key);
	return 1;
}

void php_tt_disconnect_ex(php_tt_conn *conn, zend_bool dealloc_rdb TSRMLS_DC) {
	if (conn->rdb && dealloc_rdb) {
		tcrdbdel(conn->rdb);
		conn->rdb = NULL;
	}
	conn->connected = 0;
}

zend_bool php_tt_connect_ex(php_tt_conn *conn, char *host, int port, double timeout, zend_bool persistent TSRMLS_DC) 
{
	int options = 0;
	
	assert(conn->connected == 0);

	if (persistent) {
		if ((conn->rdb = php_tt_get_persistent(host, port, timeout TSRMLS_CC))) {
			conn->persistent = 1;
			conn->connected  = 1;
			return 1;
		}
	}
	
	/* Init rdb object */
	conn->rdb = tcrdbnew();

	if (timeout <= 0.0) {
		timeout = TOKYO_G(default_timeout);
	}
	
	if (persistent) {
		options |= RDBTRECON;
	}

	/* Set options and try to connect */
	tcrdbtune(conn->rdb, timeout, options);

	if (!tcrdbopen(conn->rdb, host, port)) {
		conn->connected = 0;
		return 0;
	} else {
		conn->persistent = persistent;
		conn->connected  = 1;
		
		/* Persist */
		if (persistent) {
			php_tt_set_persistent(host, port, timeout, conn->rdb TSRMLS_CC);
		}
	}
	return 1;
}

zend_bool php_tt_connect(php_tokyo_tyrant_object *intern, char *host, int port, zval *params TSRMLS_DC) 
{
	zend_bool persistent = 0;
	int options = RDBTRECON;
	double timeout = TOKYO_G(default_timeout);
	
	/* Parse args if provided */
	if (params) {
		zval **param; 

		if (zend_hash_find(Z_ARRVAL_P(params), "persistent", sizeof("persistent"), (void **) &param) != FAILURE) {
			convert_to_boolean_ex(param);
			persistent = Z_BVAL_PP(param);
		}

		if (zend_hash_find(Z_ARRVAL_P(params), "timeout", sizeof("timeout"), (void **) &param) != FAILURE) {
			convert_to_double_ex(param);
			if (Z_DVAL_PP(param) > 0) {
				timeout = Z_DVAL_PP(param);
			}
		}

		if (zend_hash_find(Z_ARRVAL_P(params), "reconnect", sizeof("reconnect"), (void **) &param) != FAILURE) {
			convert_to_boolean_ex(param);
			if (!(Z_BVAL_PP(param))) {
				options = 0;
			}
		}
	}
	
	if (port <= 0) {
		port = PHP_TOKYO_TYRANT_DEFAULT_PORT;
	}
	
	if (intern->conn->connected) {
		php_tt_disconnect_ex(intern->conn, (intern->conn->persistent == 0) TSRMLS_CC);
	}
	return php_tt_connect_ex(intern->conn, host, port, timeout, persistent);	
}

zend_bool php_tt_connect2(php_tokyo_tyrant_object *intern, php_url *url TSRMLS_DC)
{
	int code = 0;
	if (url->query != NULL) { 
		zval *params; 

		MAKE_STD_ZVAL(params); 
		array_init(params); 
		
		sapi_module.treat_data(PARSE_STRING, estrdup(url->query), params TSRMLS_CC);
		code = php_tt_connect(intern, url->host, url->port, params TSRMLS_CC);

		zval_ptr_dtor(&params);
	} else {
		code = php_tt_connect(intern, url->host, url->port, NULL TSRMLS_CC);
	}
	return code;
}


