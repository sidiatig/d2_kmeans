#ifndef _D2_CLUSTERING_H_
#define _D2_CLUSTERING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "global.h"

#define __IN__
#define __OUT__ 
#define __IN_OUT__

  // data structure to store d2 of one phase
  typedef struct {
    int dim, str, size, col;
    int *p_str;
    SCALAR *p_supp;
    SCALAR *p_w;  
  } sph; 

  // data structure to store d2 of multiple phases
  typedef struct {
    int s_ph /* size of phases */, size /* size of entries */;
    int *label;
    int num_of_labels;
    sph *ph;
  } mph;


  /* basic utilities */
  int d2_allocate_sph(__OUT__ sph *p_data_sph,
		      const int d,
		      const int stride,
		      const int num,
		      const double semicol);

  int d2_allocate(__OUT__ mph *p_data,
		  const int size_of_phases,
		  const int size_of_samples,
		  const int *avg_strides,
		  const int *dimension_of_phases);

  int d2_load(void *fp, __OUT__ mph *p_data);
  int d2_free(mph *p_data);

  // working variables that are visible in all algorithms
  typedef struct {
    SCALAR *C;
  } var_sph;

  // working variables specific to Bregman ADMM
  typedef struct {
    SCALAR *X, *Z;
    SCALAR *Y;
    SCALAR *Xc, *Zr;
  } var_sphBregman;

  // union of working variables across multiple phases
  typedef struct {
    int s_ph;
    var_sph *g_var;
    var_sphBregman *l_var_sphBregman; // may not initialized, which depends on the actual centroid algorithm used.
  } var_mph; 


  // interface of random centroids
  int d2_centroid_randn(mph *p_data, 
			int idx_ph, 
			__OUT__ sph *c);
  
  // interface of Bregman ADMM
  int d2_allocate_work_sphBregman(sph *ph, 
				  __OUT__ var_sphBregman * var_phwork);
  int d2_free_work_sphBregman(var_sphBregman * var_phwork);
  int d2_centroid_sphBregman(mph *p_data, // data
			     var_mph * var_work, // working data
			     int idx_ph, // index of phases
			     sph *c0,
			     __OUT__ sph *c);


  // interface to users
  int d2_clustering(int k, 
		    int max_iter, 
		    mph *p_data, 
		    __OUT__ mph *centroids);

#ifdef __cplusplus
}
#endif

#endif /* _D2_CLUSTERING_H_ */
