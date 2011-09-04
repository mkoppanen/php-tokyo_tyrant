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

	buffer_len = TOKYO_G(key_prefix_len) + key_len;

	buffer = emalloc(buffer_len + 1);
	memset(buffer, 0, buffer_len + 1);
	
	/* non binary-safe prefix. */
	memcpy(buffer, TOKYO_G(key_prefix), TOKYO_G(key_prefix_len));
	memcpy(buffer + TOKYO_G(key_prefix_len), key, key_len);
	
	buffer[buffer_len] = '\0';
	*new_len = buffer_len;

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
			tcmapput(map, kbuf, new_len, "", sizeof(""));
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
				arr_key_len++;
				allocated = 1;
			}
			
			kbuf = php_tt_prefix(arr_key, arr_key_len - 1, &new_len TSRMLS_CC);
			tcmapput(map, kbuf, new_len, Z_STRVAL(tmpcopy), Z_STRLEN(tmpcopy));
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
	int name_len;
	const char *name;
	array_init(array);
	
	tcmapiterinit(map);
	while ((name = tcmapiternext(map, &name_len)) != NULL) {
		const char *buffer;
		int buffer_len;
		const char *kbuf = (char *)name;
		buffer = tcmapget(map, name, name_len, &buffer_len);

		if (buffer) {
			kbuf     += TOKYO_G(key_prefix_len);
			name_len -= TOKYO_G(key_prefix_len);
			add_assoc_stringl_ex(array, (char *)kbuf, name_len + 1, (char *)buffer, buffer_len, 1); 
		}
	}
}
/* }}} */

/* {{{ static zval* php_tt_ttstring_to_zval(const char* value, int value_len TSRMLS_DC) */
static zval* php_tt_ttstring_to_zval(const char* value, int value_len TSRMLS_DC)
{
	zval *retval;
	int iskey = 1;
	char* pcur = (char*)value;
	char* key = pcur;
	char* prev = pcur;
	char* end = pcur + value_len;

	MAKE_STD_ZVAL(retval);
	array_init(retval);

	if (!value || value_len < 4) {
		return retval;
	}

	if (!(*value) || *end) {
		return retval;
	}
	
	for (; pcur <= end; pcur++) {
		if (*pcur) {
			continue;
		}
		if (iskey) {
			key = prev;
			if (!(*key)) {
				break;
			}
			iskey = 0;
		} else {
			add_assoc_string(retval, (char *)key, (char *)prev, 1);
			iskey = 1;
		}
		prev = pcur + 1;
	}
	return retval;
}
/* }}} */


/* {{{ void php_tt_tcmapstring_to_zval(TCMAP *map, zval *array TSRMLS_DC) */
void php_tt_tcmapstring_to_zval(TCMAP *map, zval *array TSRMLS_DC)
{
	int name_len;
	const char *name;
	array_init(array);
	
	tcmapiterinit(map);
	while ((name = tcmapiternext(map, &name_len)) != NULL) {
		const char *buffer;
		int buffer_len;
		
		buffer = tcmapget(map, name, name_len, &buffer_len);

		if (buffer) {
			name     += TOKYO_G(key_prefix_len);
			name_len -= TOKYO_G(key_prefix_len);
			
			add_assoc_zval_ex(array, (char *)name, name_len + 1,
							php_tt_ttstring_to_zval((const char*) buffer, buffer_len TSRMLS_CC));
		}
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
#ifdef Z_ADDREF_P
	Z_ADDREF_P(parent);
#else
	parent->refcount++;
#endif
	return 1;
}
/* }}} */

/* {{{ zend_bool php_tt_iterator_object_init(php_tokyo_tyrant_iterator_object *iterator, zval *parent TSRMLS_DC) */
zend_bool php_tt_iterator_object_init(php_tokyo_tyrant_iterator_object *iterator, zval *parent TSRMLS_DC)
{
	php_tokyo_tyrant_object *db = (php_tokyo_tyrant_object *)zend_object_store_get_object(parent TSRMLS_CC);

	/* Check table first because it's also instanceof the parent */
	if (instanceof_function(Z_OBJCE_P(parent), php_tokyo_tyrant_table_sc_entry TSRMLS_CC)) {
		iterator->iterator_type = PHP_TOKYO_TYRANT_TABLE_ITERATOR;
	} else if (instanceof_function(Z_OBJCE_P(parent), php_tokyo_tyrant_sc_entry TSRMLS_CC)) {
		iterator->iterator_type = PHP_TOKYO_TYRANT_ITERATOR;
	} else {
		return 0;
	}

	if (!tcrdbiterinit(db->conn->rdb)) {
		return 0;
	}
	
	iterator->conn   = db->conn;
	iterator->parent = parent;

#ifdef Z_ADDREF_P
	Z_ADDREF_P(parent);
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
	int i, rsiz = 0;
	TCMAP *cols;
	
	for (i = 0; i < tclistnum(res); i++) {
		rbuf = tclistval(res, i, &rsiz);
		cols = tcrdbtblget(rdb, rbuf, rsiz);
		if (cols) {
			int name_len;
			zval *row;
			tcmapiterinit(cols);
			
			MAKE_STD_ZVAL(row);
			array_init(row);
			
			while ((name = tcmapiternext(cols, &name_len)) != NULL) {
				char *data;
				int data_len;
				const char *kbuf = name;
				kbuf     += TOKYO_G(key_prefix_len);
				name_len -= TOKYO_G(key_prefix_len);
				
				data = (char *)tcmapget(cols, name, name_len, &data_len);
				add_assoc_stringl_ex(row, (char *)kbuf, name_len + 1, data, data_len, 1); 
			}
			tcmapdel(cols);
			add_assoc_zval_ex(container, (char *)rbuf, rsiz + 1, row);
		}
	}
}
