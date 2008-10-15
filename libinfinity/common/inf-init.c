/* infinote - Collaborative notetaking application
 * Copyright (C) 2007 Armin Burgmeier
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libinfinity/common/inf-init.h>
#include <libinfinity/inf-i18n.h>

static guint inf_init_counter = 0;

/**
 * inf_init:
 * @error: Location to store error information, if any.
 *
 * This function initializes the libinfinity library and should be called
 * before any other functions of the library. The function does nothing if
 * it has already been called before().
 *
 * Returns: Whether the initialization was successful or not.
 */
gboolean
inf_init(GError** error)
{
  if(inf_init_counter == 0)
  {
    g_type_init();
    gnutls_global_init();
    _inf_gettext_init();
  }

  ++ inf_init_counter;
  return TRUE;
}

/**
 * inf_deinit:
 *
 * This functions deinitializes the libinfinity library. Make sure that all
 * objects the library provides have been freed before calling this function.
 * If inf_init() has been called multiple times, then inf_deinit() needs to be
 * called the same number of times to actually deinitialize the library.
 */
void
inf_deinit(void)
{
  if(--inf_init_counter == 0)
    gnutls_global_deinit();
}