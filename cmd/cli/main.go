package main

import (
	"fmt"
	"io"
	"os"

	"github.com/rezn-project/rezn-jcsd/internal/jcs"
)

func main() {
	input, err := io.ReadAll(os.Stdin)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Error reading stdin:", err)
		os.Exit(1)
	}

	canon, err := jsoncanonicalizer.Transform(input)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Canonicalization error:", err)
		os.Exit(1)
	}

	fmt.Println(string(canon))
}
