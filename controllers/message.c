#include <ctype.h>
#include "base.h"

typedef struct RoomInfo
{
	int id;
	int groupId;
	int joinKey;
	int memberId;
	int memberStatus;
	enum RoomState state;
	char groupName[128];
	char groupAbout[512];
	char groupBanner[128];
	char skippedMessageId[GUID_STORE];
} RoomInfo;

static errno_t room_info_callback(void *context, int argc, char **argv, char **columns)
{
	RoomInfo *room = (RoomInfo *)context;
	for (int i = 0; i < argc; i++)
	{
		KeyValuePair x = {columns[i], argv[i]};
		KVP_TO_INT(x, room->id, "id")
		KVP_TO_INT(x, room->groupId, "groupId")
		KVP_TO_INT(x, room->joinKey, "joinKey")
		KVP_TO_INT(x, room->memberId, "memberId")
		KVP_TO_INT(x, room->memberStatus, "memberStatus")
		KVP_TO_INT(x, room->state, "roomState")
		KVP_TO_STR_V2(x, room->groupName, sizeof(room->groupName), "groupName")
		KVP_TO_STR_V2(x, room->groupAbout, sizeof(room->groupAbout), "groupAbout")
		KVP_TO_STR_V2(x, room->groupBanner, sizeof(room->groupBanner), "groupBanner")
		KVP_TO_STR_V2(x, room->skippedMessageId, sizeof(room->skippedMessageId), "skippedMessageId")
	}
	return 0;
}

static apr_status_t get_room_info(HttpContext *c, RoomInfo *room, char *buffer, int roomId, int groupId, int joinKey)
{
	int userId = c->identity.authenticated ? atoi(c->identity.sub) : 0;

	DbQuery query = {.dbc = &c->dbc};
	query.callback = room_info_callback;
	query.callback_context = room;

	query.sql =
		"select *\n"
		"from ViewRooms as r\n"
		"left join ViewRoomMembers as rm on rm.RoomId = r.Id and rm.MemberId = ?\n"
		"where r.Id = ? or (GroupId = ? and RoomName = '')\n";

	JsonValue argv[3];
	argv[query.argc++] = json_new_int(userId, false);
	argv[query.argc++] = json_new_int(roomId, false);
	argv[query.argc++] = json_new_int(groupId, false);

	room->id = 0; // first clear
	room->memberId = 0;

	if (sql_exec(&query, argv) != 0)
	{
		strcpy(buffer, tl("Internal error: failed to get data"));
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	if (room->id == 0) // if not found
	{
		if (roomId != 0)
			sprintf(buffer, tl("Room %d not found"), roomId);
		else
			sprintf(buffer, tl("Group %d not found"), groupId);
		return HTTP_NOT_FOUND;
	}

	if (room->memberId != 0) // if user is a member of the group
		return OK;

	else if (room->joinKey == joinKey)
		return OK;

	else if (joinKey == 0)
	{
		strcpy(buffer, tl("You are not a member of the group"));
		return HTTP_FORBIDDEN;
	}
	else
	{
		strcpy(buffer, tl("The join key provided is not valid"));
		return HTTP_FORBIDDEN;
	}
}

struct messages_callback
{
	int userId;
	JsonArray *messages;
};

static const char *messages_sql =
	"SELECT Id, ParentId, UserId, UserName, DateSent, Status, Content\n"
	"FROM ViewMessages\n"
	"WHERE RoomId = ? and DateSent > ?\n"
	"ORDER by RoomId, DateSent\n";

static errno_t messages_callback(void *context, int argc, char **argv, char **columns)
{
	CHECK_SQL_CALLBACK(7);
	struct messages_callback *info = (struct messages_callback *)context;
	JsonObject *msg = json_new_object();
	char str[64];

	int userId = atoi(argv[2]);
	if (userId == info->userId)
		json_put_node(msg, "sentByMe", cJSON_CreateBool(true), 0);

	if (!str_empty(argv[3]))
		snprintf(str, sizeof(str), "%s", argv[3]);
	else
		snprintf(str, sizeof(str), "ANO-%s", argv[2]);
	json_put_string(msg, "senderName", str, 0);

	local_to_utc(str, sizeof(str), argv[4]);
	json_put_string(msg, "dateSent", str, 0);

	json_put_string(msg, "id", argv[0], 0);
	json_put_string(msg, "parentId", argv[1], 0);
	json_put_number(msg, "status", atoi(argv[5]), 0);
	json_put_string(msg, "content", argv[6], 0);

	json_array_add(info->messages, msg);
	return 0;
}

static apr_status_t get_messages(HttpContext *c)
{
	int roomId = 0, groupId = 0, joinKey = 0;
	char *lastMessageDateSent = NULL;

	KeyValuePair x;
	while ((x = get_next_url_query_argument(&c->request_args, '&', true)).key != NULL)
	{
		KVP_TO_INT(x, roomId, "r")
		KVP_TO_INT(x, groupId, "g")
		KVP_TO_INT(x, joinKey, "k")
		KVP_TO_INT(x, groupId, "groupId")
		KVP_TO_INT(x, joinKey, "joinKey")
		KVP_TO_STR(x, lastMessageDateSent, "lastMessageDateSent")
	}

	char dateSent[DATE_STORE];
	if (utc_to_local(dateSent, sizeof(dateSent), lastMessageDateSent) != 0)
		return HTTP_BAD_REQUEST; // invalid date format

	char buffer[1024];
	RoomInfo room;

	apr_status_t status = get_room_info(c, &room, buffer, roomId, groupId, joinKey);
	if (status != OK)
		return http_problem(c, NULL, buffer, status);

	struct messages_callback context = {.messages = json_new_array()};

	DbQuery query = {.dbc = &c->dbc};
	query.sql = messages_sql;
	query.callback = messages_callback;
	query.callback_context = &context;

	JsonValue argv[2];
	argv[query.argc++] = json_new_int(room.id, false);
	argv[query.argc++] = json_new_str(dateSent, false);

	if (c->identity.authenticated)
		context.userId = atoi(c->identity.sub);

	if (sql_exec(&query, argv) != 0)
	{
		cJSON_Delete(context.messages);
		return http_problem(c, NULL, tl("An error has occurred while obtaining the messages"), 500);
	}

	JsonObject *info = json_new_object();
	json_put_number(info, "id", room.id, 0);
	json_put_string(info, "name", room.groupName, 0);
	json_put_string(info, "skippedMessageId", room.skippedMessageId, 0);
	if (room.memberId != 0)
		json_put_node(info, "joined", cJSON_CreateBool(true), 0);
	vm_add_node(c, "roomInfo", info, 0);
	vm_add_node(c, "messages", context.messages, 0);

	return process_model(c, HTTP_OK);
}

errno_t add_message(DbContext *dbc, Message m, char id[GUID_STORE])
{
	char _id[GUID_STORE];
	if (id == NULL)
		id = _id;

	if (m.dateSent == 0)
		m.dateSent = time_us();

	char dateSent[DATE_STORE];
	time_us_to_string(dateSent, sizeof(dateSent), m.dateSent, TIME_FORMAT_LOCAL);

	if (str_empty(m.id))
	{
		if ((m.dateSent % 1000) == 0) // add microseconds if not already there
			m.dateSent += rand() % 1000;

		// Generate a unique ID for the message
		sprintf(id, "%012X%016llX0000", m.roomId, m.dateSent);
	}
	else
	{
		APP_LOG(LOG_WARNING, "Message ID was provided: %s", m.id);
		str_copy(id, GUID_STORE, m.id);
	}

	if (m.type == 0)
		m.type = MessageType_Normal;

	if (m.status == 0)
		m.status = MessageStatus_Sent;

	DbQuery query = {.dbc = dbc};
	query.sql =
		"insert into Messages\n"
		"(Id, ParentId, RoomId, SenderId, DateSent, Type, Status, Content) VALUES\n"
		"(UNHEX(?), UNHEX(?), ?, ?, ?, ?, ?, ?)";

	JsonValue argv[10];
	argv[query.argc++] = json_new_str(id, false);
	argv[query.argc++] = json_new_str(m.parentId, true);
	argv[query.argc++] = json_new_int(m.roomId, false);
	argv[query.argc++] = json_new_int(m.senderId, false);
	argv[query.argc++] = json_new_str(dateSent, false);
	argv[query.argc++] = json_new_int(m.type, false);
	argv[query.argc++] = json_new_int(m.status, false);
	argv[query.argc++] = json_new_str(m.content, false);

	errno_t e = sql_exec(&query, argv);
	if (e != 0)
		APP_LOG(LOG_ERROR, "Failed to add the message");
	return e;
}

errno_t update_room_state(DbContext *dbc, int roomId, enum RoomState state)
{
	DbQuery query = {.dbc = dbc};
	query.sql = "update Rooms set State = ? where Id = ?";
	JsonValue argv[2];
	argv[query.argc++] = json_new_int(state, false);
	argv[query.argc++] = json_new_int(roomId, false);
	return sql_exec(&query, argv);
}

static apr_status_t send_message(HttpContext *c)
{
	char buffer[MIN_BUFFER_SIZE];
	JsonObject *msg = NULL;
	apr_status_t status = OK;

	status = ensure_session_exists(c);
	if (status != OK)
		return status;

	if (get_request_body(c) != 0)
	{
		strcpy(buffer, tl("Failed to read the request body"));
		status = HTTP_BAD_REQUEST;
		goto finish;
	}

	msg = cJSON_Parse(c->request_body.data);
	if (!cJSON_IsObject(msg))
	{
		strcpy(buffer, tl("Failed to parse the request body"));
		status = HTTP_BAD_REQUEST;
		goto finish;
	}

	int roomId = (int)json_get_number(msg, "roomId");

	RoomInfo room;
	status = get_room_info(c, &room, buffer, roomId, 0, 0);
	if (status != OK)
		goto finish;

	Message m = {0};
	m.senderId = atoi(c->identity.sid);
	m.roomId = room.id;
	m.parentId = json_get_string(msg, "parentId");
	m.content = json_get_string(msg, "content");

	const char *dateSent = json_get_string(msg, "dateSent");
	if (!str_empty(dateSent))
	{
		m.dateSent = time_us_from_string(dateSent, TIMEZONE_UTC, NULL, 0);
		if (errno != 0)
		{
			strcpy(buffer, tl("Invalid date format"));
			status = HTTP_BAD_REQUEST;
			goto finish;
		}
	}

	if (str_empty(m.content))
	{
		strcpy(buffer, tl("Message content was not provided"));
		status = HTTP_BAD_REQUEST;
		goto finish;
	}

	str_trim((char *)m.content);

	if (m.content[0] == '@' && isspace(m.content[1]))
	{
		strcpy(buffer, tl("A space after '@' at the start of the message is not allowed"));
		status = HTTP_BAD_REQUEST;
		goto finish;
	}

	bool sendToAI =
		str_starts_with(m.content, "@AI ", StringCompare_CaseInsensitive) ||
		str_starts_with(m.content, "@IA ", StringCompare_CaseInsensitive);

	if (sendToAI && room.state == RoomState_AIBusy)
	{
		strcpy(buffer, tl("AI is busy, please wait"));
		status = HTTP_SERVICE_UNAVAILABLE;
		goto finish;
	}

	char id[GUID_STORE];
	if (add_message(&c->dbc, m, id) != 0)
	{
		strcpy(buffer, tl("Failed to add the message"));
		status = HTTP_INTERNAL_SERVER_ERROR;
		goto finish;
	}

	vm_add(c, "id", id, 0);
	if (sendToAI)
	{
		if (update_room_state(&c->dbc, m.roomId, RoomState_AIBusy) != 0)
		{
			strcpy(buffer, tl("An error has occurred while updating the room state"));
			status = 500;
			goto finish;
		}

		str_lit_t tracker = "send_to_ai";
		struct send_to_ai *data = _malloc(sizeof(struct send_to_ai), tracker);
		data->app_backup.malloc_tracker = tracker;

		get_app_backup(&data->app_backup, get_app());
		str_copy(data->messageId, GUID_STORE, id); // id is valid at this point
		data->roomId = m.roomId;

		apr_pool_cleanup_register(c->request->pool, data, (apr_status_t (*)(void *))send_message_to_ai, apr_pool_cleanup_null);

		// Close the connection:
		c->request->connection->keepalive = AP_CONN_CLOSE;
		apr_table_set(c->request->headers_out, "Connection", "close");

		vm_add_node(c, "ai_is_busy", cJSON_CreateBool(true), 0);
	}
	status = HTTP_CREATED;

finish:
	cJSON_Delete(msg);
	msg = NULL; // good programming habit!

	if (status == HTTP_CREATED)
		return process_model(c, status);
	return http_problem(c, NULL, buffer, status);
}

struct m_info
{
	int userId;
	char parentId[48];
};

static errno_t m_info_callback(void *context, int argc, char **argv, char **columns)
{
	CHECK_SQL_CALLBACK(2);
	struct m_info *info = (struct m_info *)context;
	info->userId = atoi(argv[0]);
	str_copy(info->parentId, sizeof(info->parentId), argv[1]);
	return 0;
}

static apr_status_t sent_or_caused_by_me(HttpContext *c, const char *id)
{
	char buffer[MIN_BUFFER_SIZE];
	int currentUserId = atoi(c->identity.sub);
	struct m_info info = {0};

	DbQuery query = {.dbc = &c->dbc};
	query.callback = m_info_callback;
	query.callback_context = &info;
	query.sql = "select UserId, ParentId from ViewMessages where Id = ?";
	JsonValue argv[1];

	while (true)
	{
		argv[0] = json_new_str(id, false);
		query.argc = 1;

		info.userId = 0; // clear first
		if (sql_exec(&query, argv) != 0)
		{
			sprintf(buffer, tl("Failed to get info of message %s"), id);
			return http_problem(c, NULL, buffer, HTTP_INTERNAL_SERVER_ERROR);
		}

		if (info.userId == 0)
		{
			sprintf(buffer, tl("Message %s not found"), id);
			return http_problem(c, NULL, buffer, HTTP_NOT_FOUND);
		}

		if (info.userId == currentUserId)
			return OK; // message was sent or caused by me

		if (info.userId != 1 || str_empty(info.parentId))
		{
			strcpy(buffer, tl("This message was not sent nor caused by you"));
			return http_problem(c, NULL, buffer, HTTP_FORBIDDEN);
		}

		id = info.parentId;
	}
	return OK;
}

static apr_status_t get_and_validate_message_id(HttpContext *c, char id[GUID_STORE])
{
	id[0] = '\0'; // clear first
	KeyValuePair pair;

	while ((pair = get_next_url_query_argument(&c->request_args, '&', true)).key != NULL)
	{
		if (str_equal(pair.key, "id"))
			str_copy(id, GUID_STORE, pair.value);
	}

	if (str_empty(id))
		return http_problem(c, NULL, tl("Message id not provided"), HTTP_BAD_REQUEST);

	if (!is_sql_safe(id, GUID_STORE))
		return http_problem(c, NULL, tl("Invalid message id provided"), HTTP_BAD_REQUEST);

	return sent_or_caused_by_me(c, id);
}

static apr_status_t delete_message(HttpContext *c)
{
	char id[GUID_STORE];

	apr_status_t status = get_and_validate_message_id(c, id);
	if (status != OK)
		return status;

	DbQuery query = {.dbc = &c->dbc};
	query.sql = "Update Messages set DateDeleted = CURRENT_TIMESTAMP(6) where Id = UNHEX(?)";

	JsonValue argv[1];
	argv[query.argc++] = json_new_str(id, false);

	if (sql_exec(&query, argv) != 0)
		return http_problem(c, NULL, tl("Failed to delete the message"), 500);

	return HTTP_NO_CONTENT;
}

static apr_status_t hide_message_from_ai(HttpContext *c)
{
	char id[GUID_STORE];

	apr_status_t status = get_and_validate_message_id(c, id);
	if (status != OK)
		return status;

	DbQuery query = {.dbc = &c->dbc};
	query.sql =
		"UPDATE Rooms AS r\n"
		"JOIN Messages AS m ON m.RoomId = r.Id\n"
		"SET r.SkippedMessageId = m.Id\n"
		"WHERE m.Id = UNHEX(?)\n";

	JsonValue argv[1];
	argv[query.argc++] = json_new_str(id, false);

	if (sql_exec(&query, argv) != 0)
		return http_problem(c, NULL, tl("Failed to hide the message from AI"), 500);

	return HTTP_NO_CONTENT;
}

static apr_status_t join_group(HttpContext *c)
{
	int roomId = 0, groupId = 0, joinKey = 0;

	KeyValuePair x;
	while ((x = get_next_url_query_argument(&c->request_args, '&', true)).key != NULL)
	{
		KVP_TO_INT(x, roomId, "r")
		KVP_TO_INT(x, groupId, "g")
		KVP_TO_INT(x, joinKey, "k")
	}

	char buffer[1024];
	RoomInfo room;

	apr_status_t status = get_room_info(c, &room, buffer, roomId, groupId, joinKey);
	if (status != OK)
		return http_problem(c, NULL, buffer, status);

	if (room.memberId != 0)
		return HTTP_NO_CONTENT;

	DbQuery query = {.dbc = &c->dbc};
	query.sql = "INSERT INTO GroupMembers (GroupId, MemberId) VALUES (?, ?)";

	int userId = str_to_int(c->identity.sub);

	JsonValue argv[2];
	argv[query.argc++] = json_new_int(room.groupId, false);
	argv[query.argc++] = json_new_int(userId, false);

	if (sql_exec(&query, argv) != 0)
	{
		strcpy(buffer, tl("Internal error: failed to get data"));
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	return HTTP_NO_CONTENT;
}

static apr_status_t chat_page(HttpContext *c)
{
	c->constants.layout_file = NO_LAYOUT_FILE;

	int roomId = 0, groupId = 0, joinKey = 0;

	KeyValuePair x;
	while ((x = get_next_url_query_argument(&c->request_args, '&', true)).key != NULL)
	{
		KVP_TO_INT(x, roomId, "r")
		KVP_TO_INT(x, groupId, "g")
		KVP_TO_INT(x, joinKey, "k")
	}

	if (roomId == 0 && groupId == 0)
	{
		set_page_title(c, "DRIIMA");
		return process_view(c);
	}

	char buffer[1024];
	RoomInfo room;

	apr_status_t status = get_room_info(c, &room, buffer, roomId, groupId, joinKey);
	if (status != OK)
		return http_problem(c, NULL, buffer, status);

	set_page_title(c, room.groupName);

	JsonObject *og = json_new_object();
	json_put_string(og, "Type", "website", 0);
	json_put_string(og, "Title", room.groupName, 0);
	json_put_string(og, "Description", room.groupAbout, 0);

	get_base_url(c->request, buffer, sizeof(buffer));
	sprintf(buffer + strlen(buffer), "%s?g=%d", c->request->uri, room.groupId);
	json_put_string(og, "URL", buffer, 0);

	if (file_path_to_full_url(c, buffer, sizeof(buffer), room.groupBanner))
		json_put_string(og, "Image", buffer, 0);

	JsonObject *page = json_get_node(c->view_model, "Page");
	json_put_node(page, "OpenGraph", og, 0);

	return process_view(c);
}

static apr_status_t home_page(HttpContext *c)
{
	return http_redirect(c, "/chat", HTTP_MOVED_TEMPORARILY, true);
}

void register_message_controller()
{
	CHECK_ERRNO;
	add_endpoint(M_GET, "/anonymous/chat", home_page, 0); // obsolete
	add_endpoint(M_GET, "/", home_page, 0);
	add_endpoint(M_GET, "/chat", chat_page, 0);

	add_endpoint(M_GET, "/api/room/messages", get_messages, Endpoint_AuthWebAPI);
	add_endpoint(M_GET, "/api/message/many", get_messages, Endpoint_AuthWebAPI); // obsolete
	add_endpoint(M_POST, "/api/room/join", join_group, Endpoint_AuthWebAPI);

	add_endpoint(M_POST, "/api/message/send", send_message, Endpoint_AuthWebAPI);
	add_endpoint(M_DELETE, "/api/message/delete", delete_message, Endpoint_AuthWebAPI);
	add_endpoint(M_PATCH, "/api/message/hide-from-ai", hide_message_from_ai, Endpoint_AuthWebAPI);
}
