#include "main.h"

#include "settings.h"
#include "theme.h"
#include "updater.h"

#include "native/filesys.h"
#include "native/main.h"
#include "native/thread.h"

#include <getopt.h>
#include <string.h>

/* The utox_ functions contained in src/main.c are wrappers for the platform native_ functions
 * if you need to localize them to a specific platform, move them from here, to each
 * src/<platform>/main.x and change from utox_ to native_
 */

bool utox_data_save_tox(uint8_t *data, size_t length) {
    FILE *fp = utox_get_file("tox_save.tox", NULL, UTOX_FILE_OPTS_WRITE);
    if (!fp) {
        return true;
    }

    if (fwrite(data, length, 1, fp) != 1) {
                fclose(fp);
        return true;
    }

    flush_file(fp);
    fclose(fp);

    return false;
}

uint8_t *utox_data_load_tox(size_t *size) {
    const char name[][20] = { "tox_save.tox", "tox_save.tox.atomic", "tox_save.tmp", "tox_save" };

    for (uint8_t i = 0; i < 4; i++) {
        size_t length = 0;

        FILE *fp = utox_get_file(name[i], &length, UTOX_FILE_OPTS_READ);
        if (!fp) {
            continue;
        }

        uint8_t *data = calloc(1, length + 1);

        if (!data) {
            fclose(fp);
            // Quit. We're out of memory, calloc will fail again.
            return NULL;
        }

        if (fread(data, length, 1, fp) != 1) {
            fclose(fp);
            free(data);
            // Return NULL, because if a Tox save exits we don't want to fall
            // back to an old version, we need the user to decide what to do.
            return NULL;
        }

        fclose(fp);
        *size = length;
        return data;
    }

    return NULL;
}

bool utox_data_save_ftinfo(char hex[TOX_PUBLIC_KEY_SIZE * 2], uint8_t *data, size_t length) {
    char name[TOX_PUBLIC_KEY_SIZE * 2 + sizeof(".ftinfo")];
    snprintf(name, sizeof(name), "%.*s.ftinfo", TOX_PUBLIC_KEY_SIZE * 2, hex);

    FILE *fp = utox_get_file(name, NULL, UTOX_FILE_OPTS_WRITE);

    if (fp == NULL) {
        return false;
    }

    if (fwrite(data, length, 1, fp) != 1) {
                fclose(fp);
        return false;
    }

    fclose(fp);

    return true;
}

/* Shared function between all four platforms */
void parse_args(int argc, char *argv[],
                bool *skip_updater,
                int8_t *should_launch_at_startup,
                int8_t *set_show_window)
{
    // set default options
    if (skip_updater) {
        *skip_updater = false;
    }

    if (should_launch_at_startup) {
        *should_launch_at_startup = 0;
    }

    if (set_show_window) {
        *set_show_window = 0;
    }

    static struct option long_options[] = {
        { "theme", required_argument, NULL, 't' },
        { "portable", no_argument, NULL, 'p' },
        { "set", required_argument, NULL, 's' },
        { "unset", required_argument, NULL, 'u' },
        { "skip-updater", no_argument, NULL, 'N' },
        { "delete-updater", required_argument, NULL, 'D'},
        { "version", no_argument, NULL, 0 },
        { "silent", no_argument, NULL, 'S' },
        { "verbose", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },
        { "debug", required_argument, NULL, 1 },
        { 0, 0, 0, 0 }
    };

    int opt, long_index = 0;
    while ((opt = getopt_long(argc, argv, "t:ps:u:nvh", long_options, &long_index)) != -1) {
        // loop through each option; ":" after each option means an argument is required
        switch (opt) {
            case 't': {
                if (!strcmp(optarg, "default")) {
                    settings.theme = THEME_DEFAULT;
                } else if (!strcmp(optarg, "dark")) {
                    settings.theme = THEME_DARK;
                } else if (!strcmp(optarg, "light")) {
                    settings.theme = THEME_LIGHT;
                } else if (!strcmp(optarg, "highcontrast")) {
                    settings.theme = THEME_HIGHCONTRAST;
                } else if (!strcmp(optarg, "zenburn")) {
                    settings.theme = THEME_ZENBURN;
                } else if (!strcmp(optarg, "solarized-light")) {
                    settings.theme = THEME_SOLARIZED_LIGHT;
                } else if (!strcmp(optarg, "solarized-dark")) {
                    settings.theme = THEME_SOLARIZED_DARK;
                } else {
                    exit(EXIT_FAILURE);
                }
                break;
            }

            case 'p': {
                settings.portable_mode = 1;
                break;
            }

            case 's': {
                if (!strcmp(optarg, "start-on-boot")) {
                    if (should_launch_at_startup) {
                        *should_launch_at_startup = 1;
                    }
                } else if (!strcmp(optarg, "show-window")) {
                    if (set_show_window) {
                        *set_show_window = 1;
                    }
                } else if (!strcmp(optarg, "hide-window")) {
                    if (set_show_window) {
                        *set_show_window = -1;
                    }
                } else {
                    exit(EXIT_FAILURE);
                }
                break;
            }

            case 'u': {
                if (!strcmp(optarg, "start-on-boot")) {
                    if (should_launch_at_startup) {
                        *should_launch_at_startup = -1;
                    }
                } else {
                    exit(EXIT_FAILURE);
                }
                break;
            }

            case 'N': {
                if (skip_updater) {
                    *skip_updater = true;
                }
                break;
            }
            case 'D': {
                if (strstr(optarg, "uTox_updater")) {
                    // We're using the windows version of strstr() here
                    // because it's currently the only platform supported
                    // by the updater.
                    // TODO expose this as a function in updater.c
                    remove(optarg);
                }
                break;
            }

            case 0: {
                exit(EXIT_SUCCESS);
                break;
            }

            case 'S': {
                break;
            }

            case 'v': {
                break;
            }

            case 1: {
                break;
            }

            case 'h': {
                break;
            }

            case '?':{
                break;
            }
        }
    }
}

/** Does all of the init work for uTox across all platforms
 *
 * it's expect this will be called AFTER you parse argc/v and will act accordingly. */
void utox_init(void) {
    atexit(utox_raze);

    UTOX_SAVE *save = config_load();
    free(save);

    /* Called by the native main for every platform after loading utox setting,
     * before showing/drawing any windows. */
    if (settings.curr_version != settings.last_version) {
        settings.show_splash = true;
    }

    // We likely want to start this on every system.
    thread(updater_thread, (void*)1);
}

void utox_raze(void) {}
