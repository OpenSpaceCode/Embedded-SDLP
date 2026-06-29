# EmbeddedSDLP

Minimal, embedded-optimized implementation of **CCSDS Space Data Link Protocol (SDLP)** for TM (Telemetry) and TC (Telecommand) frame handling, following international standards.

## Standards Compliance

- **CCSDS 132.0-B-3**: TM Space Data Link Protocol
- **CCSDS 232.0-B-4**: TC Space Data Link Protocol

## Features

### Core Protocol Implementation

- **Telemetry (TM) Frame Handling**: Create, encode, and decode TM frames
- **Telecommand (TC) Frame Handling**: Create, encode, and decode TC frames
- **Frame Error Control Field (FECF)**: 2-byte FECF carried verbatim in the wire format; computing/validating the value (e.g. CRC-16) is left to the application
- **Configurable**: Support for virtual channels, spacecraft IDs, and frame sequence numbers
- **TC Segment Header**: Optional MAP-based segmentation support (enabled with `TC_SEGMENT_HEADER_ENABLED`)

### Design Principles

- **Minimal footprint**: Small library size (stripped)
- **Zero allocation**: Stack-based, no dynamic memory
- **Embedded-optimized**: Pure C11, no external dependencies
- **Portable**: Standard C11, big-endian network byte order

## Project Structure

```
EmbeddedSDLP/
├── include/
│   ├── sdlp_common.h    # Common definitions and error codes
│   ├── sdlp_tm.h        # TM frame definitions
│   └── sdlp_tc.h        # TC frame definitions
├── src/
│   ├── sdlp_tm.c        # TM frame implementation
│   └── sdlp_tc.c        # TC frame implementation
├── examples/
│   ├── example_crc.h    # CRC-16 helper used only by the examples
│   ├── tm_example.c     # TM frame example
│   └── tc_example.c     # TC frame example
├── docs/
│   ├── 132x0b3_TM_SDLP.pdf   # CCSDS 132.0-B-3 standard
│   └── 232x0b4e1c1_TC_SDLP.pdf # CCSDS 232.0-B-4 standard
├── Makefile
└── README.md
```

## Building

### Build Everything

```bash
make all
```

This will create:
- `build/libsdlp.a` - Static library
- `build/bin/tm_example` - TM frame example
- `build/bin/tc_example` - TC frame example

### Build Library Only

```bash
make lib
# Produces: build/libsdlp.a (static)
```

### Build Examples

```bash
make examples
```

### Run Tests

```bash
make test
```

### Coverage (HTML)

Requires `gcovr` installed in your system:

```bash
sudo apt install gcovr
```

Generate coverage report:

```bash
make coverage-html
```

Output report:

```text
build/coverage/index.html
```

### Clean

```bash
make clean      # Remove build artifacts
```

## Quick Start

Look at the examples/

## Memory Usage (Estimated)

- **Library (stripped)**: < 5 KB
- **Per TM frame buffer**: `TM_PRIMARY_HEADER_SIZE` (6) + data + 2 bytes FECF
- **Per TC frame buffer**: `TC_PRIMARY_HEADER_SIZE` (5) + data + 2 bytes FECF
- **Maximum data per frame**: `TM_MAX_DATA_SIZE` = 1024 bytes (TM); `TC_MAX_DATA_SIZE` = 1017 bytes (TC — the whole frame is capped at 1024 octets per CCSDS 232.0-B-4; one less when the TC segment header is enabled)

## Limitations and Extensions

Current implementation focuses on core protocol features:

- No automatic retransmission handling
- No flow control or bandwidth management
- No segmentation beyond optional TC segment header
- Single static frame counter (not thread-safe)

These can be extended as needed for specific mission requirements.

## References

- CCSDS 132.0-B-3: TM Space Data Link Protocol ([docs/132x0b3_TM_SDLP.pdf](docs/132x0b3_TM_SDLP.pdf))
- CCSDS 232.0-B-4: TC Space Data Link Protocol ([docs/232x0b4e1c1_TC_SDLP.pdf](docs/232x0b4e1c1_TC_SDLP.pdf))

## License

Apache License 2.0 - See [LICENSE](LICENSE) file for details.
