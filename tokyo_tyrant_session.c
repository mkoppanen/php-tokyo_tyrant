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
#include "php_tokyo_tyrant_session.h"
#include "php_tokyo_tyrant_funcs.h"
#include "php_tokyo_tyrant_session_funcs.h"

ps_module ps_mod_tokyo_tyrant = {
	PS_MOD_SID(tokyo_tyrant)
};

static zend_bool _php_tt_handle_save_path(php_tokyo_tyrant_session *session, const char *save_path) 
{	
	php_url *url = NULL;
	char *ptr = NULL, *pch = NULL;
	
	ptr = estrdup(save_path);
	pch = strtok(ptr, ",");
	
	while (pch != NULL) {
		url = php_url_parse(pch);
		if (!url) {
			if (ptr) {
				efree(ptr);
			}
			return 0;
		}
		php_tokyo_session_append(session, url TSRMLS_CC);
		php_url_free(url);
		pch = strtok(NULL, ",");
	}
	efree(ptr);
	return 1;
}

PS_CREATE_SID_FUNC(tokyo_tyrant)
{
	char *sid, *pk = NULL, *rand_part, *old_rand_part;
	int idx = -1, pk_len;

	/* Create temporary session */
	php_tokyo_tyrant_session *session;

	/* Session id is being regenerated. Need to copy some data */
	if (PS(session_status) == php_session_active) {
		php_tokyo_tyrant_session *old_data = PS_GET_MOD_DATA();
		idx           = old_data->idx;
		pk            = estrdup(old_data->pk);
		old_rand_part = estrdup(old_data->rand_part);
	}
	
	session = php_tokyo_session_init();

	/* Session id format: [random]-[checksum]-[node_id]-[pk] 
		checksum = random-nodeid-pk-salt */
	rand_part = php_session_create_id(PS(mod_data), NULL TSRMLS_CC);

	/* parse save path */
	if (!_php_tt_handle_save_path(session, PS(save_path))) {
		efree(pk);
		pk = NULL;
		
		efree(old_rand_part);
		old_rand_part = NULL;
		php_tokyo_session_deinit(session);
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to parse session.save_path");
	}

	/* If we have proper idx here it means we don't have to renegerate and copy data */
	if (idx == -1) { 
		idx = php_tokyo_tyrant_session_connect(session, rand_part);

		/* primary key for this session */
		pk = php_tokyo_tyrant_generate_pk(session, &pk_len);
	} else {
		if (php_tokyo_tyrant_session_connect_ex(session, idx) == -1) {
			efree(pk);
			pk = NULL;
			efree(old_rand_part);
			old_rand_part = NULL;
			php_tokyo_session_deinit(session);
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to connect to session server");
		}

		if (!php_tokyo_session_touch(session, old_rand_part, rand_part, pk, strlen(pk))) {
			efree(pk);
			pk = NULL;
			efree(old_rand_part);
			old_rand_part = NULL;
			php_tokyo_session_deinit(session);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to store session data");
		}
		if (old_rand_part)
			efree(old_rand_part);
	}

	/* Checksum rand_part, salt, pk and idx */
	sid = php_tokyo_tyrant_create_sid(rand_part, idx, pk, TOKYO_G(salt));

	efree(rand_part);
	efree(pk);

	php_tokyo_session_deinit(session);
	return sid;
}

/* {{{ PS_OPEN*/
PS_OPEN_FUNC(tokyo_tyrant)
{	
	php_tokyo_tyrant_session *session = php_tokyo_session_init();

	if (!session) {
		PS_SET_MOD_DATA(NULL);
		return FAILURE;
	}

	if (!_php_tt_handle_save_path(session, save_path)) {
		php_tokyo_session_deinit(session);
		PS_SET_MOD_DATA(NULL);
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to parse session.save_path");
	}
	PS_SET_MOD_DATA(session);
}

PS_READ_FUNC(tokyo_tyrant)
{
	TT_SESS_DATA;
	char *rand_part, *checksum, *pk_str, *buff;
	
	if (!php_tokyo_tyrant_tokenize_session(key, &(session->rand_part), &(session->checksum), &(session->idx), &(session->pk))) {
		/* Failure in parsing.. */
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to parse the session id");
	}
	session->pk_len = strlen(session->pk);

	buff = php_tokyo_tyrant_create_checksum(session->rand_part, session->idx, session->pk, TOKYO_G(salt));

	if (strcmp(session->checksum, buff) != 0) {
		efree(buff);
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Session verification failed. Possibly tampered session");
	}
	efree(buff);

	/* Use session->idx to connect, not rand_part based hashing */
	if (php_tokyo_tyrant_session_connect_ex(session, session->idx) == -1) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to connect to session server");
	}

	*val = php_tokyo_tyrant_session_retrieve_ex(session, session->rand_part, session->pk, session->pk_len, vallen);

	if (*val == NULL) {
		*val = estrdup("");
	}
	return SUCCESS;
}

PS_WRITE_FUNC(tokyo_tyrant)
{
	TT_SESS_DATA;
	char *rand_part, *checksum, *pk;
	int idx, retcode;

	if (!php_tokyo_tyrant_tokenize_session(key, &rand_part, &checksum, &idx, &pk)) {
		/* Failure in parsing */
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Failed to parse the session id");
	}	
	
	retcode = php_tokyo_session_store(session, rand_part, pk, strlen(pk), val, vallen);
	efree(rand_part);
	efree(checksum);
	efree(pk);

	if (!retcode) {	
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to store session data");
		return FAILURE;
	}
	
	return SUCCESS;
}

PS_DESTROY_FUNC(tokyo_tyrant)
{
	TT_SESS_DATA;
	
	if (php_tokyo_session_destroy(session, session->pk, session->pk_len) != 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to destroy the session");
		return FAILURE;
	}
	return SUCCESS;
}

PS_GC_FUNC(tokyo_tyrant)
{
	return SUCCESS;
}

PS_CLOSE_FUNC(tokyo_tyrant)
{
	php_tokyo_tyrant_session *session = PS_GET_MOD_DATA();
	php_tokyo_session_deinit(session);
	session = NULL;
	
	PS_SET_MOD_DATA(NULL);
	return SUCCESS;
}


