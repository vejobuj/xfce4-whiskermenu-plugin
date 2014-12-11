/*
 * Copyright (C) 2013 Graeme Gott <graeme@gottcode.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "resizer-widget.h"

#include "settings.h"
#include "slot.h"

using namespace WhiskerMenu;

//-----------------------------------------------------------------------------

ResizerWidget::ResizerWidget(GtkWindow* window) :
	m_window(window),
	m_cursor(NULL),
	m_shape(3),
	m_supports_alpha(false)
{
	m_alignment = GTK_ALIGNMENT(gtk_alignment_new(1,0,0,0));

	m_drawing = gtk_drawing_area_new();
	gtk_widget_set_size_request(m_drawing, 10, 10);
	gtk_widget_add_events(m_drawing, GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
	gtk_container_add(GTK_CONTAINER(m_alignment), m_drawing);

	g_signal_connect_slot(m_drawing, "button-press-event", &ResizerWidget::on_button_press_event, this);
	g_signal_connect_slot(m_drawing, "enter-notify-event", &ResizerWidget::on_enter_notify_event, this);
	g_signal_connect_slot(m_drawing, "leave-notify-event", &ResizerWidget::on_leave_notify_event, this);
	g_signal_connect_slot(m_drawing, "expose-event", &ResizerWidget::on_expose_event, this);
	g_signal_connect_slot(m_window, "screen-changed", &ResizerWidget::on_screen_changed_event, this);
	on_screen_changed_event(GTK_WIDGET(m_drawing), NULL);


	set_corner(TopRight);
}

//-----------------------------------------------------------------------------

ResizerWidget::~ResizerWidget()
{
	if (m_cursor)
	{
		gdk_cursor_unref(m_cursor);
	}
}

//-----------------------------------------------------------------------------

void ResizerWidget::set_corner(Corner corner)
{
	static const GdkPoint bottomleft[] = { {10,10}, {0,10}, {0,0} };
	static const GdkPoint topleft[] = { {10,0}, {0,10}, {0,0} };
	static const GdkPoint bottomright[] = { {10,10}, {0,10}, {10,0} };
	static const GdkPoint topright[] = { {10,0}, {10,10}, {0,0} };

	GdkCursorType type;
	switch (corner)
	{
	case BottomLeft:
		gtk_alignment_set(m_alignment, 0,1,0,0);
		m_shape.assign(bottomleft, bottomleft + 3);
		m_edge = GDK_WINDOW_EDGE_SOUTH_WEST;
		type = GDK_BOTTOM_LEFT_CORNER;
		break;
	case TopLeft:
		gtk_alignment_set(m_alignment, 0,0,0,0);
		m_shape.assign(topleft, topleft + 3);
		m_edge = GDK_WINDOW_EDGE_NORTH_WEST;
		type = GDK_TOP_LEFT_CORNER;
		break;
	case BottomRight:
		gtk_alignment_set(m_alignment, 1,1,0,0);
		m_shape.assign(bottomright, bottomright + 3);
		m_edge = GDK_WINDOW_EDGE_SOUTH_EAST;
		type = GDK_BOTTOM_RIGHT_CORNER;
		break;
	case TopRight:
	default:
		gtk_alignment_set(m_alignment, 1,0,0,0);
		m_shape.assign(topright, topright + 3);
		m_edge = GDK_WINDOW_EDGE_NORTH_EAST;
		type = GDK_TOP_RIGHT_CORNER;
		break;
	}

	if (m_cursor)
	{
		gdk_cursor_unref(m_cursor);
	}
	m_cursor = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(m_alignment)), type);
}

//-----------------------------------------------------------------------------

gboolean ResizerWidget::on_button_press_event(GtkWidget*, GdkEvent* event)
{
	GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
	gtk_window_begin_resize_drag(m_window,
			m_edge,
			event_button->button,
			event_button->x_root,
			event_button->y_root,
			event_button->time);
	return true;
}

//-----------------------------------------------------------------------------

gboolean ResizerWidget::on_enter_notify_event(GtkWidget* widget, GdkEvent*)
{
	gtk_widget_set_state(widget, GTK_STATE_PRELIGHT);
	GdkWindow* window = gtk_widget_get_window(widget);
	gdk_window_set_cursor(window, m_cursor);
	return false;
}

//-----------------------------------------------------------------------------

gboolean ResizerWidget::on_leave_notify_event(GtkWidget* widget, GdkEvent*)
{
	gtk_widget_set_state(widget, GTK_STATE_NORMAL);
	GdkWindow* window = gtk_widget_get_window(widget);
	gdk_window_set_cursor(window, NULL);
	return false;
}

//-----------------------------------------------------------------------------

void ResizerWidget::on_screen_changed_event(GtkWidget* widget, GdkScreen*)
{
	GdkScreen* screen = gtk_widget_get_screen(widget);
	GdkColormap* colormap = gdk_screen_get_rgba_colormap(screen);
	if (!colormap)
	{
		colormap = gdk_screen_get_system_colormap(screen);
		m_supports_alpha = false;
	}
	else
	{
		m_supports_alpha = true;
	}
	gtk_widget_set_colormap(widget, colormap);
}

//-----------------------------------------------------------------------------

gboolean ResizerWidget::on_expose_event(GtkWidget* widget, GdkEvent*)
{
	cairo_t* cr = gdk_cairo_create(gtk_widget_get_window(widget));

	GtkStyle* style = gtk_widget_get_style(widget);

	// Draw semi-transparent background to match window
	if (m_supports_alpha)
	{
		const GdkColor& color = style->bg[GTK_STATE_NORMAL];
		cairo_set_source_rgba(cr, color.red / 65535.0, color.green / 65535.0, color.blue / 65535.0, wm_settings->menu_opacity / 100.0);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_paint(cr);
	}

	// Draw resize triangle
	const GdkColor& color = style->text_aa[gtk_widget_get_state(widget)];
	cairo_set_source_rgb(cr, color.red / 65535.0, color.green / 65535.0, color.blue / 65535.0);

	cairo_move_to(cr, m_shape.back().x, m_shape.back().y);
	for (std::vector<GdkPoint>::const_iterator point = m_shape.begin(), end = m_shape.end(); point != end; ++point)
	{
		cairo_line_to(cr, point->x, point->y);
	}
	cairo_fill(cr);

	cairo_destroy(cr);

	return true;
}

//-----------------------------------------------------------------------------
