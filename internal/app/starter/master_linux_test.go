// Copyright (c) 2018-2019, Sylabs Inc. All rights reserved.
// This software is licensed under a 3-clause BSD license. Please consult the
// LICENSE.md file distributed with the sources of this project regarding your
// rights to use or distribute this software.

package starter

import (
	"net"
	"os"
	"syscall"
	"testing"

	"github.com/sylabs/singularity/internal/pkg/runtime/engines"
	"github.com/sylabs/singularity/internal/pkg/runtime/engines/config"
	"github.com/sylabs/singularity/internal/pkg/runtime/engines/config/starter"
	"github.com/sylabs/singularity/internal/pkg/test"
)

type testEngineConfig struct {
}

type testEngineOperations struct {
	CommonConfig *config.Common    `json:"-"`
	EngineConfig *testEngineConfig `json:"engineConfig"`
}

var testOp = new(testEngineOperations)

func (e *testEngineOperations) Config() config.EngineConfig {
	return e.EngineConfig
}

func (e *testEngineOperations) InitConfig(common *config.Common) {
	return
}

func (e *testEngineOperations) PrepareConfig(starterConfig *starter.Config) error {
	return nil
}

func (e *testEngineOperations) CreateContainer(containerPid int, rpcConn net.Conn) error {
	return nil
}

func (e *testEngineOperations) StartProcess(masterConn net.Conn) error {
	return nil
}

func (e *testEngineOperations) PostStartProcess(containerPid int) error {
	return nil
}

func (e *testEngineOperations) MonitorContainer(containerPid int, signals chan os.Signal) (syscall.WaitStatus, error) {
	var status syscall.WaitStatus
	return status, nil
}

func (e *testEngineOperations) CleanupContainer(err error, status syscall.WaitStatus) error {
	return nil
}

func TestCreateContainer(t *testing.T) {
	test.DropPrivilege(t)
	defer test.ResetPrivilege(t)

	var fatal error
	fatalChan := make(chan error, 1)

	tests := []struct {
		name         string
		rpcSocket    int
		containerPid int
		engine       *engines.Engine
		shallPass    bool
	}{
		{
			name:         "fake engine struct",
			rpcSocket:    -1,
			containerPid: -1,
			engine: &engines.Engine{
				EngineOperations: testOp,
				Common: &config.Common{
					EngineConfig: &testEngineConfig{},
				},
			},
			shallPass: false,
		},
		{
			name:         "fake engine; fake rpcSocket",
			rpcSocket:    42000,
			containerPid: -1,
			engine: &engines.Engine{
				EngineOperations: testOp,
				Common: &config.Common{
					EngineConfig: &testEngineConfig{},
				},
			},
			shallPass: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			go createContainer(tt.rpcSocket, tt.containerPid, tt.engine, fatalChan)
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

func TestStartContainer(t *testing.T) {
	test.DropPrivilege(t)
	defer test.ResetPrivilege(t)

	var fatal error
	fatalChan := make(chan error, 1)

	tests := []struct {
		name         string
		masterSocket int
		containerPid int
		engine       *engines.Engine
		shallPass    bool
	}{
		{
			name:         "fake engine",
			masterSocket: -1,
			containerPid: -1,
			engine: &engines.Engine{
				EngineOperations: testOp,
			},
			shallPass: false,
		},
		{
			name:         "fake engine; fake masterSocket",
			masterSocket: 42000,
			containerPid: -1,
			engine: &engines.Engine{
				EngineOperations: testOp,
			},
			shallPass: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			go startContainer(tt.masterSocket, tt.containerPid, tt.engine, fatalChan)
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

	tests := []struct {
		name         string
		rpcSocket    int
		masterSocket int
		pid          int
		engine       *engines.Engine
		shallPass    bool
	}{
		{
			name:         "invalid case",
			rpcSocket:    -1,
			masterSocket: -1,
			pid:          -1,
			engine: &engines.Engine{
				EngineOperations: testOp,
			},
			shallPass: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Master(tt.rpcSocket, tt.masterSocket, tt.pid, tt.engine)
		})
	}
}
