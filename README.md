TCP Full Proxy
=================

This is a simple TCP Full Proxy written in C.

The program listens and accepts connections on the specified `<local port>`, for each new connection the program creates a separate connection to the `<host>` on port `<remote port>` and proxies the TCP data between them.

Authors
-------

This program is written and maintained by Grant Ashton.

Bugs
--------

I welcome bug reports, fixes, and other improvements.

Please report bugs via the [github issue tracker](https://github.com/gashton/tcp_full_proxy/issues).

Master [git repository](https://github.com/gashton/tcp_full_proxy):

   `git clone https://github.com/gashton/tcp_full_proxy.git`

Build From Source
=================

Unix
--------

1. `git clone https://github.com/gashton/tcp_full_proxy.git`
2. `cd tcp_full_proxy`
3. `make`

Usage
=================

`./tcp_full_proxy {-v} -l <local port> -h <host> -p <remote port>`

`-v` Output is verbose (Optional).
`-l` Local port that the programs listens and accepts connections to be proxied.
`-h` Remote Host that accepted connections will be proxied to.
`-p` Remote Port on Remote Host.

Legal Stuff
===========

Copyright (c) 2015, Grant Ashton

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.