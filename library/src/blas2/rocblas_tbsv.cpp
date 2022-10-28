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
#include "rocblas_tbsv.hpp"
#include "handle.hpp"
#include "logging.hpp"
#include "rocblas.h"
#include "rocblas_gemv.hpp"
#include "utility.hpp"

namespace
{
    constexpr rocblas_int STBSV_BLOCK = 512;
    constexpr rocblas_int DTBSV_BLOCK = 512;

    template <typename>
    constexpr char rocblas_tbsv_name[] = "unknown";
    template <>
    constexpr char rocblas_tbsv_name<float>[] = "rocblas_stbsv";
    template <>
    constexpr char rocblas_tbsv_name<double>[] = "rocblas_dtbsv";
    template <>
    constexpr char rocblas_tbsv_name<rocblas_float_complex>[] = "rocblas_ctbsv";
    template <>
    constexpr char rocblas_tbsv_name<rocblas_double_complex>[] = "rocblas_ztbsv";

    template <rocblas_int BLOCK, typename T>
    rocblas_status rocblas_tbsv_impl(rocblas_handle    handle,
                                     rocblas_fill      uplo,
                                     rocblas_operation transA,
                                     rocblas_diagonal  diag,
                                     rocblas_int       n,
                                     rocblas_int       k,
                                     const T*          A,
                                     rocblas_int       lda,
                                     T*                x,
                                     rocblas_int       incx)
    {
        if(!handle)
            return rocblas_status_invalid_handle;

        RETURN_ZERO_DEVICE_MEMORY_SIZE_IF_QUERIED(handle);

        auto layer_mode     = handle->layer_mode;
        auto check_numerics = handle->check_numerics;
        if(layer_mode & rocblas_layer_mode_log_trace)
            log_trace(handle, rocblas_tbsv_name<T>, uplo, transA, diag, n, k, A, lda, x, incx);

        if(layer_mode & (rocblas_layer_mode_log_bench | rocblas_layer_mode_log_profile))
        {
            auto uplo_letter   = rocblas_fill_letter(uplo);
            auto transA_letter = rocblas_transpose_letter(transA);
            auto diag_letter   = rocblas_diag_letter(diag);

            if(layer_mode & rocblas_layer_mode_log_bench)
            {
                if(handle->pointer_mode == rocblas_pointer_mode_host)
                    log_bench(handle,
                              "./rocblas-bench -f tbsv -r",
                              rocblas_precision_string<T>,
                              "--uplo",
                              uplo_letter,
                              "--transposeA",
                              transA_letter,
                              "--diag",
                              diag_letter,
                              "-n",
                              n,
                              "-k",
                              k,
                              "--lda",
                              lda,
                              "--incx",
                              incx);
            }

            if(layer_mode & rocblas_layer_mode_log_profile)
                log_profile(handle,
                            rocblas_tbsv_name<T>,
                            "uplo",
                            uplo_letter,
                            "transA",
                            transA_letter,
                            "diag",
                            diag_letter,
                            "N",
                            n,
                            "K",
                            k,
                            "lda",
                            lda,
                            "incx",
                            incx);
        }

        rocblas_status arg_status
            = rocblas_tbsv_arg_check(handle, uplo, transA, diag, n, k, A, lda, x, incx, 1);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status tbsv_check_numerics_status
                = rocblas_tbsv_check_numerics(rocblas_tbsv_name<T>,
                                              handle,
                                              n,
                                              A,
                                              0,
                                              lda,
                                              0,
                                              x,
                                              0,
                                              incx,
                                              0,
                                              1,
                                              check_numerics,
                                              is_input);
            if(tbsv_check_numerics_status != rocblas_status_success)
                return tbsv_check_numerics_status;
        }

        rocblas_status status = rocblas_tbsv_template<BLOCK>(
            handle, uplo, transA, diag, n, k, A, 0, lda, 0, x, 0, incx, 0, 1);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status tbsv_check_numerics_status
                = rocblas_tbsv_check_numerics(rocblas_tbsv_name<T>,
                                              handle,
                                              n,
                                              A,
                                              0,
                                              lda,
                                              0,
                                              x,
                                              0,
                                              incx,
                                              0,
                                              1,
                                              check_numerics,
                                              is_input);
            if(tbsv_check_numerics_status != rocblas_status_success)
                return tbsv_check_numerics_status;
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

rocblas_status rocblas_stbsv(rocblas_handle    handle,
                             rocblas_fill      uplo,
                             rocblas_operation transA,
                             rocblas_diagonal  diag,
                             rocblas_int       n,
                             rocblas_int       k,
                             const float*      A,
                             rocblas_int       lda,
                             float*            x,
                             rocblas_int       incx)
try
{
    return rocblas_tbsv_impl<STBSV_BLOCK>(handle, uplo, transA, diag, n, k, A, lda, x, incx);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dtbsv(rocblas_handle    handle,
                             rocblas_fill      uplo,
                             rocblas_operation transA,
                             rocblas_diagonal  diag,
                             rocblas_int       n,
                             rocblas_int       k,
                             const double*     A,
                             rocblas_int       lda,
                             double*           x,
                             rocblas_int       incx)
try
{
    return rocblas_tbsv_impl<DTBSV_BLOCK>(handle, uplo, transA, diag, n, k, A, lda, x, incx);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_ctbsv(rocblas_handle               handle,
                             rocblas_fill                 uplo,
                             rocblas_operation            transA,
                             rocblas_diagonal             diag,
                             rocblas_int                  n,
                             rocblas_int                  k,
                             const rocblas_float_complex* A,
                             rocblas_int                  lda,
                             rocblas_float_complex*       x,
                             rocblas_int                  incx)
try
{
    return rocblas_tbsv_impl<STBSV_BLOCK>(handle, uplo, transA, diag, n, k, A, lda, x, incx);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_ztbsv(rocblas_handle                handle,
                             rocblas_fill                  uplo,
                             rocblas_operation             transA,
                             rocblas_diagonal              diag,
                             rocblas_int                   n,
                             rocblas_int                   k,
                             const rocblas_double_complex* A,
                             rocblas_int                   lda,
                             rocblas_double_complex*       x,
                             rocblas_int                   incx)
try
{
    return rocblas_tbsv_impl<DTBSV_BLOCK>(handle, uplo, transA, diag, n, k, A, lda, x, incx);
}
catch(...)
{
    return exception_to_rocblas_status();
}

} // extern "C"
