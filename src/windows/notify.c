#include "notify.h"

#include "main.h"
#include "utf8.h"
#include "window.h"

#include "../macros.h"
#include "../self.h"
#include "../ui.h"

#include "../native/notify.h"
#include "../native/window.h"

#include <windowsx.h>

bool have_focus = false;

/** Creates a tray baloon popup with the message, and flashes the main window
 *
 * accepts: char *title, title length, char *msg, msg length;
 * returns void;
 */
void notify(char *title, uint16_t title_length, const char *msg, uint16_t msg_length, void *UNUSED(object), bool UNUSED(is_group)) {
    if (have_focus || self.status == 2) {
        return;
    }

    FlashWindow(main_window.window, true);
    flashing = true;

    NOTIFYICONDATAW nid = {
        .cbSize      = sizeof(nid),
        .hWnd        = main_window.window,
        .uFlags      = NIF_ICON | NIF_INFO,
        .hIcon       = unread_messages_icon,
        .uTimeout    = 5000,
        .dwInfoFlags = 0,
    };

    utf8tonative(title, nid.szInfoTitle, title_length > sizeof(nid.szInfoTitle) / sizeof(*nid.szInfoTitle) - 1 ?
                                             sizeof(nid.szInfoTitle) / sizeof(*nid.szInfoTitle) - 1 :
                                             title_length);
    utf8tonative(msg, nid.szInfo, msg_length > sizeof(nid.szInfo) / sizeof(*nid.szInfo) - 1 ?
                                      sizeof(nid.szInfo) / sizeof(*nid.szInfo) - 1 :
                                      msg_length);

    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void redraw_notify(UTOX_WINDOW *win) {
        native_window_set_target(win);
    panel_draw(win->_.panel, 0, 0, win->_.w, win->_.h);
    SelectObject(win->draw_DC, win->draw_BM);
    BitBlt(win->window_DC, win->_.x, win->_.y, win->_.w, win->_.h, win->draw_DC, win->_.x, win->_.y, SRCCOPY);
}

LRESULT CALLBACK notify_msg_sys(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_QUIT:
        case WM_CLOSE:
        case WM_DESTROY: {
            break;
        }
        case WM_GETMINMAXINFO: {
            POINT min = { SCALE(200), SCALE(200) };
            ((MINMAXINFO *)lParam)->ptMinTrackSize = min;
            break;
        }

        case WM_CREATE: {
            UTOX_WINDOW *win = native_window_find_notify(&window);
            if (win) {
                win->window_DC = GetDC(window);
                win->draw_DC   = CreateCompatibleDC(win->window_DC);
                win->mem_DC    = CreateCompatibleDC(win->draw_DC);
                return false;
            }
            break;
        }

        case WM_SIZE: {
            int w, h;

            w = GET_X_LPARAM(lParam);
            h = GET_Y_LPARAM(lParam);

            if (w != 0) {
                RECT r;
                GetClientRect(window, &r);
                w = r.right;
                h = r.bottom;

                UTOX_WINDOW *win = native_window_find_notify(&window);
                if (win) {
                    if (win->draw_BM) {
                        DeleteObject(win->draw_BM);
                    }
                    win->draw_BM = CreateCompatibleBitmap(win->window_DC, w, h);
                    redraw_notify(win);
                }
            }
            break;
        }

        case WM_ERASEBKGND: {
            UTOX_WINDOW *win = native_window_find_notify(&window);
            redraw_notify(win);
            return true;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(window, &ps);

            UTOX_WINDOW *win = native_window_find_notify(&window);
            RECT r = ps.rcPaint;
            BitBlt(win->window_DC, r.left, r.top, r.right - r.left, r.bottom - r.top,
                   win->draw_DC, r.left, r.top, SRCCOPY);

            EndPaint(window, &ps);
            return false;
        }

        case WM_MOUSEMOVE: {
            return false;
        }

        case WM_LBUTTONDOWN: {
            break;
        }
        case WM_LBUTTONUP: {
            ReleaseCapture();
            UTOX_WINDOW *win = native_window_find_notify(&window);
            redraw_notify(win);
            break;
        }
        case WM_LBUTTONDBLCLK: {
            DestroyWindow(window);
            break;
        }

        case WM_RBUTTONDOWN: {
            break;
        }

        case WM_RBUTTONUP: {
            break;
        }

    }

    return DefWindowProcW(window, msg, wParam, lParam);
}

static HINSTANCE current_instance = NULL;
void native_notify_init(HINSTANCE instance) {
    current_instance = instance;
}
