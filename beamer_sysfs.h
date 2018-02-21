#ifndef BEAMER_SYSFS_H
#define BEAMER_SYSFS_H

#include "beamer_mod.h"

ssize_t beamer_sysfs_write(struct kobject *kobj, struct attribute *attr, const char *buff, size_t count);
ssize_t beamer_sysfs_read(struct kobject *kobj, struct attribute *attr, char *buff);

#endif /* BEAMER_SYSFS_H */
