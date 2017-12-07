#ifndef __SINGULARITY_COMMAND_H_
#define __SINGULARITY_COMMAND_H_

extern int singularity_command_action(int argc, char **argv, struct image_object *image);
extern int singularity_command_mount(int argc, char **argv, struct image_object *image);
extern int singularity_command_start(int argc, char **argv, struct image_object *image);

#endif /* __SINGULARITY_COMMAND_H */
