// Copyright (c) 2019, Sylabs Inc. All rights reserved.
// This software is licensed under a 3-clause BSD license. Please consult the
// LICENSE.md file distributed with the sources of this project regarding your
// rights to use or distribute this software.

package example

import (
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sylabs/singularity/e2e/internal/e2e"
	"github.com/sylabs/singularity/internal/pkg/test"
)

type testingEnv struct {
	// base env for running tests
	CmdPath     string `split_words:"true"`
	TestDir     string `split_words:"true"`
	RunDisabled bool   `default:"false"`
	//  base image for tests
	ImagePath string `split_words:"true"`
}

var testenv testingEnv

// exec tests min fuctionality for singularity exec
func testSingularityGeneric(t *testing.T, name string, privileged bool, action string, options e2e.ExecOpts, image string) {
	testFn := test.WithoutPrivilege
	if privileged {
		testFn = test.WithPrivilege
	}

	const (
		testfile = "testSingularityExec.tmp"
		testdir  = "testSingularityExec.dir"
	)

	t.Run(name, testFn(func(t *testing.T) {
		if options.Userns {
			// check if user namespace is supported and skip test if not
		}

		workdir, err := e2e.MakeTmpDir(testenv.TestDir, "d-", 0755)
		defer os.RemoveAll(workdir)

		if err := os.MkdirAll(filepath.Join(workdir, "tmp", testdir), 0755); err != nil {
			t.Fatal(err)
		}
		if err := ioutil.WriteFile(filepath.Join(workdir, testfile), []byte{}, 0755); err != nil {
			t.Fatal(err)
		}

		// running parallel test could have side effects while
		// setting current working directory because it would affect
		// other Go threads too, it's safer to set cmd.Dir instead
		pwd, err := os.Getwd()
		if err != nil {
			t.Fatal(err)
		}
		defer os.Chdir(pwd)

		// required when running as root with user namespace enabled
		// because generally current working directory is located in
		// user home directory, so even if running as root and user
		// namespace enabled, root will get a permission denied while
		// mounting current working directory. Change this to temporary
		// workdir
		if err := os.Chdir(workdir); err != nil {
			t.Fatal(err)
		}

		tests := []struct {
			name string
			argv []string
			e2e.ExecOpts
			exitCode     int
			searchOutput string
		}{
			{"true", []string{"true"}, e2e.ExecOpts{}, 0, ""},
			{"trueAbsPAth", []string{"/bin/true"}, e2e.ExecOpts{}, 0, ""},
			{"false", []string{"false"}, e2e.ExecOpts{}, 1, ""},
			{"falseAbsPath", []string{"/bin/false"}, e2e.ExecOpts{}, 1, ""},
			// Scif apps tests
			{"ScifTestAppGood", []string{"testapp.sh"}, e2e.ExecOpts{App: "testapp"}, 0, ""},
			{"ScifTestAppBad", []string{"testapp.sh"}, e2e.ExecOpts{App: "fakeapp"}, 1, ""},
			{"ScifTestfolderOrg", []string{"test", "-d", "/scif"}, e2e.ExecOpts{}, 0, ""},
			{"ScifTestfolderOrg", []string{"test", "-d", "/scif/apps"}, e2e.ExecOpts{}, 0, ""},
			{"ScifTestfolderOrg", []string{"test", "-d", "/scif/data"}, e2e.ExecOpts{}, 0, ""},
			{"ScifTestfolderOrg", []string{"test", "-d", "/scif/apps/foo"}, e2e.ExecOpts{}, 0, ""},
			{"ScifTestfolderOrg", []string{"test", "-d", "/scif/apps/bar"}, e2e.ExecOpts{}, 0, ""},
			// blocked by issue [scif-apps] Files created at install step fall into an unexpected path #2404
			{"ScifTestfolderOrg", []string{"test", "-f", "/scif/apps/foo/filefoo.exec"}, e2e.ExecOpts{}, 0, ""},
			{"ScifTestfolderOrg", []string{"test", "-f", "/scif/apps/bar/filebar.exec"}, e2e.ExecOpts{}, 0, ""},
			{"ScifTestfolderOrg", []string{"test", "-d", "/scif/data/foo/output"}, e2e.ExecOpts{}, 0, ""},
			{"ScifTestfolderOrg", []string{"test", "-d", "/scif/data/foo/input"}, e2e.ExecOpts{}, 0, ""},
			{"WorkdirContain", []string{"test", "-d", "/tmp/" + testdir}, e2e.ExecOpts{Workdir: workdir, Contain: true}, 0, ""},
			{"Workdir", []string{"test", "-d", "/tmp/" + testdir}, e2e.ExecOpts{Workdir: workdir}, 1, ""},
			{"pwdGood", []string{"true"}, e2e.ExecOpts{Pwd: "/etc"}, 0, ""},
			{"home", []string{"test", "-f", filepath.Join(workdir, testfile)}, e2e.ExecOpts{Home: workdir}, 0, ""},
			{"noHome", []string{"test", "-f", "/home/" + testfile}, e2e.ExecOpts{Home: workdir + ":/home", NoHome: true}, 1, ""},
			{"homePath", []string{"test", "-f", "/home/" + testfile}, e2e.ExecOpts{Home: workdir + ":/home"}, 0, ""},
			{"homeTmp", []string{"true"}, e2e.ExecOpts{Home: "/tmp"}, 0, ""},
			{"homeTmpExplicit", []string{"true"}, e2e.ExecOpts{Home: "/tmp:/home"}, 0, ""},
			{"ScifTestAppGood", []string{"testapp.sh"}, e2e.ExecOpts{App: "testapp"}, 0, ""},
			{"ScifTestAppBad", []string{"testapp.sh"}, e2e.ExecOpts{App: "fakeapp"}, 1, ""},
			{"userBind", []string{"test", "-f", "/mnt/" + testfile}, e2e.ExecOpts{Binds: []string{workdir + ":/mnt"}}, 0, ""},
			{"PwdGood", []string{"pwd"}, e2e.ExecOpts{Pwd: "/etc"}, 0, "/etc"},
		}

		for _, tt := range tests {
			// note that we need to run tests with the same
			// privileges wrapper function otherwise tests
			// will be always executed as root
			t.Run(tt.name, testFn(func(t *testing.T) {
				if options.Userns {
					tt.ExecOpts.Userns = true
				}
				stdout, stderr, exitCode, err := e2e.ImageExec(t, testenv.CmdPath, action, tt.ExecOpts, image, tt.argv)
				if stdout != "" && !strings.Contains(stdout, tt.searchOutput) {
					t.Log(stdout)
					t.Fatalf("unexpected output returned running '%v': %v", strings.Join(tt.argv, " "), err)
				} else if tt.exitCode >= 0 && exitCode == tt.exitCode {
					// PASS
					return
				} else if tt.exitCode == 0 && exitCode != 0 {
					// FAIL
					t.Log(stderr)
					t.Fatalf("unexpected failure running '%v': %v", strings.Join(tt.argv, " "), err)
				} else if tt.exitCode != 0 && exitCode == 0 {
					// FAIL
					t.Log(stderr)
					t.Fatalf("unexpected success running '%v'", strings.Join(tt.argv, " "))
				} else if err != nil {
					// FAIL
					t.Log(stderr)
					t.Fatalf("unexpected error running '%v': %s", strings.Join(tt.argv, " "), err)
				}
			}))
		}
	}))
}

// RunE2ETests is the main func to trigger the test suite
func RunE2ETests(t *testing.T) {
	e2e.LoadEnv(t, &testenv)
	e2e.EnsureImage(t)

	// world writable to allow unprivileged build to write
	// sandbox image
	sandboxUnpriv, err := e2e.MakeTmpDir(testenv.TestDir, "d-", 0777)
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(sandboxUnpriv)

	// need to change how SINGULARITY_CACHEDIR is set for parallel tests
	test.DropPrivilege(t)
	if output, err := e2e.ImageBuild(testenv.CmdPath, e2e.BuildOpts{Force: true, Sandbox: true}, sandboxUnpriv, testenv.ImagePath); err != nil {
		t.Fatalf("%s: %s", err, string(output))
	}
	test.ResetPrivilege(t)

	sandboxPriv, err := e2e.MakeTmpDir(testenv.TestDir, "d-", 0755)
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(sandboxPriv)

	if output, err := e2e.ImageBuild(testenv.CmdPath, e2e.BuildOpts{Force: true, Sandbox: true}, sandboxPriv, testenv.ImagePath); err != nil {
		t.Fatalf("%s: %s", err, string(output))
	}

	testSingularityGeneric(t, "Exec", false, "exec", e2e.ExecOpts{}, testenv.ImagePath)
	testSingularityGeneric(t, "ExecPriv", true, "exec", e2e.ExecOpts{}, testenv.ImagePath)
	testSingularityGeneric(t, "Run", false, "run", e2e.ExecOpts{}, testenv.ImagePath)
	testSingularityGeneric(t, "RunPriv", true, "run", e2e.ExecOpts{}, testenv.ImagePath)
	testSingularityGeneric(t, "ExecUsernsWithSIF", false, "exec", e2e.ExecOpts{Userns: true}, testenv.ImagePath)
	testSingularityGeneric(t, "ExecUsernsPrivWithSIF", true, "exec", e2e.ExecOpts{Userns: true}, testenv.ImagePath)
	testSingularityGeneric(t, "RunUsernsWithSIF", false, "run", e2e.ExecOpts{Userns: true}, testenv.ImagePath)
	testSingularityGeneric(t, "RunUsernsPrivWithSIF", true, "run", e2e.ExecOpts{Userns: true}, testenv.ImagePath)
	testSingularityGeneric(t, "ExecUsernsWithSandbox", false, "exec", e2e.ExecOpts{Userns: true}, sandboxUnpriv)
	testSingularityGeneric(t, "ExecUsernsPrivWithSandbox", true, "exec", e2e.ExecOpts{Userns: true}, sandboxPriv)
	testSingularityGeneric(t, "RunUsernsWithSandbox", false, "run", e2e.ExecOpts{Userns: true}, sandboxUnpriv)
	testSingularityGeneric(t, "RunUsernsPrivWithSandbox", true, "run", e2e.ExecOpts{Userns: true}, sandboxPriv)
}
