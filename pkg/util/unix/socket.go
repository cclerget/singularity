// Copyright (c) 2018, Sylabs Inc. All rights reserved.
// This software is licensed under a 3-clause BSD license. Please consult the
// LICENSE.md file distributed with the sources of this project regarding your
// rights to use or distribute this software.

package unix

import (
	"bytes"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"runtime"
	"syscall"
)

// Listen wraps net.Listen to handle 108 characters issue
func Listen(path string) (net.Listener, error) {
	socket := path

	if len(path) >= 108 {
		runtime.LockOSThread()
		defer runtime.UnlockOSThread()

		cwd, err := os.Getwd()
		if err != nil {
			return nil, fmt.Errorf("failed to get current working directory: %s", err)
		}
		defer os.Chdir(cwd)

		dir := filepath.Dir(path)
		socket = filepath.Base(path)

		if err := os.Chdir(dir); err != nil {
			return nil, fmt.Errorf("failed to go into %s: %s", dir, err)
		}
	}

	return net.Listen("unix", socket)
}

// Dial wraps net.Dial to handle 108 characters issue
func Dial(path string) (net.Conn, error) {
	socket := path

	if len(path) >= 108 {
		runtime.LockOSThread()
		defer runtime.UnlockOSThread()

		cwd, err := os.Getwd()
		if err != nil {
			return nil, fmt.Errorf("failed to get current working directory: %s", err)
		}
		defer os.Chdir(cwd)

		dir := filepath.Dir(path)
		socket = filepath.Base(path)

		if err := os.Chdir(dir); err != nil {
			return nil, fmt.Errorf("failed to go into %s: %s", dir, err)
		}
	}

	return net.Dial("unix", socket)
}

// CreateSocket creates an unix socket and returns connection listener.
func CreateSocket(path string) (net.Listener, error) {
	oldmask := syscall.Umask(0177)
	defer syscall.Umask(oldmask)
	return Listen(path)
}

// WriteSocket writes data over unix socket
func WriteSocket(path string, data []byte) error {
	c, err := Dial(path)

	if err != nil {
		return fmt.Errorf("failed to connect to %s socket: %s", path, err)
	}
	defer c.Close()

	if _, err := c.Write(data); err != nil {
		return fmt.Errorf("failed to send data over socket: %s", err)
	}

	return nil
}

func PeerCred(conn *net.UnixConn) (*syscall.Ucred, error) {
	f, err := conn.File()
	if err != nil {
		return nil, fmt.Errorf("can't get file descriptor from unix socket: %s", err)
	}
	defer f.Close()

	return syscall.GetsockoptUcred(int(f.Fd()), syscall.SOL_SOCKET, syscall.SO_PEERCRED)
}

func SendFds(conn *net.UnixConn, buf []byte, fds []int) error {
	rights := syscall.UnixRights(fds...)
	_, _, err := conn.WriteMsgUnix(buf, rights, nil)
	return err
}

func RecvFds(conn *net.UnixConn) ([]byte, []int, error) {
	oob := make([]byte, 4096)
	buf := make([]byte, 4096)

	_, oobn, _, _, err := conn.ReadMsgUnix(buf, oob)
	if err != nil {
		return nil, nil, fmt.Errorf("while reading unix socket: %s", err)
	}

	scms, err := syscall.ParseSocketControlMessage(oob[:oobn])
	if err != nil {
		return nil, nil, fmt.Errorf("while reading from socket: %s", err)
	}
	if len(scms) != 1 {
		return nil, nil, fmt.Errorf("no control message found on unix socket")
	}

	scm := scms[0]
	fds, err := syscall.ParseUnixRights(&scm)
	if err != nil {
		return nil, nil, fmt.Errorf("while getting file descriptors: %s", err)
	}

	return bytes.Trim(buf, "\x00"), fds, nil
}
