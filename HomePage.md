# Introduction #

Welcome to Spate - the new generation of HTTP performance testing tool.


# Details #

The **Spate** project was started in 2014 by Edward Blizniuk (aka Ed Blake) as a part of his research dedicated to high performance Web servers.

The initial idea was to create a tool that can efficiently use resources of modern multi core virtual and hardware appliances to provide highest load on HTTP servers under the test.

The **Spate** tool is written in plain C-99 (actually, very close to ANSI C standard). The original configuration style and user interface are minimalistic and adopted for automatic testing.

The main features that distinguish **Spate** from the existing tools like **Apache Benchmarker** or **httperf** are:

  * Good scalability on multicore systems achieved by usage of semi-independent worker threads
  * An ability to configure affinity per worker thread
  * Useful statistic samples in csv format with configurable sampling period
  * Unlimited number of local IP-s and interfaces used for connections to the server, flexible connections configuration using 'Tuples'
  * High number of simultaneous connections (at least hundreds of thousands)
  * Configurable step-by-step load increase on start
  * Server mode
  * Support for SPOOL socket interface (https://code.google.com/p/socket-pool/)

Please visit ReleaseNotes page for details about Spate versions.


