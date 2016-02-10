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

/* {{{ void php_tt_object_init(php_tokyo_tyrant_object *intern) */
void php_tt_object_init(php_tokyo_tyrant_object *intern)
{
	intern->conn = php_tt_conn_init(TSRMLS_C);
}
/* }}} */

/* {{{ zend_bool php_tt_is_connected(php_tokyo_tyrant_object *intern) */
zend_bool php_tt_is_connected(php_tokyo_tyrant_object *intern)
{
	if (!intern->conn) {
		return 0;
	}
	
	return intern->conn->connected;
}
/* }}} */

zend_string *php_tt_prefix(zend_string *key)
{
	smart str = {0};

	smart_str_appendl(&str, TOKYO_G(key_prefix)->val, TOKYO_G(key_prefix)->len);
	smart_str_appendl(&str, key->val, key->len);
	smart_str_0(&str);

	return str.s;
}

static
zend_bool s_prepare_map(HashTable *original, zend_bool value_as_key, HashTable *hash)
{
	HashTable values;
	zend_string *str_key;
	unsigned long num_key;
	zval *zv;

	ZEND_HASH_FOREACH_KEY_VAL(original, num_key, str_key, zv) {

		zend_string *key, *value = NULL;

		if (value_as_key) {
			key = zval_get_string(&zv);
		}
		else {
			if (str_key) {
				key = str_key;
				zend_string_addref(str_key);
			}
			else {
				smart_str str = {0};
				smart_str_append_long (&str, (long) num_key);
				smart_str_0 (&str);

				key = str.s;
			}
			value = zval_get_string (zv);
		}

		if (!key) {
			continue;
		}

		if (key->len) {

			/* Need prefix? */
			smart str = {0};

			if (TOKYO_G(key_prefix)->len) {
				smart_str_appendl(&str, TOKYO_G(key_prefix)->val, TOKYO_G(key_prefix)->len);
			}
			smart_str_appendl(&str, key->val, key->len);
			smart_str_0(&str);

			tcmapput(map, str.s->val, str.s->len, value ? value->val : NULL, value ? value->len : 0);
		}

		zend_string_release (key);
		if (value) {
			zend_string_release(value);
		}
	} ZEND_HASH_FOREACH_END();
	return 1;
}



/* {{{ TCMAP *php_tt_zval_to_tcmap(zval *array, zend_bool value_as_key) */
TCMAP *php_tt_zval_to_tcmap(zval *array, zend_bool value_as_key) 
{
	zend_string *str_key;
	unsigned long num_key;
	zval *zv;
	TCMAP *map;

	buckets = zend_hash_num_elements(Z_ARRVAL_P(array));
	map     = tcmapnew2(buckets);
	
	if (!map) {
		return NULL;
	}

	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(array), num_key, str_key, zv) {

		zend_string *key, *value = NULL;

		if (value_as_key) {
			key = zval_get_string(&zv);
		}
		else {
			if (str_key) {
				key = str_key;
				zend_string_addref(str_key);
			}
			else {
				smart_str str = {0};
				smart_str_append_long (&str, (long) num_key);
				smart_str_0 (&str);

				key = str.s;
			}
			value = zval_get_string (zv);
		}

		if (!key) {
			continue;
		}

		if (key->len) {

			/* Need prefix? */
			smart str = {0};

			if (TOKYO_G(key_prefix)->len) {
				smart_str_appendl(&str, TOKYO_G(key_prefix)->val, TOKYO_G(key_prefix)->len);
			}
			smart_str_appendl(&str, key->val, key->len);
			smart_str_0(&str);

			tcmapput(map, str.s->val, str.s->len, value ? value->val : NULL, value ? value->len : 0);
		}

		zend_string_release (key);
		if (value) {
			zend_string_release(value);
		}

	} ZEND_HASH_FOREACH_END();

	return map;
}
/* }}} */

/* {{{ void php_tt_tcmap_to_zval(TCMAP *map, zval *return_value) */
void php_tt_tcmap_to_zval(TCMAP *map, zval *return_value)
{
	const char *name;
	int name_len = 0;

	array_init(return_value);
	tcmapiterinit(map);

	while ((name = tcmapiternext(map, &name_len)) != NULL) {

		const char *value;
		int value_len;

		if (name && name_len) {

			value = tcmapget(map, name, name_len, &value_len);

			name     += TOKYO_G(key_prefix)->len;
			name_len -= TOKYO_G(key_prefix)->len;

			add_assoc_stringl_ex(return_value, name, name_len, value, value_len);
		}
	}
}
/* }}} */

/* {{{ static zval* php_tt_ttstring_to_zval(const char* value, int value_len) */
static
zval *php_tt_ttstring_to_zval(const char* value, int value_len)
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


/* {{{ void php_tt_tcmapstring_to_zval(TCMAP *map, zval *return_value) */
void php_tt_tcmapstring_to_zval(TCMAP *map, zval *return_value)
{
	int name_len;
	const char *name;
	array_init(array);
	
	tcmapiterinit(map);

	while ((name = tcmapiternext(map, &name_len)) != NULL) {

		const char *value;
		int value_len;

		if (name && name_len) {

			value = tcmapget(map, name, name_len, &value_len);

			name     += TOKYO_G(key_prefix)->len;
			name_len -= TOKYO_G(key_prefix)->len;

			add_assoc_zval_ex(array, name, name_len, php_tt_ttstring_to_zval(value, value_len));
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
