package main

import (
	"encoding/json"
	"fmt"
	"net"
	"os"

	"github.com/rezn-project/rezn-jcsd/internal/jcs"
)

const defaultSocket = "/run/rezn-jcsd/jcs.sock"

type Request struct {
	Op     string `json:"op"`
	Source string `json:"source"`
}

type Response struct {
	Result string `json:"result,omitempty"`
	Error  string `json:"error,omitempty"`
}

func handleConnection(conn net.Conn) {
	defer conn.Close()

	decoder := json.NewDecoder(conn)
	encoder := json.NewEncoder(conn)

	var req Request
	if err := decoder.Decode(&req); err != nil {
		fmt.Fprintln(os.Stderr, "Decode error:", err)
		encoder.Encode(Response{Error: "invalid JSON"})
		return
	}

	if req.Op != "canon" {
		fmt.Fprintln(os.Stderr, "Unsupported op:", req.Op)
		encoder.Encode(Response{Error: "unsupported op"})
		return
	}

	canon, err := jsoncanonicalizer.Transform([]byte(req.Source))
	if err != nil {
		fmt.Fprintln(os.Stderr, "Canonicalization error:", err)
		encoder.Encode(Response{Error: err.Error()})
		return
	}

	if err := encoder.Encode(Response{Result: string(canon)}); err != nil {
		fmt.Fprintln(os.Stderr, "Encode response error:", err)
	}
}

func main() {
	socket := os.Getenv("REZN_JCSD_SOCKET")
	if socket == "" {
		socket = defaultSocket
	}

	_ = os.Remove(socket) // nuke stale socket

	ln, err := net.Listen("unix", socket)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to bind to socket: %v\n", err)
		os.Exit(1)
	}
	defer ln.Close()

	fmt.Println("Listening on", socket)

	for {
		conn, err := ln.Accept()
		if err != nil {
			continue
		}
		go handleConnection(conn)
	}
}
