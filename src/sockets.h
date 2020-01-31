#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int Socket_buf_MINUS_size = 1024;

typedef struct {
  struct sockaddr_in them;
  int socket;
  bool valid;
} Socket;

bool Socket_valid_QMARK_(Socket* s) {
  return s->valid;
}

Socket Socket_setup_MINUS_client(String* addr, int port) {
  Socket ret;

  ret.socket = socket(AF_INET, SOCK_STREAM, 0);
  ret.them.sin_family = AF_INET;
  ret.them.sin_port = htons(port);
  inet_pton(AF_INET, *addr, &(ret.them.sin_addr));
  ret.valid = connect(ret.socket, (struct sockaddr*)&(ret.them), sizeof(ret.them)) == 0;

  return ret;
}

void Socket_send(Socket* sock, String* msg) {
  send(sock->socket, *msg, strlen(*msg), 0);
}

void Socket_send_MINUS_bytes(Socket* sock, Array* msg) {
  send(sock->socket, msg->data, msg->len, 0);
}

String Socket_read(Socket* sock) {
  String buf;
  int r;
  int size = 0;
  while (1) {
    String buf = realloc(buf, size+Socket_buf_MINUS_size);
    r = read(sock->socket, buf+size, Socket_buf_MINUS_size);
    if (r != Socket_buf_MINUS_size) break;
    size += Socket_buf_MINUS_size;
  }

  return buf;
}

Array Socket_read_MINUS_bytes(Socket* sock) {
  Array buf;
  buf.capacity = Socket_buf_MINUS_size;
  buf.data = CARP_MALLOC(Socket_buf_MINUS_size);
  buf.len = read(sock->socket, buf.data, Socket_buf_MINUS_size);
  return buf;
}

Socket Socket_setup_MINUS_server(String* addr, int port) {
  Socket ret;
  int opt = 1;

  ret.socket = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(ret.socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
  ret.them.sin_family = AF_INET;
  ret.them.sin_addr.s_addr = INADDR_ANY;
  ret.them.sin_port = htons(port);
  inet_pton(AF_INET, *addr, &(ret.them.sin_addr));
  ret.valid = bind(ret.socket, (struct sockaddr *)&(ret.them),  sizeof(ret.them)) == 0;

  return ret;
}

void Socket_listen(Socket* sock) {
  listen(sock->socket, 3);
}

Socket Socket_accept(Socket* sock) {
  Socket ret;
  int size = sizeof(ret.them);
  ret.socket = accept(sock->socket, (struct sockaddr *)&sock->them, (socklen_t*)&size);
  return ret;
}

void Socket_close(Socket sock) {
  close(sock.socket);
}
