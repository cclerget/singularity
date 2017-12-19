/* 
 * Copyright (c) 2017, SingularityWare, LLC. All rights reserved.
 * 
 * This software is licensed under a 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 */

#ifndef __SINGULARITY_PROC_NOTIFY_H_
#define __SINGULARITY_PROC_NOTIFY_H_

#define NOTIFY_OK             0
#define NOTIFY_ERROR          1
#define NOTIFY_CONTINUE       2
#define NOTIFY_DETACH         3
#define NOTIFY_SET_NETNS      4
#define NOTIFY_SET_CGROUP     5

void proc_notify_init(void);
void proc_notify_child_init(void);
void proc_notify_parent_init(void);
int proc_notify_send(int value);
int proc_notify_recv(void);
int proc_notify_get_fd(void);
void proc_notify_close(void);

#endif /* __SINGULARITY_PROC_NOTIFY_H_ */
