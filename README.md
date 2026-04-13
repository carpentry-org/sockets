# socket

A networking library for Carp with TCP, UDP, Unix domain sockets, buffered I/O,
and I/O multiplexing.

## Installation

```clojure
(load "git@github.com:carpentry-org/socket@0.1.3")
```

## Usage

### TCP Server

```clojure
(match (TcpListener.bind "0.0.0.0" 8080)
  (Result.Success listener)
    (do
      (TcpListener.while-accept &listener client
        (match (TcpStream.read &client)
          (Result.Success msg) (ignore (TcpStream.send &client &msg))
          _ ()))
      (TcpListener.close listener))
  (Result.Error e) (IO.errorln &e))
```

### TCP Client

```clojure
(match (TcpStream.connect "example.com" 80)
  (Result.Success s)
    (do
      (ignore (TcpStream.send &s "GET / HTTP/1.0\r\n\r\n"))
      (match (TcpStream.read &s)
        (Result.Success data) (println* &data)
        _ ())
      (TcpStream.close s))
  (Result.Error e) (IO.errorln &e))
```

### Buffered I/O

Wrap any `TcpStream` in a `BufReader` for line-oriented reading:

```clojure
(match (TcpStream.connect "example.com" 80)
  (Result.Success s)
    (let [br (TcpStream.buffered s)]
      (do
        (BufReader.write &br "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
        (ignore (BufReader.flush &br))
        (match (BufReader.read-line &br)
          (Result.Success line) (println* &line)
          _ ())
        (BufReader.delete br)))
  _ ())
```

### UDP

```clojure
(match (UdpSocket.bind "0.0.0.0" 9999)
  (Result.Success sock)
    (do
      (ignore (UdpSocket.send-to &sock "127.0.0.1" 9999 &data))
      (match (UdpSocket.recv-from &sock)
        (Result.Success p)
          (println* "from " (Pair.b &p) ": " (Array.length (Pair.a &p)) " bytes")
        _ ())
      (UdpSocket.close sock))
  _ ())
```

### Unix Domain Sockets

```clojure
(match (UnixListener.bind "/tmp/my.sock")
  (Result.Success listener)
    (do
      (UnixListener.while-accept &listener client
        (ignore (UnixStream.send &client "hello")))
      (UnixListener.close listener))
  _ ())
```

### Non-blocking I/O

`TcpStream` ships with a small set of non-blocking primitives for use with
the event-loop pattern. Put a stream into non-blocking mode with
`set-nonblocking`, then use `send-nb` and `read-append-nb` instead of the
blocking variants:

```clojure
(TcpStream.set-nonblocking &client)

; send-nb returns the number of bytes actually written. A successful 0
; means the kernel buffer is full and you should wait for the next
; writable event before retrying.
(let [pos 0]
  (match (TcpStream.send-nb &client &payload pos)
    (Result.Success n) (set! pos (+ pos n))
    (Result.Error e) (IO.errorln &e)))

; read-append-nb appends whatever is buffered into `buf`. Returns one of:
;   > 0                       bytes appended
;   0                         peer closed cleanly
;   TcpStream.read-blocked    socket has no data right now
(match (TcpStream.read-append-nb &client &buf)
  (Result.Success n)
    (cond
      (= n TcpStream.read-blocked) ; wait for next readable event
      (= n 0)                      ; peer closed
      true                         ; got n bytes, parse them
      )
  (Result.Error e) (IO.errorln &e))
```

These pair naturally with `Poll`. Register write interest after a partial
send, switch back to read interest once the buffer drains, and so on. The
[web](https://github.com/carpentry-org/web) framework's event loop is a
worked example.

### I/O Multiplexing

Monitor multiple sockets for readiness using kqueue (macOS) or epoll (Linux):

```clojure
(match (Poll.create)
  (Result.Success poll)
    (do
      (ignore (Poll.add-read &poll &listener))
      (match (Poll.wait &poll 1000)
        (Result.Success events)
          (for [i 0 (Array.length &events)]
            (let [e (Array.unsafe-nth &events i)]
              (when (PollEvent.readable e)
                (println* "fd " (PollEvent.fd e) " is readable"))))
        _ ())
      (Poll.close poll))
  _ ())
```

## Types

| Type | Purpose |
|------|---------|
| `TcpListener` | Bind, listen, accept TCP connections |
| `TcpStream` | Connected TCP socket (send, read, keep-alive) |
| `UdpSocket` | UDP datagram socket (send-to, recv-from) |
| `UnixListener` | Unix domain socket server |
| `UnixStream` | Unix domain socket connection |
| `Poll` | I/O multiplexer (kqueue/epoll) |
| `PollEvent` | Readiness notification from Poll |
| `BufReader` | Buffered I/O (from the [bufio](https://github.com/carpentry-org/bufio) library) |

All fallible operations return `(Result T String)`.

## Socket Options

```clojure
(TcpStream.set-nodelay &s)         ; disable Nagle's algorithm
(TcpStream.set-keepalive &s 1)     ; enable TCP keep-alive
(TcpStream.set-timeout &s 30)      ; read/write timeout in seconds
(TcpStream.set-linger &s 5)        ; linger on close for 5 seconds
(TcpStream.set-send-buffer &s 65536)
(TcpStream.set-recv-buffer &s 65536)
(TcpStream.connect-timeout "host" 80 5) ; connect with 5s timeout
```

## Testing

```
carp -x test/tcp_test.carp
carp -x test/unix_test.carp
carp -x test/poll_test.carp
```

<hr/>

Have fun!
