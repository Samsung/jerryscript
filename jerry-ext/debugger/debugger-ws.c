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

#include "debugger-sha1.h"
#include "jerryscript-ext/debugger.h"
#include "jext-common.h"

#if defined(JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)

/* JerryScript debugger protocol is a simplified version of RFC-6455 (WebSockets). */

/**
 * Last fragment of a Websocket package.
 */
#define JERRYX_DEBUGGER_WEBSOCKET_FIN_BIT 0x80

/**
 * Masking-key is available.
 */
#define JERRYX_DEBUGGER_WEBSOCKET_MASK_BIT 0x80

/**
 * Opcode type mask.
 */
#define JERRYX_DEBUGGER_WEBSOCKET_OPCODE_MASK 0x0fu

/**
 * Packet length mask.
 */
#define JERRYX_DEBUGGER_WEBSOCKET_LENGTH_MASK 0x7fu

/**
 * Size of websocket header size.
 */
#define JERRYX_DEBUGGER_WEBSOCKET_HEADER_SIZE 2

/**
 * Payload mask size in bytes of a websocket package.
 */
#define JERRYX_DEBUGGER_WEBSOCKET_MASK_SIZE 4

/**
 * Maximum message size with 1 byte size field.
 */
#define JERRYX_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX 125

/**
 * WebSocket opcode types.
 */
typedef enum
{
  JERRYX_DEBUGGER_WEBSOCKET_TEXT_FRAME = 1, /**< text frame */
  JERRYX_DEBUGGER_WEBSOCKET_BINARY_FRAME = 2, /**< binary frame */
  JERRYX_DEBUGGER_WEBSOCKET_CLOSE_CONNECTION = 8, /**< close connection */
  JERRYX_DEBUGGER_WEBSOCKET_PING = 9, /**< ping (keep alive) frame */
  JERRYX_DEBUGGER_WEBSOCKET_PONG = 10, /**< reply to ping frame */
} jerryx_websocket_opcode_type_t;

/**
 * Header for incoming packets.
 */
typedef struct
{
  uint8_t ws_opcode; /**< websocket opcode */
  uint8_t size; /**< size of the message */
  uint8_t mask[4]; /**< mask bytes */
} jerryx_websocket_receive_header_t;

/**
 * Convert a 6-bit value to a Base64 character.
 *
 * @return Base64 character
 */
static uint8_t
jerryx_to_base64_character (uint8_t value) /**< 6-bit value */
{
  if (value < 26)
  {
    return (uint8_t) (value + 'A');
  }

  if (value < 52)
  {
    return (uint8_t) (value - 26 + 'a');
  }

  if (value < 62)
  {
    return (uint8_t) (value - 52 + '0');
  }

  if (value == 62)
  {
    return (uint8_t) '+';
  }

  return (uint8_t) '/';
} /* jerryx_to_base64_character */

/**
 * Encode a byte sequence into Base64 string.
 */
static void
jerryx_to_base64 (const uint8_t *source_p, /**< source data */
                  uint8_t *destination_p, /**< destination buffer */
                  size_t length) /**< length of source, must be divisible by 3 */
{
  while (length >= 3)
  {
    uint8_t value = (source_p[0] >> 2);
    destination_p[0] = jerryx_to_base64_character (value);

    value = (uint8_t) (((source_p[0] << 4) | (source_p[1] >> 4)) & 0x3f);
    destination_p[1] = jerryx_to_base64_character (value);

    value = (uint8_t) (((source_p[1] << 2) | (source_p[2] >> 6)) & 0x3f);
    destination_p[2] = jerryx_to_base64_character (value);

    value = (uint8_t) (source_p[2] & 0x3f);
    destination_p[3] = jerryx_to_base64_character (value);

    source_p += 3;
    destination_p += 4;
    length -= 3;
  }
} /* jerryx_to_base64 */

/**
 * Process WebSocket handshake.
 *
 * @return true - if the handshake was completed successfully
 *         false - otherwise
 */
static bool
jerryx_process_handshake (uint8_t *request_buffer_p) /**< temporary buffer */
{
  size_t request_buffer_size = 1024;
  uint8_t *request_end_p = request_buffer_p;

  /* Buffer request text until the double newlines are received. */
  while (true)
  {
    jerry_debugger_transport_receive_context_t context;
    if (!jerry_debugger_transport_receive (&context))
    {
      JERRYX_ASSERT (!jerry_debugger_transport_is_connected ());
      return false;
    }

    if (context.message_p == NULL)
    {
      jerry_debugger_transport_sleep ();
      continue;
    }

    size_t length = request_buffer_size - 1u - (size_t) (request_end_p - request_buffer_p);

    if (length < context.message_length)
    {
      JERRYX_ERROR_MSG ("Handshake buffer too small.\n");
      return false;
    }

    /* Both stream and datagram packets are supported. */
    memcpy (request_end_p, context.message_p, context.message_length);

    jerry_debugger_transport_receive_completed (&context);

    request_end_p += (size_t) context.message_length;
    *request_end_p = 0;

    if (request_end_p > request_buffer_p + 4 && memcmp (request_end_p - 4, "\r\n\r\n", 4) == 0)
    {
      break;
    }
  }

  /* Check protocol. */
  const char get_text[] = "GET /jerry-debugger";
  size_t text_len = sizeof (get_text) - 1;

  if ((size_t) (request_end_p - request_buffer_p) < text_len || memcmp (request_buffer_p, get_text, text_len) != 0)
  {
    JERRYX_ERROR_MSG ("Invalid handshake format.\n");
    return false;
  }

  uint8_t *websocket_key_p = request_buffer_p + text_len;

  const char key_text[] = "Sec-WebSocket-Key:";
  text_len = sizeof (key_text) - 1;

  while (true)
  {
    if ((size_t) (request_end_p - websocket_key_p) < text_len)
    {
      JERRYX_ERROR_MSG ("Sec-WebSocket-Key not found.\n");
      return false;
    }

    if (websocket_key_p[0] == 'S' && websocket_key_p[-1] == '\n' && websocket_key_p[-2] == '\r'
        && memcmp (websocket_key_p, key_text, text_len) == 0)
    {
      websocket_key_p += text_len;
      break;
    }

    websocket_key_p++;
  }

  /* String terminated by double newlines. */

  while (*websocket_key_p == ' ')
  {
    websocket_key_p++;
  }

  uint8_t *websocket_key_end_p = websocket_key_p;

  while (*websocket_key_end_p > ' ')
  {
    websocket_key_end_p++;
  }

  /* Since the request_buffer_p is not needed anymore it can
   * be reused for storing the SHA-1 key and Base64 string. */

  const size_t sha1_length = 20;

  jerryx_debugger_compute_sha1 (websocket_key_p,
                                (size_t) (websocket_key_end_p - websocket_key_p),
                                (const uint8_t *) "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
                                36,
                                request_buffer_p);

  /* The SHA-1 key is 20 bytes long but jerryx_to_base64 expects
   * a length divisible by 3 so an extra 0 is appended at the end. */
  request_buffer_p[sha1_length] = 0;

  jerryx_to_base64 (request_buffer_p, request_buffer_p + sha1_length + 1, sha1_length + 1);

  /* Last value must be replaced by equal sign. */

  const uint8_t response_prefix[] =
    "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";

  if (!jerry_debugger_transport_send (response_prefix, sizeof (response_prefix) - 1)
      || !jerry_debugger_transport_send (request_buffer_p + sha1_length + 1, 27))
  {
    return false;
  }

  const uint8_t response_suffix[] = "=\r\n\r\n";
  return jerry_debugger_transport_send (response_suffix, sizeof (response_suffix) - 1);
} /* jerryx_process_handshake */

/**
 * Close a tcp connection.
 */
static void
jerryx_debugger_ws_close (jerry_debugger_transport_header_t *header_p) /**< tcp implementation */
{
  JERRYX_ASSERT (!jerry_debugger_transport_is_connected ());

  jerry_heap_free ((void *) header_p, sizeof (jerry_debugger_transport_header_t));
} /* jerryx_debugger_ws_close */

/**
 * Send data over a websocket connection.
 *
 * @return true - if the data has been sent successfully
 *         false - otherwise
 */
static bool
jerryx_debugger_ws_send (jerry_debugger_transport_header_t *header_p, /**< tcp implementation */
                         uint8_t *message_p, /**< message to be sent */
                         size_t message_length) /**< message length in bytes */
{
  JERRYX_ASSERT (message_length <= JERRYX_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX);

  message_p[-2] = JERRYX_DEBUGGER_WEBSOCKET_FIN_BIT | JERRYX_DEBUGGER_WEBSOCKET_BINARY_FRAME;
  message_p[-1] = (uint8_t) message_length;

  return header_p->next_p->send (header_p->next_p, message_p - 2, message_length + 2);
} /* jerryx_debugger_ws_send */

/**
 * Receive data from a websocket connection.
 */
static bool
jerryx_debugger_ws_receive (jerry_debugger_transport_header_t *header_p, /**< tcp implementation */
                            jerry_debugger_transport_receive_context_t *receive_context_p) /**< receive context */
{
  if (!header_p->next_p->receive (header_p->next_p, receive_context_p))
  {
    return false;
  }

  if (receive_context_p->message_p == NULL)
  {
    return true;
  }

  size_t message_total_length = receive_context_p->message_total_length;

  if (message_total_length == 0)
  {
    /* Byte stream. */
    if (receive_context_p->message_length < sizeof (jerryx_websocket_receive_header_t))
    {
      receive_context_p->message_p = NULL;
      return true;
    }
  }
  else
  {
    /* Datagram packet. */
    JERRYX_ASSERT (receive_context_p->message_length >= sizeof (jerryx_websocket_receive_header_t));
  }

  uint8_t *message_p = receive_context_p->message_p;

  if ((message_p[0] & ~JERRYX_DEBUGGER_WEBSOCKET_OPCODE_MASK) != JERRYX_DEBUGGER_WEBSOCKET_FIN_BIT
      || (message_p[1] & JERRYX_DEBUGGER_WEBSOCKET_LENGTH_MASK) > JERRYX_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX
      || !(message_p[1] & JERRYX_DEBUGGER_WEBSOCKET_MASK_BIT))
  {
    JERRYX_ERROR_MSG ("Unsupported Websocket message.\n");
    jerry_debugger_transport_close ();
    return false;
  }

  if ((message_p[0] & JERRYX_DEBUGGER_WEBSOCKET_OPCODE_MASK) != JERRYX_DEBUGGER_WEBSOCKET_BINARY_FRAME)
  {
    JERRYX_ERROR_MSG ("Unsupported Websocket opcode.\n");
    jerry_debugger_transport_close ();
    return false;
  }

  size_t message_length = (size_t) (message_p[1] & JERRYX_DEBUGGER_WEBSOCKET_LENGTH_MASK);

  if (message_total_length == 0)
  {
    size_t new_total_length = message_length + sizeof (jerryx_websocket_receive_header_t);

    /* Byte stream. */
    if (receive_context_p->message_length < new_total_length)
    {
      receive_context_p->message_p = NULL;
      return true;
    }

    receive_context_p->message_total_length = new_total_length;
  }
  else
  {
    /* Datagram packet. */
    JERRYX_ASSERT (receive_context_p->message_length == (message_length + sizeof (jerryx_websocket_receive_header_t)));
  }

  message_p += sizeof (jerryx_websocket_receive_header_t);

  receive_context_p->message_p = message_p;
  receive_context_p->message_length = message_length;

  /* Unmask data bytes. */
  const uint8_t *mask_p = message_p - JERRYX_DEBUGGER_WEBSOCKET_MASK_SIZE;
  const uint8_t *mask_end_p = message_p;
  const uint8_t *message_end_p = message_p + message_length;

  while (message_p < message_end_p)
  {
    /* Invert certain bits with xor operation. */
    *message_p = *message_p ^ *mask_p;

    message_p++;
    mask_p++;

    if (JERRY_UNLIKELY (mask_p >= mask_end_p))
    {
      mask_p -= JERRYX_DEBUGGER_WEBSOCKET_MASK_SIZE;
    }
  }

  return true;
} /* jerryx_debugger_ws_receive */

/**
 * Initialize the websocket transportation layer.
 *
 * @return true - if the connection succeeded
 *         false - otherwise
 */
bool
jerryx_debugger_ws_create (void)
{
  bool is_handshake_ok = false;

  const size_t buffer_size = 1024;
  uint8_t *request_buffer_p = (uint8_t *) jerry_heap_alloc (buffer_size);

  if (!request_buffer_p)
  {
    return false;
  }

  is_handshake_ok = jerryx_process_handshake (request_buffer_p);

  jerry_heap_free ((void *) request_buffer_p, buffer_size);

  if (!is_handshake_ok && jerry_debugger_transport_is_connected ())
  {
    return false;
  }

  const size_t interface_size = sizeof (jerry_debugger_transport_header_t);
  jerry_debugger_transport_header_t *header_p = (jerry_debugger_transport_header_t *) jerry_heap_alloc (interface_size);

  if (!header_p)
  {
    return false;
  }

  header_p->close = jerryx_debugger_ws_close;
  header_p->send = jerryx_debugger_ws_send;
  header_p->receive = jerryx_debugger_ws_receive;

  jerry_debugger_transport_add (header_p,
                                JERRYX_DEBUGGER_WEBSOCKET_HEADER_SIZE,
                                JERRYX_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX,
                                JERRYX_DEBUGGER_WEBSOCKET_HEADER_SIZE + JERRYX_DEBUGGER_WEBSOCKET_MASK_SIZE,
                                JERRYX_DEBUGGER_WEBSOCKET_ONE_BYTE_LEN_MAX);

  return true;
} /* jerryx_debugger_ws_create */

#else /* !(defined (JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1)) */

/**
 * Dummy function when debugger is disabled.
 *
 * @return false
 */
bool
jerryx_debugger_ws_create (void)
{
  return false;
} /* jerryx_debugger_ws_create */

#endif /* defined (JERRY_DEBUGGER) && (JERRY_DEBUGGER == 1) */
