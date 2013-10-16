/*
 * Project:  LevenbergMarquardtLeastSquaresFitting
 *
 * File:     lm_test.c
 *
 * Contents: This test program demonstrates the least-squares standard task:
 *           optimize p to fit some data y(t) by my_fit_function(t;p).
 *
 * Author:   Joachim Wuttke 2004-8 
 * 
 * Homepage: www.messen-und-deuten.de/lmfit
 *
 * Licence:  Public domain.
 *
 * Make:     gcc -o lmtest -lm lmmin.c lm_eval.c lm_test.c
 */
 
#include "lmmin.h"
#include "lm_eval.h"
#include <stdio.h>

double my_fit_function(double t, double *p)
{
    return (p[0] * t + (1 - p[0] + p[1] + p[2]) * t * t) /
	(1 + p[1] * t + p[2] * t * t);
}

int main()
{
    // data and pameter arrays:

    int m_dat = 15;
    int n_p = 3;

    double t[15] = { .07, .13, .19, .26, .32, .38, .44, .51,
	.57, .63, .69, .76, .82, .88, .94
    };
    double y[15] = { .24, .35, .43, .49, .55, .61, .66, .71,
	.75, .79, .83, .87, .90, .94, .97
    };
    double p[3] = { 1., 1., 1. };  // use any starting value, but not { 0,0,0 }

    // auxiliary settings:

    lm_control_type control;
    lm_data_type data;

    lm_initialize_control(&control);

    data.user_func = my_fit_function;
    data.user_t = t;
    data.user_y = y;

    // perform the fit:

    printf
	("modify or replace lm_print_default for less verbous fitting\n");

    lm_minimize(m_dat, n_p, p, lm_evaluate_default, lm_print_default,
		&data, &control);

    // print results:

    printf("status: %s after %d evaluations\n",
	   lm_shortmsg[control.info], control.nfev);

    return 0;
}
