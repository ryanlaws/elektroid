/*
 *   notifier.c
 *   Copyright (C) 2021 David García Goñi <dagargo@gmail.com>
 *
 *   This file is part of Elektroid.
 *
 *   Elektroid is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Elektroid is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Elektroid. If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include "notifier.h"
#include "utils.h"

void
notifier_init (struct notifier *notifier, struct browser *browser)
{
  notifier->fd = inotify_init ();
  notifier->wd = -1;
  notifier->event_size = sizeof (struct inotify_event) + PATH_MAX;
  notifier->event = malloc (notifier->event_size);
  notifier->running = 1;
  notifier->browser = browser;
}

void
notifier_set_dir (struct notifier *notifier, gchar * path)
{
  debug_print (1, "Changing notifier path to %s...\n", path);
  if (notifier->fd < 0)
    {
      return;
    }
  if (notifier->wd >= 0)
    {
      inotify_rm_watch (notifier->fd, notifier->wd);
    }
  notifier->wd =
    inotify_add_watch (notifier->fd, path,
		       IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF
		       | IN_MOVE_SELF | IN_MOVED_TO);
}

void
notifier_close (struct notifier *notifier)
{
  if (notifier->fd < 0)
    {
      return;
    }
  if (notifier->wd >= 0)
    {
      inotify_rm_watch (notifier->fd, notifier->wd);
    }
  close (notifier->fd);
}

void
notifier_free (struct notifier *notifier)
{
  free (notifier->event);
}

static gboolean
notifier_go_up (gpointer data)
{
  struct browser *browser = data;
  browser_go_up (NULL, browser);
  return FALSE;
}

gpointer
notifier_run (gpointer data)
{
  ssize_t size;
  struct notifier *notifier = data;

  while (notifier->running)
    {
      size = read (notifier->fd, notifier->event, notifier->event_size);

      if (size == 0 || size == EBADF)
	{
	  break;
	}

      if (size < 0)
	{
	  debug_print (2, "Error while reading notifier: %s\n",
		       g_strerror (errno));
	  continue;
	}

      if (notifier->event->mask & IN_CREATE
	  || notifier->event->mask & IN_DELETE
	  || notifier->event->mask & IN_MOVED_FROM
	  || notifier->event->mask & IN_MOVED_TO)
	{
	  debug_print (1, "Reloading local dir...\n");
	  g_idle_add (browser_load_dir, notifier->browser);
	}
      else if (notifier->event->mask & IN_DELETE_SELF
	       || notifier->event->mask & IN_MOVE_SELF ||
	       notifier->event->mask & IN_MOVED_TO)
	{
	  debug_print (1, "Loading local parent dir...\n");
	  g_idle_add (notifier_go_up, notifier->browser);
	}
      else
	{
	  if (!(notifier->event->mask & IN_IGNORED))
	    {
	      error_print ("Unexpected event: %d\n", notifier->event->mask);
	    }
	}
    }

  return NULL;
}
