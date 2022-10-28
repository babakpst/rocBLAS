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
#include "rocblas_syr2.hpp"
#include "logging.hpp"
#include "utility.hpp"

namespace
{
    template <typename>
    constexpr char rocblas_syr2_name[] = "unknown";
    template <>
    constexpr char rocblas_syr2_name<float>[] = "rocblas_ssyr2";
    template <>
    constexpr char rocblas_syr2_name<double>[] = "rocblas_dsyr2";
    template <>
    constexpr char rocblas_syr2_name<rocblas_float_complex>[] = "rocblas_csyr2";
    template <>
    constexpr char rocblas_syr2_name<rocblas_double_complex>[] = "rocblas_zsyr2";

    template <typename T>
    rocblas_status rocblas_syr2_impl(rocblas_handle handle,
                                     rocblas_fill   uplo,
                                     rocblas_int    n,
                                     const T*       alpha,
                                     const T*       x,
                                     rocblas_int    incx,
                                     const T*       y,
                                     rocblas_int    incy,
                                     T*             A,
                                     rocblas_int    lda)
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
                          rocblas_syr2_name<T>,
                          uplo,
                          n,
                          LOG_TRACE_SCALAR_VALUE(handle, alpha),
                          x,
                          incx,
                          y,
                          incy,
                          A,
                          lda);

            if(layer_mode & rocblas_layer_mode_log_bench)
                log_bench(handle,
                          "./rocblas-bench -f syr2 -r",
                          rocblas_precision_string<T>,
                          "--uplo",
                          uplo_letter,
                          "-n",
                          n,
                          LOG_BENCH_SCALAR_VALUE(handle, alpha),
                          "--lda",
                          lda,
                          "--incx",
                          incx,
                          "--incy",
                          incy);

            if(layer_mode & rocblas_layer_mode_log_profile)
                log_profile(handle,
                            rocblas_syr2_name<T>,
                            "uplo",
                            uplo_letter,
                            "N",
                            n,
                            "lda",
                            lda,
                            "incx",
                            incx,
                            "incy",
                            incy);
        }

        static constexpr rocblas_int    batch_count = 1;
        static constexpr rocblas_stride offset_x = 0, offset_y = 0, offset_A = 0, stride_x = 0,
                                        stride_y = 0, stride_A = 0;

        rocblas_status arg_status = rocblas_syr2_arg_check(handle,
                                                           uplo,
                                                           n,
                                                           alpha,
                                                           x,
                                                           offset_x,
                                                           incx,
                                                           stride_x,
                                                           y,
                                                           offset_y,
                                                           incy,
                                                           stride_y,
                                                           A,
                                                           lda,
                                                           offset_A,
                                                           stride_A,
                                                           batch_count);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status syr2_check_numerics_status
                = rocblas_syr2_check_numerics(rocblas_syr2_name<T>,
                                              handle,
                                              uplo,
                                              n,
                                              A,
                                              offset_A,
                                              lda,
                                              stride_A,
                                              x,
                                              offset_x,
                                              incx,
                                              stride_x,
                                              y,
                                              offset_y,
                                              incy,
                                              stride_y,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(syr2_check_numerics_status != rocblas_status_success)
                return syr2_check_numerics_status;
        }

        rocblas_status status = rocblas_internal_syr2_template(handle,
                                                               uplo,
                                                               n,
                                                               alpha,
                                                               x,
                                                               offset_x,
                                                               incx,
                                                               stride_x,
                                                               y,
                                                               offset_y,
                                                               incy,
                                                               stride_y,
                                                               A,
                                                               lda,
                                                               offset_A,
                                                               stride_A,
                                                               batch_count);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status syr2_check_numerics_status
                = rocblas_syr2_check_numerics(rocblas_syr2_name<T>,
                                              handle,
                                              uplo,
                                              n,
                                              A,
                                              offset_A,
                                              lda,
                                              stride_A,
                                              x,
                                              offset_x,
                                              incx,
                                              stride_x,
                                              y,
                                              offset_y,
                                              incy,
                                              stride_y,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(syr2_check_numerics_status != rocblas_status_success)
                return syr2_check_numerics_status;
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

rocblas_status rocblas_ssyr2(rocblas_handle handle,
                             rocblas_fill   uplo,
                             rocblas_int    n,
                             const float*   alpha,
                             const float*   x,
                             rocblas_int    incx,
                             const float*   y,
                             rocblas_int    incy,
                             float*         A,
                             rocblas_int    lda)
try
{
    return rocblas_syr2_impl(handle, uplo, n, alpha, x, incx, y, incy, A, lda);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dsyr2(rocblas_handle handle,
                             rocblas_fill   uplo,
                             rocblas_int    n,
                             const double*  alpha,
                             const double*  x,
                             rocblas_int    incx,
                             const double*  y,
                             rocblas_int    incy,
                             double*        A,
                             rocblas_int    lda)
try
{
    return rocblas_syr2_impl(handle, uplo, n, alpha, x, incx, y, incy, A, lda);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_csyr2(rocblas_handle               handle,
                             rocblas_fill                 uplo,
                             rocblas_int                  n,
                             const rocblas_float_complex* alpha,
                             const rocblas_float_complex* x,
                             rocblas_int                  incx,
                             const rocblas_float_complex* y,
                             rocblas_int                  incy,
                             rocblas_float_complex*       A,
                             rocblas_int                  lda)
try
{
    return rocblas_syr2_impl(handle, uplo, n, alpha, x, incx, y, incy, A, lda);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_zsyr2(rocblas_handle                handle,
                             rocblas_fill                  uplo,
                             rocblas_int                   n,
                             const rocblas_double_complex* alpha,
                             const rocblas_double_complex* x,
                             rocblas_int                   incx,
                             const rocblas_double_complex* y,
                             rocblas_int                   incy,
                             rocblas_double_complex*       A,
                             rocblas_int                   lda)
try
{
    return rocblas_syr2_impl(handle, uplo, n, alpha, x, incx, y, incy, A, lda);
}
catch(...)
{
    return exception_to_rocblas_status();
}

} // extern "C"
