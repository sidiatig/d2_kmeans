#include "d2/clustering.h"
#include "d2/math.h"
#include "d2/param.h"
#include <stdio.h>
#include <float.h>
#include <assert.h>

void calculate_distmat(sph *data_ph, int* label, size_t size, sph *c, SCALAR* C) {
  size_t dim = c->dim, str = c->str, col = c->col, strxdim = c->dim*c->str;
  size_t i;
  int *p_str = data_ph->p_str;
  SCALAR *p_supp = data_ph->p_supp;
  int *p_supp_sym = data_ph->p_supp_sym;
  size_t *p_str_cum = data_ph->p_str_cum;

  switch (data_ph->metric_type) {
  case D2_EUCLIDEAN_L2 :
    for (i=0;i < size;  ++i) 
      _D2_FUNC(pdist2)(dim, str, p_str[i], c->p_supp + label[i]*strxdim, p_supp + dim*p_str_cum[i], C + str*p_str_cum[i]);
    break;    

  case D2_SEMI_EUCLIDEAN :
    for (i=0;i < size;  ++i) 
      _D2_FUNC(pdist2)(dim, str, p_str[i], c->p_supp + label[i]*strxdim, p_supp + dim*p_str_cum[i], C + str*p_str_cum[i]);
    {
    SCALAR sigma = data_ph->metric_param * 2.;
    SCALAR sigma2 = data_ph->metric_param * data_ph->metric_param;
    for (i=0; i<str * col; ++i)
      if (C[i] > sigma2) C[i] = sigma * sqrt(C[i]) - sigma2;
    }
    break;    

  case D2_WORD_EMBED : 
    for (i=0; i< size; ++i)
      _D2_FUNC(pdist2_sym)(dim, str, p_str[i], c->p_supp + label[i]*strxdim, p_supp_sym + p_str_cum[i], C + str*p_str_cum[i], data_ph->vocab_vec);
    break;

  case D2_SEMI_WORD_EMBED : 
    for (i=0; i< size; ++i)
      _D2_FUNC(pdist2_sym)(dim, str, p_str[i], c->p_supp + label[i]*strxdim, p_supp_sym + p_str_cum[i], C + str*p_str_cum[i], data_ph->vocab_vec);
    {
    SCALAR sigma = data_ph->metric_param * 2.;
    SCALAR sigma2 = data_ph->metric_param * data_ph->metric_param;
    for (i=0; i<str * col; ++i)
      if (C[i] > sigma2) C[i] = sigma * sqrt(C[i]) - sigma2;
    }
    break;

  case D2_HISTOGRAM : 
    for (i=0; i< size; ++i) 
      _D2_CBLAS_FUNC(copy)(str*p_str[i], data_ph->dist_mat, 1, C + str*p_str_cum[i], 1);
    break;

  case D2_N_GRAM :
    for (i=0;i < size; ++i)
      _D2_FUNC(pdist_symbolic)(dim, str, p_str[i], c->p_supp_sym + label[i]*strxdim, p_supp_sym + dim*p_str_cum[i], C + str*p_str_cum[i], data_ph->vocab_size, data_ph->dist_mat);
    break;
  }
}
