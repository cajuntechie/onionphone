/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:noexpandtab */

/*

$Log$
Revision 1.15  2004/06/26 03:50:14  markster
Merge source cleanups (bug #1911)

Revision 1.14  2003/02/12 13:59:15  matteo
mer feb 12 14:56:57 CET 2003

Revision 1.1.1.1  2003/02/12 13:59:15  matteo
mer feb 12 14:56:57 CET 2003

Revision 1.2  2000/01/05 08:20:39  markster
Some OSS fixes and a few lpc changes to make it actually work

 * Revision 1.2  1996/08/20  20:41:32  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:49  jaf
 * Initial revision
 *

*/

#include "lpc10.h"
#include "random.h"

/* ********************************************************************** */

/* 	RANDOM Version 49 */

/* $Log$
 * Revision 1.15  2004/06/26 03:50:14  markster
 * Merge source cleanups (bug #1911)
 *
 * Revision 1.14  2003/02/12 13:59:15  matteo
 * mer feb 12 14:56:57 CET 2003
 *
 * Revision 1.1.1.1  2003/02/12 13:59:15  matteo
 * mer feb 12 14:56:57 CET 2003
 *
 * Revision 1.2  2000/01/05 08:20:39  markster
 * Some OSS fixes and a few lpc changes to make it actually work
 *
 * Revision 1.2  1996/08/20  20:41:32  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:49  jaf
 * Initial revision
 * */
/* Revision 1.3  1996/03/20  16:13:54  jaf */
/* Rearranged comments a little bit, and added comments explaining that */
/* even though there is local state here, there is no need to create an */
/* ENTRY for reinitializing it. */

/* Revision 1.2  1996/03/14  22:25:29  jaf */
/* Just rearranged the comments and local variable declarations a bit. */

/* Revision 1.1  1996/02/07 14:49:01  jaf */
/* Initial revision */

/* ********************************************************************* */

/*  Pseudo random number generator based on Knuth, Vol 2, p. 27. */

/* Function Return: */
/*  RANDOM - Integer variable, uniformly distributed over -32768 to 32767 */

/* This subroutine maintains local state from one call to the next. */
/* In the context of the LPC10 coder, there is no reason to reinitialize */
/* this local state when switching between audio streams, because its */
/* results are only used to generate noise for unvoiced frames. */

int32_t lpc10_random(struct lpc10_decoder_state * st)
{
	/* Initialized data */

	int32_t *j;
	int32_t *k;
	int16_t *y;

	/* System generated locals */
	int32_t ret_val;

/* 	Parameters/constants */
/*       Local state */
/*   The following is a 16 bit 2's complement addition, */
/*   with overflow checking disabled */

	j = &(st->j);
	k = &(st->k);
	y = &(st->y[0]);

	y[*k - 1] += y[*j - 1];
	ret_val = y[*k - 1];
	--(*k);
	if (*k <= 0) {
		*k = 5;
	}
	--(*j);
	if (*j <= 0) {
		*j = 5;
	}
	return ret_val;
}				/* lpc10_random */
