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
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sched.h>

#include "./notify.h"

#include "util/registry.h"
#include "util/message.h"
#include "util/config_parser.h"
#include "util/proc_notify.h"
#include "util/capability.h"

static int notify_event_call(pid_t child);

static struct singularity_event notify_event = {
    .name = "notify",
    .call = notify_event_call,
    .exit = NULL,
    .fd   = -1,
};

/*
// cleanup host interface/bridge/iptables rules
static struct singularity_event netns_cleanup_event = {
    .name = "netns-cleanup",
    .call = NULL,
    .exit = singularity_netns_cleanup,
    .fd   = -1
};*/

int notify_event_init(pid_t child) {
    notify_event.fd = proc_notify_get_fd();
    return singularity_event_register(&notify_event);
}

static int wait_child(pid_t child) {
    int status;
    singularity_event_signal_ignore_child(child);
    waitpid(child, &status, 0);
    return WEXITSTATUS(status);
}

static int notify_event_call(pid_t child) {
    int notify;

    notify = proc_notify_recv();
    if ( notify < 0 ) {
        return(notify);
    }

    if ( notify == NOTIFY_SET_NETNS ) {
        char *network_script = joinpath(LIBEXECDIR, "/singularity/network");
        pid_t forked = fork();
        if ( forked == 0 ) {
            setresuid(0, 0, 0);
            setresgid(0, 0, 0);
            singularity_capability_init_network();
            execl("/bin/bash", "/bin/bash", "--norc", "--noprofile", "--restricted", network_script, int2str(child), NULL);
        } else if ( forked > 0 ) {
            if ( wait_child(forked) == 0 ) {
                pid_t forked = fork();
                if ( forked == 0 ) {
                    char netns[1024];
                    sprintf(netns, "/proc/%d/ns/net", child);
                    setresuid(0, 0, 0);
                    setresgid(0, 0, 0);
                    int fd = open(netns, O_RDONLY);
                    setns(fd, CLONE_NEWNET);
                    close(fd);
                    singularity_capability_init_network();
                    execl("/bin/bash", "/bin/bash", "--norc", "--noprofile", "--restricted", network_script, NULL);
                } else {
                    if ( wait_child(forked) == 0 )
                        return proc_notify_send(NOTIFY_OK);
                }
            }
        }
        return proc_notify_send(NOTIFY_ERROR);
    }
    if ( notify == NOTIFY_SET_CGROUP ) {
        return proc_notify_send(NOTIFY_OK);
    }

    return EVENT_NOTIFY(notify);
}
