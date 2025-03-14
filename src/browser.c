/*
 *   browser.c
 *   Copyright (C) 2019 David García Goñi <dagargo@gmail.com>
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

#include "browser.h"

gint
browser_sort_samples (GtkTreeModel * model,
		      GtkTreeIter * a, GtkTreeIter * b, gpointer data)
{
  struct item *itema;
  struct item *itemb;
  gint ret = 0;

  itema = browser_get_item (model, a);
  itemb = browser_get_item (model, b);

  if (itema->type == itemb->type)
    {
      ret = g_utf8_collate (itema->name, itemb->name);
    }
  else
    {
      ret = itema->type > itemb->type;
    }

  browser_free_item (itema);
  browser_free_item (itemb);

  return ret;
}

gint
browser_sort_data (GtkTreeModel * model,
		   GtkTreeIter * a, GtkTreeIter * b, gpointer data)
{
  struct item *itema;
  struct item *itemb;
  gint ret = 0;

  itema = browser_get_item (model, a);
  itemb = browser_get_item (model, b);

  ret = itema->index > itemb->index;

  browser_free_item (itema);
  browser_free_item (itemb);

  return ret;
}

struct item *
browser_get_item (GtkTreeModel * model, GtkTreeIter * iter)
{
  struct item *item = malloc (sizeof (struct item));

  gtk_tree_model_get (model, iter, BROWSER_LIST_STORE_TYPE_FIELD, &item->type,
		      BROWSER_LIST_STORE_NAME_FIELD, &item->name,
		      BROWSER_LIST_STORE_SIZE_FIELD, &item->size,
		      BROWSER_LIST_STORE_INDEX_FIELD, &item->index, -1);

  return item;
}

void
browser_set_selected_row_iter (struct browser *browser, GtkTreeIter * iter)
{
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (browser->view));
  GtkTreeModel *model =
    GTK_TREE_MODEL (gtk_tree_view_get_model (browser->view));
  GList *paths = gtk_tree_selection_get_selected_rows (selection, &model);

  gtk_tree_model_get_iter (model, iter, g_list_nth_data (paths, 0));
  g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
}

void
browser_reset (gpointer data)
{
  struct browser *browser = data;
  GtkListStore *list_store =
    GTK_LIST_STORE (gtk_tree_view_get_model (browser->view));

  gtk_entry_set_text (browser->dir_entry, browser->dir);
  gtk_list_store_clear (list_store);
  browser->check_selection (NULL);
}

void
browser_selection_changed (GtkTreeSelection * selection, gpointer data)
{
  struct browser *browser = data;
  g_idle_add (browser->check_selection, NULL);
}

void
browser_refresh (GtkWidget * object, gpointer data)
{
  struct browser *browser = data;
  g_idle_add (browser_load_dir, browser);
}

void
browser_go_up (GtkWidget * object, gpointer data)
{
  char *dup;
  char *new_path;
  struct browser *browser = data;

  if (strcmp (browser->dir, "/") != 0)
    {
      dup = strdup (browser->dir);
      new_path = dirname (dup);
      strcpy (browser->dir, new_path);
      free (dup);
      if (browser->notify_dir_change)
	{
	  browser->notify_dir_change (browser);
	}
    }

  g_idle_add (browser_load_dir, browser);
}

void
browser_item_activated (GtkTreeView * view, GtkTreePath * path,
			GtkTreeViewColumn * column, gpointer data)
{
  GtkTreeIter iter;
  struct item *item;
  struct browser *browser = data;
  GtkTreeModel *model = GTK_TREE_MODEL (gtk_tree_view_get_model
					(browser->view));

  gtk_tree_model_get_iter (model, &iter, path);
  item = browser_get_item (model, &iter);

  if (item->type == ELEKTROID_DIR)
    {
      if (strcmp (browser->dir, "/") != 0)
	{
	  strcat (browser->dir, "/");
	}
      strcat (browser->dir, item->name);
      browser_load_dir (browser);
      if (browser->notify_dir_change)
	{
	  browser->notify_dir_change (browser);
	}
    }

  browser_free_item (item);
}

gint
browser_get_selected_items_count (struct browser *browser)
{
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (browser->view));
  return gtk_tree_selection_count_selected_rows (selection);
}

void
browser_free_item (struct item *item)
{
  g_free (item->name);
  g_free (item);
}

gchar *
browser_get_item_path (struct browser *browser, struct item *item)
{
  gchar *path = chain_path (browser->dir, item->name);
  debug_print (1, "Using %s path for item %s (%d)...\n", path, item->name,
	       item->index);
  return path;
}

gchar *
browser_get_item_id_path (struct browser *browser, struct item *item)
{
  gchar *id = browser->fs_ops->getid (item);
  gchar *path = chain_path (browser->dir, id);
  debug_print (1, "Using %s path for item %s (%d)...\n", path, item->name,
	       item->index);
  g_free (id);
  return path;
}

static void
local_add_dentry_item (struct browser *browser, struct item_iterator *iter)
{
  gchar *hsize;
  GtkListStore *list_store =
    GTK_LIST_STORE (gtk_tree_view_get_model (browser->view));

  hsize = iter->item.size ? get_human_size (iter->item.size, TRUE) : "";

  gtk_list_store_insert_with_values (list_store, NULL, -1,
				     BROWSER_LIST_STORE_ICON_FIELD,
				     iter->item.type ==
				     ELEKTROID_DIR ? DIR_ICON :
				     browser->file_icon,
				     BROWSER_LIST_STORE_NAME_FIELD,
				     iter->item.name,
				     BROWSER_LIST_STORE_SIZE_FIELD,
				     iter->item.size,
				     BROWSER_LIST_STORE_SIZE_STR_FIELD,
				     hsize,
				     BROWSER_LIST_STORE_TYPE_FIELD,
				     iter->item.type,
				     BROWSER_LIST_STORE_INDEX_FIELD,
				     iter->item.index, -1);
  if (strlen (hsize))
    {
      g_free (hsize);
    }
}

static gboolean
browser_file_match_extensions (struct browser *browser,
			       struct item_iterator *iter)
{
  gboolean match;
  const gchar *entry_ext;
  gchar **ext = browser->extensions;

  if (iter->item.type == ELEKTROID_DIR)
    {
      return TRUE;
    }

  if (!ext)
    {
      return TRUE;
    }

  entry_ext = get_ext (iter->item.name);

  if (!entry_ext)
    {
      return FALSE;
    }

  match = FALSE;
  while (*ext != NULL && !match)
    {
      match = !strcasecmp (entry_ext, *ext);
      ext++;
    }

  return match;
}

gboolean
browser_load_dir (gpointer data)
{
  struct item_iterator iter;
  struct browser *browser = data;

  browser_reset (browser);

  if (browser->fs_ops->readdir (&iter, browser->dir, browser->data))
    {
      error_print ("Error while opening %s dir\n", browser->dir);
      goto end;
    }

  while (!next_item_iterator (&iter))
    {
      if (browser_file_match_extensions (browser, &iter))
	{
	  local_add_dentry_item (browser, &iter);
	}
    }
  free_item_iterator (&iter);

end:
  if (browser->check_callback)
    {
      browser->check_callback ();
    }
  gtk_tree_view_columns_autosize (browser->view);
  return FALSE;
}
