#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <db_context.h>
#include "../enums.h"

typedef struct Message
{
	const char *id;
	const char *parentId;
	int roomId;
	int senderId; // a FK to Sessions(Id)
	time_us_t dateSent;
	enum MessageType type;
	enum MessageStatus status;
	const char *content;
} Message;

errno_t add_message(DbContext *dbc, Message m, char id[GUID_STORE]);

errno_t update_room_state(DbContext *dbc, int roomId, enum RoomState state);

struct send_to_ai
{
	AppBackup app_backup;
	char messageId[GUID_STORE];
	int roomId;
};
errno_t send_message_to_ai(struct send_to_ai *data);

#endif
