#ifndef BEAMER_CONN_LOOKUP_H
#define BEAMER_CONN_LOOKUP_H

#include <net/sock.h>
#include <net/tcp.h>
#include <net/netns/hash.h>
#include "beamer_mod.h"
#include "beamer_checksum.h"

enum field_t
{
	F_SRC,
	F_DST,
};

static inline void beamer_xlat_ip(struct iphdr *iph, __be32 new_ip, enum field_t field)
{
	__be32 old_ip;

	/* most packets are outbound */
	if (field == F_SRC)
	{
		old_ip = iph->saddr;
		iph->saddr = new_ip;
		
		atomic_inc(&beamer_stats.pkts.out);
		atomic_add(ntohs(iph->tot_len) + 14, &beamer_stats.bytes.out);
	}
	else /* F_DST */
	{
		old_ip = iph->daddr;
		iph->daddr = new_ip;
		
		atomic_inc(&beamer_stats.pkts.in);
		atomic_add(ntohs(iph->tot_len) + 14, &beamer_stats.bytes.in);
	}
	iph->check = beamer_checksum_fold(
		beamer_checksum_fixup_32(old_ip, new_ip,
		iph->check));
}

static inline void beamer_xlat_tcp(struct iphdr *iph, struct tcphdr *tcph, __be32 new_ip, enum field_t field, int complement)
{
	__be16 old_ip_sum = iph->check;

	beamer_xlat_ip(iph, new_ip, field);

	/* HACK: figure out if we're working with a partially computed complement or the real thing */
	if (!complement)
	{
		/* do it normally */
		tcph->check = beamer_checksum_fold(
			beamer_checksum_meta_fixup(old_ip_sum, iph->check,
			tcph->check));
	}
	else
	{
		/* replace meta with complement_meta */
		tcph->check = beamer_checksum_fold(
			beamer_checksum_complement_meta_fixup(old_ip_sum, iph->check,
			tcph->check));
	}
}

static inline void beamer_xlat_udp(struct iphdr *iph, struct udphdr *udph, __be32 new_ip, enum field_t field)
{
	__be16 old_ip_sum = iph->check;

	beamer_xlat_ip(iph, new_ip, field);

	udph->check = beamer_checksum_fold(
		beamer_checksum_meta_fixup(old_ip_sum, iph->check,
		udph->check));
}

static inline void beamer_in_decap(struct sk_buff *skb)
{
	/* ip_hdr(skb) == skb->data, unless something went horribly wrong... */
	skb_pull(skb, ip_hdr(skb)->ihl * 4);

	skb_set_network_header(skb, 0);
	skb_set_transport_header(skb, ip_hdr(skb)->ihl * 4);
}

static inline void beamer_in_reroute(struct iphdr *iph, uint32_t dst)
{
	iph->check = beamer_checksum_fold(
		beamer_checksum_fixup_32(iph->daddr, dst,
		iph->check));
	iph->daddr = dst;
	
	atomic_inc(&beamer_stats.pkts.chained);
	atomic_add(ntohs(iph->tot_len) + 14, &beamer_stats.bytes.chained);
}

int beamer_got_connection(struct net *netns, __be32 dip, struct iphdr *iph, struct tcphdr *tcph);

#endif /* BEAMER_CONN_LOOKUP_H */
