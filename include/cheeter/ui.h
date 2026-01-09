#ifndef CHEETER_UI_H
#define CHEETER_UI_H

#include <gtk/gtk.h>

void cheeter_ui_init(int *argc, char ***argv);
void cheeter_ui_run(void);
void cheeter_ui_quit(void);

// Toggle visibility. If showing, hide. If hiding, show using the given sheet.
// If sheet_path is NULL, and currently hidden, show empty or default state.
void cheeter_ui_toggle(const char *sheet_path);

// Helper to create the viewer widget
GtkWidget *cheeter_viewer_new(void);
void cheeter_viewer_load_file(GtkWidget *viewer, const char *path);

// Get the dimensions of the currently loaded page (returns FALSE if no page)
gboolean cheeter_viewer_get_page_size(GtkWidget *viewer, double *width,
                                      double *height);

// Set the scale factor for rendering
void cheeter_viewer_set_scale(GtkWidget *viewer, double scale);

#endif
