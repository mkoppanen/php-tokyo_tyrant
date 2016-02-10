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
#include "ext/standard/sha1.h"

#define PHP_TT_CHECKSUM_LEN 41
 
php_tt_session *php_tt_session_init() 
{	
	php_tt_session *session = emalloc(sizeof(php_tt_session));
	
	session->pool    = NULL;
	session->conn    = NULL;
	
	session->pk        = NULL;
	session->sess_rand = NULL;
	
	session->checksum = NULL;
	session->remap    = 0;
		
	return session;
}

void php_tt_session_deinit(php_tt_session *session) 
{
	if (session->conn) {
		php_tt_conn_deinit(session->conn TSRMLS_CC);
		session->conn = NULL;
	}
	
	if (session->pool) {
		php_tt_pool_deinit(session->pool TSRMLS_CC);
		session->pool = NULL;
	}

	if (session->pk) {
		efree(session->pk);
		session->pk = NULL;
	}
	
	if (session->sess_rand) {
		efree(session->sess_rand);
		session->sess_rand = NULL;
	}
	
	if (session->checksum) {
		efree(session->checksum);
		session->checksum = NULL;
	}
	
	efree(session);
}

static void php_tt_checksum(char *sess_rand, int idx, char *pk, char *salt, char checksum[PHP_TT_CHECKSUM_LEN]) 
{	
	PHP_SHA1_CTX context;
	char pk_src[512];
	int pk_src_len;
	unsigned char digest[20];

	/* This is what we need to hash */
	pk_src_len = snprintf(pk_src, 512, "#%s# #%s# #%d# #%s#", sess_rand, salt, idx, pk);
	
	/* sha1 hash the pk_src and compare to session_hash */
	PHP_SHA1Init(&context);
	PHP_SHA1Update(&context, (unsigned char *)pk_src, pk_src_len);
    PHP_SHA1Final(digest, &context);
	
	make_sha1_digest(checksum, digest);
	checksum[40] = '\0';
}

char *php_tt_create_sid(char *sess_rand, int idx, char *pk, char *salt) 
{
	char *tmp, buffer[PHP_TT_CHECKSUM_LEN];
	
	php_tt_checksum(sess_rand, idx, pk, salt, buffer);
	spprintf(&tmp, 512, "%s-%s-%d-%s", sess_rand, buffer, idx, pk);

	return tmp;
}

char *php_tt_create_pk(php_tt_conn *conn, int *pk_len)
{
	long pk = -1;
	char *pk_str;
	
	pk = (long)tcrdbtblgenuid(conn->rdb);
	*pk_len = 0;
	
	if (pk == -1) {
		return NULL;
	}
	
	*pk_len = spprintf(&pk_str, 256, "%ld", pk);
	return pk_str;
}

zend_bool php_tt_tokenize(char *session_id, char **sess_rand, char **checksum, int *idx, char **pk_str) 
{
	int i, matches = 0, ptr_len;
	char *ptr = NULL;
	
	/* Should be a fairly sensible limitation */
	if (!session_id || strlen(session_id) >= 512) {
		return 0;
	}
		
	ptr     = estrdup(session_id);
	ptr_len = strlen(ptr);

	/* Make it easy to sscanf */
	for (i = 0; i < ptr_len; i++) {
		if (ptr[i] == '-') {
			ptr[i] = ' ';
		}
	}

	*sess_rand = emalloc(65);
	*checksum  = emalloc(41);
	*pk_str    = emalloc(65);
	
	memset(*sess_rand, '\0', 65);
	memset(*checksum,  '\0', 41);
	memset(*pk_str,    '\0', 65);

	/* TODO: does this cost alot? */
	matches = sscanf(ptr, "%64s %40s %d %64s", *sess_rand, *checksum, &(*idx), *pk_str);
	efree(ptr);

	if (matches == 4) {
		return 1;		
	}	
	
	efree(*sess_rand);
	*sess_rand = NULL;
	
	efree(*checksum);
	*checksum = NULL;
	
	efree(*pk_str);
	*pk_str = NULL;
	
	return 0;	
}

zend_bool php_tt_validate(char *sess_rand, char *checksum, int idx, char *pk, char *salt TSRMLS_DC) 
{
	int code = 0;
	char buffer[PHP_TT_CHECKSUM_LEN];
	php_tt_checksum(sess_rand, idx, pk, salt, buffer);
	
	if (strlen(checksum) == strlen(buffer)) {
		if (strcmp(checksum, buffer) == 0) {
			code = 1;
		}
	}
	return code;
}

char *php_tt_get_sess_data(php_tt_conn *conn, char *sess_rand, const char *pk, int pk_len, int *data_len, zend_bool *mismatch TSRMLS_DC)
{
	TCMAP *cols;
	char *buffer = NULL;

	*data_len = 0;
	*mismatch = 0;

	cols = tcrdbtblget(conn->rdb, pk, pk_len);
	
	if (cols) {
		const char *checksum = tcmapget2(cols, "hash");
		/* Make sure that we get back the expected session */
		if (strcmp(checksum, sess_rand) == 0) {
			const char *data;
			data = tcmapget(cols, "data", sizeof("data") - 1, data_len);

			if (data) {
				buffer = emalloc(*data_len);
				memcpy(buffer, data, *data_len);
			}
		} else {
			*mismatch = 1;
		}
		tcmapdel(cols);
	}
	return buffer;
}

zend_bool php_tt_save_sess_data(php_tt_conn *conn, char *rand_part, char *pk, int pk_len, const char *data, int data_len TSRMLS_DC) 
{
	TCMAP *cols;
	char timestamp[64];
	time_t expiration;

	if (!data) {
		return 1;
	}
	
	/* Expiration is handled by lua script */
	expiration = SG(global_request_time) + PS(gc_maxlifetime);
	
	memset(timestamp, '\0', 64);
	sprintf(timestamp, "%ld", expiration);	

	/* store a record */
	cols = tcmapnew();
	
	tcmapput(cols, "data", sizeof("data") - 1, data, data_len);
	tcmapput2(cols, "hash", rand_part);
	tcmapput2(cols, "ts",   timestamp);

	if (!tcrdbtblput(conn->rdb, pk, pk_len, cols)) {
		tcmapdel(cols);
		return 0;
	}
	
	tcmapdel(cols);
	return 1;
}

zend_bool php_tt_sess_touch(php_tt_conn *conn, char *current_rand, char *sess_rand, char *pk, int pk_len TSRMLS_DC)
{
	char *data;
	int data_len;
	zend_bool mismatch;

	data = php_tt_get_sess_data(conn, current_rand, pk, pk_len, &data_len, &mismatch TSRMLS_CC);

	if (!data) {
		return 1;
	}
	return php_tt_save_sess_data(conn, sess_rand, pk, pk_len, data, data_len TSRMLS_CC);	
}

zend_bool php_tt_sess_destroy(php_tt_conn *conn, char *pk, int pk_len TSRMLS_DC)
{
	if (tcrdbtblout(conn->rdb, pk, pk_len)) {
		return 1;
	} else {
		/* TTENOREC means that record did not exist. This should not cause error */
		if (tcrdbecode(conn->rdb) == TTENOREC) {
			return 1;
		}
	}
	return 0;
}

zend_bool php_tt_gc(php_tt_server_pool *pool TSRMLS_DC)
{
	int i;
	zend_bool overal_res = SUCCESS;
	char timestamp[64];

	memset(timestamp, '\0', 64);
	sprintf(timestamp, "%ld", SG(global_request_time));

	for (i = 0; i < pool->num_servers; i++) {
		php_tt_server *server;
		php_tt_conn *conn;
		
		RDBQRY *query;

		server = php_tt_pool_get_server(pool, i TSRMLS_CC);
		conn   = php_tt_conn_init(TSRMLS_C);
		
		if (!php_tt_connect_ex(conn, server->host, server->port, TOKYO_G(default_timeout), 1 TSRMLS_CC)) {
			overal_res = FAILURE;
			continue;
		}
		
		query = tcrdbqrynew(conn->rdb);
		tcrdbqryaddcond(query, "ts", RDBQCNUMLT, timestamp);
		
		if (!tcrdbqrysearchout(query)) {
			php_tt_server_fail_incr(server->host, server->port TSRMLS_CC);
			overal_res = FAILURE;
			break;
		}

		tcrdbqrydel(query);
		php_tt_conn_deinit(conn TSRMLS_CC);
	}
	return overal_res;
}

#if 0
void php_tt_regen_id(php_tt_session *session TSRMLS_DC) 
{
	zval *fname, retval;

	/* Force regeneration of id and force session to be active */
	session->remap       = 1;
	PS(session_status)   = php_session_active;

	MAKE_STD_ZVAL(fname);
	ZVAL_STRING(fname, "session_regenerate_id", 1);

	call_user_function(EG(function_table), NULL, fname, &retval, 0, NULL TSRMLS_CC);
	session->remap = 0;
	zval_dtor(fname);
	FREE_ZVAL(fname);
}
#endif