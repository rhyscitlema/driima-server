#include <stdio.h>
#include <curl/curl.h>
#include "../models/message.h"

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
		m.content = "Internal Error: No output in AI response.";
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

typedef struct AIResponse
{
	int duration;
	int status_code;
	Charray content;
} AIResponse;

// Callback function to handle the response content
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	CLEAR_ERRNO;
	size_t total_size = size * nmemb;
	Charray *response = (Charray *)userdata;
	if (response->ext->add(response, (char *)ptr, total_size) != 0)
		return 0;
	return total_size;
}

static void send_request_to_ai(CURL *curl, const char *request_content, AIResponse *response)
{
	// reset the response
	response->status_code = 0;
	clear_char_array(&response->content);

	time_us_t start = time_us();
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_content);

	// Perform the request
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		APP_LOG(LOG_DEBUG, "Payload: %s", request_content);
		APP_LOG(LOG_ERROR, "curl_easy_perform() failed: %s.", curl_easy_strerror(res));
		goto finish;
	}

	long response_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
	response->status_code = (int)response_code;

	if (response->status_code != 200)
	{
		APP_LOG(LOG_DEBUG, "Payload: %s", request_content);
		APP_LOG(LOG_ERROR, "Response:%d: %s", response->status_code, response->content.data);
	}
	else if (str_empty(response->content.data))
	{
		APP_LOG(LOG_DEBUG, "Payload: %s", request_content);
		APP_LOG(LOG_ERROR, "Failed to read the response content from AI.");
	}

finish:
	response->duration = (int)(time_us() - start) / 1000000;
}

/* store the HTTP request in the database */
static void store_http_request(const char *api_url, const char *request_content, const AIResponse *response, DbContext *dbc, const char *messageId)
{
	DbQuery query = {.dbc = dbc};
	query.sql =
		"INSERT INTO HttpRequests\n"
		"(MessageId, URL, Duration, StatusCode, RequestContent, ResponseContent)\n"
		"VALUES (UNHEX(?), ?, ?, ?, ?, ?);\n";

	JsonNode argv[8];
	argv[query.argc++] = json_new_str(messageId, true);
	argv[query.argc++] = json_new_str(api_url, false);
	argv[query.argc++] = json_new_int(response->duration, false);
	argv[query.argc++] = json_new_int(response->status_code, false);
	argv[query.argc++] = json_new_str(request_content, true);
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

	const char *api_url = get_setting("AI_API_URL");
	if (str_empty(api_url))
	{
		APP_LOG(LOG_ERROR, "AI_API_URL not found");
		return;
	}

	const char *api_key = get_setting("AI_API_KEY");
	if (str_empty(api_key))
	{
		APP_LOG(LOG_ERROR, "AI_API_KEY not found");
		return;
	}

	CURL *curl = curl_easy_init();
	if (curl == NULL)
	{
		APP_LOG(LOG_ERROR, "Failed to initialize curl");
		return;
	}

	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10 * 60L);

	char _buffer[2048];
	Charray buffer = buffer_to_char_array(_buffer, sizeof(_buffer));
	AIResponse response = {.content = new_char_array("ai_response")};

	curl_easy_setopt(curl, CURLOPT_URL, api_url);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	// Set the callback function to handle the HTTP response
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.content);

	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	sprintf(_buffer, "Authorization: Bearer %s", api_key);
	headers = curl_slist_append(headers, _buffer);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	JsonObject *payload = NULL; // declare before the first goto

	struct App *app = get_app();
	const char *cwd = str_empty(app->cwd) ? "." : app->cwd;

	char filename[256];
	sprintf(filename, "%s/ai/prompt.json", cwd);

	if (buffer.ext->read_file(&buffer, filename) != 0)
		goto finish;

	payload = cJSON_Parse(buffer.data);
	sprintf(filename, "%s/ai/developer_prompt.txt", cwd);

	if (buffer.ext->read_file(&buffer, filename) != 0)
		goto finish;

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
		goto finish;

	query.callback = messages_callback;
	query.callback_context = messages;
	query.sql =
		"select Id, ParentId, UserId, DateSent, Status, Content\n"
		"from ViewMessages where Content is not null and RoomId = ?\n"
		"and DateSent > ?\n"
		"order by RoomId, DateSent\n";

	argv[query.argc++] = json_new_str(skippedDateSent, false);

	if (sql_exec(&query, argv) != 0)
		goto finish;

	while (true)
	{
		char *request_content = cJSON_Print(payload);
		if (request_content == NULL)
		{
			APP_LOG(LOG_ERROR, "Failed to serialize json payload");
			continue;
		}
		send_request_to_ai(curl, request_content, &response);

		store_http_request(api_url, request_content, &response, dbc, m.parentId);

		cJSON_free(request_content);

		if (response.status_code != 200)
		{
			sprintf(_buffer, "The request to AI failed with status %d.", response.status_code);
			m.content = _buffer;
			add_message(dbc, m, NULL);
			break;
		}

		if (str_empty(response.content.data))
		{
			m.content = "Failed to read the response content from AI.";
			add_message(dbc, m, NULL);
			break;
		}

		if (!process_ai_response(response.content.data, messages, dbc, m))
			break;
	}

finish:
	response.content.ext->free(&response.content);
	cJSON_Delete(payload);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	CLEAR_ERRNO;
}

errno_t send_message_to_ai(struct send_to_ai *data)
{
	struct App app = {0};
	if (set_app(&app, SetApp_Init) != 0) // must come first
		return errno;

	// this was allocated before send_to_ai() was called
	app.memory.track("send_to_ai", MEM_OPR_ALLOC);

	str_copy(app.logger.tag, GUID_STORE, data->logger_tag);
	app.cwd = data->cwd;

	DbContext dbc = db_context_init(DBMS_MySQL, NULL);

	APP_LOG(LOG_INFO, "AI replying to message %s", data->messageId);

	chat_with_ai(&dbc, data->roomId, data->messageId);

	update_room_state(&dbc, data->roomId, RoomState_Normal);

	_free(data, "send_to_ai");
	set_app(NULL, SetApp_Clear);
	return 0;
}
