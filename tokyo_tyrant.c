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
#include "ext/standard/info.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"

#if PHP_MAJOR_VERSION >=5 && PHP_MINOR_VERSION >= 3
# include "ext/date/php_date.h"
#endif

zend_class_entry *php_tokyo_tyrant_sc_entry;
zend_class_entry *php_tokyo_tyrant_table_sc_entry;
zend_class_entry *php_tokyo_tyrant_query_sc_entry;
zend_class_entry *php_tokyo_tyrant_iterator_sc_entry;
zend_class_entry *php_tokyo_tyrant_exception_sc_entry;

static zend_object_handlers tokyo_tyrant_object_handlers;
static zend_object_handlers tokyo_tyrant_table_object_handlers;
static zend_object_handlers tokyo_tyrant_query_object_handlers;
static zend_object_handlers tokyo_tyrant_iterator_object_handlers;

ZEND_DECLARE_MODULE_GLOBALS(tokyo_tyrant);

/* {{{ TokyoTyrant TokyoTyrant::__construct([string host, int port, array params]);
	The constructor
	@throws TokyoTyrantException if host is set and database connection fails
*/
PHP_METHOD(tokyotyrant, __construct) 
{
	php_tokyo_tyrant_object *intern;
	char *host = NULL;
	int host_len, port = PHP_TOKYO_TYRANT_DEFAULT_PORT;
	zval *params = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sla!", &host, &host_len, &port, &params) == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_OBJECT;

	if (host && !php_tt_connect(intern, host, port, params TSRMLS_CC)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to connect to database: %s");
	}
	
	return;
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::connect(string host, int port[, array params]);
	Connect to a database
	@throws TokyoTyrantException if disconnecting previous connection fails
	@throws TokyoTyrantException if connection fails
*/
PHP_METHOD(tokyotyrant, connect) 
{
	php_tokyo_tyrant_object *intern;
	char *host = NULL;
	int host_len, port = PHP_TOKYO_TYRANT_DEFAULT_PORT;
	zval *params = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|la!", &host, &host_len, &port, &params) == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_OBJECT;
	
	if (!php_tt_connect(intern, host, port, params TSRMLS_CC)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to connect to database: %s");
	}

	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::connectUri(string uri);
	Connect to a database using uri tcp://hostname:port?persistent=1&timeout=2.2&reconnect=true
	@throws TokyoTyrantException if disconnecting previous connection fails
	@throws TokyoTyrantException if connection fails
*/
PHP_METHOD(tokyotyrant, connecturi) 
{
	php_tokyo_tyrant_object *intern;
	char *uri;
	int uri_len;
	php_url *url;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &uri, &uri_len) == FAILURE) {
		return;
	}
	
	if (!(url = php_url_parse(uri))) {
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Failed to parse the uri");
	}
	
	intern = PHP_TOKYO_OBJECT;

	if (!php_tt_connect2(intern, url TSRMLS_CC)) {
		php_url_free(url);
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to connect to database: %s");
	}
	php_url_free(url);
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ static int _php_tt_real_write(TCRDB *rdb, long type, char *key, int key_len, char *value, int value_len TSRMLS_DC) */
static int _php_tt_real_write(TCRDB *rdb, long type, char *key, int key_len, char *value, int value_len TSRMLS_DC)
{
	int code = 0;
	int new_len;
	
	char *kbuf = php_tt_prefix(key, key_len, &new_len TSRMLS_CC);

	switch (type) {
		
		case PHP_TOKYO_TYRANT_OP_PUT:
			code = tcrdbput(rdb, kbuf, new_len, value, value_len);
		break;
		
		case PHP_TOKYO_TYRANT_OP_PUTKEEP:
			code = tcrdbputkeep(rdb, kbuf, new_len, value, value_len);
		break;
		
		case PHP_TOKYO_TYRANT_OP_PUTCAT:
			code = tcrdbputcat(rdb, kbuf, new_len, value, value_len);
		break;
		
		case PHP_TOKYO_TYRANT_OP_PUTNR:
			code = tcrdbputnr(rdb, kbuf, new_len, value, value_len);
		break;

		case PHP_TOKYO_TYRANT_OP_OUT:
			code = tcrdbout(rdb, kbuf, new_len);
		break;
		
		case PHP_TOKYO_TYRANT_OP_TBLOUT:
			code = tcrdbtblout(rdb, kbuf, new_len);
		break;
	}
	efree(kbuf);
	
	/* TODO: maybe add strict flag but ignore non-existent keys for now */
	if (!code) {
		if (tcrdbecode(rdb) == TTENOREC) {
			code = 1;
		}
	}
	return code;
}
/* }}} */

/* {{{ static int _php_tt_op_many(zval **ppzval TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key) */
static int _php_tt_op_many(zval **ppzval TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key)
{
	zval tmpcopy;
	php_tokyo_tyrant_object *intern;
	int type, *code, key_len;
	char *key, key_buf[256];

    if (num_args != 3) {
		return 0;
    }

	intern = va_arg(args, php_tokyo_tyrant_object *);
	type   = va_arg(args, int);
	code   = va_arg(args, int *);
	
	if (hash_key->nKeyLength == 0) {
		key_buf[0] = '\0';
		key_len    = snprintf(key_buf, 256, "%ld", hash_key->h);
		key        = key_buf;
	} else {
		key     = hash_key->arKey;
		key_len = hash_key->nKeyLength - 1;
	}

	tmpcopy = **ppzval;
	zval_copy_ctor(&tmpcopy);
	INIT_PZVAL(&tmpcopy);
	convert_to_string(&tmpcopy);

	if (type == PHP_TOKYO_TYRANT_OP_OUT || type == PHP_TOKYO_TYRANT_OP_TBLOUT) {
		*code = _php_tt_real_write(intern->conn->rdb, type, Z_STRVAL(tmpcopy), Z_STRLEN(tmpcopy), NULL, 0 TSRMLS_CC);
	} else {
		*code = _php_tt_real_write(intern->conn->rdb, type, key, key_len, Z_STRVAL(tmpcopy), Z_STRLEN(tmpcopy) TSRMLS_CC);
	}

	zval_dtor(&tmpcopy);
	
	if (!(*code)) {
		return ZEND_HASH_APPLY_STOP;
	}
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

/* {{{ static void _php_tt_write_wrapper(INTERNAL_FUNCTION_PARAMETERS, long type) */
static void _php_tt_write_wrapper(INTERNAL_FUNCTION_PARAMETERS, long type) 
{
	php_tokyo_tyrant_object *intern;
	zval *key, *value = NULL;
	int code;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z!", &key, &value) == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	if (Z_TYPE_P(key) == IS_ARRAY) {
		zend_hash_apply_with_arguments(Z_ARRVAL_P(key) TSRMLS_CC, (apply_func_args_t) _php_tt_op_many, 3, intern, type, &code);
		
		if (!code) {
			if (type == PHP_TOKYO_TYRANT_OP_OUT || type == PHP_TOKYO_TYRANT_OP_TBLOUT) {
				PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to remove the records: %s");
			} else {
				PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to put the records: %s");
			}
		}
		
	} else {
		convert_to_string(key);
		
		if (type == PHP_TOKYO_TYRANT_OP_OUT || type == PHP_TOKYO_TYRANT_OP_TBLOUT) {
			if (!_php_tt_real_write(intern->conn->rdb, type, Z_STRVAL_P(key), Z_STRLEN_P(key), NULL, 0 TSRMLS_CC)) {
				zend_throw_exception_ex(php_tokyo_tyrant_exception_sc_entry, tcrdbecode(intern->conn->rdb) TSRMLS_CC, "Unable to remove the record '%s': %s", 
										Z_STRVAL_P(key), tcrdberrmsg(tcrdbecode(intern->conn->rdb)));
				return;
			}
		} else {
			if (!value) {
				PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to store the record: no value provided");
			}
			convert_to_string(value);
			
			if (!_php_tt_real_write(intern->conn->rdb, type, Z_STRVAL_P(key), Z_STRLEN_P(key), Z_STRVAL_P(value), Z_STRLEN_P(value) TSRMLS_CC)) {
				zend_throw_exception_ex(php_tokyo_tyrant_exception_sc_entry, tcrdbecode(intern->conn->rdb) TSRMLS_CC, "Unable to store the record '%s': %s", 
										Z_STRVAL_P(key), tcrdberrmsg(tcrdbecode(intern->conn->rdb)));
				return;
			}
		}
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::put(mixed key[, string value]);
	Stores a string record
	@throws TokyoTyrantException if put fails
	@throws TokyoTyrantException if key is not an array and not value is provided
*/
PHP_METHOD(tokyotyrant, put) 
{
	_php_tt_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_PUT);
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::putkeep(mixed key[, string value]);
	Stores a string record if not exists
	@throws TokyoTyrantException if put fails
	@throws TokyoTyrantException if key is not an array and not value is provided
*/
PHP_METHOD(tokyotyrant, putkeep) 
{
	_php_tt_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_PUTKEEP);
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::putcat(mixed key[, string value]);
	Concatenate a string value
	@throws TokyoTyrantException if put fails
	@throws TokyoTyrantException if key is not an array and not value is provided
*/
PHP_METHOD(tokyotyrant, putcat) 
{
	_php_tt_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_PUTCAT);
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::putnr(mixed key[, string value]);
	Put a record and don't wait for server to respond
	@throws TokyoTyrantException if put fails
	@throws TokyoTyrantException if key is not an array and not value is provided
*/
PHP_METHOD(tokyotyrant, putnr) 
{
	_php_tt_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_PUTNR);
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::putshl(string key, string value, int width);
	Concatenate a string value and shift to left
	@throws TokyoTyrantException if put fails
	@throws TokyoTyrantException if key is not an array and not value is provided
*/
PHP_METHOD(tokyotyrant, putshl) 
{
	php_tokyo_tyrant_object *intern;
	char *kbuf, *key, *value;
	int code, key_len, value_len, new_len;
	long width;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssl", &key, &key_len, &value, &value_len, &width) == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	/* Create a prefix */
	kbuf = php_tt_prefix(key, key_len, &new_len TSRMLS_CC);
	code = tcrdbputshl(intern->conn->rdb, kbuf, new_len, value, value_len, width);
	efree(kbuf);
	
	if (!code) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to putshl the record: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::out(mixed key);
	Removes a record
	@throws TokyoTyrantException if remove fails
*/
PHP_METHOD(tokyotyrant, out) 
{
	_php_tt_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_OUT);
}
/* }}} */

/* {{{ TokyoTyrantIterator TokyoTyrant::getIterator();
	Get iterator object
	@throws TokyoTyrantException if not connected to a database
*/
PHP_METHOD(tokyotyrant, getiterator) 
{
	php_tokyo_tyrant_object *intern;
	php_tokyo_tyrant_iterator_object *intern_iterator;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	object_init_ex(return_value, php_tokyo_tyrant_iterator_sc_entry);
	intern_iterator = (php_tokyo_tyrant_iterator_object *)zend_object_store_get_object(return_value TSRMLS_CC); 

	if (!php_tt_iterator_object_init(intern_iterator, getThis() TSRMLS_CC)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Failed to initialize the iterator: %s");
	}
	return;
}
/* }}} */

/* {{{ mixed TokyoTyrant::get(mixed key);
	Gets string record(s)
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyrant, get) 
{
	php_tokyo_tyrant_object *intern;
	zval *key;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &key) == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	if (Z_TYPE_P(key) == IS_ARRAY) {
		TCMAP *map = php_tt_zval_to_tcmap(key, 1 TSRMLS_CC);
		tcrdbget3(intern->conn->rdb, map);

		if (!map) {
			PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the records: %s");
		}

		php_tt_tcmap_to_zval(map, return_value TSRMLS_CC);		
		tcmapdel(map);
	} else {
		zval tmpcopy;
		char *value, *kbuf;
		int new_len, value_len;
		
		tmpcopy = *key;
		zval_copy_ctor(&tmpcopy);
		INIT_PZVAL(&tmpcopy);
		convert_to_string(&tmpcopy);
	
		kbuf = php_tt_prefix(Z_STRVAL(tmpcopy), Z_STRLEN(tmpcopy), &new_len TSRMLS_CC);
		value = tcrdbget(intern->conn->rdb, kbuf, new_len, &value_len);
		zval_dtor(&tmpcopy);
		efree(kbuf);
		
		if (!value) {
			PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the record: %s");
		}
		RETVAL_STRINGL(value, value_len, 1);
		free(value);
	}
	return;
}
/* }}} */

/* {{{ int TokyoTyrant::add(string key, mixed value[, CONST type]);
	Add to a key
	@throws TokyoTyrantException if sync fails
*/
PHP_METHOD(tokyotyrant, add) 
{
	php_tokyo_tyrant_object *intern;
	char *key, *kbuf;
	int key_len = 0, new_len, retint;
	long type = 0;
	zval *value;
	
	double retdouble;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|l", &key, &key_len, &value, &type) == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	kbuf = php_tt_prefix(key, key_len, &new_len TSRMLS_CC);

	if (type == 0) {
		if (Z_TYPE_P(value) == IS_DOUBLE) {
			type = PHP_TOKYO_TYRANT_RECTYPE_DOUBLE;
		} else {
			type = PHP_TOKYO_TYRANT_RECTYPE_INT;
		}
	}
	
	switch (type) {
		
		case PHP_TOKYO_TYRANT_RECTYPE_INT:
			convert_to_long(value);
			retint = tcrdbaddint(intern->conn->rdb, kbuf, new_len, Z_LVAL_P(value));
			if (retint == INT_MIN) {
				RETURN_NULL();
			}
			RETVAL_LONG(retint);
		break;
		
		case PHP_TOKYO_TYRANT_RECTYPE_DOUBLE:
			convert_to_double(value);
			retdouble = tcrdbadddouble(intern->conn->rdb, kbuf, new_len, Z_DVAL_P(value));
			if (isnan(retdouble)) {
				RETURN_NULL();
			}
			RETVAL_DOUBLE(retdouble);
		break;
		
		default:
			efree(kbuf);
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unknown record type");
		break;	
	}
	efree(kbuf);
	return;
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::sync();
	Flushes the changes to disk
	@throws TokyoTyrantException if sync fails
*/
PHP_METHOD(tokyotyrant, sync) 
{
	php_tokyo_tyrant_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	if (!tcrdbsync(intern->conn->rdb)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to synchronise the database: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::tune(float timeout[, int options]);
	Tunes to options / timeout
	@throws TokyoTyrantException if sync fails
*/
PHP_METHOD(tokyotyrant, tune) 
{
	php_tokyo_tyrant_object *intern;
	double timeout;
	long options = RDBTRECON;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d|l", &timeout, &options) == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	if (!tcrdbtune(intern->conn->rdb, timeout, options)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to tune the database options: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::vanish();
	Empties the remote database
*/
PHP_METHOD(tokyotyrant, vanish) 
{
	php_tokyo_tyrant_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	if (!tcrdbvanish(intern->conn->rdb)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to empty the database: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ string TokyoTyrant::stat();
	Gets the status string of the remote database
*/
PHP_METHOD(tokyotyrant, stat) 
{
	php_tokyo_tyrant_object *intern;
	char *status = NULL, *ptr;
	char k[1024], v[1024];
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	status = tcrdbstat(intern->conn->rdb);
	
	if (!status) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the status string: %s");
	}
	
	array_init(return_value);
	
	/* add elements */
	ptr = strtok(status, "\n");
	while (ptr) {
		if (strlen(ptr) >= 1024) {
			continue;
		}
		memset(k, '\0', 1024);
		memset(v, '\0', 1024);

		if (sscanf(ptr, "%s %s", k, v) != 2) {
			continue;
		}
		add_assoc_string(return_value, k, v, 1);
		ptr = strtok(NULL, "\n");
	}
	free(status);
	return;
}
/* }}} */

/* {{{ int TokyoTyrant::size(string key);
	Get the size of a row
*/
PHP_METHOD(tokyotyrant, size)
{
	php_tokyo_tyrant_object *intern;
	char *key, *kbuf;
	int key_len, new_len;
	long rec_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	kbuf = php_tt_prefix(key, key_len, &new_len TSRMLS_CC);
	rec_len = tcrdbvsiz2(intern->conn->rdb, kbuf);
	efree(kbuf);
	
	if (rec_len == -1) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the record size: %s");
	}
	RETURN_LONG(rec_len);
}
/* }}} */

/* {{{ int TokyoTyrant::num();
	Number of records in the database
*/
PHP_METHOD(tokyotyrant, num)
{
	php_tokyo_tyrant_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	RETURN_LONG(tcrdbrnum(intern->conn->rdb));
}
/* }}} */

/* {{{ array TokyoTyrant::fwmkeys(string prefix, int max_records) */
PHP_METHOD(tokyotyrant, fwmkeys)
{
	php_tokyo_tyrant_object *intern;
	char *prefix, *kbuf;
	int prefix_len, new_len;
	long max_recs;
	TCLIST *res = NULL;
	const char *rbuf;
	int i, rsiz;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &prefix, &prefix_len, &max_recs) == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	res = tcrdbfwmkeys2(intern->conn->rdb, prefix, max_recs);

	array_init(return_value);
	
	for (i = 0; i < tclistnum(res); i++) {
		rbuf = tclistval(res, i, &rsiz);
		kbuf = php_tt_prefix((char *)rbuf, rsiz, &new_len TSRMLS_CC);
		add_next_index_stringl(return_value, kbuf, new_len, 0);
	}
	
	tclistdel(res);
	return;
}
/* }}} */

/* {{{ string TokyoTyrant::ext(string name, CONST opts, string key, string value);
	Get the size of a row
*/
PHP_METHOD(tokyotyrant, ext)
{
	php_tokyo_tyrant_object *intern;
	char *name, *key, *value, *response;
	int name_len, key_len, value_len;
	long opts;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slss", &name, &name_len, &opts, &key, &key_len, &value, &value_len) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	response = tcrdbext2(intern->conn->rdb, name, opts, key, value);

	if (!response) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to execute the remote script: %s");
	}
	RETVAL_STRING(response, 1);
	free(response);
	return;
}
/* }}} */

/* {{{ int TokyoTyrant::copy(string path);
	Copy the database
*/
PHP_METHOD(tokyotyrant, copy)
{
	php_tokyo_tyrant_object *intern;
	char *path;
	int path_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);

	if (!tcrdbcopy(intern->conn->rdb, path)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to copy the database: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

#if PHP_MAJOR_VERSION >=5 && PHP_MINOR_VERSION >= 3
static uint_fast64_t _php_tt_get_ts(zval *date_param TSRMLS_DC) 
{
	zval *fname, retval, *params[1];
	
	MAKE_STD_ZVAL(fname);
	ZVAL_STRING(fname, "date_timestamp_get", 1);

	params[0] = date_param;

	call_user_function(EG(function_table), NULL, fname, &retval, 1, params TSRMLS_CC);
	zval_dtor(fname);
	FREE_ZVAL(fname);

	if (Z_TYPE(retval) == IS_BOOL && !Z_BVAL(retval)) {
		return 0;
	}

	/* Microseconds */
	return (Z_LVAL(retval) * 1000);
}

/* {{{ TokyoTyrant TokyoTyrant::restore(string log_dir, mixed timestamp[, bool check_consistency = true]);
	restore the database
*/
PHP_METHOD(tokyotyrant, restore)
{
	zval *date_param;
	php_tokyo_tyrant_object *intern;
	char *path;
	int path_len, opts;
	uint_fast64_t ts;
	zend_bool check_consistency = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|b", &path, &path_len, &date_param, &check_consistency) == FAILURE) {
		return;
	}
#if !defined(__LP64__) && !defined(__ILP64__)
	PHP_TOKYO_TYRANT_EXCEPTION_MSG("TokyoTyrant::restore is not supported on a 32bit platform");
#endif
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	if (Z_TYPE_P(date_param) == IS_OBJECT) {
		zend_class_entry *date_ce_date = php_date_get_date_ce();
		if (!instanceof_function_ex(Z_OBJCE_P(date_param), date_ce_date, 0 TSRMLS_CC)) { 
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("The timestamp parameter must be instanceof DateTime");
		}

		ts = _php_tt_get_ts(date_param TSRMLS_CC);
		if (ts == 0) {
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("Failed to get timestamp from the DateTime object");
		}
	} else {
		convert_to_long(date_param);
		ts = Z_LVAL_P(date_param);
	}
	
	if (check_consistency) {
		opts |= RDBROCHKCON;
	}

	if (!tcrdbrestore(intern->conn->rdb, path, ts, opts)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to restore the database: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */
#else 
/* {{{ TokyoTyrant TokyoTyrant::restore(string log_dir, int timestamp[, bool check_consistency = true]);
	restore the database
*/
PHP_METHOD(tokyotyrant, restore)
{
	php_tokyo_tyrant_object *intern;
	char *path;
	int path_len, opts;
	uint_fast64_t ts;
	zend_bool check_consistency = 1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|b", &path, &path_len, &ts, &check_consistency) == FAILURE) {
		return;
	}
#if !defined(__LP64__) && !defined(__ILP64__)
	PHP_TOKYO_TYRANT_EXCEPTION_MSG("TokyoTyrant::restore is not supported on a 32bit platform");
#endif	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	if (check_consistency) {
		opts |= RDBROCHKCON;
	}

	if (!tcrdbrestore(intern->conn->rdb, path, ts, opts)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to restore the database: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */
#endif

#if PHP_MAJOR_VERSION >=5 && PHP_MINOR_VERSION >= 3
/* {{{ TokyoTyrant TokyoTyrant::setMaster(string host, int port, mixed timestamp[, zend_bool check_consistency]);
	Set the master
*/
PHP_METHOD(tokyotyrant, setmaster)
{
	zval *date_param;
	php_tokyo_tyrant_object *intern;
	char *host;
	int host_len, code, opts;
	long port;
	uint_fast64_t ts;
	zend_bool check_consistency = 1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz|b", &host, &host_len, &port, &date_param, &check_consistency) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);

	if (Z_TYPE_P(date_param) == IS_OBJECT) {
		zend_class_entry *date_ce_date = php_date_get_date_ce();
		if (!instanceof_function_ex(Z_OBJCE_P(date_param), date_ce_date, 0 TSRMLS_CC)) { 
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("The timestamp parameter must be instanceof DateTime");
		}

		ts = _php_tt_get_ts(date_param TSRMLS_CC);
		if (ts == 0) {
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("Failed to get timestamp from the DateTime object");
		}
	} else {
		convert_to_long(date_param);
		ts = Z_LVAL_P(date_param);
	}

	if (check_consistency) {
		opts |= RDBROCHKCON;
	}
	
	if (host_len == 0) {
		code = tcrdbsetmst(intern->conn->rdb, NULL, 0, ts, opts);
	} else {
		code = tcrdbsetmst(intern->conn->rdb, host, port, ts, opts);
	}
	
	if (!code) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to set the master: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */
#else
/* {{{ TokyoTyrant TokyoTyrant::setMaster(string host, int port, int timestamp[, zend_bool check_consistency]);
	Set the master
*/
PHP_METHOD(tokyotyrant, setmaster)
{
	php_tokyo_tyrant_object *intern;
	char *host;
	int host_len, code;
	long ts, opts = 0, port;
	zend_bool check_consistency = 1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll|b", &host, &host_len, &port, &ts, &check_consistency) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);

	if (check_consistency) {
		opts |= RDBROCHKCON;
	}
	
	if (host_len == 0) {
		code = tcrdbsetmst(intern->conn->rdb, NULL, 0, ts, opts);
	} else {
		code = tcrdbsetmst(intern->conn->rdb, host, port, ts, opts);
	}
	
	if (!code) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to set the master: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */
#endif

/** -------- Begin table api -------- **/

/* {{{ TokyoTyrantQuery TokyoTyrantTable::getquery();
	Get query object
	@throws TokyoTyrantException if not connected to a database
*/
PHP_METHOD(tokyotyranttable, getquery) 
{
	php_tokyo_tyrant_object *intern;
	php_tokyo_tyrant_query_object *intern_query;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	object_init_ex(return_value, php_tokyo_tyrant_query_sc_entry);
	intern_query = (php_tokyo_tyrant_query_object *)zend_object_store_get_object(return_value TSRMLS_CC); 
	
	if (!php_tt_query_object_init(intern_query, getThis() TSRMLS_CC)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to initialize the query: %s");
	}
	return;
}
/* }}} */

/* {{{ TokyoTyrantQuery TokyoTyrantTable::genUid();
	Generate unique PK
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if uid generation fails
*/
PHP_METHOD(tokyotyranttable, genuid) 
{
	php_tokyo_tyrant_object *intern;
	long pk = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	pk = (long)tcrdbtblgenuid(intern->conn->rdb);
	
	if (pk == -1) {
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to generate a primary key. Not connected to a table database?");
	}
	RETURN_LONG(pk);
}
/* }}} */

static void _php_tt_table_write_wrapper(INTERNAL_FUNCTION_PARAMETERS, long type)
{
	php_tokyo_tyrant_object *intern;
	zval *keys;
	TCMAP *map;
	char *key = NULL, *kbuf = NULL;
	int key_len = 0, new_len, code = 0;
	long pk = -1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s!a", &key, &key_len, &keys) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	if (key) {
		kbuf = php_tt_prefix(key, key_len, &new_len TSRMLS_CC);	
	} else {
		char buf[256];
		int len;
		pk = (long)tcrdbtblgenuid(intern->conn->rdb);
		if (pk == -1) {
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to generate a primary key. Not connected to a table database?");
		}
		len = snprintf(buf, 256, "%ld", pk);
		kbuf = php_tt_prefix(buf, len, &new_len TSRMLS_CC);
	}
	
	map = php_tt_zval_to_tcmap(keys, 0 TSRMLS_CC);
	
	if (!map) {
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to get values from the argument");
	}

	switch (type) {
		
		case PHP_TOKYO_TYRANT_OP_TBLPUT:
			code = tcrdbtblput(intern->conn->rdb, kbuf, new_len, map);
		break;

		case PHP_TOKYO_TYRANT_OP_TBLPUTKEEP:
			code = tcrdbtblputkeep(intern->conn->rdb, kbuf, new_len, map);
		break;
		
		case PHP_TOKYO_TYRANT_OP_TBLPUTCAT:
			code = tcrdbtblputcat(intern->conn->rdb, kbuf, new_len, map);
		break;
		
		default:
			tcmapdel(map);
			efree(kbuf);
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unknown operation type (should not happen)");
		break;
	}

	tcmapdel(map);

	if (!code) {
		efree(kbuf);
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to store columns: %s");
	}

	if (pk > 0) {
		RETVAL_LONG(pk);
		efree(kbuf);
	} else {
		RETVAL_STRINGL(kbuf, new_len, 0);
	}
	return;
}

/* {{{ int TokyoTyrantTable::put(string pk, array row);
	put a row. if pk = null new key is generated
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, put)
{
	_php_tt_table_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_TBLPUT);
}
/* }}} */

/* {{{ int TokyoTyrantTable::putkeep(string pk, array row);
	put a row
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, putkeep)
{
	_php_tt_table_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_TBLPUTKEEP);
}
/* }}} */

/* {{{ int TokyoTyrantTable::putcat(string pk, array row);
	put a row
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, putcat)
{
	_php_tt_table_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_TBLPUTCAT);
}
/* }}} */

/* {{{ int TokyoTyrantTable::add(void);
	not supported
*/
PHP_METHOD(tokyotyranttable, add)
{
	PHP_TOKYO_TYRANT_EXCEPTION_MSG("add operation is not supported with table databases");
}
/* }}} */

/* {{{ int TokyoTyrantTable::putShl(void);
	not supported
*/
PHP_METHOD(tokyotyranttable, putshl)
{
	PHP_TOKYO_TYRANT_EXCEPTION_MSG("putShl operation is not supported with table databases");
}
/* }}} */

/* {{{ int TokyoTyrantTable::putnr(void);
	not supported
*/
PHP_METHOD(tokyotyranttable, putnr)
{
	PHP_TOKYO_TYRANT_EXCEPTION_MSG("putNr operation is not supported with table databases");
}
/* }}} */

/* {{{ int TokyoTyrantTable::get(mixed pk);
	Get a table entry
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, get) 
{
	php_tokyo_tyrant_object *intern;
	TCMAP *map;
	char *key, *kbuf;
	int key_len, new_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	kbuf = php_tt_prefix(key, key_len, &new_len TSRMLS_CC);	
	map = tcrdbtblget(intern->conn->rdb, kbuf, new_len);
	efree(kbuf);

	if (!map) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the record: %s");
	}

	php_tt_tcmap_to_zval(map, return_value TSRMLS_CC);
	return;
}
/* }}} */

/* {{{ boolean TokyoTyrantTable::out(mixed pk);
	Remove an entry
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, out) 
{
	_php_tt_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_TBLOUT);
}
/* }}} */

/* {{{ boolean TokyoTyrantTable::setIndex(string name, CONST type);
	Set index
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, setindex) 
{
	php_tokyo_tyrant_object *intern;
	char *name;
	int name_len;
	long type;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &name, &name_len, &type) == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	if (!tcrdbtblsetindex(intern->conn->rdb, name, type)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to set index: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/** -------- end table api -------- **/


/** -------- Begin Table Query API --------- **/

/* {{{ string TokyoTyrantQuery::__construct(TokyoTyrant conn);
	construct a query
*/
PHP_METHOD(tokyotyrantquery, __construct) 
{
	zval *objvar; 
	php_tokyo_tyrant_query_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &objvar, php_tokyo_tyrant_sc_entry) == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	if (!php_tt_query_object_init(intern, objvar TSRMLS_CC)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to initialize the query: %s");
	}
	return;
}
/* }}} */

/* {{{ string TokyoTyrantQuery::setLimit([int max, int skip]);
	Set the limit on the query
*/
PHP_METHOD(tokyotyrantquery, setlimit) 
{
	php_tokyo_tyrant_query_object *intern;
	zval *max = NULL, *skip = NULL;
	long l_max = -1, l_skip = -1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!z!", &max, &skip) == FAILURE) {
		return;
	}
	intern = PHP_TOKYO_QUERY_OBJECT;
	
	if (max) {
		if (Z_TYPE_P(max) != IS_LONG) {
			convert_to_long(max);
		}
		l_max = Z_LVAL_P(max);
	}

	if (skip) {
		if (Z_TYPE_P(skip) != IS_LONG) {
			convert_to_long(skip);
		}
		l_skip = Z_LVAL_P(skip);
	}

	tcrdbqrysetlimit(intern->qry, l_max, l_skip);
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ string TokyoTyrantQuery::setOrder(string name, int type);
	Set the order of a query
*/
PHP_METHOD(tokyotyrantquery, setorder)
{
	php_tokyo_tyrant_query_object *intern;
	char *name;
	int name_len;
	long type;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &name, &name_len, &type) == FAILURE) {
		return;
	}

	intern = PHP_TOKYO_QUERY_OBJECT;

	tcrdbqrysetorder(intern->qry, name, type);
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ string TokyoTyrantQuery::addCond(string column, int operator, string expression);
	Add a condition
*/
PHP_METHOD(tokyotyrantquery, addcond) 
{
	php_tokyo_tyrant_query_object *intern;
	char *name, *expr;
	int name_len, expr_len;
	long op;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sls", &name, &name_len, &op, &expr, &expr_len) == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	
	tcrdbqryaddcond(intern->qry, name, op, expr);
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */


#if PHP_TOKYO_TYRANT_VERSION >= 1001033
/* {{{ string TokyoTyrantQuery::hint();
	Get the hint string of a query object
*/
PHP_METHOD(tokyotyrantquery, hint) 
{
	php_tokyo_tyrant_query_object *intern;
	const char *hint;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	hint = tcrdbqryhint(intern->qry);
	RETURN_STRING((char *)hint, 1);
}
/* }}} */

/* {{{ string TokyoTyrantQuery::metaSearch(array queries, int type);
	Retrieve records with multiple query objects
*/
PHP_METHOD(tokyotyrantquery, metasearch) 
{
	php_tokyo_tyrant_query_object *intern;
	zval *queries;
	long type;
	RDBQRY **qrys;
	int num_qrys, i = 0;
	TCLIST *res;
	HashPosition pos;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "al", &queries, &type) == FAILURE) {
		return;
	}

	/* Queries in array and self */
	num_qrys = zend_hash_num_elements(Z_ARRVAL_P(queries)) + 1;
	qrys     = emalloc(num_qrys * sizeof(RDBQRY *));
	
	/* Self is always the 'left-most' */
	intern     = PHP_TOKYO_QUERY_OBJECT;
	qrys[i++]  = intern->qry;
	
	for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(queries), &pos);
		zend_hash_has_more_elements_ex(Z_ARRVAL_P(queries), &pos) == SUCCESS;
		zend_hash_move_forward_ex(Z_ARRVAL_P(queries), &pos)) {

		zval **ppzval;

		if (zend_hash_get_current_data_ex(Z_ARRVAL_P(queries), (void**)&ppzval, &pos) == SUCCESS &&
			Z_TYPE_PP(ppzval) == IS_OBJECT &&
			instanceof_function_ex(Z_OBJCE_PP(ppzval), php_tokyo_tyrant_query_sc_entry, 0 TSRMLS_CC)) {

			php_tokyo_tyrant_query_object *intern_query;
			zval tmpcopy = **ppzval;

			zval_copy_ctor(&tmpcopy);
			INIT_PZVAL(&tmpcopy);

			intern_query = (php_tokyo_tyrant_query_object *)zend_object_store_get_object(&tmpcopy TSRMLS_CC);

			qrys[i++] = intern_query->qry;
			zval_dtor(&tmpcopy);
		} else {
			efree(qrys);
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("The parameter must contain only TokyoTyrantQuery instances");
		}
	}

	res = tcrdbmetasearch(qrys, num_qrys, type);
	efree(qrys);
	
	array_init(return_value);
	php_tt_tclist_to_array(intern->conn->rdb, res, return_value TSRMLS_CC);
	tclistdel(res);
	
	return;
}
/* }}} */
#endif

/* {{{ array TokyoTyrantQuery::search();
	Executes a search query and returns results
*/
PHP_METHOD(tokyotyrantquery, search) 
{
	php_tokyo_tyrant_query_object *intern;
	TCLIST *res;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	res    = tcrdbqrysearch(intern->qry);
	array_init(return_value);
	
	php_tt_tclist_to_array(intern->conn->rdb, res, return_value TSRMLS_CC);
	tclistdel(res);
	
	return;
}
/* }}} */

/* {{{ array TokyoTyrantQuery::count();
	Executes a search query and return the count of corresponding records.
*/
PHP_METHOD(tokyotyrantquery, count)
{
	php_tokyo_tyrant_query_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	RETURN_LONG(tcrdbqrysearchcount(intern->qry));
}
/* }}} */

/* {{{ TokyoTyrantQuery TokyoTyrantQuery::out();
	Executes a delete query
*/
PHP_METHOD(tokyotyrantquery, out) 
{
	php_tokyo_tyrant_query_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	
	if (!tcrdbqrysearchout(intern->qry)) {
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to execute out query");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/******* Start query iterator interface ******/

/* {{{ mixed TokyoTyrantQuery::key();
	Gets the current key
*/
PHP_METHOD(tokyotyrantquery, key) 
{
	php_tokyo_tyrant_query_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	
	if (intern->pos < tclistnum(intern->res)) {
		const char *rbuf;
		int rsiz;
		rbuf = tclistval(intern->res, intern->pos, &rsiz);
		
		if (!rbuf) {
			RETURN_FALSE;
		}
		
		RETURN_STRINGL((char *)rbuf, rsiz, 1);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ mixed TokyoTyrantQuery::next();
	Move to next value and return it
*/
PHP_METHOD(tokyotyrantquery, next) 
{
	php_tokyo_tyrant_query_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	intern = PHP_TOKYO_QUERY_OBJECT;

	if (++(intern->pos) < tclistnum(intern->res)) {
		TCMAP *cols;
		const char *rbuf, *name;
		int rsiz;
		
		rbuf = tclistval(intern->res, intern->pos, &rsiz);
		
		if (!rbuf) {
			RETURN_FALSE;
		}
		
		cols = tcrdbtblget(intern->conn->rdb, rbuf, rsiz);
		
		if (cols) {
			int name_len;
			array_init(return_value);
			tcmapiterinit(cols);

			while ((name = tcmapiternext(cols, &name_len)) != NULL) {
				int data_len;
				const char *data;
				
				data = tcmapget(cols, name, name_len, &data_len);
				add_assoc_stringl(return_value, (char *)name, (char *)data, data_len, 1);
			}
			tcmapdel(cols);
			return;
		}
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ mixed TokyoTyrantQuery::rewind();
	Start from the beginning
*/
PHP_METHOD(tokyotyrantquery, rewind) 
{
	php_tokyo_tyrant_query_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	
	if (!intern->executed) { 
		if (intern->res) {
			tclistdel(intern->res);
		}
		intern->res = tcrdbqrysearch(intern->qry);
	}
	intern->pos = 0;
	RETURN_TRUE;
}
/* }}} */

/* {{{ mixed TokyoTyrantQuery::current();
	Returns the current value
*/
PHP_METHOD(tokyotyrantquery, current) 
{
	php_tokyo_tyrant_query_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	intern = PHP_TOKYO_QUERY_OBJECT;
	
	if (intern->pos < tclistnum(intern->res)) {
		const char *rbuf, *name;
		int rsiz;
		TCMAP *cols;
		
		rbuf = tclistval(intern->res, intern->pos, &rsiz);
		
		if (!rbuf) {
			RETURN_FALSE;
		}
		
		cols = tcrdbtblget(intern->conn->rdb, rbuf, rsiz);
		
		if (cols) {
			int name_len;
			array_init(return_value);
			tcmapiterinit(cols);

			while ((name = tcmapiternext(cols, &name_len)) != NULL) {
				int data_len;
				const char *data;
				
				data = tcmapget(cols, name, name_len, &data_len);
				add_assoc_stringl(return_value, (char *)name, (char *)data, data_len, 1);
			}
			
			tcmapdel(cols);
			return;
		}
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ boolean TokyoTyrantQuery::valid();
	Whether the current item is valid
*/
PHP_METHOD(tokyotyrantquery, valid) 
{
	php_tokyo_tyrant_query_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	intern = PHP_TOKYO_QUERY_OBJECT;
	RETURN_BOOL(intern->pos < tclistnum(intern->res));
}
/* }}} */


/******* End query iterator interface *********/

/** -------- End Table Query API --------- **/

/** -------- Start tokyo tyrant iterator ------- **/

PHP_METHOD(tokyotyrantiterator, __construct)
{
	php_tokyo_tyrant_iterator_object *intern;
	zval *objvar;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &objvar) == FAILURE) {
		return;
	}
	
	if (Z_TYPE_P(objvar) != IS_OBJECT) {
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("The parameter must be a valid TokyoTyrant or TokyoTyrantTable object");
	}
	
	intern = PHP_TOKYO_ITERATOR_OBJECT;
	if (!php_tt_iterator_object_init(intern, objvar TSRMLS_CC)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Failed to initialize the iterator: %s");
	}
	return;
}

PHP_METHOD(tokyotyrantiterator, key)
{
	php_tokyo_tyrant_iterator_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_ITERATOR_OBJECT;
	
	if (!intern->current) {
		RETURN_LONG(0);
	} else {
		RETURN_STRINGL(intern->current, intern->current_len, 1);
	}
}

PHP_METHOD(tokyotyrantiterator, next)
{
	php_tokyo_tyrant_iterator_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_ITERATOR_OBJECT;
	
	if (intern->current) {
		free(intern->current);
		intern->current = NULL;
	}

	intern->current_len = 0;
	intern->current     = tcrdbiternext(intern->conn->rdb, &(intern->current_len));
}

PHP_METHOD(tokyotyrantiterator, rewind)
{
	php_tokyo_tyrant_iterator_object *intern;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_ITERATOR_OBJECT;
	
	/* The iterator has been traversed */
	if (!tcrdbiterinit(intern->conn->rdb)) { 
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Failed to rewind the iterator");
	}
	
	if (intern->current) {
		free(intern->current);
		intern->current = NULL;
	}
	
	intern->current_len = 0;
	intern->current     = tcrdbiternext(intern->conn->rdb, &(intern->current_len));
	return;
}

PHP_METHOD(tokyotyrantiterator, current)
{
	php_tokyo_tyrant_iterator_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_ITERATOR_OBJECT;

	if (intern->iterator_type == PHP_TOKYO_TYRANT_ITERATOR) {

		char *value, *kbuf;
		int new_len, value_len;

		kbuf  = php_tt_prefix(intern->current, intern->current_len, &new_len TSRMLS_CC);
		value = tcrdbget(intern->conn->rdb, kbuf, new_len, &value_len);
		efree(kbuf);
		
		if (!value) {
			PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the record: %s");
		}
		RETVAL_STRINGL(value, value_len, 1);
		free(value);
		
	} else if (intern->iterator_type == PHP_TOKYO_TYRANT_TABLE_ITERATOR) {
		TCMAP *map;
		char *kbuf;
		int new_len;

		kbuf = php_tt_prefix(intern->current, intern->current_len, &new_len TSRMLS_CC);	
		map  = tcrdbtblget(intern->conn->rdb, kbuf, new_len);
		efree(kbuf);

		if (!map) {
			PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the record: %s");
		}
		php_tt_tcmap_to_zval(map, return_value TSRMLS_CC);
	} else {
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unknown iterator type (this should not happen)");
	}
	return;
}

PHP_METHOD(tokyotyrantiterator, valid)
{
	php_tokyo_tyrant_iterator_object *intern;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_ITERATOR_OBJECT;
	
	if (intern->current) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

/** -------- End tokyo tyrant iterator ------- **/


ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_empty_args, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_construct_args, 0, 0, 0)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, port)
	ZEND_ARG_INFO(0, persistent)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_connect_args, 0, 0, 1)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, port)
	ZEND_ARG_INFO(0, persistent)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_connecturi_args, 0, 0, 1)
	ZEND_ARG_INFO(0, uri)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_put_args, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_putshl_args, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, width)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_get_args, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_out_args, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_stat_args, 0, 0, 0)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_tune_args, 0, 0, 1)
	ZEND_ARG_INFO(0, timeout)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_tableput_args, 0, 0, 1)
	ZEND_ARG_INFO(0, columns)
	ZEND_ARG_INFO(0, pk)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_add_args, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_size_args, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_fwmkeys_args, 0, 0, 2)
	ZEND_ARG_INFO(0, prefix)
	ZEND_ARG_INFO(0, max_recs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_ext_args, 0, 0, 4)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, options)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_copy_args, 0, 0, 1)
	ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_restore_args, 0, 0, 2)
	ZEND_ARG_INFO(0, log_dir)
	ZEND_ARG_INFO(0, timestamp)
	ZEND_ARG_INFO(0, check_consistency)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_setmaster_args, 0, 0, 3)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, port)
	ZEND_ARG_INFO(0, timestamp)
	ZEND_ARG_INFO(0, check_consistency)
ZEND_END_ARG_INFO()

static function_entry php_tokyo_tyrant_class_methods[] =
{
	PHP_ME(tokyotyrant, __construct,	tokyo_tyrant_construct_args,	ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(tokyotyrant, connect,		tokyo_tyrant_connect_args,		ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, connecturi,		tokyo_tyrant_connecturi_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, vanish,			tokyo_tyrant_empty_args,		ZEND_ACC_PUBLIC)	
	PHP_ME(tokyotyrant, stat,			tokyo_tyrant_stat_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, tune,			tokyo_tyrant_tune_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, fwmkeys,		tokyo_tyrant_fwmkeys_args,		ZEND_ACC_PUBLIC)
	
	PHP_ME(tokyotyrant, size,			tokyo_tyrant_size_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, num,			tokyo_tyrant_empty_args,		ZEND_ACC_PUBLIC)
	
	PHP_ME(tokyotyrant, sync,			tokyo_tyrant_empty_args,		ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, ext,			tokyo_tyrant_ext_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, copy,			tokyo_tyrant_copy_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, restore,		tokyo_tyrant_restore_args,		ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, setmaster,		tokyo_tyrant_setmaster_args,	ZEND_ACC_PUBLIC)

	/* These are the shared methods */
	PHP_ME(tokyotyrant, put,			tokyo_tyrant_put_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, putkeep,		tokyo_tyrant_put_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, putcat,			tokyo_tyrant_put_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, get,			tokyo_tyrant_get_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, out,			tokyo_tyrant_out_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, getiterator,	tokyo_tyrant_empty_args,		ZEND_ACC_PUBLIC)
	
	/* These throw exception on table api */
	PHP_ME(tokyotyrant, add,			tokyo_tyrant_add_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, putshl,			tokyo_tyrant_putshl_args,		ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrant, putnr,			tokyo_tyrant_put_args,			ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL} 
};

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_table_setindex_args, 0, 0, 2)
	ZEND_ARG_INFO(0, column)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

static function_entry php_tokyo_tyrant_table_class_methods[] =
{
	/* Inherit and override */
	PHP_ME(tokyotyranttable, put,		tokyo_tyrant_put_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, putkeep,	tokyo_tyrant_put_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, putcat,	tokyo_tyrant_put_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, get,		tokyo_tyrant_get_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, out,		tokyo_tyrant_out_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, setindex,	tokyo_tyrant_table_setindex_args,	ZEND_ACC_PUBLIC)
	/* Inherit and throw error if used */
	PHP_ME(tokyotyranttable, add,		tokyo_tyrant_add_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, putshl,	tokyo_tyrant_putshl_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, putnr,		tokyo_tyrant_put_args,				ZEND_ACC_PUBLIC)
	/* Table API */
	PHP_ME(tokyotyranttable, getquery,		tokyo_tyrant_empty_args,		ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, genuid,		tokyo_tyrant_empty_args,		ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_query_construct_args, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, TokyoTyrantTable, TokyoTyrantTable, 0) 
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_query_addcond_args, 0, 0, 3)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, op)
	ZEND_ARG_INFO(0, expr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_query_setlimit_args, 0, 0, 0)
	ZEND_ARG_INFO(0, max)
	ZEND_ARG_INFO(0, skip)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_query_setorder_args, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_query_metasearch_args, 0, 0, 2)
	ZEND_ARG_INFO(0, queries)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

static function_entry php_tokyo_tyrant_query_class_methods[] =
{
	PHP_ME(tokyotyrantquery, __construct,	tokyo_tyrant_query_construct_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, addcond,		tokyo_tyrant_query_addcond_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, setlimit,		tokyo_tyrant_query_setlimit_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, setorder,		tokyo_tyrant_query_setorder_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, search,		tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, count,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, out,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
#if PHP_TOKYO_TYRANT_VERSION >= 1001033
	PHP_ME(tokyotyrantquery, hint,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, metasearch,	tokyo_tyrant_query_metasearch_args,	ZEND_ACC_PUBLIC)
#endif
	PHP_ME(tokyotyrantquery, key,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, next,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, rewind,		tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, current,		tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, valid,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL} 
};

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_iterator_construct_args, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, TokyoTyrant, TokyoTyrant, 0) 
ZEND_END_ARG_INFO()

static function_entry php_tokyo_tyrant_iterator_class_methods[] =
{
	/* Iterator interface */
	PHP_ME(tokyotyrantiterator, __construct,	tokyo_tyrant_iterator_construct_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantiterator, key,			tokyo_tyrant_empty_args, 				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantiterator, next,			tokyo_tyrant_empty_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantiterator, rewind,			tokyo_tyrant_empty_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantiterator, current,		tokyo_tyrant_empty_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantiterator, valid,			tokyo_tyrant_empty_args,				ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

zend_function_entry tokyo_tyrant_functions[] = {
	{NULL, NULL, NULL} 
};

static void php_tokyo_tyrant_query_object_free_storage(void *object TSRMLS_DC)
{
	php_tokyo_tyrant_query_object *intern = (php_tokyo_tyrant_query_object *)object;

	if (!intern) {
		return;
	}
	
	if (intern->parent) {
	
#ifdef Z_REFCOUNT_P
		Z_SET_REFCOUNT_P(intern->parent, Z_REFCOUNT_P(intern->parent) - 1);
		if (Z_REFCOUNT_P(intern->parent) <= 0) {
#else
		intern->parent->refcount--; 
		if (intern->parent->refcount == 0) {
#endif		
	 	/* TODO: check if this leaks */
			efree(intern->parent);
		}
	}
	
	if (intern->res) {
		tclistdel(intern->res);
	}

	if (intern->qry)
		tcrdbqrydel(intern->qry);

	zend_object_std_dtor(&intern->zo TSRMLS_CC);
	efree(intern);
}

static zend_object_value php_tokyo_tyrant_query_object_new(zend_class_entry *class_type TSRMLS_DC)
{
	zval *tmp;
	zend_object_value retval;
	php_tokyo_tyrant_query_object *intern;

	/* Allocate memory for it */
	intern = (php_tokyo_tyrant_query_object *) emalloc(sizeof(php_tokyo_tyrant_query_object));
	memset(&intern->zo, 0, sizeof(zend_object));

	intern->executed = 0;
	intern->qry    = NULL;
	intern->res    = NULL;
	intern->parent = NULL;

	zend_object_std_init(&intern->zo, class_type TSRMLS_CC);
	zend_hash_copy(intern->zo.properties, &class_type->default_properties, (copy_ctor_func_t) zval_add_ref,(void *) &tmp, sizeof(zval *));

	retval.handle = zend_objects_store_put(intern, NULL, (zend_objects_free_object_storage_t) php_tokyo_tyrant_query_object_free_storage, NULL TSRMLS_CC);
	retval.handlers = (zend_object_handlers *) &tokyo_tyrant_query_object_handlers;
	return retval;
}

static void php_tokyo_tyrant_iterator_object_free_storage(void *object TSRMLS_DC)
{
	php_tokyo_tyrant_iterator_object *intern = (php_tokyo_tyrant_iterator_object *)object;

	if (!intern) {
		return;
	}
	
	if (intern->current) {
		free(intern->current);
	}
	
	if (intern->parent) {
	
#ifdef Z_REFCOUNT_P
		Z_SET_REFCOUNT_P(intern->parent, Z_REFCOUNT_P(intern->parent) - 1);
		if (Z_REFCOUNT_P(intern->parent) <= 0) {
#else
		intern->parent->refcount--; 
		if (intern->parent->refcount == 0) {
#endif		
	 	/* TODO: check if this leaks */
			efree(intern->parent);
		}
	}
	zend_object_std_dtor(&intern->zo TSRMLS_CC);
	efree(intern);
}

static zend_object_value php_tokyo_tyrant_iterator_object_new(zend_class_entry *class_type TSRMLS_DC)
{
	zval *tmp;
	zend_object_value retval;
	php_tokyo_tyrant_iterator_object *intern;
	
	/* Allocate memory for it */
	intern = (php_tokyo_tyrant_iterator_object *) emalloc(sizeof(php_tokyo_tyrant_iterator_object));
	memset(&intern->zo, 0, sizeof(zend_object));

	intern->conn	= NULL;
	intern->parent	= NULL;
	intern->current	= NULL;

	zend_object_std_init(&intern->zo, class_type TSRMLS_CC);
	zend_hash_copy(intern->zo.properties, &class_type->default_properties, (copy_ctor_func_t) zval_add_ref,(void *) &tmp, sizeof(zval *));

	retval.handle = zend_objects_store_put(intern, NULL, (zend_objects_free_object_storage_t) php_tokyo_tyrant_iterator_object_free_storage, NULL TSRMLS_CC);
	retval.handlers = (zend_object_handlers *) &tokyo_tyrant_iterator_object_handlers;
	return retval;
}

static void php_tokyo_tyrant_object_free_storage(void *object TSRMLS_DC)
{
	php_tokyo_tyrant_object *intern = (php_tokyo_tyrant_object *)object;

	if (!intern) {
		return;
	}
	
	php_tt_conn_deinit(intern->conn TSRMLS_CC);
	zend_object_std_dtor(&intern->zo TSRMLS_CC);
	efree(intern);
}

static zend_object_value php_tokyo_tyrant_object_new_ex(zend_class_entry *class_type, php_tokyo_tyrant_object **ptr TSRMLS_DC)
{
	zval *tmp;
	zend_object_value retval;
	php_tokyo_tyrant_object *intern;

	/* Allocate memory for it */
	intern = (php_tokyo_tyrant_object *) emalloc(sizeof(php_tokyo_tyrant_object));
	memset(&intern->zo, 0, sizeof(zend_object));

	/* Init the object. TODO: clone might leak */
	php_tt_object_init(intern TSRMLS_CC);

	if (ptr) {
		*ptr = intern;
	}

	zend_object_std_init(&intern->zo, class_type TSRMLS_CC);
	zend_hash_copy(intern->zo.properties, &class_type->default_properties, (copy_ctor_func_t) zval_add_ref,(void *) &tmp, sizeof(zval *));

	retval.handle = zend_objects_store_put(intern, NULL, (zend_objects_free_object_storage_t) php_tokyo_tyrant_object_free_storage, NULL TSRMLS_CC);
	retval.handlers = (zend_object_handlers *) &tokyo_tyrant_object_handlers;
	return retval;
}

static zend_object_value php_tokyo_tyrant_object_new(zend_class_entry *class_type TSRMLS_DC)
{
	return php_tokyo_tyrant_object_new_ex(class_type, NULL TSRMLS_CC);
}

static zend_object_value php_tokyo_tyrant_clone_object(zval *this_ptr TSRMLS_DC)
{
	php_tokyo_tyrant_object *new_obj = NULL;
	php_tokyo_tyrant_object *old_obj = (php_tokyo_tyrant_object *) zend_object_store_get_object(this_ptr TSRMLS_CC);
	zend_object_value new_ov = php_tokyo_tyrant_object_new_ex(old_obj->zo.ce, &new_obj TSRMLS_CC);
	
	if (old_obj->conn->connected) {
		php_tt_connect_ex(new_obj->conn, old_obj->conn->rdb->host, old_obj->conn->rdb->port, old_obj->conn->rdb->timeout, old_obj->conn->persistent TSRMLS_CC);
	}
	
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);
	return new_ov;
}

static PHP_INI_MH(OnUpdateKeyPrefix)
{
	if (new_value_length > 0) {
		TOKYO_G(key_prefix_len) = new_value_length;
	} else {
		TOKYO_G(key_prefix_len) = 0;
	}
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("tokyo_tyrant.default_timeout", "0.0", PHP_INI_ALL, OnUpdateReal, default_timeout, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
	STD_PHP_INI_ENTRY("tokyo_tyrant.session_salt", "", PHP_INI_ALL, OnUpdateString, salt, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
	STD_PHP_INI_ENTRY("tokyo_tyrant.key_prefix", "", PHP_INI_ALL, OnUpdateKeyPrefix, key_prefix, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
	STD_PHP_INI_ENTRY("tokyo_tyrant.allow_failover", "1", PHP_INI_ALL, OnUpdateBool, allow_failover, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
	STD_PHP_INI_ENTRY("tokyo_tyrant.fail_threshold", "5", PHP_INI_ALL, OnUpdateLong, fail_threshold, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
	STD_PHP_INI_ENTRY("tokyo_tyrant.health_check_divisor", "1000", PHP_INI_ALL, OnUpdateLong, health_check_divisor, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
	STD_PHP_INI_ENTRY("tokyo_tyrant.php_expiration", "0", PHP_INI_ALL, OnUpdateBool, php_expiration, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
PHP_INI_END()

static void php_tokyo_tyrant_init_globals(zend_tokyo_tyrant_globals *tokyo_tyrant_globals)
{
	tokyo_tyrant_globals->connections = NULL;
	tokyo_tyrant_globals->failures    = NULL;
	
	tokyo_tyrant_globals->default_timeout = 0.0;
	tokyo_tyrant_globals->salt = NULL;
	
	tokyo_tyrant_globals->key_prefix     = NULL;
	tokyo_tyrant_globals->key_prefix_len = 0;
	
	tokyo_tyrant_globals->allow_failover = 1;
	tokyo_tyrant_globals->fail_threshold = 5;
	tokyo_tyrant_globals->health_check_divisor = 1000;
	
	tokyo_tyrant_globals->php_expiration = 0;
}

PHP_MINIT_FUNCTION(tokyo_tyrant)
{
	zend_class_entry ce;
	ZEND_INIT_MODULE_GLOBALS(tokyo_tyrant, php_tokyo_tyrant_init_globals, NULL);
	
	REGISTER_INI_ENTRIES();
	
	memcpy(&tokyo_tyrant_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcpy(&tokyo_tyrant_table_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));	
	memcpy(&tokyo_tyrant_query_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));	
	memcpy(&tokyo_tyrant_iterator_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));	
	
	INIT_CLASS_ENTRY(ce, "tokyotyrant", php_tokyo_tyrant_class_methods);
	ce.create_object = php_tokyo_tyrant_object_new;
	tokyo_tyrant_object_handlers.clone_obj = php_tokyo_tyrant_clone_object;
	php_tokyo_tyrant_sc_entry = zend_register_internal_class(&ce TSRMLS_CC);

	INIT_CLASS_ENTRY(ce, "tokyotyranttable", php_tokyo_tyrant_table_class_methods);
	ce.create_object = php_tokyo_tyrant_object_new;
	tokyo_tyrant_table_object_handlers.clone_obj = php_tokyo_tyrant_clone_object;
	php_tokyo_tyrant_table_sc_entry = zend_register_internal_class_ex(&ce, php_tokyo_tyrant_sc_entry, NULL TSRMLS_CC);
	
	INIT_CLASS_ENTRY(ce, "tokyotyrantquery", php_tokyo_tyrant_query_class_methods);
	ce.create_object = php_tokyo_tyrant_query_object_new;
	tokyo_tyrant_query_object_handlers.clone_obj = NULL;
	php_tokyo_tyrant_query_sc_entry = zend_register_internal_class(&ce TSRMLS_CC);
	zend_class_implements(php_tokyo_tyrant_query_sc_entry TSRMLS_CC, 1, zend_ce_iterator);
	
	INIT_CLASS_ENTRY(ce, "tokyotyrantiterator", php_tokyo_tyrant_iterator_class_methods);
	ce.create_object = php_tokyo_tyrant_iterator_object_new;
	tokyo_tyrant_iterator_object_handlers.clone_obj = NULL;
	php_tokyo_tyrant_iterator_sc_entry = zend_register_internal_class(&ce TSRMLS_CC);
	zend_class_implements(php_tokyo_tyrant_iterator_sc_entry TSRMLS_CC, 1, zend_ce_iterator);	
	
	INIT_CLASS_ENTRY(ce, "tokyotyrantexception", NULL);
	php_tokyo_tyrant_exception_sc_entry = zend_register_internal_class_ex(&ce, zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
	php_tokyo_tyrant_exception_sc_entry->ce_flags |= ZEND_ACC_FINAL;
	
#define TOKYO_REGISTER_CONST_LONG(const_name, value) \
	zend_declare_class_constant_long(php_tokyo_tyrant_sc_entry, const_name, sizeof(const_name)-1, (long)value TSRMLS_CC);	
	
	/* Non Tokyo Tyrant constants */
	TOKYO_REGISTER_CONST_LONG("RDBDEF_PORT", PHP_TOKYO_TYRANT_DEFAULT_PORT);
	TOKYO_REGISTER_CONST_LONG("RDBREC_INT", PHP_TOKYO_TYRANT_RECTYPE_INT);
	TOKYO_REGISTER_CONST_LONG("RDBREC_DBL", PHP_TOKYO_TYRANT_RECTYPE_DOUBLE);
	
	/* And the rest */
	TOKYO_REGISTER_CONST_LONG("RDBQC_STREQ", RDBQCSTREQ); 		/* string is equal to */
	TOKYO_REGISTER_CONST_LONG("RDBQC_STRINC", RDBQCSTRINC);		/* string is included in */
  	TOKYO_REGISTER_CONST_LONG("RDBQC_STRBW", RDBQCSTRBW);		/* string begins with */
	TOKYO_REGISTER_CONST_LONG("RDBQC_STREW", RDBQCSTREW);		/* string ends with */
	TOKYO_REGISTER_CONST_LONG("RDBQC_STRAND", RDBQCSTRAND);		/* string includes all tokens in */
	TOKYO_REGISTER_CONST_LONG("RDBQC_STROR", RDBQCSTROR);		/* string includes at least one token in */
	TOKYO_REGISTER_CONST_LONG("RDBQC_STROREQ", RDBQCSTROREQ);	/* string is equal to at least one token in */
	TOKYO_REGISTER_CONST_LONG("RDBQC_STRRX", RDBQCSTRRX);		/* string matches regular expressions of */
	TOKYO_REGISTER_CONST_LONG("RDBQC_NUMEQ", RDBQCNUMEQ);		/* number is equal to */                            
	TOKYO_REGISTER_CONST_LONG("RDBQC_NUMGT", RDBQCNUMGT);		/* number is greater than */                        
	TOKYO_REGISTER_CONST_LONG("RDBQC_NUMGE", RDBQCNUMGE);		/* number is greater than or equal to */           
	TOKYO_REGISTER_CONST_LONG("RDBQC_NUMLT", RDBQCNUMLT);		/* number is less than */                           
	TOKYO_REGISTER_CONST_LONG("RDBQC_NUMLE", RDBQCNUMLE);		/* number is less than or equal to */               
	TOKYO_REGISTER_CONST_LONG("RDBQC_NUMBT", RDBQCNUMBT);		/* number is between two tokens of */              
	TOKYO_REGISTER_CONST_LONG("RDBQC_NUMOREQ", RDBQCNUMOREQ);	/* number is equal to at least one token in */     
	TOKYO_REGISTER_CONST_LONG("RDBQC_NEGATE",  RDBQCNEGATE);	/* negation flag */                                 
	TOKYO_REGISTER_CONST_LONG("RDBQC_NOIDX",   RDBQCNOIDX);		/* no index flag */ 

#if PHP_TOKYO_TYRANT_VERSION >= 1001029
	TOKYO_REGISTER_CONST_LONG("RDBQCFTS_PH",  RDBQCFTSPH);		/* full-text search with the phrase of */
	TOKYO_REGISTER_CONST_LONG("RDBQCFTS_AND", RDBQCFTSAND);		/* full-text search with all tokens in */
	TOKYO_REGISTER_CONST_LONG("RDBQCFTS_OR",  RDBQCFTSOR);		/* full-text search with at least one token in */
	TOKYO_REGISTER_CONST_LONG("RDBQCFTS_EX",  RDBQCFTSEX);		/* full-text search with the compound expression of */
#endif

	TOKYO_REGISTER_CONST_LONG("RDBXOLCK_REC", RDBXOLCKREC);		/* record locking */
	TOKYO_REGISTER_CONST_LONG("RDBXOLCK_GLB", RDBXOLCKGLB);		/* global locking */

	TOKYO_REGISTER_CONST_LONG("RDBQO_STRASC", RDBQOSTRASC);		/* string ascending */
	TOKYO_REGISTER_CONST_LONG("RDBQO_STRDESC", RDBQOSTRDESC);	/* string descending */
	TOKYO_REGISTER_CONST_LONG("RDBQO_NUMASC", RDBQONUMASC);		/* number ascending */
	TOKYO_REGISTER_CONST_LONG("RDBQO_NUMDESC", RDBQONUMDESC);	/* number descending */
	
	TOKYO_REGISTER_CONST_LONG("RDBIT_LEXICAL", RDBITLEXICAL);	/* lexical string */
	TOKYO_REGISTER_CONST_LONG("RDBIT_DECIMAL", RDBITDECIMAL);	/* decimal string */
	TOKYO_REGISTER_CONST_LONG("RDBIT_OPT", RDBITOPT);			/* optimize */
	TOKYO_REGISTER_CONST_LONG("RDBIT_VOID", RDBITVOID);			/* void */
	TOKYO_REGISTER_CONST_LONG("RDBIT_KEEP", RDBITKEEP);			/* keep existing index */
#if PHP_TOKYO_TYRANT_VERSION >= 1001029
	TOKYO_REGISTER_CONST_LONG("RDBIT_TOKEN", RDBITTOKEN);		/* token inverted index */
	TOKYO_REGISTER_CONST_LONG("RDBIT_QGRAM", RDBITQGRAM);		/* q-gram inverted index */
#endif	
		
	TOKYO_REGISTER_CONST_LONG("TTE_SUCCESS", TTESUCCESS);		/* success */
	TOKYO_REGISTER_CONST_LONG("TTE_INVALID", TTEINVALID);		/* invalid operation */
	TOKYO_REGISTER_CONST_LONG("TTE_NOHOST",  TTENOHOST);		/* host not found */
	TOKYO_REGISTER_CONST_LONG("TTE_REFUSED", TTEREFUSED);		/* connection refused */
	TOKYO_REGISTER_CONST_LONG("TTE_SEND", TTESEND);				/* send error */
	TOKYO_REGISTER_CONST_LONG("TTE_RECV", TTERECV);				/* recv error */
	TOKYO_REGISTER_CONST_LONG("TTE_KEEP", TTEKEEP);				/* existing record */
	TOKYO_REGISTER_CONST_LONG("TTE_NOREC", TTENOREC);			/* no record found */
	TOKYO_REGISTER_CONST_LONG("TTE_MISC", TTEMISC);				/* miscellaneous error */
	
#if PHP_TOKYO_TYRANT_VERSION >= 1001033
	TOKYO_REGISTER_CONST_LONG("RDBMS_UNION", RDBMSUNION);		/* union */            
	TOKYO_REGISTER_CONST_LONG("RDBMS_ISECT", RDBMSISECT);		/* intersection */
	TOKYO_REGISTER_CONST_LONG("RDBMS_DIFF", RDBMSDIFF);			/* difference */
#endif

	TOKYO_REGISTER_CONST_LONG("RDBT_RECON", RDBTRECON);			/* reconnect */
	
#undef TOKYO_REGISTER_CONST_LONG

#ifdef HAVE_PHP_TOKYO_TYRANT_SESSION
	php_session_register_module(ps_tokyo_tyrant_ptr);
#endif
	return SUCCESS;
}

static int _php_tt_connections_hash_dtor(TCRDB **datas TSRMLS_DC) 
{
	TCRDB *rdb = *datas;
	tcrdbdel(rdb);
	return ZEND_HASH_APPLY_REMOVE;
}

PHP_MSHUTDOWN_FUNCTION(tokyo_tyrant)
{
	if (TOKYO_G(connections)) {
		zend_hash_apply(TOKYO_G(connections), (apply_func_t)_php_tt_connections_hash_dtor TSRMLS_CC);
		zend_hash_destroy(TOKYO_G(connections));
		free(TOKYO_G(connections));
		TOKYO_G(connections) = NULL;
	}
	
	if (TOKYO_G(failures)) {
		zend_hash_destroy(TOKYO_G(failures));
		free(TOKYO_G(failures));
		TOKYO_G(failures) = NULL;
	}
	
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_MINFO_FUNCTION(tokyo_tyrant)
{
	php_info_print_table_start();
	
		php_info_print_table_header(2, "tokyo_tyrant extension", "enabled");
		php_info_print_table_row(2, "tokyo_tyrant extension version", PHP_TOKYO_TYRANT_EXTVER);
#ifdef HAVE_PHP_TOKYO_TYRANT_SESSION		
		php_info_print_table_row(2, "tokyo_tyrant session handler", "available");
#else
		php_info_print_table_row(2, "tokyo_tyrant session handler", "unavailable");
#endif
	php_info_print_table_end();
	
	DISPLAY_INI_ENTRIES();
}

zend_module_entry tokyo_tyrant_module_entry =
{
        STANDARD_MODULE_HEADER,
        PHP_TOKYO_TYRANT_EXTNAME,
        tokyo_tyrant_functions,			/* Functions */
        PHP_MINIT(tokyo_tyrant),		/* MINIT */
        PHP_MSHUTDOWN(tokyo_tyrant),	/* MSHUTDOWN */
        NULL,							/* RINIT */
        NULL,							/* RSHUTDOWN */
        PHP_MINFO(tokyo_tyrant),		/* MINFO */
        PHP_TOKYO_TYRANT_EXTVER,		/* version */
        STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_TOKYO_TYRANT
ZEND_GET_MODULE(tokyo_tyrant)
#endif /* COMPILE_DL_TOKYO_TYRANT */



