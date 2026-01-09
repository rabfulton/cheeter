#include "cheeter/log.h"
#include "cheeter/ui.h"
#include <gtk/gtk.h>

static GtkWidget *g_window = NULL;
static GtkWidget *g_viewer = NULL;

void cheeter_ui_init(int *argc, char ***argv) { gtk_init(argc, argv); }

void cheeter_ui_run(void) { gtk_main(); }

void cheeter_ui_quit(void) { gtk_main_quit(); }

static void ensure_window(void) {
  if (g_window)
    return;

  g_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  // Overlay-style properties
  gtk_window_set_decorated(GTK_WINDOW(g_window), FALSE); // Borderless
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_window), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(g_window), TRUE);
  gtk_window_set_keep_above(GTK_WINDOW(g_window), TRUE);
  gtk_window_set_type_hint(GTK_WINDOW(g_window),
                           GDK_WINDOW_TYPE_HINT_POPUP_MENU);

  // Add Viewer
  g_viewer = cheeter_viewer_new();
  gtk_container_add(GTK_CONTAINER(g_window), g_viewer);

  // Handle close/delete
  g_signal_connect(g_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_realize(g_window);
}

void cheeter_ui_toggle(const char *sheet_path) {
  ensure_window();

  if (gtk_widget_get_visible(g_window)) {
    // Hide
    gtk_widget_hide(g_window);
    LOG_INFO("UI Hidden");
  } else {
    // Load the sheet first so we can get its dimensions
    if (sheet_path) {
      cheeter_viewer_load_file(g_viewer, sheet_path);
    } else {
      cheeter_viewer_load_file(g_viewer, NULL);
    }

    // Get monitor dimensions
    GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(g_window));
    GdkDisplay *display = gdk_screen_get_display(screen);
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    if (!monitor) {
      monitor = gdk_display_get_monitor(display, 0);
    }

    GdkRectangle monitor_rect;
    gdk_monitor_get_geometry(monitor, &monitor_rect);
    int mon_w = monitor_rect.width;
    int mon_h = monitor_rect.height;

    // Get PDF page dimensions (in points, 72 points/inch)
    double page_w = 800, page_h = 600; // Default fallback
    if (cheeter_viewer_get_page_size(g_viewer, &page_w, &page_h)) {
      LOG_DEBUG("PDF page size (points): %.0f x %.0f", page_w, page_h);
    }

    // PDF is in points (72 DPI). Scale up to display resolution.
    // Get the monitor scale factor (for HiDPI)
    int gdk_scale = gdk_monitor_get_scale_factor(monitor);

    // Base scale: convert 72 DPI points to ~96 DPI pixels (common baseline)
    // Then multiply by GDK scale factor for HiDPI
    double dpi_scale = (96.0 / 72.0) * gdk_scale;

    double scaled_w = page_w * dpi_scale;
    double scaled_h = page_h * dpi_scale;

    LOG_DEBUG("Scaled size: %.0f x %.0f (dpi_scale=%.2f, gdk_scale=%d)",
              scaled_w, scaled_h, dpi_scale, gdk_scale);

    // Cap at 90% of monitor if still too large
    double max_w = mon_w * 0.9;
    double max_h = mon_h * 0.9;
    double final_scale = 1.0;

    if (scaled_w > max_w || scaled_h > max_h) {
      double scale_w = max_w / scaled_w;
      double scale_h = max_h / scaled_h;
      final_scale = (scale_w < scale_h) ? scale_w : scale_h;
    }

    int win_w = (int)(scaled_w * final_scale);
    int win_h = (int)(scaled_h * final_scale);

    // Tell the viewer what scale to use for rendering
    double render_scale = dpi_scale * final_scale;
    cheeter_viewer_set_scale(g_viewer, render_scale);

    // Center the window on the monitor
    int win_x = monitor_rect.x + (mon_w - win_w) / 2;
    int win_y = monitor_rect.y + (mon_h - win_h) / 2;

    gtk_window_resize(GTK_WINDOW(g_window), win_w, win_h);
    gtk_window_move(GTK_WINDOW(g_window), win_x, win_y);

    LOG_INFO("Window: %dx%d at (%d,%d), render_scale=%.2f", win_w, win_h, win_x,
             win_y, render_scale);

    gtk_widget_show_all(g_window);
    gtk_window_present(GTK_WINDOW(g_window));
    LOG_INFO("UI Shown: %s", sheet_path ? sheet_path : "(none)");
  }
}
