//------------------------------------------------------------------------------
// LAGraph_Calloc:  wrapper for calloc
//------------------------------------------------------------------------------

// LAGraph, (c) 2019-2022 by The LAGraph Contributors, All Rights Reserved.
// SPDX-License-Identifier: BSD-2-Clause
//
// For additional details (including references to third party source code and
// other files) see the LICENSE file or contact permission@sei.cmu.edu. See
// Contributors.txt for a full list of contributors. Created, in part, with
// funding and support from the U.S. Government (see Acknowledgments.txt file).
// DM22-0790

// Contributed by Timothy A. Davis, Texas A&M University

//------------------------------------------------------------------------------

#include "LG_internal.h"

int LAGraph_Calloc
(
    // output:
    void **p,               // pointer to allocated block of memory
    // input:
    size_t nitems,          // number of items
    size_t size_of_item,    // size of each item
    char *msg
)
{
    // check inputs
    LG_CLEAR_MSG ;
    LG_ASSERT (p != NULL, GrB_NULL_POINTER) ;
    (*p) = NULL ;

    // make sure at least one item is allocated
    nitems = LAGRAPH_MAX (1, nitems) ;

    // make sure at least one byte is allocated
    size_of_item = LAGRAPH_MAX (1, size_of_item) ;

    // compute the size and check for integer overflow
    size_t size ;
    bool ok = LG_Multiply_size_t (&size, nitems, size_of_item) ;
    if (!ok || nitems > GrB_INDEX_MAX || size_of_item > GrB_INDEX_MAX)
    {
        // overflow
        return (GrB_OUT_OF_MEMORY) ;
    }

    if (LAGraph_Calloc_function == NULL)
    {
        // calloc function not available; use malloc and memset
        LG_TRY (LAGraph_Malloc (p, nitems, size_of_item, msg)) ;
        memset (*p, 0, size) ;
        return (GrB_SUCCESS) ;
    }

    // use the calloc function
    (*p) = LAGraph_Calloc_function (nitems, size_of_item) ;
    return (((*p) == NULL) ? GrB_OUT_OF_MEMORY : GrB_SUCCESS) ;
}
