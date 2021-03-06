/*	$OpenBSD: atomic.h,v 1.12 2014/07/18 10:40:14 dlg Exp $	*/

/* Public Domain */

#ifndef _M88K_ATOMIC_H_
#define _M88K_ATOMIC_H_

#if defined(_KERNEL)

#ifdef MULTIPROCESSOR

/* actual implementation is hairy, see atomic.S */
void		atomic_setbits_int(volatile unsigned int *, unsigned int);
void		atomic_clearbits_int(volatile unsigned int *, unsigned int);
unsigned int	atomic_add_int_nv_mp(volatile unsigned int *, unsigned int);
unsigned int	atomic_sub_int_nv_mp(volatile unsigned int *, unsigned int);
unsigned int	atomic_cas_uint_mp(unsigned int *, unsigned int, unsigned int);
unsigned int	atomic_swap_uint_mp(unsigned int *, unsigned int);

#define	atomic_add_int_nv	atomic_add_int_nv_mp
#define	atomic_sub_int_nv	atomic_sub_int_nv_mp
#define	atomic_cas_uint		atomic_cas_uint_mp
#define	atomic_swap_uint	atomic_swap_uint_mp

#else

#include <machine/asm_macro.h>
#include <machine/psl.h>

static __inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	u_int psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	*uip |= v;
	set_psr(psr);
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	u_int psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	*uip &= ~v;
	set_psr(psr);
}

static __inline unsigned int
atomic_add_int_nv_sp(volatile unsigned int *uip, unsigned int v)
{
	u_int psr;
	unsigned int nv;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	*uip += v;
	nv = *uip;
	set_psr(psr);

	return nv;
}

static __inline unsigned int
atomic_sub_int_nv_sp(volatile unsigned int *uip, unsigned int v)
{
	u_int psr;
	unsigned int nv;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	*uip -= v;
	nv = *uip;
	set_psr(psr);

	return nv;
}

static inline unsigned int
atomic_cas_uint_sp(unsigned int *p, unsigned int o, unsigned int n)
{
	u_int psr;
	unsigned int ov;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	ov = *p;
	if (ov == o)
		*p = n;
	set_psr(psr);

	return ov;
}

static inline unsigned int
atomic_swap_uint_sp(unsigned int *p, unsigned int v)
{
	u_int psr;
	unsigned int ov;

	psr = get_psr();
	set_psr(psr | PSR_IND);
	ov = *p;
	*p = v;
	set_psr(psr);

	return ov;
}

#define	atomic_add_int_nv	atomic_add_int_nv_sp
#define	atomic_sub_int_nv	atomic_sub_int_nv_sp
#define	atomic_cas_uint		atomic_cas_uint_sp
#define	atomic_swap_uint	atomic_swap_uint_sp

#endif	/* MULTIPROCESSOR */

static __inline__ unsigned int
atomic_clear_int(volatile unsigned int *uip)
{
	u_int oldval;

	oldval = 0;
	__asm__ volatile
	    ("xmem %0, %2, %%r0" : "+r"(oldval), "+m"(*uip) : "r"(uip));
	return oldval;
}

#define	atomic_add_long_nv(p,v) \
	((unsigned long)atomic_add_int_nv((unsigned int *)p, (unsigned int)v))
#define	atomic_sub_long_nv(p,v) \
	((unsigned long)atomic_sub_int_nv((unsigned int *)p, (unsigned int)v))

#define	atomic_cas_ulong(p,o,n) \
	((unsigned long)atomic_cas_uint((unsigned int *)p, (unsigned int)o, \
	 (unsigned int)n))
#define	atomic_cas_ptr(p,o,n) \
	((void *)atomic_cas_uint((void *)p, (void *)o, (void *)n))

#define	atomic_swap_ulong(p,o) \
	((unsigned long)atomic_swap_uint((unsigned int *)p, (unsigned int)o)
#define	atomic_swap_ptr(p,o) \
	((void *)atomic_swap_uint((void *)p, (void *)o))

static inline void
__sync_synchronize(void)
{
	/* flush_pipeline(); */
	__asm__ volatile ("tb1 0, %%r0, 0" ::: "memory");
}

#endif /* defined(_KERNEL) */
#endif /* _M88K_ATOMIC_H_ */
