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

#include "php_tokyo_tyrant_session.h"
#include "php_tokyo_tyrant_session_funcs.h"

ps_module ps_mod_tokyo_tyrant = {
	PS_MOD_SID(tokyo_tyrant)
};

#define TT_SESS_DATA php_tt_session *session = PS_GET_MOD_DATA();

/* {{{ Create new session id */
PS_CREATE_SID_FUNC(tokyo_tyrant)
{
	php_tt_conn *conn;
	php_tt_server *server;
	php_tt_server_pool *pool;
	
	char *current_rand = NULL;
	char *sess_rand, *sid, *pk = NULL;
	int idx = -1, pk_len;
	
	if (!TOKYO_G(salt)) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "tokyo_tyrant.session_salt needs to be set in order to use the session handler");
	}

	/* Session id is being regenerated. Need to copy some data */
	if (PS(session_status) == php_session_active) {
		TT_SESS_DATA;

		/* Use old values unless regeneration is forced */
		if (session->remap == 0) {
			idx          = session->idx;
			pk           = estrdup(session->pk);
			current_rand = estrdup(session->sess_rand);
		} else {
			session->remap = 0;
		}
	}

	/* Create the random part of the session id */
	sess_rand = php_session_create_id(PS(mod_data), NULL TSRMLS_CC);

	if (!sess_rand) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to generate session id");
	}

	/* Init the server pool */
	pool = php_tt_pool_init2(PS(save_path) TSRMLS_CC);
	
	if (!pool) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to parse session.save_path");
	}

	/* Create idx if there isn't one already */
	if (idx == -1) {	
		idx = php_tt_pool_map(pool, sess_rand TSRMLS_CC);

		if (idx < 0) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to map the session to a server");
		}
	}
	
	/* Get the server for the node */
	server = php_tt_pool_get_server(pool, idx TSRMLS_CC);
	
	if (!server) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Internal error: idx does not map to a server (should not happen)");
	}

	/* Create connection to the server */
	conn = php_tt_conn_init(TSRMLS_C);
	if (!php_tt_connect_ex(conn, server->host, server->port, TOKYO_G(default_timeout), 1 TSRMLS_CC)) {
		php_tt_server_fail_incr(server->host, server->port TSRMLS_CC);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to connect to the session server");
	}
	
	if (!pk) {
		pk = php_tt_create_pk(conn, &pk_len TSRMLS_CC);
	} else {
		if (!php_tt_sess_touch(conn, current_rand, sess_rand, pk, strlen(pk) TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to update the session");
		}
		efree(current_rand);
	}

	sid = php_tt_create_sid(sess_rand, idx, pk, TOKYO_G(salt) TSRMLS_CC); 

	efree(sess_rand);
	efree(pk);

	php_tt_conn_deinit(conn TSRMLS_CC);
	php_tt_pool_deinit(pool TSRMLS_CC);
	return sid;
}
/* }}} */

/* {{{ PS_OPEN */
PS_OPEN_FUNC(tokyo_tyrant)
{	
	php_tt_session *session = php_tt_session_init(TSRMLS_C);

	if (!session) {
		PS_SET_MOD_DATA(NULL);
		return FAILURE;
	}
	
	session->pool = php_tt_pool_init2(PS(save_path) TSRMLS_CC);
	if (!session->pool) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to parse session.save_path");
	}
	
	PS_SET_MOD_DATA(session);
	return SUCCESS;
}
/* }}} */

PS_READ_FUNC(tokyo_tyrant)
{
	TT_SESS_DATA;
	php_tt_server *server;
	zend_bool mismatch;

	/* Try to tokenize session id */
	if (!php_tt_tokenize((char *)key, &(session->sess_rand), &(session->checksum), &(session->idx), &(session->pk) TSRMLS_CC)) {
		session->remap         = 1;
		PS(invalid_session_id) = 1;
		return FAILURE;
	}
	
	/* Set additional data */
	session->sess_rand_len = strlen(session->sess_rand);
	session->checksum_len  = strlen(session->checksum);
	session->pk_len        = strlen(session->pk);
	
	/* Validate the session id */
	if (!php_tt_validate(session->sess_rand, session->checksum, session->idx, session->pk, TOKYO_G(salt) TSRMLS_CC)) {
		session->remap         = 1;
		PS(invalid_session_id) = 1;
		return FAILURE;
	}
	
	server = php_tt_pool_get_server(session->pool, session->idx TSRMLS_CC);
	
	if (!server) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Internal error: idx does not map to a server");
		session->remap         = 1;
		PS(invalid_session_id) = 1;
		return FAILURE;
	}

	session->conn = php_tt_conn_init(TSRMLS_C);
	
	if (!php_tt_connect_ex(session->conn, server->host, server->port, TOKYO_G(default_timeout), 1 TSRMLS_CC)) {
		php_tt_server_fail_incr(server->host, server->port TSRMLS_CC);
		
		/* Remap if the server has been failed */
		if (!php_tt_server_ok(server->host, server->port TSRMLS_CC)) {
			session->remap         = 1;
			PS(invalid_session_id) = 1;
			return FAILURE;
		}
	}
	
	*val = php_tt_get_sess_data(session->conn, session->sess_rand, session->pk, session->pk_len, vallen, &mismatch TSRMLS_CC);

	if (*val == NULL) {
		/* Session got mapped to wrong server */
		if (mismatch) {
			session->remap         = 1;
			PS(invalid_session_id) = 1;
			return FAILURE;
		}
		/* Return empty data */
		*val = estrdup("");
	}

	return SUCCESS;
}

PS_WRITE_FUNC(tokyo_tyrant)
{
	TT_SESS_DATA;
	php_tt_server *server;

	/* Try to tokenize session id */
	efree(session->sess_rand);
	efree(session->checksum);
	efree(session->pk);
	
	if (!php_tt_tokenize((char *)key, &(session->sess_rand), &(session->checksum), &(session->idx), &(session->pk) TSRMLS_CC)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse the session id");
		session->remap         = 1;
		PS(invalid_session_id) = 1;
		return FAILURE;
	}
	
	/* Set additional data */
	session->sess_rand_len = strlen(session->sess_rand);
	session->checksum_len  = strlen(session->checksum);
	session->pk_len        = strlen(session->pk);
	
	if (!php_tt_validate(session->sess_rand, session->checksum, session->idx, session->pk, TOKYO_G(salt) TSRMLS_CC)) {
		return FAILURE;
	}	
	
	if (!php_tt_save_sess_data(session->conn, session->sess_rand, session->pk, strlen(session->pk), val, vallen TSRMLS_CC)) {
		server = php_tt_pool_get_server(session->pool, session->idx TSRMLS_CC);
		php_tt_server_fail_incr(server->host, server->port TSRMLS_CC);
		
		/* Remap if the server has been marked as failed */
		if (!php_tt_server_ok(server->host, server->port TSRMLS_CC)) {
			session->remap         = 1;
			PS(invalid_session_id) = 1;
			return FAILURE;
		}
		
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to store session data");
		return FAILURE;
	}

	return SUCCESS;
}

PS_DESTROY_FUNC(tokyo_tyrant)
{
	TT_SESS_DATA;
	zend_bool res;

	res = php_tt_sess_destroy(session->conn, session->pk, session->pk_len TSRMLS_CC);
	php_tt_session_deinit(session TSRMLS_CC);
	session = NULL;

	PS_SET_MOD_DATA(NULL);

	if (!res) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to destroy the session");
		return FAILURE;
	}
	return SUCCESS;
}

PS_GC_FUNC(tokyo_tyrant)
{
	/* Handle expiration on PHP side? */
	if (TOKYO_G(php_expiration)) {
		TT_SESS_DATA;
		return php_tt_gc(session->pool TSRMLS_CC);
	}
	
	return SUCCESS;
}

PS_CLOSE_FUNC(tokyo_tyrant)
{
	TT_SESS_DATA;

	php_tt_session_deinit(session TSRMLS_CC);
	session = NULL;
	
	PS_SET_MOD_DATA(NULL);
	return SUCCESS;
}