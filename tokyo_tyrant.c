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
#include "php_tokyo_tyrant_session.h"
#include "php_tokyo_tyrant_session_funcs.h"

#include "php_tokyo_tyrant_hash.h"

#include "SAPI.h" 
#include "php_variables.h"
#include "ext/standard/info.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"

#include <math.h>

zend_class_entry *php_tokyo_tyrant_sc_entry;
zend_class_entry *php_tokyo_tyrant_table_sc_entry;
zend_class_entry *php_tokyo_tyrant_query_sc_entry;
zend_class_entry *php_tokyo_tyrant_exception_sc_entry;

static zend_object_handlers tokyo_tyrant_object_handlers;
static zend_object_handlers tokyo_tyrant_table_object_handlers;
static zend_object_handlers tokyo_tyrant_query_object_handlers;

ZEND_DECLARE_MODULE_GLOBALS(tokyo_tyrant);

static char *_php_tt_key(char *key, int key_len, int *new_len TSRMLS_DC)
{
	char *buffer;
	int buffer_len;
	
	if (!TOKYO_G(key_prefix)) {
		return estrdup(key);
	}
	
	buffer_len = strlen(TOKYO_G(key_prefix)) + key_len + 1;
	
	buffer = emalloc(buffer_len);
	memset(buffer, '\0', buffer_len);
	
	*new_len = sprintf(buffer, "%s%s", TOKYO_G(key_prefix), key);
	return buffer;
}

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

	if (host && !php_tokyo_tyrant_connect(intern, host, port, params TSRMLS_CC)) {
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
	
	if (!php_tokyo_tyrant_connect(intern, host, port, params TSRMLS_CC)) {
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

	if (!php_tokyo_tyrant_connection_from_uri(intern, url TSRMLS_CC)) {
		php_url_free(url);
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to connect to database: %s");
	}
	php_url_free(url);
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ static int _php_tt_real_write(TCRDB *rdb, long type, char *key, char *value TSRMLS_DC) */
static int _php_tt_real_write(TCRDB *rdb, long type, char *key, char *value TSRMLS_DC)
{
	int code = 0;
	switch (type) {
		
		case PHP_TOKYO_TYRANT_OP_PUT:
			code = tcrdbput2(rdb, key, value);
		break;
		
		case PHP_TOKYO_TYRANT_OP_PUTKEEP:
			code = tcrdbputkeep2(rdb, key, value);
		break;
		
		case PHP_TOKYO_TYRANT_OP_PUTCAT:
			code = tcrdbputcat2(rdb, key, value);
		break;

		case PHP_TOKYO_TYRANT_OP_OUT:
			code = tcrdbout2(rdb, key);
		break;
		
		case PHP_TOKYO_TYRANT_OP_TBLOUT:
			code = tcrdbtblout(rdb, key, strlen(key));
		break;
	}
	
	/* TODO: maybe add strict flag but ignore non-existent keys for now */
	if (!code) {
		if (tcrdbecode(rdb) == TTENOREC) {
			code = 1;
		}
	}
	return code;
}
/* }}} */

/* {{{ static int _php_tt_op_many(zval **ppzval, int num_args, va_list args, zend_hash_key *hash_key) */
static int _php_tt_op_many(zval **ppzval, int num_args, va_list args, zend_hash_key *hash_key)
{
	zval tmpcopy;
	php_tokyo_tyrant_object *intern;
	int type, *code;
	char *key, key_buf[256];

    if (num_args != 3) {
		return 0;
    }

	TSRMLS_FETCH();

	intern = va_arg(args, php_tokyo_tyrant_object *);
	type   = va_arg(args, int);
	code   = va_arg(args, int *);
	
	if (hash_key->nKeyLength == 0) {
		key_buf[0] = '\0';
		sprintf(key_buf, "%ld", hash_key->h);
		key = key_buf;
	} else {
		key = hash_key->arKey;
	}

	tmpcopy = **ppzval;
	zval_copy_ctor(&tmpcopy);
	INIT_PZVAL(&tmpcopy);
	convert_to_string(&tmpcopy);
	
	if (type == PHP_TOKYO_TYRANT_OP_OUT || type == PHP_TOKYO_TYRANT_OP_TBLOUT) {
		*code = _php_tt_real_write(intern->conn->rdb, type, Z_STRVAL(tmpcopy), NULL TSRMLS_CC);
	} else {
		*code = _php_tt_real_write(intern->conn->rdb, type, key, Z_STRVAL(tmpcopy) TSRMLS_CC);
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
		zend_hash_apply_with_arguments(Z_ARRVAL_P(key), (apply_func_args_t) _php_tt_op_many, 3, intern, type, &code);
		
		if (!code) {
			if (type  == PHP_TOKYO_TYRANT_OP_OUT || type == PHP_TOKYO_TYRANT_OP_TBLOUT) {
				PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to remove the records: %s");
			} else {
				PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to put the records: %s");
			}
		}
		
	} else {
		convert_to_string(key);
		
		if (type == PHP_TOKYO_TYRANT_OP_OUT || type == PHP_TOKYO_TYRANT_OP_TBLOUT) {
			if (!_php_tt_real_write(intern->conn->rdb, type, Z_STRVAL_P(key), NULL TSRMLS_CC)) {
				zend_throw_exception_ex(php_tokyo_tyrant_exception_sc_entry, 0 TSRMLS_CC, "Unable to remove the record '%s': %s", 
										Z_STRVAL_P(key), tcrdberrmsg(tcrdbecode(intern->conn->rdb)));
				return;
			}
		} else {
			if (!value) {
				PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to store the record: no value provided");
			}
			convert_to_string(value);
			
			if (!_php_tt_real_write(intern->conn->rdb, type, Z_STRVAL_P(key), Z_STRVAL_P(value) TSRMLS_CC)) {
				zend_throw_exception_ex(php_tokyo_tyrant_exception_sc_entry, 0 TSRMLS_CC, "Unable to store the record '%s': %s", 
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

/* {{{ TokyoTyrant TokyoTyrant::out(mixed key);
	Removes a record
	@throws TokyoTyrantException if remove fails
*/
PHP_METHOD(tokyotyrant, out) 
{
	_php_tt_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_OUT);
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
		TCMAP *map = php_tokyo_tyrant_zval_to_tcmap(key, 1 TSRMLS_CC);
		tcrdbget3(intern->conn->rdb, map);

		if (!map) {
			PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the records: %s");
		}

		php_tokyo_tyrant_tcmap_to_zval(map, return_value TSRMLS_CC);		
		tcmapdel(map);
	} else {
		zval tmpcopy;
		char *value;
		
		tmpcopy = *key;
		zval_copy_ctor(&tmpcopy);
		INIT_PZVAL(&tmpcopy);
		convert_to_string(&tmpcopy);
	
		value = tcrdbget2(intern->conn->rdb, Z_STRVAL(tmpcopy));
		zval_dtor(&tmpcopy);
		
		if (!value) {
			PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the record: %s");
		}
		RETVAL_STRING(value, 1);
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
	char *key;
	int key_len = 0, retint;
	long type = 0;
	zval *value;
	
	double retdouble;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|l", &key, &key_len, &value, &type) == FAILURE) {
		return;
	}
	
	PHP_TOKYO_CONNECTED_OBJECT(intern);

	if (type == 0) {
		if (Z_TYPE_P(value) == IS_DOUBLE) {
			type = PHP_TOKYO_TYRANT_RECTYPE_DOUBLE;
		} else {
			type = PHP_TOKYO_TYRANT_RECTYPE_INT;
		}
	}
	
	switch (type) {
		
		case PHP_TOKYO_TYRANT_RECTYPE_INT:
			retint = tcrdbaddint(intern->conn->rdb, key, key_len, Z_LVAL_P(value));
			if (retint == INT_MIN) {
				RETURN_NULL();
			}
			RETURN_LONG(retint);
		break;
		
		case PHP_TOKYO_TYRANT_RECTYPE_DOUBLE:
			retdouble = tcrdbadddouble(intern->conn->rdb, key, key_len, Z_DVAL_P(value));
			if (isnan(retdouble)) {
				RETURN_NULL();
			}
			RETURN_DOUBLE(retdouble);
		break;
		
		default:
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unknown record type");
		break;	
	}
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

		if (sscanf(ptr, "%s\t%s", k, v) != 2) {
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
	char *key;
	int key_len;
	long rec_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	rec_len = tcrdbvsiz2(intern->conn->rdb, key);
	
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
	char *prefix;
	int prefix_len;
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
		add_next_index_stringl(return_value, (char *)rbuf, rsiz, 1);
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

/* {{{ TokyoTyrant TokyoTyrant::restore(string log_dir, int timestamp[, bool check_consistency = true]);
	restore the database
*/
PHP_METHOD(tokyotyrant, restore)
{
	php_tokyo_tyrant_object *intern;
	char *path;
	int path_len;
	long ts, opts;
	zend_bool check_consistency = 1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|b", &path, &path_len, &ts, &check_consistency) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	if (check_consistency) {
		opts |= RDBROCHKCON;
	}

	if (!tcrdbcopy(intern->conn->rdb, path)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to copy the database: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

/* {{{ TokyoTyrant TokyoTyrant::setMaster(string host, int port, int timestamp[, zend_bool check_consistency]);
	Set the master
*/
PHP_METHOD(tokyotyrant, setmaster)
{
	php_tokyo_tyrant_object *intern;
	char *host;
	int host_len;
	long ts, opts = 0, port;
	zend_bool check_consistency = 1;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll|b", &host, &host_len, &port, &ts, &check_consistency) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);

	if (check_consistency) {
		opts |= RDBROCHKCON;
	}
	
	if (!tcrdbsetmst(intern->conn->rdb, host, port, ts, opts)) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to set the master: %s");
	}
	PHP_TOKYO_CHAIN_METHOD;
}
/* }}} */

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
	
	php_tokyo_tyrant_init_tt_query_object(intern_query, getThis() TSRMLS_CC);
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
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to generate primary key. Not connected to a table database?");
	}
	RETURN_LONG(pk);
}
/* }}} */

static void _php_tt_table_write_wrapper(INTERNAL_FUNCTION_PARAMETERS, long type)
{
	php_tokyo_tyrant_object *intern;
	zval *keys;
	TCMAP *map;
	zval *pk_arg;
	char pk_str[256];
	int pk_str_len, code = 0;
	long pk;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "za", &pk_arg, &keys) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	if (Z_TYPE_P(pk_arg) == IS_NULL) {
		pk = (long)tcrdbtblgenuid(intern->conn->rdb);
		
		if (pk == -1) {
			PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to generate primary key. Not connected to a table database?");
		}
	} else {
		convert_to_long(pk_arg);
		pk = Z_LVAL_P(pk_arg);
	}
	
	memset(pk_str, '\0', 256);
	pk_str_len = sprintf(pk_str, "%ld", pk);
	
	map = php_tokyo_tyrant_zval_to_tcmap(keys, 0 TSRMLS_CC);
	
	if (!map) {
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to get values from the argument");
	}

	switch (type) {
		
		case PHP_TOKYO_TYRANT_OP_TBLPUT:
			code = tcrdbtblput(intern->conn->rdb, pk_str, pk_str_len, map);
		break;

		case PHP_TOKYO_TYRANT_OP_TBLPUTKEEP:
			code = tcrdbtblputkeep(intern->conn->rdb, pk_str, pk_str_len, map);
		break;
		
		case PHP_TOKYO_TYRANT_OP_TBLPUTCAT:
			code = tcrdbtblputcat(intern->conn->rdb, pk_str, pk_str_len, map);
		break;
	}

	tcmapdel(map);
	
	if (!code) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to store columns: %s");
	}
	RETURN_LONG(pk);
}

/* {{{ int TokyoTyrantTable::put(int pk, array row);
	put a row. if pk = null new key is generated
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, put)
{
	_php_tt_table_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_TBLPUT);
}
/* }}} */

/* {{{ int TokyoTyrantTable::putkeep(int pk, array row);
	put a row
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, putkeep)
{
	_php_tt_table_write_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PHP_TOKYO_TYRANT_OP_TBLPUTKEEP);
}
/* }}} */

/* {{{ int TokyoTyrantTable::putcat(int pk, array row);
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
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, add)
{
	PHP_TOKYO_TYRANT_EXCEPTION_MSG("add operation is not supported with table databases");
}
/* }}} */

/* {{{ int TokyoTyrantTable::get(int pk);
	Get a table entry
	@throws TokyoTyrantException if not connected to a database
	@throws TokyoTyrantException if get fails
*/
PHP_METHOD(tokyotyranttable, get) 
{
	php_tokyo_tyrant_object *intern;
	TCMAP *map;
	long pk = 0;
	char pk_str[256];
	int pk_str_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &pk) == FAILURE) {
		return;
	}
	PHP_TOKYO_CONNECTED_OBJECT(intern);
	
	memset(pk_str, 0, 256);
	pk_str_len = sprintf(pk_str, "%ld", pk);
	
	map = tcrdbtblget(intern->conn->rdb, pk_str, pk_str_len);

	if (!map) {
		PHP_TOKYO_TYRANT_EXCEPTION(intern, "Unable to get the record: %s");
	}

	php_tokyo_tyrant_tcmap_to_zval(map, return_value TSRMLS_CC);
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
	php_tokyo_tyrant_init_tt_query_object(intern, objvar TSRMLS_CC);
	return;
}
/* }}} */

/* {{{ string TokyoTyrantQuery::setLimit(int max[, int skip]);
	Set the limit on the query
*/
PHP_METHOD(tokyotyrantquery, setlimit) 
{
	php_tokyo_tyrant_query_object *intern;
	long max = 0, skip = 0;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|l", &max, &skip) == FAILURE) {
		return;
	}
	
	intern = PHP_TOKYO_QUERY_OBJECT;
	
	tcrdbqrysetlimit(intern->qry, max, skip);
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
	
	php_tokyo_tyrant_tclist_to_array(intern->conn->rdb, res, return_value TSRMLS_CC);
	tclistdel(res);
	
	return;
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
		PHP_TOKYO_TYRANT_EXCEPTION_MSG("Unable to execute query: %s");
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
		
		RETURN_STRING((char *)rbuf, 1);
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
			array_init(return_value);
			tcmapiterinit(cols);

			while ((name = tcmapiternext2(cols)) != NULL) {
				add_assoc_string(return_value, (char *)name, (char *)tcmapget2(cols, name), 1); 
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
	
	if (intern->res) {
		tclistdel(intern->res);
	}

	intern->res = tcrdbqrysearch(intern->qry);
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
			array_init(return_value);
			tcmapiterinit(cols);

			while ((name = tcmapiternext2(cols)) != NULL) {
				add_assoc_string(return_value, (char *)name, (char *)tcmapget2(cols, name), 1); 
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

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_get_args, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_out_args, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_stat_args, 0, 0, 0)
	ZEND_ARG_INFO(0, params)
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
	PHP_ME(tokyotyrant, add,			tokyo_tyrant_add_args,			ZEND_ACC_PUBLIC)

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
	PHP_ME(tokyotyranttable, add,		tokyo_tyrant_add_args,				ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, setindex,	tokyo_tyrant_table_setindex_args,	ZEND_ACC_PUBLIC)
	/* Table API */
	PHP_ME(tokyotyranttable, getquery,		tokyo_tyrant_empty_args,		ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyranttable, genuid,		tokyo_tyrant_empty_args,		ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_query_construct_args, 0, 0, 2)
	ZEND_ARG_OBJ_INFO(0, TokyoTyrant, TokyoTyrant, 0) 
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_query_addcond_args, 0, 0, 3)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, op)
	ZEND_ARG_INFO(0, expr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(tokyo_tyrant_query_setlimit_args, 0, 0, 1)
	ZEND_ARG_INFO(0, max)
	ZEND_ARG_INFO(0, skip)
ZEND_END_ARG_INFO()

static function_entry php_tokyo_tyrant_query_class_methods[] =
{
	PHP_ME(tokyotyrantquery, __construct,	tokyo_tyrant_query_construct_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, addcond,		tokyo_tyrant_query_addcond_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, setlimit,		tokyo_tyrant_query_setlimit_args,	ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, search,		tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, out,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	
	PHP_ME(tokyotyrantquery, key,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, next,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, rewind,		tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, current,		tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	PHP_ME(tokyotyrantquery, valid,			tokyo_tyrant_empty_args,			ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL} 
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
	
	intern->parent->refcount--; 
	if (intern->parent->refcount == 0) { /* TODO: check if this leaks */
		efree(intern->parent);
	}
	
	if (intern->res) {
		tclistdel(intern->res);
	}

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

	zend_object_std_init(&intern->zo, class_type TSRMLS_CC);
	zend_hash_copy(intern->zo.properties, &class_type->default_properties, (copy_ctor_func_t) zval_add_ref,(void *) &tmp, sizeof(zval *));

	retval.handle = zend_objects_store_put(intern, NULL, (zend_objects_free_object_storage_t) php_tokyo_tyrant_query_object_free_storage, NULL TSRMLS_CC);
	retval.handlers = (zend_object_handlers *) &tokyo_tyrant_query_object_handlers;
	return retval;
}

static void php_tokyo_tyrant_object_free_storage(void *object TSRMLS_DC)
{
	php_tokyo_tyrant_object *intern = (php_tokyo_tyrant_object *)object;

	if (!intern) {
		return;
	}
	
	if (php_tokyo_tyrant_is_connected(intern) && !intern->conn->persistent) {
		php_tokyo_tyrant_disconnect(intern->conn TSRMLS_CC);
		free(intern->conn);
	}

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

	php_tokyo_tyrant_init_tt_object(intern TSRMLS_CC);

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
	
	if (php_tokyo_tyrant_is_connected(old_obj)) {
		php_tokyo_tyrant_connect_ex(new_obj, old_obj->conn->host, old_obj->conn->port, old_obj->conn->timeout, old_obj->conn->options, old_obj->conn->persistent TSRMLS_CC);
	}
	
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);
	return new_ov;
}

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("tokyo_tyrant.default_timeout", "2.0", PHP_INI_ALL, OnUpdateReal, default_timeout, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
	STD_PHP_INI_ENTRY("tokyo_tyrant.session_salt", "", PHP_INI_ALL, OnUpdateString, salt, zend_tokyo_tyrant_globals, tokyo_tyrant_globals)
PHP_INI_END()

static void php_tokyo_tyrant_init_globals(zend_tokyo_tyrant_globals *tokyo_tyrant_globals)
{
	tokyo_tyrant_globals->connections = NULL;
	tokyo_tyrant_globals->default_timeout = 2.0;
	tokyo_tyrant_globals->salt = NULL;
	
	tokyo_tyrant_globals->key_prefix     = NULL;
	tokyo_tyrant_globals->key_prefix_len = 0;
}

PHP_MINIT_FUNCTION(tokyo_tyrant)
{
	zend_class_entry ce;
	ZEND_INIT_MODULE_GLOBALS(tokyo_tyrant, php_tokyo_tyrant_init_globals, NULL);
	
	REGISTER_INI_ENTRIES();
	
	memcpy(&tokyo_tyrant_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	memcpy(&tokyo_tyrant_table_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));	
	memcpy(&tokyo_tyrant_query_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));	
	
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
	
	INIT_CLASS_ENTRY(ce, "tokyotyrantexception", NULL);
	php_tokyo_tyrant_exception_sc_entry = zend_register_internal_class_ex(&ce, zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
	php_tokyo_tyrant_exception_sc_entry->ce_flags |= ZEND_ACC_FINAL;
	
#define TOKYO_REGISTER_CONST_LONG(const_name, value) \
	zend_declare_class_constant_long(php_tokyo_tyrant_sc_entry, const_name, sizeof(const_name)-1, (long)value TSRMLS_CC);	
	
	TOKYO_REGISTER_CONST_LONG("DEFAULT_PORT", PHP_TOKYO_TYRANT_DEFAULT_PORT);
	
	TOKYO_REGISTER_CONST_LONG("RDBQ_CSTREQ", RDBQCSTREQ); 		/* string is equal to */
	TOKYO_REGISTER_CONST_LONG("RDBQ_CSTRINC", RDBQCSTRINC);		/* string is included in */
  	TOKYO_REGISTER_CONST_LONG("RDBQ_CSTRBW", RDBQCSTRBW);		/* string begins with */
	TOKYO_REGISTER_CONST_LONG("RDBQ_CSTREW", RDBQCSTREW);		/* string ends with */
	TOKYO_REGISTER_CONST_LONG("RDBQ_CSTRAND", RDBQCSTRAND);		/* string includes all tokens in */
	TOKYO_REGISTER_CONST_LONG("RDBQ_CSTROR", RDBQCSTROR);		/* string includes at least one token in */
	TOKYO_REGISTER_CONST_LONG("RDBQ_CSTROREQ", RDBQCSTROREQ);	/* string is equal to at least one token in */
	TOKYO_REGISTER_CONST_LONG("RDBQ_CSTRRX", RDBQCSTRRX);		/* string matches regular expressions of */
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNUMEQ", RDBQCNUMEQ);		/* number is equal to */                            
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNUMGT", RDBQCNUMGT);		/* number is greater than */                        
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNUMGE", RDBQCNUMGE);		/* number is greater than or equal to */           
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNUMLT", RDBQCNUMLT);		/* number is less than */                           
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNUMLE", RDBQCNUMLE);		/* number is less than or equal to */               
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNUMBT", RDBQCNUMBT);		/* number is between two tokens of */              
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNUMOREQ", RDBQCNUMOREQ);	/* number is equal to at least one token in */     
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNEGATE", RDBQCNEGATE);		/* negation flag */                                 
	TOKYO_REGISTER_CONST_LONG("RDBQ_CNOIDX", RDBQCNOIDX);		/* no index flag */     
	
	TOKYO_REGISTER_CONST_LONG("RDBQO_STRASC", RDBQOSTRASC);		/* string ascending */
	TOKYO_REGISTER_CONST_LONG("RDBQO_STRDESC", RDBQOSTRDESC);	/* string descending */
	TOKYO_REGISTER_CONST_LONG("RDBQO_NUMASC", RDBQONUMASC);		/* number ascending */
	TOKYO_REGISTER_CONST_LONG("RDBQO_NUMDESC", RDBQONUMDESC);	/* number descending */
	
	TOKYO_REGISTER_CONST_LONG("RDBIT_LEXICAL", RDBITLEXICAL);
	TOKYO_REGISTER_CONST_LONG("RDBIT_DECIMAL", RDBITDECIMAL);
	TOKYO_REGISTER_CONST_LONG("RDBIT_OPT", RDBITOPT);
	TOKYO_REGISTER_CONST_LONG("RDBIT_VOID", RDBITVOID);
	TOKYO_REGISTER_CONST_LONG("RDBIT_KEEP", RDBITKEEP);
	
	/* Not required, these are passed in during connecting
	TOKYO_REGISTER_CONST_LONG("RDBTRECON", RDBTRECON); */   
	
	TOKYO_REGISTER_CONST_LONG("RDB_RECINT", PHP_TOKYO_TYRANT_RECTYPE_INT);
	TOKYO_REGISTER_CONST_LONG("RDB_RECDOUBLE", PHP_TOKYO_TYRANT_RECTYPE_DOUBLE);
	
#undef TOKYO_REGISTER_CONST_LONG

	php_session_register_module(ps_tokyo_tyrant_ptr);
	php_tokyo_hash_func = php_tokyo_tyrant_simple_hash;
	return SUCCESS;
}

static int php_tokyo_tyrant_hash_dtor(php_tokyo_tyrant_conn **datas TSRMLS_DC) 
{
	php_tokyo_tyrant_conn *conn = *datas;
	php_tokyo_tyrant_disconnect(conn);

	free(conn);
	conn = NULL;
	
	return ZEND_HASH_APPLY_REMOVE;
}

PHP_MSHUTDOWN_FUNCTION(tokyo_tyrant)
{
	if (TOKYO_G(connections)) {
		zend_hash_apply(TOKYO_G(connections), (apply_func_t)php_tokyo_tyrant_hash_dtor TSRMLS_CC);
		free(TOKYO_G(connections));
		TOKYO_G(connections) = NULL;
	}
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_MINFO_FUNCTION(tokyo_tyrant)
{
	php_info_print_table_start();
	
		php_info_print_table_header(2, "tokyo_tyrant extension", "enabled");
		php_info_print_table_row(2, "tokyo_tyrant extension version", PHP_TOKYO_TYRANT_EXTVER);
	
	php_info_print_table_end();
}

zend_module_entry tokyo_tyrant_module_entry =
{
        STANDARD_MODULE_HEADER,
        "tokyo_tyrant",
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



