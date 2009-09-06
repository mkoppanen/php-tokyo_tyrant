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
#include "php_tokyo_tyrant_funcs.h"
#include "php_tokyo_tyrant_connection.h"

#include "SAPI.h" 
#include "php_variables.h"

/* {{{ void php_tt_object_init(php_tokyo_tyrant_object *intern TSRMLS_DC) */
void php_tt_object_init(php_tokyo_tyrant_object *intern TSRMLS_DC)
{
	intern->conn = php_tt_conn_init(TSRMLS_C);
}
/* }}} */

/* {{{ zend_bool php_tt_is_connected(php_tokyo_tyrant_object *intern TSRMLS_DC) */
zend_bool php_tt_is_connected(php_tokyo_tyrant_object *intern TSRMLS_DC)
{
	if (!intern->conn) {
		return 0;
	}
	
	return intern->conn->connected;
}
/* }}} */

char *php_tt_prefix(char *key, int key_len, int *new_len TSRMLS_DC)
{
	char *buffer;
	int buffer_len;
	
	if (!TOKYO_G(key_prefix)) {
		return estrdup(key);
	}
	
	buffer_len = strlen(TOKYO_G(key_prefix)) + key_len + 1;
	
	buffer    = emalloc(buffer_len);
	buffer[0] = '\0';
	
	*new_len = sprintf(buffer, "%s%s", TOKYO_G(key_prefix), key);
	return buffer;
}


/* {{{ TCMAP *php_tt_zval_to_tcmap(zval *array, zend_bool value_as_key TSRMLS_DC) */
TCMAP *php_tt_zval_to_tcmap(zval *array, zend_bool value_as_key TSRMLS_DC) 
{
	HashPosition pos;
	int buckets, new_len;
	TCMAP *map;
	char *kbuf;
	
	buckets = zend_hash_num_elements(Z_ARRVAL_P(array));
	map     = tcmapnew2(buckets);
	
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
			kbuf = php_tt_prefix(Z_STRVAL(tmpcopy), Z_STRLEN(tmpcopy), &new_len TSRMLS_CC);
			tcmapput2(map, kbuf, "");
			efree(kbuf);
		} else {
			char *arr_key;
	        uint arr_key_len;
	        ulong num_key;
			int n;
			zend_bool allocated = 0;
			
			n = zend_hash_get_current_key_ex(Z_ARRVAL_P(array), &arr_key, &arr_key_len, &num_key, 0, &pos);

			if (n == HASH_KEY_IS_LONG) {
				arr_key_len = spprintf(&arr_key, 78, "%ld", num_key);
				allocated = 1;
			}
			
			kbuf = php_tt_prefix(arr_key, arr_key_len, &new_len TSRMLS_CC);
			tcmapput2(map, kbuf, Z_STRVAL(tmpcopy));
			efree(kbuf);
			
			if (allocated) {
				efree(arr_key);
			}
		}
		
		zval_dtor(&tmpcopy); 
	}
	return map;
}
/* }}} */

/* {{{ void php_tt_tcmap_to_zval(TCMAP *map, zval *array TSRMLS_DC) */
void php_tt_tcmap_to_zval(TCMAP *map, zval *array TSRMLS_DC)
{
	const char *name;
	array_init(array);
	
	tcmapiterinit(map);
	while ((name = tcmapiternext2(map)) != NULL) {
		const char *kbuf = (char *)name;
		kbuf += TOKYO_G(key_prefix_len);
		
		add_assoc_string(array, (char *)kbuf, (char *)tcmapget2(map, name), 1); 
    }
}
/* }}} */

/* {{{ zend_bool php_tt_query_object_init(php_tokyo_tyrant_query_object *query, zval *parent TSRMLS_DC) */
zend_bool php_tt_query_object_init(php_tokyo_tyrant_query_object *query, zval *parent TSRMLS_DC)
{
	php_tokyo_tyrant_object *db = (php_tokyo_tyrant_object *)zend_object_store_get_object(parent TSRMLS_CC);
	
	query->qry = tcrdbqrynew(db->conn->rdb);
	
	if (!query->qry) {
		return 0;
	}
	
	query->conn    = db->conn;
	query->parent  = parent;
	query->res     = NULL;
#ifdef Z_REFCOUNT_P
	Z_SET_REFCOUNT_P(parent, Z_REFCOUNT_P(parent) + 1);
#else
	parent->refcount++;
#endif
	return 1;
}
/* }}} */

/* void php_tt_tclist_to_array(TCRDB *rdb, TCLIST *res, zval *container TSRMLS_DC) */
void php_tt_tclist_to_array(TCRDB *rdb, TCLIST *res, zval *container TSRMLS_DC)
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
				const char *kbuf = name;
				kbuf += TOKYO_G(key_prefix_len);
				
				add_assoc_string(row, (char *)kbuf, (char *)tcmapget2(cols, name), 1); 
			}
			tcmapdel(cols);
			add_assoc_zval(container, (char *)rbuf, row);
		}
	}
}
