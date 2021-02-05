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

#include "jerryscript-debugger-transport.h"
#include "jerryscript-ext/debugger.h"
#include "jext-common.h"

#if (defined(JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)) && !defined _WIN32

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/* Max size of configuration string */
#define CONFIG_SIZE (255)

/**
 * Implementation of transport over serial connection.
 */
typedef struct
{
  jerry_debugger_transport_header_t header; /**< transport header */
  int fd; /**< file descriptor */
} jerryx_debugger_transport_serial_t;

/**
 * Configure parameters for a serial port.
 */
typedef struct
{
  char *device_id;
  uint32_t baud_rate; /**< specify the rate at which bits are transmitted for the serial interface */
  uint32_t data_bits; /**< specify the number of data bits to transmit over the serial interface */
  char parity; /**< specify how you want to check parity bits in the data bits transmitted via the serial port */
  uint32_t stop_bits; /**< specify the number of bits used to indicate the end of a byte. */
} jerryx_debugger_transport_serial_config_t;

/**
 * Correctly close a file descriptor.
 */
static inline void
jerryx_debugger_serial_close_fd (int fd) /**< file descriptor to close */
{
  if (close (fd) != 0)
  {
    JERRYX_ERROR_MSG ("Error while closing the file descriptor: %d\n", errno);
  }
} /* jerryx_debugger_serial_close_fd */

/**
 * Set a file descriptor to blocking or non-blocking mode.
 *
 * @return true if everything is ok
 *         false if there was an error
 **/
static bool
jerryx_debugger_serial_set_blocking (int fd, bool blocking)
{
  /* Save the current flags */
  int flags = fcntl (fd, F_GETFL, 0);
  if (flags == -1)
  {
    JERRYX_ERROR_MSG ("Error %d during get flags from file descriptor\n", errno);
    return false;
  }

  if (blocking)
  {
    flags &= ~O_NONBLOCK;
  }
  else
  {
    flags |= O_NONBLOCK;
  }

  if (fcntl (fd, F_SETFL, flags) == -1)
  {
    JERRYX_ERROR_MSG ("Error %d during set flags from file descriptor\n", errno);
    return false;
  }

  return true;
} /* jerryx_debugger_serial_set_blocking */

/**
 * Configure the file descriptor used by the serial communcation.
 *
 * @return true if everything is ok
 *         false if there was an error
 */
static inline bool
jerryx_debugger_serial_configure_attributes (int fd, jerryx_debugger_transport_serial_config_t serial_config)
{
  struct termios options;
  memset (&options, 0, sizeof (options));

  /* Get the parameters associated with the file descriptor */
  if (tcgetattr (fd, &options) != 0)
  {
    JERRYX_ERROR_MSG ("Error %d from tggetattr\n", errno);
    return false;
  }

  /* Set the input and output baud rates */
  cfsetispeed (&options, serial_config.baud_rate);
  cfsetospeed (&options, serial_config.baud_rate);

  /* Set the control modes */
  options.c_cflag &= (uint32_t) ~CSIZE; // character size mask
  options.c_cflag |= (CLOCAL | CREAD); // ignore modem control lines and enable the receiver

  switch (serial_config.data_bits)
  {
    case 5:
    {
      options.c_cflag |= CS5; // set character size mask to 5-bit chars
      break;
    }
    case 6:
    {
      options.c_cflag |= CS6; // set character size mask to 6-bit chars
      break;
    }
    case 7:
    {
      options.c_cflag |= CS7; // set character size mask to 7-bit chars
      break;
    }
    case 8:
    {
      options.c_cflag |= CS8; // set character size mask to 8-bit chars
      break;
    }
    default:
    {
      JERRYX_ERROR_MSG ("Unsupported data bits: %d\n", serial_config.data_bits);
      return false;
    }
  }

  switch (serial_config.parity)
  {
    case 'N':
    {
      options.c_cflag &= (unsigned int) ~(PARENB | PARODD);
      break;
    }
    case 'O':
    {
      options.c_cflag |= PARENB;
      options.c_cflag |= PARODD;
      break;
    }
    case 'E':
    {
      options.c_cflag |= PARENB;
      options.c_cflag |= PARODD;
      break;
    }
    default:
    {
      JERRYX_ERROR_MSG ("Unsupported parity: %c\n", serial_config.parity);
      return false;
    }
  }

  switch (serial_config.stop_bits)
  {
    case 1:
    {
      options.c_cflag &= (uint32_t) ~CSTOPB; // set 1 stop bits
      break;
    }
    case 2:
    {
      options.c_cflag |= CSTOPB; // set 2 stop bits
      break;
    }
    default:
    {
      JERRYX_ERROR_MSG ("Unsupported stop bits: %d\n", serial_config.stop_bits);
      return false;
    }
  }

  /* Set the input modes */
  options.c_iflag &= (uint32_t) ~IGNBRK; // disable break processing
  options.c_iflag &= (uint32_t) ~(IXON | IXOFF | IXANY); // disable xon/xoff ctrl

  /* Set the output modes: no remapping, no delays */
  options.c_oflag = 0;

  /* Set the local modes: no signaling chars, no echo, no canoncial processing */
  options.c_lflag = 0;

  /* Read returns when at least one byte of data is available. */
  options.c_cc[VMIN] = 1; // read block
  options.c_cc[VTIME] = 5; // 0.5 seconds read timeout

  /* Set the parameters associated with the file descriptor */
  if (tcsetattr (fd, TCSANOW, &options) != 0)
  {
    JERRYX_ERROR_MSG ("Error %d from tcsetattr", errno);
    return false;
  }

  /* Flushes both data received but not read, and data written but not transmitted */
  if (tcflush (fd, TCIOFLUSH) != 0)
  {
    JERRYX_ERROR_MSG ("Error %d in tcflush() :%s\n", errno, strerror (errno));
    jerryx_debugger_serial_close_fd (fd);
    return false;
  }

  return true;
} /* jerryx_debugger_serial_configure_attributes */

/**
 * Close a serial connection.
 */
static void
jerryx_debugger_serial_close (jerry_debugger_transport_header_t *header_p) /**< serial implementation */
{
  JERRYX_ASSERT (!jerry_debugger_transport_is_connected ());

  jerryx_debugger_transport_serial_t *serial_p = (jerryx_debugger_transport_serial_t *) header_p;

  JERRYX_DEBUG_MSG ("Serial connection closed.\n");

  jerryx_debugger_serial_close_fd (serial_p->fd);

  jerry_heap_free ((void *) header_p, sizeof (jerryx_debugger_transport_serial_t));
} /* jerryx_debugger_serial_close */

/**
 * Send data over a serial connection.
 *
 * @return true - if the data has been sent successfully
 *         false - otherwise
 */
static bool
jerryx_debugger_serial_send (jerry_debugger_transport_header_t *header_p, /**< serial implementation */
                             uint8_t *message_p, /**< message to be sent */
                             size_t message_length) /**< message length in bytes */
{
  JERRYX_ASSERT (jerry_debugger_transport_is_connected ());

  jerryx_debugger_transport_serial_t *serial_p = (jerryx_debugger_transport_serial_t *) header_p;

  do
  {
    ssize_t sent_bytes = write (serial_p->fd, message_p, message_length);

    if (sent_bytes < 0)
    {
      if (errno == EWOULDBLOCK)
      {
        continue;
      }

      JERRYX_ERROR_MSG ("Error: write to file descriptor: %d\n", errno);
      jerry_debugger_transport_close ();
      return false;
    }

    message_p += sent_bytes;
    message_length -= (size_t) sent_bytes;
  } while (message_length > 0);

  return true;
} /* jerryx_debugger_serial_send */

/**
 * Receive data from a serial connection.
 */
static bool
jerryx_debugger_serial_receive (jerry_debugger_transport_header_t *header_p, /**< serial implementation */
                                jerry_debugger_transport_receive_context_t *receive_context_p) /**< receive context */
{
  jerryx_debugger_transport_serial_t *serial_p = (jerryx_debugger_transport_serial_t *) header_p;

  uint8_t *buffer_p = receive_context_p->buffer_p + receive_context_p->received_length;
  size_t buffer_size = JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE - receive_context_p->received_length;

  ssize_t length = read (serial_p->fd, buffer_p, buffer_size);

  if (length <= 0)
  {
    if (errno != EWOULDBLOCK || length == 0)
    {
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
} /* jerryx_debugger_serial_receive */

/**
 * Create a serial connection.
 *
 * @return true if successful,
 *         false otherwise
 */
bool
jerryx_debugger_serial_create (const char *config) /**< specify the configuration */
{
  /* Parse the configuration string */
  char tmp_config[CONFIG_SIZE];
  strncpy (tmp_config, config, CONFIG_SIZE);
  jerryx_debugger_transport_serial_config_t serial_config;

  char *token = strtok (tmp_config, ",");
  serial_config.device_id = token ? token : "/dev/ttyS0";
  serial_config.baud_rate = (token = strtok (NULL, ",")) ? (uint32_t) strtoul (token, NULL, 10) : 115200;
  serial_config.data_bits = (token = strtok (NULL, ",")) ? (uint32_t) strtoul (token, NULL, 10) : 8;
  serial_config.parity = (token = strtok (NULL, ",")) ? token[0] : 'N';
  serial_config.stop_bits = (token = strtok (NULL, ",")) ? (uint32_t) strtoul (token, NULL, 10) : 1;

  int fd = open (serial_config.device_id, O_RDWR);

  if (fd < 0)
  {
    JERRYX_ERROR_MSG ("Error %d opening %s: %s", errno, serial_config.device_id, strerror (errno));
    return false;
  }

  if (!jerryx_debugger_serial_configure_attributes (fd, serial_config))
  {
    jerryx_debugger_serial_close_fd (fd);
    return false;
  }

  JERRYX_DEBUG_MSG ("Waiting for client connection\n");

  /* Client will sent a 'c' char to initiate the connection. */
  uint8_t conn_char;
  ssize_t t = read (fd, &conn_char, 1);
  if (t != 1 || conn_char != 'c' || !jerryx_debugger_serial_set_blocking (fd, false))
  {
    return false;
  }

  JERRYX_DEBUG_MSG ("Client connected\n");

  size_t size = sizeof (jerryx_debugger_transport_serial_t);

  jerry_debugger_transport_header_t *header_p = (jerry_debugger_transport_header_t *) jerry_heap_alloc (size);

  if (!header_p)
  {
    jerryx_debugger_serial_close_fd (fd);
    return false;
  }

  header_p->close = jerryx_debugger_serial_close;
  header_p->send = jerryx_debugger_serial_send;
  header_p->receive = jerryx_debugger_serial_receive;

  ((jerryx_debugger_transport_serial_t *) header_p)->fd = fd;

  jerry_debugger_transport_add (header_p,
                                0,
                                JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE,
                                0,
                                JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE);

  return true;
} /* jerryx_debugger_serial_create */

#else /* !(defined (JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)) || _WIN32 */
/**
 * Dummy function when debugger is disabled.
 *
 * @return false
 */
bool
jerryx_debugger_serial_create (const char *config)
{
  JERRYX_UNUSED (config);
  return false;
} /* jerryx_debugger_serial_create */

#endif /* (defined (JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)) && !defined _WIN32 */
