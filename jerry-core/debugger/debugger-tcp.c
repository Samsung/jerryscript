/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "debugger-tcp.h"
#include "jrt.h"

#ifdef JERRY_DEBUGGER

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * Implementation of transport over tcp/ip.
 */
typedef struct
{
  jerry_debugger_transport_header_t header; /**< transport header */
  int tcp_socket; /**< tcp socket */
} jerry_debugger_transport_tcp_t;

/**
 * Log tcp error message.
 */
static void
jerry_debugger_tcp_log_error (void)
{
  JERRY_ERROR_MSG ("Error: %s\n", strerror (errno));
} /* jerry_debugger_tcp_log_error */

/**
 * Close a tcp connection.
 */
static void
jerry_debugger_tcp_close (jerry_debugger_transport_header_t *header_p) /**< tcp implementation */
{
  JERRY_ASSERT (jerry_debugger_transport_is_connected ());

  jerry_debugger_transport_tcp_t *tcp_p = (jerry_debugger_transport_tcp_t *) header_p;

  JERRY_DEBUG_MSG ("TCP connection closed.\n");

  close (tcp_p->tcp_socket);

  jerry_debugger_transport_free ((void *) header_p, sizeof (jerry_debugger_transport_tcp_t));
} /* jerry_debugger_tcp_close */

/**
 * Send data over a tcp connection.
 *
 * @return true - if the data has been sent successfully
 *         false - otherwise
 */
static bool
jerry_debugger_tcp_send (jerry_debugger_transport_header_t *header_p, /**< tcp implementation */
                         uint8_t *message_p, /**< message to be sent */
                         size_t message_length) /**< message length in bytes */
{
  JERRY_ASSERT (jerry_debugger_transport_is_connected ());

  jerry_debugger_transport_tcp_t *tcp_p = (jerry_debugger_transport_tcp_t *) header_p;

  do
  {
    ssize_t sent_bytes = send (tcp_p->tcp_socket, message_p, message_length, 0);

    if (sent_bytes < 0)
    {
      if (errno == EWOULDBLOCK)
      {
        continue;
      }

      jerry_debugger_tcp_log_error ();
      jerry_debugger_transport_close ();
      return false;
    }

    message_p += sent_bytes;
    message_length -= (size_t) sent_bytes;
  }
  while (message_length > 0);

  return true;
} /* jerry_debugger_tcp_send */

/**
 * Receive data from a tcp connection.
 */
static bool
jerry_debugger_tcp_receive (jerry_debugger_transport_header_t *header_p, /**< tcp implementation */
                            jerry_debugger_transport_receive_context_t *receive_context_p) /**< receive context */
{
  jerry_debugger_transport_tcp_t *tcp_p = (jerry_debugger_transport_tcp_t *) header_p;

  uint8_t *buffer_p = receive_context_p->buffer_p + receive_context_p->received_length;
  size_t buffer_size = JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE - receive_context_p->received_length;

  ssize_t length = recv (tcp_p->tcp_socket, buffer_p, buffer_size, 0);

  if (length < 0)
  {
    if (errno != EWOULDBLOCK)
    {
      jerry_debugger_tcp_log_error ();
      jerry_debugger_transport_close ();
      return false;
    }
    length = 0;
  }

  receive_context_p->received_length += (size_t) length;

  if (receive_context_p->received_length > 0)
  {
    receive_context_p->message_p = receive_context_p->buffer_p;
    receive_context_p->message_length = receive_context_p->received_length;
  }

  return true;
} /* jerry_debugger_tcp_receive */

/**
 * Create a tcp connection.
 *
 * @return true if successful,
 *         false otherwise
 */
bool
jerry_debugger_tcp_create (uint16_t port) /**< listening port */
{
  int server_socket;
  struct sockaddr_in addr;
  socklen_t sin_size = sizeof (struct sockaddr_in);

  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if ((server_socket = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  {
    JERRY_ERROR_MSG ("Error: %s\n", strerror (errno));
    return false;
  }

  int opt_value = 1;

  if (setsockopt (server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof (int)) == -1)
  {
    close (server_socket);
    JERRY_ERROR_MSG ("Error: %s\n", strerror (errno));
    return false;
  }

  if (bind (server_socket, (struct sockaddr *)&addr, sizeof (struct sockaddr)) == -1)
  {
    close (server_socket);
    JERRY_ERROR_MSG ("Error: %s\n", strerror (errno));
    return false;
  }

  if (listen (server_socket, 1) == -1)
  {
    close (server_socket);
    JERRY_ERROR_MSG ("Error: %s\n", strerror (errno));
    return false;
  }

  JERRY_DEBUG_MSG ("Waiting for client connection\n");

  int tcp_socket = accept (server_socket, (struct sockaddr *)&addr, &sin_size);

  close (server_socket);

  if (tcp_socket == -1)
  {
    JERRY_ERROR_MSG ("Error: %s\n", strerror (errno));
    return false;
  }

  /* Set non-blocking mode. */
  int socket_flags = fcntl (tcp_socket, F_GETFL, 0);

  if (socket_flags < 0)
  {
    close (tcp_socket);
    return false;
  }

  if (fcntl (tcp_socket, F_SETFL, socket_flags | O_NONBLOCK) == -1)
  {
    close (tcp_socket);
    return false;
  }

  JERRY_DEBUG_MSG ("Connected from: %s\n", inet_ntoa (addr.sin_addr));

  size_t size = sizeof (jerry_debugger_transport_tcp_t);

  jerry_debugger_transport_header_t *header_p;
  header_p = (jerry_debugger_transport_header_t *) jerry_debugger_transport_malloc (size);

  if (!header_p)
  {
    close (tcp_socket);
    return false;
  }

  header_p->close = jerry_debugger_tcp_close;
  header_p->send = jerry_debugger_tcp_send;
  header_p->receive = jerry_debugger_tcp_receive;

  ((jerry_debugger_transport_tcp_t *) header_p)->tcp_socket = tcp_socket;

  jerry_debugger_transport_add (header_p,
                                0,
                                JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE,
                                0,
                                JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE);

  return true;
} /* jerry_debugger_tcp_create */

#endif /* JERRY_DEBUGGER */
