#ifndef BEAMER_P4_CRC32_H
#define BEAMER_P4_CRC32_H

#include "beamer_mod.h"

struct beamer_hash_touple
{
	uint32_t src_ip;
	uint16_t src_port;
} __attribute__((packed));

uint32_t beamer_p4_crc32(const char *buf, size_t len);

uint32_t beamer_p4_crc32_6(const char *buf);

#endif /* BEAMER_P4_CRC32_H */
