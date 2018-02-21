#ifndef BEAMER_MOD_H
#define BEAMER_MOD_H

#include <linux/types.h>
#include <linux/hashtable.h>
#include <linux/kobject.h>
#include <linux/pci.h>
#include <linux/inet.h>
#include <linux/slab.h>

#define DRIVER_NAME "beamer"
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "%s:" fmt, DRIVER_NAME, ##args)
#define PERR(fmt, args...)   printk(KERN_ERR   "%s:" fmt, DRIVER_NAME, ##args)
#define PINFO(fmt, args...)  printk(KERN_INFO  "%s:" fmt, DRIVER_NAME, ##args)

#define BEAMER_HASHFN_BOB 0
#define BEAMER_HASHFN_CRC 1

#ifndef BEAMER_HASHFN
#define BEAMER_HASHFN BEAMER_HASHFN_CRC
#endif

struct beamer_counters
{
	struct
	{
		atomic_t in;
		atomic_t chained;
		atomic_t out;
	} pkts, bytes;
};

/* args */
extern ulong ring_size;
extern __be32 vip;
extern __be32 dip;
extern uint16_t id;
extern int32_t daisy_timeout;

/* stuff */
extern struct beamer_counters beamer_stats;

#endif /* BEAMER_MOD_H */
