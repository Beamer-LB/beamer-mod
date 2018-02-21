#include "beamer_sysfs.h"
#include "beamer_freebsd_bob.h"
#include "beamer_srv.h"

ssize_t beamer_sysfs_write(struct kobject *kobj, struct attribute *attr, const char *buff, size_t count)
{
	int err;

	if (!strcmp(attr->name, "add_srv"))
	{
		uint16_t port;
		
		err = kstrtou16(buff, 10, &port);
		if (err < 0)
			return err;
		
		if (port >= BEAMER_N_PORTS)
			return -EINVAL;
		
		beamer_srv_put(htons(port));
	}
	else if (!strcmp(attr->name, "remove_srv"))
	{
		uint16_t port;
		
		err = kstrtou16(buff, 10, &port);
		if (err < 0)
			return err;
		
		if (port >= BEAMER_N_PORTS)
			return -EINVAL;
		
		beamer_srv_remove(htons(port));
	}
	else if (!strcmp(attr->name, "daisy_timeout"))
	{
		int32_t timeout ;
		
		err = kstrtos32(buff, 10, &timeout);
		if (err < 0)
			return err;
		
		daisy_timeout = timeout;
	}
	else
	{
		PERR("Bad sysfs attribute %s", attr->name);
		return -EINVAL;
	}

	return count;
}

ssize_t beamer_sysfs_read(struct kobject *kobj, struct attribute *attr, char *buff)
{
	ssize_t count = 0;
	
	if (!strcmp(attr->name, "counters"))
	{
		count = sprintf(buff, "\tbytes\tpkts\n"
			"in\t%d\t%d\n"
			"out\t%d\t%d\n"
			"chained\t%d\t%d\n",
			atomic_read(&beamer_stats.bytes.in),      atomic_read(&beamer_stats.pkts.in),
			atomic_read(&beamer_stats.bytes.out),     atomic_read(&beamer_stats.pkts.out),
			atomic_read(&beamer_stats.bytes.chained), atomic_read(&beamer_stats.pkts.chained)
		);

	}
	else
	{
		PERR("Bad sysfs attribute %s", attr->name);
		return -EINVAL;
	}
	
	return count;
}
