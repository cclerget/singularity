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

#include "signalev.h"

#include "util/util.h"
#include "util/signal.h"
#include "util/registry.h"
#include "util/message.h"
#include "util/config_parser.h"


sigset_t sig_mask;

int signal_event_init(struct singularity_event *event, pid_t child) {
    int fd;

    singularity_message(DEBUG, "Creating signal handler\n");

    sigfillset(&sig_mask);
    singularity_mask_signals(&sig_mask, NULL);

    fd = singularity_set_signalfd(sig_mask);
    if ( fd < 0 ) {
        singularity_message(ERROR, "Failed to set signal handler: %s\n", strerror(errno));
        ABORT(255);
    }

    event->fd = fd;

    return(0);
}

int signal_event_call(struct singularity_event *event, pid_t child) {
    int retval;
    int fd = event->fd;
    struct signalfd_siginfo siginfo;

    if ( singularity_wait_signalfd(fd, &siginfo) < 0 ) {
        singularity_message(ERROR, "Failed to read siginfo: %s\n", strerror(errno));
        return EVENT_EXIT(255);
    }

    if ( siginfo.ssi_signo == SIGCHLD ) {
        int status;
        while(1) {
            if ( waitpid(siginfo.ssi_pid, &status, WNOHANG) <= 0 ) break;
        }
        if ( WIFEXITED(status) ) {
            retval = WEXITSTATUS(status);
            return EVENT_EXIT(retval);
        } else if ( WIFSIGNALED(status) ) {
            return EVENT_SIGNAL(255);
        }
    }

    if ( siginfo.ssi_signo == SIGCONT ) {
        return(0);
    }

    /* all other signal cause exit */
    return EVENT_EXIT(255);
}
