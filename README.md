# `x25519`: Elliptic-Curve Diffie-Hellman Key Exchange using Curve25519

# Installation:

Copy both [x25519.c](x25519.c) and [x25519.h](x25519.h) into your project and include the header where needed.

# Usage:

The [x25519.h](x25519.h) header provides
```C
void x25519(uint8_t *public, const uint8_t *secret, const uint8_t *basepoint);
```
which you can use to build functions for deriving public and shared keys.

# Sample Usage:

See [key_exchange.c](key_exchange.c) for a full example.