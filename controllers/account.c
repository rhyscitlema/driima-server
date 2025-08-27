#include "base.h"

static errno_t user_query_callback(void *context, int argc, char **argv, char **columns)
{
	CHECK_SQL_CALLBACK(1);
	*(row_id_t *)context = str_to_long(argv[0]);
	return 0;
}

apr_status_t ensure_session_exists(HttpContext *c)
{
	if (!c->identity.authenticated)
		return OK; // nothing to validate for unauthenticated users

	row_id_t sessionId = str_to_long(c->identity.sid);
	row_id_t userId = 0;

	// Check if the session exists in the database
	DbQuery query = {.dbc = &c->dbc};
	query.callback = user_query_callback;
	query.callback_context = &userId;
	query.sql = "SELECT UserId FROM Sessions WHERE Id = ?";

	JsonValue argv[4];
	argv[query.argc++] = json_new_long(sessionId, false);

	if (sql_exec(&query, argv) != 0)
		return http_problem(c, NULL, tl("Failed to check session existence"), HTTP_INTERNAL_SERVER_ERROR);

	if (userId != 0)
		return OK; // session exists, all good

	// Session not found, need to recreate it
	userId = str_to_long(c->identity.sub);

	char ip_addr[40];
	get_ip_addr(c->request, ip_addr, sizeof(ip_addr));

	query.callback = NULL;
	query.sql = "INSERT INTO `Users` (Id, Type) VALUES (?, ?);";
	query.argc = 0;
	argv[query.argc++] = json_new_long(userId, false);
	argv[query.argc++] = json_new_int(UserType_Anonymous, false);

	if (sql_exec(&query, argv) != 0)
		return http_problem(c, NULL, tl("Failed to create user entry"), HTTP_INTERNAL_SERVER_ERROR);

	query.sql = "INSERb INTO `Sessions` (Id, UserId, IPAddress) VALUES (?, ?, ?);";
	query.argc = 0;
	argv[query.argc++] = json_new_long(sessionId, false);
	argv[query.argc++] = json_new_long(userId, false);
	argv[query.argc++] = json_new_str(ip_addr, true);

	if (sql_exec(&query, argv) != 0)
		return http_problem(c, NULL, tl("Failed to recreate user session"), HTTP_INTERNAL_SERVER_ERROR);

	APP_LOG(LOG_INFO, "Recreated session %lld for user %lld", sessionId, userId);
	return OK;
}

static apr_status_t anonymous_login(HttpContext *c, const char *password, AccessIdentity *auth)
{
	if (!is_sql_safe(password, 32))
		return http_problem(c, NULL, tl("Invalid GUID format"), HTTP_BAD_REQUEST);

	row_id_t userId = 0, sessionId = 0;
	DbQuery query = {.dbc = &c->dbc};

	query.sql = "SELECT Id FROM Users WHERE AccountId = UNHEX(?)";
	query.callback = user_query_callback;
	query.callback_context = &userId;

	JsonValue argv[3];
	argv[query.argc++] = json_new_str(password, false);

	if (sql_exec(&query, argv) != 0)
		return http_problem(c, NULL, tl("Failed to query user"), HTTP_INTERNAL_SERVER_ERROR);

	query.callback = NULL;

	char ip_addr[40];
	get_ip_addr(c->request, ip_addr, sizeof(ip_addr));

	// if user not found, create one
	if (userId == 0)
	{
		query.insert_id = &userId;
		query.sql = "INSERT INTO `Users` (AccountId, Type) VALUES (UNHEX(?), ?)";

		argv[query.argc++] = json_new_int(UserType_Anonymous, false);

		if (sql_exec(&query, argv) != 0)
			return http_problem(c, NULL, tl("Failed to create new user"), HTTP_INTERNAL_SERVER_ERROR);
	}

	// now create the session
	query.insert_id = &sessionId;
	query.sql = "INSERT INTO `Sessions` (UserId, IPAddress) VALUES (?, ?);";

	query.argc = 0;
	argv[query.argc++] = json_new_long(userId, false);
	argv[query.argc++] = json_new_str(ip_addr, true);

	if (sql_exec(&query, argv) != 0)
		return http_problem(c, NULL, tl("Failed to create new session"), HTTP_INTERNAL_SERVER_ERROR);

	snprintf(auth->sub, sizeof(auth->sub), "%lld", userId);
	snprintf(auth->sid, sizeof(auth->sid), "%lld", sessionId);

	APP_LOG(LOG_INFO, "Created session %lld for user %lld", sessionId, userId);
	return OK;
}

static apr_status_t login(HttpContext *c)
{
	if (get_request_body(c) != 0)
		return http_problem(c, NULL, tl("Failed to read the request body"), HTTP_BAD_REQUEST);

	char *username = NULL, *password = NULL;

	KeyValuePair x;
	while ((x = get_next_url_query_argument(&c->request_body, '&', true)).key != NULL)
	{
		KVP_TO_STR(x, username, "username")
		KVP_TO_STR(x, password, "password")
	}

	if (str_empty(username) || str_empty(password))
		return http_problem(c, NULL, tl("Username or password not provided"), HTTP_BAD_REQUEST);

	AccessIdentity auth = {0};

	if (!str_equal(username, "ANO"))
		return http_problem(c, NULL, tl("Please use an ANO user"), HTTP_NOT_IMPLEMENTED);

	apr_status_t status = anonymous_login(c, password, &auth);
	if (status != OK)
		return status;

	int max_age = 10 * 365 * 24 * 60 * 60;

	if (set_authentication_cookie(c, &auth, max_age) != 0)
		return http_problem(c, NULL, tl("Failed to set authentication cookie"), HTTP_INTERNAL_SERVER_ERROR);

	return HTTP_NO_CONTENT;
}

static apr_status_t logout(HttpContext *c)
{
	clear_authentication_cookie(c);
	return HTTP_NO_CONTENT;
}

void register_account_controller()
{
	add_endpoint(M_POST, "/api/account/login", login, Endpoint_IsaWebAPI);
	add_endpoint(M_POST, "/api/account/logout", logout, Endpoint_AuthWebAPI);
}

