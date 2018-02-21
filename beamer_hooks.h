#ifndef BEAMER_HOOKS_H
#define BEAMER_HOOKS_H

#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/skbuff.h>
#include <net/ip_tunnels.h>
#include "beamer_mod.h"

unsigned int beamer_in_hookfn(const struct nf_hook_ops *ops,
	struct sk_buff *skb,
	const struct net_device *in,
	const struct net_device *out,
	int (*okfn)(struct sk_buff *));

unsigned int beamer_out_hookfn(const struct nf_hook_ops *ops,
	struct sk_buff *skb,
	const struct net_device *in,
	const struct net_device *out,
	int (*okfn)(struct sk_buff *));

#endif /* BEAMER_HOOKS_H */
