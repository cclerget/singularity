// Copyright (c) 2019, Sylabs Inc. All rights reserved.
// This software is licensed under a 3-clause BSD license. Please consult the
// LICENSE.md file distributed with the sources of this project regarding your
// rights to use or distribute this software.

package test

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"os/user"
	"strconv"
	"testing"
)

var origUID, origGID, unprivUID, unprivGID int
var origHome, unprivHome string

// EnsurePrivilege ensures elevated privileges are available during a test.
func EnsurePrivilege(t *testing.T) {
	uid := os.Getuid()
	if uid != 0 {
		t.Fatal("test must be run with privilege")
	}
}

// DropPrivilege drops privilege. Use this at the start of a test that does
// not require elevated privileges. A matching call to ResetPrivilege must
// occur before the test completes (a defer statement is recommended.)
func DropPrivilege(t *testing.T) {
	t.Skip("Currently not supported for Darwin")
}

// ResetPrivilege returns effective privilege to the original user.
func ResetPrivilege(t *testing.T) {
	t.Skip("Currently not supported for Darwin")
}

// WithPrivilege wraps the supplied test function with calls to ensure
// the test is run with elevated privileges.
func WithPrivilege(f func(t *testing.T)) func(t *testing.T) {
	return func(t *testing.T) {
		t.Helper()

		EnsurePrivilege(t)

		f(t)
	}
}

// WithoutPrivilege wraps the supplied test function with calls to ensure
// the test is run without elevated privileges.
func WithoutPrivilege(f func(t *testing.T)) func(t *testing.T) {
	return func(t *testing.T) {
		t.Skip("Currently not supported for Darwin")
	}
}

// getProcInfo returns the parent PID, UID, and GID associated with the
// supplied PID. Calls os.Exit on error.
func getProcInfo(pid int) (ppid int, uid int, gid int) {
	f, err := os.Open(fmt.Sprintf("/proc/%v/status", pid))
	if err != nil {
		log.Fatalf("failed to open /proc/%v/status", pid)
	}

	for s := bufio.NewScanner(f); s.Scan(); {
		var temp int
		if n, _ := fmt.Sscanf(s.Text(), "PPid:\t%d", &temp); n == 1 {
			ppid = temp
		}
		if n, _ := fmt.Sscanf(s.Text(), "Uid:\t%d", &temp); n == 1 {
			uid = temp
		}
		if n, _ := fmt.Sscanf(s.Text(), "Gid:\t%d", &temp); n == 1 {
			gid = temp
		}
	}
	return ppid, uid, gid
}

// getUnprivIDs searches recursively up the process parent chain to find a
// process with a non-root UID, then returns the UID and GID of that process.
// Calls os.Exit on error, or if no non-root process is found.
func getUnprivIDs(pid int) (uid int, gid int) {
	if 1 == pid {
		log.Fatal("no unprivileged process found")
	}

	ppid, uid, gid := getProcInfo(pid)
	if uid != 0 {
		return uid, gid
	}
	return getUnprivIDs(ppid)
}

func init() {
	origUID = os.Getuid()
	origGID = os.Getgid()
	origUser, err := user.LookupId(strconv.Itoa(origUID))

	if err != nil {
		log.Fatalf("err: %s", err)
	}

	origHome = origUser.HomeDir

	unprivUID, unprivGID = getUnprivIDs(os.Getpid())
	unprivUser, err := user.LookupId(strconv.Itoa(unprivUID))

	if err != nil {
		log.Fatalf("err: %s", err)
	}

	unprivHome = unprivUser.HomeDir
}
