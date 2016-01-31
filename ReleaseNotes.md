# Spate Release Notes #

**Version 2.0.0** Two main features were introduced in the version 2.0.0:
  1. Server mode. Spate now can work either as HTTP test client or as HTTP server emulator. In the server mode Spate listens for TCP connections on one or more sockets, accepts connections and sends static HTTP responses once it gets HTTP requests.
  1. SPOOL support. Spate now can be compiled with SPOOL interface support (https://code.google.com/p/socket-pool/). SPOOL is a Kernel module that provides an alter socket interface that helps increase TCP performance when many simultaneous connections are processed.