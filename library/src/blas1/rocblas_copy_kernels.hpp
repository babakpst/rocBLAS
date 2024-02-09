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
#include "rocblas.h"
#include "rocblas_copy.hpp"

template <typename API_INT, typename T, typename U>
ROCBLAS_KERNEL_NO_BOUNDS rocblas_copy_kernel(rocblas_int    n,
                                             const T        xa,
                                             rocblas_stride shiftx,
                                             API_INT        incx,
                                             rocblas_stride stridex,
                                             U              ya,
                                             rocblas_stride shifty,
                                             API_INT        incy,
                                             rocblas_stride stridey)
{
    int64_t     tid = blockIdx.x * blockDim.x + threadIdx.x;
    const auto* x   = load_ptr_batch(xa, blockIdx.y, shiftx, stridex);
    auto*       y   = load_ptr_batch(ya, blockIdx.y, shifty, stridey);
    if(tid < n)
    {

        y[tid * incy] = x[tid * incx];
    }
}

//! @brief Optimized kernel for the floating points.
//!
template <rocblas_int NB, typename T, typename U>
ROCBLAS_KERNEL(NB)
rocblas_scopy_2_kernel(rocblas_int n,
                       const T __restrict xa,
                       rocblas_stride shiftx,
                       rocblas_stride stridex,
                       U __restrict ya,
                       rocblas_stride shifty,
                       rocblas_stride stridey)
{
    int64_t     tid = (blockIdx.x * blockDim.x + threadIdx.x) * 2;
    const auto* x   = load_ptr_batch(xa, blockIdx.y, shiftx, stridex);
    auto*       y   = load_ptr_batch(ya, blockIdx.y, shifty, stridey);
    if(tid < n - 1)
    {
        for(int j = 0; j < 2; ++j)
        {
            y[tid + j] = x[tid + j];
        }
    }
    if(n % 2 != 0 && tid == n - 1)
        y[tid] = x[tid];
}

template <typename API_INT, rocblas_int NB, typename T, typename U>
rocblas_status rocblas_internal_copy_launcher(rocblas_handle handle,
                                              API_INT        n,
                                              T              x,
                                              rocblas_stride offsetx,
                                              API_INT        incx,
                                              rocblas_stride stridex,
                                              U              y,
                                              rocblas_stride offsety,
                                              API_INT        incy,
                                              rocblas_stride stridey,
                                              API_INT        batch_count)
{
    // Quick return if possible.
    if(n <= 0 || batch_count <= 0)
        return rocblas_status_success;

    if(!x || !y)
        return rocblas_status_invalid_pointer;

    static constexpr bool using_rocblas_float
        = std::is_same_v<U, rocblas_float*> || std::is_same_v<U, rocblas_float* const*>;

    if(!using_rocblas_float || incx != 1 || incy != 1)
    {
        // In case of negative inc shift pointer to end of data for negative indexing tid*inc
        int64_t shiftx = offsetx - ((incx < 0) ? int64_t(incx) * (n - 1) : 0);
        int64_t shifty = offsety - ((incy < 0) ? int64_t(incy) * (n - 1) : 0);

        int  blocks = (n - 1) / NB + 1;
        dim3 grid(blocks, batch_count);
        dim3 threads(NB);

        ROCBLAS_LAUNCH_KERNEL(rocblas_copy_kernel,
                              grid,
                              threads,
                              0,
                              handle->get_stream(),
                              n,
                              x,
                              shiftx,
                              incx,
                              stridex,
                              y,
                              shifty,
                              incy,
                              stridey);
    }
    else if constexpr(using_rocblas_float)
    {
        // Kernel function for improving the performance of SCOPY when incx==1 and incy==1

        // In case of negative inc shift pointer to end of data for negative indexing tid*inc
        int64_t shiftx = offsetx - 0;
        int64_t shifty = offsety - 0;

        int  blocks = 1 + ((n - 1) / (NB * 2));
        dim3 grid(blocks, batch_count);
        dim3 threads(NB);

        ROCBLAS_LAUNCH_KERNEL(rocblas_scopy_2_kernel<NB>,
                              grid,
                              threads,
                              0,
                              handle->get_stream(),
                              n,
                              x,
                              shiftx,
                              stridex,
                              y,
                              shifty,
                              stridey);
    }
    return rocblas_status_success;
}
