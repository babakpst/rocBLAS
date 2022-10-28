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
#include "rocblas_spr.hpp"
#include "utility.hpp"

namespace
{
    template <typename>
    constexpr char rocblas_spr_strided_batched_name[] = "unknown";
    template <>
    constexpr char rocblas_spr_strided_batched_name<float>[] = "rocblas_sspr_strided_batched";
    template <>
    constexpr char rocblas_spr_strided_batched_name<double>[] = "rocblas_dspr_strided_batched";
    template <>
    constexpr char rocblas_spr_strided_batched_name<rocblas_float_complex>[]
        = "rocblas_cspr_strided_batched";
    template <>
    constexpr char rocblas_spr_strided_batched_name<rocblas_double_complex>[]
        = "rocblas_zspr_strided_batched";

    template <typename T>
    rocblas_status rocblas_spr_strided_batched_impl(rocblas_handle handle,
                                                    rocblas_fill   uplo,
                                                    rocblas_int    n,
                                                    const T*       alpha,
                                                    const T*       x,
                                                    rocblas_int    incx,
                                                    rocblas_stride stridex,
                                                    T*             AP,
                                                    rocblas_stride strideA,
                                                    rocblas_int    batch_count)
    {
        if(!handle)
            return rocblas_status_invalid_handle;

        RETURN_ZERO_DEVICE_MEMORY_SIZE_IF_QUERIED(handle);

        auto layer_mode     = handle->layer_mode;
        auto check_numerics = handle->check_numerics;
        if(layer_mode
           & (rocblas_layer_mode_log_trace | rocblas_layer_mode_log_bench
              | rocblas_layer_mode_log_profile))
        {
            auto uplo_letter = rocblas_fill_letter(uplo);

            if(layer_mode & rocblas_layer_mode_log_trace)
                log_trace(handle,
                          rocblas_spr_strided_batched_name<T>,
                          uplo,
                          n,
                          LOG_TRACE_SCALAR_VALUE(handle, alpha),
                          x,
                          incx,
                          stridex,
                          AP,
                          strideA,
                          batch_count);

            if(layer_mode & rocblas_layer_mode_log_bench)
                log_bench(handle,
                          "./rocblas-bench -f spr_strided_batched -r",
                          rocblas_precision_string<T>,
                          "--uplo",
                          uplo_letter,
                          "-n",
                          n,
                          LOG_BENCH_SCALAR_VALUE(handle, alpha),
                          "--incx",
                          incx,
                          "--stride_x",
                          stridex,
                          "--stride_a",
                          strideA,
                          "--batch_count",
                          batch_count);

            if(layer_mode & rocblas_layer_mode_log_profile)
                log_profile(handle,
                            rocblas_spr_strided_batched_name<T>,
                            "uplo",
                            uplo_letter,
                            "N",
                            n,
                            "incx",
                            incx,
                            "stride_x",
                            stridex,
                            "stride_a",
                            strideA,
                            "batch_count",
                            batch_count);
        }

        static constexpr rocblas_stride offset_x = 0, offset_A = 0;

        rocblas_status arg_status = rocblas_spr_arg_check(
            handle, uplo, n, alpha, x, offset_x, incx, stridex, AP, offset_A, strideA, batch_count);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status spr_check_numerics_status
                = rocblas_spr_check_numerics(rocblas_spr_strided_batched_name<T>,
                                             handle,
                                             n,
                                             AP,
                                             offset_A,
                                             strideA,
                                             x,
                                             offset_x,
                                             incx,
                                             stridex,
                                             batch_count,
                                             check_numerics,
                                             is_input);
            if(spr_check_numerics_status != rocblas_status_success)
                return spr_check_numerics_status;
        }

        rocblas_status status = rocblas_spr_template(
            handle, uplo, n, alpha, x, offset_x, incx, stridex, AP, offset_A, strideA, batch_count);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status spr_check_numerics_status
                = rocblas_spr_check_numerics(rocblas_spr_strided_batched_name<T>,
                                             handle,
                                             n,
                                             AP,
                                             offset_A,
                                             strideA,
                                             x,
                                             offset_x,
                                             incx,
                                             stridex,
                                             batch_count,
                                             check_numerics,
                                             is_input);
            if(spr_check_numerics_status != rocblas_status_success)
                return spr_check_numerics_status;
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

rocblas_status rocblas_sspr_strided_batched(rocblas_handle handle,
                                            rocblas_fill   uplo,
                                            rocblas_int    n,
                                            const float*   alpha,
                                            const float*   x,
                                            rocblas_int    incx,
                                            rocblas_stride stridex,
                                            float*         AP,
                                            rocblas_stride strideA,
                                            rocblas_int    batch_count)
try
{
    return rocblas_spr_strided_batched_impl(
        handle, uplo, n, alpha, x, incx, stridex, AP, strideA, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dspr_strided_batched(rocblas_handle handle,
                                            rocblas_fill   uplo,
                                            rocblas_int    n,
                                            const double*  alpha,
                                            const double*  x,
                                            rocblas_int    incx,
                                            rocblas_stride stridex,
                                            double*        AP,
                                            rocblas_stride strideA,
                                            rocblas_int    batch_count)
try
{
    return rocblas_spr_strided_batched_impl(
        handle, uplo, n, alpha, x, incx, stridex, AP, strideA, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_cspr_strided_batched(rocblas_handle               handle,
                                            rocblas_fill                 uplo,
                                            rocblas_int                  n,
                                            const rocblas_float_complex* alpha,
                                            const rocblas_float_complex* x,
                                            rocblas_int                  incx,
                                            rocblas_stride               stridex,
                                            rocblas_float_complex*       AP,
                                            rocblas_stride               strideA,
                                            rocblas_int                  batch_count)
try
{
    return rocblas_spr_strided_batched_impl(
        handle, uplo, n, alpha, x, incx, stridex, AP, strideA, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_zspr_strided_batched(rocblas_handle                handle,
                                            rocblas_fill                  uplo,
                                            rocblas_int                   n,
                                            const rocblas_double_complex* alpha,
                                            const rocblas_double_complex* x,
                                            rocblas_int                   incx,
                                            rocblas_stride                stridex,
                                            rocblas_double_complex*       AP,
                                            rocblas_stride                strideA,
                                            rocblas_int                   batch_count)
try
{
    return rocblas_spr_strided_batched_impl(
        handle, uplo, n, alpha, x, incx, stridex, AP, strideA, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

} // extern "C"
