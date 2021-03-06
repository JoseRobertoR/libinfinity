/* libinfinity - a GObject-based infinote implementation
 * Copyright (C) 2007-2014 Armin Burgmeier <armin@arbur.net>
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
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <libinfinity/client/infc-browser.h>
#include <libinfinity/communication/inf-communication-manager.h>
#include <libinfinity/common/inf-xmpp-connection.h>
#include <libinfinity/common/inf-tcp-connection.h>
#include <libinfinity/common/inf-ip-address.h>
#include <libinfinity/common/inf-standalone-io.h>
#include <libinfinity/common/inf-io.h>
#include <libinfinity/common/inf-protocol.h>
#include <libinfinity/common/inf-init.h>

#include <string.h>

typedef struct _InfTestBrowser InfTestBrowser;
struct _InfTestBrowser {
  InfStandaloneIo* io;
  InfXmppConnection* conn;
  InfBrowser* browser;
#ifndef G_OS_WIN32
  int input_fd;
#endif

  InfBrowserIter cwd;
};

typedef void(*InfTestBrowserCmdFunc)(InfTestBrowser*, const gchar*);

typedef struct _InfTestBrowserCmd InfTestBrowserCmd;
struct _InfTestBrowserCmd {
  const gchar* name;
  InfTestBrowserCmdFunc func;
};

static gboolean
inf_test_browser_find_node(InfTestBrowser* test,
                           const gchar* name,
                           InfBrowserIter* result_iter)
{
  InfBrowserIter iter;
  gboolean result;

  if(inf_browser_get_explored(test->browser, &test->cwd) == FALSE)
  {
    fprintf(
      stderr,
      "Directory '%s' not yet explored\n",
      inf_browser_get_node_name(test->browser, &test->cwd)
    );
  }
  else
  {
    iter = test->cwd;
    for(result = inf_browser_get_child(test->browser, &iter);
        result == TRUE;
        result = inf_browser_get_next(test->browser, &iter))
    {
      if(strcmp(inf_browser_get_node_name(test->browser, &iter), name) == 0)
      {
        *result_iter = iter;
        return TRUE;
      }
    }
  }

  return FALSE;
}

static void
inf_test_browser_cmd_ls(InfTestBrowser* test,
                        const gchar* param)
{
  InfBrowserIter iter;
  gboolean result;

  if(inf_browser_get_explored(test->browser, &test->cwd) == FALSE)
  {
    fprintf(
      stderr,
      "Directory '%s' not yet explored\n",
      inf_browser_get_node_name(test->browser, &test->cwd)
    );
  }
  else
  {
    iter = test->cwd;
    for(result = inf_browser_get_child(test->browser, &iter);
        result == TRUE;
        result = inf_browser_get_next(test->browser, &iter))
    {
      printf("%s\n", inf_browser_get_node_name(test->browser, &iter));
    }
  }
}

static void
inf_test_browser_cmd_cd(InfTestBrowser* test,
                        const gchar* param)
{
  InfBrowserIter iter;

  if(strcmp(param, "..") == 0)
  {
    iter = test->cwd;
    if(inf_browser_get_parent(test->browser, &iter) == FALSE)
    {
      fprintf(stderr, "Already at the root directory\n");
    }
    else
    {
      test->cwd = iter;
    }
  }
  else if(inf_test_browser_find_node(test, param, &iter) == FALSE)
  {
    fprintf(
      stderr,
      "Directory '%s' does not exist\n",
      param
    );
  }
  else if(inf_browser_get_explored(test->browser, &iter) == FALSE)
  {
    fprintf(
      stderr,
      "Directory '%s' not yet explored\n",
      inf_browser_get_node_name(test->browser, &iter)
    );
  }
  else
  {
    test->cwd = iter;
  }
}

static void
inf_test_browser_cmd_explore(InfTestBrowser* test,
                             const gchar* param)
{
  InfBrowserIter iter;

  if(inf_test_browser_find_node(test, param, &iter) == FALSE)
  {
    fprintf(
      stderr,
      "Directory '%s' does not exist\n",
      param
    );
  }
  else if(inf_browser_get_explored(test->browser, &iter) == TRUE)
  {
    fprintf(
      stderr,
      "Directory '%s' is already explored",
      inf_browser_get_node_name(test->browser, &iter)
    );
  }
  else
  {
    inf_browser_explore(test->browser, &iter, NULL, NULL);
  }
}

static void
inf_test_browser_cmd_create(InfTestBrowser* test,
                            const gchar* param)
{
  inf_browser_add_subdirectory(
    test->browser,
    &test->cwd,
    param,
    NULL,
    NULL,
    NULL
  );
}

static void
inf_test_browser_cmd_remove(InfTestBrowser* test,
                            const gchar* param)
{
  InfBrowserIter iter;
  if(inf_test_browser_find_node(test, param, &iter) == FALSE)
  {
    fprintf(
      stderr,
      "Directory '%s' does not exist\n",
      param
    );
  }
  else
  {
    inf_browser_remove_node(test->browser, &iter, NULL, NULL);
  }
}

static const InfTestBrowserCmd inf_test_browser_commands[] = {
  { "ls", inf_test_browser_cmd_ls },
  { "cd", inf_test_browser_cmd_cd },
  { "explore", inf_test_browser_cmd_explore },
  { "create", inf_test_browser_cmd_create },
  { "remove", inf_test_browser_cmd_remove }
};

static void
inf_test_browser_input_cb(InfNativeSocket* fd,
                          InfIoEvent io,
                          gpointer user_data)
{
  InfTestBrowser* test;
  char buffer[1024];
  char* occ;
  guint i;

  test = (InfTestBrowser*)user_data;

  if(io & INF_IO_ERROR)
  {
  }

  if(io & INF_IO_INCOMING)
  {
    if(fgets(buffer, sizeof(buffer), stdin) == NULL)
    {
      inf_standalone_io_loop_quit(test->io);
    }
    else if(strlen(buffer) != sizeof(buffer) ||
            buffer[sizeof(buffer)-2] == '\n')
    {
      buffer[strlen(buffer)-1] = '\0';

      /* Read entire line */
      occ = strchr(buffer, ' ');
      if(occ != NULL)
      {
        *occ = '\0';
        ++ occ;
      }

      for(i = 0; i < G_N_ELEMENTS(inf_test_browser_commands); ++ i)
      {
        if(strcmp(inf_test_browser_commands[i].name, buffer) == 0)
        {
          inf_test_browser_commands[i].func(test, occ);
          break;
        }
      }

      if(i == G_N_ELEMENTS(inf_test_browser_commands))
      {
        fprintf(stderr, "'%s': Command not found\n", buffer);
      }
    }
  }
}

static void
inf_test_browser_error_cb(InfcBrowser* browser,
                          GError* error,
                          gpointer user_data)
{
  fprintf(stderr, "Connection error: %s\n", error->message);
}

static void
inf_test_browser_notify_status_cb(GObject* object,
                                  GParamSpec* pspec,
                                  gpointer user_data)
{
  InfTestBrowser* test;
  InfBrowserStatus status;

  test = (InfTestBrowser*)user_data;
  g_object_get(G_OBJECT(test->browser), "status", &status, NULL);

  if(status == INF_BROWSER_OPEN)
  {
    printf("Connection established\n");

#ifndef G_OS_WIN32
    inf_io_add_watch(
      INF_IO(test->io),
      &test->input_fd,
      INF_IO_INCOMING | INF_IO_ERROR,
      inf_test_browser_input_cb,
      test,
      NULL
    );
#endif

    /* Explore root node */
    inf_browser_get_root(test->browser, &test->cwd);
    inf_browser_explore(test->browser, &test->cwd, NULL, NULL);
  }

  if(status == INF_BROWSER_CLOSED)
  {
    if(inf_standalone_io_loop_running(test->io))
      inf_standalone_io_loop_quit(test->io);
  }
}

int
main(int argc, char* argv[])
{
  InfTestBrowser test;
  InfIpAddress* address;
  InfCommunicationManager* manager;
  InfTcpConnection* tcp_conn;
  GError* error;

  error = NULL;
  if(!inf_init(&error))
  {
    fprintf(stderr, "%s\n", error->message);
    return 1;
  }

  test.io = inf_standalone_io_new();
#ifndef G_OS_WIN32
  test.input_fd = STDIN_FILENO;
#endif
  address = inf_ip_address_new_loopback4();

  tcp_conn =
    inf_tcp_connection_new_and_open(INF_IO(test.io), address, inf_protocol_get_default_port(), &error);

  inf_ip_address_free(address);

  if(tcp_conn == NULL)
  {
    fprintf(stderr, "Could not open TCP connection: %s\n", error->message);
    g_error_free(error);
  }
  else
  {
    test.conn = inf_xmpp_connection_new(
      tcp_conn,
      INF_XMPP_CONNECTION_CLIENT,
      NULL,
      "localhost",
      INF_XMPP_CONNECTION_SECURITY_BOTH_PREFER_TLS,
      NULL,
      NULL,
      NULL
    );

    g_object_unref(G_OBJECT(tcp_conn));

    manager = inf_communication_manager_new();
    test.browser = INF_BROWSER(
      infc_browser_new(
        INF_IO(test.io),
        manager,
        INF_XML_CONNECTION(test.conn)
      )
    );

    g_signal_connect_after(
      G_OBJECT(test.browser),
      "notify::status",
      G_CALLBACK(inf_test_browser_notify_status_cb),
      &test
    );

    g_signal_connect(
      G_OBJECT(test.browser),
      "error",
      G_CALLBACK(inf_test_browser_error_cb),
      &test
    );

    inf_standalone_io_loop(test.io);
    g_object_unref(G_OBJECT(manager));
    g_object_unref(G_OBJECT(test.browser));

    /* TODO: Wait until the XMPP connection is in status closed */
    g_object_unref(G_OBJECT(test.conn));
  }

  g_object_unref(G_OBJECT(test.io));
  return 0;
}

/* vim:set et sw=2 ts=2: */
