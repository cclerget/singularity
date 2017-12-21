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
#include <sys/socket.h>
#include <sys/eventfd.h>

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
#include "util/proc_notify.h"
#include "lib/image/image.h"
#include "lib/runtime/runtime.h"
#include "command/command.h"
#include "lib/event/event.h"

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

#define BOOTED          0xB007ED

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

static volatile int grandchild_exit = 0;

void catch_child_signal(int signo, siginfo_t *siginfo, void *unused) {
    grandchild_exit = 1;
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

        proc_notify_init();

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

            proc_notify_child_init();

            /* wait parent notification before continue */
            if ( proc_notify_recv() != NOTIFY_CONTINUE ) {
                singularity_message(ERROR, "Received bad notification\n");
                ABORT(255);
            }

            return(cmd_wrapper[index].function(argc, argv, cmd_wrapper[index].nsflags));
        } else if ( child > 0 ) {
            int code;

            singularity_event_init(child);

            proc_notify_parent_init();
            /* notify child to continue execution */
            proc_notify_send(NOTIFY_CONTINUE);
exit(0);
            while(1) {
                code = singularity_event_call(child);
                if ( EVENT_EXITED(code) || EVENT_SIGNALED(code) ) {
                    break;
                }
            }

            singularity_event_exit(child);

            if ( EVENT_SIGNALED(code) ) {
                kill(getpid(), SIGKILL);
            }

            return(EVENT_CODE(code));
        }
    } else if ( cmd_wrapper[index].cmdflags & CMD_DAEMON ) {
        struct tempfile *stdout_log, *stderr_log;
        pid_t grandchild;
        pid_t child;
        int i;
        int efd = eventfd(0, EFD_NONBLOCK);

        stdout_log = make_logfile("stdout");
        stderr_log = make_logfile("stderr");

        grandchild = fork();

        if ( grandchild > 0 ) {
            char *buffer;
            FILE *errlog;
            eventfd_t status = 0;
            int code;

            singularity_set_signal_handler(SIGCHLD, &catch_child_signal);

            while ( grandchild_exit == 0 ) {
                eventfd_read(efd, &status);
                if ( status != 0 ) break;
            }

            if ( status == 0 ) {
                code = 255;
            } else {
                code = (int)status;
            }

            if ( code == BOOTED ) {
                singularity_message(DEBUG, "Successfully spawned daemon, waiting for signal_go_ahead from child\n");
                return(0);
            } else {
                singularity_message(ERROR, "Failed to spawn daemon process\n");
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
            free(buffer);

            unlink(stdout_log->filename);
            unlink(stderr_log->filename);

            return(code);
        } else if ( grandchild < 0 ) {
            singularity_message(ERROR, "Failed to create daemon process\n");
            ABORT(255);
        }

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
            if ( i == efd ) {
                continue;
            }
            close(i);
        }

        proc_notify_init();

        if ( singularity_registry_get("PIDNS_ENABLED") ) {
            singularity_priv_escalate();
            child = fork_ns(CLONE_NEWPID);
            singularity_priv_drop();
        } else {
            child = fork();
        }

        if ( child == 0 ) {

            close(efd);
            proc_notify_child_init();

            if ( proc_notify_recv() != NOTIFY_CONTINUE ) {
                singularity_message(ERROR, "Received bad notification\n");
                ABORT(255);
            }

            return(cmd_wrapper[index].function(argc, argv, cmd_wrapper[index].nsflags));
        } else {
            int code;

            singularity_event_init(child);

            proc_notify_parent_init();
            proc_notify_send(NOTIFY_CONTINUE);

            while(1) {
                code = singularity_event_call(child);
                if ( EVENT_NOTIFIED(code) && EVENT_CODE(code) == NOTIFY_DETACH ) {
                    eventfd_write(efd, (eventfd_t)BOOTED);
                    close(efd);
                    efd = -1;
                }
                if ( EVENT_EXITED(code) || EVENT_SIGNALED(code) ) {
                    break;
                }
            }

            singularity_event_exit(child);

            if ( EVENT_EXITED(code) ) {
                if ( efd >= 0 && EVENT_CODE(code) ) {
                    eventfd_write(efd, (eventfd_t)EVENT_CODE(code));
                } else if ( efd >= 0 ) {
                    eventfd_write(efd, (eventfd_t)BOOTED);
                    unlink(stdout_log->filename);
                    unlink(stderr_log->filename);
                }
            }
            if ( EVENT_SIGNALED(code) ) {
                unlink(stdout_log->filename);
                unlink(stderr_log->filename);
            }

            return(EVENT_CODE(code));
        }
    }

    singularity_message(ERROR, "Failed to execute command %s\n", command);
    ABORT(255);

    return(0);
}
