#ifndef BEAMER_GEN_H
#define BEAMER_GEN_H

#include <linux/hashtable.h>
#include <linux/types.h>

enum
{
	BEAMER_GEN_NEW = 1,
	BEAMER_GEN_CUR = 0,
	BEAMER_GEN_OLD = -1,
};

int beamer_gen_update(u64 gen);


#endif /* BEAMER_GEN_H */
