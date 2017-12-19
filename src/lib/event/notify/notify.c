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
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/signalfd.h>

#include "./notify.h"

#include "util/registry.h"
#include "util/message.h"
#include "util/config_parser.h"
#include "util/proc_notify.h"


int notify_event_init(struct singularity_event *event, pid_t child) {
    event->fd = proc_notify_get_fd();
    return(0);
}

int notify_event_call(struct singularity_event *event, pid_t child) {
    int notify;

    notify = proc_notify_recv();
    if ( notify < 0 ) return(notify);

    if ( notify == NOTIFY_SET_NETNS ) {
        return proc_notify_send(NOTIFY_OK);
    }
    if ( notify == NOTIFY_SET_CGROUP ) {
        return proc_notify_send(NOTIFY_OK);
    }

    return EVENT_NOTIFY(notify);
}

int notify_event_exit(struct singularity_event *event, pid_t child) {
    return(0);
}
