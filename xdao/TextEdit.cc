/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999 Andreas Mueller <mueller@daneb.ping.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * $Log: TextEdit.cc,v $
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:40:01  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/19 20:28:12  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: TextEdit.cc,v 1.2 2000/02/20 23:34:54 llanero Exp $";

#include "TextEdit.h"

#include <stddef.h>
#include <ctype.h>

TextEdit::TextEdit(const char *sample) : Gtk::Entry()
{
  upper_ = 1;
  lower_ = 1;
  digits_ = 1;
  space_ = 1;

  if (sample != NULL && *sample != 0)
    setSize(sample);
}

TextEdit::~TextEdit()
{
}

void TextEdit::upperCase(int f)
{
  upper_ = (f != 0) ? 1 : 0;
}

void TextEdit::lowerCase(int f)
{
  lower_ = (f != 0) ? 1 : 0;
}

void TextEdit::digits(int f)
{
  digits_ = (f != 0) ? 1 : 0;
}

void TextEdit::space(int f)
{
  space_ = (f != 0) ? 1 : 0;
}

void TextEdit::insert_text_impl(const gchar *c, gint p2, gint *p3)
{
  char *s = new char[strlen(c) + 1];
  char *p = s;

  while (*c != 0) {
    if (islower(*c)) {
      if (!lower_) {
	if (upper_)
	  *p++ = toupper(*c);
      }
      else 
	*p++ = *c;
    }
    else if (isupper(*c)) {
      if (!upper_) {
	if (lower_)
	  *p++ = tolower(*c);
      }
      else
	*p++ = *c;
    }
    else if (isdigit(*c)) {
      if (digits_)
	*p++ = *c;
    }
    else if (isspace(*c)) {
      if (space_)
	*p++ = *c;
    }
    
    c++;
  }

  *p = 0;

  Gtk::Entry::insert_text_impl(s, p2, p3);

  delete[] s;
}

void TextEdit::setSize(const char *sample)
{
  const Gtk::Style *style = get_style();
  const GtkStyle *s = style->gtkobj();

  Gdk_Font font(s->font);

  set_usize(font.string_width(string(sample)) + 8,
	    font.string_height(string(sample)) + 12);
}
