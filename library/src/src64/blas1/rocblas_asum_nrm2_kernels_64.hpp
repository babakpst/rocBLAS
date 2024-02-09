/* ************************************************************************
 * Copyright (C) 2016-2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell cop-
 * ies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IM-
 * PLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNE-
 * CTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ************************************************************************ */

#pragma once

#include "handle.hpp"
#include "int64_helpers.hpp"
#include "rocblas_asum_nrm2_64.hpp"
#include "rocblas_block_sizes.h"

#include "blas1/rocblas_asum_nrm2.hpp" // rocblas_int API called
#include "blas1/rocblas_asum_nrm2_kernels.hpp" // inst kernels with int64_t

// rocblas_reduction_kernel_part2_64 gathers all the partial results in workspace and finishes the final reduction;
template <bool is_finalize, int NB, typename FINALIZE, typename To, typename Tr>
ROCBLAS_KERNEL(NB)
rocblas_reduction_kernel_part2_64(rocblas_int nblocks, To* workspace, Tr* result)
{
    rocblas_int tx = threadIdx.x;
    To          sum;

    if(tx < nblocks)
    {
        To* work = workspace + blockIdx.y * nblocks;
        sum      = work[tx];

        // bound, loop
        for(rocblas_int i = tx + NB; i < nblocks; i += NB)
            sum += work[i];
    }
    else
    { // pad with default value
        sum = rocblas_default_value<To>{}();
    }

    sum = rocblas_dot_block_reduce<NB, To>(sum);

    // Store result on device or in workspace
    if(tx == 0)
        result[blockIdx.y] = Tr(is_finalize ? FINALIZE{}(sum) : sum);
}

//Using specialized launcher which does the FINALIZE only when bool is_finalize is true.
template <bool is_finalize,
          typename API_INT,
          int NB,
          typename FETCH,
          typename FINALIZE,
          typename TPtrX,
          typename To,
          typename Tr>
rocblas_status rocblas_internal_asum_nrm2_kernel_launcher(rocblas_handle handle,
                                                          rocblas_int    n,
                                                          TPtrX          x,
                                                          rocblas_stride shiftx,
                                                          API_INT        incx,
                                                          rocblas_stride stridex,
                                                          rocblas_int    batch_count,
                                                          To*            workspace,
                                                          Tr*            result)
{
    // param REDUCE is always SUM for these kernels so not passed on

    rocblas_int blocks = rocblas_reduction_kernel_block_count(n, NB);

    //Calling the original rocblas_reduction_kernel_part1 kernel in rocbblas_asum_nrm2_kernels.hpp
    ROCBLAS_LAUNCH_KERNEL((rocblas_reduction_kernel_part1<API_INT, NB, FETCH>),
                          dim3(blocks, batch_count),
                          NB,
                          0,
                          handle->get_stream(),
                          n,
                          blocks,
                          x,
                          shiftx,
                          incx,
                          stridex,
                          workspace);

    ROCBLAS_LAUNCH_KERNEL((rocblas_reduction_kernel_part2_64<is_finalize, NB, FINALIZE>),
                          dim3(1, batch_count),
                          NB,
                          0,
                          handle->get_stream(),
                          blocks,
                          workspace,
                          result);

    return rocblas_status_success;
}
