#ifndef __SINGULARITY_COMMAND_H_
#define __SINGULARITY_COMMAND_H_

extern int singularity_command_action(int argc, char **argv, unsigned int namespaces);
extern int singularity_command_mount(int argc, char **argv, unsigned int namespaces);
extern int singularity_command_start(int argc, char **argv, unsigned int namespaces);

#endif /* __SINGULARITY_COMMAND_H */
