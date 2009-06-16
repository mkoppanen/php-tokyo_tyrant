/*
  +----------------------------------------------------------------------+
  | PHP Version 5 / Tokyo tyrant session handler                         |
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

#include "ext/standard/sha1.h"

php_tokyo_tyrant_session *php_tokyo_session_init() 
{	
	php_tokyo_tyrant_session *session = emalloc(sizeof(php_tokyo_tyrant_session));
	
	session->host       = NULL;
	session->port       = NULL;
	session->timeout    = NULL;
	session->server_count = 0;
	
	session->pk         = NULL;
	session->pk_len     = -1;
	
	session->rand_part  = NULL;
	session->checksum   = NULL;
	
	session->force_regen = 0;
	
	session->obj_conn   = emalloc(sizeof(php_tokyo_tyrant_object));
	php_tokyo_tyrant_init_tt_object(session->obj_conn TSRMLS_CC);
	
	return session;
}

void php_tokyo_session_deinit(php_tokyo_tyrant_session *session) 
{
	if (session->server_count > 0) {
		size_t i;
		for (i = 0; i < session->server_count; i++) {
			efree(session->host[i]);
		}
		efree(session->port);
		efree(session->timeout);
		efree(session->host);
	}
	if (session->obj_conn)
		efree(session->obj_conn);

	if (session->pk)
		efree(session->pk);

	if (session->rand_part)
		efree(session->rand_part);
		
	if (session->checksum)
		efree(session->checksum);
		
	efree(session);
}

int php_tokyo_tyrant_session_connect_ex(php_tokyo_tyrant_session *session, int idx) 
{
	/* Node does not exist */
	if (idx > session->server_count || idx < 0) {
		return -1;
	}
	
	if (!php_tokyo_tyrant_connect_ex(session->obj_conn, session->host[idx], session->port[idx], TOKYO_G(default_timeout), 0, 1)) {
		return -1;
	}
	return idx;
}

int php_tokyo_tyrant_session_connect(php_tokyo_tyrant_session *session, char *key) 
{
	int idx = php_tokyo_hash_func(session, key);
	
	/* Mapping fails */
	if (idx == -1) {
		return -1;
	}
	
	/* Node does not exist */
	return php_tokyo_tyrant_session_connect_ex(session, idx);
}

char *php_tokyo_tyrant_generate_pk(php_tokyo_tyrant_session *session, int *pk_len)
{
	long pk = -1;
	char *pk_str;
	
	pk = (long)tcrdbtblgenuid(session->obj_conn->conn->rdb);
	*pk_len = 0;
	
	if (pk == -1) {
		return NULL;
	}
	
	*pk_len = spprintf(&pk_str, 256, "%ld", pk);
	return pk_str;
}

void php_tokyo_session_append(php_tokyo_tyrant_session *session, php_url *url) 
{
	int pos = session->server_count;
	session->server_count += 1;
	
	session->host    = erealloc(session->host,    sizeof(char *) * session->server_count);
	session->port    = erealloc(session->port,    sizeof(int)    * session->server_count);
	session->timeout = erealloc(session->timeout, sizeof(double) * session->server_count);
	
	session->host[pos]    = estrdup(url->host);
	session->port[pos]    = url->port;
	session->timeout[pos] = TOKYO_G(default_timeout);
}

zend_bool php_tokyo_session_touch(php_tokyo_tyrant_session *session, char *old_rand, char *rand_part, char *pk, int pk_len)
{
	char *data;
	int data_len;
	zend_bool mismatch;

	data = php_tokyo_tyrant_session_retrieve_ex(session, old_rand, pk, pk_len, &data_len, &mismatch);

	if (!data) {
		return 1;
	}
	return php_tokyo_session_store(session, rand_part, pk, pk_len, data, data_len);	
}

zend_bool php_tokyo_session_store(php_tokyo_tyrant_session *session, char *rand_part, char *pk, int pk_len, const char *data, int data_len) 
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
	
	tcmapput2(cols, "data", data);
	tcmapput2(cols, "hash", rand_part);
	tcmapput2(cols, "ts",   timestamp);

	if (!tcrdbtblput(session->obj_conn->conn->rdb, pk, pk_len, cols)) {
		tcmapdel(cols);
		return 0;
	}

	tcmapdel(cols);
	return 1;
}

char *php_tokyo_tyrant_session_retrieve_ex(php_tokyo_tyrant_session *session, char *rand_part, const char *pk, int pk_len, int *data_len, zend_bool *mismatch)
{
	TCMAP *cols;
	char *buffer = NULL;

	*data_len = 0;
	*mismatch = 0;

	cols = tcrdbtblget(session->obj_conn->conn->rdb, pk, pk_len);
	
	if (cols) {
		const char *checksum = tcmapget2(cols, "hash");

		/* Make sure that we get back the expected session */
		if (strcmp(checksum, rand_part) == 0) {
			buffer = estrdup(tcmapget2(cols, "data"));
			*data_len = strlen(buffer);
		} else {
			*mismatch = 1;
		}
		tcmapdel(cols);
	}
	return buffer;
}

char *php_tokyo_tyrant_session_retrieve(php_tokyo_tyrant_session *session, const char *session_id, int *data_len)  
{
	int rsiz;
	const char *rbuf;
	RDBQRY *qry;
	TCLIST *res;
	char *buffer = NULL;
	zend_bool mismatch;
	
	*data_len = 0;
	qry = tcrdbqrynew(session->obj_conn->conn->rdb);

	tcrdbqryaddcond(qry, "hash", RDBQCSTROR, session_id);
	tcrdbqrysetlimit(qry, 1, 0);

	res  = tcrdbqrysearch(qry);
	rbuf = tclistval(res, 0, &rsiz);

	if (tclistnum(res) > 0) {
		buffer = php_tokyo_tyrant_session_retrieve_ex(session, session->rand_part, rbuf, rsiz, data_len, &mismatch);
	}
	
	tclistdel(res);
	tcrdbqrydel(qry);
	return buffer;
}

zend_bool php_tokyo_session_delete_where(php_tokyo_tyrant_session *session, char *key, char *value, int limit)
{
	int rsiz, i;
	const char *rbuf;
	TCMAP *cols;
	RDBQRY *qry;
	TCLIST *res;

	qry = tcrdbqrynew(session->obj_conn->conn->rdb);
	
	tcrdbqryaddcond(qry, key, RDBQCSTROR, value);
	tcrdbqrysetlimit(qry, limit, 0);
	
	res = tcrdbqrysearch(qry);
	
	for (i = 0; i < tclistnum(res); i++) {
		rbuf = tclistval(res, i, &rsiz);
		cols = tcrdbtblget(session->obj_conn->conn->rdb, rbuf, rsiz);
		if (cols) {
			tcrdbout2(session->obj_conn->conn->rdb, rbuf);
			tcmapdel(cols);
		}
	}
	tclistdel(res);
	tcrdbqrydel(qry);
	
	return 0;
}

zend_bool php_tokyo_session_destroy(php_tokyo_tyrant_session *session, char *pk, int pk_len) 
{
	if (tcrdbtblout(session->obj_conn->conn->rdb, pk, pk_len)) {
		return 1;
	} else {
		/* TTENOREC means that record did not exist. This should not cause error */
		if (tcrdbecode(session->obj_conn->conn->rdb) == TTENOREC) {
			return 1;
		}
	}
	return 0;
}

char *php_tokyo_tyrant_create_checksum(char *rand_part, int idx, char *pk, char *salt) 
{	
	PHP_SHA1_CTX context;
	char *pk_src;
	int pk_src_len;
	unsigned char digest[20];
	char sha1_str[41];

	/* This is what we need to hash */
	pk_src_len = spprintf(&pk_src, 512, "#%s# #%s# #%d# #%s#", rand_part, salt, idx, pk);
	
	/* sha1 hash the pk_src and compare to session_hash */
	PHP_SHA1Init(&context);
	PHP_SHA1Update(&context, (unsigned char *)pk_src, pk_src_len);
    PHP_SHA1Final(digest, &context);
	
	memset(sha1_str, '\0', 41);
	make_sha1_digest(sha1_str, digest);
	
	efree(pk_src);
	return estrdup(sha1_str);
}

char *php_tokyo_tyrant_create_sid(char *rand_part, int idx, char *pk, char *salt) 
{
	char *tmp, *buffer = php_tokyo_tyrant_create_checksum(rand_part, idx, pk, salt);
	spprintf(&tmp, 512, "%s-%s-%d-%s", rand_part, buffer, idx, pk);
	efree(buffer);
	return tmp;
}

zend_bool php_tokyo_tyrant_tokenize_session(char *orig_sess_id, char **sess_rand, char **checksum, int *idx, char **pk_str) 
{
	int i, matches = 0, ptr_len;
	char *ptr = NULL;
	
	/* Should be a fairly sensible limitation */
	if (strlen(orig_sess_id) >= 512) {
		return 0;
	}
		
	ptr = estrdup(orig_sess_id);
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

char *php_tokyo_tyrant_validate_pk(char *session_id, int session_id_len, char *salt) 
{
	char *sess_rand, *checksum, *pk_str, *sha1_str;
	int idx;
	
	if (session_id_len > 512) {
		return NULL;
	}
	
	if (!php_tokyo_tyrant_tokenize_session(session_id, &sess_rand, &checksum, &idx, &pk_str)) {
		return NULL;
	}
	
	sha1_str = php_tokyo_tyrant_create_checksum(sess_rand, idx, pk_str, salt);
 	
	if (strlen(sha1_str) == strlen(checksum)) {
		if (strcmp(sha1_str, checksum) == 0) {	
			efree(sess_rand);
			efree(checksum);
			
			/* Hash matches, we are set to go */
			efree(sha1_str);
			return pk_str;
		}
	}
	
	efree(sess_rand);
	efree(checksum);
	efree(pk_str);
	efree(sha1_str);
	return NULL;
}
