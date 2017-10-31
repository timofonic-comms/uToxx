#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x600
#endif

#include "main.h"

#include "utf8.h"

#include "../chatlog.h"
#include "../file_transfers.h"
#include "../filesys.h"
#include "../friend.h"
#include "../main.h"
#include "../settings.h"
#include "../tox.h"

#include <io.h>
#include <shlobj.h>

void native_export_chatlog_init(uint32_t friend_number) {
    FRIEND *f = get_friend(friend_number);
    if (!f) {
        return;
    }

    char *path = calloc(1, UTOX_FILE_NAME_LENGTH);
    snprintf(path, UTOX_FILE_NAME_LENGTH, "%.*s.txt", (int)f->name_length, f->name);

    wchar_t filepath[UTOX_FILE_NAME_LENGTH] = { 0 };
    utf8_to_nativestr(path, filepath, UTOX_FILE_NAME_LENGTH * 2);

    OPENFILENAMEW ofn = {
        .lStructSize = sizeof(OPENFILENAMEW),
        .lpstrFilter = L".txt",
        .lpstrFile   = filepath,
        .nMaxFile    = UTOX_FILE_NAME_LENGTH,
        .Flags       = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT,
        .lpstrDefExt = L"txt",
    };

    if (GetSaveFileNameW(&ofn)) {
        path = calloc(1, UTOX_FILE_NAME_LENGTH);
        native_to_utf8str(filepath, path, UTOX_FILE_NAME_LENGTH);

        FILE *file = utox_get_file_simple(path, UTOX_FILE_OPTS_WRITE);
        if (file) {
            utox_export_chatlog(f->id_str, file);
        }
    }
    free(path);
}

void native_select_dir_ft(uint32_t fid, uint32_t num, FILE_TRANSFER *file) {
    if (!sanitize_filename(file->name)) {
        return;
    }

    wchar_t filepath[UTOX_FILE_NAME_LENGTH] = { 0 };
    utf8_to_nativestr((char *)file->name, filepath, file->name_length * 2);

    OPENFILENAMEW ofn = {
        .lStructSize = sizeof(OPENFILENAMEW),
        .lpstrFile   = filepath,
        .nMaxFile    = UTOX_FILE_NAME_LENGTH,
        .Flags       = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT,
    };

    if (GetSaveFileNameW(&ofn)) {
        char *path = calloc(1, UTOX_FILE_NAME_LENGTH);
        native_to_utf8str(filepath, path, UTOX_FILE_NAME_LENGTH * 2);
        postmessage_toxcore(TOX_FILE_ACCEPT, fid, num, path);
    }
}

void native_autoselect_dir_ft(uint32_t fid, FILE_TRANSFER *file) {
    wchar_t *autoaccept_folder = NULL;

    if (settings.portable_mode) {
        autoaccept_folder = calloc(1, UTOX_FILE_NAME_LENGTH * sizeof(wchar_t));
        utf8_to_nativestr(portable_mode_save_path, autoaccept_folder, strlen(portable_mode_save_path) * 2);
    } else if (SHGetKnownFolderPath((REFKNOWNFOLDERID)&FOLDERID_Downloads,
                                    KF_FLAG_CREATE, NULL, &autoaccept_folder) != S_OK)
    {
        return;
    }

    wchar_t subpath[UTOX_FILE_NAME_LENGTH] = { 0 };
    swprintf(subpath, UTOX_FILE_NAME_LENGTH, L"%ls%ls", autoaccept_folder, L"\\Tox_Auto_Accept");

    if (settings.portable_mode) {
        free(autoaccept_folder);
    } else {
        CoTaskMemFree(autoaccept_folder);
    }

    CreateDirectoryW(subpath, NULL);

    if (!sanitize_filename(file->name)) {
        return;
    }

    wchar_t filename[UTOX_FILE_NAME_LENGTH] = { 0 };
    utf8_to_nativestr((char *)file->name, filename, file->name_length * 2);

    wchar_t fullpath[UTOX_FILE_NAME_LENGTH] = { 0 };
    swprintf(fullpath, UTOX_FILE_NAME_LENGTH, L"%ls\\%ls", subpath, filename);

    char *path = calloc(1, UTOX_FILE_NAME_LENGTH);
    native_to_utf8str(fullpath, path, UTOX_FILE_NAME_LENGTH);
    postmessage_toxcore(TOX_FILE_ACCEPT_AUTO, fid, file->file_number, path);
}

void launch_at_startup(bool should) {
    const wchar_t *run_key_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (should) {
        HKEY hKey;
        if (RegOpenKeyW(HKEY_CURRENT_USER, run_key_path, &hKey) == ERROR_SUCCESS) {
            wchar_t path[UTOX_FILE_NAME_LENGTH * 2];
            uint16_t path_length  = GetModuleFileNameW(NULL, path + 1, UTOX_FILE_NAME_LENGTH * 2);
            path[0]               = '\"';
            path[path_length + 1] = '\"';
            path[path_length + 2] = '\0';
            path_length += 2;

            RegSetKeyValueW(hKey, NULL, L"uTox", REG_SZ, path, path_length * 2);
            RegCloseKey(hKey);
        }
    } else {
        HKEY hKey;
        if (RegOpenKeyW(HKEY_CURRENT_USER, run_key_path, &hKey) == ERROR_SUCCESS) {
            RegDeleteKeyValueW(hKey, NULL, L"uTox");
            RegCloseKey(hKey);
        }
    }
}
