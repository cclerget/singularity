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
#include "util/network.h"

static int notify_event_call(pid_t child);

static struct singularity_event notify_event = {
    .name = "notify",
    .call = notify_event_call,
    .exit = NULL,
    .fd   = -1,
};


// cleanup host interface/bridge/iptables rules
static struct singularity_event network_cleanup_event = {
    .name = "network-cleanup",
    .call = NULL,
    .exit = singularity_network_cleanup,
    .fd   = -1
};

int notify_event_init(pid_t child) {
    notify_event.fd = proc_notify_get_fd();
    return singularity_event_register(&notify_event);
}

static int notify_event_call(pid_t child) {
    int notify;

    notify = proc_notify_recv();
    if ( notify < 0 ) {
        return(notify);
    }

    if ( notify == NOTIFY_SET_NETNS ) {
        if ( singularity_network_setup(child) < 0 ) {
            return proc_notify_send(NOTIFY_ERROR);
        }

        singularity_event_register(&network_cleanup_event);
        return proc_notify_send(NOTIFY_OK);
    }
    if ( notify == NOTIFY_SET_CGROUP ) {
        return proc_notify_send(NOTIFY_OK);
    }

    return EVENT_NOTIFY(notify);
}
