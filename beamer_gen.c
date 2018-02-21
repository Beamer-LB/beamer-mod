#include "beamer_gen.h"

static volatile u64 gen_max;
static DEFINE_SPINLOCK(gen_max_lock);

int beamer_gen_update(u64 gen)
{
	if (likely(gen_max == gen))
		return BEAMER_GEN_CUR;
	
	if (gen < gen_max)
		return BEAMER_GEN_OLD;
	
	spin_lock(&gen_max_lock);
	
	if (gen > gen_max)
		gen_max = gen;
	
	spin_unlock(&gen_max_lock);
	
	return BEAMER_GEN_NEW;
}
