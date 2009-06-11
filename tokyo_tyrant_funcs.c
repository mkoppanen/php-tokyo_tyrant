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

#include "php_tokyo_tyrant_funcs.h"

/* {{{ void php_tokyo_tyrant_init_tt_object(php_tokyo_tyrant_object *intern TSRMLS_DC) */
void php_tokyo_tyrant_init_tt_object(php_tokyo_tyrant_object *intern TSRMLS_DC)
{
	intern->conn = NULL;
}
/* }}} */

/* {{{ zend_bool php_tokyo_tyrant_is_connected(php_tokyo_tyrant_object *intern TSRMLS_DC) */
zend_bool php_tokyo_tyrant_is_connected(php_tokyo_tyrant_object *intern TSRMLS_DC)
{
	if (!intern->conn) {
		return 0;
	}
	
	return intern->conn->connected;
}
/* }}} */

/* {{{ void php_tokyo_tyrant_hash_void_dtor(php_tokyo_tyrant_conn **conn TSRMLS_DC) */
void php_tokyo_tyrant_hash_void_dtor(php_tokyo_tyrant_conn **conn TSRMLS_DC)
{
	/* For some reason this gets called too early, destroy manually in MSHUTDOWN */
}
/* }}} */

/* {{{ char *php_tokyo_tyrant_phash_key(char *host, int port, int *key_len, double timeout TSRMLS_DC) */
char *php_tokyo_tyrant_phash_key(char *host, int port, int *key_len, double timeout TSRMLS_DC)
{
	char *buffer;
	*key_len = spprintf(&buffer, strlen(host) + 256, "%s|%d|%f", host, port, timeout);
	return buffer;	
}
/* }}} */

/* void php_tokyo_tyrant_allocate_conn_pool(TSRMLS_D) */
void php_tokyo_tyrant_allocate_conn_pool(TSRMLS_D) 
{
	if (!TOKYO_G(connections)) {
		TOKYO_G(connections) = malloc(sizeof(HashTable)); 
		zend_hash_init(TOKYO_G(connections), 0, NULL, (dtor_func_t)php_tokyo_tyrant_hash_void_dtor, 1);
	}
}
/* }}} */

/* {{{ php_tokyo_tyrant_conn *php_tokyo_tyrant_load_pconn(char *host, int port, double timeout, int options TSRMLS_DC) */
php_tokyo_tyrant_conn *php_tokyo_tyrant_load_pconn(char *host, int port, double timeout, int options TSRMLS_DC)
{
	php_tokyo_tyrant_conn **conn;
	int pkey_len;
	char *pkey = php_tokyo_tyrant_phash_key(host, port, &pkey_len, timeout TSRMLS_CC);
	php_tokyo_tyrant_allocate_conn_pool(TSRMLS_C);
	
	if (zend_hash_find(TOKYO_G(connections), pkey, pkey_len + 1, (void **)&conn) == SUCCESS) {
		efree(pkey);
		return *conn;
	}
	
	efree(pkey);
	return NULL;
}
/* }}} */

/* {{{ zend_bool php_tokyo_tyrant_save_pconn(php_tokyo_tyrant_conn *conn TSRMLS_DC) */
zend_bool php_tokyo_tyrant_save_pconn(php_tokyo_tyrant_conn *conn TSRMLS_DC)
{
	int pkey_len;
	char *pkey = php_tokyo_tyrant_phash_key(conn->host, conn->port, &pkey_len, conn->timeout TSRMLS_CC);
	php_tokyo_tyrant_allocate_conn_pool(TSRMLS_C);
	
	if (zend_hash_update(TOKYO_G(connections), pkey, pkey_len + 1, (void *)&conn, sizeof(php_tokyo_tyrant_conn *), NULL) == SUCCESS) {
		efree(pkey);
		return 0;
	}
	efree(pkey);
	return 1;
}
/* }}} */

/* {{{ php_tokyo_tyrant_conn *php_tokyo_tyrant_conn_init(TSRMLS_D) */
php_tokyo_tyrant_conn *php_tokyo_tyrant_conn_init(TSRMLS_D)
{
	php_tokyo_tyrant_conn *conn = malloc(sizeof(php_tokyo_tyrant_conn));
	
	conn->rdb        = tcrdbnew();
	conn->host       = NULL;
	conn->port       = -1;
	conn->persistent = 0;
	conn->connected  = 0;
	
	conn->timeout    = 2.0;
	conn->options    = 0;
	
	return conn;
}
/* }}} */

/* {{{ zend_bool php_tokyo_tyrant_connect(php_tokyo_tyrant_object *intern, char *host, int port, zval *params TSRMLS_DC) */
zend_bool php_tokyo_tyrant_connect(php_tokyo_tyrant_object *intern, char *host, int port, zval *params TSRMLS_DC) 
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
	
	return php_tokyo_tyrant_connect_ex(intern, host, port, timeout, options, persistent);	
}
/* }}} */

/* {{{ zend_bool php_tokyo_tyrant_connect_tt_object(php_tokyo_tyrant_object *intern, char *host, int port, zend_bool persistent TSRMLS_DC) */
zend_bool php_tokyo_tyrant_connect_ex(php_tokyo_tyrant_object *intern, char *host, int port, double timeout, int options, zend_bool persistent TSRMLS_DC)
{
	/* Existing connection that is not persistent needs to be disconnected */
	if (php_tokyo_tyrant_is_connected(intern) && !intern->conn->persistent) {
		php_tokyo_tyrant_disconnect(intern->conn TSRMLS_CC);
	}
	
	if (persistent) {
		intern->conn = php_tokyo_tyrant_load_pconn(host, port, timeout, options TSRMLS_CC);
	}
	
	if (!intern->conn) {
		/* New connection object */
		intern->conn = php_tokyo_tyrant_conn_init(TSRMLS_C);
	
		/* Set options and timeout */
		tcrdbtune(intern->conn->rdb, timeout, options);

		if (!tcrdbopen(intern->conn->rdb, host, port)) {
			return 0;
		}

		/* TODO: the connection init should be all encapsulated.. hmm. conn_init ??? */
		intern->conn->host       = strdup(host);
		intern->conn->port       = port;
		intern->conn->connected  = 1;
		intern->conn->persistent = persistent;
		intern->conn->timeout    = timeout;
		intern->conn->options    = options;
		
		if (persistent) {
			php_tokyo_tyrant_save_pconn(intern->conn TSRMLS_CC);
		}
	}
	return 1;
}
/* }}} */

/* {{{ zend_bool php_tokyo_tyrant_disconnect(php_tokyo_tyrant_object *intern TSRMLS_DC) */
zend_bool php_tokyo_tyrant_disconnect(php_tokyo_tyrant_conn *conn TSRMLS_DC) 
{
	if (conn->connected) {
		tcrdbclose(conn->rdb);
	}
	
	tcrdbdel(conn->rdb);
	
	if (conn->host) {
		free(conn->host);
		conn->host = NULL;
	}
	
	conn->rdb = NULL;
	conn->connected = 0;
	return 1;
}
/* }}} */

/* {{{ TCMAP *php_tokyo_tyrant_zval_to_tcmap(zval *array, zend_bool value_as_key TSRMLS_DC) */
TCMAP *php_tokyo_tyrant_zval_to_tcmap(zval *array, zend_bool value_as_key TSRMLS_DC) 
{
	HashPosition pos;
	int buckets = zend_hash_num_elements(Z_ARRVAL_P(array));
	TCMAP *map = tcmapnew2(buckets);
	
	if (!map) {
		return NULL;
	}
	
	for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(array), &pos);
		zend_hash_has_more_elements_ex(Z_ARRVAL_P(array), &pos) == SUCCESS;
		zend_hash_move_forward_ex(Z_ARRVAL_P(array), &pos)) {

		zval **ppzval, tmpcopy;

		if (zend_hash_get_current_data_ex(Z_ARRVAL_P(array), (void**)&ppzval, &pos) == FAILURE) {
			continue;
		}
	
		tmpcopy = **ppzval;
		zval_copy_ctor(&tmpcopy);
		INIT_PZVAL(&tmpcopy);
		convert_to_string(&tmpcopy);
		
		if (value_as_key) {
			tcmapput2(map, Z_STRVAL(tmpcopy), "");
		} else {
			char *arr_key;
	        uint arr_key_len;
	        ulong num_key;
			int n;
			zend_bool allocated = 0;
			
			n = zend_hash_get_current_key_ex(Z_ARRVAL_P(array), &arr_key, &arr_key_len, &num_key, 0, &pos);

			if (n == HASH_KEY_IS_LONG) {
				spprintf(&arr_key, 78, "%ld", num_key);
				allocated = 1;
			}
			
			tcmapput2(map, arr_key, Z_STRVAL(tmpcopy));
			
			if (allocated) {
				efree(arr_key);
			}
		}
		
		zval_dtor(&tmpcopy); 
	}
	return map;
}
/* }}} */

/* {{{ void php_tokyo_tyrant_tcmap_to_zval(TCMAP *map, zval *array TSRMLS_DC) */
void php_tokyo_tyrant_tcmap_to_zval(TCMAP *map, zval *array TSRMLS_DC)
{
	const char *name;
	array_init(array);
	
	tcmapiterinit(map);
	while ((name = tcmapiternext2(map)) != NULL) {
		add_assoc_string(array, (char *)name, (char *)tcmapget2(map, name), 1); 
    }
}
/* }}} */

/* {{{ zend_bool php_tokyo_tyrant_init_tt_query_object(php_tokyo_tyrant_query_object *query, zval *parent TSRMLS_DC) */
zend_bool php_tokyo_tyrant_init_tt_query_object(php_tokyo_tyrant_query_object *query, zval *parent TSRMLS_DC)
{
	php_tokyo_tyrant_object *db = (php_tokyo_tyrant_object *)zend_object_store_get_object(parent TSRMLS_CC);
	
	query->qry = tcrdbqrynew(db->conn->rdb);
	
	if (!query->qry) {
		return 0;
	}
	
	query->conn    = db->conn;
	query->parent  = parent;
	query->res     = NULL;
	
	parent->refcount++;
	return 1;
}
/* }}} */

/* void php_tokyo_tyrant_tclist_to_array(TCRDB *rdb, TCLIST *res, zval *container TSRMLS_DC) */
void php_tokyo_tyrant_tclist_to_array(TCRDB *rdb, TCLIST *res, zval *container TSRMLS_DC)
{
	const char *rbuf, *name;
	int i, rsiz;
	TCMAP *cols;
	
	for (i = 0; i < tclistnum(res); i++) {
		rbuf = tclistval(res, i, &rsiz);
		cols = tcrdbtblget(rdb, rbuf, rsiz);
		if (cols) {
			zval *row;
			tcmapiterinit(cols);
			
			MAKE_STD_ZVAL(row);
			array_init(row);
			
			while ((name = tcmapiternext2(cols)) != NULL) {
				add_assoc_string(row, (char *)name, (char *)tcmapget2(cols, name), 1); 
			}
			tcmapdel(cols);
			add_assoc_zval(container, (char *)rbuf, row);
		}
	}
}
