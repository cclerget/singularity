/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 * 
 * This software is licensed under a 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <sched.h>
#include <grp.h>

#include "config.h"
#include "util/util.h"
#include "util/signal.h"
#include "util/fork.h"
#include "util/capability.h"
#include "util/privilege.h"

static int wait_child(pid_t child) {
    int status;
    waitpid(child, &status, 0);
    return WEXITSTATUS(status);
}

static void call_network_script(char *command, pid_t pid, pid_t ppid) {
    char *network_script = joinpath(LIBEXECDIR, "/singularity/network");

    singularity_priv_escalate();

    if ( setreuid(0, 0) < 0 ) {
        exit(-1);
    }
    if ( setregid(0, 0) < 0 ) {
        exit(-1);
    }
    if ( setgroups(0, NULL) < 0 ) {
        exit(-1);
    }

    if ( envclean() != 0 ) {
        exit(-1);
    }

    envar_set("SINGULARITY_NETNS_TYPE", "bridge", 1);
    envar_set("SINGULARITY_NETNS_COMMAND", command, 1);
    envar_set("SINGULARITY_NETNS_PID", int2str(pid), 1);
    envar_set("SINGULARITY_NETNS_PPID", int2str(ppid), 1);
    envar_set("SINGULARITY_NETNS_CONFDIR", joinpath(SYSCONFDIR, "/singularity/network"), 1);
    envar_set("SINGULARITY_NETNS_CONF", "bridge.conf", 1);
    envar_set("SINGULARITY_NETNS_CNIPATH", "/tmp/CNI", 1);
    envar_set("SINGULARITY_NETNS_IFNAME", "eth0", 1);

    execl("/bin/bash", "/bin/bash", "--norc", "--noprofile", network_script, NULL);
    exit(-1);
}

static int switch_network_namespace(pid_t pid) {
    static char netns[64];
    static int fd;

    memset(netns, 0, 64);
    snprintf(netns, 63, "/proc/%d/ns/net", pid);

    singularity_priv_escalate();

    fd = open(netns, O_RDONLY);
    if ( fd < 0 ) {
        return(-1);
    }
    if ( setns(fd, CLONE_NEWNET) < 0 ) {
        return(-1);
    }

    singularity_priv_drop();

    close(fd);

    return(0);
}

int singularity_network_setup(pid_t pid) {
    pid_t forked = fork();

    if ( forked == 0 ) {
        call_network_script("ADD", pid, 0);
    } else if ( forked > 0 ) {
        if ( wait_child(forked) == 0 ) {
            /* hold a reference in container network namespace for cleanup */
            if ( switch_network_namespace(pid) < 0 ) {
                return(-1);
            }
            return(0);
        }
    }
    return(-1);
}

int singularity_network_cleanup(pid_t pid) {
    pid_t parent = getppid();
    pid_t forked = fork();

    if ( forked == 0 ) {
        if ( switch_network_namespace(parent) < 0 ) {
            exit(-1);
        }
        call_network_script("DEL", pid, getppid());
    } else if ( forked > 0 ) {
        if ( wait_child(forked) != 0 ) {
            singularity_message(ERROR, "Network cleanup script returns error\n");
            return(-1);
        }
    } else {
        singularity_message(ERROR, "Failed to call network cleanup script\n");
        return(-1);
    }
}
