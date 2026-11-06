# net: transport, protocol, handshake, replication, interest management, LAN
# discovery, asset transfer. HEADLESS: shared by the client and the dedicated
# server, so it may not use Qt or OpenGL either.

arcane_add_module(net PUBLIC_DEPS arcane::core)

# Planned third-party libraries (added by their stories):
#   NET-01  Asio (standalone) + TLS (OpenSSL or mbedTLS)
#   ARCH-06 Protobuf or FlatBuffers, depending on spike S4
