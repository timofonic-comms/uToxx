#include "groups.h"

#include "flist.h"
#include "macros.h"
#include "self.h"
#include "settings.h"
#include "text.h"

#include "av/audio.h"
#include "av/utox_av.h"

#include "native/notify.h"

#include "ui/edit.h"

#include "layout/group.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <tox/tox.h>

static GROUPCHAT *group = NULL;

GROUPCHAT *get_group(uint32_t group_number) {
    if (group_number >= self.groups_list_size) {
        return NULL;
    }

    return &group[group_number];
}

/*
 * Create a new slot for the group if group_number is greater than self.groups_list_size and return a pointer to it
 * If group_number is less than self.groups_list_size return a pointer to that slot
 */
static GROUPCHAT *group_make(uint32_t group_number) {
    if (group_number >= self.groups_list_size) {
        GROUPCHAT *tmp = realloc(group, sizeof(GROUPCHAT) * (group_number + 1));
        if (!tmp) {
            exit(1);
        }

        group = tmp;
        self.groups_list_size++;
        self.groups_list_count++;
    }

    memset(&group[group_number], 0, sizeof(GROUPCHAT));

    return &group[group_number];
}

bool group_create(uint32_t group_number, bool av_group) {
    GROUPCHAT *g = group_make(group_number);
    if (!g) {
        return false;
    }

    group_init(g, group_number, av_group);
    return true;
}

void group_init(GROUPCHAT *g, uint32_t group_number, bool av_group) {
    pthread_mutex_lock(&messages_lock); /* make sure that messages has posted before we continue */
    if (!g->peer) {
        g->peer = calloc(UTOX_MAX_GROUP_PEERS, sizeof(GROUP_PEER *));
    }

    g->name_length = snprintf((char *)g->name, sizeof(g->name), "Groupchat #%u", group_number);
    if (g->name_length >= sizeof(g->name)) {
        g->name_length = sizeof(g->name) - 1;
    }

    g->topic_length = sizeof("Drag friends to invite them") - 1;
    memcpy(g->topic, "Drag friends to invite them", sizeof("Drag friends to invite them") - 1);

    g->msg.scroll               = 1.0;
    g->msg.panel.type           = PANEL_MESSAGES;
    g->msg.panel.content_scroll = &scrollbar_group;
    g->msg.panel.y              = MAIN_TOP;
    g->msg.panel.height         = CHAT_BOX_TOP;
    g->msg.panel.width          = -SCROLL_WIDTH;
    g->msg.is_groupchat         = true;

    g->number   = group_number;
    g->notify   = settings.group_notifications;
    g->av_group = av_group;
    pthread_mutex_unlock(&messages_lock);

    flist_add_group(g);
    flist_select_last();
}

uint32_t group_add_message(GROUPCHAT *g, uint32_t peer_id, const uint8_t *message, size_t length,
                           uint8_t m_type)
{
    pthread_mutex_lock(&messages_lock);

    if (peer_id >= UTOX_MAX_GROUP_PEERS) {
        pthread_mutex_unlock(&messages_lock);
        return UINT32_MAX;
    }

    const GROUP_PEER *peer = g->peer[peer_id];
    if (!peer) {
        pthread_mutex_unlock(&messages_lock);
        return UINT32_MAX;
    }

    MSG_HEADER *msg = calloc(1, sizeof(MSG_HEADER));
    msg->our_msg  = (g->our_peer_number == peer_id ? true : false);
    msg->msg_type = m_type;

    msg->via.grp.length    = length;
    msg->via.grp.author_id = peer_id;

    msg->via.grp.author_length = peer->name_length;
    msg->via.grp.author_color  = peer->name_color;
    time(&msg->time);

    msg->via.grp.author = calloc(1, peer->name_length);
    memcpy(msg->via.grp.author, peer->name, peer->name_length);

    msg->via.grp.msg = calloc(1, length);
    memcpy(msg->via.grp.msg, message, length);

    pthread_mutex_unlock(&messages_lock);

    MESSAGES *m = &g->msg;
    return message_add_group(m, msg);
}

void group_peer_add(GROUPCHAT *g, uint32_t peer_id, bool UNUSED(our_peer_number), uint32_t name_color) {
    pthread_mutex_lock(&messages_lock); /* make sure that messages has posted before we continue */
    if (!g->peer) {
        g->peer = calloc(UTOX_MAX_GROUP_PEERS, sizeof(GROUP_PEER *));
    }

    const char *default_peer_name = "<unknown>";

    // Allocate space for the struct and the dynamic array holding the peer's name.
    GROUP_PEER *peer = calloc(1, sizeof(GROUP_PEER) + strlen(default_peer_name) + 1);
    strcpy2(peer->name, default_peer_name);
    peer->name_length = 0;
    peer->name_color  = name_color;
    peer->id          = peer_id;

    g->peer[peer_id] = peer;
    g->peer_count++;

    if (g->av_group) {
        group_av_peer_add(g, peer_id); //add a source for the peer
    }

    pthread_mutex_unlock(&messages_lock);
}

void group_peer_del(GROUPCHAT *g, uint32_t peer_id) {
    group_add_message(g, peer_id, (uint8_t *)"<- has Quit!", 12, MSG_TYPE_NOTICE);

    pthread_mutex_lock(&messages_lock);

    if (!g->peer) {
        pthread_mutex_unlock(&messages_lock);
        return;
    }

    GROUP_PEER *peer = g->peer[peer_id];

    if (peer) {
        free(peer);
    } else {
        pthread_mutex_unlock(&messages_lock);
        return;
    }
    g->peer_count--;
    g->peer[peer_id] = NULL;
    pthread_mutex_unlock(&messages_lock);
}

void group_peer_name_change(GROUPCHAT *g, uint32_t peer_id, const uint8_t *name, size_t length) {
    pthread_mutex_lock(&messages_lock); /* make sure that messages has posted before we continue */
    if (!g->peer) {
        pthread_mutex_unlock(&messages_lock);
        return;
    }

    GROUP_PEER *peer = g->peer[peer_id];
    if (!peer) {
        exit(1);
    }

    if (peer->name_length) {
        char old[TOX_MAX_NAME_LENGTH];
        char msg[TOX_MAX_NAME_LENGTH];

        memcpy(old, peer->name, peer->name_length);
        size_t size = snprintf(msg, TOX_MAX_NAME_LENGTH, "<- has changed their name from %.*s",
                               peer->name_length, old);

        GROUP_PEER *new_peer = realloc(peer, sizeof(GROUP_PEER) + sizeof(char) * length);

        if (!new_peer) {
            free(peer);
            exit(1);
        }

        peer = new_peer;
        peer->name_length = utf8_validate(name, length);
        memcpy(peer->name, name, length);
        g->peer[peer_id] = peer;

        pthread_mutex_unlock(&messages_lock);
        group_add_message(g, peer_id, (uint8_t *)msg, size, MSG_TYPE_NOTICE);
        return;
    }

    /* Hopefully, they just joined, because that's the UX message we're going with! */
    GROUP_PEER *new_peer = realloc(peer, sizeof(GROUP_PEER) + sizeof(char) * length);
    if (!new_peer) {
        free(peer);
        exit(1);
    }

    peer = new_peer;
    peer->name_length = utf8_validate(name, length);
    memcpy(peer->name, name, length);
    g->peer[peer_id] = peer;

    pthread_mutex_unlock(&messages_lock);
    group_add_message(g, peer_id, (uint8_t *)"<- has joined the chat!", 23, MSG_TYPE_NOTICE);
}

void group_reset_peerlist(GROUPCHAT *g) {
    for (size_t i = 0; i < g->peer_count; ++i) {
        if (g->peer[i]) {
            free(g->peer[i]);
        }
    }
    free(g->peer);
}

void group_free(GROUPCHAT *g) {
    for (size_t i = 0; i < g->edit_history_length; ++i) {
        free(g->edit_history[i]);
    }

    free(g->edit_history);

    group_reset_peerlist(g);

    for (size_t i = 0; i < g->msg.number; ++i) {
        free(g->msg.data[i]->via.grp.author);

        // Freeing this here was causing a double free.
        // TODO: Is it needed to prevent a memory leak in some cases?
        // free(g->msg.data[i]->via.grp.msg);

        message_free(g->msg.data[i]);
    }
    free(g->msg.data);

    memset(g, 0, sizeof(GROUPCHAT));

    self.groups_list_count--;
}

void raze_groups(void) {
    for (size_t i = 0; i < self.groups_list_count; i++) {
        GROUPCHAT *g = get_group(i);
        if (!g) {
            continue;
        }
        group_free(g);
    }

    free(group);
    group = NULL;
}

void init_groups(void) {
    self.groups_list_size = 0;

    if (self.groups_list_size == 0) {
        return;
    }

    group = calloc(self.groups_list_size, sizeof(GROUPCHAT));

    for (size_t i = 0; i < self.groups_list_size; i++) {
        // TODO: figure out if groupchats are text or audio
        group_create(i, false);
    }
}


void group_notify_msg(GROUPCHAT *g, const char *msg, size_t msg_length) {
    if (g->notify == GNOTIFY_NEVER) {
        return;
    }

    if (g->notify == GNOTIFY_HIGHLIGHTS && strstr(msg, self.name) == NULL) {
        return;
    }

    char title[g->name_length + 25];

    size_t title_length = snprintf(title, g->name_length + 25, "uTox new message in %.*s",
                                   g->name_length, g->name);

    notify(title, title_length, msg, msg_length, g, 1);

    if (flist_get_groupchat() != g) {
        postmessage_audio(UTOXAUDIO_PLAY_NOTIFICATION, NOTIFY_TONE_FRIEND_NEW_MSG, 0, NULL);
    }
}
