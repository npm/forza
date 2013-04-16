#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <uv.h>

#include <estragon.h>

char hostname[512];
uv_tcp_t client;
uv_loop_t* loop;
static uv_connect_t connect_req;
estragon_connect_cb connect_cb;

char* estragon__json_escape(char* string) {
  char* result = malloc((2 * strlen(string) + 1) * sizeof(char));
  result[0] = '\0';

  while (*string != '\0') {
    switch (*string) {
      case '\\': strcat(result, "\\\\"); break;
      case '"':  strcat(result, "\\\""); break;
      case '/':  strcat(result, "\\/");  break;
      case '\b': strcat(result, "\\b");  break;
      case '\f': strcat(result, "\\f"); break;
      case '\n': strcat(result, "\\n"); break;
      case '\r': strcat(result, "\\r"); break;
      case '\t': strcat(result, "\\t"); break;
      default:   strncat(result, string, 1); break;
    }
    ++string;
  }

  return result;
}

void estragon__on_connect(uv_connect_t* req, int status) {
  connect_cb(status);
}

void estragon_connect(char* host, int port, estragon_connect_cb connect_cb_) {
  struct sockaddr_in addr = uv_ip4_addr(host, port);

  loop = uv_default_loop();

  /* Get the hostname so that it can be provided to the server */
  gethostname(hostname, sizeof(hostname));

  connect_cb = connect_cb_;
  /* Set up a TCP keepalive connection to the godot server */
  uv_tcp_init(loop, &client);
  uv_tcp_keepalive(&client, 1, 180);
  uv_tcp_connect(&connect_req, &client, addr, estragon__on_connect);
}

char* estragon__make_json(char* service, char* state, char* description, double value) {
  char* json_buf = malloc(sizeof(char) * 1024);
  /* We PROBABLY won't ever need to send more than 1kb of JSON at one time. */
  /* TODO: Revisit this decision. */
  service = estragon__json_escape(service);
  state = estragon__json_escape(state);
  description = estragon__json_escape(description);

  snprintf(json_buf, 1024,
      "{"
      "\"host\":\"%s\","
      "\"service\":\"%s\","
      "\"state\":\"%s\","
      "\"description\":\"%s\","
      "\"time\":%li,"
      "\"metric\":\"%f\","
      "\"ttl\":\"15\""
      "}\n", hostname, service, state, description, time(NULL), value);

  free(service);
  free(state);
  free(description);

  return json_buf;
}

void estragon__after_write(uv_write_t* req, int status) {
  if (status) {
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(uv_last_error(loop)));
  }
  free(req);
}

void estragon_send(char* service, char* state, char* description, double value) {
  int r;
  uv_buf_t send_buf;
  uv_stream_t *stream;
  uv_write_t *write_req;
  char *json_data = estragon__make_json(service, state, description, value);

  write_req = malloc(sizeof *write_req);
  send_buf = uv_buf_init(json_data, sizeof(char) * strlen(json_data));

  stream = connect_req.handle;

  r = uv_write(write_req, stream, &send_buf, 1, estragon__after_write);
  if (r) {
    fprintf(stderr, "uv_write: %s\n", uv_strerror(uv_last_error(loop)));
    return;
  }
  free(json_data);
}

