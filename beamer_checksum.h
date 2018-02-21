#ifndef BEAMER_CHECKSUM_H
#define BEAMER_CHECKSUM_H

#include <linux/types.h>

static inline __be32 beamer_checksum_fixup_16(__be16 old_field, __be16 new_field, __be32 sum)
{
	return sum + old_field - new_field;
}

static inline __be32 beamer_checksum_fixup_32(__be32 old_field, __be32 new_field, __be32 sum)
{
	return beamer_checksum_fixup_16(old_field >> 16, new_field >> 16,
		beamer_checksum_fixup_16(old_field & 0xffff, new_field & 0xffff,
		sum));
}

static inline __be32 beamer_checksum_meta_fixup(__be16 old_field, __be16 new_field, __be32 sum)
{
	return beamer_checksum_fixup_16(new_field, old_field, sum);
}

static inline __be32 beamer_checksum_complement_meta_fixup(__be16 old_field, __be16 new_field, __be32 sum)
{
	return beamer_checksum_fixup_16(old_field, new_field, sum);
}

static inline __be16 beamer_checksum_fold(__be32 sum)
{
	__be16 ret = (sum >> 16) + (sum & 0xffff);
	ret &= 0xffff;
	return ret;
}

#endif /* BEAMER_CHECKSUM_H */
