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

#ifndef _PHP_TOKYO_SESSION_H_
# define _PHP_TOKYO_SESSION_H_

typedef struct _php_tokyo_tyrant_session {
	char  **host;
	int    *port;
	double *timeout;
	
	size_t server_count;
	php_tokyo_tyrant_object *obj_conn;
	
	char *pk;
	int  pk_len;
	
	char *rand_part;
	char *checksum;
	
	int idx;
	zend_bool force_regen;
	
} php_tokyo_tyrant_session;

int (*php_tokyo_hash_func)(php_tokyo_tyrant_session *session, char *key);
#endif


