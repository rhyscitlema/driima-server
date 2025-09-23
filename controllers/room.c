#include "base.h"

struct get_rooms
{
	HttpContext *c;
	JsonArray *rooms;
};

static errno_t get_rooms_callback(void *context, int argc, char **argv, char **columns)
{
	struct get_rooms *info = (struct get_rooms *)context;

	JsonArray *room = json_new_object();
	json_array_add(info->rooms, room);

	const char *logo = NULL, *banner = NULL;

	for (int i = 0; i < argc; i++)
	{
		KeyValuePair x = {columns[i], argv[i]};
		json_kvp_number(room, x, "roomId");
		json_kvp_number(room, x, "groupId");
		json_kvp_string(room, x, "roomName");
		json_kvp_string(room, x, "groupName");
		json_kvp_number(room, x, "groupStatus");
		json_kvp_number(room, x, "memberStatus");
		json_kvp_date_l(room, x, "dateMuted");
		json_kvp_date_l(room, x, "datePinned");
		json_kvp_date_l(room, x, "latestDateSent");
		json_kvp_string(room, x, "latestMessage");
		KVP_TO_STR(x, logo, "groupLogo");
		KVP_TO_STR(x, banner, "groupBanner");
	}

	char buffer[MIN_BUFFER_SIZE];
	if (str_empty(logo))
		logo = banner;

	if (file_path_to_full_url(info->c, buffer, sizeof(buffer), logo))
		json_put_string(room, "logo", buffer, 0);

	return 0;
}

static apr_status_t get_rooms(HttpContext *c)
{
	JsonArray *rooms = json_new_array();
	vm_add_node(c, "rooms", rooms, 0);

	DbQuery query = {.dbc = &c->dbc};
	query.callback = get_rooms_callback;

	struct get_rooms info = {c, rooms};
	query.callback_context = &info;

	query.sql =
		"select * from ViewRooms as r\n"
		"join ViewRoomMembers as rm on rm.RoomId = r.Id\n"
		"where MemberId = ?\n"
		"order by LatestDateSent desc, GroupName asc\n";

	long userId = str_to_long(c->identity.sub);
	JsonValue argv[1];
	argv[0] = json_new_long(userId, false);
	query.argc = 1;

	if (sql_exec(&query, argv) != 0)
		return http_problem(c, NULL, tl("Internal error: failed to get data"), 500);

	return process_model(c, HTTP_OK);
}

void register_room_controller(void)
{
	CHECK_ERRNO;
	add_endpoint(M_GET, "/api/rooms", get_rooms, Endpoint_AuthWebAPI);
}
