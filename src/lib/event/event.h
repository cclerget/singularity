/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 * 
 * This software is licensed under a 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 */

#ifndef __SINGULARITY_EVENT_H_
#define __SINGULARITY_EVENT_H_

#define EVENT_CODE(code)        (code & 0xFF)

#define EVENT_EXIT(code)        (EVENT_CODE(code) | (1 << 8))
#define EVENT_SIGNAL(code)      (EVENT_CODE(code) | (1 << 9))
#define EVENT_NOTIFY(code)      (EVENT_CODE(code) | (1 << 10))
#define EVENT_FAIL(code)        (EVENT_CODE(code) | (1 << 11)) 

#define EVENT_EXITED(code)      (code & (1 << 8))
#define EVENT_SIGNALED(code)    (code & (1 << 9))
#define EVENT_NOTIFIED(code)    (code & (1 << 10))
#define EVENT_FAILED(code)      (code & (1 << 11))

struct singularity_event {
    char *name;
    int (*init)(struct singularity_event *, pid_t);
    int (*exit)(struct singularity_event *, pid_t);
    int (*call)(struct singularity_event *, pid_t);
    int fd;
    int events;
};

int singularity_event_init(pid_t);
int singularity_event_call(pid_t);
int singularity_event_exit(pid_t);

#endif
