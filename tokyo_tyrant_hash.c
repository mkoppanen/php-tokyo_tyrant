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
#include "php_tokyo_tyrant_session.h"

/* modulo hashing */
int php_tokyo_tyrant_simple_hash(php_tokyo_tyrant_session *session, char *key)
{
	unsigned int hash = 5381;
	const char *s;

	if (session->server_count == 0) {
		return -1;
	}

	if (key == NULL) 
		return 0;

	for (s = key; *s; s++) {
		hash = ((hash << 5) + hash) + *s;
	}

	hash &= 0x7FFFFFFF;	
	return hash % session->server_count;
}

/* TODO: other hashing functions (?) */

















