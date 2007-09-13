#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus-gmain.h>
#include "gvfsdaemon.h"
#include "gdbusutils.h"
#include "gmountsource.h"
#include "gvfsbackendsmbbrowse.h"
#include <gvfsdaemonprotocol.h>

int
main (int argc, char *argv[])
{
  DBusConnection *connection;
  GMainLoop *loop;
  GVfsDaemon *daemon;
  DBusError derror;
  GMountSource *mount_source;
  GMountSpec *mount_spec;
  GError *error;
  int res;

  dbus_threads_init_default ();
  g_thread_init (NULL);
  g_type_init ();

  g_vfs_register_backend (G_TYPE_VFS_BACKEND_SMB_BROWSE, "smb-network");
  g_vfs_register_backend (G_TYPE_VFS_BACKEND_SMB_BROWSE, "smb-server");
  
  dbus_error_init (&derror);
  connection = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (connection == NULL)
    {
      g_print ("Error connecting dbus: %s\n", derror.message);
      dbus_error_free (&derror);
      return 1;
    }
  
  if (argc > 1 && strcmp (argv[1], "--mount") == 0)
    {
      if (argc < 4)
	{
	  g_print ("Args: --mount dbus-id object_path\n");
	  return 1;
	}

      mount_source = g_mount_source_new_dbus (argv[2], argv[3], NULL);
    }
  else
    {
      if (argc > 1)
	{
	  mount_spec = g_mount_spec_new ("smb-server");
	  g_mount_spec_set (mount_spec, "server", argv[1]);
	}
      else
	mount_spec = g_mount_spec_new ("smb-network");

      mount_source = g_mount_source_new_null (mount_spec);
      g_mount_spec_unref (mount_spec);
    }

  res = dbus_bus_request_name (connection,
			       "org.gtk.vfs.mountpoint.smb_browse",
			       0, &derror);

  error = NULL;
  
  if (res != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      if (res == -1)
	_g_error_from_dbus (&derror, &error);
      else
	g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_IO,
		     "smb-browser already running");
      g_mount_source_failed (mount_source, error);
      return 1;
    }
  
  daemon = g_vfs_daemon_new (FALSE, FALSE);
  if (daemon == NULL)
    {
      g_set_error (&error, G_FILE_ERROR, G_FILE_ERROR_IO,
		   "error starting mount daemon");
      g_mount_source_failed (mount_source, error);
      return 1;
    }

  g_vfs_daemon_initiate_mount (daemon, mount_source);
  g_object_unref (mount_source);
  
  loop = g_main_loop_new (NULL, FALSE);

  g_print ("Entering mainloop\n");
  g_main_loop_run (loop);
  
  return 0;
}
