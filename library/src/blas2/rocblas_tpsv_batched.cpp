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
#include "handle.hpp"
#include "logging.hpp"
#include "rocblas.h"
#include "rocblas_tpsv.hpp"
#include "utility.hpp"

namespace
{
    constexpr rocblas_int STPSV_BLOCK = 512;
    constexpr rocblas_int DTPSV_BLOCK = 512;

    template <typename>
    constexpr char rocblas_tpsv_batched_name[] = "unknown";
    template <>
    constexpr char rocblas_tpsv_batched_name<float>[] = "rocblas_stpsv_batched";
    template <>
    constexpr char rocblas_tpsv_batched_name<double>[] = "rocblas_dtpsv_batched";
    template <>
    constexpr char rocblas_tpsv_batched_name<rocblas_float_complex>[] = "rocblas_ctpsv_batched";
    template <>
    constexpr char rocblas_tpsv_batched_name<rocblas_double_complex>[] = "rocblas_ztpsv_batched";

    template <rocblas_int BLOCK, typename T>
    rocblas_status rocblas_tpsv_batched_impl(rocblas_handle    handle,
                                             rocblas_fill      uplo,
                                             rocblas_operation transA,
                                             rocblas_diagonal  diag,
                                             rocblas_int       n,
                                             const T* const    AP[],
                                             T* const          x[],
                                             rocblas_int       incx,
                                             rocblas_int       batch_count)
    {
        if(!handle)
            return rocblas_status_invalid_handle;

        RETURN_ZERO_DEVICE_MEMORY_SIZE_IF_QUERIED(handle);

        auto layer_mode = handle->layer_mode;
        if(layer_mode & rocblas_layer_mode_log_trace)
            log_trace(handle,
                      rocblas_tpsv_batched_name<T>,
                      uplo,
                      transA,
                      diag,
                      n,
                      AP,
                      x,
                      incx,
                      batch_count);

        if(layer_mode & (rocblas_layer_mode_log_bench | rocblas_layer_mode_log_profile))
        {
            auto uplo_letter   = rocblas_fill_letter(uplo);
            auto transA_letter = rocblas_transpose_letter(transA);
            auto diag_letter   = rocblas_diag_letter(diag);

            if(layer_mode & rocblas_layer_mode_log_bench)
            {
                if(handle->pointer_mode == rocblas_pointer_mode_host)
                    log_bench(handle,
                              "./rocblas-bench -f tpsv_batched -r",
                              rocblas_precision_string<T>,
                              "--uplo",
                              uplo_letter,
                              "--transposeA",
                              transA_letter,
                              "--diag",
                              diag_letter,
                              "-n",
                              n,
                              "--incx",
                              incx,
                              "--batch_count",
                              batch_count);
            }

            if(layer_mode & rocblas_layer_mode_log_profile)
                log_profile(handle,
                            rocblas_tpsv_batched_name<T>,
                            "uplo",
                            uplo_letter,
                            "transA",
                            transA_letter,
                            "diag",
                            diag_letter,
                            "N",
                            n,
                            "incx",
                            incx,
                            "batch_count",
                            batch_count);
        }

        rocblas_status arg_status
            = rocblas_tpsv_arg_check(handle, uplo, transA, diag, n, AP, x, incx, batch_count);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        auto check_numerics = handle->check_numerics;
        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status tpsv_check_numerics_status
                = rocblas_tpsv_check_numerics(rocblas_tpsv_batched_name<T>,
                                              handle,
                                              n,
                                              AP,
                                              0,
                                              0,
                                              x,
                                              0,
                                              incx,
                                              0,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(tpsv_check_numerics_status != rocblas_status_success)
                return tpsv_check_numerics_status;
        }

        rocblas_status status = rocblas_tpsv_template<BLOCK>(
            handle, uplo, transA, diag, n, AP, 0, 0, x, 0, incx, 0, batch_count);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status tpsv_check_numerics_status
                = rocblas_tpsv_check_numerics(rocblas_tpsv_batched_name<T>,
                                              handle,
                                              n,
                                              AP,
                                              0,
                                              0,
                                              x,
                                              0,
                                              incx,
                                              0,
                                              1,
                                              check_numerics,
                                              is_input);
            if(tpsv_check_numerics_status != rocblas_status_success)
                return tpsv_check_numerics_status;
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

rocblas_status rocblas_stpsv_batched(rocblas_handle     handle,
                                     rocblas_fill       uplo,
                                     rocblas_operation  transA,
                                     rocblas_diagonal   diag,
                                     rocblas_int        n,
                                     const float* const AP[],
                                     float* const       x[],
                                     rocblas_int        incx,
                                     rocblas_int        batch_count)
try
{
    return rocblas_tpsv_batched_impl<STPSV_BLOCK>(
        handle, uplo, transA, diag, n, AP, x, incx, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dtpsv_batched(rocblas_handle      handle,
                                     rocblas_fill        uplo,
                                     rocblas_operation   transA,
                                     rocblas_diagonal    diag,
                                     rocblas_int         n,
                                     const double* const AP[],
                                     double* const       x[],
                                     rocblas_int         incx,
                                     rocblas_int         batch_count)
try
{
    return rocblas_tpsv_batched_impl<DTPSV_BLOCK>(
        handle, uplo, transA, diag, n, AP, x, incx, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_ctpsv_batched(rocblas_handle                     handle,
                                     rocblas_fill                       uplo,
                                     rocblas_operation                  transA,
                                     rocblas_diagonal                   diag,
                                     rocblas_int                        n,
                                     const rocblas_float_complex* const AP[],
                                     rocblas_float_complex* const       x[],
                                     rocblas_int                        incx,

                                     rocblas_int batch_count)
try
{
    return rocblas_tpsv_batched_impl<STPSV_BLOCK>(
        handle, uplo, transA, diag, n, AP, x, incx, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_ztpsv_batched(rocblas_handle                      handle,
                                     rocblas_fill                        uplo,
                                     rocblas_operation                   transA,
                                     rocblas_diagonal                    diag,
                                     rocblas_int                         n,
                                     const rocblas_double_complex* const AP[],
                                     rocblas_double_complex* const       x[],
                                     rocblas_int                         incx,
                                     rocblas_int                         batch_count)
try
{
    return rocblas_tpsv_batched_impl<DTPSV_BLOCK>(
        handle, uplo, transA, diag, n, AP, x, incx, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

} // extern "C"
