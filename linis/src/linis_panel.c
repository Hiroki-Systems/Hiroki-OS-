/*
 * linis_panel.c — GTK3 + Cairo tabanlı panel (Tokyo Night tema)
 *
 * Sol dikey dock: workspace göstergeleri, uygulama ikonları
 * Alt yatay bar: tasklist, saat, sistem tepsi ikonları
 * EWMH üzerinden linis WM ile iletişim
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo/cairo.h>

#include "linis_config.h"
#include "linis_ewmh.h"
#include "linis_util.h"

/* ── Tokyo Night palette ── */
#define TN_BG       0x1a1b26
#define TN_BG_DARK  0x16161e
#define TN_FG       0xc0caf5
#define TN_FG_DIM   0x565f89
#define TN_ACCENT   0x7aa2f7
#define TN_GREEN    0x9ece6a
#define TN_RED      0xf7768e
#define TN_YELLOW   0xe0af68
#define TN_PURPLE   0xbb9af7
#define TN_CYAN     0x7dcfff
#define TN_ORANGE   0xff9e64
#define TN_PANEL_H  32
#define TN_DOCK_W   48

typedef struct {
    Display *dpy;
    int      scr;
    Window   root;
    Atom     net[NET_LAST];

    LConfig *cfg;

    /* durum */
    int      curws;
    int      ws_n;
    int      ntasks;
    Window  *tasks;
    char   **tnames;
    int      active_win;

    /* GTK */
    GtkWidget *dock_window;
    GtkWidget *bar_window;
    GtkWidget *dock_drawing;
    GtkWidget *bar_drawing;
    guint      refresh_timer;
} Panel;

static Panel g_panel;

/* ── X11 yardımcıları ── */

static unsigned long tn_color(uint32_t rgb) {
    unsigned long r = (rgb >> 16) & 0xff;
    unsigned long g = (rgb >> 8) & 0xff;
    unsigned long b = rgb & 0xff;
    return (r << 16) | (g << 8) | b;
}

static Atom xatom(const char *name) {
    return XInternAtom(g_panel.dpy, name, False);
}

static long root_cardinal(Atom a) {
    long v = 0;
    ewmh_get_cardinal(g_panel.dpy, g_panel.root, a, &v);
    return v;
}

static int win_desktop(Window w) {
    Atom prop = xatom("_NET_WM_DESKTOP");
    Atom type = None; int fmt = 0;
    unsigned long n = 0, after = 0;
    unsigned char *data = NULL;
    int out = -1;
    if (XGetWindowProperty(g_panel.dpy, w, prop, 0, 1, False, XA_CARDINAL,
                           &type, &fmt, &n, &after, &data) == Success && data && n >= 1)
        out = (int)((long *)data)[0];
    if (data) XFree(data);
    return out;
}

static int win_hidden(Window w) {
    Atom prop = xatom("_NET_WM_STATE_HIDDEN");
    Atom t = None; int f = 0;
    unsigned long n = 0, after = 0;
    unsigned char *data = NULL;
    int hid = 0;
    if (XGetWindowProperty(g_panel.dpy, w, xatom("_NET_WM_STATE"), 0, 64, False,
                           XA_ATOM, &t, &f, &n, &after, &data) == Success && data) {
        Atom *as = (Atom *)data;
        for (unsigned long i = 0; i < n; i++)
            if (as[i] == prop) hid = 1;
    }
    if (data) XFree(data);
    return hid;
}

static void refresh_tasks(void) {
    Window *list = NULL;
    int n = ewmh_get_client_list(g_panel.dpy, g_panel.root,
                                 g_panel.net[NET_CLIENT_LIST], &list);
    if (g_panel.tasks) free(g_panel.tasks);
    if (g_panel.tnames) {
        for (int i = 0; i < g_panel.ntasks; i++)
            free(g_panel.tnames[i]);
        free(g_panel.tnames);
    }
    g_panel.tasks = list;
    g_panel.tnames = xcalloc((size_t)LMAX(n, 1), sizeof(char *));
    g_panel.ntasks = n;
    for (int i = 0; i < n; i++) {
        char *nm = ewmh_get_name(g_panel.dpy, list[i]);
        g_panel.tnames[i] = nm ? nm : xstrdup("");
    }
    g_panel.curws = (int)root_cardinal(g_panel.net[NET_CURRENT_DESKTOP]);
    g_panel.ws_n = (int)root_cardinal(g_panel.net[NET_NUMBER_OF_DESKTOPS]);
    if (g_panel.ws_n < 1) g_panel.ws_n = 4;
    g_panel.active_win = (int)root_cardinal(g_panel.net[NET_ACTIVE_WINDOW]);
}

/* ── Cairo çizim yardımcıları ── */

static void cairo_set_color(cairo_t *cr, uint32_t rgb, double alpha) {
    double r = ((rgb >> 16) & 0xff) / 255.0;
    double g = ((rgb >> 8) & 0xff) / 255.0;
    double b = (rgb & 0xff) / 255.0;
    cairo_set_source_rgba(cr, r, g, b, alpha);
}

static void cairo_rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI_2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, G_PI_2);
    cairo_arc(cr, x + r, y + h - r, r, G_PI_2, G_PI);
    cairo_arc(cr, x + r, y + r, r, G_PI, 3 * G_PI_2);
    cairo_close_path(cr);
}

/* ── Sol dock çizimi ── */

static gboolean on_dock_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    /* Arka plan */
    cairo_set_color(cr, TN_BG_DARK, 0.92);
    cairo_paint(cr);

    /* Ayırıcı çizgi (sağ kenar) */
    cairo_set_color(cr, TN_FG_DIM, 0.3);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, w - 0.5, 0);
    cairo_line_to(cr, w - 0.5, h);
    cairo_stroke(cr);

    /* ── Workspace göstergeleri ── */
    int ws_area_h = g_panel.ws_n * 36 + 16;
    int y = 12;

    for (int i = 0; i < g_panel.ws_n; i++) {
        int active = (i == g_panel.curws);
        double cx = w / 2.0;
        double cy = y + 14;

        if (active) {
            /* Aktif workspace: yuvarlak accent */
            cairo_set_color(cr, TN_ACCENT, 0.25);
            cairo_rounded_rect(cr, 8, y, w - 16, 28, 6);
            cairo_fill(cr);

            cairo_set_color(cr, TN_ACCENT, 1.0);
            cairo_rounded_rect(cr, 8, y, w - 16, 28, 6);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
        }

        /* Workspace numarası */
        cairo_text_extents_t ext;
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, active ? 13 : 11);
        cairo_text_extents(cr, num, &ext);
        cairo_set_color(cr, active ? TN_FG : TN_FG_DIM, active ? 1.0 : 0.6);
        cairo_move_to(cr, cx - ext.width / 2, cy + ext.height / 2 - 1);
        cairo_show_text(cr, num);

        /* Bu workspace'teki pencere sayısı */
        int count = 0;
        for (int j = 0; j < g_panel.ntasks; j++) {
            if (win_desktop(g_panel.tasks[j]) == i && !win_hidden(g_panel.tasks[j]))
                count++;
        }
        if (count > 0) {
            char cnt[8];
            snprintf(cnt, sizeof(cnt), "%d", count);
            cairo_set_font_size(cr, 9);
            cairo_text_extents(cr, cnt, &ext);
            cairo_set_color(cr, TN_FG_DIM, 0.5);
            cairo_move_to(cr, w - 12 - ext.width, cy + ext.height / 2 - 1);
            cairo_show_text(cr, cnt);
        }

        y += 36;
    }

    /* ── Uygulama ikonları (dock alt kısım) ── */
    int icon_area_top = ws_area_h + 24;
    int icon_size = 32;
    int icon_pad = 8;
    int icons_per_col = (h - icon_area_top - 16) / (icon_size + icon_pad);

    /* Aktif pencereleri göster */
    int drawn = 0;
    for (int i = 0; i < g_panel.ntasks && drawn < icons_per_col; i++) {
        if (win_hidden(g_panel.tasks[i])) continue;

        int ix = (w - icon_size) / 2;
        int iy = icon_area_top + drawn * (icon_size + icon_pad);
        int is_active = (g_panel.tasks[i] == (Window)g_panel.active_win);

        if (is_active) {
            cairo_set_color(cr, TN_ACCENT, 0.2);
            cairo_rounded_rect(cr, ix - 2, iy - 2, icon_size + 4, icon_size + 4, 6);
            cairo_fill(cr);
        }

        /* Basit renkli kare ikon (WM class rengine göre) */
        char *cls = ewmh_get_class(g_panel.dpy, g_panel.tasks[i]);
        uint32_t icon_color = TN_FG_DIM;
        if (cls) {
            if (strstr(cls, "firefox") || strstr(cls, "chrom")) icon_color = TN_ORANGE;
            else if (strstr(cls, "alacritty") || strstr(cls, "kitty") || strstr(cls, "terminal")) icon_color = TN_GREEN;
            else if (strstr(cls, "pcmanfm") || strstr(cls, "thunar") || strstr(cls, "nautilus")) icon_color = TN_YELLOW;
            else if (strstr(cls, "code") || strstr(cls, "vim")) icon_color = TN_PURPLE;
            else if (strstr(cls, "vlc") || strstr(cls, "mpv")) icon_color = TN_RED;
            else icon_color = TN_CYAN;
            free(cls);
        }

        /* Yuvarlak ikon arka planı */
        cairo_set_color(cr, icon_color, 0.2);
        cairo_arc(cr, ix + icon_size / 2.0, iy + icon_size / 2.0, icon_size / 2.0, 0, 2 * G_PI);
        cairo_fill(cr);

        /* İlk harf */
        if (g_panel.tnames[i] && g_panel.tnames[i][0]) {
            char letter[2] = { g_panel.tnames[i][0], 0 };
            /* Büyük harfe çevir */
            if (letter[0] >= 'a' && letter[0] <= 'z') letter[0] -= 32;
            cairo_text_extents_t ext2;
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 14);
            cairo_text_extents(cr, letter, &ext2);
            cairo_set_color(cr, icon_color, 1.0);
            cairo_move_to(cr, ix + icon_size / 2.0 - ext2.width / 2,
                          iy + icon_size / 2.0 + ext2.height / 2 - 1);
            cairo_show_text(cr, letter);
        }

        drawn++;
    }

    /* ── Logo (alt kısım) ── */
    cairo_set_color(cr, TN_ACCENT, 0.8);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, "L", &ext);
    cairo_move_to(cr, w / 2.0 - ext.width / 2, h - 20);
    cairo_show_text(cr, "L");

    return TRUE;
}

/* ── Alt bar çizimi ── */

static gboolean on_bar_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    /* Arka plan */
    cairo_set_color(cr, TN_BG_DARK, 0.95);
    cairo_paint(cr);

    /* Üst ayırıcı */
    cairo_set_color(cr, TN_FG_DIM, 0.2);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, 0.5);
    cairo_line_to(cr, w, 0.5);
    cairo_stroke(cr);

    /* ── Sol: workspace indicator (yatay) ── */
    int x = 12;
    for (int i = 0; i < g_panel.ws_n; i++) {
        int active = (i == g_panel.curws);
        int dot_r = active ? 5 : 3;
        double cy = h / 2.0;

        if (active) {
            cairo_set_color(cr, TN_ACCENT, 0.3);
            cairo_arc(cr, x + 8, cy, dot_r + 3, 0, 2 * G_PI);
            cairo_fill(cr);
        }
        cairo_set_color(cr, active ? TN_ACCENT : TN_FG_DIM, active ? 1.0 : 0.5);
        cairo_arc(cr, x + 8, cy, dot_r, 0, 2 * G_PI);
        cairo_fill(cr);

        x += 24;
    }

    /* ── Orta: aktif pencere başlığı ── */
    if (g_panel.active_win && g_panel.active_win != g_panel.root) {
        char *name = ewmh_get_name(g_panel.dpy, (Window)g_panel.active_win);
        if (name && *name) {
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 12);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, name, &ext);

            double max_w = w * 0.5;
            if (ext.width > max_w) {
                /* Kısalt */
                char *trunc = g_strdup_printf("%.30s…", name);
                free(name);
                name = trunc;
                cairo_text_extents(cr, name, &ext);
            }

            cairo_set_color(cr, TN_FG, 0.9);
            cairo_move_to(cr, w / 2.0 - ext.width / 2, h / 2.0 + ext.height / 2 - 2);
            cairo_show_text(cr, name);
            free(name);
        }
    }

    /* ── Sağ: saat ── */
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%H:%M", tm);

    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, timebuf, &ext);
    cairo_set_color(cr, TN_FG, 0.95);
    cairo_move_to(cr, w - 12 - ext.width, h / 2.0 + ext.height / 2 - 2);
    cairo_show_text(cr, timebuf);

    /* Tarih */
    char datebuf[32];
    strftime(datebuf, sizeof(datebuf), "%d %b", tm);
    cairo_set_font_size(cr, 10);
    cairo_text_extents(cr, datebuf, &ext);
    cairo_set_color(cr, TN_FG_DIM, 0.6);
    cairo_move_to(cr, w - 12 - ext.width, h / 2.0 + ext.height / 2 - 14);
    cairo_show_text(cr, datebuf);

    return TRUE;
}

/* ── Tıklama olayları ── */

static gboolean on_dock_button_press(GtkWidget *widget, GdkEventButton *ev, gpointer data) {
    (void)data;
    int w = gtk_widget_get_allocated_width(widget);

    if (ev->button != 1) return FALSE;

    /* Workspace tıklaması */
    int ws_area_h = g_panel.ws_n * 36 + 16;
    if (ev->y < ws_area_h) {
        int clicked_ws = (int)(ev->y - 12) / 36;
        if (clicked_ws >= 0 && clicked_ws < g_panel.ws_n) {
            /* WM'e workspace değiştirme mesajı gönder */
            XEvent xev;
            memset(&xev, 0, sizeof(xev));
            xev.xclient.type = ClientMessage;
            xev.xclient.window = g_panel.root;
            xev.xclient.message_type = xatom("_NET_CURRENT_DESKTOP");
            xev.xclient.format = 32;
            xev.xclient.data.l[0] = clicked_ws;
            xev.xclient.data.l[1] = 1; /* Source indication: normal app */
            XSendEvent(g_panel.dpy, g_panel.root, False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &xev);
            XFlush(g_panel.dpy);
        }
        return TRUE;
    }

    /* Uygulama ikonu tıklaması */
    int icon_area_top = ws_area_h + 24;
    int icon_size = 32;
    int icon_pad = 8;
    if (ev->y >= icon_area_top) {
        int idx = (int)((ev->y - icon_area_top) / (icon_size + icon_pad));
        if (idx >= 0 && idx < g_panel.ntasks) {
            Window w_id = g_panel.tasks[idx];
            /* Pencereyi öne getir */
            XEvent xev;
            memset(&xev, 0, sizeof(xev));
            xev.xclient.type = ClientMessage;
            xev.xclient.window = w_id;
            xev.xclient.message_type = xatom("_NET_ACTIVE_WINDOW");
            xev.xclient.format = 32;
            xev.xclient.data.l[0] = 2; /* Source: pager/taskbar */
            xev.xclient.data.l[1] = CurrentTime;
            xev.xclient.data.l[2] = w_id;
            XSendEvent(g_panel.dpy, g_panel.root, False,
                       SubstructureRedirectMask | SubstructureNotifyMask, &xev);
            XFlush(g_panel.dpy);
        }
        return TRUE;
    }

    return FALSE;
}

/* ── Solik panel penceresi ── */

static void create_dock(void) {
    g_panel.dock_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_panel.dock_window), "linis-dock");
    gtk_window_set_default_size(GTK_WINDOW(g_panel.dock_window), TN_DOCK_W, 800);
    gtk_window_set_decorated(GTK_WINDOW(g_panel.dock_window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_panel.dock_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(g_panel.dock_window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(g_panel.dock_window), TRUE);
    gtk_widget_set_app_paintable(g_panel.dock_window, TRUE);

    /* EWMH: dock panel */
    GdkScreen *screen = gtk_widget_get_screen(g_panel.dock_window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    gtk_widget_set_visual(g_panel.dock_window, visual);

    /* Cairo draw */
    g_panel.dock_drawing = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(g_panel.dock_window), g_panel.dock_drawing);
    g_signal_connect(g_panel.dock_drawing, "draw", G_CALLBACK(on_dock_draw), NULL);
    g_signal_connect(g_panel.dock_window, "button-press-event", G_CALLBACK(on_dock_button_press), NULL);

    gtk_widget_show_all(g_panel.dock_window);

    /* Sol kenara yerleştir */
    GdkDisplay *gdk_dpy = gdk_display_get_default();
    int n_monitors = gdk_display_get_n_monitors(gdk_dpy);
    if (n_monitors > 0) {
        GdkMonitor *mon = gdk_display_get_monitor(gdk_dpy, 0);
        GdkRectangle geom;
        gdk_monitor_get_geometry(mon, &geom);
        gtk_window_move(GTK_WINDOW(g_panel.dock_window),
                        geom.x, geom.y);
    }

    /* WM_NESTED_DOCK Atomunu ayarla — WM bunu strut olarak kullanabilir */
    GdkWindow *gdk_win = gtk_widget_get_window(g_panel.dock_window);
    if (gdk_win) {
        Atom dock_atom = xatom("_NET_WM_WINDOW_TYPE_DOCK");
        gdk_x11_window_set_utf8_property(gdk_win, "_NET_WM_WINDOW_TYPE", "dock");
        /* Strut: sol 48px */
        long strut[12] = {0};
        strut[0] = TN_DOCK_W; /* left */
        strut[4] = TN_PANEL_H; /* top */
        Atom strut_atom = xatom("_NET_WM_STRUT_PARTIAL");
        XChangeProperty(g_panel.dpy, GDK_WINDOW_XID(gdk_win), strut_atom,
                        XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 12);
    }
}

/* ── Alt bar penceresi ── */

static void create_bar(void) {
    g_panel.bar_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_panel.bar_window), "linis-bar");
    gtk_window_set_default_size(GTK_WINDOW(g_panel.bar_window), 1920, TN_PANEL_H);
    gtk_window_set_decorated(GTK_WINDOW(g_panel.bar_window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_panel.bar_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(g_panel.bar_window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(g_panel.bar_window), TRUE);
    gtk_widget_set_app_paintable(g_panel.bar_window, TRUE);

    GdkScreen *screen = gtk_widget_get_screen(g_panel.bar_window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    gtk_widget_set_visual(g_panel.bar_window, visual);

    g_panel.bar_drawing = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(g_panel.bar_window), g_panel.bar_drawing);
    g_signal_connect(g_panel.bar_drawing, "draw", G_CALLBACK(on_bar_draw), NULL);

    gtk_widget_show_all(g_panel.bar_window);

    /* Alt kenara yerleştir — full genişlik */
    GdkDisplay *gdk_dpy = gdk_display_get_default();
    int n_monitors = gdk_display_get_n_monitors(gdk_dpy);
    if (n_monitors > 0) {
        GdkMonitor *mon = gdk_display_get_monitor(gdk_dpy, 0);
        GdkRectangle geom;
        gdk_monitor_get_geometry(mon, &geom);
        gtk_window_move(GTK_WINDOW(g_panel.bar_window),
                        geom.x, geom.y + geom.height - TN_PANEL_H);
        gtk_window_set_default_size(GTK_WINDOW(g_panel.bar_window),
                                    geom.width, TN_PANEL_H);
        gtk_widget_set_size_request(g_panel.bar_window, geom.width, TN_PANEL_H);
    }

    /* Strut: alt 32px */
    GdkWindow *gdk_win = gtk_widget_get_window(g_panel.bar_window);
    if (gdk_win) {
        long strut[12] = {0};
        strut[8] = TN_PANEL_H; /* bottom */
        Atom strut_atom = xatom("_NET_WM_STRUT_PARTIAL");
        XChangeProperty(g_panel.dpy, GDK_WINDOW_XID(gdk_win), strut_atom,
                        XA_CARDINAL, 32, PropModeReplace, (unsigned char *)strut, 12);
    }
}

/* ── Refresh timer ── */

static gboolean refresh_timer_cb(gpointer data) {
    (void)data;
    refresh_tasks();
    gtk_widget_queue_draw(g_panel.dock_drawing);
    gtk_widget_queue_draw(g_panel.bar_drawing);
    return TRUE;
}

/* ── X11 Property değişikliklerini dinle ── */

static GdkFilterReturn event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data) {
    (void)event; (void)data;
    XEvent *ev = (XEvent *)xevent;
    if (ev->type == PropertyNotify) {
        Atom changed = ev->xproperty.atom;
        if (changed == g_panel.net[NET_ACTIVE_WINDOW] ||
            changed == g_panel.net[NET_CURRENT_DESKTOP] ||
            changed == g_panel.net[NET_CLIENT_LIST] ||
            changed == g_panel.net[NET_NUMBER_OF_DESKTOPS]) {
            refresh_tasks();
            gtk_widget_queue_draw(g_panel.dock_drawing);
            gtk_widget_queue_draw(g_panel.bar_drawing);
        }
    }
    return GDK_FILTER_CONTINUE;
}

/* ── Ana giriş ── */

int main(int argc, char **argv) {
    /* X11 başlat */
    g_panel.dpy = XOpenDisplay(NULL);
    if (!g_panel.dpy) {
        fprintf(stderr, "linis-panel: X baglantisi kurulamadi\n");
        return 1;
    }
    g_panel.scr = DefaultScreen(g_panel.dpy);
    g_panel.root = RootWindow(g_panel.dpy, g_panel.scr);

    /* EWMH atomları */
    g_panel.net[NET_CLIENT_LIST] = xatom("_NET_CLIENT_LIST");
    g_panel.net[NET_ACTIVE_WINDOW] = xatom("_NET_ACTIVE_WINDOW");
    g_panel.net[NET_CURRENT_DESKTOP] = xatom("_NET_CURRENT_DESKTOP");
    g_panel.net[NET_NUMBER_OF_DESKTOPS] = xatom("_NET_NUMBER_OF_DESKTOPS");
    g_panel.net[NET_WM_DESKTOP] = xatom("_NET_WM_DESKTOP");

    /* Config yükle */
    static LConfig lcfg;
    config_defaults(&lcfg);
    const char *cfg_paths[] = {
        "/home/live/.config/linis/config.toml",
        "/etc/xdg/linis/config.toml",
        "/usr/share/linis/config.toml",
        NULL
    };
    config_load(&lcfg, cfg_paths, 3);
    g_panel.cfg = &lcfg;

    /* İlk refresh */
    refresh_tasks();

    /* GTK başlat */
    gtk_init(&argc, &argv);

    /* Dock ve bar oluştur */
    create_dock();
    create_bar();

    /* Property change filtresi */
    gdk_window_add_filter(NULL, event_filter, NULL);
    XSelectInput(g_panel.dpy, g_panel.root,
                 PropertyChangeMask | SubstructureNotifyMask);

    /* 1 saniye aralıkla yenile (saat için) */
    g_panel.refresh_timer = g_timeout_add(1000, refresh_timer_cb, NULL);

    /* İlk çizim */
    gtk_widget_queue_draw(g_panel.dock_drawing);
    gtk_widget_queue_draw(g_panel.bar_drawing);

    fprintf(stderr, "[linis-panel] baslatildi (GTK3 + Cairo)\n");
    gtk_main();

    /* Temizle */
    if (g_panel.tasks) free(g_panel.tasks);
    if (g_panel.tnames) {
        for (int i = 0; i < g_panel.ntasks; i++)
            free(g_panel.tnames[i]);
        free(g_panel.tnames);
    }
    XCloseDisplay(g_panel.dpy);
    return 0;
}
