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
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "util/util.h"
#include "util/proc_notify.h"

/*
 * Setup parent/child communication process
 */

static int child_pipes[2];
static int parent_pipes[2];

void proc_notify_init(void) {
    if ( pipe2(child_pipes, O_CLOEXEC ) < 0 ) {
        singularity_message(ERROR, "Failed to create child communication pipe\n");
        ABORT(255);
    }
    if ( pipe2(parent_pipes, O_CLOEXEC ) < 0 ) {
        singularity_message(ERROR, "Failed to create parent communication pipe\n");
        ABORT(255);
    }
}

void proc_notify_child_init(void) {
    /* write to parent */
    close(child_pipes[0]);
    child_pipes[0] = -1;
    /* read from parent */
    close(parent_pipes[1]);
    parent_pipes[1] = -1;
}

void proc_notify_parent_init(void) {
    /* read from child */
    close(child_pipes[1]);
    child_pipes[1] = -1;
    /* write to child */
    close(parent_pipes[0]);
    parent_pipes[0] = -1;
}

int proc_notify_send(int value) {
    int fd = child_pipes[1];

    /* we are in the parent process */
    if ( fd == -1 ) {
        fd = parent_pipes[1];
    }

    if ( write(fd, &value, sizeof(int)) != sizeof(int) ) {
        return(-1);
    }
    return(0);
}

int proc_notify_recv(void) {
    int fd = child_pipes[0];
    int value;

    if ( fd == -1 ) {
        fd = parent_pipes[0];
    }

    if ( read(fd, &value, sizeof(int)) != sizeof(int) ) {
        return(-1);
    }
    return value;
}

int proc_notify_get_fd(void) {
    int fd = child_pipes[0];

    if ( fd == -1 ) {
        fd = parent_pipes[0];
    }

    return(fd);
}

void proc_notify_close(void) {
    int fd = child_pipes[0];

    if ( fd == -1 ) {
        close(parent_pipes[0]);
        close(child_pipes[1]);
    } else {
        close(child_pipes[0]);
        close(parent_pipes[1]);
    }
}
