#ifndef BEAMER_PM_H
#define BEAMER_PM_H

#include <net/mptcp.h>
#include "beamer_mod.h"

int beamer_pm_get_local_id(sa_family_t family, union inet_addr *addr, struct net *net, bool *low_prio);

void beamer_pm_new_session(const struct sock *meta_sk);

void beamer_pm_addr_signal(struct sock *sk, unsigned *size, struct tcp_out_options *opts, struct sk_buff *skb);

#endif /* BEAMER_PM_H */
 
