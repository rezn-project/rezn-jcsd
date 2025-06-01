# internal/jcs

This directory contains a vendored implementation of the [JSON Canonicalization Scheme (JCS)](https://datatracker.ietf.org/doc/html/rfc8785), originally developed by WebPKI.org.

The source code was copied from the following repository:

- https://github.com/cyberphone/json-canonicalization
- Subdirectory: `go/src/webpki.org/jsoncanonicalizer`

## License

These files are licensed under the [Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0).  
The full license text is available in the root of this repository.

Each source file retains its original license header as required.

## Why It's Vendored

The upstream Go implementation is not structured as a Go module and has not been updated in years.  
Vendoring the code ensures:

- Long-term build stability
- Full control over updates and patches
- Independence from broken or abandoned module paths

## What This Code Does

The canonicalizer converts JSON objects and arrays into a deterministic byte representation suitable for signing, as defined in [RFC 8785](https://datatracker.ietf.org/doc/html/rfc8785).

This functionality is used by `rezn-jcsd` to provide a UNIX socket–based JSON canonicalization service.
