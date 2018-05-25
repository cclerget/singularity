package fused

/*
#include <unistd.h>
#include "lib/image/image.h"
#include "lib/util/config_parser.h"
*/
// #cgo CFLAGS: -I../c/lib
// #cgo LDFLAGS: -L../../../builddir/lib -lruntime -luuid
import "C"

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"net/rpc"
	"os"
	"os/exec"
	"path"
	"strings"
	"sync"
	"syscall"

	"github.com/singularityware/singularity/src/pkg/buildcfg"
	"github.com/singularityware/singularity/src/pkg/sylog"
	"github.com/singularityware/singularity/src/pkg/util/loop"
	runtime "github.com/singularityware/singularity/src/pkg/workflows"
	"github.com/singularityware/singularity/src/runtime/workflows/workflows/singularity/rpc/client"

	"bazil.org/fuse"
	"bazil.org/fuse/fs"
	"bazil.org/fuse/fuseutil"
)

type Node struct {
	inode uint64
	name  string
	mode  os.FileMode
}

var inode uint64

func NewInode() uint64 {
	inode += 1
	return inode
}

type Dir struct {
	Node
	files       *[]*File
	directories *[]*Dir
}

type FS struct {
	root *Dir
}

func (f *FS) Root() (fs.Node, error) {
	return f.root, nil
}

func (d *Dir) Attr(ctx context.Context, a *fuse.Attr) error {
	a.Inode = d.inode
	a.Mode = os.ModeDir | 0750
	a.Uid = 0
	a.Gid = uint32(os.Getgid())
	return nil
}
func (d *Dir) Lookup(ctx context.Context, name string) (fs.Node, error) {
	if d.files != nil {
		for _, n := range *d.files {
			if n.name == name {
				return n, nil
			}
		}
	}
	if d.directories != nil {
		for _, n := range *d.directories {
			if n.name == name {
				return n, nil
			}
		}
	}
	return nil, fuse.ENOENT
}

func (d *Dir) ReadDirAll(ctx context.Context) ([]fuse.Dirent, error) {
	var children []fuse.Dirent
	if d.files != nil {
		for _, f := range *d.files {
			children = append(children, fuse.Dirent{Inode: f.inode, Type: fuse.DT_File, Name: f.name})
		}
	}
	if d.directories != nil {
		for _, dir := range *d.directories {
			children = append(children, fuse.Dirent{Inode: dir.inode, Type: fuse.DT_Dir, Name: dir.name})
		}
	}
	return children, nil
}

func (d *Dir) Create(ctx context.Context, req *fuse.CreateRequest, resp *fuse.CreateResponse) (fs.Node, fs.Handle, error) {
	f := &File{Node: Node{name: req.Name, inode: NewInode()}}
	files := []*File{f}
	if d.files != nil {
		files = append(files, *d.files...)
	}
	d.files = &files
	return f, f, nil
}

func (d *Dir) Remove(ctx context.Context, req *fuse.RemoveRequest) error {
	if req.Dir && d.directories != nil {
		newDirs := []*Dir{}
		for _, dir := range *d.directories {
			if dir.name != req.Name {
				newDirs = append(newDirs, dir)
			}
		}
		d.directories = &newDirs
		return nil
	} else if !req.Dir && *d.files != nil {
		newFiles := []*File{}
		for _, f := range *d.files {
			if f.name != req.Name {
				newFiles = append(newFiles, f)
			}
		}
		d.files = &newFiles
		return nil
	}
	return fuse.ENOENT
}

func (d *Dir) Mkdir(ctx context.Context, req *fuse.MkdirRequest) (fs.Node, error) {
	dir := &Dir{Node: Node{name: req.Name, inode: NewInode()}}
	directories := []*Dir{dir}
	if d.directories != nil {
		directories = append(*d.directories, directories...)
	}
	d.directories = &directories
	return dir, nil

}

type File struct {
	Node
	data []byte
}

func (f *File) Attr(ctx context.Context, a *fuse.Attr) error {
	size := uint64(0)
	if f.name == "config.json" {
		v := ctx.Value(&contextKey).(*Testing)
		json, _ := json.MarshalIndent(v.engine.RuntimeOciSpec, "", "    ")
		size = uint64(len(json))
	}
	a.Inode = f.inode
	a.Mode = f.mode
	a.Size = size
	a.Uid = 0
	a.Gid = uint32(os.Getgid())
	return nil
}

func (f *File) Read(ctx context.Context, req *fuse.ReadRequest, resp *fuse.ReadResponse) error {
	fmt.Println("read", f.name)
	fuseutil.HandleRead(req, resp, f.data)
	return nil
}

func (f *File) ReadAll(ctx context.Context) ([]byte, error) {
	if f.name == "config.json" {
		v := ctx.Value(&contextKey).(*Testing)
		json, _ := json.MarshalIndent(v.engine.RuntimeOciSpec, "", "    ")
		return json, nil
	}
	return []byte(f.data), nil
}

func (f *File) Write(ctx context.Context, req *fuse.WriteRequest, resp *fuse.WriteResponse) error {
	data := strings.TrimSpace(string(req.Data))
	v := ctx.Value(&contextKey).(*Testing)
	if f.name == "image" {
		st, err := os.Stat(data)
		if err != nil {
			return fmt.Errorf("stat on %s failed", data)
		}

		if st.IsDir() == false {
			C.singularity_config_init()

			imageObject := C.singularity_image_init(C.CString(data), 0)

			info := new(loop.Info64)
			mountType := ""

			switch C.singularity_image_type(&imageObject) {
			case 1:
				mountType = "squashfs"
				info.Offset = uint64(C.uint(imageObject.offset))
				info.SizeLimit = uint64(C.uint(imageObject.size))
			case 2:
				mountType = "ext3"
				info.Offset = uint64(C.uint(imageObject.offset))
				info.SizeLimit = uint64(C.uint(imageObject.size))
			}
			var number int
			info.Flags = loop.FlagsAutoClear
			number, err := v.rpc.LoopDevice(data, os.O_RDONLY, *info)
			if err != nil {
				return err
			}

			path := fmt.Sprintf("/dev/loop%d", number)
			sylog.Debugf("Mounting loop device %s\n", path)
			_, err = v.rpc.Mount(path, buildcfg.CONTAINER_FINALDIR, mountType, syscall.MS_NOSUID|syscall.MS_RDONLY|syscall.MS_NODEV, "errors=remount-ro")
			if err != nil {
				return fmt.Errorf("failed to mount %s filesystem: %s", mountType, err)
			}
		} else {
			sylog.Debugf("Mounting image directory %s\n", data)
			_, err = v.rpc.Mount(data, buildcfg.CONTAINER_FINALDIR, "", syscall.MS_BIND|syscall.MS_NOSUID|syscall.MS_RDONLY|syscall.MS_NODEV, "errors=remount-ro")
			if err != nil {
				return fmt.Errorf("failed to mount directory filesystem %s: %s", data, err)
			}
		}
	} else if f.name == "chroot" {
		sylog.Debugf("Chroot into %s\n", buildcfg.CONTAINER_FINALDIR)
		_, err := v.rpc.Chroot(buildcfg.CONTAINER_FINALDIR)
		if err != nil {
			return fmt.Errorf("chroot failed: %s", err)
		}
	} else if f.name == "mount" {
		mount := data
		sylog.Debugf("Mounting %s at %s\n", mount, path.Join(buildcfg.CONTAINER_FINALDIR, mount))
		_, err := v.rpc.Mount(mount, path.Join(buildcfg.CONTAINER_FINALDIR, mount), "", syscall.MS_BIND, "")
		if err != nil {
			return fmt.Errorf("mount %s failed: %s", mount, err)
		}
	} else if f.name == "image_unpriv" {
		var wg sync.WaitGroup
		wg.Add(1)
		go func() {
			cmd := exec.Command("/home/ced/linux/tools/lkl/lklfuse", data, buildcfg.CONTAINER_FINALDIR, "-f", "-o", "allow_root,imgoverlay=tmpfs,type=squashfs")
			cmd.SysProcAttr = &syscall.SysProcAttr{Pdeathsig: 9}
			groups, _ := os.Getgroups()
			gr := make([]uint32, len(groups))
			for i, v := range groups {
				gr[i] = uint32(v)
			}
			cmd.SysProcAttr.Credential = &syscall.Credential{
				Uid:    uint32(os.Getuid()),
				Gid:    uint32(os.Getgid()),
				Groups: gr,
			}
			err := cmd.Start()
			if err != nil {
				sylog.Fatalf("%s", err)
			}
			wg.Done()
			err = cmd.Wait()
		}()
		wg.Wait()
	}
	resp.Size = len(req.Data)
	f.data = req.Data
	return nil
}

func (f *File) Flush(ctx context.Context, req *fuse.FlushRequest) error {
	return nil
}
func (f *File) Open(ctx context.Context, req *fuse.OpenRequest, resp *fuse.OpenResponse) (fs.Handle, error) {
	return f, nil
}

func (f *File) Release(ctx context.Context, req *fuse.ReleaseRequest) error {
	return nil
}

func (f *File) Fsync(ctx context.Context, req *fuse.FsyncRequest) error {
	return nil
}

type Testing struct {
	engine *runtime.Engine
	rpc    *client.RPC
}

var contextKey int

func StartDaemon(engine *runtime.Engine, rpcConn net.Conn, fwg *sync.WaitGroup) error {
	mountpoint := buildcfg.SESSIONDIR

	c, err := fuse.Mount(
		mountpoint,
		fuse.FSName("ocifs"),
		fuse.Subtype("ocifs"),
		fuse.LocalVolume(),
		fuse.DefaultPermissions(),
	)
	if err != nil {
		log.Fatal(err)
	}
	defer c.Close()
	if p := c.Protocol(); !p.HasInvalidate() {
		return fmt.Errorf("kernel FUSE support is too old to have invalidations: version %v", p)
	}

	rpcOps := &client.RPC{
		Client: rpc.NewClient(rpcConn),
		Name:   engine.RuntimeSpec.RuntimeName,
	}
	if rpcOps.Client == nil {
		return fmt.Errorf("failed to initialiaze RPC client")
	}
	testing := &Testing{engine, rpcOps}

	config := &fs.Config{
		WithContext: func(ctx context.Context, req fuse.Request) context.Context {
			return context.WithValue(ctx, &contextKey, testing)
		},
	}
	srv := fs.New(c, config)
	filesys := &FS{
		&Dir{Node: Node{name: "head", inode: NewInode()}, files: &[]*File{
			&File{Node: Node{name: "image", mode: 0220, inode: NewInode()}, data: []byte("")},
			&File{Node: Node{name: "image_unpriv", mode: 0220, inode: NewInode()}, data: []byte("")},
			&File{Node: Node{name: "mount", mode: 0220, inode: NewInode()}, data: []byte("")},
			&File{Node: Node{name: "config.json", mode: 0440, inode: NewInode()}, data: []byte("")},
		}, directories: nil,
		},
	}

	fwg.Done()
	if err := srv.Serve(filesys); err != nil {
		log.Panicln(err)
	}
	// Check if the mount process has an error to report.
	<-c.Ready
	if err := c.MountError; err != nil {
		log.Panicln(err)
	}
	return nil
}
