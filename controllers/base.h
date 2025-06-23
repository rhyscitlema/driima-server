#ifndef _BASE_CONTROLLER_H_
#define _BASE_CONTROLLER_H_

#include "../startup.h"
#include "../models/message.h"

apr_status_t ensure_session_exists(HttpContext *c);

#endif
