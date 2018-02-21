#ifndef BEAMER_TCPOPT_H
#define BEAMER_TCPOPT_H

#include <net/mptcp.h>

int beamer_is_mptcp(struct tcphdr *tcph);

__be32 *beamer_get_ecr(struct tcphdr *tcph);

struct mp_join *beamer_get_mpjoin(struct tcphdr *tcph);

#endif /* BEAMER_TCPOPT_H */
