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
#include "rocblas_gbmv.hpp"
#include "logging.hpp"

namespace
{
    template <typename>
    constexpr char rocblas_gbmv_name[] = "unknown";
    template <>
    constexpr char rocblas_gbmv_name<float>[] = "rocblas_sgbmv";
    template <>
    constexpr char rocblas_gbmv_name<double>[] = "rocblas_dgbmv";
    template <>
    constexpr char rocblas_gbmv_name<rocblas_float_complex>[] = "rocblas_cgbmv";
    template <>
    constexpr char rocblas_gbmv_name<rocblas_double_complex>[] = "rocblas_zgbmv";

    template <typename T>
    rocblas_status rocblas_gbmv_impl(rocblas_handle    handle,
                                     rocblas_operation transA,
                                     rocblas_int       m,
                                     rocblas_int       n,
                                     rocblas_int       kl,
                                     rocblas_int       ku,
                                     const T*          alpha,
                                     const T*          A,
                                     rocblas_int       lda,
                                     const T*          x,
                                     rocblas_int       incx,
                                     const T*          beta,
                                     T*                y,
                                     rocblas_int       incy)
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
            auto transA_letter = rocblas_transpose_letter(transA);

            if(layer_mode & rocblas_layer_mode_log_trace)
                log_trace(handle,
                          rocblas_gbmv_name<T>,
                          transA,
                          m,
                          n,
                          kl,
                          ku,
                          LOG_TRACE_SCALAR_VALUE(handle, alpha),
                          A,
                          lda,
                          x,
                          incx,
                          LOG_TRACE_SCALAR_VALUE(handle, beta),
                          y,
                          incy);

            if(layer_mode & rocblas_layer_mode_log_bench)
                log_bench(handle,
                          "./rocblas-bench -f gbmv -r",
                          rocblas_precision_string<T>,
                          "--transposeA",
                          transA_letter,
                          "-m",
                          m,
                          "-n",
                          n,
                          "--kl",
                          kl,
                          "--ku",
                          ku,
                          LOG_BENCH_SCALAR_VALUE(handle, alpha),
                          "--lda",
                          lda,
                          "--incx",
                          incx,
                          LOG_BENCH_SCALAR_VALUE(handle, beta),
                          "--incy",
                          incy);

            if(layer_mode & rocblas_layer_mode_log_profile)
                log_profile(handle,
                            rocblas_gbmv_name<T>,
                            "transA",
                            transA_letter,
                            "M",
                            m,
                            "N",
                            n,
                            "kl",
                            kl,
                            "ku",
                            ku,
                            "lda",
                            lda,
                            "incx",
                            incx,
                            "incy",
                            incy);
        }

        rocblas_status arg_status = rocblas_gbmv_arg_check(handle,
                                                           transA,
                                                           m,
                                                           n,
                                                           kl,
                                                           ku,
                                                           alpha,
                                                           A,
                                                           0,
                                                           lda,
                                                           0,
                                                           x,
                                                           0,
                                                           incx,
                                                           0,
                                                           beta,
                                                           y,
                                                           0,
                                                           incy,
                                                           0,
                                                           1);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status gbmv_check_numerics_status
                = rocblas_gbmv_check_numerics(rocblas_gbmv_name<T>,
                                              handle,
                                              transA,
                                              m,
                                              n,
                                              A,
                                              0,
                                              lda,
                                              0,
                                              x,
                                              0,
                                              incx,
                                              0,
                                              y,
                                              0,
                                              incy,
                                              0,
                                              1,
                                              check_numerics,
                                              is_input);
            if(gbmv_check_numerics_status != rocblas_status_success)
                return gbmv_check_numerics_status;
        }

        rocblas_status status = rocblas_gbmv_template(handle,
                                                      transA,
                                                      m,
                                                      n,
                                                      kl,
                                                      ku,
                                                      alpha,
                                                      A,
                                                      0,
                                                      lda,
                                                      0,
                                                      x,
                                                      0,
                                                      incx,
                                                      0,
                                                      beta,
                                                      y,
                                                      0,
                                                      incy,
                                                      0,
                                                      1);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status gbmv_check_numerics_status
                = rocblas_gbmv_check_numerics(rocblas_gbmv_name<T>,
                                              handle,
                                              transA,
                                              m,
                                              n,
                                              A,
                                              0,
                                              lda,
                                              0,
                                              x,
                                              0,
                                              incx,
                                              0,
                                              y,
                                              0,
                                              incy,
                                              0,
                                              1,
                                              check_numerics,
                                              is_input);
            if(gbmv_check_numerics_status != rocblas_status_success)
                return gbmv_check_numerics_status;
        }
        return status;
    }

} // namespace

/*
* ===========================================================================
*    C wrapper
* ===========================================================================
*/

extern "C" {

rocblas_status rocblas_sgbmv(rocblas_handle    handle,
                             rocblas_operation transA,
                             rocblas_int       m,
                             rocblas_int       n,
                             rocblas_int       kl,
                             rocblas_int       ku,
                             const float*      alpha,
                             const float*      A,
                             rocblas_int       lda,
                             const float*      x,
                             rocblas_int       incx,
                             const float*      beta,
                             float*            y,
                             rocblas_int       incy)
try
{
    return rocblas_gbmv_impl(handle, transA, m, n, kl, ku, alpha, A, lda, x, incx, beta, y, incy);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dgbmv(rocblas_handle    handle,
                             rocblas_operation transA,
                             rocblas_int       m,
                             rocblas_int       n,
                             rocblas_int       kl,
                             rocblas_int       ku,
                             const double*     alpha,
                             const double*     A,
                             rocblas_int       lda,
                             const double*     x,
                             rocblas_int       incx,
                             const double*     beta,
                             double*           y,
                             rocblas_int       incy)
try
{
    return rocblas_gbmv_impl(handle, transA, m, n, kl, ku, alpha, A, lda, x, incx, beta, y, incy);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_cgbmv(rocblas_handle               handle,
                             rocblas_operation            transA,
                             rocblas_int                  m,
                             rocblas_int                  n,
                             rocblas_int                  kl,
                             rocblas_int                  ku,
                             const rocblas_float_complex* alpha,
                             const rocblas_float_complex* A,
                             rocblas_int                  lda,
                             const rocblas_float_complex* x,
                             rocblas_int                  incx,
                             const rocblas_float_complex* beta,
                             rocblas_float_complex*       y,
                             rocblas_int                  incy)
try
{
    return rocblas_gbmv_impl(handle, transA, m, n, kl, ku, alpha, A, lda, x, incx, beta, y, incy);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_zgbmv(rocblas_handle                handle,
                             rocblas_operation             transA,
                             rocblas_int                   m,
                             rocblas_int                   n,
                             rocblas_int                   kl,
                             rocblas_int                   ku,
                             const rocblas_double_complex* alpha,
                             const rocblas_double_complex* A,
                             rocblas_int                   lda,
                             const rocblas_double_complex* x,
                             rocblas_int                   incx,
                             const rocblas_double_complex* beta,
                             rocblas_double_complex*       y,
                             rocblas_int                   incy)
try
{
    return rocblas_gbmv_impl(handle, transA, m, n, kl, ku, alpha, A, lda, x, incx, beta, y, incy);
}
catch(...)
{
    return exception_to_rocblas_status();
}

} // extern "C"
