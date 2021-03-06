/* vim: set tabstop=4:softtabstop=4:shiftwidth=4:noexpandtab */

/* ITU-T G.729 Software Package Release 2 (November 2006) */
/*
   ITU-T G.729 Annex C+ - Reference C code for floating point
                         implementation of G.729 Annex C+
                         (integration of Annexes B, D and E)
                          Version 2.1 of October 1999
*/

/*
 File : PSTCP.C
*/

/************************************************************************/
/*      Post - filtering : short term + long term                       */
/************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/**** Prototypes                                                        */
#include "ld8k.h"
#include "ld8cp.h"
#include "tab_ld8k.h"

/* Prototypes for local functions */
/**********************************/

static void pst_ltpe(int t0, float * ptr_sig_in,
		     float * ptr_sig_pst0, int *vo, float gamma_harm);
static void search_del(int t0, float * ptr_sig_in, int *ltpdel, int *phase,
		       float * num_gltp, float * den_gltp, float * y_up,
		       int *off_yup);
static void filt_plt(float * s_in, float * s_ltp, float * s_out,
		     float gain_plt);
static void compute_ltp_l(float * s_in, int ltpdel, int phase, float * y_up,
			  float * num, float * den);
static int select_ltp(float num1, float den1, float num2, float den2);
static void calc_st_filte(float * apond2, float * apond1, float * parcor0,
			  float * signal_ltp0_ptr, int long_h_st, int m_pst);
static void filt_mu(float * sig_in, float * sig_out, float parcor0);
static void calc_rc0_he(float * h, float * rc0, int long_h_st);
static void scale_st(float * sig_in, float * sig_out, float * gain_prec);

/* Arrays */
static float apond2[LONG_H_ST_E];	/* s.t. numerator coeff.        */
static float mem_stp[M_BWD];	/* s.t. postfilter memory       */
static float mem_zero[M_BWD];	/* null memory to compute h_st  */
static float res2[SIZ_RES2];	/* A(gamma2) residual           */

/* pointers */
float *res2_ptr;
float *ptr_mem_stp;

/* Variables */
float gain_prec;

/************************************************************************/
/****   Short term postfilter :                                     *****/
/*      Hst(z) = Hst0(z) Hst1(z)                                        */
/*      Hst0(z) = 1/g0 A(gamma2)(z) / A(gamma1)(z)                      */
/*      if {hi} = i.r. filter A(gamma2)/A(gamma1) (truncated)           */
/*      g0 = SUM(|hi|) if > 1                                           */
/*      g0 = 1. else                                                    */
/*      Hst1(z) = 1/(1 - |mu|) (1 + mu z-1)                             */
/*      with mu = k1 * gamma3                                           */
/*      k1 = 1st parcor calculated on {hi}                              */
/*      gamma3 = gamma3_minus if k1<0, gamma3_plus if k1>0              */
/****   Long term postfilter :                                      *****/
/*      harmonic postfilter :   H0(z) = gl * (1 + b * z-p)              */
/*      b = gamma_g * gain_ltp                                          */
/*      gl = 1 / 1 + b                                                  */
/*      computation of delay p on A(gamma2)(z) s(z)                     */
/*      sub optimal search                                              */
/*      1. search around 1st subframe delay (3 integer values)          */
/*      2. search around best integer with fract. delays (1/8)          */
/************************************************************************/
/*----------------------------------------------------------------------------
 * Init_Post_Filter -  Initialize postfilter functions
 *----------------------------------------------------------------------------
 */
void init_post_filter(void)
{
	int i;

	/* Initialize arrays and pointers */

	/* res2 =  A(gamma2) residual */
	for (i = 0; i < MEM_RES2; i++)
		res2[i] = (float) 0.;
	res2_ptr = res2 + MEM_RES2;

	/* 1/A(gamma1) memory */
	for (i = 0; i < M_BWD; i++)
		mem_stp[i] = (float) 0.;
	ptr_mem_stp = mem_stp + M_BWD - 1;

	/* fill apond2[M+1->long_h_st-1] with zeroes */
	for (i = M_BWDP1; i < LONG_H_ST_E; i++)
		apond2[i] = (float) 0.;

	/* null memory to compute i.r. of A(gamma2)/A(gamma1) */
	for (i = 0; i < M_BWD; i++)
		mem_zero[i] = (float) 0.;

	/* for gain adjustment */
	gain_prec = (float) 1.;

	return;
}

/*----------------------------------------------------------------------------
 * Post - adaptive postfilter main function
 *----------------------------------------------------------------------------
 */
void poste(int t0,		/* input : pitch delay given by coder */
	   float * signal_ptr,	/* input : input signal (pointer to current subframe */
	   float * coeff,	/* input : LPC coefficients for current subframe */
	   float * sig_out,	/* output: postfiltered output */
	   int *vo,		/* output: voicing decision 0 = uv,  > 0 delay */
	   float gamma1,	/* input: short term postfilt. den. weighting factor */
	   float gamma2,	/* input: short term postfilt. num. weighting factor */
	   float gamma_harm,	/* input: long term postfilter weighting factor */
	   int long_h_st,	/* input: impulse response length */
	   int m_pst,		/* input:  LPC order */
	   int Vad		/* input : VAD information (frame type)  */
    ) {

	/* Local variables and arrays */
	float apond1[M_BWDP1];	/* s.t. denominator coeff.      */
	float sig_ltp[L_SUBFRP1];	/* H0 output signal             */
	float *sig_ltp_ptr;
	float parcor0;

	/* Compute weighted LPC coefficients */
	weight_az(coeff, gamma1, m_pst, apond1);
	weight_az(coeff, gamma2, m_pst, apond2);
	set_zero(&apond2[m_pst + 1], (M_BWD - m_pst));

	/* Compute A(gamma2) residual */
	residue(m_pst, apond2, signal_ptr, res2_ptr, L_SUBFR);

	/* Harmonic filtering */
	sig_ltp_ptr = sig_ltp + 1;

	if (Vad > 1)
		pst_ltpe(t0, res2_ptr, sig_ltp_ptr, vo, gamma_harm);
	else {
		*vo = 0;
		copy(res2_ptr, sig_ltp_ptr, L_SUBFR);
	}

	/* Save last output of 1/A(gamma1)  */
	/* (from preceding subframe)        */
	sig_ltp[0] = *ptr_mem_stp;

	/* Controls short term pst filter gain and compute parcor0   */
	calc_st_filte(apond2, apond1, &parcor0, sig_ltp_ptr, long_h_st, m_pst);

	/* 1/A(gamma1) filtering, mem_stp is updated */
	syn_filte(m_pst, apond1, sig_ltp_ptr, sig_ltp_ptr, L_SUBFR,
		  &mem_stp[M_BWD - m_pst], 0);
	copy(&sig_ltp_ptr[L_SUBFR - M_BWD], mem_stp, M_BWD);

	/* Tilt filtering */
	filt_mu(sig_ltp, sig_out, parcor0);

	/* Gain control */
	scale_st(signal_ptr, sig_out, &gain_prec);

    /**** Update for next subframe */
	copy(&res2[L_SUBFR], &res2[0], MEM_RES2);

	return;
}

/*----------------------------------------------------------------------------
 *  pst_ltp - harmonic postfilter
 *----------------------------------------------------------------------------
 */
static void pst_ltpe(int t0,	/* input : pitch delay given by coder */
		     float * ptr_sig_in,	/* input : postfilter input filter (residu2) */
		     float * ptr_sig_pst0,	/* output: harmonic postfilter output */
		     int *vo,	/* output: voicing decision 0 = uv,  > 0 delay */
		     float gamma_harm	/* input: harmonic postfilter coefficient */
    )
{

/**** Declare variables                                 */
	int ltpdel, phase;
	float num_gltp, den_gltp;
	float num2_gltp, den2_gltp;
	float gain_plt;
	float y_up[SIZ_Y_UP];
	float *ptr_y_up;
	int off_yup;

	/* Sub optimal delay search */
	search_del(t0, ptr_sig_in, &ltpdel, &phase, &num_gltp, &den_gltp,
		   y_up, &off_yup);
	*vo = ltpdel;

	if (num_gltp == (float) 0.) {
		copy(ptr_sig_in, ptr_sig_pst0, L_SUBFR);
	}

	else {
		if (phase == 0) {
			ptr_y_up = ptr_sig_in - ltpdel;
		}

		else {
			/* Filtering with long filter */
			compute_ltp_l(ptr_sig_in, ltpdel, phase, ptr_sig_pst0,
				      &num2_gltp, &den2_gltp);

			if (select_ltp(num_gltp, den_gltp, num2_gltp, den2_gltp)
			    == 1) {

				/* select short filter */
				ptr_y_up =
				    y_up + ((phase - 1) * L_SUBFRP1 + off_yup);
			} else {
				/* select long filter */
				num_gltp = num2_gltp;
				den_gltp = den2_gltp;
				ptr_y_up = ptr_sig_pst0;
			}

		}

		if (num_gltp >= den_gltp) {
			/* beta bounded to 1 */
			if (gamma_harm == GAMMA_HARM)
				gain_plt = MIN_GPLT;
			else
				gain_plt = (float) 1. / ((float) 1. + gamma_harm);
		} else {
			gain_plt =
			    den_gltp / (den_gltp + gamma_harm * num_gltp);
		}

	/** filtering by H0(z) = harmonic filter **/
		filt_plt(ptr_sig_in, ptr_y_up, ptr_sig_pst0, gain_plt);

	}

	return;
}

/*----------------------------------------------------------------------------
 *  search_del: computes best (shortest) integer LTP delay + fine search
 *----------------------------------------------------------------------------
 */
static void search_del(int t0,	/* input : pitch delay given by coder */
		       float * ptr_sig_in,	/* input : input signal (with delay line) */
		       int *ltpdel,	/* output: delay = *ltpdel - *phase / f_up */
		       int *phase,	/* output: phase */
		       float * num_gltp,	/* output: numerator of LTP gain */
		       float * den_gltp,	/* output: denominator of LTP gain */
		       float * y_up,	/*       : */
		       int *off_yup	/*       : */
    )
{
	/* pointers on tables of constants */
	float *ptr_h;

	/* Variables and local arrays */
	float tab_den0[F_UP_PST - 1], tab_den1[F_UP_PST - 1];
	float *ptr_den0, *ptr_den1;
	float *ptr_sig_past, *ptr_sig_past0;
	float *ptr1;

	int i, n, ioff, i_max;
	float ener, num, numsq, den0, den1;
	float den_int, num_int;
	float den_max, num_max, numsq_max;
	int phi_max;
	int lambda, phi;
	float temp0, temp1;
	float *ptr_y_up;

    /*****************************************/
	/* Compute current signal energy         */
    /*****************************************/

	ener = (float) 0.;
	for (i = 0; i < L_SUBFR; i++) {
		ener += ptr_sig_in[i] * ptr_sig_in[i];
	}
	if (ener < (float) 0.1) {
		*num_gltp = (float) 0.;
		*den_gltp = (float) 1.;
		*ltpdel = 0;
		*phase = 0;
		return;
	}

    /*************************************/
	/* Selects best of 3 integer delays  */
	/* Maximum of 3 numerators around t0 */
	/* coder LTP delay                   */
    /*************************************/
	lambda = t0 - 1;

	ptr_sig_past = ptr_sig_in - lambda;

	num_int = (float) - 1.0e30;

	/* initialization used only to suppress Microsoft Visual C++ warnings */
	i_max = 0;
	for (i = 0; i < 3; i++) {
		num = (float) 0.;
		for (n = 0; n < L_SUBFR; n++) {
			num += ptr_sig_in[n] * ptr_sig_past[n];
		}
		if (num > num_int) {
			i_max = i;
			num_int = num;
		}
		ptr_sig_past--;
	}
	if (num_int <= (float) 0.) {
		*num_gltp = (float) 0.;
		*den_gltp = (float) 1.;
		*ltpdel = 0;
		*phase = 0;
		return;
	}

	/* Calculates denominator for lambda_max */
	lambda += i_max;
	ptr_sig_past = ptr_sig_in - lambda;
	den_int = (float) 0.;
	for (n = 0; n < L_SUBFR; n++) {
		den_int += ptr_sig_past[n] * ptr_sig_past[n];
	}
	if (den_int < (float) 0.1) {
		*num_gltp = (float) 0.;
		*den_gltp = (float) 1.;
		*ltpdel = 0;
		*phase = 0;
		return;
	}
    /***********************************/
	/* Select best phase around lambda */
    /***********************************/

	/* Compute y_up & denominators */
    /*******************************/
	ptr_y_up = y_up;
	den_max = den_int;
	ptr_den0 = tab_den0;
	ptr_den1 = tab_den1;
	ptr_h = tab_hup_s;
	ptr_sig_past0 = ptr_sig_in + LH_UP_S - 1 - lambda;	/* points on lambda_max+1 */

	/* loop on phase  */
	for (phi = 1; phi < F_UP_PST; phi++) {

		/* Computes criterion for (lambda_max+1) - phi/F_UP_PST     */
		/* and lambda_max - phi/F_UP_PST                            */
		ptr_sig_past = ptr_sig_past0;
		/* computes y_up[n] */
		for (n = 0; n <= L_SUBFR; n++) {
			ptr1 = ptr_sig_past++;
			temp0 = (float) 0.;
			for (i = 0; i < LH2_S; i++) {
				temp0 += ptr_h[i] * ptr1[-i];
			}
			ptr_y_up[n] = temp0;
		}

		/* recursive computation of den0 (lambda_max+1) and den1 (lambda_max) */

		/* common part to den0 and den1 */
		temp0 = (float) 0.;
		for (n = 1; n < L_SUBFR; n++) {
			temp0 += ptr_y_up[n] * ptr_y_up[n];
		}

		/* den0 */
		den0 = temp0 + ptr_y_up[0] * ptr_y_up[0];
		*ptr_den0++ = den0;

		/* den1 */
		den1 = temp0 + ptr_y_up[L_SUBFR] * ptr_y_up[L_SUBFR];
		*ptr_den1++ = den1;

		if (fabs(ptr_y_up[0]) > fabs(ptr_y_up[L_SUBFR])) {
			if (den0 > den_max) {
				den_max = den0;
			}
		} else {
			if (den1 > den_max) {
				den_max = den1;
			}
		}
		ptr_y_up += L_SUBFRP1;
		ptr_h += LH2_S;
	}
	if (den_max < (float) 0.1) {
		*num_gltp = (float) 0.;
		*den_gltp = (float) 1.;
		*ltpdel = 0;
		*phase = 0;
		return;
	}
	/* Computation of the numerators                */
	/* and selection of best num*num/den            */
	/* for non null phases                          */

	/* Initialize with null phase */
	num_max = num_int;
	den_max = den_int;
	numsq_max = num_max * num_max;
	phi_max = 0;
	ioff = 1;

	ptr_den0 = tab_den0;
	ptr_den1 = tab_den1;
	ptr_y_up = y_up;

	/* if den_max = 0 : will be selected and declared unvoiced */
	/* if num!=0 & den=0 : will be selected and declared unvoiced */
	/* degenerated seldom cases, switch off LT is OK */

	/* Loop on phase */
	for (phi = 1; phi < F_UP_PST; phi++) {

		/* computes num for lambda_max+1 - phi/F_UP_PST */
		num = (float) 0.;
		for (n = 0; n < L_SUBFR; n++) {
			num += ptr_sig_in[n] * ptr_y_up[n];
		}
		if (num < (float) 0.)
			num = (float) 0.;
		numsq = num * num;

		/* selection if num/sqrt(den0) max */
		den0 = *ptr_den0++;
		temp0 = numsq * den_max;
		temp1 = numsq_max * den0;
		if (temp0 > temp1) {
			num_max = num;
			numsq_max = numsq;
			den_max = den0;
			ioff = 0;
			phi_max = phi;
		}

		/* computes num for lambda_max - phi/F_UP_PST */
		ptr_y_up++;
		num = (float) 0.;
		for (n = 0; n < L_SUBFR; n++) {
			num += ptr_sig_in[n] * ptr_y_up[n];
		}
		if (num < (float) 0.)
			num = (float) 0.;
		numsq = num * num;

		/* selection if num/sqrt(den1) max */
		den1 = *ptr_den1++;
		temp0 = numsq * den_max;
		temp1 = numsq_max * den1;
		if (temp0 > temp1) {
			num_max = num;
			numsq_max = numsq;
			den_max = den1;
			ioff = 1;
			phi_max = phi;
		}
		ptr_y_up += L_SUBFR;
	}

    /***************************************************/
    /*** test if normalised crit0[iopt] > THRESCRIT  ***/
    /***************************************************/

	if ((num_max == (float) 0.) || (den_max <= (float) 0.1)) {
		*num_gltp = (float) 0.;
		*den_gltp = (float) 1.;
		*ltpdel = 0;
		*phase = 0;
		return;
	}

	/* comparison num * num            */
	/* with ener * den x THRESCRIT      */
	temp1 = den_max * ener * THRESCRIT;
	if (numsq_max >= temp1) {
		*ltpdel = lambda + 1 - ioff;
		*off_yup = ioff;
		*phase = phi_max;
		*num_gltp = num_max;
		*den_gltp = den_max;
	} else {
		*num_gltp = (float) 0.;
		*den_gltp = (float) 1.;
		*ltpdel = 0;
		*phase = 0;
	}
	return;
}

/*----------------------------------------------------------------------------
 *  filt_plt -  ltp  postfilter
 *----------------------------------------------------------------------------
 */
static void filt_plt(float * s_in,	/* input : input signal with past */
		     float * s_ltp,	/* input : filtered signal with gain 1 */
		     float * s_out,	/* output: output signal */
		     float gain_plt	/* input : filter gain  */
    )
{
	/* Local variables */
	int n;
	float gain_plt_1;

	gain_plt_1 = (float) 1. - gain_plt;

	for (n = 0; n < L_SUBFR; n++) {
		s_out[n] = gain_plt * s_in[n] + gain_plt_1 * s_ltp[n];
	}

	return;
}

/*----------------------------------------------------------------------------
 *  compute_ltp_l : compute delayed signal,
                    num & den of gain for fractional delay
 *                  with long interpolation filter
 *----------------------------------------------------------------------------
 */
static void compute_ltp_l(float * s_in,	/* input signal with past */
			  int ltpdel,	/* delay factor */
			  int phase,	/* phase factor */
			  float * y_up,	/* delayed signal */
			  float * num,	/* numerator of LTP gain */
			  float * den	/* denominator of LTP gain */
    )
{
	/* Pointer on table of constants */
	float *ptr_h;

	/* Local variables */
	int n, i;
	float *ptr2, temp;

	/* Filtering with long filter */
	ptr_h = tab_hup_l + (phase - 1) * LH2_L;
	ptr2 = s_in - ltpdel + LH_UP_L;

	/* Compute y_up */
	for (n = 0; n < L_SUBFR; n++) {
		temp = (float) 0.;
		for (i = 0; i < LH2_L; i++) {
			temp += ptr_h[i] * *ptr2--;
		}
		y_up[n] = temp;
		ptr2 += LH2_L_P1;
	}

	/* Compute num */
	*num = (float) 0.;
	for (n = 0; n < L_SUBFR; n++) {
		*num += y_up[n] * s_in[n];
	}
	if (*num < (float) 0.0)
		*num = (float) 0.0;

	*den = (float) 0.;
	/* Compute den */
	for (n = 0; n < L_SUBFR; n++) {
		*den += y_up[n] * y_up[n];
	}

	return;
}

/*----------------------------------------------------------------------------
 *  select_ltp : selects best of (gain1, gain2)
 *  with gain1 = num1 * 2** sh_num1 / den1 * 2** sh_den1
 *  and  gain2 = num2 * 2** sh_num2 / den2 * 2** sh_den2
 *----------------------------------------------------------------------------
 */
static int select_ltp(		/* output : 1 = 1st gain, 2 = 2nd gain */
			     float num1,	/* input : numerator of gain1 */
			     float den1,	/* input : denominator of gain1 */
			     float num2,	/* input : numerator of gain2 */
			     float den2	/* input : denominator of gain2 */
    )
{
	if (den2 == (float) 0.) {
		return (1);
	}
	if (num2 * num2 * den1 > num1 * num1 * den2) {
		return (2);
	} else {
		return (1);
	}
}

/*----------------------------------------------------------------------------
 *   calc_st_filt -  computes impulse response of A(gamma2) / A(gamma1)
 *   controls gain : computation of energy impulse response as
 *                    SUMn  (abs (h[n])) and computes parcor0
 *----------------------------------------------------------------------------
 */
static void calc_st_filte(float * apond2,	/* input : coefficients of numerator */
			  float * apond1,	/* input : coefficients of denominator */
			  float * parcor0,	/* output: 1st parcor calcul. on composed filter */
			  float * sig_ltp_ptr,	/* in/out: input of 1/A(gamma1) : scaled by 1/g0 */
			  int long_h_st,	/* input : impulse response length                   */
			  int m_pst	/* input : LPC order    */
    )
{
	float h[LONG_H_ST_E];
	float g0, temp;
	int i;

	/* compute i.r. of composed filter apond2 / apond1 */
	syn_filte(m_pst, apond1, apond2, h, long_h_st, mem_zero, 0);

	/* compute 1st parcor */
	calc_rc0_he(h, parcor0, long_h_st);

	/* compute g0 */
	g0 = (float) 0.;
	for (i = 0; i < long_h_st; i++) {
		g0 += (float) fabs(h[i]);
	}

	/* Scale signal input of 1/A(gamma1) */
	if (g0 > (float) 1.) {
		temp = (float) 1. / g0;
		for (i = 0; i < L_SUBFR; i++) {
			sig_ltp_ptr[i] = sig_ltp_ptr[i] * temp;
		}
	}
	return;
}

/*----------------------------------------------------------------------------
 * calc_rc0_h - computes 1st parcor from composed filter impulse response
 *----------------------------------------------------------------------------
 */
static void calc_rc0_he(float * h,	/* input : impulse response of composed filter */
			float * rc0,	/* output: 1st parcor */
			int long_h_st	/*input : impulse response length */
    )
{
	float acf0, acf1;
	float temp, temp2;
	float *ptrs;
	int i;

	/* computation of the autocorrelation function acf */
	temp = (float) 0.;
	for (i = 0; i < long_h_st; i++) {
		temp += h[i] * h[i];
	}
	acf0 = temp;

	temp = (float) 0.;
	ptrs = h;
	for (i = 0; i < long_h_st - 1; i++) {
		temp2 = *ptrs++;
		temp += temp2 * (*ptrs);
	}
	acf1 = temp;

	/* Initialisation of the calculation */
	if (acf0 == (float) 0.) {
		*rc0 = (float) 0.;
		return;
	}

	/* Compute 1st parcor */
    /**********************/
	if (acf0 < (float) fabs(acf1)) {
		*rc0 = (float) 0.0;
		return;
	}
	*rc0 = -acf1 / acf0;

	return;
}

/*----------------------------------------------------------------------------
 * filt_mu - tilt filtering with : (1 + mu z-1) * (1/1-|mu|)
 *   computes y[n] = (1/1-|mu|) (x[n]+mu*x[n-1])
 *----------------------------------------------------------------------------
 */
static void filt_mu(float * sig_in,	/* input : input signal (beginning at sample -1) */
		    float * sig_out,	/* output: output signal */
		    float parcor0	/* input : parcor0 (mu = parcor0 * gamma3) */
    )
{
	int n;
	float mu, ga, temp;
	float *ptrs;

	if (parcor0 > (float) 0.) {
		mu = parcor0 * GAMMA3_PLUS;
	} else {
		mu = parcor0 * GAMMA3_MINUS;
	}
	ga = (float) 1. / ((float) 1. - (float) fabs(mu));

	ptrs = sig_in;		/* points on sig_in(-1) */
	for (n = 0; n < L_SUBFR; n++) {
		temp = mu * (*ptrs++);
		temp += (*ptrs);
		sig_out[n] = ga * temp;
	}
	return;
}

/*----------------------------------------------------------------------------
 *   scale_st  - control of the subframe gain
 *   gain[n] = AGC_FAC * gain[n-1] + (1 - AGC_FAC) g_in/g_out
 *----------------------------------------------------------------------------
 */
void scale_st(float * sig_in,	/* input : postfilter input signal */
	      float * sig_out,	/* in/out: postfilter output signal */
	      float * gain_prec	/* in/out: last value of gain for subframe */
    )
{
	int i;
	float gain_in, gain_out;
	float g0, gain;

	/* compute input gain */
	gain_in = (float) 0.;
	for (i = 0; i < L_SUBFR; i++) {
		gain_in += (float) fabs(sig_in[i]);
	}
	if (gain_in == (float) 0.) {
		g0 = (float) 0.;
	} else {
		/* Compute output gain */
		gain_out = (float) 0.;
		for (i = 0; i < L_SUBFR; i++) {
			gain_out += (float) fabs(sig_out[i]);
		}
		if (gain_out == (float) 0.) {
			*gain_prec = (float) 0.;
			return;
		}

		g0 = gain_in / gain_out;
		g0 *= AGC_FAC1;
	}

	/* compute gain(n) = AGC_FAC gain(n-1) + (1-AGC_FAC)gain_in/gain_out */
	/* sig_out(n) = gain(n) sig_out(n)                                   */
	gain = *gain_prec;
	for (i = 0; i < L_SUBFR; i++) {
		gain *= AGC_FAC;
		gain += g0;
		sig_out[i] *= gain;
	}
	*gain_prec = gain;
	return;
}
