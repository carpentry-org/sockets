#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
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

Socket Socket_setup_MINUS_client_MINUS_for(String* addr, String* proto, int port) {
  Socket ret;
  struct addrinfo* result;
  struct addrinfo* rp;
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  ret.valid = getaddrinfo(*addr, *proto, &hints, &result) == 0;
  if (!ret.valid) return ret;

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    ret.socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

    ret.valid = connect(ret.socket, rp->ai_addr, rp->ai_addrlen) == 0;
    if (ret.valid) break;
    close(ret.socket);
  }

  freeaddrinfo(result);

  return ret;
}

void Socket_send(Socket* sock, String* msg) {
  send(sock->socket, *msg, strlen(*msg), 0);
}

void Socket_send_MINUS_bytes(Socket* sock, Array* msg) {
  send(sock->socket, msg->data, msg->len, 0);
}

String Socket_read(Socket* sock) {
  String buf = CARP_MALLOC(Socket_buf_MINUS_size+1);
  int r = read(sock->socket, buf, Socket_buf_MINUS_size);
  if (r == -1) {
    buf[0] = '\0';
    return buf;
  }
  buf[r] = '\0';
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

Socket Socket_copy(Socket* sock) {
  Socket res;
  res.socket = sock->socket;
  res.valid = sock->valid;
  res.them = sock->them;
  return res;
}
