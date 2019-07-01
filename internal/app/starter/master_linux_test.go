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
			engine:       nil,
			shallPass:    false,
		},
		{
			name:         "fake engine; fake rpcSocket",
			rpcSocket:    42000,
			containerPid: -1,
			engine:       nil,
			shallPass:    false,
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
			engine:       nil,
			shallPass:    false,
		},
		{
			name:         "fake engine; fake masterSocket",
			masterSocket: 42000,
			containerPid: -1,
			engine:       nil,
			shallPass:    false,
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
