/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2009 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#if !defined (__PACKAGEKIT_H_INSIDE__) && !defined (PK_COMPILATION)
#error "Only <packagekit.h> can be included directly."
#endif

#ifndef __PK_PACKAGE_IDS_H
#define __PK_PACKAGE_IDS_H

#include <glib.h>

G_BEGIN_DECLS

/* rationalle:
 *
 * '%': breaks printf
 * '|': used as the filename seporator
 * '~': conary
 * '@': conary
 *
 * If this has to be changed, also change:
 * - backends/urpmi/helpers/urpmi-dispatched-backend.pl
 * - python/packagekit/backend.py
 */
#define PK_PACKAGE_IDS_DELIM	"&"

void		 pk_package_ids_test			(gpointer	 user_data);
gchar		**pk_package_ids_from_id		(const gchar	*package_id);
gchar		**pk_package_ids_from_text		(const gchar	*package_id);
gboolean	 pk_package_ids_check			(gchar		**package_ids);
gchar		*pk_package_ids_to_text			(gchar		**package_ids);
gboolean	 pk_package_ids_present_id		(gchar		**package_ids,
							 const gchar	*package_id);
gchar		**pk_package_ids_add_id			(gchar		**package_ids,
							 const gchar	*package_id);
gchar		**pk_package_ids_add_ids		(gchar		**package_ids,
							 gchar		**package_ids_new);
gchar		**pk_package_ids_remove_id		(gchar		**package_ids,
							 const gchar	*package_id);

G_END_DECLS

#endif /* __PK_PACKAGE_IDS_H */