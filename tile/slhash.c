/* slhash.c -- slide the hash table during fill_window()
 * Copyright (C) 1995-2010 Jean-loup Gailly and Mark Adler
 * Copyright (C) 2011 Jan Seiffert
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* NOTE:
 * We do not precheck the length or wsize for small values because
 * we assume a minimum len of 256 (for MEM_LEVEL 1) and a minimum wsize
 * of 256 for windowBits 8
 */

/* we use a bunch of inline asm, so GCC it is */
#ifdef __GNUC__
#  define HAVE_SLHASH_VEC
#  define SOUL (sizeof(unsigned long))

/* ========================================================================= */
local inline unsigned long v2sub(unsigned long a, unsigned long b)
{
    unsigned long r;
#  ifdef __tilegx__
    __asm__ ("v2sub	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
#  else
    __asm__ ("subh	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
#  endif
    return r;
}

/* ========================================================================= */
local inline unsigned long v2cmpltu(unsigned long a, unsigned long b)
{
    unsigned long r;
#  ifdef __tilegx__
    __asm__ ("v2cmpltu	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
#  else
    __asm__ ("slth_u	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
#  endif
    return r;
}

/* ========================================================================= */
local inline unsigned long v2mz(unsigned long a, unsigned long b)
{
    unsigned long r;
#  ifdef __tilegx__
    __asm__ ("v2mz	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
#  else
    __asm__ ("mzh	%0, %1, %2" : "=r" (r) : "r" (a), "r" (b));
#  endif
    return r;
}

/* ========================================================================= */
local void update_hoffset_m(Posf *p, uInt wsize, unsigned n)
{
    unsigned long vwsize;
    unsigned int i, f;

    vwsize = (unsigned short)wsize;
    vwsize = (vwsize << 16) | (vwsize & 0x0000ffff);
    if (SOUL > 4)
        vwsize = ((vwsize << 16) << 16) | (vwsize & 0xffffffff);

    /* align */
    f = (unsigned)ALIGN_DIFF(p, SOUL);
    if (unlikely(f)) {
        f /= sizeof(*p);
        n -= f;
        if (f & 1) {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
            f--;
        }
        if (SOUL > 4 && f >= 2) {
            unsigned int m = *(unsigned int *)p;
            m = v2mz(v2cmpltu(m, vwsize), v2sub(m, vwsize));
            *(unsigned int *)p = m;
            p += 2;
        }
    }

    /* do it */
    i  = n / (SOUL/sizeof(*p));
    n %= SOUL/sizeof(*p);
    if (i & 1) {
            unsigned long m = *(unsigned long *)p;
            m = v2mz(v2cmpltu(m, vwsize), v2sub(m, vwsize));
            *(unsigned long *)p = m;
            p += SOUL/sizeof(*p);
            i--;
    }
    i /= 2;
    do {
            unsigned long m1 = ((unsigned long *)p)[0];
            unsigned long m2 = ((unsigned long *)p)[1];
            unsigned long mask1, mask2;
            asm (
#  ifdef __tilegx__
                "{v2sub %2, %0, %4; v2cmpltu %0, %0, %4}\n\t"
                "{v2sub %3, %1, %4; v2cmpltu %1, %1, %4}\n\t"
                "{v2mz %0, %0, %2;  v2mz %1, %1, %3}\n\t"
#  else
                "{subh %2, %0, %4; slth_u %0, %0, %4}\n\t"
                "{subh %3, %1, %4; slth_u %1, %1, %4}\n\t"
                "{mzh %0, %0, %2;  mzh %1, %1, %3}\n\t"
#  endif
                : /* %0 */ "=r" (m1),
                  /* %1 */ "=r" (m2),
                  /* %2 */ "=&r" (mask1),
                  /* %3 */ "=&r" (mask2)
                : /* %4 */ "r" (vwsize),
                  "0" (m1),
                  "1" (m2)
            );
            ((unsigned long *)p)[0] = m1;
            ((unsigned long *)p)[1] = m2;
            p += 2*(SOUL/sizeof(*p));
    } while (--i);

    /* handle trailer */
    if (unlikely(n)) {
        if (SOUL > 4 && n >= 2) {
            unsigned int m = *(unsigned int *)p;
            m = v2mz(v2cmpltu(m, vwsize), v2sub(m, vwsize));
            *(unsigned int *)p = m;
            p += 2;
            n -= 2;
        }
        if (n & 1) {
            register unsigned m = *p;
            *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
        }
    }
}

/* ========================================================================= */
local void update_hoffset_l(Posf *p, uInt wsize, unsigned n)
{
    do {
        register unsigned m = *p;
        *p++ = (Pos)(m >= wsize ? m-wsize : NIL);
    } while (--n);
}

/* ========================================================================= */
local inline void update_hoffset(Posf *p, uInt wsize, unsigned n)
{
    /*
     * Unfortunatly most chip designer prefer signed saturation...
     * So we are better off with a parralel compare and move/mask
     */
    if (likely(2 == sizeof(*p) && wsize < (1<<16)))
        update_hoffset_m(p, wsize, n);
    else
        update_hoffset_l(p, wsize, n);
}
#endif
