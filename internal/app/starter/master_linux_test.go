// Copyright (c) 2018-2019, Sylabs Inc. All rights reserved.
// This software is licensed under a 3-clause BSD license. Please consult the
// LICENSE.md file distributed with the sources of this project regarding your
// rights to use or distribute this software.

package starter

import (
	"testing"

	"github.com/sylabs/singularity/internal/pkg/runtime/engines"
	"github.com/sylabs/singularity/internal/pkg/test"
)

func TestDoRPCThread(t *testing.T) {
	test.DropPrivilege(t)
	defer test.ResetPrivilege(t)

	var fatal error
	fatalChan := make(chan error, 1)

	var fakeEngine engines.Engine

	tests := []struct {
		name         string
		rpcSocket    int
		containerPid int
		engine       *engines.Engine
		shallPass    bool
	}{
		{
			name:         "invalid case",
			rpcSocket:    -1,
			containerPid: -1,
			engine:       nil,
			shallPass:    false,
		},
		{
			name:         "fake engine struct",
			rpcSocket:    -1,
			containerPid: -1,
			engine:       &fakeEngine,
			shallPass:    false,
		},
		{
			name:         "fake engine; fake rpcSocket",
			rpcSocket:    42000,
			containerPid: -1,
			engine:       &fakeEngine,
			shallPass:    false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			go doRPCthread(tt.rpcSocket, tt.containerPid, tt.engine, fatalChan)
			fatal = <-fatalChan
			if tt.shallPass && fatal != nil {
				t.Fatalf("test %s expected to succeed but failed: %s", tt.name, fatal)
			}

			if !tt.shallPass && fatal == nil {
				t.Fatalf("test %s expected to fail but succeeded", tt.name)
			}
		})
	}
}

func TestDoMasterThread(t *testing.T) {
	test.DropPrivilege(t)
	defer test.ResetPrivilege(t)

	var fatal error
	fatalChan := make(chan error, 1)

	var fakeEngine engines.Engine

	tests := []struct {
		name         string
		masterSocket int
		containerPid int
		ppid         int
		engine       *engines.Engine
		isInstance   bool
		shallPass    bool
	}{
		{
			name:         "invalid case",
			masterSocket: -1,
			isInstance:   false,
			containerPid: -1,
			ppid:         -1,
			engine:       nil,
			shallPass:    false,
		},
		{
			name:         "fake engine",
			masterSocket: -1,
			isInstance:   false,
			containerPid: -1,
			ppid:         -1,
			engine:       &fakeEngine,
			shallPass:    false,
		},
		{
			name:         "fake engine; fake masterSocket",
			masterSocket: 42000,
			isInstance:   false,
			containerPid: -1,
			ppid:         -1,
			engine:       &fakeEngine,
			shallPass:    false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			go doMasterThread(tt.masterSocket, tt.isInstance, tt.containerPid, tt.ppid, tt.engine, fatalChan)
			fatal = <-fatalChan
			if tt.shallPass && fatal != nil {
				t.Fatalf("test %s expected to succeed but failed: %s", tt.name, fatal)
			}

			if !tt.shallPass && fatal == nil {
				t.Fatalf("test %s expected to fail but succeeded", tt.name)
			}
		})
	}
}

func TestMaster(t *testing.T) {
	test.DropPrivilege(t)
	defer test.ResetPrivilege(t)

	var fakeEngine engines.Engine

	tests := []struct {
		name         string
		rpcSocket    int
		masterSocket int
		pid          int
		engine       *engines.Engine
		isInstance   bool
		shallPass    bool
	}{
		{
			name:         "invalid case",
			rpcSocket:    -1,
			masterSocket: -1,
			isInstance:   false,
			pid:          -1,
			engine:       nil,
			shallPass:    false,
		},
		{
			name:         "fake engine",
			rpcSocket:    -1,
			masterSocket: -1,
			isInstance:   false,
			pid:          -1,
			engine:       &fakeEngine,
			shallPass:    false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Master(tt.rpcSocket, tt.masterSocket, tt.isInstance, tt.pid, tt.engine)
		})
	}
}
