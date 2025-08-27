#include "startup.h"
#include <http_core.h>

static void register_controllers()
{
	register_account_controller();
	register_message_controller();
	register_room_controller();
	register_file_upload_controller();
}

static apr_status_t startup_init(HttpContext *c)
{
	static bool g_startup_done = false;
	if (!g_startup_done)
	{
		g_startup_done = true;

		register_controllers();

		database_apply_migrations(&c->dbc, NULL);
	}
	return errno ? 500 : OK;
}

/* The handler function for our module. */
apr_status_t http_request_handler(request_rec *r)
{
	HttpContext c[1]; // no need for = {0}
	http_context_init(c, r, NULL, DBMS_MySQL);

	apr_status_t status = startup_init(c);

	if (status == OK)
		status = get_endpoint(c);

	if (status == OK)
		status = authenticate_access(c);

	if (status == OK)
		status = authorize_endpoint(c);

	if (status == OK)
		status = execute_endpoint(c);

	if (0 < status && status < 200) // should never happen
		APP_LOG(LOG_ERROR, "Invalid status code: %d", status);

	http_context_cleanup(c);
	return status;
}
