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

static pid_t ignored_child = -1;

static int signal_event_call(pid_t child);

static struct singularity_event signal_event = {
    .name = "signal",
    .call = signal_event_call,
    .exit = NULL,
    .fd   = -1,
};

void singularity_event_signal_ignore_child(pid_t child) {
    ignored_child = child;
}

int signal_event_init(pid_t child) {
    singularity_message(DEBUG, "Creating signal handler\n");

    sigfillset(&sig_mask);
    singularity_mask_signals(&sig_mask, NULL);

    signal_event.fd = singularity_set_signalfd(sig_mask);
    if ( signal_event.fd < 0 ) {
        return(-1);
    }

    return singularity_event_register(&signal_event);
}

static int signal_event_call(pid_t child) {
    int retval;
    struct signalfd_siginfo siginfo;

    if ( singularity_wait_signalfd(signal_event.fd, &siginfo) < 0 ) {
        singularity_message(ERROR, "Failed to read siginfo: %s\n", strerror(errno));
        return EVENT_EXIT(255);
    }

    if ( siginfo.ssi_signo == SIGCHLD ) {
        int status;

        if ( siginfo.ssi_pid == ignored_child ) {
            ignored_child = -1;
            return(0);
        }

        while(1) {
            if ( waitpid(siginfo.ssi_pid, &status, WNOHANG) <= 0 ) break;
        }
        if ( siginfo.ssi_pid == child ) {
            if ( WIFEXITED(status) ) {
                retval = WEXITSTATUS(status);
                return EVENT_EXIT(retval);
            } else if ( WIFSIGNALED(status) ) {
                return EVENT_SIGNAL(255);
            }
        }
        /* handle first child others are handled one by one */
        return(0);
    }

    if ( siginfo.ssi_signo == SIGCONT ) {
        return(0);
    }

    /* all other signal cause exit */
    return EVENT_EXIT(255);
}
