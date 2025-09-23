#ifndef _ENUMS_H_
#define _ENUMS_H_

enum PlatformType
{
	PlatformType_Unknown = 0,
	PlatformType_Server = 1,
	PlatformType_Mobile = 2,
	PlatformType_Desktop = 3,
	PlatformType_Browser = 4,
};

enum UserType
{
	UserType_Unknown,
	UserType_Normal,
	UserType_Anonymous,
	UserType_Server,
};

enum UserStatus
{
	UserStatus_Unknown,
	UserStatus_Active,
	UserStatus_Deleted,
	UserStatus_Blocked,
};

enum SessionStatus
{
	SessionStatus_Unknown = 0,
	SessionStatus_Active = 1,
	SessionStatus_LoggedOut = 2,
	SessionStatus_Blocked = 3,
};

enum RoomState
{
	RoomState_Unknown = 0,
	RoomState_Normal = 1,
	RoomState_AIBusy = 2,
};

enum MessageType
{
	MessageType_Unknown,
	MessageType_Normal,
	MessageType_ToolCall,
	MessageType_Event,
};

enum MessageStatus
{
	MessageStatus_Unknown = 0,
	MessageStatus_Sent = 1,
};

#endif
