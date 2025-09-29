#include <ctype.h>
#include "base.h"
#include "../includes/message.h"

typedef struct UrlArgs
{
	int userId;
	int roomId;
	int groupId;
	int joinKey;
	char *lastMessageDateSent;
} UrlArgs;

static UrlArgs get_url_args(HttpContext *c)
{
	UrlArgs args = {0};

	if (c->identity.authenticated)
		args.userId = atoi(c->identity.sub);

	KeyValuePair x;
	while ((x = get_next_url_query_argument(&c->request_args, '&', true)).key != NULL)
	{
		KVP_TO_INT(x, args.roomId, "r")
		KVP_TO_INT(x, args.groupId, "g")
		KVP_TO_INT(x, args.joinKey, "k")
		KVP_TO_INT(x, args.groupId, "groupId")
		KVP_TO_INT(x, args.joinKey, "joinKey")
		KVP_TO_STR(x, args.lastMessageDateSent, "lastMessageDateSent")
	}
	return args;
}

typedef struct RoomInfo
{
	int id;
	int groupId;
	int joinKey;
	int memberId;
	int memberStatus;
	enum RoomState state;
	char roomName[64];
	char groupName[128];
	char *groupAbout;
	char groupBanner[FILE_PATH_STORE];
	char skippedMessageId[GUID_STORE];

	bool get_extra_info;
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

		KVP_TO_STR_COPY(x, room->roomName, sizeof(room->roomName), "roomName")
		KVP_TO_STR_COPY(x, room->groupName, sizeof(room->groupName), "groupName")
		KVP_TO_STR_COPY(x, room->groupBanner, sizeof(room->groupBanner), "groupBanner")
		KVP_TO_STR_COPY(x, room->skippedMessageId, sizeof(room->skippedMessageId), "skippedMessageId")

		if (room->get_extra_info)
			KVP_TO_STR_DUPL(x, room->groupAbout, "room_groupAbout", "groupAbout")
	}
	return 0;
}

static apr_status_t get_room_info(HttpContext *c, RoomInfo *room, UrlArgs args, char *buffer, bool get_extra_info)
{
	DbQuery query = {.dbc = &c->dbc};
	query.callback = room_info_callback;
	query.callback_context = room;

	query.sql =
		"select *\n"
		"from ViewRooms as r\n"
		"left join ViewRoomMembers as rm on rm.RoomId = r.Id and rm.MemberId = ?\n"
		"where r.Id = ? or (GroupId = ? and RoomName = '')\n";

	JsonValue argv[3];
	argv[query.argc++] = json_new_int(args.userId, false);
	argv[query.argc++] = json_new_int(args.roomId, false);
	argv[query.argc++] = json_new_int(args.groupId, false);

	memset(room, 0, sizeof(*room)); // first clear
	room->get_extra_info = get_extra_info;

	if (sql_exec(&query, argv) != 0)
	{
		strcpy(buffer, tl("Internal error: failed to get data"));
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	if (room->id == 0) // if not found
	{
		if (args.roomId != 0)
			sprintf(buffer, tl("Room %d not found"), args.roomId);
		else
			sprintf(buffer, tl("Group %d not found"), args.groupId);
		return HTTP_NOT_FOUND;
	}

	if (room->memberId != 0) // if user is a member of the group
		return OK;

	else if (room->joinKey == args.joinKey)
		return OK;

	else if (args.joinKey == 0)
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
	int signedInUserId;
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
	if (userId == info->signedInUserId)
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
	UrlArgs args = get_url_args(c);

	char dateSent[DATE_STORE];
	if (utc_to_local(dateSent, sizeof(dateSent), args.lastMessageDateSent) != 0)
		return HTTP_BAD_REQUEST; // invalid date format

	char buffer[1024];
	RoomInfo room;

	apr_status_t status = get_room_info(c, &room, args, buffer, false);
	if (status != OK)
		return http_problem(c, NULL, buffer, status);

	struct messages_callback context = {
		.signedInUserId = args.userId,
		.messages = json_new_array()
	};

	DbQuery query = {.dbc = &c->dbc};
	query.sql = messages_sql;
	query.callback = messages_callback;
	query.callback_context = &context;

	JsonValue argv[2];
	argv[query.argc++] = json_new_int(room.id, false);
	argv[query.argc++] = json_new_str(dateSent, false);

	if (sql_exec(&query, argv) != 0)
	{
		cJSON_Delete(context.messages);
		return http_problem(c, NULL, tl("An error has occurred while obtaining the messages"), 500);
	}

	JsonObject *info = json_new_object();
	json_put_number(info, "id", room.id, 0);
	json_put_string(info, "skippedMessageId", room.skippedMessageId, 0);

	if (str_empty(room.roomName))
		strcpy(buffer, room.groupName);
	else sprintf(buffer, "%s: %s", room.groupName, room.roomName);
	json_put_string(info, "name", buffer, 0);

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
	{
		APP_LOG(LOG_ERROR, "Failed to add the message");
	}
	else
	{
		query.sql = "UPDATE Rooms SET LatestMessageId = UNHEX(?) WHERE Id = ?";
		query.argc = 0;
		argv[query.argc++] = json_new_str(id, false);
		argv[query.argc++] = json_new_int(m.roomId, false);
		sql_exec(&query, argv);
	}
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

	UrlArgs args;
	args.userId = atoi(c->identity.sub);
	args.roomId = (int)json_get_number(msg, "roomId");

	RoomInfo room;
	status = get_room_info(c, &room, args, buffer, false);
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

	char id[GUID_STORE] = {0};
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

static apr_status_t validate_message_id(HttpContext *c, const char *id)
{
	if (str_empty(id))
		return http_problem(c, NULL, tl("Message id not provided"), HTTP_BAD_REQUEST);

	else if (!is_sql_safe(id, GUID_STORE))
		return http_problem(c, NULL, tl("Invalid message id provided"), HTTP_BAD_REQUEST);

	else return OK;
}

static apr_status_t get_and_validate_message_id(HttpContext *c, char id[GUID_STORE], bool check_if_from_me)
{
	id[0] = '\0'; // clear first
	KeyValuePair pair;

	while ((pair = get_next_url_query_argument(&c->request_args, '&', true)).key != NULL)
	{
		if (str_equal(pair.key, "id"))
			str_copy(id, GUID_STORE, pair.value);
	}

	apr_status_t status = validate_message_id(c, id);

	if (status == OK && check_if_from_me)
		status = sent_or_caused_by_me(c, id);

	return status;
}

static apr_status_t delete_message(HttpContext *c)
{
	char id[GUID_STORE];

	apr_status_t status = get_and_validate_message_id(c, id, true);
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

	apr_status_t status = get_and_validate_message_id(c, id, true);
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

struct voice_info
{
	int roomId;
	char voice_file[FILE_PATH_STORE];
	char *message_content;
};

static errno_t get_voice_info(void *context, int argc, char **argv, char **columns)
{
	CLEAR_ERRNO;
	(void)argc;
	(void)columns;
	struct voice_info *info = (struct voice_info *)context;
	info->roomId = str_to_int(argv[0]);
	str_copy(info->voice_file, sizeof(info->voice_file), argv[1]);
	info->message_content = str_duplicate(NULL, argv[2], "info_message_content");
	return 0;
}

static apr_status_t read_aloud(HttpContext *c)
{
	char id[GUID_STORE];

	apr_status_t status = get_and_validate_message_id(c, id, false);
	if (status != OK)
		return status;

	char buffer[1024];
	struct voice_info info = {0};
	DbQuery query = {.dbc = &c->dbc};

	query.callback = get_voice_info;
	query.callback_context = &info;

	query.sql = "SELECT m.RoomId, voice.Path,\n"
		"IF(voice.Id IS NULL, m.Content, NULL) AS Content\n"
		"FROM Messages as m\n"
		"LEFT JOIN FilePaths as voice on voice.Id = m.FileId\n"
		"WHERE m.Id = UNHEX(?) AND m.DateDeleted IS NULL\n";

	JsonValue argv[2];
	argv[query.argc++] = json_new_str(id, false);

	if (sql_exec(&query, argv) != 0)
	{
		strcpy(buffer, tl("Internal error: failed to get data"));
		status = 500;
		goto finish;
	}

	// TODO: check if user is a member of the room

	if (str_empty(info.voice_file))
	{
		if (str_empty(info.message_content))
		{
			strcpy(buffer, tl("The message appears to have been deleted."));
			status = 400;
			goto finish;
		}

		struct tts_input in = {
			.input = info.message_content,
			.dbc = &c->dbc,
			.messageId = id
		};
		struct tts_output out = {.content = new_char_array(NULL)};
		Charray buf = buffer_to_char_array(buffer, sizeof(buffer));

		errno_t e = text_to_speech(&out, in, &buf);
		if (e != 0)
		{
			status = errno_to_status_code(e);
			goto finish;
		}

		UploadFile uf = {0};
		uf.sessionId = str_to_long(c->identity.sid);

		uf.data.content = array_to_string(&out.content);
		uf.data.content_type.value = out.content_type;

		strcpy(uf.folder, "ai_voice");
		sprintf(uf.name, "%s.mp3", id);

		// provide the filename seen by user upon download
		uf.data.disposition.filename = "DRIIMA-voice.mp3";

		e = complete_file_upload(&c->dbc, &uf, &buf);
		charray_free(&out.content); // free memory

		if (e != 0)
			goto finish;

		query.callback = NULL;
		query.sql = "UPDATE Messages SET FileId = ? WHERE Id = UNHEX(?)";
		query.argc = 0;
		argv[query.argc++] = json_new_long(uf.id, false);
		argv[query.argc++] = json_new_str(id, false);
		sql_exec(&query, argv);

		sprintf(info.voice_file, "%s/%s", uf.folder, uf.name);
	}

	if (!file_path_to_full_url(c, buffer, sizeof(buffer), info.voice_file))
		status = 500;

	vm_add_string(c, "url", buffer, 0);

finish:
	if (status == OK)
		status = process_model(c, HTTP_OK);
	else
		status = http_problem(c, NULL, buffer, status);

	_free(info.message_content, "info_message_content");
	return status;
}

static apr_status_t join_group(HttpContext *c)
{
	UrlArgs args = get_url_args(c);
	char buffer[1024];
	RoomInfo room;

	apr_status_t status = get_room_info(c, &room, args, buffer, false);
	if (status != OK)
		return http_problem(c, NULL, buffer, status);

	if (room.memberId != 0)
		return HTTP_NO_CONTENT;

	DbQuery query = {.dbc = &c->dbc};
	query.sql = "INSERT INTO GroupMembers (GroupId, MemberId) VALUES (?, ?)";

	JsonValue argv[2];
	argv[query.argc++] = json_new_int(room.groupId, false);
	argv[query.argc++] = json_new_int(args.userId, false);

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

	UrlArgs args = get_url_args(c);

	if (args.roomId == 0 && args.groupId == 0)
	{
		set_page_title(c, "DRIIMA");
		return process_view(c);
	}

	char buffer[1024];
	RoomInfo room;

	apr_status_t status = get_room_info(c, &room, args, buffer, true);
	if (status != OK)
		return http_problem(c, NULL, buffer, status);

	JsonObject *og = json_new_object();
	json_put_string(og, "Type", "website", 0);

	if (str_empty(room.roomName))
		sprintf(buffer, "%s - DRIIMA", room.groupName);
	else sprintf(buffer, "%s: %s - DRIIMA", room.groupName, room.roomName);
	json_put_string(og, "Title", buffer, 0);
	set_page_title(c, buffer);

	json_put_string(og, "Description", room.groupAbout, 0);
	_free(room.groupAbout, "room_groupAbout");
	room.groupAbout = NULL;

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

static apr_status_t anonymous_chat(HttpContext *c)
{
	return http_redirect(c, "/chat", HTTP_INTERNAL_REDIRECT, true);
}

void register_message_controller(void)
{
	CHECK_ERRNO;
	add_endpoint(M_GET, "/anonymous/chat", anonymous_chat, 0); // obsolete
	add_endpoint(M_GET, "/", home_page, 0);
	add_endpoint(M_GET, "/chat", chat_page, 0);

	add_endpoint(M_GET, "/api/room/messages", get_messages, Endpoint_AuthWebAPI);
	add_endpoint(M_GET, "/api/message/many", get_messages, Endpoint_AuthWebAPI); // obsolete
	add_endpoint(M_POST, "/api/room/join", join_group, Endpoint_AuthWebAPI);

	add_endpoint(M_POST, "/api/message/send", send_message, Endpoint_AuthWebAPI);
	add_endpoint(M_DELETE, "/api/message/delete", delete_message, Endpoint_AuthWebAPI);
	add_endpoint(M_PATCH, "/api/message/hide-from-ai", hide_message_from_ai, Endpoint_AuthWebAPI);
	add_endpoint(M_GET, "/api/message/read-aloud", read_aloud, Endpoint_AuthWebAPI);
}
