#include "notify.h"

#include "main.h"
#include "window.h"

#include "../ui.h"

#include "../native/window.h"

#include <windowsx.h>

static void redraw_notify(UTOX_WINDOW *win) {
        native_window_set_target(win);
    panel_draw(win->_.panel, 0, 0, win->_.w, win->_.h);
    SelectObject(win->draw_DC, win->draw_BM);
    BitBlt(win->window_DC, win->_.x, win->_.y, win->_.w, win->_.h, win->draw_DC, win->_.x, win->_.y, SRCCOPY);
}

LRESULT CALLBACK notify_msg_sys(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    UTOX_WINDOW *win = native_window_find_notify(&window);

    static int mdown_x, mdown_y;
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
            redraw_notify(win);
            return true;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;

            BeginPaint(window, &ps);

            RECT r = ps.rcPaint;
            BitBlt(win->window_DC, r.left, r.top, r.right - r.left, r.bottom - r.top, win->draw_DC, r.left, r.top, SRCCOPY);

            EndPaint(window, &ps);
            return false;
        }

        case WM_MOUSEMOVE: {
            return false;
        }

        case WM_LBUTTONDOWN: {
            mdown_x = GET_X_LPARAM(lParam);
            mdown_y = GET_Y_LPARAM(lParam);
            break;
        }
        case WM_LBUTTONUP: {
            ReleaseCapture();
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
