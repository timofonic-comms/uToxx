#include "settings.h"

#include "flist.h"
#include "groups.h"
#include "main.h" // UTOX_VERSION_NUMBER, MAIN_HEIGHT, MAIN_WIDTH, all save things..
#include "tox.h"

#include "layout/settings.h"

#include "native/filesys.h"
#include "native/keyboard.h"

#include "ui/dropdown.h"
#include "ui/edit.h"
#include "ui/switch.h"

#include <stdlib.h>
#include <string.h>

uint16_t loaded_audio_out_device = 0;
uint16_t loaded_audio_in_device  = 0;

SETTINGS settings = {
    // .last_version                // included here to match the full struct
    .curr_version = UTOX_VERSION_NUMBER,
    .next_version = UTOX_VERSION_NUMBER,

    // Low level settings (network, profile, portable-mode)
    .enable_udp     = true,
    .enable_ipv6    = true,

    .use_proxy      = false,
    .force_proxy    = false,
    .proxy_port     = 0,

    // Tox level settings
    .save_encryption        = true,

    // .portable_mode               // included here to match the full struct

    // User interface settings
    .audiofilter_enabled    = true,
    .push_to_talk           = false,
    .audio_preview          = false,
    .video_preview          = false,
    .send_typing_status     = false,
    // .inline_video                // included here to match the full struct
    .use_long_time_msg      = true,
    .accept_inline_images   = true,

    // UX Settings
    .logging_enabled        = true,
    .close_to_tray          = false,
    .start_in_tray          = false,
    .start_with_system      = false,
    .use_mini_flist         = false,
    .magic_flist_enabled    = false,


    // Notifications / Alerts
    .ringtone_enabled       = true,
    .status_notifications   = true,
    .group_notifications    = GNOTIFY_ALWAYS,

    // .theme                       // included here to match the full struct
    // OS interface settings
    .window_x             = 0,
    .window_y             = 0,
    .window_height        = MAIN_HEIGHT,
    .window_width         = MAIN_WIDTH,
    .window_baseline      = 0,

    .window_maximized     = 0,
};

// TODO refactor to match same order in main.h
UTOX_SAVE *config_load(void) {
    UTOX_SAVE *save = utox_data_load_utox();
    if (!save) {
        /* Create and set defaults */
        save = calloc(1, sizeof(UTOX_SAVE));
        save->enableipv6  = 1;
        save->disableudp  = 0;
        save->proxyenable = 0;

        save->audio_filtering_enabled       = 1;
        save->audible_notifications_enabled = 1;
    }

    if (save->scale > 30) {
        save->scale = 30;
    } else if (save->scale < 5) {
        save->scale = 10;
    }

    if (save->window_width < MAIN_WIDTH) {
        save->window_width = MAIN_WIDTH;
    }
    if (save->window_height < MAIN_HEIGHT) {
        save->window_height = MAIN_HEIGHT;
    }

    /* UX Settings */
    dropdown_dpi.selected = dropdown_dpi.over = save->scale - 5;

    switch_save_chat_history.switch_on  = save->logging_enabled;
    switch_close_to_tray.switch_on      = save->close_to_tray;
    switch_start_in_tray.switch_on      = save->start_in_tray;
    switch_mini_contacts.switch_on      = save->use_mini_flist;
    switch_magic_sidebar.switch_on      = save->magic_flist_enabled;

    switch_ipv6.switch_on        = save->enableipv6;
    switch_udp.switch_on         = !save->disableudp;
    switch_proxy.switch_on       = save->proxyenable;

    // TODO, this is a bug. We really should be saving this data, but I don't want
    // to touch this until we decide how we want to save uTox data in the future.
    // -- Grayhatter, probably...
    switch_proxy_force.switch_on = false;

    switch_auto_startup.switch_on = save->auto_startup;

    switch_audible_notifications.switch_on = save->audible_notifications_enabled;
    switch_audio_filtering.switch_on       = save->audio_filtering_enabled;
    switch_push_to_talk.switch_on          = save->push_to_talk;
    switch_status_notifications.switch_on  = save->status_notifications;

    dropdown_theme.selected = dropdown_theme.over = save->theme;

    switch_typing_notes.switch_on = !save->no_typing_notifications;

    flist_set_filter(save->filter); /* roster list filtering */

    /* Network settings */
    settings.enable_ipv6 = save->enableipv6;
    settings.enable_udp  = !save->disableudp;
    settings.use_proxy   = !!save->proxyenable;
    settings.proxy_port  = save->proxy_port;

    if (strlen((char *)save->proxy_ip) <= 256){
        strcpy((char *)proxy_address, (char *)save->proxy_ip);
    }

    edit_proxy_ip.length = strlen((char *)save->proxy_ip);

    strcpy((char *)edit_proxy_ip.data, (char *)save->proxy_ip);

    if (save->proxy_port) {
        edit_proxy_port.length =
            snprintf((char *)edit_proxy_port.data, edit_proxy_port.maxlength + 1, "%u", save->proxy_port);
        if (edit_proxy_port.length >= edit_proxy_port.maxlength + 1) {
            edit_proxy_port.length = edit_proxy_port.maxlength;
        }
    }

    /* UX settings */
    settings.logging_enabled        = save->logging_enabled;
    settings.close_to_tray          = save->close_to_tray;
    settings.start_in_tray          = save->start_in_tray;
    settings.start_with_system      = save->auto_startup;
    settings.use_mini_flist         = save->use_mini_flist;
    settings.magic_flist_enabled    = save->magic_flist_enabled;

    settings.ringtone_enabled       = save->audible_notifications_enabled;
    settings.audiofilter_enabled    = save->audio_filtering_enabled;

    settings.send_typing_status     = !save->no_typing_notifications;
    settings.group_notifications    = save->group_notifications;
    settings.status_notifications   = save->status_notifications;

    settings.window_width           = save->window_width;
    settings.window_height          = save->window_height;

    settings.last_version           = save->utox_last_version;

    loaded_audio_out_device         = save->audio_device_out;
    loaded_audio_in_device          = save->audio_device_in;

    // TODO: Don't clobber (and start saving) commandline flags.

    // Allow users to override theme on the cmdline.
    // 0 is the default theme.
    // TODO: `utox -t default` is still broken.
    if (settings.theme == 0) {
        settings.theme = save->theme;
    }

    ui_set_scale(save->scale);

    if (save->push_to_talk) {
        init_ptt();
    }

    return save;
}

// TODO refactor to match order in main.h
void config_save(UTOX_SAVE *save_in) {
    UTOX_SAVE *save = calloc(1, sizeof(UTOX_SAVE) + 256);

    /* Copy the data from the in data to protect the calloc */
    save->window_x                      = save_in->window_x;
    save->window_y                      = save_in->window_y;
    save->window_width                  = save_in->window_width;
    save->window_height                 = save_in->window_height;

    save->save_version                  = UTOX_SAVE_VERSION;
    save->scale                         = ui_scale;
    save->proxyenable                   = switch_proxy.switch_on;
    save->audible_notifications_enabled = settings.ringtone_enabled;
    save->audio_filtering_enabled       = settings.audiofilter_enabled;
    save->push_to_talk                  = settings.push_to_talk;

    /* UX Settings */
    save->logging_enabled               = settings.logging_enabled;
    save->close_to_tray                 = settings.close_to_tray;
    save->start_in_tray                 = settings.start_in_tray;
    save->auto_startup                  = settings.start_with_system;
    save->use_mini_flist                = settings.use_mini_flist;
    save->magic_flist_enabled           = settings.magic_flist_enabled;


    save->disableudp              = !settings.enable_udp;
    save->enableipv6              = settings.enable_ipv6;
    save->no_typing_notifications = !settings.send_typing_status;

    save->filter     = flist_get_filter();
    save->proxy_port = settings.proxy_port;

    save->audio_device_in  = dropdown_audio_in.selected;
    save->audio_device_out = dropdown_audio_out.selected;
    save->theme            = settings.theme;

    save->utox_last_version    = settings.curr_version;
    save->group_notifications  = settings.group_notifications;
    save->status_notifications = settings.status_notifications;

    memcpy(save->proxy_ip, proxy_address, 256); /* Magic number inside toxcore */

    utox_data_save_utox(save, sizeof(UTOX_SAVE) + 256); /* Magic number inside toxcore */
    free(save);
}


bool utox_data_save_utox(UTOX_SAVE *data, size_t size) {
    FILE *fp = utox_get_file("utox_save", NULL, UTOX_FILE_OPTS_WRITE);

    if (!fp) {
        return false;
    }

    if (fwrite(data, size, 1, fp) != 1) {
        fclose(fp);
        return false;
    }

    flush_file(fp);
    fclose(fp);

    return true;
}

UTOX_SAVE *utox_data_load_utox(void) {
    size_t size = 0;
    FILE *fp = utox_get_file("utox_save", &size, UTOX_FILE_OPTS_READ);

    if (!fp) {
        return NULL;
    }

    UTOX_SAVE *save = calloc(1, size + 1);
    if (fread(save, size, 1, fp) != 1) {
        fclose(fp);
        free(save);
        return NULL;
    }

    fclose(fp);
    return save;
}
