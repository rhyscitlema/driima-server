#ifndef _BASE_CONTROLLER_H_
#define _BASE_CONTROLLER_H_

#include <http_context.h>
#include <file_upload.h>
#include "../enums.h"

void register_account_controller(void);
void register_message_controller(void);
void register_room_controller(void);

apr_status_t ensure_session_exists(HttpContext *c);

#endif
