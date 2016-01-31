Scalable, compact, high performance utility that either floods tons of HTTP requests as test HTTP client or accepts zillions of TCP connections and emulates on them HTTP server. Collects useful statistics. Can be great tool for HTTP server or client performance tests.

Versions 1.x.x support only client mode and traditional asynchronous architecture based on epoll().

Starting with version 2.0.0 Spate supports both client and server modes. Moreover it comes in two architecture flavours: traditional (like version 1.x.x) and SPOOL-bases, using SPOOL socket accelerator.

Please visit Wiki pages for more details: https://code.google.com/p/spate-tool/wiki/HomePage