#include "d2_clustering.h"
#include "d2_math.h"
#include "stdio.h"
#include "d2_param.h"
#include "d2_solver.h"
#include <float.h>
#include <assert.h>

#ifndef __APPLE__
#include <omp.h>
#endif 


int d2_centroid_sphADMM(mph *p_data,
			var_mph *var_work,
			int idx_ph,
			sph *c0,
			__OUT__ sph *c) {
  long i,j;
  int iter, nIter = 50, admm, admmIter = 5;
  double fval0, fval = DBL_MAX;
  int *label = p_data->label;
  int num_of_labels = p_data->num_of_labels;
  int str, strxdim;
  long size = p_data->size;
  sph *data_ph = p_data->ph + idx_ph;
  int dim = data_ph->dim;
  int *p_str = data_ph->p_str;
  SCALAR *p_supp = data_ph->p_supp;
  SCALAR *p_w = data_ph->p_w;
  long *p_str_cum = data_ph->p_str_cum;
  SCALAR *C = var_work->g_var[idx_ph].C;
  SCALAR *X = var_work->g_var[idx_ph].X;
  SCALAR *L = var_work->g_var[idx_ph].L;
  long *label_count;
  double rho;
  SCALAR *q;


  if (!c0) {
    d2_centroid_randn(p_data, idx_ph, c);// For profile purpose only
  } else {
    *c = *c0; // warm start (for clustering purpose)
  }  
  str = c->str; assert(str > 0);
  strxdim = str * dim;

  /** Calculate labels counts:
      it could be possible that some clusters might not 
      have any instances
   */
  label_count = _D2_CALLOC_LONG(num_of_labels);    
  for (i=0; i<size; ++i) ++label_count[label[i]];
  for (i=0; i<num_of_labels; ++i) assert(label_count[i] != 0);

  // allocate q for update of c->p_w
  q = _D2_MALLOC_SCALAR(num_of_labels * str);

  /* start iterations */
  nclock_start();
  for (iter = 0; iter < nIter; ++iter) {
    fval0 = fval;
    fval = 0;

    /* compute exact distances */
#pragma omp parallel for reduction(+:fval)
    for (i=0; i<size; ++i) {
      fval += d2_match_by_coordinates(dim,
				      str, c->p_supp + label[i]*strxdim, c->p_w + label[i]*str,
				      p_str[i], p_supp + p_str_cum[i]*dim, p_w + p_str_cum[i],
				      X + p_str_cum[i]*str,
				      L + i*str);
    }
    fval /= size;
    

    printf("\t%d\t%f\t%f\n", iter, fval, nclock_end());        
    if (fabs(fval - fval0) < 1E-3 * fval) break;


    /* update c->p_supp */
    for (i=0; i<strxdim*num_of_labels; ++i) c->p_supp[i] = 0; // reset c->p_supp
    for (i=0; i<str*num_of_labels; ++i) q[i] = 0; //reset q to temporarily storage

    for (i=0;i < size;  ++i) {
      _D2_CBLAS_FUNC(gemm)(CblasColMajor, CblasNoTrans, CblasTrans, 
			   dim, str, p_str[i], 1, p_supp + dim*p_str_cum[i], dim, X + str*p_str_cum[i], str, 1, 
			   c->p_supp + label[i]*strxdim, dim);
      _D2_FUNC(rsum2)(str, p_str[i], X + str*p_str_cum[i], q + label[i]*str);
    }
    for (i=0; i<num_of_labels; ++i) {
      _D2_FUNC(irms)(dim, str, c->p_supp + i*strxdim, q + i*str);
    }

    /*
    fval0 = fval;
    fval = 0;

#pragma omp parallel for reduction(+:fval)
    for (i=0; i<size; ++i) {
      fval += d2_match_by_coordinates(dim,
				      str, c->p_supp + label[i]*strxdim, c->p_w + label[i]*str,
				      p_str[i], p_supp + p_str_cum[i]*dim, p_w + p_str_cum[i],
				      X + p_str_cum[i]*str,
				      L + i*str);
    }
    fval /= size;   
    printf("\t%d\t%f\t%f\n", iter, fval, nclock_end());        
    */

    /** start ADMM **/
    rho = 50 * fval; // empirical parameters

    /* compute C */    
    for (i=0;i < size;  ++i) {
      _D2_FUNC(pdist2)(dim, str, p_str[i], c->p_supp + label[i]*strxdim, p_supp + dim*p_str_cum[i], C + str*p_str_cum[i]);
    }

    /* reset lagrange multiplier L(ambda) */
    for (i=0;i<size*str; ++i) L[i] = 0;
    for (admm = 0; admm < admmIter; ++admm) {
      /* step 1, update X */

      //#pragma omp parallel for 
      for (i=0; i < size; ++i) {
	d2_match_by_distmat_qp(str, p_str[i], 
			       C + p_str_cum[i]*str, 
			       L + i*str,
			       rho,
			       c->p_w + label[i]*str, 
			       p_w + p_str_cum[i], 
			       X + p_str_cum[i]*str, 
			       X + p_str_cum[i]*str);
	/*
	for (j=0; j<p_str[i]; ++j) printf("%lf ", p_w[j]);
	printf("\n");
	
	for (j=0; j<str; ++j) {
	  int k;
	  for (k=0; k<p_str[i]; ++k) 
	    printf("%lf ", X[j + k*str]); 
	  printf("\n");
	} exit(0);
	*/
      }

      /* step 2, update c->p_w */
      for (i=0; i < num_of_labels * str; ++i) q[i] = 0;
      for (i=0; i < size; ++i) {
	_D2_FUNC(rsum2)(str, p_str[i], X + p_str_cum[i]*str, q + label[i]*str);
	_D2_CBLAS_FUNC(axpy)(str, 1, L + i*str, 1, q + label[i]*str, 1);
      } 
      // _D2_FUNC(cnorm)(str, num_of_labels, q, NULL); _D2_CBLAS_FUNC(copy)(num_of_labels*str, q, 1, c->p_w,1);
      for (i=0; i < num_of_labels; ++i) d2_qpsimple(str, label_count[i], q + label[i]*str, c->p_w + label[i]*str);

      /* step 3, update L */
      for (i=0; i<size; ++i) {
	_D2_FUNC(rsum2)(str, p_str[i], X + p_str_cum[i]*str, L + i*str);
	_D2_CBLAS_FUNC(axpy)(str, -1, c->p_w + label[i]*str, 1, L + i*str, 1);
      }
      /** end of ADMM **/
    }
  }


  _D2_FREE(label_count);
  _D2_FREE(q);
  return 0;
}