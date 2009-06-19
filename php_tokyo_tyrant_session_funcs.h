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

#ifndef _PHP_TOKYO_TYRANT_SESSION_FUNCS_H_
# define _PHP_TOKYO_TYRANT_SESSION_FUNCS_H_

php_tokyo_tyrant_session *php_tokyo_session_init();

void php_tokyo_session_deinit(php_tokyo_tyrant_session *session);

void php_tokyo_session_append(php_tokyo_tyrant_session *session, php_url *url);

zend_bool php_tokyo_session_store(php_tokyo_tyrant_session *session, char*, char *, int, const char *data, int data_len);

char *php_tokyo_tyrant_session_retrieve(php_tokyo_tyrant_session *session, const char *session_id, int *data_len);

zend_bool php_tokyo_session_delete_where(php_tokyo_tyrant_session *session, char *key, char *value, int limit);

zend_bool php_tokyo_session_destroy(php_tokyo_tyrant_session *session, char *pk, int pk_len);

char *php_tokyo_tyrant_generate_pk(php_tokyo_tyrant_session *session, int *pk_len);

char *php_tokyo_tyrant_create_sid(char *rand_part, int idx, char *pk, char *salt);

zend_bool php_tokyo_tyrant_tokenize_session(char *orig_sess_id, char **sess_rand, char **checksum, int *idx, char **pk_str);

char *php_tokyo_tyrant_create_checksum(char *rand_part, int idx, char *pk, char *salt);

char *php_tokyo_tyrant_session_retrieve_ex(php_tokyo_tyrant_session *session, char *, const char *pk, int pk_len, int *data_len, zend_bool *mismatch);

int php_tokyo_tyrant_session_connect_ex(php_tokyo_tyrant_session *session, int idx);

int php_tokyo_tyrant_session_connect(php_tokyo_tyrant_session *session, char *key);

zend_bool php_tokyo_session_touch(php_tokyo_tyrant_session *session, char *old_rand, char *rand_part, char *pk, int pk_len);

void php_tokyo_tyrant_force_session_regen(php_tokyo_tyrant_session *session TSRMLS_DC);

#endif