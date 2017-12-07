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
    int (*function)(int, char **, struct image_object *);
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
        .function     = NULL,
        .capinit    = NULL,
        .cmdflags   = 0,
        .nsflags    = 0
    }
};

static void start_fork_sync(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

static void stop_fork_sync(void) {
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

static int wait_parent(pid_t *mypid) {
    int retval = -1;
    pid_t parent = getppid();
    sigset_t mask;
    siginfo_t ac;

    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);

    while(1) {
        sigwaitinfo(&mask, &ac);
        if ( parent == ac.si_pid ) {
            *mypid = (pid_t)ac.si_value.sival_int;
            retval = 0;
        }
        break;
    }
    stop_fork_sync();
    return(retval);
}

static void start_child(pid_t child) {
    union sigval sv;

    sv.sival_int = child;
    sigqueue(child, SIGALRM, sv);
    stop_fork_sync();
}

struct image_object open_image(void) {
    if ( singularity_registry_get("WRITABLE") != NULL ) {
        singularity_message(VERBOSE3, "Instantiating writable container image object\n");
        return singularity_image_init(singularity_registry_get("IMAGE"), O_RDWR);
    } else {
        singularity_message(VERBOSE3, "Instantiating read only container image object\n");
        return singularity_image_init(singularity_registry_get("IMAGE"), O_RDONLY);
    }
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

    if ( cmd_wrapper[index].cmdflags & CMD_NOFORK ) {
        struct image_object image;

        singularity_runtime_autofs();
        image = open_image();
        singularity_runtime_ns(cmd_wrapper[index].nsflags);
        return(cmd_wrapper[index].function(argc, argv, &image));
    } else if ( cmd_wrapper[index].cmdflags & CMD_FORK ) {
        pid_t child;

        start_fork_sync();

        singularity_runtime_ns(SR_NS_PID);
        cmd_wrapper[index].nsflags &= ~SR_NS_PID;

        if ( singularity_registry_get("PIDNS_ENABLED") ) {
            child = singularity_fork(CLONE_NEWPID);
        } else {
            child = singularity_fork(0);
        }
        if ( child == 0 ) {
            struct image_object image;
            pid_t child_pid = 0;

            if ( wait_parent(&child_pid) < 0 ) {
                singularity_message(ERROR, "Receive signal from non parent\n");
                ABORT(255);
            }

            singularity_message(DEBUG, "Executing command with PID %d\n", child_pid);

            singularity_runtime_autofs();

            image = open_image();
            singularity_runtime_ns(cmd_wrapper[index].nsflags);
            return(cmd_wrapper[index].function(argc, argv, &image));
        } else if ( child > 0 ) {
            int status;
            int retval = -1;
            char *cleanup_dir = singularity_registry_get("CLEANUPDIR");

            start_child(child);

            singularity_priv_escalate();
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
            return(retval);
        }
    } else if ( cmd_wrapper[index].cmdflags & CMD_DAEMON ) {
        pid_t child;

        singularity_fork_daemonize(0);

        start_fork_sync();

        singularity_runtime_ns(SR_NS_PID);
        cmd_wrapper[index].nsflags &= ~SR_NS_PID;

        if ( chdir("/") < 0 ) {
            singularity_message(ERROR, "Can't change directory to /\n");
        }
        if ( setsid() < 0 ) {
            singularity_message(ERROR, "Can't set session leader\n");
            ABORT(255);
        }
        umask(0);

        if ( singularity_registry_get("PIDNS_ENABLED") ) {
            singularity_priv_escalate();
            child = fork_ns(CLONE_NEWPID);
            singularity_priv_drop();
        } else {
            child = fork();
        }

        if ( child == 0 ) {
            struct image_object image;
            pid_t child_pid = 0;

            if ( wait_parent(&child_pid) < 0 ) {
                singularity_message(ERROR, "Receive signal from non parent\n");
                ABORT(255);
            }

            singularity_message(DEBUG, "Executing command with PID %d\n", child_pid);

            singularity_runtime_autofs();
            image = open_image();
            singularity_runtime_ns(cmd_wrapper[index].nsflags);
            return(cmd_wrapper[index].function(argc, argv, &image));
        } else {
            int status;
            int retval = -1;
            char *cleanup_dir = singularity_registry_get("CLEANUPDIR");

            start_child(child);

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
            return(retval);
        }
    }

    singularity_message(ERROR, "Failed to execute command %s\n", command);
    ABORT(255);

    return(0);
}
