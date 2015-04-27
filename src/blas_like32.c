#include <math.h>

#define __USE_C99_MATH
#include "common.h"
#include <stdbool.h>
#include "blas_like.h"
#include "blas_util.h"
#include <stdio.h>
#include <assert.h>

#ifdef _D2_SINGLE
void _sgzero(size_t n, float *a) {
  size_t i;
  for (i=0; i<n; ++i) assert(a[i] > 1E-10);
}

void _sadd(size_t n, float *a, float b) {
  size_t i;
  for (i=0; i<n; ++i) a[i] += b;
}

// a(:,*) = a(:,*) .+ b
void _sgcmv(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa, *pb;
  for (i=0,pa=a; i<n; ++i)
    for (j=0,pb=b; j<m; ++j, ++pa, ++pb)
      *pa += *pb;
}

// a(*,:) = a(*,:) .+ b
void _sgrmv(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa =a, *pb =b;
  for (i=0; i<n; ++i,++pb)
    for (j=0; j<m; ++j, ++pa)
      *pa += *pb;
}

// a = diag(b) * a
void _sgcms(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa = a, *pb;
  for (i=0; i<n; ++i)
    for (j=0, pb=b; j<m; ++j, ++pa, ++pb)
      *pa *= *pb;
}

// a = a * diag(b) 
void _sgrms(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa = a, *pb = b;
  for (i=0; i<n; ++i,++pb)
    for (j=0; j<m; ++j, ++pa)
      *pa *= *pb;
}

// a = diag(1./b) * a
void _sicms(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa, *pb;
  for (j=0; j<m; ++j) assert(b[j] > 0);
  for (i=0,pa=a; i<n; ++i)
    for (j=0,pb=b; j<m; ++j, ++pa, ++pb)
      *pa /= *pb;
}

// a = a * diag(1./b) 
void _sirms(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa, *pb;
  for (i=0; i<n; ++i) assert(b[i] > 0);
  for (i=0,pa=a,pb=b; i<n; ++i,++pb) {
    for (j=0; j<m; ++j, ++pa)
      *pa /= *pb;
  }
}

// b(*) = sum(a(:,*))
void _scsum(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa, *pb;
  for (i=0,pa=a,pb=b; i<n; ++i, ++pb) {
    *pb = 0;
    for (j=0; j<m; ++j, ++pa)
      *pb += *pa;
  }
}

// b(*) += sum(a(:,*))
void _scsum2(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa, *pb;
  for (i=0,pa=a,pb=b; i<n; ++i, ++pb) {
    for (j=0; j<m; ++j, ++pa)
      *pb += *pa;
  }
}


// b(*) = sum(a(*,:))
void _srsum(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa, *pb;
  for (j=0,pb=b; j<m; ++j, ++pb) 
    *pb = 0;
  for (i=0,pa=a; i<n; ++i)
    for (j=0,pb=b; j<m; ++j, ++pa, ++pb)
      *pb += *pa;  
}

// b(*) += sum(a(*,:))
void _srsum2(size_t m, size_t n, float *a, float *b) {
  size_t i,j;
  float *pa, *pb;
  for (i=0,pa=a; i<n; ++i)
    for (j=0,pb=b; j<m; ++j, ++pa, ++pb)
      *pb += *pa;  
}

// normalize by column
void _scnorm(size_t m, size_t n, float *a, float *sa) {
  size_t i, j;
  float *pa;
  bool isAllocated = true;
  if (!sa) {
    isAllocated = false;
    sa = _D2_MALLOC_SCALAR(n);
  }    
  _scsum(m, n, a, sa);
  for (i=0; i<n; ++i) assert(sa[i] > 0);
  for (i=0, pa=sa; i<n; ++i, ++pa) {
    for (j=0; j<m; ++j, ++a) (*a) /= *pa;
  }
  if (!isAllocated) _D2_FREE(sa);
}

// normalize by row
void _srnorm(size_t m, size_t n, float *a, float *sa) {
  size_t i, j;
  float *pa;
  bool isAllocated = true;
  if (!sa) {
    isAllocated = false;
    sa = _D2_MALLOC_SCALAR(m);
  }    
  _srsum(m, n, a, sa);
  for (i=0; i<m; ++i) assert(sa[i] > 0);
  for (i=0; i<n; ++i) {
    pa = sa;
    for (j=0; j<m; ++j, ++a, ++pa) (*a) /= *pa;
  }
  if (!isAllocated) _D2_FREE(sa);
}

// center by column
void _sccenter(size_t m, size_t n, float *a, float *sa) {
  size_t i, j;
  float *pa;
  bool isAllocated = true;
  if (!sa) {
    isAllocated = false;
    sa = _D2_MALLOC_SCALAR(n);
  }    
  _scsum(m, n, a, sa);
  cblas_sscal(n, 1./m, sa, 1);
  for (i=0, pa=sa; i<n; ++i, ++pa) {
    for (j=0; j<m; ++j, ++a) (*a) -= *pa;
  }
  if (!isAllocated) _D2_FREE(sa);
}

// center by row
void _srcenter(size_t m, size_t n, float *a, float *sa) {
  size_t i, j;
  float *pa;
  bool isAllocated = true;
  if (!sa) {
    isAllocated = false;
    sa = _D2_MALLOC_SCALAR(m);
  }    
  _srsum(m, n, a, sa);
  cblas_sscal(m, 1./n, sa, 1);
  for (i=0; i<n; ++i) {
    pa = sa;
    for (j=0; j<m; ++j, ++a, ++pa) (*a) -= *pa;
  }
  if (!isAllocated) _D2_FREE(sa);
}

// c = a.*b
void _svmul(size_t n, float *a, float *b, float *c) {
  size_t i;
  for (i=0; i<n; ++i, ++c, ++a, ++b)
    *c = (*a) * (*b);
}

void _spdist2(int d, size_t n, size_t m, float * A, float * B, float *C) {
  size_t i, j; int k;
  assert(d>0 && n>0 && m>0);
  cblas_sgemm(CblasColMajor, CblasTrans, CblasNoTrans, n, m, d, -2, 
	      A, d, B, d, 0, C, n);

  for (i=0; i<m; ++i)
    for (j=0; j<n; ++j)
      for (k=0; k<d; ++k)
	C[i*n + j] += A[j*d + k] * A[j*d + k] + B[i*d + k] * B[i*d + k];
}

void _spdist2_sym(int d, size_t n, size_t m, float *A, int *Bi, float *C, const float *vocab) {
  size_t i, j; int k;
  for (i=0; i<m*n; ++i) C[i] = 0;
  for (i=0; i<m; ++i)
    for (j=0; j<n; ++j) {
      float diff;
      for (k=0; k<d; ++k) 
	if (Bi[i] < 0) {
	  C[i*n +j] += A[j*d+k]*A[j*d+k];
	}	  
	else {
	  diff = A[j*d + k] - vocab[Bi[i]*d + k];
	  C[i*n+j] += diff * diff;
	}
    }
}

void _spdist_symbolic(int d, size_t n, size_t m, int * A, int * B, float *C, 
		      const int vocab_size, const float* dist_mat) {
  size_t i,j; int k;
  assert(d>0 && n>0 && m>0);
 
  for (i=0; i<m*n; ++i) C[i] = 0;
  for (i=0; i<m; ++i)
    for (j=0; j<n; ++j) 
      for (k=0; k<d; ++k) {
	C[i*n+j] += dist_mat[A[j*d + k]*vocab_size + B[i*d + k]];
      }
}

// inplace a -> exp(a)
void _sexp(size_t n, float *a) {
  size_t i;
  for (i=0; i<n; ++i, ++a) *a = exp(*a);
}

#endif
