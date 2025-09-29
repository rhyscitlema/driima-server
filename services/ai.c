#include <http_fetch.h>
#include "../includes/message.h"

static JsonObject *get_message(const char *role, const char *content)
{
	JsonObject *message = json_new_object();
	json_put_string(message, "role", role, 0);
	json_put_string(message, "content", content, 0);
	return message;
}

static JsonObject *call_tool(JsonObject *tool)
{
	CLEAR_ERRNO;
	const char *call_id = json_get_string(tool, "call_id");
	const char *call_name = json_get_string(tool, "name");
	const char *call_args = json_get_string(tool, "arguments");

	APP_LOG(LOG_INFO, "%s: %s", call_name, call_args);
	char *result = NULL;

	APP_LOG(LOG_CRITICAL, "Unknown tool call name: %s", call_name);

	JsonObject *r = json_new_object();
	json_put_string(r, "type", "function_call_output", 0);
	json_put_string(r, "call_id", call_id, 0);
	json_put_string(r, "output", result, 0);
	cJSON_free(result);
	return r;
}

static bool process_ai_response(const char *response, JsonArray *messages, DbContext *dbc, Message m)
{
	JsonObject *response_json = cJSON_Parse(response);
	JsonArray *output = json_get_node(response_json, "output");

	if (output == NULL)
	{
		cJSON_Delete(response_json);
		APP_LOG(LOG_ERROR, "No output in AI response: %s", response);
		m.content = tl("Internal Error: No output in AI response.");
		add_message(dbc, m, NULL);
		return false;
	}

	JsonObject *usage = json_get_node(response_json, "usage");
	if (usage != NULL)
	{
		char *usage_str = cJSON_Print(usage);
		APP_LOG(LOG_INFO, "AI tokens usage: %s", usage_str);
		cJSON_free(usage_str);
	}

	assert(m.id == NULL);
	bool send_to_ai_again = false;

	for (JsonObject *message = output->child; message != NULL; message = message->next)
	{
		CLEAR_ERRNO;
		const char *type = json_get_string(message, "type");

		JsonObject *duplicate = cJSON_Duplicate(message, true);
		json_array_add(messages, duplicate);

		JsonArray *choices = json_get_node(message, "content");
		JsonObject *choice = choices == NULL ? NULL : choices->child;
		const char *text = json_get_string(choice, "text");
		if (text != NULL)
		{
			m.content = text;
			m.type = MessageType_Normal;
			add_message(dbc, m, NULL);
		}

		if (str_equal(type, "function_call"))
		{
			send_to_ai_again = true;

			JsonObject *r = call_tool(message);
			json_array_add(messages, r);

			// store the tool call in the database
			duplicate = cJSON_Duplicate(message, true);

			const char *output = json_get_string(r, "output");
			json_put_string(duplicate, "output", output, 0);

			char *content = cJSON_Print(duplicate);
			m.content = content;
			m.type = MessageType_ToolCall;
			add_message(dbc, m, NULL);

			cJSON_free(content);
			cJSON_Delete(duplicate);
		}
	}

	cJSON_Delete(response_json);
	return send_to_ai_again;
}

/* store the HTTP request in the database */
static void store_http_request(HttpFetch *fetch, const char *request_content, const HttpResponse *response, DbContext *dbc, const char *messageId)
{
	DbQuery query = {.dbc = dbc};

	query.sql =
		"INSERT INTO HttpRequests (\n"
		"\tMessageId,\n"
		"\tURL,\n"
		"\tDuration,\n"
		"\tStatusCode,\n"
		"\tRequestContent,\n"
		"\tResponseHeaders,\n"
		"\tResponseContent)\n"
		"VALUES (UNHEX(?), ?, ?, ?, ?, ?, ?);\n";

	JsonNode argv[8];
	argv[query.argc++] = json_new_str(messageId, true);
	argv[query.argc++] = json_new_str(fetch->url, false);
	argv[query.argc++] = json_new_int(response->duration, false);
	argv[query.argc++] = json_new_int(response->status_code, false);
	argv[query.argc++] = json_new_str(request_content, true);
	argv[query.argc++] = json_new_str(response->headers.data, true);
	argv[query.argc++] = json_new_str(response->content.data, true);

	sql_exec(&query, argv);
}

static errno_t skippedDateSent_callback(void *context, int argc, char **argv, char **columns)
{
	CHECK_SQL_CALLBACK(1);
	str_copy((char *)context, DATE_STORE, argv[0]);
	return 0;
}

static errno_t messages_callback(void *context, int argc, char **argv, char **columns)
{
	CHECK_SQL_CALLBACK(6);

	int userId = str_to_int(argv[2]);
	const char *role = userId == 1 ? "assistant" : "user";

	JsonObject *msg = get_message(role, argv[5]);
	// json_put_string(msg, "name", argv[2], 0);

	json_array_add((JsonArray *)context, msg);
	return 0;
}

static void chat_with_ai(DbContext *dbc, int roomId, const char *messageId)
{
	CHECK_ERRNO;

	Message m = {.senderId = 1}; // corresponds to AI
	m.parentId = messageId;
	m.roomId = roomId;

	char _buffer[2048];
	Charray buffer = buffer_to_char_array(_buffer, sizeof(_buffer));

	HttpResponse response = {.content = new_char_array("ai_response")};
	JsonObject *payload = NULL; // declare before the first goto

	HttpFetch fetch = {
		.method = "POST",
		.content_type = "application/json",
		.response_timeout = 10 * 60};

	fetch.url = get_setting("AI_API_URL");
	const char *api_key = get_setting("AI_API_KEY");

	if (str_empty(fetch.url) || str_empty(api_key))
	{
		m.content = "AI_API_URL or AI_API_KEY not found";
		goto finish;
	}

	if (http_fetch_init(&fetch) != 0)
	{
		m.content = "http_fetch_init() failed";
		goto finish;
	}

	bprintf(&buffer, "Authorization: Bearer %s", api_key);
	add_request_header_v2(&fetch, _buffer);

	struct App *app = get_app();
	const char *cwd = str_empty(app->cwd) ? "." : app->cwd;

	char filename[256];
	sprintf(filename, "%s/ai/prompt.json", cwd);

	if (buffer.ext->read_file(&buffer, filename) != 0)
	{
		m.content = "Failed to read prompt.json file";
		goto finish;
	}

	payload = cJSON_Parse(buffer.data);
	sprintf(filename, "%s/ai/developer_prompt.txt", cwd);

	if (buffer.ext->read_file(&buffer, filename) != 0)
	{
		m.content = "Failed to read developer_prompt.txt file";
		goto finish;
	}

	JsonArray *messages = json_get_node(payload, "input");
	json_array_add(messages, get_message("developer", buffer.data));

	char skippedDateSent[DATE_STORE];
	str_copy(skippedDateSent, DATE_STORE, "1970-01-01 00:00:00");

	DbQuery query = {.dbc = dbc};
	query.callback = skippedDateSent_callback;
	query.callback_context = skippedDateSent;
	query.sql =
		"select m.DateSent from Rooms as r\n"
		"join Messages as m on r.SkippedMessageId = m.Id\n"
		"where m.RoomId = ?\n";

	JsonValue argv[4];
	argv[query.argc++] = json_new_int(roomId, false);

	if (sql_exec(&query, argv) != 0)
	{
		m.content = tl("Internal error: failed to get data");
		goto finish;
	}

	query.callback = messages_callback;
	query.callback_context = messages;
	query.sql =
		"select Id, ParentId, UserId, DateSent, Status, Content\n"
		"from ViewMessages where Content is not null and RoomId = ?\n"
		"and DateSent > ?\n"
		"order by RoomId, DateSent\n";

	// roomId already added before, so just add skippedDateSent
	argv[query.argc++] = json_new_str(skippedDateSent, false);

	if (sql_exec(&query, argv) != 0)
	{
		m.content = tl("Internal error: failed to get data");
		goto finish;
	}

	while (true)
	{
		char *request_content = cJSON_Print(payload);
		if (request_content == NULL)
		{
			m.content = tl("Failed to JSON serialize");
			break;
		}

		send_http_request(&fetch, NS(request_content), &response, &buffer);

		if (response.status_code != 200)
			APP_LOG(LOG_DEBUG, "request_content: %s", request_content);

		store_http_request(&fetch, request_content, &response, dbc, m.parentId);

		cJSON_free(request_content);

		if (response.status_code != 200)
		{
			m.content = _buffer;
			break;
		}

		if (!process_ai_response(response.content.data, messages, dbc, m))
			break;
	}

finish:
	if (m.content != NULL)
		add_message(dbc, m, NULL);
	cJSON_Delete(payload);
	http_response_cleanup(&response);
	http_fetch_cleanup(&fetch);
	errno = 0;
}

errno_t send_message_to_ai(struct send_to_ai *data)
{
	struct App app = {0};
	if (set_app(&app, SetApp_Init) != 0) // must come first
		return errno;

	use_app_backup(&data->app_backup, &app); // must come second

	DbContext dbc = db_context_init(DBMS_MySQL, NULL);

	APP_LOG(LOG_INFO, "AI replying to message %s", data->messageId);

	chat_with_ai(&dbc, data->roomId, data->messageId);

	update_room_state(&dbc, data->roomId, RoomState_Normal);

	_free(data, data->app_backup.malloc_tracker); // must come second to last

	set_app(NULL, SetApp_Clear); // must come last
	return 0;
}

errno_t text_to_speech(struct tts_output *out, struct tts_input info, Charray *buffer)
{
	assert(out != NULL);
	assert(info.input != NULL);
	assert(buffer != NULL);

	CHECK_ERRNO errno;
	JsonObject *payload = NULL; // declare before the first goto

	HttpResponse response = {
		.headers = new_char_array("tts_headers"),
		.content = new_char_array("tts_content"),
		.get_response_headers = true};

	HttpFetch fetch = {
		.url = "https://api.openai.com/v1/audio/speech",
		.method = "POST",
		.content_type = "application/json",
		.response_timeout = 10 * 60};

	const char *api_key = get_setting("AI_API_KEY");
	if (str_empty(api_key))
	{
		bprintf(buffer, "AI_API_KEY not found");
		return EAGAIN;
	}

	errno_t e = http_fetch_init(&fetch);
	if (e != 0)
	{
		bprintf(buffer, "http_fetch_init() failed");
		return e;
	}

	bprintf(buffer, "Authorization: Bearer %s", api_key);
	add_request_header_v2(&fetch, buffer->data);

	if (str_empty(info.model))
		info.model = "tts-1";

	if (str_empty(info.voice))
		info.voice = "alloy";

	payload = json_new_object();
	json_put_string(payload, "model", info.model, 0);
	json_put_string(payload, "voice", info.voice, 0);
	json_put_string(payload, "input", info.input, 0);

	char *request_content = cJSON_Print(payload);
	if (request_content == NULL)
	{
		bprintf(buffer, tl("Failed to serialize json payload"));
		e = errno ? errno : EINVAL;
		goto finish;
	}

	send_http_request(&fetch, NS(request_content), &response, buffer);

	if (response.status_code == 200)
	{
		// move response.content to out->content
		charray_free(&out->content); // avoid memory leaks
		out->content = response.content; // transfer the char array
		response.content = new_char_array(NULL); // nullify the char array
	}
	else APP_LOG(LOG_DEBUG, "request_content: %s", request_content);

	store_http_request(&fetch, request_content, &response, info.dbc, info.messageId);

	cJSON_free(request_content);

	if (response.status_code != 200)
	{
		e = errno ? errno : EAGAIN;
		goto finish;
	}

	APP_LOG(LOG_INFO, "Got a voice file of size %zu.", out->content.length);

finish:
	cJSON_Delete(payload);
	http_response_cleanup(&response);
	http_fetch_cleanup(&fetch);
	errno = 0;
	return e;
}

