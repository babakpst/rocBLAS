/* ************************************************************************
 * Copyright (C) 2016-2022 Advanced Micro Devices, Inc. All rights reserved.
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

#include "logging.hpp"
#include "rocblas_block_sizes.h"
#include "rocblas_swap.hpp"
#include "utility.hpp"

namespace
{
    template <typename>
    constexpr char rocblas_swap_strided_batched_name[] = "unknown";
    template <>
    constexpr char rocblas_swap_strided_batched_name<float>[] = "rocblas_sswap_strided_batched";
    template <>
    constexpr char rocblas_swap_strided_batched_name<double>[] = "rocblas_dswap_strided_batched";
    template <>
    constexpr char rocblas_swap_strided_batched_name<rocblas_float_complex>[]
        = "rocblas_cswap_strided_batched";
    template <>
    constexpr char rocblas_swap_strided_batched_name<rocblas_double_complex>[]
        = "rocblas_zswap_strided_batched";

    template <typename T>
    rocblas_status rocblas_swap_strided_batched_impl(rocblas_handle handle,
                                                     rocblas_int    n,
                                                     T*             x,
                                                     rocblas_int    incx,
                                                     rocblas_stride stridex,
                                                     T*             y,
                                                     rocblas_int    incy,
                                                     rocblas_stride stridey,
                                                     rocblas_int    batch_count)
    {
        if(!handle)
            return rocblas_status_invalid_handle;

        RETURN_ZERO_DEVICE_MEMORY_SIZE_IF_QUERIED(handle);

        auto layer_mode     = handle->layer_mode;
        auto check_numerics = handle->check_numerics;
        if(layer_mode & rocblas_layer_mode_log_trace)
            log_trace(handle,
                      rocblas_swap_strided_batched_name<T>,
                      n,
                      x,
                      incx,
                      stridex,
                      y,
                      incy,
                      stridey,
                      batch_count);
        if(layer_mode & rocblas_layer_mode_log_bench)
            log_bench(handle,
                      "./rocblas-bench -f swap_strided_batched -r",
                      rocblas_precision_string<T>,
                      "-n",
                      n,
                      "--incx",
                      incx,
                      "--incy",
                      incy,
                      "--stride_x",
                      stridex,
                      "--stride_y",
                      stridey,
                      "--batch_count",
                      batch_count);
        if(layer_mode & rocblas_layer_mode_log_profile)
            log_profile(handle,
                        rocblas_swap_strided_batched_name<T>,
                        "N",
                        n,
                        "incx",
                        incx,
                        "stride_x",
                        stridex,
                        "incy",
                        incy,
                        "stride_y",
                        stridey,
                        "batch_count",
                        batch_count);

        if(batch_count <= 0 || n <= 0)
            return rocblas_status_success;

        if(!x || !y)
            return rocblas_status_invalid_pointer;
        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status swap_check_numerics_status
                = rocblas_swap_check_numerics(rocblas_swap_strided_batched_name<T>,
                                              handle,
                                              n,
                                              x,
                                              0,
                                              incx,
                                              stridex,
                                              y,
                                              0,
                                              incy,
                                              stridey,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(swap_check_numerics_status != rocblas_status_success)
                return swap_check_numerics_status;
        }

        static constexpr rocblas_int NB     = ROCBLAS_SWAP_NB;
        rocblas_status               status = rocblas_swap_template<NB>(
            handle, n, x, 0, incx, stridex, y, 0, incy, stridey, batch_count);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status swap_check_numerics_status
                = rocblas_swap_check_numerics(rocblas_swap_strided_batched_name<T>,
                                              handle,
                                              n,
                                              x,
                                              0,
                                              incx,
                                              stridex,
                                              y,
                                              0,
                                              incy,
                                              stridey,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(swap_check_numerics_status != rocblas_status_success)
                return swap_check_numerics_status;
        }
        return status;
    }
}

/*
 * ===========================================================================
 *    C wrapper
 * ===========================================================================
 */

extern "C" {

rocblas_status rocblas_sswap_strided_batched(rocblas_handle handle,
                                             rocblas_int    n,
                                             float*         x,
                                             rocblas_int    incx,
                                             rocblas_stride stridex,
                                             float*         y,
                                             rocblas_int    incy,
                                             rocblas_stride stridey,
                                             rocblas_int    batch_count)
try
{
    return rocblas_swap_strided_batched_impl(
        handle, n, x, incx, stridex, y, incy, stridey, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dswap_strided_batched(rocblas_handle handle,
                                             rocblas_int    n,
                                             double*        x,
                                             rocblas_int    incx,
                                             rocblas_stride stridex,
                                             double*        y,
                                             rocblas_int    incy,
                                             rocblas_stride stridey,
                                             rocblas_int    batch_count)
try
{
    return rocblas_swap_strided_batched_impl(
        handle, n, x, incx, stridex, y, incy, stridey, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_cswap_strided_batched(rocblas_handle         handle,
                                             rocblas_int            n,
                                             rocblas_float_complex* x,
                                             rocblas_int            incx,
                                             rocblas_stride         stridex,
                                             rocblas_float_complex* y,
                                             rocblas_int            incy,
                                             rocblas_stride         stridey,
                                             rocblas_int            batch_count)
try
{
    return rocblas_swap_strided_batched_impl(
        handle, n, x, incx, stridex, y, incy, stridey, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_zswap_strided_batched(rocblas_handle          handle,
                                             rocblas_int             n,
                                             rocblas_double_complex* x,
                                             rocblas_int             incx,
                                             rocblas_stride          stridex,
                                             rocblas_double_complex* y,
                                             rocblas_int             incy,
                                             rocblas_stride          stridey,
                                             rocblas_int             batch_count)
try
{
    return rocblas_swap_strided_batched_impl(
        handle, n, x, incx, stridex, y, incy, stridey, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

} // extern "C"
