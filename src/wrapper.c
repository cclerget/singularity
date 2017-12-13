/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 * 
 * This software is licensed under a 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <signal.h>
#include <sched.h>
#include <poll.h>

#include "config.h"
#include "util/file.h"
#include "util/util.h"
#include "util/fork.h"
#include "util/registry.h"
#include "util/config_parser.h"
#include "util/capability.h"
#include "util/privilege.h"
#include "util/suid.h"
#include "util/signal.h"
#include "lib/image/image.h"
#include "lib/runtime/runtime.h"
#include "command/command.h"

#ifndef SYSCONFDIR
#error SYSCONFDIR not defined
#endif

#define MOUNT_FUNC      singularity_command_mount
#define START_FUNC      singularity_command_start
#define ACTION_FUNC     singularity_command_action

#define CMD_BS(x)       (1 << (x))

#define CMD_NOFORK      CMD_BS(0)
#define CMD_FORK        CMD_BS(1)
#define CMD_DAEMON      CMD_BS(2)

extern char **environ;

struct cmd_wrapper {
    char *command;
    int (*function)(int, char **, unsigned int);
    void (*capinit)(void);
    unsigned int cmdflags;
    unsigned int nsflags;
};

struct cmd_wrapper cmd_wrapper[] = {
    {
        .command    = "shell",
        .function   = ACTION_FUNC,
        .capinit    = singularity_capability_init,
        .cmdflags   = CMD_FORK,
        .nsflags    = SR_NS_ALL
    },{
        .command    = "exec",
        .function   = ACTION_FUNC,
        .capinit    = singularity_capability_init,
        .cmdflags   = CMD_FORK,
        .nsflags    = SR_NS_ALL
    },{
        .command    = "run",
        .function   = ACTION_FUNC,
        .capinit    = singularity_capability_init,
        .cmdflags   = CMD_FORK,
        .nsflags    = SR_NS_ALL
    },{
        .command    = "test",
        .function   = ACTION_FUNC,
        .capinit    = singularity_capability_init,
        .cmdflags   = CMD_FORK,
        .nsflags    = SR_NS_ALL
    },{
        .command    = "mount",
        .function   = MOUNT_FUNC,
        .capinit    = singularity_capability_init_default,
        .cmdflags   = CMD_NOFORK,
        .nsflags    = SR_NS_MNT
    },{
        .command    = "help",
        .function   = MOUNT_FUNC,
        .capinit    = singularity_capability_init_default,
        .cmdflags   = CMD_NOFORK,
        .nsflags    = SR_NS_MNT
    },{
        .command    = "apps",
        .function   = MOUNT_FUNC,
        .capinit    = singularity_capability_init_default,
        .cmdflags   = CMD_NOFORK,
        .nsflags    = SR_NS_MNT
    },{
        .command    = "inspect",
        .function   = MOUNT_FUNC,
        .capinit    = singularity_capability_init_default,
        .cmdflags   = CMD_NOFORK,
        .nsflags    = SR_NS_MNT
    },{
        .command    = "check",
        .function   = MOUNT_FUNC,
        .capinit    = singularity_capability_init_default,
        .cmdflags   = CMD_NOFORK,
        .nsflags    = SR_NS_MNT
    },{
        .command    = "image.import",
        .function   = MOUNT_FUNC,
        .capinit    = singularity_capability_init_default,
        .cmdflags   = CMD_NOFORK,
        .nsflags    = SR_NS_MNT
    },{
        .command    = "image.export",
        .function   = MOUNT_FUNC,
        .capinit    = singularity_capability_init_default,
        .cmdflags   = CMD_NOFORK,
        .nsflags    = SR_NS_MNT
    },{
        .command    = "instance.start",
        .function   = START_FUNC,
        .capinit    = singularity_capability_init,
        .cmdflags   = CMD_DAEMON,
        .nsflags    = SR_NS_ALL
    },{
        .command    = NULL,
        .function   = NULL,
        .capinit    = NULL,
        .cmdflags   = 0,
        .nsflags    = 0
    }
};

int sync_pipe[2];
int pipe_size;

static void start_fork_sync(void) {
    if ( pipe(sync_pipe) < 0 ) {
        singularity_message(ERROR, "Can't install fork sync pipe\n");
        ABORT(255);
    }
}

static void wait_parent(void) {
    struct pollfd pfd;

    /*
       this will be reset on next uid change, so a race persist if
       parent die during child initialization
     */
    if ( prctl(PR_SET_PDEATHSIG, SIGKILL) < 0 ) {
        singularity_message(ERROR, "Failed to parent death signal\n");
        ABORT(255);
    }

    close(sync_pipe[1]);

    pfd.fd = sync_pipe[0];
    pfd.events = POLLIN;
    pfd.revents = 0;

    while( poll(&pfd, 1, 1000) >= 0 ) {
        /* waiting parent breaks the pipe */
        if ( pfd.revents & POLLHUP ) {
            break;
        }
    }
    if ( ! (pfd.revents & POLLIN) ) {
        kill(getpid(), SIGKILL);
    }
    close(sync_pipe[0]);
}

static void start_child(void) {
    close(sync_pipe[0]);
    if ( write(sync_pipe[1], "go", 2) != 2 ) {
        singularity_message(ERROR, "Failed to send child sync\n");
        ABORT(255);
    }
    close(sync_pipe[1]);
}

int main(int argc, char **argv) {
    int index;
    char *command;

    singularity_registry_init();
    singularity_config_init();

    command = singularity_registry_get("COMMAND");
    if ( command == NULL ) {
        singularity_message(ERROR, "no command passed\n");
        ABORT(255);
    }

    for ( index = 0; cmd_wrapper[index].command != NULL; index++) {
        if ( strcmp(command, cmd_wrapper[index].command) == 0 ) {
            break;
        }
    }

    if ( cmd_wrapper[index].command == NULL ) {
        singularity_message(ERROR, "unknown command %s\n", command);
        ABORT(255);
    }

    /* if allow setuid is no or nosuid requested fallback to non suid command */
    if ( singularity_suid_init() < 0) {
        singularity_priv_init();
        singularity_priv_drop_perm();

        if ( singularity_suid_enabled() ) {
            char *libexec_bin = joinpath(LIBEXECDIR, "/singularity/bin/");
            char *binary = strjoin(libexec_bin, "wrapper");

            execve(binary, argv, environ); // Flawfinder: ignore

            singularity_message(ERROR, "Failed to execute wrapper\n");
            ABORT(255);
        }

        singularity_runtime_ns(SR_NS_USER);
        cmd_wrapper[index].nsflags &= ~SR_NS_USER;
    } else {
        singularity_priv_init();
        cmd_wrapper[index].capinit();
        singularity_priv_drop();
    }

    singularity_message(DEBUG, "Executing command %s\n", command);

    if ( cmd_wrapper[index].cmdflags & CMD_NOFORK ) {
        return(cmd_wrapper[index].function(argc, argv, cmd_wrapper[index].nsflags));
    } else if ( cmd_wrapper[index].cmdflags & CMD_FORK ) {
        pid_t child;

        start_fork_sync();

        singularity_runtime_ns(SR_NS_PID);
        cmd_wrapper[index].nsflags &= ~SR_NS_PID;

        if ( singularity_registry_get("PIDNS_ENABLED") ) {
            singularity_priv_escalate();
            child = fork_ns(CLONE_NEWPID);
            singularity_priv_drop();
        } else {
            child = fork();
        }
        if ( child == 0 ) {
            wait_parent();

            return(cmd_wrapper[index].function(argc, argv, cmd_wrapper[index].nsflags));
        } else if ( child > 0 ) {
            int status;
            int retval = -1;
            char *cleanup_dir = singularity_registry_get("CLEANUPDIR");

            start_child();

            while(1) {
                waitpid(child, &status, 0);
                if ( WIFEXITED(status) || WIFSIGNALED(status) ) {
                    retval = WEXITSTATUS(status);
                    break;
                }
            }
            if ( cleanup_dir ) {
                if ( s_rmdir(cleanup_dir) < 0 ) {
                    singularity_message(WARNING, "Can't delete cleanup dir %s\n", cleanup_dir);
                }
            }
            if ( WIFSIGNALED(status) ) {
                kill(getpid(), SIGKILL);
            }
            return(retval);
        }
    } else if ( cmd_wrapper[index].cmdflags & CMD_DAEMON ) {
        struct tempfile *stdout_log, *stderr_log;
        pid_t grandchild;
        pid_t child;
        int i;
        struct stat filestat;

        stdout_log = make_logfile("stdout");
        stderr_log = make_logfile("stderr");

        grandchild = singularity_fork(0);

        if ( grandchild > 0 ) {
            int code = singularity_wait_for_go_ahead();
            char *buffer;
            FILE *errlog;

            if ( code == 0 ) {
                singularity_message(DEBUG, "Successfully spawned daemon, waiting for signal_go_ahead from child\n");
                return(0);
            }

            /* display error log */
            errlog = fopen(stderr_log->filename, "r");
            if ( errlog == NULL ) {
                singularity_message(ERROR, "Can't display log\n");
                ABORT(code);
            }

            buffer = (char *)calloc(sizeof(char), 1024);
            if ( buffer == NULL ) {
                singularity_message(ERROR, "Can't allocate 1024 memory bytes\n");
                ABORT(code);
            }

            while ( fgets(buffer, 1023, errlog) ) {
                printf("%s", buffer);
            }

            fclose(errlog);

            unlink(stdout_log->filename);
            unlink(stderr_log->filename);

            return(code);
        } else if ( grandchild < 0 ) {
            singularity_message(ERROR, "Failed to spawn daemon process\n");
            ABORT(255);
        }

        start_fork_sync();

        singularity_runtime_ns(SR_NS_PID);
        cmd_wrapper[index].nsflags &= ~SR_NS_PID;

        if ( chdir("/") < 0 ) {
            singularity_message(ERROR, "Can't change directory to /\n");
            ABORT(255);
        }
        if ( setsid() < 0 ) {
            singularity_message(ERROR, "Can't set session leader\n");
            ABORT(255);
        }
        umask(0);

        /* close standard stream and redirect stdout/stderr to file */
        close(STDIN_FILENO);
        if ( stdout_log != NULL ) {
            if ( -1 == dup2(stdout_log->fd, STDOUT_FILENO) ) {
                singularity_message(ERROR, "Unable to dup2(): %s\n", strerror(errno));
                ABORT(255);
            }
        }

        if ( stderr_log != NULL ) {
            if ( -1 == dup2(stderr_log->fd, STDERR_FILENO) ) {
                singularity_message(ERROR, "Unable to dup2(): %s\n", strerror(errno));
                ABORT(255);
            }
        }

        /* Close all open fd's that may be present */
        singularity_message(DEBUG, "Closing open fd's\n");
        for( i = sysconf(_SC_OPEN_MAX); i > 2; i-- ) {
            if ( fstat(i, &filestat) == 0 ) {
                if ( S_ISFIFO(filestat.st_mode) != 0 ) {
                    continue;
                }
            }
            close(i);
        }

        if ( singularity_registry_get("PIDNS_ENABLED") ) {
            singularity_priv_escalate();
            child = fork_ns(CLONE_NEWPID);
            singularity_priv_drop();
        } else {
            child = fork();
        }

        if ( child == 0 ) {
            wait_parent();

            return(cmd_wrapper[index].function(argc, argv, cmd_wrapper[index].nsflags));
        } else {
            int status;
            int retval = -1;
            char *cleanup_dir = singularity_registry_get("CLEANUPDIR");

            start_child();

            while(1) {
                waitpid(child, &status, 0);
                if ( WIFEXITED(status) || WIFSIGNALED(status) ) {
                    retval = WEXITSTATUS(status);
                    break;
                }
            }

            if ( cleanup_dir ) {
                if ( s_rmdir(cleanup_dir) < 0 ) {
                    singularity_message(WARNING, "Can't delete cleanup dir %s\n", cleanup_dir);
                }
            }

            if ( WIFEXITED(status) ) {
                singularity_signal_go_ahead(retval);
            } else {
                unlink(stdout_log->filename);
                unlink(stderr_log->filename);
            }
            return(retval);
        }
    }

    singularity_message(ERROR, "Failed to execute command %s\n", command);
    ABORT(255);

    return(0);
}
