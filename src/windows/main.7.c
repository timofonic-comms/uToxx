#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x600
#endif

#include "main.h"

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
    char *path = calloc(1, UTOX_FILE_NAME_LENGTH);
    if (!path){
        return;
    }

    FRIEND *f = get_friend(friend_number);
    if (!f) {
        return;
    }

    snprintf(path, UTOX_FILE_NAME_LENGTH, "%.*s.txt", (int)f->name_length, f->name);

    OPENFILENAME ofn = {
        .lStructSize = sizeof(OPENFILENAME),
        .lpstrFilter = ".txt",
        .lpstrFile   = path,
        .nMaxFile    = UTOX_FILE_NAME_LENGTH,
        .Flags       = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT,
        .lpstrDefExt = "txt",
    };

    if (GetSaveFileName(&ofn)) {
        FILE *file = fopen(path, "wb");
        if (file) {
            utox_export_chatlog(f->id_str, file);
        }
    }
}

void native_select_dir_ft(uint32_t fid, uint32_t num, FILE_TRANSFER *file) {
    char *path = calloc(1, UTOX_FILE_NAME_LENGTH);
    if (!path) {
        return;
    }

    memcpy(path, file->name, file->name_length);

    OPENFILENAME ofn = {
        .lStructSize = sizeof(OPENFILENAME),
        .lpstrFile   = path,
        .nMaxFile    = UTOX_FILE_NAME_LENGTH,
        .Flags       = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT,
    };

    if (GetSaveFileName(&ofn)) {
        postmessage_toxcore(TOX_FILE_ACCEPT, fid, num, path);
    }
}

void native_autoselect_dir_ft(uint32_t fid, FILE_TRANSFER *file) {
    wchar_t *autoaccept_folder = NULL;

    if (settings.portable_mode) {
        autoaccept_folder = calloc(1, UTOX_FILE_NAME_LENGTH * sizeof(wchar_t));

        // Convert the portable_mode_save_path into a wide string.
        wchar_t tmp[UTOX_FILE_NAME_LENGTH];
        mbstowcs(tmp, portable_mode_save_path, strlen(portable_mode_save_path));

        swprintf(autoaccept_folder, UTOX_FILE_NAME_LENGTH, L"%ls", tmp);
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

    wchar_t filename[UTOX_FILE_NAME_LENGTH] = { 0 };
    MultiByteToWideChar(CP_UTF8, 0, (char *)file->name, file->name_length, filename, UTOX_FILE_NAME_LENGTH);

    wchar_t fullpath[UTOX_FILE_NAME_LENGTH] = { 0 };
    swprintf(fullpath, UTOX_FILE_NAME_LENGTH, L"%ls\\%ls", subpath, filename);


    FILE *f = _fdopen(_open_osfhandle((intptr_t)CreateFileW(fullpath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL),
                                      0),
                      "wb");

    if (f) {
        postmessage_toxcore(TOX_FILE_ACCEPT_AUTO, fid, file->file_number, f);
    }
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

            // 2 bytes per wchar_t
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
