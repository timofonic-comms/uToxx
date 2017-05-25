#include "background.h"

#include "friend.h"
#include "group.h"
#include "notify.h"
#include "settings.h"
#include "sidebar.h"

#include "../theme.h"
#include "../ui.h"

#include "../ui/draw.h"
#include "../ui/panel.h"

#include <stddef.h>

static void draw_background(int x, int y, int width, int height) {
    /* Default background */
    drawrect(x, y, width, height, COLOR_BKGRND_MAIN);

    if (!panel_chat.disabled) {
        /* Top frame for main chat panel */
        drawrect(x, 0, width, SCALE(MAIN_TOP_FRAME_THICK), COLOR_BKGRND_ALT);
        drawhline(x, SCALE(MAIN_TOP_FRAME_THICK), width, COLOR_EDGE_NORMAL);
        /* Frame for the bottom chat text entry box */
        drawrect(x, height + SCALE(CHAT_BOX_TOP), width, height, COLOR_BKGRND_ALT);
        drawhline(x, height + SCALE(CHAT_BOX_TOP), width, COLOR_EDGE_NORMAL);
    }
    // Chat and chat header separation
    if (panel_settings_master.disabled) {
        drawhline(x, SCALE(MAIN_TOP_FRAME_THICK), width, COLOR_EDGE_NORMAL);
    }
}

PANEL
panel_root = {
    .type = PANEL_NONE,
    .drawfunc = draw_background,
    .disabled = 0,
    .child = (PANEL*[]) {
        &panel_side_bar,
        &panel_main,
        NULL
    }
},

/* Main panel, holds the overhead/settings, or the friend/group containers */
panel_main = {
    .type = PANEL_NONE,
    .disabled = 0,
    .child = (PANEL*[]) {
        &panel_chat,
        &panel_overhead,
        NULL
    }
},

/* Chat panel, friend or group, depending on what's selected */
panel_chat = {
    .type = PANEL_NONE,
    .disabled = 1,
    .child = (PANEL*[]) {
        &panel_group,
        &panel_friend,
        &panel_friend_request,
        NULL
    }
},

/* Settings master panel, holds the lower level settings */
panel_overhead = {
    .type = PANEL_NONE,
    .disabled = 0,
    .child = (PANEL*[]) {
        &panel_profile_password,
        &panel_add_friend,
        &panel_settings_master,
        NULL
    }
};
