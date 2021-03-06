#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <float.h>
#include <string.h>
#include "d2/clustering.h"
#include "d2/math.h"
#include "d2/solver.h"
#include "d2/param.h"
#include "d2/centroid_util.h"
#ifdef __USE_MPI__
#include <mpi.h>
#endif

const char intro[]="-------------------------------------------------------------------------\n  Accelerated Discrete Distribution Clustering under Wasserstein Distance\n\tversion preview, build #1, timestamp 2015/9/16 15:40:24\n\twritten by Jianbo Ye, 2014 - 2015\n-------------------------------------------------------------------------\n\n";

extern int d2_alg_type;

const double time_budget_ratio = 2000.0;
double time_budget;
double global_startTime;

int d2_alg_type = D2_CENTROID_BADMM;
int world_rank = 0; 
int nprocs = 1;

/**
 * Compute the distance between i-th d2 in a and j-th d2 in b 
 * Return square root of the undergoing cost as distance
 * @param(i) the cached space indicator
 */
double d2_compute_distance(mph *a, size_t i, 
			   mph *b, size_t j, 
			   int selected_phase, 
			   var_mph *var_work, size_t index_task) {
  int n;
  double d = 0.0, val; assert(a->s_ph == b->s_ph);
  for (n=0; n<a->s_ph; ++n) 
    if (selected_phase < 0 || n == selected_phase) {
      sph *a_sph = a->ph + n, *b_sph = b->ph + n;
      int dim = a->ph[n].dim; assert(dim == b_sph->dim);      
      size_t index = (selected_phase < 0 ? (index_task * a->s_ph + n) : index_task);
      size_t idx = b_sph->p_str[j] * a_sph->p_str_cum[i];
      switch (a_sph->metric_type) {
      case D2_EUCLIDEAN_L2 :
	_D2_FUNC(pdist2)(dim, 
			 b_sph->p_str[j], 
			 a_sph->p_str[i], 
			 b_sph->p_supp + b_sph->p_str_cum[j]*dim, 
			 a_sph->p_supp + a_sph->p_str_cum[i]*dim, 
			 var_work->g_var[n].C + idx);
	val = d2_match_by_distmat(b_sph->p_str[j], 
				  a_sph->p_str[i], 				  
				  var_work->g_var[n].C + idx,
				  b_sph->p_w + b_sph->p_str_cum[j], 
				  a_sph->p_w + a_sph->p_str_cum[i], 
				  NULL, // x and lambda are implemented later
				  NULL,
				  index);
	d += val;
	break;
      case D2_WORD_EMBED :
	_D2_FUNC(pdist2_sym)(dim,
			     b_sph->p_str[j],
			     a_sph->p_str[i],
			     b_sph->p_supp + b_sph->p_str_cum[j]*dim,
			     a_sph->p_supp_sym + a_sph->p_str_cum[i],
			     var_work->g_var[n].C + idx,
			     a_sph->vocab_vec);
	val = d2_match_by_distmat(b_sph->p_str[j], 
				  a_sph->p_str[i], 				  
				  var_work->g_var[n].C + idx,
				  b_sph->p_w + b_sph->p_str_cum[j], 
				  a_sph->p_w + a_sph->p_str_cum[i], 
				  NULL, // x and lambda are implemented later
				  NULL,
				  index);
	d += val;
	break;
      case D2_HISTOGRAM :
	val = d2_match_by_distmat(b_sph->p_str[j],
				  a_sph->p_str[i], 				   
				  a_sph->dist_mat, 
				  b_sph->p_w + b_sph->p_str_cum[j], 
				  a_sph->p_w + a_sph->p_str_cum[i], 
				  NULL, NULL,
				  index);
	d += val;
	break;
      case D2_SPARSE_HISTOGRAM :
	val = d2_match_by_distmat(b_sph->p_str[j],
				  a_sph->p_str[i],
				  var_work->g_var[n].C + idx,
				  b_sph->p_w + b_sph->p_str_cum[j],
				  a_sph->p_w + a_sph->p_str_cum[i],
				  NULL,
				  NULL,
				  index);
	d += val;
	break;
      case D2_N_GRAM : 
	_D2_FUNC(pdist_symbolic)(dim, 
				 b_sph->p_str[j], 
				 a_sph->p_str[i], 
				 b_sph->p_supp_sym + b_sph->p_str_cum[j]*dim, 
				 a_sph->p_supp_sym + a_sph->p_str_cum[i]*dim, 
				 var_work->g_var[n].C + idx,
				 a_sph->vocab_size,
				 a_sph->dist_mat);

	val = d2_match_by_distmat(b_sph->p_str[j], 
				  a_sph->p_str[i], 
				  var_work->g_var[n].C + idx,
				  b_sph->p_w + b_sph->p_str_cum[j], 
				  a_sph->p_w + a_sph->p_str_cum[i], 
				  NULL, // x and lambda are implemented later
				  NULL,
				  index) / dim;

	d += 2*val;	
	break;
      }
    }

  if (d <= 0) return 0.;
  return sqrt(d);
}



/**
 * See the paper for detailed algorithm description: 
 * Using the Triangle Inequality to Accelerate k-Means, Charles Elkan, ICML 2003 
 */

/**
 * Compute the distance from each point to the all centroids.
 * This part can be parallelized.
 */
size_t d2_labeling_prep(__IN_OUT__ mph *p_data,
		      mph *centroids,
		      var_mph * var_work,
		      int selected_phase) {
  size_t i, count = 0, dist_count = 0;
  const size_t size = p_data->size;
  const size_t num_of_labels = centroids->size;
  int *label = p_data->label;
  double startTime;
  trieq *p_tr = &var_work->tr;

  startTime = getRealTime();
  /* step 1 */
  for (i=0; i<num_of_labels; ++i) p_tr->s[i] = DBL_MAX;
  for (i=0; i<num_of_labels * num_of_labels; ++i) p_tr->c[i] = 0;

  /* pre-compute pairwise distance between centroids */
  for (i=0; i<num_of_labels; ++i) 
    if (world_rank == i % nprocs) {
    size_t j;

    for (j=i+1; j<num_of_labels; ++j) {
      double d;
      d = d2_compute_distance(centroids, i, centroids, j, selected_phase, var_work, p_data->size + i); 
      dist_count +=1;
      p_tr->c[i*num_of_labels + j] = d; 
      p_tr->c[i + j*num_of_labels] = d;

      if (p_tr->s[i] > d / 2.f) p_tr->s[i] = d / 2.f;
      if (p_tr->s[j] > d / 2.f) p_tr->s[j] = d / 2.f;
    }    
    }
#ifdef __USE_MPI__
    MPI_Allreduce(MPI_IN_PLACE, p_tr->c, num_of_labels*num_of_labels, MPI_SCALAR, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(MPI_IN_PLACE, p_tr->s, num_of_labels, MPI_SCALAR, MPI_MIN, MPI_COMM_WORLD);
#endif

  /* initialization */
  for (i=0; i<size; ++i) 
    if (d2_alg_type == D2_CENTROID_BADMM)
      { var_work->label_switch[i] = 0; }

  for (i=0; i<size; ++i) {
  /* step 2 */
  if (label[i]<0 || p_tr->u[i] > p_tr->s[label[i]]) {
    int init_label = label[i];
    int jj = init_label>=0? init_label: 0;
    int j;
    SCALAR min_distance;
    SCALAR *U = p_tr->u + i;
    SCALAR *L = p_tr->l + i*num_of_labels;

    /* step 3 */
    for (j=0; j<num_of_labels; ++j) 
      if ((j != jj || init_label < 0) && *U > L[j] && *U > p_tr->c[j*num_of_labels + jj] / 2.) {

	/* 3a. */
	if (p_tr->r[i] == 1) {
	  /* compute distance */
	  double d;
	  d = d2_compute_distance(p_data, i, centroids, jj, selected_phase, var_work, i);
	  dist_count +=1;
	  L[jj] = d;
	  *U = d;
	  min_distance = d;
	  p_tr->r[i] = 0;
	} else {
	  min_distance = *U;
	}

	/* 3b. */
	if ((min_distance > L[j] || min_distance > p_tr->c[j*num_of_labels + jj] / 2.) && j!=jj) {
	  /* compute distance */
	  double d;
	  d = d2_compute_distance(p_data, i, centroids, j, selected_phase, var_work, i);
	  dist_count +=1;
	  L[j] = d;
	  if (d < min_distance) {jj = j; min_distance = d; *U = d;}
	}
      }
    
    if (jj != init_label) {
      label[i] = jj;
      if (d2_alg_type == D2_CENTROID_BADMM) 
	{ var_work->label_switch[i] = 1;}
      count += 1;
    }
  }
  }

#ifdef __USE_MPI__
  assert(sizeof(size_t)  == sizeof(unsigned long long));
  MPI_Allreduce(MPI_IN_PLACE, &count, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allreduce(MPI_IN_PLACE, &dist_count, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
#endif

  VPRINTF("\n\t\t\t\t %ld objects change their labels\n\t\t\t\t %ld distance pairs computed\n\t\t\t\t seconds: %f\n", count, dist_count, getRealTime() - global_startTime);

  // set time budget for update step
  time_budget = time_budget_ratio * (getRealTime() - startTime);
  return count;
}


/**
 * Copy mph data from a to b 
 */
int d2_copy(mph* a, mph *b) {
  int n;
  char new_init_tag = false;
  b->s_ph = a->s_ph;
  b->size = a->size;
  if (!b->ph) {
    b->ph = (sph *) malloc(b->s_ph * sizeof(sph));
    new_init_tag = true;
  }
  for (n=0; n<a->s_ph; ++n) 
    // check whether n-th phase is allocated
    if (a->ph[n].col > 0) {
      // check whether b->ph[n] is allocated; if not, allocate first
      if (new_init_tag)  {
	d2_allocate_sph(b->ph + n, a->ph[n].dim, a->ph[n].str, a->size, 0., a->ph[n].metric_type);
	b->ph[n].col = a->ph[n].col;
	b->ph[n].vocab_size = a->ph[n].vocab_size;
      }
      memcpy(b->ph[n].p_str, a->ph[n].p_str, a->size * sizeof(int));
      memcpy(b->ph[n].p_str_cum, a->ph[n].p_str_cum, a->size * sizeof(size_t));
      memcpy(b->ph[n].p_w, a->ph[n].p_w, a->ph[n].col * sizeof(SCALAR));

      switch (a->ph[n].metric_type) {
      case D2_EUCLIDEAN_L2 :
	memcpy(b->ph[n].p_supp, a->ph[n].p_supp, a->ph[n].col * a->ph[n].dim * sizeof(SCALAR));
	break;
      case D2_HISTOGRAM:
      case D2_SPARSE_HISTOGRAM:
	b->ph[n].dist_mat = a->ph[n].dist_mat;
	break;
      case D2_N_GRAM:
	memcpy(b->ph[n].p_supp_sym, a->ph[n].p_supp_sym, a->ph[n].col * a->ph[n].dim * sizeof(int));
	b->ph[n].dist_mat = a->ph[n].dist_mat;
	break;
      }
    } else {
      b->ph[n].col = 0;
    }
  
  return 0;
}

#if !defined(max)
#define max(X,Y) (((X) > (Y)) ? (X) : (Y))
#endif
size_t d2_labeling_post(mph *p_data,
		      mph *c_old,
		      mph *c_new,
		      var_mph * var_work,
		      int selected_phase) {
  int i, num_of_labels = c_old->size;
  size_t j, size = p_data->size;
  SCALAR *d_changes = _D2_MALLOC_SCALAR(num_of_labels);
  int *label = p_data->label;

  for (i=0; i<num_of_labels; ++i) {
    double d;
    d = d2_compute_distance(c_new, i, c_old, i, selected_phase, var_work, p_data->size + i);
    d_changes[i] = d;
  }

  for (j=0; j<size; ++j) {
    SCALAR * L = var_work->tr.l + j*num_of_labels;
    for (i=0; i<num_of_labels; ++i) L[i] = max(L[i] - d_changes[i], 0);
    var_work->tr.u[j] += d_changes[label[j]];
    var_work->tr.r[j] = 1;
  }

  _D2_FREE(d_changes);
  return 0;
}


/** Compute the distance from each point to the all centroids.
    This part can be parallelized.
 */
size_t d2_labeling(__IN_OUT__ mph *p_data,
		mph *centroids,
		var_mph * var_work,
		int selected_phase) {
  size_t i, count = 0;
  int *label = p_data->label;
  size_t size = p_data->size;
  double cost = 0.f;
  double startTime;

  startTime = getRealTime();

  for (i=0; i<size; ++i) {
    double min_distance = -1;	
    int jj = label[i]>=0? label[i]: 0;
    size_t j;

    for (j=0; j<centroids->size; ++j) {
      double d;
      d = d2_compute_distance(p_data, i, centroids, j, selected_phase, var_work, i);
      if (min_distance < 0 || d < min_distance) {
	min_distance = d; jj = j;
      }
    }
    cost += min_distance * min_distance;

    if (p_data->label[i] == jj) {
      if (d2_alg_type == D2_CENTROID_BADMM) {
	var_work->label_switch[i] = 0;
      }
    } else {
      p_data->label[i] = jj;
      if (d2_alg_type == D2_CENTROID_BADMM) {
	var_work->label_switch[i] = 1;
      }
      count ++;
    }
  }

#ifdef __USE_MPI__
  assert(sizeof(size_t)  == sizeof(unsigned long long));
  MPI_Allreduce(MPI_IN_PLACE, &count, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allreduce(MPI_IN_PLACE, &cost,  1, MPI_SCALAR,   MPI_SUM, MPI_COMM_WORLD);
#endif

  VPRINTF("\t %ld labels change.\tmean cost %lf\ttime %f s [done]\n", 
           count, cost/p_data->global_size, getRealTime() - global_startTime);

  // set time budget for update step
  time_budget = time_budget_ratio * (getRealTime() - startTime) / p_data->num_of_labels;
  
  return count;
}


int d2_init_centroid(mph *p_data, __OUT__ mph *centroids, int selected_phase, int allocate_only) {
  int i;
  // MPI note: to be done only on one node
  // initialize centroids from random
  if (!allocate_only) VPRINTF("Initializing centroids ... "); VFLUSH();
  centroids->s_ph = p_data->s_ph;
  centroids->size = p_data->num_of_labels;
  centroids->ph = (sph *) malloc(p_data->s_ph * sizeof(sph));
  for (i=0; i<p_data->s_ph; ++i) 
    if (selected_phase < 0 || i == selected_phase) {
      /* allocate mem for centroids */
      d2_allocate_sph(&centroids->ph[i],  p_data->ph[i].dim, p_data->ph[i].str, p_data->num_of_labels, 0., p_data->ph[i].metric_type % D2_GROUP_SIZE); // yet a trick here
      /* initialize centroids from random samples */
      if (!allocate_only) {
	d2_centroid_rands(p_data, i, &centroids->ph[i]);
	broadcast_centroids(centroids, i);
      }
    } else {
      centroids->ph[i].dim = p_data->ph[i].dim;
      centroids->ph[i].col = 0;
      centroids->ph[i].metric_type = p_data->ph[i].metric_type % D2_GROUP_SIZE;
    }
  // d2_write(NULL, centroids);     getchar();
  if (!allocate_only) VPRINTF("[done]\n");
  return 0;
}

/**
 * The main algorithm for d2 clustering 
 */
int d2_clustering(int num_of_clusters, 
		  int max_iter, 
		  mph *p_data, 
		  __OUT__ mph *centroids, /**
					     if centroids->ph is not NULL, 
					     then it assumes the centroids are
					     already initialized, otherwise an 
					     initialization will start before 
					     iterations. */
		  int selected_phase,
		  char use_triangle,
		  const char* log_file){
  int i;
  int iter;
  int s_ph = p_data->s_ph;
  size_t size = p_data->size;
  // int *label = p_data->label;
  size_t label_change_count, *label_count;
  var_mph var_work = {.tr = {NULL, NULL, NULL, NULL, NULL}};
  mph the_centroids_copy = {0, 0, 0, NULL, 0, NULL};

  VPRINTF(intro);

  assert(num_of_clusters>0 && max_iter > 0 && selected_phase < s_ph);

  // label all objects as invalid numbers
  p_data->num_of_labels = num_of_clusters;

  if (!centroids->ph) {
    d2_init_centroid(p_data, centroids, selected_phase, false);
  } else {
    VPRINTF("Centroid initialization provided\n");
  }
  assert(centroids->s_ph == s_ph && centroids->size == num_of_clusters);

  // allocate initialize auxiliary variables
  d2_allocate_work(p_data, &var_work, use_triangle, selected_phase);


  // start centroid-based clustering here
  d2_solver_setup();

#ifdef __USE_MPI__
  MPI_Pcontrol(1);
#endif
  global_startTime = getRealTime();
  for (iter=0; iter<max_iter; ++iter) {
    VPRINTF("Round %d ... \n", iter);
    VPRINTF("\tRe-labeling all instances ... "); VFLUSH();
    if (use_triangle)
      label_change_count = d2_labeling_prep(p_data, centroids, &var_work, selected_phase);
    else 
      label_change_count = d2_labeling(p_data, centroids, &var_work, selected_phase);


    if (log_file && ((iter+1) % 10 == 0) ){
      char centroid_filename[255], label_filename[255];
      sprintf(centroid_filename, "%s_c.d2", log_file);
      sprintf(label_filename, "%s.label", log_file);
      d2_write(centroid_filename, centroids);
      d2_write_labels(label_filename, p_data);
    }
    /*********************************************************
     * Termination criterion                                 *
     * For performance profile: comment this part out and    *
     * use --max_iter parameter of main.                     *
     *********************************************************/        
    if (label_change_count < 0.001 * p_data->global_size) {
      VPRINTF("Terminate!\n");        
      break;
    }
    /*********************************************************/


    /* make copies of centroids */
    if (use_triangle) d2_copy(centroids, &the_centroids_copy);

    VPRINTF("\tUpdate centroids ... \n");
    /* update centroids */
    for (i=0; i<s_ph; ++i) 
      if (selected_phase < 0 || i == selected_phase) {
	VPRINTF("\t phase %d: \n", i);            
      
	if (d2_alg_type == D2_CENTROID_BADMM) 
	  d2_centroid_sphBregman(p_data, &var_work, i, centroids->ph + i, centroids->ph + i);
	if (d2_alg_type == D2_CENTROID_GRADDEC)
	  d2_centroid_sphGradDecent(p_data, &var_work, i, centroids->ph + i, centroids->ph + i);
	if (d2_alg_type == D2_CENTROID_ADMM)
	  d2_centroid_sphADMM(p_data, &var_work, i, centroids->ph + i, centroids->ph + i);
      }

    /* post updates */
    if (use_triangle) 
      d2_labeling_post(p_data, &the_centroids_copy, centroids, &var_work, selected_phase);
  }
#ifdef __USE_MPI__
  MPI_Pcontrol(0);
#endif
  VPRINTF("Iteration time: %lf\n", getRealTime() - global_startTime);

  if (use_triangle)  label_change_count = d2_labeling(p_data, centroids, &var_work, selected_phase);
  d2_solver_release();

  d2_free_work(&var_work, selected_phase);
  if (use_triangle) d2_free(&the_centroids_copy);


  label_count = _D2_CALLOC_SIZE_T(num_of_clusters);    
  for (i=0; i<size; ++i) ++label_count[p_data->label[i]];
#ifdef __USE_MPI__
  assert(sizeof(size_t) == sizeof(unsigned long long));
  MPI_Allreduce(MPI_IN_PLACE, label_count, num_of_clusters, MPI_UNSIGNED_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
#endif  
  for (i=0; i<num_of_clusters; ++i) VPRINTF("%d ", label_count[i]); VPRINTF("\n");
  _D2_FREE(label_count);
  return 0;
}


int d2_assignment(int num_of_clusters,
		  mph *p_data, 
		  mph *centroids, 
		  int selected_phase,
		  const char* centroid_filename,
		  const char* meta_filename) {

  var_mph var_work = {.tr = {NULL, NULL, NULL, NULL, NULL}};
  p_data->num_of_labels = num_of_clusters;
  d2_init_centroid(p_data, centroids, selected_phase, true);
  d2_read(centroid_filename, meta_filename, centroids);  
  d2_allocate_work(p_data, &var_work, false, selected_phase);
  d2_solver_setup();
  global_startTime = getRealTime();
  d2_labeling(p_data, centroids, &var_work, selected_phase);
  d2_solver_release();
  d2_free_work(&var_work, selected_phase);
  return 0;
}



