#include "cheeter/log.h"
#include "cheeter/ui.h"
#include <gtk/gtk.h>
#include <poppler.h>

// Simple viewer widget: A GtkScrolledWindow containing a GtkDrawingArea
// MVP: Render page 1 only.

typedef struct {
  GtkWidget *drawing_area;
  PopplerDocument *doc;
  PopplerPage *page;
  GdkPixbuf *image;
  double scale;
  int current_page;
  int n_pages;
} ViewerData;

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
  ViewerData *data = (ViewerData *)user_data;
  if (!data->page && !data->image) {
    // Draw nothing or placeholder
    return FALSE;
  }

  // Set white background
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget, &alloc);
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
  cairo_fill(cr);

  // Apply scale
  cairo_scale(cr, data->scale, data->scale);

  if (data->image) {
    gdk_cairo_set_source_pixbuf(cr, data->image, 0, 0);
    cairo_paint(cr);
  } else if (data->page) {
    poppler_page_render(data->page, cr);
  }

  return FALSE;
}

static void free_viewer_data(gpointer user_data) {
  ViewerData *data = (ViewerData *)user_data;
  if (data->page)
    g_object_unref(data->page);
  g_object_unref(data->doc);
  if (data->image)
    g_object_unref(data->image);
  g_free(data);
}

GtkWidget *cheeter_viewer_new(void) {
  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  GtkWidget *da = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER(scroll), da);

  ViewerData *data = g_new0(ViewerData, 1);
  data->drawing_area = da;
  data->scale = 1.0;

  g_signal_connect(da, "draw", G_CALLBACK(on_draw), data);
  g_object_set_data_full(G_OBJECT(scroll), "viewer-data", data,
                         free_viewer_data);

  return scroll;
}

static void load_page(ViewerData *data, int page_index) {
  if (!data->doc || page_index < 0 || page_index >= data->n_pages)
    return;

  if (data->page) {
    g_object_unref(data->page);
  }

  data->current_page = page_index;
  data->page = poppler_document_get_page(data->doc, page_index);

  // Resize drawing area
  if (data->page) {
    double w, h;
    poppler_page_get_size(data->page, &w, &h);
    gtk_widget_set_size_request(data->drawing_area, (int)(w * data->scale),
                                (int)(h * data->scale));
  }

  gtk_widget_queue_draw(data->drawing_area);
}

void cheeter_viewer_load_file(GtkWidget *viewer, const char *path) {
  ViewerData *data =
      (ViewerData *)g_object_get_data(G_OBJECT(viewer), "viewer-data");
  if (!data)
    return;

  if (data->page) {
    g_object_unref(data->page);
    data->page = NULL;
  }
  if (data->doc) {
    g_object_unref(data->doc);
    data->doc = NULL;
  }
  if (data->image) {
    g_object_unref(data->image);
    data->image = NULL;
  }
  data->current_page = 0;
  data->n_pages = 0;

  if (!path) {
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  GError *error = NULL;

  // Try loading as image first using gdk-pixbuf
  data->image = gdk_pixbuf_new_from_file(path, &error);
  if (data->image) {
    LOG_INFO("Loaded image: %s", path);
    data->n_pages = 1;

    // Resize drawing area for image
    double w = gdk_pixbuf_get_width(data->image);
    double h = gdk_pixbuf_get_height(data->image);
    gtk_widget_set_size_request(data->drawing_area, (int)(w * data->scale),
                                (int)(h * data->scale));
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  // If not an image (or failed), try PDF
  if (error) {
    // Clear error from image load attempt so we can reuse for PDF
    // or maybe we just ignore it and trust PDF load to fail if it's not a PDF
    // either. But strictly speaking, gdk_pixbuf might set error.
    g_clear_error(&error);
  }

  char *uri = g_filename_to_uri(path, NULL, NULL);
  data->doc = poppler_document_new_from_file(uri, NULL, &error);
  g_free(uri);

  if (!data->doc) {
    LOG_WARN("Failed to load as Image or PDF %s: %s", path,
             error ? error->message : "unknown");
    if (error)
      g_error_free(error);
    gtk_widget_queue_draw(data->drawing_area);
    return;
  }

  data->n_pages = poppler_document_get_n_pages(data->doc);
  LOG_DEBUG("Loaded PDF with %d pages", data->n_pages);

  // Load page 0
  load_page(data, 0);
}

gboolean cheeter_viewer_get_page_size(GtkWidget *viewer, double *width,
                                      double *height) {
  ViewerData *data =
      (ViewerData *)g_object_get_data(G_OBJECT(viewer), "viewer-data");
  if (!data)
    return FALSE;

  if (data->image) {
    *width = gdk_pixbuf_get_width(data->image);
    *height = gdk_pixbuf_get_height(data->image);
    return TRUE;
  }

  if (!data->page)
    return FALSE;

  poppler_page_get_size(data->page, width, height);
  return TRUE;
}

void cheeter_viewer_set_scale(GtkWidget *viewer, double scale) {
  ViewerData *data =
      (ViewerData *)g_object_get_data(G_OBJECT(viewer), "viewer-data");
  if (!data)
    return;

  data->scale = scale;

  // Update drawing area size for new scale
  if (data->image) {
    double w = gdk_pixbuf_get_width(data->image);
    double h = gdk_pixbuf_get_height(data->image);
    gtk_widget_set_size_request(data->drawing_area, (int)(w * scale),
                                (int)(h * scale));
  } else if (data->page) {
    double w, h;
    poppler_page_get_size(data->page, &w, &h);
    gtk_widget_set_size_request(data->drawing_area, (int)(w * scale),
                                (int)(h * scale));
  }

  gtk_widget_queue_draw(data->drawing_area);
}

void cheeter_viewer_next_page(GtkWidget *viewer) {
  ViewerData *data =
      (ViewerData *)g_object_get_data(G_OBJECT(viewer), "viewer-data");
  if (!data || !data->doc)
    return;

  if (data->current_page < data->n_pages - 1) {
    load_page(data, data->current_page + 1);
  }
}

void cheeter_viewer_prev_page(GtkWidget *viewer) {
  ViewerData *data =
      (ViewerData *)g_object_get_data(G_OBJECT(viewer), "viewer-data");
  if (!data || !data->doc)
    return;

  if (data->current_page > 0) {
    load_page(data, data->current_page - 1);
  }
}
