# sockets

A simple wrapper around C sockets for Carp.

## Usage

Setting up a socket can be done through `setup-server` and
`setup-client` or through the macros `with-server` and
`with-client`.

```clojure
(let [sock (Socket.setup-server @"127.0.0.1" 80)]
  ; ... work with sock
  )

(let [sock (Socket.setup-client @"127.0.0.1" 80)]
  ; ...
  )

(Socket.with-server sock @"127.0.0.1" 80
  ; use sock as above
  )

(Socket.with-client sock @"127.0.0.1" 80
  ; use sock as above
  )
```

Client sockets can `send` and `read` right away, whereas server sockets have to
`listen` and `accept` first. Just like in C!

Alternatively, you can also use the macro `with-connection` in the server, like
so:

```clojure
(Socket.with-server server @"127.0.0.1" 80
  (Socket.with-connection server client
    (send client @"nice to meet you!")
  )
)
```

If you want a server that accepts connections forever, use `while-connection`:

```clojure
(Socket.with-server server @"127.0.0.1" 80
  (Socket.while-connection server client
    (send client @"nice to meet you!")
  )
)
```

This will never terminate, unless interrupted by the user or failure.

<hr/>

Have fun!
