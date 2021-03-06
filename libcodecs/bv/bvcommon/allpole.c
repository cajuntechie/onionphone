/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:noexpandtab */

/*****************************************************************************/
/* BroadVoice(R)16 (BV16) Fixed-Point ANSI-C Source Code                     */
/* Revision Date: November 13, 2009                                          */
/* Version 1.1                                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Copyright 2000-2009 Broadcom Corporation                                  */
/*                                                                           */
/* This software is provided under the GNU Lesser General Public License,    */
/* version 2.1, as published by the Free Software Foundation ("LGPL").       */
/* This program is distributed in the hope that it will be useful, but       */
/* WITHOUT ANY SUPPORT OR WARRANTY; without even the implied warranty of     */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LGPL for     */
/* more details.  A copy of the LGPL is available at                         */
/* http://www.broadcom.com/licenses/LGPLv2.1.php,                            */
/* or by writing to the Free Software Foundation, Inc.,                      */
/* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.                 */
/*****************************************************************************/

/*****************************************************************************
  allpole.c : Common Fixed-Point Library: all-pole filter

  $Log$
******************************************************************************/

#include <stdint.h>
#include "bvcommon.h"
#include "basop32.h"
#include "utility.h"

#define BUFFERSIZE  (LPCO+160)

void apfilter(int16_t a[],	/* (i) Q12 : prediction coefficients  */
	      int16_t m,		/* (i)     : LPC order                */
	      int16_t x[],	/* (i) Q0  : input signal             */
	      int16_t y[],	/* (o) Q0  : output signal            */
	      int16_t lg,	/* (i)     : size of filtering        */
	      int16_t mem[],	/* (i/o) Q0: filter memory            */
	      int16_t update	/* (i)     : memory update flag       */
    )
{
	int16_t buf[BUFFERSIZE];	/* buffer for filter memory & signal */
	int32_t a0;
	int16_t *fp1;
	int16_t i, n;

	/* copy filter memory to beginning part of temporary buffer */
	W16copy(buf, mem, m);

	/* loop through every element of the current vector */
	for (n = 0; n < lg; n++) {

		/* perform bv_multiply-bv_adds along the delay line of filter */
		fp1 = &buf[n];
		a0 = L_bv_mult0(4096, x[n]);	// Q12
		for (i = m; i > 0; i--)
			a0 = bv_L_msu0(a0, a[i], *fp1++);	// Q12

		/* update temporary buffer for filter memory */
		*fp1 = intround(L_bv_shl(a0, 4));
	}

	/* copy to output array */
	W16copy(y, buf + m, lg);

	/* get the filter memory after filtering the current vector */
	if (update)
		W16copy(mem, buf + lg, m);

	return;
}
