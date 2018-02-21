#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "beamer_mod.h"
#include "beamer_hooks.h"
#include "beamer_srv.h"
#include "beamer_sysfs.h"
#include "beamer_pm.h"
#include "beamer_bucket_table.h"

//TODO: license
MODULE_LICENSE("GPL");
MODULE_AUTHOR("VLADIMIR OLTEANU");

/* module params */
ulong ring_size = 0;
__be32 dip;
__be32 vip;
uint16_t id;

char *dip_string = NULL;
char *vip_string = NULL;

#define BEAMER_DEFAULT_DAISY_TIMEOUT ( 4 * 60 ) // 4 min

int32_t daisy_timeout = BEAMER_DEFAULT_DAISY_TIMEOUT;


/* other global stuff */
struct beamer_counters beamer_stats;


struct beamer_private
{
	struct pci_dev *pdev;
};

static const struct pci_device_id  beamer_pci_driver_ids[] = {
	{},
};

MODULE_DEVICE_TABLE(pci,beamer_pci_driver_ids);

static const struct sysfs_ops beamer_ops = {
	.store = beamer_sysfs_write,
	.show  = beamer_sysfs_read,
};

static const struct attribute beamer_attr[] = {
	{
		.name = "add_srv",
		.mode = 0200,
	},
	{
		.name = "remove_srv",
		.mode = 0200,
	},
	{
		.name = "daisy_timeout",
		.mode = 0200,
	},
	{
		.name = "counters",
		.mode = 0400,
	},
};

#define BEAMER_N_FILES (sizeof(beamer_attr) / sizeof(struct attribute))

struct kobj_type beamer_kobj_type	= {
	.sysfs_ops = &beamer_ops,
	.default_attrs = (struct attribute **)&beamer_attr
};

struct kobject *beamer_kobj;

static int beamer_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct beamer_private *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
	{

		dev_err(&pdev->dev, "Unable to allocate memory for pci private\n");
		return -ENOMEM;
	}

	priv->pdev = pdev;
	pci_set_drvdata(pdev, priv);

	return 0;
}

static void beamer_remove(struct pci_dev *pdev)
{
	struct beamer_private *priv = pci_get_drvdata(pdev);

	kfree(priv);
}

static struct pci_driver beamer_pci_driver ={
	.name		= DRIVER_NAME,
	.probe		= beamer_probe,
	.remove		= beamer_remove,
	.id_table	= beamer_pci_driver_ids,

};

#define BEAMER_N_NF_HOOKS 2

static struct nf_hook_ops beamer_hook_ops[] = {
	{
		.hook = beamer_in_hookfn,
		.owner = THIS_MODULE,
		.pf = PF_INET,
		.hooknum = NF_INET_PRE_ROUTING,
		.priority = NF_IP_PRI_FIRST,
	},
	{
		.hook = beamer_out_hookfn,
		.owner = THIS_MODULE,
		.pf = PF_INET,
		.hooknum = NF_INET_LOCAL_OUT,
		.priority = NF_IP_PRI_FIRST,
	},
};

static struct mptcp_pm_ops beamer_pathmgr_ops __read_mostly = {
	.new_session = beamer_pm_new_session,
	.get_local_id = beamer_pm_get_local_id,
	.addr_signal = beamer_pm_addr_signal,
	.name = "beamer",
	.owner = THIS_MODULE,
};

#define MISSING_ARG(name) PERR("Missing argument %s: ", name)
#define BAD_ARG(name, fmt, val) PERR("Bad argument %s: " fmt, name, val)

static int beamer_check_params(void)
{
	/* bad or missing ring size */
	if (!ring_size)
	{
		BAD_ARG("ring_size", "%lu", ring_size);
		return -EINVAL;
	}
	
	/* bad or missing id */
	if (!id)
	{
		MISSING_ARG("id");
		return -EINVAL;
	}

	/* missing dip/vip/srvs */
	if (!dip_string)
	{
		MISSING_ARG("dip");
		return -EINVAL;
	}
	if (!vip_string)
	{
		MISSING_ARG("vip");
		return -EINVAL;
	}

	dip = in_aton(dip_string);
	if (!dip)
	{
		BAD_ARG("dip", "%s", dip_string);
		return -EINVAL;
	}
	vip = in_aton(vip_string);
	if (!vip)
	{
		BAD_ARG("vip", "%s", vip_string);
		return -EINVAL;
	}

	return 0;
}

static int __init beamer_init(void)
{
	int res;
	int i;

	res = beamer_check_params();
	if (res < 0)
		goto err_quit;

	res = beamer_bucket_table_init();
	if (res < 0)
		goto err_quit;
	
	beamer_srv_init();

	res = pci_register_driver(&beamer_pci_driver);
	if (res < 0)
	{
		PERR("Adding driver to pci core failed\n");
		goto err_bucket_table_del;
	}

	beamer_kobj = kobject_create_and_add("beamer", NULL);
	if (!beamer_kobj)
	{
		res = -ENOMEM;
		goto err_unreg;
	}
	beamer_kobj->ktype = &beamer_kobj_type;

	for (i = 0; i < BEAMER_N_FILES; i++)
	{
		res = sysfs_create_file(beamer_kobj, &beamer_attr[i]);
		if (res < 0)
			goto err_rm;
	}

	res = nf_register_hooks(beamer_hook_ops, BEAMER_N_NF_HOOKS);
	if (res < 0)
		goto err_rm;
	
	res = mptcp_register_path_manager(&beamer_pathmgr_ops);
	if (res < 0)
		goto err_unreg_hooks;

	return 0;
	
err_unreg_hooks:
	nf_unregister_hooks(beamer_hook_ops, BEAMER_N_NF_HOOKS);

err_rm:
	for (i = 0; i < BEAMER_N_FILES; i++)
		sysfs_remove_file(beamer_kobj, &beamer_attr[i]);

	kobject_del(beamer_kobj);

err_unreg:
	pci_unregister_driver(&beamer_pci_driver);
	
err_bucket_table_del:
	beamer_bucket_table_destroy();
	
err_quit:
	return res;
}

static void __exit beamer_exit(void)
{
	int i;

	mptcp_unregister_path_manager(&beamer_pathmgr_ops);
	
	nf_unregister_hooks(beamer_hook_ops, BEAMER_N_NF_HOOKS);

	for (i = 0; i < BEAMER_N_FILES; i++)
		sysfs_remove_file(beamer_kobj, &beamer_attr[i]);

	kobject_del(beamer_kobj);

	pci_unregister_driver(&beamer_pci_driver);
	
	beamer_bucket_table_destroy();
}

module_init(beamer_init);
module_exit(beamer_exit);

module_param      (ring_size,             ulong,  S_IRUGO);
module_param_named(dip,       dip_string, charp,  S_IRUGO);
module_param_named(vip,       vip_string, charp,  S_IRUGO);
module_param      (id,                    ushort, S_IRUGO);
