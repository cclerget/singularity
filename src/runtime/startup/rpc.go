// Copyright (c) 2018, Sylabs Inc. All rights reserved.
// This software is licensed under a 3-clause BSD license. Please consult the
// LICENSE file distributed with the sources of this project regarding your
// rights to use or distribute this software.

package main

import "C"

import (
	"net"
	"os"
	"syscall"

	"github.com/singularityware/singularity/src/pkg/buildcfg"
	"github.com/singularityware/singularity/src/pkg/sylog"
	"github.com/singularityware/singularity/src/runtime/workflows/rpc"
)

// RPCServer serves runtime engine requests
//export RPCServer
func RPCServer(socket C.int, sruntime *C.char) {
	runtime := C.GoString(sruntime)

	comm := os.NewFile(uintptr(socket), "unix")

	conn, err := net.FileConn(comm)
	if err != nil {
		sylog.Fatalf("socket communication error: %s\n", err)
	}
	comm.Close()

	rpc.ServeRuntimeEngineRequests(runtime, conn)

	if err := syscall.Chdir(buildcfg.CONTAINER_FINALDIR); err != nil {
		sylog.Errorf("Failed to change directory to %s", buildcfg.CONTAINER_FINALDIR)
	}

	sylog.Debugf("Called pivot_root(%s, etc)\n", buildcfg.CONTAINER_FINALDIR)
	if err := syscall.PivotRoot(".", "etc"); err != nil {
		sylog.Errorf("pivot_root %s: %s", buildcfg.CONTAINER_FINALDIR, err)
	}

	sylog.Debugf("Called chroot(%s)\n", buildcfg.CONTAINER_FINALDIR)
	if err := syscall.Chroot("."); err != nil {
		sylog.Errorf("chroot %s", err)
	}

	sylog.Debugf("Called unmount(etc, syscall.MNT_DETACH)\n")
	if err := syscall.Unmount("etc", syscall.MNT_DETACH); err != nil {
		sylog.Errorf("unmount pivot_root dir %s", err)
	}

	sylog.Debugf("Changing directory to / to avoid getpwd issues\n")
	if err := syscall.Chdir("/"); err != nil {
		sylog.Errorf("chdir / %s", err)
	}

	os.Exit(0)
}
