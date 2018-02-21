#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/inet.h>
#include <net/mptcp_v4.h>
#include "beamer_mod.h"

struct beamer_pm_state
{
	int need_add;
};

static inline struct beamer_pm_state *beamer_pm_get_state(const struct mptcp_cb *mpcb)
{
	return (struct beamer_pm_state *)&mpcb->mptcp_pm[0];
}

int beamer_pm_get_local_id(sa_family_t family, union inet_addr *addr, struct net *net, bool *low_prio)
{
	(void)family; (void)addr; (void)net; (void)low_prio;
	
	return 0;
}

void beamer_pm_new_session(const struct sock *meta_sk)
{
	struct mptcp_cb *mpcb = tcp_sk(meta_sk)->mpcb;
	struct beamer_pm_state *state = beamer_pm_get_state(mpcb);
	
	/* no Beamer MPTCP support */
	if (id == 0)
		return;
	
	state->need_add = 1;
	
	mpcb->addr_signal = 1;
	tcp_send_ack(mpcb->master_sk);
}

void beamer_pm_addr_signal(struct sock *sk, unsigned *size, struct tcp_out_options *opts, struct sk_buff *skb)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct mptcp_cb *mpcb = tp->mpcb;
	struct beamer_pm_state *state = beamer_pm_get_state(mpcb);
	
	/* no Beamer MPTCP support */
	if (id == 0)
		return;

	if (MAX_TCP_OPTION_SPACE - *size >= MPTCP_SUB_LEN_ADD_ADDR4_P_ALIGN)
	{
		opts->options |= OPTION_MPTCP;
		opts->mptcp_options |= OPTION_ADD_ADDR;
		opts->add_addr4.addr_id = 1;
		opts->add_addr4.addr.s_addr = vip;
		opts->add_addr4.port = htons(id);
		opts->add_addr_v4 = 1;
		
		*size += MPTCP_SUB_LEN_ADD_ADDR4_P_ALIGN;
		
		if (skb)
			state->need_add = 0;
	}

	mpcb->addr_signal = state->need_add;	
}
