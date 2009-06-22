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
#ifndef _PHP_TOKYO_TYRANT_SESSION_H_
# define _PHP_TOKYO_TYRANT_SESSION_H_

#include "php_tokyo_tyrant.h"
#include "php_tokyo_tyrant_private.h"
#include "php_tokyo_tyrant_connection.h"
#include "php_tokyo_tyrant_server_pool.h"
#include "php_tokyo_tyrant_failover.h"
#include "php_tokyo_tyrant_funcs.h"

PS_CREATE_SID_FUNC(tokyo_tyrant);

typedef struct _php_tt_session {
	php_tt_server_pool *pool; /* Pool of session servers */
	php_tt_conn *conn;        /* Connection to the session server */
	
	char *pk;           /* Primary key of the session data */
	int   pk_len;       /* pk length */
	int   idx;          /* node where the data is stored at */
	
	char *sess_rand;          /* The session id part */
	int   sess_rand_len;      /* session id length */
	
	char *checksum;     /* Session validation checksum */
	int   checksum_len; /* Checksum length */
	
	zend_bool remap;    /* If 1 create_sid will remap the session to a new server */

} php_tt_session;

#endif