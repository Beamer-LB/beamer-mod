#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "beamer_bucket_table.h"
#include "beamer_mod.h"

static volatile struct beamer_bucket_info *buckets;

DEFINE_SPINLOCK(beamer_bucket_table_lock);

int beamer_bucket_table_init()
{
	int i;
	
	buckets = vmalloc(ring_size * sizeof(struct beamer_bucket_info));
	if (!buckets)
		return -ENOMEM;
	
	for (i = 0; i < ring_size; i++)
	{
		buckets[i].ds_dip = 0;
		buckets[i].ts = 0;
	}
	
	return 0;
}

volatile struct beamer_bucket_info *beamer_bucket_table_get(u32 hash)
{
	return &buckets[hash];
}

void beamer_bucket_table_destroy(void)
{
	vfree((void *)buckets);
}
