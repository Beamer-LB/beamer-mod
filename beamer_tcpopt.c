#include "beamer_tcpopt.h"

struct tcp_option
{
	__u8 kind;
	__u8 len;
} __attribute__((packed));

struct tcp_timestamp
{
	struct tcp_option option;
	__be32 tsval;
	__be32 tsecr;
} __attribute__((packed));

static struct tcp_option *tcp_option_next(unsigned char **ptr, int *length)
{
	while (*length > 0)
	{
		struct tcp_option *option = (struct tcp_option *)(*ptr);

		switch (option->kind)
		{
		case TCPOPT_EOL:
			goto abort;
			
		case TCPOPT_NOP:	/* Ref: RFC 793 section 3.1 */
			(*ptr)++;
			(*length)--;
			continue;
			
		default:
			if (*length == 1)
				goto abort;
			if (option->len < 2)	/* "silly options" */
				goto abort;
			if (option->len > *length)
				goto abort;  /* don't parse partial options */
			
			*ptr += option->len;
			*length -= option->len;
			
			return option;
		}
	}
	
	return NULL;
	
abort:
	*length = 0;
	return NULL;
}

static inline struct tcp_option *tcp_option_first(struct tcphdr *th, int kind)
{
	unsigned char *ptr = (unsigned char *)(th + 1);
	int length = th->doff * 4 - sizeof(struct tcphdr);
	struct tcp_option *option;
	
	while ((option = tcp_option_next(&ptr, &length)) != NULL)
	{
		if (option->kind == kind)
			return option;
	}
	return NULL;
}

/* shamelessly adapted from tcp_parse_aligned_timestamp */
static inline __be32 *get_ecr_fast(struct tcphdr *th)
{
	__be32 *ptr = (__be32 *)(th + 1);

	if (*ptr == htonl((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) | (TCPOPT_TIMESTAMP << 8) | TCPOLEN_TIMESTAMP))
	{
		++ptr;
		++ptr;
		return ptr;
	}
	return NULL;
}

int beamer_is_mptcp(struct tcphdr *tcph)
{
	return (tcp_option_first(tcph, TCPOPT_MPTCP) != NULL);
}

__be32 *beamer_get_ecr(struct tcphdr *tcph)
{
	__be32 *ret = get_ecr_fast(tcph);
	struct tcp_timestamp *ts;
	
	if (ret)
		return ret;
	
	ts = (struct tcp_timestamp *)tcp_option_first(tcph, TCPOPT_TIMESTAMP);
	if (!ts || ts->option.len != TCPOLEN_TIMESTAMP)
		return NULL;
	
	return &(ts->tsecr);
}

struct mp_join *beamer_get_mpjoin(struct tcphdr *tcph)
{
	struct mptcp_option *mptcpopt = (struct mptcp_option *)tcp_option_first(tcph, TCPOPT_MPTCP);
	
	if (!mptcpopt || mptcpopt->sub != MPTCP_SUB_JOIN)
		return NULL;
	
	return (struct mp_join *)mptcpopt;
}
