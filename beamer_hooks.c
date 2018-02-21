#include <net/sock.h>
#include <net/tcp.h>
#include <net/netns/hash.h>
#include <linux/time.h>
#include "beamer_mod.h"
#include "beamer_hooks.h"
#include "beamer_checksum.h"
#include "beamer_srv.h"
#include "beamer_tcpopt.h"
#include "beamer_hook_utils.h"
#include "beamer_bucket_table.h"
#include "beamer_gen.h"
#include "beamer_freebsd_bob.h"
#include "beamer_p4_crc32.h"

static inline uint32_t beamer_bucket(struct iphdr *iph, struct tcphdr *tcph)
{
#if BEAMER_HASHFN == BEAMER_HASHFN_BOB
	return beamer_freebsd_bob(iph->saddr, tcph->source, tcph->dest) % ring_size;
#elif BEAMER_HASHFN == BEAMER_HASHFN_CRC
	struct beamer_hash_touple touple = { iph->saddr, tcph->source };
	return beamer_p4_crc32_6((char *)&touple) % ring_size;
#else
#error Invalid BEAMER_HASHFN
#endif
}

static inline __be32 beamer_former_owner(struct iphdr *iph, struct tcphdr *tcph)
{
	volatile struct beamer_bucket_info *info = beamer_bucket_table_get(beamer_bucket(iph, tcph));
	struct timeval now;
	
	if (daisy_timeout < 0)
		return 0;
	
	/* old entry? */
	do_gettimeofday(&now);
	if (now.tv_sec > info->ts + daisy_timeout)
		return 0;
	
	return info->ds_dip;
}

static inline int beamer_skb_has_room(struct sk_buff *skb, void *from, size_t size)
{
	return (skb->len - ((uint64_t)from - (uint64_t)skb->data) >= size);
}

struct bucket_opt
{
#if defined(__BIG_ENDIAN_BITFIELD)
	u8 copied : 1,
		oclass: 2,
		num : 5;
#elif defined (__LITTLE_ENDIAN_BITFIELD)
	u8 num : 5,
		oclass: 2,
		copied : 1;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
	u8 len;
	uint16_t padding;
	uint32_t pdip;
	uint32_t ts;
	uint32_t gen;
} __attribute__((packed));


static inline void beamer_update_bucket(uint32_t bucket, struct bucket_opt *opt)
{
	volatile struct beamer_bucket_info *info = beamer_bucket_table_get(bucket);
	
	if (likely(opt->ts <= info->ts))
		return;
	
	/* option with pdip == 0 (P4?) */
	if (unlikely(opt->pdip == 0))
		return;
	
	/* MUX implementation might not check this before encapping */
	if (unlikely(opt->pdip == dip))
		return;
	
	spin_lock(&beamer_bucket_table_lock);
	if (likely(opt->ts > info->ts))
	{
		info->ds_dip = opt->pdip; //must write this one first
		info->ts = opt->ts;
	}
	spin_unlock(&beamer_bucket_table_lock);
}

static inline void beamer_suppress_option(struct iphdr *iph, struct bucket_opt *opt)
{
	iph->check = beamer_checksum_fold(
		beamer_checksum_fixup_32(opt->ts, 0,
		iph->check));
	opt->ts = 0;
}

unsigned int beamer_in_hookfn(const struct nf_hook_ops *ops,
	struct sk_buff *skb,
	const struct net_device *in,
	const struct net_device *out,
	int (*okfn)(struct sk_buff *))
{
	struct iphdr *outer_iph = ip_hdr(skb);
	struct iphdr *inner_iph;
	struct bucket_opt *opt = NULL;
	int gen_type = BEAMER_GEN_CUR;
	
	if ((unsigned char *)outer_iph != skb->data)
		goto done;
	if (!beamer_skb_has_room(skb, outer_iph, sizeof(struct iphdr)))
		goto done;
	if (outer_iph->version != 4)
		goto done;
	if (outer_iph->ihl < 5)
		goto done;

	/* is it IPIP, sent by the mux and destined to us? */
	if (outer_iph->protocol != IPPROTO_IPIP)
		goto done;
	if (outer_iph->saddr != vip)
		goto done;
	if (outer_iph->daddr != dip)
		goto done;
	
	inner_iph = (struct iphdr *)((char *)outer_iph + outer_iph->ihl * 4);
	if (!beamer_skb_has_room(skb, inner_iph, sizeof(struct iphdr)))
		goto done;
	if (inner_iph->version != 4)
		goto done;
	if (inner_iph->ihl < 5)
		goto done;
	
	/* option */
	if (outer_iph->ihl * 4 - sizeof(struct iphdr) == sizeof(struct bucket_opt))
	{
		opt = (struct bucket_opt *)(outer_iph + 1);
		
		if (opt->copied != 0 || opt->oclass != 3 || opt->num != 1 || opt->len != sizeof(struct bucket_opt))
			opt = NULL;
	}
	
	if (inner_iph->protocol == IPPROTO_TCP)
	{
		struct tcphdr *tcph = (struct tcphdr *)((char *)inner_iph + inner_iph->ihl * 4);
		__be32 dst;
		
		if (!beamer_skb_has_room(skb, tcph, sizeof(struct tcphdr)))
			goto done;
		
		if (tcph->dest == htons(id))
			goto tcp_local;

		if (!beamer_srv_contains(tcph->dest))
			goto done;
		
		/* learn? */
		if (opt)
		{
			beamer_update_bucket(beamer_bucket(inner_iph, tcph), opt);
			gen_type = beamer_gen_update(ntohl(opt->gen));
		}
		
		/* SYN */
		if (tcph->syn && !tcph->ack)
			goto tcp_local;
		
		if (beamer_got_connection(dev_net(in), dip, inner_iph, tcph))
			goto tcp_local;
		
		/* must we daisy chain it to downstream peer? */
		dst = beamer_former_owner(inner_iph, tcph);
		if (dst)
		{
			if (opt)
				beamer_suppress_option(outer_iph, opt);
			beamer_in_reroute(outer_iph, dst);
			goto done;
		}

		/* RST */
		if (opt && gen_type == BEAMER_GEN_OLD)
			return NF_DROP;
		
tcp_local:
		beamer_xlat_tcp(inner_iph, tcph, dip, F_DST, 0);
	}
	else if (inner_iph->protocol == IPPROTO_UDP)
	{
		struct udphdr *udph = (struct udphdr *)((char *)inner_iph + inner_iph->ihl * 4);
		if (!beamer_skb_has_room(skb, udph, sizeof(struct udphdr)))
			goto done;

		if (!beamer_srv_contains(udph->dest))
			goto done;

		beamer_xlat_udp(inner_iph, udph, dip, F_DST);
	}
	else
	{
		goto done;
	}

	beamer_in_decap(skb);

done:
	return NF_ACCEPT;
}

unsigned int beamer_out_hookfn(const struct nf_hook_ops *ops,
	struct sk_buff *skb,
	const struct net_device *in,
	const struct net_device *out,
	int (*okfn)(struct sk_buff *))
{
	struct iphdr *iph = ip_hdr(skb);
	
	if ((unsigned char *)iph != skb->data)
		goto done;
	if (!beamer_skb_has_room(skb, iph, sizeof(struct iphdr)))
		goto done;
	if (iph->version != 4)
		goto done;

	if (iph->protocol == IPPROTO_TCP)
	{
		struct tcphdr *tcph = tcp_hdr(skb);

		if (!beamer_skb_has_room(skb, tcph, sizeof(struct tcphdr)))
			goto done;

		if (!beamer_srv_contains(tcph->source) && tcph->source != htons(id))
			goto done;

		beamer_xlat_tcp(iph, tcph, vip, F_SRC, skb->ip_summed == CHECKSUM_PARTIAL);
	}
	else if (iph->protocol == IPPROTO_UDP)
	{
		struct udphdr *udph = udp_hdr(skb);
		
		if (!beamer_skb_has_room(skb, udph, sizeof(struct udphdr)))
			goto done;

		if (!beamer_srv_contains(udph->source))
			goto done;

		beamer_xlat_udp(iph, udph, vip, F_SRC);
	}

done:
	return NF_ACCEPT;
}

