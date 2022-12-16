//------------------------------------------------------------------------------
// LAGraph/src/benchmark/tc_demo.c: benchmark for LAGr_TriangleCount 
//------------------------------------------------------------------------------

// LAGraph, (c) 2021 by The LAGraph Contributors, All Rights Reserved.
// SPDX-License-Identifier: BSD-2-Clause
// See additional acknowledgments in the LICENSE file,
// or contact permission@sei.cmu.edu for the full terms.

// Contributed by Timothy A. Davis, Texas A&M University

//------------------------------------------------------------------------------

// Usage:  test_tc < matrixmarketfile.mtx
//         test_tc matrixmarketfile.mtx
//         test_tc matrixmarketfile.grb

//  Known triangle counts:
//      kron:       106873365648
//      urand:      5378
//      twitter:    34824916864
//      web:        84907041475
//      road:       438804

#include "LAGraph_demo_GPU.h"

#define NTHREAD_LIST 2
#define THREAD_LIST 40, 0

//  #define NTHREAD_LIST 7
//  #define THREAD_LIST 64, 32, 24, 12, 8, 4, 0

#define LG_FREE_ALL                 \
{                                   \
    LAGraph_Delete (&G, NULL) ;     \
    GrB_free (&A) ;                 \
}

char t [256] ;

char *method_name (int method, int sorting)
{
    char *s ;
    switch (method)
    {
        case LAGraph_TriangleCount_Default:    s = "default (SandiaDot)             " ; break ;
        case LAGraph_TriangleCount_Burkhardt:  s = "Burkhardt:  sum ((A^2) .* A) / 6" ; break ;
        case LAGraph_TriangleCount_Cohen:      s = "Cohen:      sum ((L*U) .* A) / 2" ; break ;
        case LAGraph_TriangleCount_Sandia:     s = "Sandia:     sum ((L*L) .* L)    " ; break ;
        case LAGraph_TriangleCount_Sandia2:    s = "Sandia2:    sum ((U*U) .* U)    " ; break ;
        case LAGraph_TriangleCount_SandiaDot:  s = "SandiaDot:  sum ((L*U') .* L)   " ; break ;
        case LAGraph_TriangleCount_SandiaDot2: s = "SandiaDot2: sum ((U*L') .* U)   " ; break ;
        default: abort ( ) ;
    }

    if (sorting == LAGraph_TriangleCount_Descending) sprintf (t, "%s sort: descending degree", s) ;
    else if (sorting == LAGraph_TriangleCount_Ascending) sprintf (t, "%s ascending degree", s) ;
    else if (sorting == LAGraph_TriangleCount_AutoSort) sprintf (t, "%s auto-sort", s) ;
    else sprintf (t, "%s sort: none", s) ;
    return (t) ;
}


void print_method (FILE *f, int method, int sorting)
{
    fprintf (f, "%s\n", method_name (method, sorting)) ;
}

int main (int argc, char **argv)
{

    //--------------------------------------------------------------------------
    // initialize LAGraph and GraphBLAS
    //--------------------------------------------------------------------------

    char msg [LAGRAPH_MSG_LEN] ;

    GrB_Matrix A = NULL ;
    LAGraph_Graph G = NULL ;

    // start GraphBLAS and LAGraph
    bool burble = true ;
    demo_init (burble) ;

    int ntrials = 5 ;
    ntrials = 3 ;        // HACK
    printf ("# of trials: %d\n", ntrials) ;

    int nt = NTHREAD_LIST ;
    int Nthreads [20] = { 0, THREAD_LIST } ;
    int nthreads_max, nthreads_hi, nthreads_lo ;
    LAGRAPH_TRY (LAGraph_GetNumThreads (&nthreads_hi, &nthreads_lo, msg)) ;
    nthreads_max = nthreads_hi * nthreads_lo ;
    if (Nthreads [1] == 0)
    {
        // create thread list automatically
        Nthreads [1] = nthreads_max ;
        for (int t = 2 ; t <= nt ; t++)
        {
            Nthreads [t] = Nthreads [t-1] / 2 ;
            if (Nthreads [t] == 0) nt = t-1 ;
        }
    }
    printf ("threads to test: ") ;
    for (int t = 1 ; t <= nt ; t++)
    {
        int nthreads = Nthreads [t] ;
        if (nthreads > nthreads_max) continue ;
        printf (" %d", nthreads) ;
    }
    printf ("\n") ;

    //--------------------------------------------------------------------------
    // read in the graph
    //--------------------------------------------------------------------------

    char *matrix_name = (argc > 1) ? argv [1] : "stdin" ;
    LAGRAPH_TRY (readproblem (&G, NULL,
        true, true, true, NULL, false, argc, argv)) ;
    LAGRAPH_TRY (LAGraph_DisplayGraph (G, LAGraph_SHORT, stdout, msg)) ;

    // determine the cached out degree property
    LAGRAPH_TRY (LAGraph_Cached_OutDegree (G, msg)) ;

    GrB_Index n, nvals ;
    GRB_TRY (GrB_Matrix_nrows (&n, G->A)) ;
    GRB_TRY (GrB_Matrix_nvals (&nvals, G->A)) ;

//  printf ("Hack: force hypersparse\n") ;
//  GxB_set (G->A, GxB_SPARSITY_CONTROL, GxB_HYPERSPARSE) ;

#if 0
    // HACK: make sure G->A is non-iso

    // create an iterator
    GxB_Iterator iterator ;
    GxB_Iterator_new (&iterator) ;
    // attach it to the matrix A
    GrB_Info info = GxB_Matrix_Iterator_attach (iterator, G->A, NULL) ;
    if (info < 0) { abort ( ) ; }
    // seek to the first entry
    info = GxB_Matrix_Iterator_seek (iterator, 1) ;
    printf ("info %d\n", info) ;
    while (info != GxB_EXHAUSTED)
    {
        // get the entry A(i,j)
        GrB_Index i, j ;
        GxB_Matrix_Iterator_getIndex (iterator, &i, &j) ;
        // set it to 0
        printf ("setting A(%d,%d) = 0\n", (int) i, (int) j) ;
        GRB_TRY (GrB_Matrix_setElement (G->A, 0, i, j)) ;
        break ;
    }
    GrB_free (&iterator) ;

    GxB_print (G->A, 2) ;
    GrB_wait (G->A, GrB_MATERIALIZE) ;

#else
    bool A_iso ;
    GxB_Matrix_iso (&A_iso, A) ;
    printf ("G->A iso: %d\n", A_iso) ;
#endif

    //--------------------------------------------------------------------------
    // triangle counting
    //--------------------------------------------------------------------------

    GrB_Index ntriangles, ntsimple = 0 ;
    double tic [2], ttot ;

#if 1
    // check # of triangles
    LAGRAPH_TRY (LAGraph_Tic (tic, NULL)) ;
    LAGRAPH_TRY (LG_check_tri (&ntsimple, G, NULL)) ;
    double tsimple ;
    LAGRAPH_TRY (LAGraph_Toc (&tsimple, tic, NULL)) ;
    printf ("# of triangles: %" PRId64 " slow time: %g sec\n",
        ntsimple, tsimple) ;
#endif

    // warmup for more accurate timing, and also print # of triangles
    printf ("\nwarmup method: ") ;
//  int presort = LAGraph_TriangleCount_AutoSort ; // = 2 (auto selection)
    int presort = LAGraph_TriangleCount_NoSort ;    // HACK
    print_method (stdout, 6, presort) ;

    // warmup method: without the GPU
    // LAGraph_TriangleCount_SandiaDot2 = 6,   // sum (sum ((U * L') .* U))
    GxB_set (GxB_GPU_CONTROL, GxB_GPU_NEVER) ;
    LAGRAPH_TRY (LAGraph_Tic (tic, NULL)) ;
    LAGRAPH_TRY (LAGr_TriangleCount (&ntriangles, G,
        LAGraph_TriangleCount_SandiaDot2, &presort, msg) );
    LAGRAPH_TRY (LAGraph_Toc (&ttot, tic, NULL)) ;
    printf ("# of triangles: %" PRIu64 " (CPU)\n", ntriangles) ;
    print_method (stdout, 6, presort) ;
    printf ("nthreads: %3d time: %12.6f rate: %6.2f (SandiaDot2, one trial)\n",
            nthreads_max, ttot, 1e-6 * nvals / ttot) ;

#if 1
    if (ntriangles != ntsimple)
    {
        printf ("wrong # triangles: %g %g\n", (double) ntriangles,
            (double) ntsimple) ;
        fflush (stdout) ;
        fflush (stderr) ;
        // abort ( ) ;
    }
#endif

    // warmup method: with the GPU
    // LAGraph_TriangleCount_SandiaDot2 = 6,   // sum (sum ((U * L') .* U))
    GrB_Index ntriangles_gpu ;
    //presort = LAGraph_TriangleCount_NoSort ; //turn off sorting on GPU
    GxB_set (GxB_GPU_CONTROL, GxB_GPU_ALWAYS) ;
    LAGRAPH_TRY (LAGraph_Tic (tic, NULL)) ;
    LAGRAPH_TRY (LAGr_TriangleCount (&ntriangles_gpu, G,
        LAGraph_TriangleCount_SandiaDot2, &presort, msg) );
    LAGRAPH_TRY (LAGraph_Toc (&ttot, tic, NULL)) ;
    printf ("# of triangles: %" PRIu64 " (GPU)\n", ntriangles_gpu) ;
    print_method (stdout, 6, presort) ;
    printf ("nthreads: %3d time: %12.6f rate: %6.2f (SandiaDot2, warmup GPU)\n",
            nthreads_max, ttot, 1e-6 * nvals / ttot) ;

    GxB_set (GxB_GPU_CONTROL, GxB_GPU_NEVER) ;

    presort = LAGraph_TriangleCount_AutoSort ; // = 2 (auto selection)
#if 1
    if (ntriangles_gpu != ntsimple)
    {
        printf ("wrong # triangles: %g %g\n", (double) ntriangles_gpu,
            (double) ntsimple) ;
        fflush (stdout) ;
        fflush (stderr) ;
        // abort ( ) ;
    }
#endif

    double t_best = INFINITY ;
    int method_best = -1 ;
    int nthreads_best = -1 ;
    int sorting_best = 0 ;

    // kron: input graph: nodes: 134217726 edges: 4223264644
    // fails on methods 3 and 4.

    // just try methods 5 and 6
    // for (int method = 5 ; method <= 6 ; method++)

    // try all methods 3 to 5
    for (int method = 1 ; method <= 1 ; method++)
    {
        // for (int sorting = -1 ; sorting <= 2 ; sorting++)

        int sorting = LAGraph_TriangleCount_AutoSort ; // just use auto-sort
            sorting = LAGraph_TriangleCount_NoSort ;    // HACK: no-sort
        {
            printf ("\nMethod: ") ;
            int presort ;
            print_method (stdout, method, sorting) ;
            if (n == 134217726 && method < 5)
            {
                printf ("kron fails on method %d; skipped\n", method) ;
                continue ;
            }

            for (int t = 1 ; t <= nt ; t++)
            {
                int nthreads = Nthreads [t] ;
                if (nthreads > nthreads_max) continue ;
                if (nthreads != 0) // Don't Use GPU
                {
                  GxB_Global_Option_set( GxB_GLOBAL_GPU_CONTROL, GxB_GPU_NEVER);
                  printf(" CPU ONLY using %d threads", nthreads);
                  presort = LAGraph_TriangleCount_AutoSort ; // = 2 (auto selection)
                }
                else
                {
                  nthreads = 40;
                  GxB_Global_Option_set( GxB_GLOBAL_GPU_CONTROL, GxB_GPU_ALWAYS);
                  printf(" GPU ONLY using %d threads", nthreads);
                  presort = LAGraph_TriangleCount_NoSort ; //turn off sorting on GPU
                }

                LAGRAPH_TRY (LAGraph_SetNumThreads (1, nthreads, msg)) ;
                GrB_Index nt2 ;
                double ttot = 0, ttrial [100] ;
                for (int trial = 0 ; trial < ntrials ; trial++)
                {
                    LAGRAPH_TRY (LAGraph_Tic (tic, NULL)) ;
                    //presort = sorting ;

                    LAGRAPH_TRY(
                        LAGr_TriangleCount (&nt2, G, method,
                                                      &presort, msg) );

                    LAGRAPH_TRY (LAGraph_Toc (&ttrial [trial], tic, NULL)) ;
                    ttot += ttrial [trial] ;
                    printf ("trial %2d: %12.6f sec rate %6.2f  # triangles: "
                        "%g\n", trial, ttrial [trial],
                        1e-6 * nvals / ttrial [trial], (double) nt2) ;
                }
                ttot = ttot / ntrials ;
                printf ("nthreads: %3d time: %12.6f rate: %6.2f", nthreads,
                        ttot, 1e-6 * nvals / ttot) ;
                printf ("   # of triangles: %" PRId64 " presort: %d\n",
                        nt2, presort) ;
            #if 1
                if (nt2 != ntriangles)
                {
                    printf ("Test failure!\n") ;
                    fflush (stdout) ;
                    fflush (stderr) ;
                    // abort ( ) ;
                }
            #endif
                fprintf (stderr, "Avg: TC method%d.%d %3d: %10.3f sec: %s\n",
                         method, sorting, nthreads, ttot, matrix_name) ;

                if (ttot < t_best)
                {
                    t_best = ttot ;
                    method_best = method ;
                    nthreads_best = nthreads ;
                    sorting_best = sorting ;
                }
            }
        }
    }

    printf ("\nBest method: ") ;
    print_method (stdout, method_best, sorting_best) ;
    printf ("nthreads: %3d time: %12.6f rate: %6.2f\n",
        nthreads_best, t_best, 1e-6 * nvals / t_best) ;
    LG_FREE_ALL ;
    LAGRAPH_TRY (LAGraph_Finalize (msg)) ;
    return (GrB_SUCCESS) ;
}

