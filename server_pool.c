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
#include "ext/standard/php_rand.h"

static php_tt_server *php_tt_server_init(char *host, int port) 
{
	php_tt_server *server = emalloc(sizeof(server));
	
	server->host = estrdup(host);
	server->port = port;
	
	return server;
}

static void php_tt_server_deinit(php_tt_server *server TSRMLS_DC) 
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
	php_tt_server *server = php_tt_server_init(host, port);
	php_tt_pool_append(pool, server TSRMLS_CC);
}

void php_tt_pool_deinit(php_tt_server_pool *pool TSRMLS_DC) 
{
	if (pool->num_servers > 0) {
		int i;
		
		for (i = 0; i < pool->num_servers; i++) {
			php_tt_server_deinit(pool->servers[i] TSRMLS_CC);
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
			goto failure;
		}
		
		if (!url->host || !url->port) {
			php_url_free(url);
			goto failure;
		}
		
		php_tt_pool_append2(pool, url->host, url->port TSRMLS_CC);
		php_url_free(url);
		url = NULL;
		pch = strtok(NULL, ",");
	}
	efree(ptr);
	return pool;
	
failure:
	if (ptr)
		efree(ptr);
	return NULL;
}

int php_tt_pool_map(php_tt_server_pool *pool, char *key TSRMLS_DC)
{
	php_tt_server *server;
	int idx = -1;
	
	if (pool->num_servers == 0) {
		return -1;
	}
	
	idx    = php_rand(TSRMLS_C) % pool->num_servers;
	server = pool->servers[idx];
	
	/* Server has failed. Get a new random server */
	if (!php_tt_server_ok(server->host, server->port TSRMLS_CC)) {
		int i, x = php_rand(TSRMLS_C) % pool->num_servers;
		
		for (i = x; i < pool->num_servers; i++) {
			if (i != idx) {
				server = pool->servers[i];
				if (php_tt_server_ok(server->host, server->port TSRMLS_CC)) {
					return i;
				}
			}
		}
		
		for (i = x; i >= 0; i--) {
			if (i != idx) {
				server = pool->servers[i];
				if (php_tt_server_ok(server->host, server->port TSRMLS_CC)) {
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
	if (idx >= pool->num_servers || idx < 0) {
		return NULL;
	}
	
	return pool->servers[idx];
}