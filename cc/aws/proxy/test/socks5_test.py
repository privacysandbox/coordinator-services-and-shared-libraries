#!/usr/bin/env python3
# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""
In this file, we test the socks5 protocol by using a tcp echo server, and a
client to check the SHA256 of the sent and received bytes.
"""

import hashlib
import os
import socket
import socketserver
import struct
import subprocess
import sys
import threading
import time


class TCPEchoHandler(socketserver.BaseRequestHandler):
    """
    A simple echo server on TCP. This is for the client to verify the round trip
    transfer is intact.
    """

    def handle(self) -> None:
        data = self.request.recv(4096)
        self.request.sendall(data)


class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    """
    Python's threading sucks, we know. However here it is basically IO-bound
    processing, so we are fine.
    """
    pass


def Socks5Handshake(sock, ip, port):
    socks5_req_ipv4 = b"\x05\x01\x00\x05\x01\x00\x01"
    ipv4_addr = socket.inet_aton(ip)
    socks5_req_ipv4 = socks5_req_ipv4 + ipv4_addr
    socks5_req_ipv4 = socks5_req_ipv4 + struct.pack('!H', port)
    sock.send(socks5_req_ipv4)
    # "Method selection reply" is 2 bytes.
    #    +----+--------+
    #    |VER | METHOD |
    #    +----+--------+
    #    | 1  |   1    |
    #    +----+--------+
    buf = sock.recv(2, socket.MSG_WAITALL)
    if buf != b'\x05\x00':
        print("Bad method selection: ", buf)
        return False
    # Connection request reply is 10 bytes, in IPv4.
    #   +----+-----+-------+------+----------+----------+
    #   |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
    #   +----+-----+-------+------+----------+----------+
    #   | 1  |  1  | X'00' |  1   | Variable |    2     |
    #   +----+-----+-------+------+----------+----------+
    buf = sock.recv(10, socket.MSG_WAITALL)
    # If REP byte is not 0, then there's a failure.
    if len(buf) != 10 or buf[1] != 0:
        print("Bad reply: ", buf)
        return False
    return True


def VsockProxyClient(vsock_port, ip, ip_port):
    """
    A client that expects the server to send back exact thing, namely, to be an
    echo server.
    """
    rand_data = os.urandom(1024)
    h = hashlib.sha256()
    h.update(rand_data)
    checksum = h.digest()
    # Since many machines, including kokoro, do not support VSOCK loopback, we
    # use IP socket to to simulate VSOCK here
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect(('localhost', vsock_port))
        if Socks5Handshake(sock, ip, ip_port) == False:
            print("SOCKS5 Handshake failed")
            return False
        sock.send(rand_data)
        buf = sock.recv(len(rand_data), socket.MSG_WAITALL)
        h = hashlib.sha256()
        h.update(buf)
        if checksum != h.digest():
            print("Data checksum does not match")
            return False
        else:
            return True


def ClientThread(ret, vsock_port, ip, ip_port):
    ret.append(VsockProxyClient(vsock_port, ip, ip_port))


if __name__ == "__main__":
    # Find an arbitrary unused port by specifying 0.
    HOST, PORT = "localhost", 0
    server = ThreadedTCPServer((HOST, PORT), TCPEchoHandler)
    proxy_path = sys.argv[1]
    # Since many machines, including kokoro, do not support VSOCK loopback, we
    # use IP socket to to simulate VSOCK here by setting "-t".
    proxy_proc = subprocess.Popen(
        [proxy_path, "-tp", "0"], stdout=subprocess.PIPE)
    proxy_proc.stdout.readline()  # skip first line
    line = proxy_proc.stdout.readline().strip()
    line_prefix = "Running on TCP port "
    proxy_port = int(line[len(line_prefix):])

    ip, port = server.server_address
    # Start server threads
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.daemon = True
    server_thread.start()
    # Run many threads to stress the proxy and find potential race conditions
    success = []
    client_threads = []
    num_threads = 100
    # Create 100 threads
    for i in range(num_threads):
        th = threading.Thread(target=ClientThread,
                              args=(success, proxy_port, ip, port))
        client_threads.append(th)
        th.start()
    # Wait for them to finish
    for th in client_threads:
        th.join()
    proxy_proc.kill()
    server.shutdown()
    # Check results
    assert (len(success) == 100)
    for ret in success:
        if ret is False:
            sys.exit(1)
