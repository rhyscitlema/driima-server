/*
	Do not modify this file.
*/
#include <http_core.h>

#ifndef SITE_NAME
#error "SITE_NAME is not defined."
#endif

/* Use MACRO magic to get: {SITE_NAME}_module */
#define X(x) x##_module
#define Y(y) X(y)
#define SITE_MODULE Y(SITE_NAME)

/* Use MACRO magic to get: "{SITE_NAME}_module" */
#define Z(z) #z
#define STRINGIFY(s) Z(s)
static const char *handler = STRINGIFY(SITE_MODULE);

extern apr_status_t http_request_handler(request_rec *r);

static apr_status_t request_handler(request_rec *r)
{
	/* First off, we need to check if this is a call for us to handle,
	 * else we simply return DECLINED, and Apache will try somewhere else.
	 */
	if (r->handler == NULL || strcmp(r->handler, handler) != 0)
		return DECLINED;

	return http_request_handler(r);
}

static void register_hooks(apr_pool_t *pool)
{
	ap_hook_handler(request_handler, NULL, NULL, APR_HOOK_LAST);
}

/* Define our module as an entity and assign a function for registering hooks  */
module AP_MODULE_DECLARE_DATA SITE_MODULE = {
	STANDARD20_MODULE_STUFF,
	NULL, // Per-directory configuration handler
	NULL, // Merge handler for per-directory configurations
	NULL, // Per-server configuration handler
	NULL, // Merge handler for per-server configurations
	NULL, // Any directives we may have for httpd
	register_hooks,
};
