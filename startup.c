#include "controllers/base.h"

/* Called by only one server process at a time to avoid a race condition. */
static apr_status_t prepare_database(HttpContext *c)
{
	database_apply_migrations(&c->dbc, NULL);
	return errno_to_status_code(errno);
}

/* Called only by the first HTTP request received by the server process. */
static apr_status_t prepare_process(HttpContext *c)
{
	(void)c; // unused for now
	register_account_controller();
	register_message_controller();
	register_room_controller();
	register_file_upload_controller();
	return errno_to_status_code(errno);
}

/* The handler function for our module. See module.c */
apr_status_t http_request_handler(request_rec *r)
{
	HttpContext c[1]; // no need to clear
	http_context_init(c, r, NULL);

	// below must come right after above
	c->dbc = db_context_init(DBMS_MySQL, NULL);

	apr_status_t status = startup_init(c, prepare_database, prepare_process);

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
