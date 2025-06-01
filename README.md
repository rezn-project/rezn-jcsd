# rezn-jcsd

**rezn-jcsd** is a fast, dependency-free UNIX socket daemon for canonicalizing JSON according to [RFC 8785 (JCS)](https://datatracker.ietf.org/doc/html/rfc8785).  

---

## Why?

https://discuss.ocaml.org/t/canonical-json-with-yojson-for-signature-verification/16725/16

---

## Features

- Canonicalizes JSON via UNIX socket or CLI
- Vendored Go implementation (Apache 2.0)
- Suitable for cryptographic signing pipelines
- Minimal, auditable, production-ready

---

## Usage

### Socket mode (default)

Launch the service (creating socket file in /tmp for quick testing)

```bash
REZN_JCSD_SOCKET=/tmp/jcsd.sock ./jcsd
```

Hit it with some JSON

```bash
echo '{"op":"canon","source":"{ \"b\":2, \"a\":1 }"}' | socat - UNIX-CONNECT:/tmp/jcsd.sock
````

Returns:

```json
{"result":"{\"a\":1,\"b\":2}"}
```

### CLI mode

```bash
cat input.json | reznjcs-cli > canonical.json
```

---

## Environment

* `REZN_JCSD_SOCKET`: override default socket path (`/run/rezn-jcsd/jcs.sock`)

---

## Building

```bash
go build -o reznjcs-cli ./cmd/cli
go build -o jcsd ./cmd/jcsd
```

---

## License

* `rezn-jcsd` is licensed under **MIT**
* Canonicalization logic is vendored from [Anders Rundgren's json-canonicalization library](https://github.com/cyberphone/json-canonicalization), originally published under the Apache License 2.0

  * See [`internal/jcs/README.md`](internal/jcs/README.md)
