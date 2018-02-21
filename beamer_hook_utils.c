#include "beamer_hook_utils.h"

#define BEAMER_INET_MATCH(__sk, __saddr, __daddr, __ports) \
	(((__sk)->sk_portpair == (__ports))             &&              \
	((__sk)->sk_daddr      == (__saddr))           &&              \
	((__sk)->sk_rcv_saddr  == (__daddr)))

struct sock *beamer_tcp_lookup_established(struct net *net,
	const __be32 saddr, const __be16 sport,
	const __be32 daddr, const u16 hnum)
{
	const __portpair ports = INET_COMBINED_PORTS(sport, hnum);
	struct sock *sk;
	const struct hlist_nulls_node *node;
	/* Optimize here for direct hit, only listening connections can
	* have wildcards anyways.
	*/
	unsigned int hash = inet_ehashfn(net, daddr, hnum, saddr, sport);
	unsigned int slot = hash & tcp_hashinfo.ehash_mask;
	struct inet_ehash_bucket *head = &tcp_hashinfo.ehash[slot];

	rcu_read_lock();
begin:
	sk_nulls_for_each_rcu(sk, node, &head->chain)
	{
		if (sk->sk_hash != hash)
			continue;
		if (likely(BEAMER_INET_MATCH(sk, saddr, daddr, ports)))
		{
			if (unlikely(!atomic_inc_not_zero(&sk->sk_refcnt)))
				goto out;
			if (unlikely(!BEAMER_INET_MATCH(sk, saddr, daddr, ports)))
			{
				sock_gen_put(sk);
				goto begin;
			}
			goto found;
		}
	}
	/*
	* if the nulls value we got at the end of this lookup is
	* not the expected one, we must restart lookup.
	* We probably met an item that was moved to another chain.
	*/
	if (get_nulls_value(node) != slot)
		goto begin;
out:
	sk = NULL;
found:
	rcu_read_unlock();
	return sk;
}

static inline int beamer_compute_score(struct sock *sk, struct net *net,
	const unsigned short hnum, const __be32 daddr)
{
	int score = -1;
	struct inet_sock *inet = inet_sk(sk);

	if (net_eq(sock_net(sk), net) && inet->inet_num == hnum && !ipv6_only_sock(sk))
	{
		__be32 rcv_saddr = inet->inet_rcv_saddr;
		score = sk->sk_family == PF_INET ? 2 : 1;
		if (rcv_saddr)
		{
			if (rcv_saddr != daddr)
				return -1;
			score += 4;
		}
		if (sk->sk_bound_dev_if)
		{
			//if (sk->sk_bound_dev_if != dif)
			//	return -1;
			score += 4;
		}
	}
	return score;
}

struct sock *beamer_tcp_lookup_listener(struct net *net,
	const __be32 saddr, __be16 sport,
	const __be32 daddr, const unsigned short hnum)
{
	struct sock *sk, *result;
	struct hlist_nulls_node *node;
	unsigned int hash = inet_lhashfn(net, hnum);
	struct inet_listen_hashbucket *ilb = &tcp_hashinfo.listening_hash[hash];
	int score, hiscore, matches = 0, reuseport = 0;
	u32 phash = 0;

	rcu_read_lock();
begin:
	result = NULL;
	hiscore = 0;
	sk_nulls_for_each_rcu(sk, node, &ilb->head)
	{
		score = beamer_compute_score(sk, net, hnum, daddr);
		if (score > hiscore) {
			result = sk;
			hiscore = score;
			reuseport = sk->sk_reuseport;
			if (reuseport) {
				phash = inet_ehashfn(net, daddr, hnum,
						saddr, sport);
				matches = 1;
			}
		} else if (score == hiscore && reuseport) {
			matches++;
			if (((u64)phash * matches) >> 32 == 0)
				result = sk;
			phash = next_pseudo_random32(phash);
		}
	}
	/*
	* if the nulls value we got at the end of this lookup is
	* not the expected one, we must restart lookup.
	* We probably met an item that was moved to another chain.
	*/
	if (get_nulls_value(node) != hash + LISTENING_NULLS_BASE)
		goto begin;
	if (result)
	{
		if (unlikely(!atomic_inc_not_zero(&result->sk_refcnt)))
			result = NULL;
		else if (unlikely(beamer_compute_score(result, net, hnum, daddr) < hiscore))
		{
			sock_put(result);
			goto begin;
		}
	}
	rcu_read_unlock();
	return result;
}

int beamer_got_connection(struct net *netns, __be32 dip, struct iphdr *iph, struct tcphdr *tcph)
{
	struct sock *sk;
	struct request_sock *req;
	struct request_sock **prev;
	int ret = 0;

	local_bh_disable();
	
	sk = beamer_tcp_lookup_established(netns, iph->saddr, tcph->source, dip, ntohs(tcph->dest));
	if (sk)
	{
		ret = 1;
		sock_put(sk);
		goto done;
	}
	
	sk = beamer_tcp_lookup_listener(netns, iph->saddr, tcph->source, dip, ntohs(tcph->dest));
	if (!sk)
		goto done;
	if (sk->sk_state != TCP_LISTEN)
	{
		sock_put(sk);
		goto cookies;
	}
	
	req = inet_csk_search_req(sk, &prev, tcph->source, iph->saddr, dip);
	if (req)
	{
		ret = 1;
		//reqsk_put(req); //on newer kernel versions
		goto done;
	}
	
cookies:
	
//TODO: revisit later; MPTCP SYN cookies might cause headaches
#ifdef CONFIG_SYN_COOKIES
	if (!tcph->syn && sysctl_tcp_syncookies && tcph->ack && !tcph->rst)
	{
		__be32 old_dst = iph->daddr;
		
		/* speed hack: rewrite destination IP, then set it back */
		iph->daddr = dip;
		ret = __cookie_v4_check(iph, tcph, ntohl(tcph->ack_seq) - 1) != 0;
		iph->daddr = old_dst;
	}
#endif

done:
	local_bh_enable();
	
	return ret;
}
