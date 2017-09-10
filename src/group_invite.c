#include "group_invite.h"

#include "groups.h"
#include "utox.h"

#include <stdlib.h>

#include <stdio.h>

#define MAX_GROUP_INVITES 16

typedef struct group_invite {
    uint32_t friend_number;
    const uint8_t *cookie;
    size_t length;
    bool av_group;
} GROUP_INVITE;

static GROUP_INVITE *invites[MAX_GROUP_INVITES];

static uint8_t find_free_slot(void) {
    for (uint8_t i = 0; i < MAX_GROUP_INVITES; ++i) {
        if (!invites[i]) {
            printf("found id %d\n", i);
            fflush(NULL);
            return i;
        }
    }
    return UINT8_MAX;
}

static void group_invite_free(const uint8_t invite_id) {
    free(invites[invite_id]);
    invites[invite_id] = NULL;
}

uint8_t group_invite_new(const uint32_t friend_number,
                         const uint8_t *cookie,
                         const size_t length,
                         const bool is_av_group)
{
    const uint8_t invite_id = find_free_slot();
    if (invite_id == UINT8_MAX) {
        return UINT8_MAX;
    }

    GROUP_INVITE *invite = calloc(1, sizeof(GROUP_INVITE));
    if (!invite) {
        return UINT8_MAX;
    }

    invite->friend_number = friend_number;
    invite->cookie = cookie;
    invite->length = length;
    invite->av_group = is_av_group;

    invites[invite_id] = invite;
    printf("created group invite %d from friend %u\n",
           invite_id, invites[invite_id]->friend_number);
    fflush(NULL);

    return invite_id;
}

bool group_invite_accept(Tox *tox, const uint8_t invite_id) {
    const uint32_t group_id = tox_conference_join(tox,
                                                  invites[invite_id]->friend_number,
                                                  invites[invite_id]->cookie,
                                                  invites[invite_id]->length,
                                                  NULL);
    if (group_id == UINT32_MAX) {
        group_invite_free(invite_id);
        return false;
    }

    GROUPCHAT *groupchat = get_group(group_id);
    if (groupchat) {
        group_init(groupchat, group_id, invites[invite_id]->is_av_group);
    } else {
        group_create(group_id, invites[invite_id]->is_av_group);
    }

    group_invite_free(invite_id);

    postmessage_utox(GROUP_ADD, group_id, 0, tox);
    return true;
}

void group_invite_reject(const uint8_t invite_id) {
    free(invites[invite_id]);
    invites[invite_id] = NULL;
}

uint32_t group_invite_get_friend_id(const uint8_t invite_id) {
    return invites[invite_id]->friend_number;
}
