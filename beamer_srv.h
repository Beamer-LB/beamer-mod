#ifndef BEAMER_SRV_H
#define BEAMER_SRV_H

#include <linux/types.h>
#include <linux/bitmap.h>

#define BEAMER_N_PORTS 1024

extern DECLARE_BITMAP(beamer_srvs_bmp, BEAMER_N_PORTS);

static inline void beamer_srv_init(void)
{
	bitmap_zero(beamer_srvs_bmp, BEAMER_N_PORTS);
}

static inline void beamer_srv_put(__be16 port)
{
	uint16_t port_hb = ntohs(port);
	
	set_bit(port_hb, beamer_srvs_bmp);
}

static inline void beamer_srv_remove(__be16 port)
{
	uint16_t port_hb = ntohs(port);
	
	clear_bit(port_hb, beamer_srvs_bmp);
}

static inline int beamer_srv_contains(__be16 port)
{
	uint16_t port_hb = ntohs(port);
	
	if (port_hb >= BEAMER_N_PORTS)
		return 0;
	
	return test_bit(port_hb, beamer_srvs_bmp);
}

#endif /* BEAMER_SRV_H */
