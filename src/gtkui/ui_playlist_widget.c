/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2008 Tomasz Moń <desowin@gmail.com>
 *  Copyright (C) 2009 William Pitcock <nenolod@atheme.org>
 *  Copyright (C) 2010 Michał Lipski <tallica@o2.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>

#include "ui_manager.h"
#include "ui_playlist_model.h"
#include "playlist_util.h"

typedef struct
{
    GList *targets;
    GtkWidget *source_widget;
    GtkTreePath *dest_path;
    gint source_playlist;
    gint source_pos;
    gboolean append;
} UiPlaylistDragTracker;

static UiPlaylistDragTracker *t;
static gboolean dropped = FALSE;

static void _ui_playlist_widget_drag_begin(GtkWidget *widget, GdkDragContext * context, gpointer data)
{
    t = g_slice_new0(UiPlaylistDragTracker);
    GList *list;
    gint playlist;

    g_signal_stop_emission_by_name(widget, "drag-begin");

    playlist_block_selection(GTK_TREE_VIEW(widget));

    list = playlist_get_selected_list(GTK_TREE_VIEW(widget));
    playlist = playlist_get_playlist_from_treeview(GTK_TREE_VIEW(widget));

    t->targets = list;
    t->source_widget = widget;
    t->source_playlist = playlist;
    t->source_pos = GPOINTER_TO_INT (g_object_get_data ((GObject *) widget,
     "recently clicked"));
    t->dest_path = NULL;
    t->append = FALSE;
}

static void _ui_playlist_widget_drag_motion(GtkTreeView * widget, GdkDragContext * context, gint x, gint y, guint time, gpointer user_data)
{
    GtkTreeViewDropPosition dp = GTK_TREE_VIEW_DROP_AFTER;
    GdkRectangle win;
    GtkAdjustment *vadj;
    GdkRectangle rect;
    GtkTreePath *path = NULL;
    gint tx, ty, dest_pos, end_pos, dest_playlist;
    gboolean next_row = FALSE;
    static guint last_time = 0;

    if (time - last_time > 250)
    {
        last_time = time;
        return;
    }

    if (t == NULL)
    {
        /* Dragging from other application */
        t = g_slice_new0(UiPlaylistDragTracker);
        t->source_playlist = -1;
        t->dest_path = NULL;
    }

    t->append = FALSE;

    g_signal_stop_emission_by_name(widget, "drag-motion");

    dest_playlist = playlist_get_playlist_from_treeview(widget);
    end_pos = aud_playlist_entry_count(dest_playlist) - 1;

    gdk_window_get_geometry(gtk_tree_view_get_bin_window(widget), NULL, NULL, NULL, &win.height, NULL);
    gtk_tree_view_convert_widget_to_bin_window_coords(widget, x, y, &tx, &ty);

    if (t->source_playlist != dest_playlist)
        t->source_pos = aud_playlist_entry_count(dest_playlist);

    gtk_tree_view_get_path_at_pos(widget, tx, ty, &path, NULL, NULL, NULL);

    if (path)
    {
        gtk_tree_view_get_background_area(widget, path, NULL, &rect);

        next_row = (ty - rect.y >= rect.height / 2);

        dest_pos = playlist_get_index_from_path(path);

        /* Increase dest_pos if in the lower half of the row */
        if (next_row)
            dest_pos++;

        dp = GTK_TREE_VIEW_DROP_BEFORE;

        if (next_row)
        {
            if (dest_pos > end_pos)
            {
                dp = GTK_TREE_VIEW_DROP_AFTER;
                t->append = TRUE;
            }
            else
            {
                gtk_tree_path_free(path);
                path = gtk_tree_path_new_from_indices(dest_pos, -1);
            }
        }
    }
    else
    {
        t->append = TRUE;
        path = gtk_tree_path_new_from_indices(end_pos, -1);
    }

    if (path != NULL)
    {
        t->dest_path = path;

        gtk_tree_view_set_drag_dest_row(widget, path, dp);

        gtk_tree_view_get_background_area(widget, path, NULL, &rect);

        vadj = gtk_tree_view_get_vadjustment(widget);

        if (ty >= 0 && ty < rect.height * 2 && vadj->value > 0)
        {
            gtk_adjustment_set_value(vadj, MAX(0, vadj->value - rect.height));
        }
        else if (win.height - ty <= rect.height * 2 && vadj->value < vadj->upper - vadj->page_size)
        {
            gtk_adjustment_set_value(vadj, MIN(vadj->upper - vadj->page_size, vadj->value + rect.height));
        }
    }
}

static void drag_tracker_cleanup()
{
    if (!t)
        return;

    if (t->targets)
    {
        g_list_foreach(t->targets, (GFunc) gtk_tree_path_free, NULL);
        g_list_free(t->targets);
    }

    if (t->dest_path)
        gtk_tree_path_free(t->dest_path);

    g_slice_free(UiPlaylistDragTracker, t);
    t = NULL;
}

static gboolean drag_drop_cb (GtkWidget * widget, GdkDragContext * context,
 gint x, gint y, guint time, void * unused)
{
    g_signal_stop_emission_by_name (widget, "drag-drop");

    dropped = TRUE;
    gtk_drag_get_data (widget, context, gdk_atom_intern ("text/uri-list",
     FALSE), time);
    return TRUE;
}

static void _ui_playlist_widget_drag_data_received(GtkTreeView * widget, GdkDragContext * context, gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
    g_signal_stop_emission_by_name (widget, "drag-data-received");

    if (! dropped)
    {
        gdk_drag_status (context, GDK_ACTION_COPY, time);
        return;
    }

    dropped = FALSE;

    /* Maybe dragged some files from another application. */
    if (gtk_drag_get_source_widget(context) == NULL)
    {
        gint playlist = playlist_get_playlist_from_treeview (widget);
        gint dest_pos;

        g_return_if_fail(t != NULL);

        if (t->dest_path != NULL)
        {
            dest_pos = playlist_get_index_from_path(t->dest_path);
        }
        else
            dest_pos = aud_playlist_entry_count (playlist);

        if (t->append)
            dest_pos++;

        audgui_urilist_insert (playlist, dest_pos, (const gchar *) data->data);

        GtkTreePath *end_path = gtk_tree_path_new_from_indices(MAX(0, dest_pos), -1);
        GtkTreePath *start_path = gtk_tree_path_new_from_indices(MAX(0, dest_pos), -1);
        playlist_pending_selection_set(widget, start_path, end_path);

        drag_tracker_cleanup();
    }

    gtk_drag_finish(context, FALSE, FALSE, time);
}

static void _ui_playlist_widget_drag_end(GtkTreeView * widget, GdkDragContext * context, gpointer data)
{
    gint pos, dest_playlist, dest_pos, selected_length;
    struct index *filenames;
    struct index *tuples;
    GtkTreeView *dest_treeview;

    g_return_if_fail(t != NULL);

    if (!t->dest_path || !t->targets || !t->source_widget)
        goto CLEANUP;

    dest_treeview = playlist_get_active_treeview();
    dest_pos = playlist_get_index_from_path(t->dest_path);
    dest_playlist = playlist_get_playlist_from_treeview(dest_treeview);
    selected_length = g_list_length(t->targets);
    gboolean same_playlist = (t->source_playlist == dest_playlist);

    if (t->append)
        dest_pos ++;

    if (same_playlist)
    {
        /* Adjust the shift amount so that the selected entry closest to the
         * destination ends up at the destination. */
        if (dest_pos > t->source_pos)
            dest_pos -= playlist_count_selected_in_range (dest_playlist,
             t->source_pos, dest_pos - t->source_pos);
        else
            dest_pos += playlist_count_selected_in_range (dest_playlist,
             dest_pos, t->source_pos - dest_pos);

        aud_playlist_shift (dest_playlist, t->source_pos, dest_pos -
         t->source_pos);
        treeview_set_focus (dest_treeview, dest_pos);
        treeview_set_selection_from_playlist (dest_treeview, dest_playlist);
    }
    else
    {
        filenames = index_new();
        tuples = index_new();

        for (GList *target = g_list_first(t->targets); target; target = target->next)
        {
            if (target->data != NULL)
            {
                pos = playlist_get_index_from_path(target->data);
                gchar *filename = g_strdup(aud_playlist_entry_get_filename(t->source_playlist, pos));

                if (filename != NULL)
                {
                    Tuple *tuple = tuple_copy(aud_playlist_entry_get_tuple(t->source_playlist, pos));
                    index_append(filenames, filename);
                    index_append(tuples, tuple);
                }
            }
        }

        aud_playlist_delete_selected(t->source_playlist);
        aud_playlist_entry_insert_batch(dest_playlist, dest_pos, filenames, tuples);
    }

    GtkTreePath *start_path = gtk_tree_path_new_from_indices(MAX(0, dest_pos), -1);
    GtkTreePath *end_path = gtk_tree_path_new_from_indices(MAX(0, dest_pos) + selected_length - 1, -1);

    if (! same_playlist)
    {
        playlist_set_selected(dest_treeview, g_list_first(t->targets)->data);

        playlist_pending_selection_set(dest_treeview, start_path, end_path);
    }

    gtk_widget_grab_focus(GTK_WIDGET(dest_treeview));

CLEANUP:
    playlist_unblock_selection(GTK_TREE_VIEW(widget));
    drag_tracker_cleanup();
    return;
}

static void _ui_playlist_widget_selection_update(GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer playlist_p)
{
    gint entry;

    gtk_tree_model_get(model, iter, PLAYLIST_COLUMN_NUM, &entry, -1);

    /* paths are numbered from 1, playlist index start from 0 */
    entry -= 1;

    aud_playlist_entry_set_selected(GPOINTER_TO_INT(playlist_p), entry, TRUE);
}

static void _ui_playlist_widget_selection_changed(GtkTreeSelection * selection, gpointer treeview)
{
    gint playlist = playlist_get_playlist_from_treeview(treeview);

    aud_playlist_select_all(playlist, FALSE);

    gtk_tree_selection_selected_foreach(selection, _ui_playlist_widget_selection_update, GINT_TO_POINTER(playlist));
}

static void ui_playlist_widget_change_song(GtkTreeView * treeview, guint pos)
{
    gint playlist = playlist_get_playlist_from_treeview(treeview);

    aud_playlist_set_playing(playlist);
    aud_playlist_set_position(playlist, pos);

    if (!audacious_drct_get_playing())
        audacious_drct_play();
}

static void ui_playlist_widget_row_activated(GtkTreeView * treeview, GtkTreePath * path, GtkTreeViewColumn *column, gpointer user_data)
{
    if (path)
    {
        gint pos = playlist_get_index_from_path(path);

        if (pos >= 0)
            ui_playlist_widget_change_song(treeview, pos);
    }
}

static gboolean ui_playlist_widget_keypress_cb(GtkWidget * widget, GdkEventKey * event, gpointer data)
{
    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
      case 0:
        switch (event->keyval)
        {
          case GDK_KP_Enter:
          {
            GtkTreePath *path = playlist_get_first_selected_path(GTK_TREE_VIEW(widget));
            gtk_tree_view_row_activated(GTK_TREE_VIEW(widget), path, NULL);
            return TRUE;
          }
          default:
            return FALSE;
        }
        break;
      case GDK_MOD1_MASK:
      {
        if ((event->keyval == GDK_Up) || (event->keyval == GDK_Down)) {
            gint playlist = playlist_get_playlist_from_treeview(GTK_TREE_VIEW(widget));
            /* Copy the event, so we can get the selection to move as well */
            GdkEvent * ev = gdk_event_copy((GdkEvent *) event);
            ((GdkEventKey *) ev)->state = 0;

            aud_playlist_shift_selected(playlist, (event->keyval == GDK_Up) ? -1 : 1);
            gtk_propagate_event(widget, ev);
            gdk_event_free(ev);
            return TRUE;
        }
      }
      default:
        return FALSE;
    }
    return FALSE;
}

static gint pos[2];

static gboolean ui_playlist_widget_button_press_cb(GtkWidget * widget, GdkEventButton * event)
{
    GtkTreePath *path = NULL;
    GtkTreeSelection *sel = NULL;
    gint state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK);

    gtk_tree_view_get_path_at_pos ((GtkTreeView *) widget, event->x, event->y,
     & path, NULL, NULL, NULL);

    /* Save the row clicked on for drag and drop. */
    if (path)
        g_object_set_data ((GObject *) widget, "recently clicked",
         GINT_TO_POINTER (gtk_tree_path_get_indices (path)[0]));

    if (event->button == 1 && !state)
    {
        pos[0] = event->x;
        pos[1] = event->y;
    }

    if (event->button == 1 && state)
        goto NOT_HANDLED;

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
        ui_manager_popup_menu_show(GTK_MENU(playlistwin_popup_menu), event->x_root, event->y_root + 2, 3, event->time);

    if (path == NULL)
        goto NOT_HANDLED;

    if (event->button == 1 && !state && event->type == GDK_2BUTTON_PRESS)
    {
        gtk_tree_view_row_activated(GTK_TREE_VIEW(widget), path, NULL);
        pos[0] = -1;
        goto HANDLED;
    }

    if (gtk_tree_selection_path_is_selected(sel, path) &&
        playlist_get_selected_length(GTK_TREE_VIEW(widget)) > 1 &&
        event->type == GDK_BUTTON_PRESS)
        goto HANDLED;

    pos[0] = -1;

NOT_HANDLED:
    if (path)
        gtk_tree_path_free (path);
    return FALSE;

HANDLED:
    if (path)
        gtk_tree_path_free (path);
    return TRUE;
}

static gboolean ui_playlist_widget_button_release_cb(GtkWidget * widget, GdkEventButton * event)
{
    GtkTreePath *path = NULL;
    GtkTreeSelection *sel = NULL;
    gint state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK);

    if (event->button == 1 && !state &&
        pos[0] == event->x && pos[1] == event->y)
    {
        gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, NULL, NULL, NULL);
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

        if (path == NULL)
            return FALSE;

        playlist_block_selection(GTK_TREE_VIEW(widget));
        gtk_tree_selection_unselect_all(sel);
        playlist_unblock_selection(GTK_TREE_VIEW(widget));
        gtk_tree_selection_select_path(sel, path);

        gtk_tree_path_free(path);
    }

    return FALSE;
}

static void ui_playlist_widget_set_column(GtkWidget *treeview, gchar *title, gint column_id, gint width, gboolean ellipsize, gboolean resizable)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(title, renderer, "text", column_id, "weight", PLAYLIST_MULTI_COLUMN_WEIGHT, NULL);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);

    if (column_id == PLAYLIST_MULTI_COLUMN_NUM)
        gtk_tree_view_column_set_min_width(column, width);
    else
        gtk_tree_view_column_set_fixed_width(column, width);

    if (resizable)
        gtk_tree_view_column_set_resizable(column, TRUE);

    if (ellipsize)
        g_object_set(G_OBJECT(renderer), "ypad", 1, "xpad", 1, "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    else
        g_object_set(G_OBJECT(renderer), "ypad", 1, "xpad", 1, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
}

GtkWidget *ui_playlist_widget_new(gint playlist)
{
    GtkWidget *treeview;
    UiPlaylistModel *model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    gulong selection_changed_handler_id;
    const GtkTargetEntry target_entry[] = {
        {"text/uri-list", 0, 0}
    };

    model = ui_playlist_model_new(playlist);
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
    g_object_unref(model);

    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(treeview), TRUE);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
    gtk_drag_dest_set_track_motion(treeview, TRUE);

    if (multi_column_view)
    {
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);

        ui_playlist_widget_set_column(treeview, NULL, PLAYLIST_MULTI_COLUMN_NUM, calculate_column_width(treeview, model->num_rows), FALSE, FALSE);
        ui_playlist_widget_set_column(treeview, "Artist", PLAYLIST_MULTI_COLUMN_ARTIST, 150, TRUE, TRUE);
        ui_playlist_widget_set_column(treeview, "Album", PLAYLIST_MULTI_COLUMN_ALBUM, 200, TRUE, TRUE);
        ui_playlist_widget_set_column(treeview, "No", PLAYLIST_MULTI_COLUMN_TRACK_NUM, 40, FALSE, TRUE);
        ui_playlist_widget_set_column(treeview, "Title", PLAYLIST_MULTI_COLUMN_TITLE, 250, TRUE, TRUE);
        ui_playlist_widget_set_column(treeview, "Time", PLAYLIST_MULTI_COLUMN_TIME, 50, FALSE, FALSE);
    }
    else
    {
        column = gtk_tree_view_column_new();
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_spacing(column, 8);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, FALSE);
        gtk_tree_view_column_set_attributes(column, renderer, "text", PLAYLIST_COLUMN_NUM, "weight", PLAYLIST_COLUMN_WEIGHT, NULL);
        g_object_set(G_OBJECT(renderer), "ypad", 1, "xpad", 1, NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, TRUE);
        gtk_tree_view_column_set_attributes(column, renderer, "text", PLAYLIST_COLUMN_TEXT, "weight", PLAYLIST_COLUMN_WEIGHT, NULL);
        g_object_set(G_OBJECT(renderer), "ypad", 1, "xpad", 1, "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(column, renderer, FALSE);
        gtk_tree_view_column_set_attributes(column, renderer, "text", PLAYLIST_COLUMN_TIME, "weight", PLAYLIST_COLUMN_WEIGHT, NULL);
        g_object_set(G_OBJECT(renderer), "ypad", 1, "xpad", 1, "xalign", 1.0, NULL);

        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    }

    gtk_drag_dest_set(treeview, GTK_DEST_DEFAULT_ALL, target_entry, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    gtk_drag_source_set(treeview, GDK_BUTTON1_MASK, target_entry, 1, GDK_ACTION_MOVE);

    g_signal_connect(treeview, "row-activated", G_CALLBACK(ui_playlist_widget_row_activated), NULL);

    g_signal_connect(treeview, "key-press-event", G_CALLBACK(ui_playlist_widget_keypress_cb), NULL);
    g_signal_connect(treeview, "button-press-event", G_CALLBACK(ui_playlist_widget_button_press_cb), NULL);
    g_signal_connect(treeview, "button-release-event", G_CALLBACK(ui_playlist_widget_button_release_cb), NULL);

    g_signal_connect(treeview, "drag-begin", G_CALLBACK(_ui_playlist_widget_drag_begin), NULL);
    g_signal_connect(treeview, "drag-motion", G_CALLBACK(_ui_playlist_widget_drag_motion), NULL);
    g_signal_connect(treeview, "drag-drop", (GCallback) drag_drop_cb, NULL);
    g_signal_connect(treeview, "drag-data-received", G_CALLBACK(_ui_playlist_widget_drag_data_received), NULL);
    g_signal_connect(treeview, "drag-end", G_CALLBACK(_ui_playlist_widget_drag_end), NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
    gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(treeview), FALSE);
    selection_changed_handler_id = g_signal_connect(selection, "changed", G_CALLBACK(_ui_playlist_widget_selection_changed), treeview);
    g_object_set_data(G_OBJECT(treeview), "selection_changed_handler_id", GINT_TO_POINTER(selection_changed_handler_id));

    return treeview;
}
