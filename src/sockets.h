#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "carp_memory.h"
#include "core.h"

int Socket_buf_MINUS_size = 1024;

typedef struct {
  struct sockaddr_in them;
  int socket;
} Socket;

Socket Socket_setup_MINUS_client(String addr, int port) {
  Socket ret;

  ret.socket = socket(AF_INET, SOCK_STREAM, 0);
  ret.them.sin_family = AF_INET;
  ret.them.sin_port = htons(port);
  inet_pton(AF_INET, addr, &(ret.them.sin_addr));
  connect(ret.socket, (struct sockaddr*)&(ret.them), sizeof(ret.them));

  return ret;
}

void Socket_send(Socket sock, String msg) {
  send(sock.socket, msg, strlen(msg), 0);
}

String Socket_read(Socket sock) {
  String buf = CARP_MALLOC(Socket_buf_MINUS_size);
  int val = read(sock.socket, buf, Socket_buf_MINUS_size);
  return buf;
}

Socket Socket_setup_MINUS_server(String addr, int port) {
  Socket ret;
  int opt = 1;

  ret.socket = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(ret.socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
  ret.them.sin_family = AF_INET;
  ret.them.sin_addr.s_addr = INADDR_ANY;
  ret.them.sin_port = htons(port);
  inet_pton(AF_INET, addr, &(ret.them.sin_addr));
  bind(ret.socket, (struct sockaddr *)&(ret.them),  sizeof(ret.them));

  return ret;
}

void Socket_listen(Socket sock) {
  listen(sock.socket, 3);
}

Socket Socket_accept(Socket sock) {
  Socket ret;
  int size = sizeof(ret.them);
  ret.socket = accept(sock.socket, (struct sockaddr *)&sock.them, (socklen_t*)&size);
  return ret;
}

void Socket_close(Socket sock) {
  close(sock.socket);
}
