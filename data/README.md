## Dataset and Format
We provides four types of input formats for programs to read and write d2 data.
Currently, no generic IO is supported for n-gram data, 
but examples of protein n-gram can be found at directory protein_seq/ .

1. [Discrete Distributions over Vector Space].
   We implement multi-phases D2, which means each object can be represented
   by multiple D2. For example, an image can be represented by a D2 in color
   space and a D2 in texture space. The format of .d2 data is as follows:
   ```emacs-lisp
   ;; file ext: .d2
   ;; start of first object
   d1 ;; dimension of the first phase
   n1 ;; number of bins in the first phase
   w{1} w{2} ... w{n1} ;; weights of bins
   x{1,1} x{1,2} ... x{1,d1}
   x{2,1} x{2,2} ... x{2,d1}
   ...
   x{n1,1} x{n1,2} ... x{n1,d1}
   d2 ;; dimension of the second phase
   n2 ;; number of bins in the second phase
   w{1} w{2} ... w{n2} 
   x{1,1} x{1,2} ... x{1,d2}
   x{2,1} x{2,2} ... x{2,d2}
   ...
   x{n2,1} x{n2,2} ... x{n2,d2}
   ;; end of first object
   ;; start of the second object
   ...
   ```
   It is required that `w{i}` are strictly larger than zero.
   
   To read 1000 2-phase entries with the first phase in 3 dimension
   and 6 average number of bins, and the second phase  in 3 dimension
   and 11 bins. Starting from root directory, you may type
   ```bash
   $ time ./d2 -i data/mountaindat.d2 -p 2 -n 1000 -d 3,3 -s 6,11
   ```
2. [Discrete Distribution over Vocabulary Space].
   When the values of each supports are screened from a vocabulary with
   Euclidean embeddings, it would be convenient to represent D2 using
   their symbolic ids (starting from one, zero id saved for empty document).

   ```emacs-lisp
   ;; file ext: .d2s.vocab0
   d m
   <m x d matrix> ;; m is the size of vocabulary, d is the dimension of embeddings
   ;; file ext: .d2s
   d
   n
   w{1} w{2} ... w{n}
   id{1} id{2} ... id{n}
   ...
   ```
   The last row of vocabulary embedding matrix is zero, serving as default
   embedding (in case where id{}<1). Starting from root directory, you may
   type
   ```bash
   $ time ./d2 -i data/mnist/mnist60k.d2s -n 100 -d 2 -s 80 --types 7
   ```
3. [Dense Histograms].
   In some cases, it would be useful to work with histograms (where the sum
   of bins is equal to one). For example, a histogram representation of two
   phases can be as follows:

   ```emacs-lisp
   ;; file ext: .d2.hist0
   n1 ;; number of bins for the first phase
   d{1,1} d{2,1} ... d{n1,1} ;; transportation cost between different bins
   d{1,2} d{2,2} ... d{n1,2} ;; d has to be symmetric, and d{i,i} = 0
   ...
   d{1,n1} d{2,n1} ... d{n1,n1} ;; End of transportation cost

   
   ;; file ext: .d2
   0 n1 w{1,1} w{2,1} ... w{n1,1} ;; first phase histogram of first object

   0 n1 w{1,1} w{2,1} ... w{n1,1} ;; first phase histogram of the second object

   ...
   
   ```
   Generally, it is not necessary that the distance d^{1/p} is a true metric:
   But for using triangle inequality to accelerate the undergoing computation,
   you can enforce to modify the distance such that they are qualified under
   a true metric. 

4. [Sparse Histograms].
   To save computation cost, it is possible to handle histogram data with sparse
   non-zero bins. In those cases, one has to provide a sparse data format to
   enable this feature. The histogram representation is like discrete distribution
   over vocabulary space (aka, 2nd case), with the difference that a distince matrix
   is specified instead of the embedding space. Therefore, an example of sparse
   histogram format is as follows:
   ```emacs-lisp
   ;; file ext: .d2.hist0
   n1 ;; number of bins for this phase
   d{1,1} d{2,1} ... d{n1,1} ;; transportation cost between different bins
   d{1,2} d{2,2} ... d{n1,2} ;; d has to be symmetric, and d{i,i} = 0
   ...
   d{1,n1} d{2,n1} ... d{n1,n1} ;; End of transportation cost
   
   ;; file ext: d2
   0
   n
   w{1} w{2} ... w{n}
   id{1} id{2} ... id{n}
   ...
   
   ```
   Remark it is required that n>0. 


## Data format with hybrid phases

It is possible to read objects with hybrid phases. In such cases, each header file
(ending with `.d2.histN` or `.d2.vocabN`) is associated
with its respective phase. Here `N` denotes the index of phase
(starting from zero). In the main file (ending with `.d2`), objects
are written in the phase-major order. 












