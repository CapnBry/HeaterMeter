/*
 * Project:  LevenbergMarquardtLeastSquaresFitting
 *
 * File:     lm_eval.h 
 *
 * Contents: Default declarations of 
 *           - user-defined data transfer structure,
 *           - user-defined data evalution function,
 *           - user-defined printout routine.
 *           Default implementation is in lm_eval.c.
 *           Usage is shown in lm_test.c.
 *
 * Author:   Joachim Wuttke 2004-8 
 * 
 * Homepage: www.messen-und-deuten.de/lmfit
 *
 * Licence:  Public domain.
 */
 
#ifndef LM_EVAL_H
#define LM_EVAL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* may be modified to hold arbitrary data */
    double *user_t;
    double *user_y;
    double (*user_func) (double user_t_point, double *par);
} lm_data_type;

void lm_evaluate_default(double *par, int m_dat, double *fvec,
			 void *data, int *info);

void lm_print_default(int n_par, double *par, int m_dat, double *fvec,
		      void *data, int iflag, int iter, int nfev);

#ifdef __cplusplus
}
#endif
		      
#endif /* LM_EVAL_H */
