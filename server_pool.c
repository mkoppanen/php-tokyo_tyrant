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

php_tt_server *php_tt_server_init(char *host, int port TSRMLS_DC) 
{
	php_tt_server *server = emalloc(sizeof(server));
	
	server->host = estrdup(host);
	server->port = port;
	
	return server;
}

void php_tt_server_deinit(php_tt_server *server TSRMLS_DC) 
{
	efree(server->host);
	efree(server);
}

php_tt_server_pool *php_tt_pool_init(TSRMLS_D) 
{
	php_tt_server_pool *pool = emalloc(sizeof(php_tt_server_pool));

	pool->servers     = emalloc(sizeof(php_tt_server *));
	pool->num_servers = 0;

	return pool;
}

void php_tt_pool_append(php_tt_server_pool *pool, php_tt_server *server TSRMLS_DC) 
{
	pool->servers = erealloc(pool->servers, (pool->num_servers + 1) * sizeof(php_tt_server));
	pool->servers[pool->num_servers] = server;
	pool->num_servers += 1;
}

void php_tt_pool_append2(php_tt_server_pool *pool, char *host, int port TSRMLS_DC) 
{
	php_tt_server *server = emalloc(sizeof(php_tt_server));
	server->host = estrdup(host);
	server->port = port;
	
	php_tt_pool_append(pool, server);
}

void php_tt_pool_deinit(php_tt_server_pool *pool TSRMLS_DC) 
{
	if (pool->num_servers > 0) {
		int i;
		
		for (i = 0; i < pool->num_servers; i++) {
			php_tt_server_deinit(pool->servers[i]);
			pool->servers[i] = NULL;
		}
		efree(pool->servers);
	}
	efree(pool);
}

php_tt_server_pool *php_tt_pool_init2(const char *save_path TSRMLS_DC)
{
	php_tt_server_pool *pool = php_tt_pool_init(TSRMLS_C);
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
			return NULL;
		}
		php_tt_pool_append2(pool, url->host, url->port TSRMLS_CC);
		php_url_free(url);
		url = NULL;
		pch = strtok(NULL, ",");
	}
	efree(ptr);
	return pool;
}


int php_tt_pool_map(php_tt_server_pool *pool, char *key TSRMLS_DC)
{
	php_tt_server *server;
	int idx = -1;
	
	srand(time(NULL));
	idx = rand() % pool->num_servers;
	server = pool->servers[idx];
	
	/* Server has failed. Check neighbours*/
	if (php_tt_server_color(server->host, server->port TSRMLS_CC) == PHP_TT_COLOR_RED) {
		int i, x = rand() % pool->num_servers;
		
		for (i = x; i < pool->num_servers; i++) {
			if (i != idx) {
				server = pool->servers[i];
				if (php_tt_server_color(server->host, server->port TSRMLS_CC) < PHP_TT_COLOR_RED) {
					return i;
				}
			}
		}
		
		for (i = x; i >= 0; i--) {
			if (i != idx) {
				server = pool->servers[i];
				if (php_tt_server_color(server->host, server->port TSRMLS_CC) < PHP_TT_COLOR_RED) {
					return i;
				}
			}
		}
		idx = -1;
	}
	return idx;
}

php_tt_server *php_tt_pool_get_server(php_tt_server_pool *pool, int idx TSRMLS_DC)
{
	if (idx >= pool->num_servers) {
		return NULL;
	}
	
	return pool->servers[idx];
}



