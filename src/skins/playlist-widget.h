/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2009-2011 John Lindgren
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#ifndef SKINS_UI_SKINNED_PLAYLIST_H
#define SKINS_UI_SKINNED_PLAYLIST_H

#include <glib.h>
#include <libaudcore/objects.h>

#include "widget.h"

class PlaylistSlider;

typedef SmartPtr<PangoFontDescription, pango_font_description_free> PangoFontDescPtr;

class PlaylistWidget : public Widget
{
public:
    PlaylistWidget (int width, int height, const char * font);
    ~PlaylistWidget () { cancel_all (); }

    void set_slider (PlaylistSlider * slider) { m_slider = slider; }
    void resize (int width, int height);
    void set_font (const char * m_font);
    void refresh ();
    bool handle_keypress (GdkEventKey * event);
    void row_info (int * m_rows, int * m_first);
    void scroll_to (int row);
    void set_focused (int row);
    void hover (int x, int y);
    int hover_end ();

private:
    void draw (cairo_t * cr);
    bool button_press (GdkEventButton * event);
    bool button_release (GdkEventButton * event);
    bool motion (GdkEventMotion * event);
    bool leave ();

    void update_title ();
    void calc_layout ();

    int calc_position (int y) const;
    int adjust_position (bool relative, int position) const;

    void ensure_visible (int position);
    void select_single (bool relative, int position);
    void select_extend (bool relative, int position);
    void select_slide (bool relative, int position);
    void select_toggle (bool relative, int position);
    void select_move (bool relative, int position);
    void delete_selected ();

    void cancel_all ();
    void scroll_timeout ();
    void popup_trigger (int pos);
    void popup_show ();
    void popup_hide ();

    static void scroll_timeout_cb (void * me)
        { ((PlaylistWidget *) me)->scroll_timeout (); }
    static gboolean popup_show_cb (void * me)
        { ((PlaylistWidget *) me)->popup_show (); return G_SOURCE_REMOVE; }

    PlaylistSlider * m_slider = nullptr;
    PangoFontDescPtr m_font;
    String title_text;

    int m_playlist = -1, m_playlist_id = -1, m_length = 0;
    int m_width = 0, m_height = 0, m_row_height = 1, m_offset = 0, m_rows = 0, m_first = 0;
    int m_scroll = 0, m_hover = -1, m_drag = 0, m_popup_pos = -1, m_popup_source = 0;
    bool popup_shown = false;
};

#endif
