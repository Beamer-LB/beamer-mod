#ifndef BEAMER_BUCKET_TABLE_H
#define BEAMER_BUCKET_TABLE_H

#include <linux/hashtable.h>
#include <linux/types.h>

extern spinlock_t beamer_bucket_table_lock;

struct beamer_bucket_info
{
	__be32 ds_dip;
	u32 ts;
};

int beamer_bucket_table_init(void);

volatile struct beamer_bucket_info *beamer_bucket_table_get(u32 hash);

void beamer_bucket_table_destroy(void);


#endif /* BEAMER_BUCKET_TABLE_H */
