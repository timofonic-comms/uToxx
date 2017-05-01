#include "updater.h"

#include "branding.h"
#include "macros.h"
#include "settings.h"

#include "main.h" // File name length

#include "native/thread.h"
#include "native/filesys.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sodium.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#if defined __WIN32__
#define UPDATER_HOST "win"
#define UPDATER_OUT "uTox.exe"
#elif defined __OBJC__
#define UPDATER_HOST "osx"
#define UPDATER_OUT "uTox.dmg"
#else
#define UPDATER_HOST "unx"
#define UPDATER_OUT "utox"
#endif


#if (defined __WIN64__ || defined _WIN64 || defined __x86_64__ || defined __ppc64__ )
#define UPDATER_ARCH 64u
#else
#define UPDATER_ARCH 32u
#endif

// Stable versions we plan to ship to users.
#define UPDATER_VERSION_STABLE_URI "utox_stable_" UPDATER_HOST
#define UPDATER_VERSION_DEVEL_URI "utox_develop_" UPDATER_HOST

// Small rollout versions we can use to find issues before
// shipping to ~all users
#define UPDATER_VERSION_CANARY_URI "utox_canary_" UPDATER_HOST

// Direct rollouts
#define UPDATER_VERSION_TESTING_URI "utox_testing_" UPDATER_HOST


static const uint8_t pk[crypto_sign_ed25519_PUBLICKEYBYTES] = {
    0x64, 0x3B, 0xF6, 0xEF, 0x40, 0xAF, 0x61, 0x94,
    0x79, 0x64, 0xDD, 0x41, 0x3D, 0x41, 0xC7, 0x3C,
    0xDE, 0xA3, 0x66, 0xD1, 0x7E, 0x3C, 0x6C, 0x49,
    0x1D, 0xD4, 0x8F, 0x8F, 0x4B, 0xFD, 0xFF, 0xC8
};

static size_t mk_request(char *host, char *file, char *data) {
    return snprintf(data, 1024, "GET /%s HTTP/1.0\r\n""Host: %s\r\n\r\n", file, host);
}

// I don't like it either, but windows is a dirty dirty liar!
#if defined(_WIN32) || defined(__WIN32__)
#define SOCKET_SUCKS(s) ((uint64_t)s == INVALID_SOCKET)
#define INIT_SOCKETS() do { \
        WSADATA wsaData; \
        WSAStartup(MAKEWORD(2, 2), &wsaData); \
    } while(0)
#else
#define SOCKET_SUCKS(s) (s < 0)
#define INIT_SOCKETS() // lol windows :D
#endif


static uint8_t *download(char *host, char *file, size_t *out_len) {
    if (settings.force_proxy) {
        return NULL;
    }

    struct addrinfo *root;

    if (getaddrinfo(host, "80", NULL, &root)) {
        return NULL;
    }

    for (struct addrinfo *info = root; info; info = info->ai_next) {
        if (info->ai_socktype && info->ai_socktype != SOCK_STREAM) {
            continue;
        }

        int64_t sock = socket(info->ai_family, SOCK_STREAM, IPPROTO_TCP);
        if (SOCKET_SUCKS(sock)) {
            continue;
        }

        if (connect(sock, info->ai_addr, info->ai_addrlen)) {
                        close(sock);
            continue;
        }

        char reqst[1024] = {0}; // 1024 aught to be enough for anyone!
        size_t size = mk_request(host, file, reqst);
        if (size >= 1024) {
            close(sock);
            freeaddrinfo(root);
            return NULL;
        }

        if (send(sock, reqst, size, 0) != (ssize_t)size) {
            close(sock);
            continue;
        }
        freeaddrinfo(root);

        uint8_t *data = NULL;

        ssize_t len = 0;
        uint32_t real_len = 0, header_len = 0;
        uint8_t *buffer = calloc(1, 0x10000);
        if (!buffer) {
            close(sock);
            return NULL;
        }

        bool have_header = false;
        while ((len = recv(sock, (char *)buffer, 0xffff, 0)) > 0) {
            if (!have_header) {
                buffer[len] = 0; // Buffer must be null term
                // Fail with 404
                if (strstr((char *)buffer, "404 Not Found\r\n")) {
                    break;
                }
                // Get the real file length
                char *str = strstr((char*)buffer, "Content-Length: ");
                if (!str) {
                    break;
                }

                /* parse the length field */
                str += sizeof("Content-Length: ") - 1;
                header_len = strtoul(str, NULL, 10);
                if (header_len > 100 * 1024 * 1024) {
                    close(sock);
                    free(buffer);
                    return NULL;
                }

                /* find the end of the http response header */
                str = strstr(str, "\r\n\r\n");
                if (!str) {
                    break;
                }
                str += sizeof("\r\n\r\n") - 1; // and trim

                /* allocate buffer to read into) */
                data = calloc(header_len, 1);
                if (!data) {
                    break;
                }


                /* read the first piece */
                real_len = len - (str - (char*)buffer);
                memcpy(data, str, real_len);

                have_header = true;
                continue;
            }

            if (real_len + len > header_len) {
                close(sock);
                free(buffer);
                free(data);
                return NULL;
            }

            memcpy(data + real_len, buffer, len);
            real_len += len;
        }
        close(sock);
        free(buffer);

        if (have_header && data && real_len) {
            if (out_len) {
                *out_len = real_len;
            }

            return data;
        }

        return NULL;
    }

    freeaddrinfo(root);
    return NULL;
}

static uint8_t *verify_sig(uint8_t *raw, uint32_t len, size_t *out_len) {
    uint8_t *message = calloc(1, len);
    if (!message) {
        return NULL;
    }

    size_t m_len = 0;
    if (crypto_sign_ed25519_open(message, (unsigned long long*)&m_len, raw, len, pk) == -1) {
        free(message);
        return NULL;
    }

    if (m_len) {
        if (out_len) {
            *out_len = m_len;
        }
        return message;
    }

    return NULL;
}

static uint32_t download_version(void) {
    size_t len = 0;
    uint8_t *raw = download("downloads.utox.io", UPDATER_VERSION_STABLE_URI, &len);
    if (!raw) {
        return 0;
    }

    size_t msg_len = 0;
    uint8_t *data = verify_sig(raw, len, &msg_len);
    free(raw);
    if (!data) {
        return 0;
    }


    if (msg_len < 8) {
        free(data);
        return 0;
    }

    uint32_t v;
    memcpy(&v, data + 4, sizeof(v));
    uint32_t version = ntohl(v);

    free(data);
    return version;
}

uint32_t updater_check(uint64_t UNUSED(v)) {
    return download_version();
}

#ifdef ENABLE_AUTOUPDATE
void updater_thread(void *from_startup) {
    static bool updater_running = false;

    if (from_startup) {
        // always start the updater thread if started during init
        updater_running = true;
        INIT_SOCKETS();
    } else if (updater_running) {
        // not called by startup, so we're already running
        return;
    } else {
        // cool, thanks for re enabling updates :D
        updater_running = true;
    }

    char pwd[UTOX_FILE_NAME_LENGTH];
    getcwd(pwd, sizeof pwd);

    while (updater_running) {
        if (!settings.auto_update) {
            updater_running = false;
            return;
        }

        static uint32_t version;
        yieldcpu(1000); // We want to delay a second before pulling the download
                        // to make sure we're not currently mid update
        version = updater_check(UTOX_VERSION_NUMBER);
        if (version > UTOX_VERSION_NUMBER) {

            char str[100];
            snprintf(str, 100, "%.3s_%u-%u.%u.%u", UPDATER_HOST, UPDATER_ARCH,
                     (version & 0xFF0000) >> 16, (version & 0xFF00) >> 8, (version & 0xFF));

            char name[UTOX_FILE_NAME_LENGTH];
            snprintf(name, UTOX_FILE_NAME_LENGTH, "%s/next_%s", pwd, UPDATER_OUT);
            FILE *file = fopen(name, "rb");
            if (file) {
                fclose(file);
                return;
            }

            file = fopen(name, "wb");
            if (!file) {
                return;
            }

            size_t raw_size = 0;
            uint8_t *raw = download("downloads.utox.io", str, &raw_size);

            size_t data_size = 0;
            uint8_t *data = verify_sig(raw, raw_size, &data_size);
            free(raw);

            if (!data) {
                fclose(file);
                return;
            }

            // The default updater also adds a timestamp to the signature (the +4)
            // I'm not sure I want to change how signatures are done yet, so I'm just
            // gonna leave this hack here for now... -- grayhatter
            fwrite(data + 4, data_size - 4, 1, file);
            fclose(file);
            free(data);
            return;
        }

        yieldcpu(1000 * 60 * 5);
    }
}
#else
void updater_thread(void *from_startup)
{
    (void)from_startup;
}
#endif

void updater_start(bool from_startup) {
    thread(updater_thread, (void *)from_startup);
}
