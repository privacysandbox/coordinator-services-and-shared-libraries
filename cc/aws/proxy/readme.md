# Overview
This utility is a C++ based proxy for supporting network traffic on AWS Nitro
Enclaves. It speaks mostly the [SOCKS5](https://www.rfc-editor.org/rfc/rfc1928)
protocol.

# How it works
The basic concept behind this proxy is using LD_PRELOAD library to override a
few libc functions, and convert those calls into SOCK5 calls to the proxy. The
current list of overridden symbols are listed as below:
* accept
* accept4
* bind
* connect
* epoll_add
* getsockopt
* ioctl
* listen
* setsockopt

On outgoing connection, all traffic starts with `connect()` call. When a TCP
socket is created via `socket()`, namely a `AF_INET`/`AF_INET6` socket with
`SOCK_STEAM` type, we forcibly make it `AF_VSOCK`. SOCKS5 handshake is performed
inside `connect()` before it returns.

With regards to incoming connections, for security reasons, we do not listen on
VSOCK port inside the enclave. Instead, we connect out to the proxy to establish
a pool of standby connections. Upon each new incoming connection, one standby
connection from the pool is chosen to forward traffic to. Since we cannot
maintain the pool within the application process itself (as it needs extra
threads, etc, to maintain the state, which do not work with fork; and it will
also not work with select/poll/epoll), we created socket_vendor to manage the
pools. The application talks to socket_vendor on bind/listen/accept over unix
domain socket.

# What works and what does not
Any application that dynamically links libc to do syscalls will work. This
includes C/C++ based applications, rust, CPython, and Java, etc.

Application that statically links libc, or uses its own syscall implementation,
won't work. This likely includes golang. A future improvement may be using eBPF
inside the enclave if possible, to replace the LD_PRELOAD libs.

The LD_PRELOAD library currently keeps no internal state of each socket. It
functions by looking at the type and domain of each socket. This means, if you
call getsockname/getpeername after connection establishment, you'll probably get
some garbage.

# Build
## Building the proxy
    bazel build //cc/aws/proxy/...

## Building and running tests

    bazel test //cc/aws/proxy/...
# Other useful tips
## Running your own application
If you'd like to try out this proxy with your own application, follow these
steps:
1. `bazel build //cc/aws/proxy:reproducible_proxy_outputs`
1. Under directory `bazel-bin/cc/aws/proxy`, find the built binaries  `proxy`,
   `proxify`, `libproxy_preload.so`, `socket_vendor`, upload to your EC2
   instance.
1. Run proxy in background on the EC2 instance.
1. Add `proxify`, `libproxy_preload.so` and `socket_vendor` to the docker image
   under the same directory.
1. Put CMD/ENTRYPOINT as `["/path/to/proxify", "your_app", "one_arg", "more_args"]`
1. Build and run the enclave image.

## Notes
1. **Currently we only support TCP**. UDP is not supported at the moment. DNS
   lookups that goes through UDP won't work. We uses 'use-vc' option in
   resolv.conf to make DNS lookup to go through TCP as well.
1. In nitro enclaves, `/etc/resolv.conf` is likely to be a dangling symlink.
   `proxify` by default unlinks it and creates a new one with pre-defined
   content that defines the nameservers to 8.8.8.8 and 1.1.1.1. This will make
   the enclave application talk to these servers during DNS resolution. It may
   not work in a private VPC without internet access. You may need to override
   the `/etc/resolv.conf` inside the enclave with your desired private DNS
   settings.
1. AmazonLinux2 (AL2) uses an old version of glibc. Newer compilers with newer
   glibc on your build machine may generate binary that's not runnable on AL2.
   That's why we created `//cc/aws/proxy:reproducible_proxy_outputs` target to
   generate binaries that's guaranteed runnable on AL2. It also makes the build
   reproducible for building enclave images.
