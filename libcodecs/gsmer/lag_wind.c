/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:noexpandtab */

/*************************************************************************
 *
 *  FUNCTION:  w_Lag_window()
 *
 *  PURPOSE:  Lag windowing of autocorrelations.
 *
 *  DESCRIPTION:
 *         r[i] = r[i]*lag_wind[i],   i=1,...,10
 *
 *     r[i] and lag_wind[i] are in special double precision format.
 *     See "oper_32b.c" for the format.
 *
 *************************************************************************/

#include <stdint.h>
#include "basic_op.h"
#include "oper_32b.h"


#include "lag_wind.tab"

void w_Lag_window(int16_t m,	/* (i)     : LPC order                        */
		  int16_t r_h[],	/* (i/o)   : w_Autocorrelations  (msb)          */
		  int16_t r_l[]	/* (i/o)   : w_Autocorrelations  (lsb)          */
    )
{
	int16_t i;
	int32_t x;

	for (i = 1; i <= m; i++) {
		x = w_Mpy_32(r_h[i], r_l[i], w_lag_h[i - 1], w_lag_l[i - 1]);
		w_L_Extract(x, &r_h[i], &r_l[i]);
	}
	return;
}
