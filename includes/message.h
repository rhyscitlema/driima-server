#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <db_context.h>
#include "../includes/enums.h"

#define FILE_PATH_STORE 200

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

struct tts_input
{
	const char *model;
	const char *voice;
	const char *input;

	DbContext *dbc; // must not be NULL
	const char *messageId; // can be NULL
};
struct tts_output
{
	Charray content;
	char content_type[GUID_STORE];
};
errno_t text_to_speech(struct tts_output *out, struct tts_input info, Charray *buffer);

#endif
