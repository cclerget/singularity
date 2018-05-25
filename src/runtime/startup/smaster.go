// Copyright (c) 2018, Sylabs Inc. All rights reserved.
// This software is licensed under a 3-clause BSD license. Please consult the
// LICENSE file distributed with the sources of this project regarding your
// rights to use or distribute this software.

package main

/*
#include <sys/types.h>
#include "startup/wrapper.h"
*/
import "C"

import (
	"io"
	"log"
	"net"
	"os"
	"os/exec"
	"os/signal"
	"strconv"
	"sync"
	"syscall"
	"time"
	"unsafe"

	"github.com/singularityware/singularity/src/pkg/sylog"
	runtime "github.com/singularityware/singularity/src/pkg/workflows"
	fused "github.com/singularityware/singularity/src/runtime/fused"
	internalRuntime "github.com/singularityware/singularity/src/runtime/workflows"
)

func runAsInstance(conn *os.File) {
	data := make([]byte, 1)

	n, err := conn.Read(data)
	if n == 0 && err != io.EOF {
		os.Exit(1)
	} else {
		/* sleep a bit to see if child exit */
		time.Sleep(100 * time.Millisecond)
		syscall.Kill(syscall.Getpid(), syscall.SIGSTOP)
	}
}

func startChild(wg *sync.WaitGroup, engine *runtime.Engine, conn net.Conn, fwg *sync.WaitGroup) {
	fused.StartDaemon(engine, conn, fwg)
	wg.Done()
}

func startCmd(wg *sync.WaitGroup, engine *runtime.Engine, conn net.Conn) {
	//cmd := exec.Command("/bin/bash", engine.OciConfig.Root.GetPath())
	cmd := exec.Command("/bin/bash")
	cmd.SysProcAttr = &syscall.SysProcAttr{Pdeathsig: 9}
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	cmd.Stdin = os.Stdin
	err := cmd.Start()
	if err != nil {
		log.Fatal(err)
	}
	err = cmd.Wait()
	conn.Close()
	wg.Done()
}

func handleChild(pid int, signal chan os.Signal, engine *runtime.Engine) {
	var status syscall.WaitStatus

	for {
		select {
		case _ = <-signal:
			if wpid, err := syscall.Wait4(pid, &status, syscall.WNOHANG, nil); err != nil {
				sylog.Errorf("test")
			} else if wpid != pid {
				continue
			}

			if err := engine.CleanupContainer(); err != nil {
				sylog.Errorf("container cleanup failed: %s", err)
			}

			if status.Exited() {
				sylog.Debugf("Child exited with exit status %d", status.ExitStatus())
				os.Exit(status.ExitStatus())
			} else if status.Signaled() {
				sylog.Debugf("Child exited due to signal %d", status.Signal())
				syscall.Kill(os.Getpid(), status.Signal())
			}
		}
	}
}

// SMaster initializes a runtime engine and runs it
//export SMaster
func SMaster(socket C.int, sruntime *C.char, config *C.struct_cConfig, jsonC *C.char) {
	var wg sync.WaitGroup
	var fwg sync.WaitGroup

	sigchld := make(chan os.Signal, 1)
	signal.Notify(sigchld, syscall.SIGCHLD)

	os.Setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin")

	containerPid := int(config.containerPid)
	runtimeName := C.GoString(sruntime)
	jsonBytes := C.GoBytes(unsafe.Pointer(jsonC), C.int(config.jsonConfSize))

	comm := os.NewFile(uintptr(socket), "socket")
	conn, err := net.FileConn(comm)
	comm.Close()

	/* hold a reference to container network namespace for cleanup */
	_, err = os.Open("/proc/" + strconv.Itoa(containerPid) + "/ns/net")
	if err != nil {
		sylog.Fatalf("can't open network namespace: %s\n", err)
	}

	engine, err := internalRuntime.NewRuntimeEngine(runtimeName, jsonBytes)
	if err != nil {
		sylog.Fatalf("failed to initialize runtime: %s\n", err)
	}

	wg.Add(1)
	go handleChild(containerPid, sigchld, engine)

	wg.Add(1)
	fwg.Add(1)
	go startChild(&wg, engine, conn, &fwg)
	fwg.Wait()

	wg.Add(1)
	go startCmd(&wg, engine, conn)

	wg.Add(1)
	go engine.MonitorContainer()

	if engine.IsRunAsInstance() {
		wg.Add(1)
		go runAsInstance(comm)
	}

	wg.Wait()
	os.Exit(0)
}

func main() {}
