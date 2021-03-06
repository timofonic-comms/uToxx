#include "../../src/filesys.h"
#include "../../src/native/filesys.h"

#include "../../src/filesys.c"
#include "../../src/posix/filesys.c"

// TODO copied from xlib/filesys.c might be possible to keep this DRY at some point
bool native_remove_file(const uint8_t *name, size_t length, bool portable_mode) {
    char path[UTOX_FILE_NAME_LENGTH] = { 0 };

    // TODO this is duplicated in more methods, make this portable thing a common method in filesys.c
    if (portable_mode) {
        snprintf((char *)path, UTOX_FILE_NAME_LENGTH, "./tox/");
    } else {
        snprintf((char *)path, UTOX_FILE_NAME_LENGTH, "%s/.config/tox/", getenv("HOME"));
    }

    if (strlen((const char *)path) + length >= UTOX_FILE_NAME_LENGTH) {
        return false;
    }
    snprintf((char *)path + strlen((const char *)path), UTOX_FILE_NAME_LENGTH - strlen((const char *)path), "%.*s",
             (int)length, (char *)name);

    if (remove((const char *)path)) {
        return false;
    }

    return true;
}
