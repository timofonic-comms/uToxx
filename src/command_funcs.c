#include "command_funcs.h"

#include "friend.h"
#include "groups.h"
#include "tox.h"
#include "macros.h"

#include <stdlib.h>
#include <string.h>

bool slash_send_file(void *object, char *filepath, int UNUSED(arg_length)) {
    if (!filepath) {
        return false;
    }

    FRIEND *f = object;
    postmessage_toxcore(TOX_FILE_SEND_NEW_SLASH, f->number, 0xFFFF, (void *)filepath);
    return true;
}

bool slash_alias(void *object, char *arg, int arg_length) {
    FRIEND *f =  object;
    if (arg) {
        friend_set_alias(f, (uint8_t *)arg, arg_length);
    } else {
        friend_set_alias(f, NULL, 0);
    }

    utox_write_metadata(f);
    return true;
}

bool slash_invite(void *object, char *arg, int UNUSED(arg_length)) {
    FRIEND *f = find_friend_by_name((uint8_t *)arg);
    if (!f || !f->online) {
        return false;
    }

    GROUPCHAT *g = object;
    postmessage_toxcore(TOX_GROUP_SEND_INVITE, g->number, f->number, NULL);
    return true;
}

bool slash_topic(void *object, char *arg, int arg_length) {
    void *d = calloc(1, arg_length);
    if (!d) {
        return false;
    }

    GROUPCHAT *g = object;
    memcpy(d, arg, arg_length);
    postmessage_toxcore(TOX_GROUP_SET_TOPIC, g->number, arg_length, d);
    return true;
}
