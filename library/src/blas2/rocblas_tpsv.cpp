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
#include "rocblas_tpsv.hpp"
#include "handle.hpp"
#include "logging.hpp"
#include "rocblas.h"
#include "rocblas_gemv.hpp"
#include "utility.hpp"

namespace
{
    constexpr rocblas_int STPSV_BLOCK = 512;
    constexpr rocblas_int DTPSV_BLOCK = 512;

    template <typename>
    constexpr char rocblas_tpsv_name[] = "unknown";
    template <>
    constexpr char rocblas_tpsv_name<float>[] = "rocblas_stpsv";
    template <>
    constexpr char rocblas_tpsv_name<double>[] = "rocblas_dtpsv";
    template <>
    constexpr char rocblas_tpsv_name<rocblas_float_complex>[] = "rocblas_ctpsv";
    template <>
    constexpr char rocblas_tpsv_name<rocblas_double_complex>[] = "rocblas_ztpsv";

    template <rocblas_int BLOCK, typename T>
    rocblas_status rocblas_tpsv_impl(rocblas_handle    handle,
                                     rocblas_fill      uplo,
                                     rocblas_operation transA,
                                     rocblas_diagonal  diag,
                                     rocblas_int       n,
                                     const T*          AP,
                                     T*                x,
                                     rocblas_int       incx)
    {
        if(!handle)
            return rocblas_status_invalid_handle;

        RETURN_ZERO_DEVICE_MEMORY_SIZE_IF_QUERIED(handle);

        auto layer_mode = handle->layer_mode;
        if(layer_mode & rocblas_layer_mode_log_trace)
            log_trace(handle, rocblas_tpsv_name<T>, uplo, transA, diag, n, AP, x, incx);

        if(layer_mode & (rocblas_layer_mode_log_bench | rocblas_layer_mode_log_profile))
        {
            auto uplo_letter   = rocblas_fill_letter(uplo);
            auto transA_letter = rocblas_transpose_letter(transA);
            auto diag_letter   = rocblas_diag_letter(diag);

            if(layer_mode & rocblas_layer_mode_log_bench)
            {
                if(handle->pointer_mode == rocblas_pointer_mode_host)
                    log_bench(handle,
                              "./rocblas-bench -f tpsv -r",
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
                              incx);
            }

            if(layer_mode & rocblas_layer_mode_log_profile)
                log_profile(handle,
                            rocblas_tpsv_name<T>,
                            "uplo",
                            uplo_letter,
                            "transA",
                            transA_letter,
                            "diag",
                            diag_letter,
                            "N",
                            n,
                            "incx",
                            incx);
        }

        rocblas_status arg_status
            = rocblas_tpsv_arg_check(handle, uplo, transA, diag, n, AP, x, incx, 1);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        auto check_numerics = handle->check_numerics;
        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status tpsv_check_numerics_status
                = rocblas_tpsv_check_numerics(rocblas_tpsv_name<T>,
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

        rocblas_status status = rocblas_tpsv_template<BLOCK>(
            handle, uplo, transA, diag, n, AP, 0, 0, x, 0, incx, 0, 1);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status tpsv_check_numerics_status
                = rocblas_tpsv_check_numerics(rocblas_tpsv_name<T>,
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

rocblas_status rocblas_stpsv(rocblas_handle    handle,
                             rocblas_fill      uplo,
                             rocblas_operation transA,
                             rocblas_diagonal  diag,
                             rocblas_int       n,
                             const float*      AP,
                             float*            x,
                             rocblas_int       incx)
try
{
    return rocblas_tpsv_impl<STPSV_BLOCK>(handle, uplo, transA, diag, n, AP, x, incx);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dtpsv(rocblas_handle    handle,
                             rocblas_fill      uplo,
                             rocblas_operation transA,
                             rocblas_diagonal  diag,
                             rocblas_int       n,
                             const double*     AP,
                             double*           x,
                             rocblas_int       incx)
try
{
    return rocblas_tpsv_impl<DTPSV_BLOCK>(handle, uplo, transA, diag, n, AP, x, incx);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_ctpsv(rocblas_handle               handle,
                             rocblas_fill                 uplo,
                             rocblas_operation            transA,
                             rocblas_diagonal             diag,
                             rocblas_int                  n,
                             const rocblas_float_complex* AP,
                             rocblas_float_complex*       x,
                             rocblas_int                  incx)
try
{
    return rocblas_tpsv_impl<STPSV_BLOCK>(handle, uplo, transA, diag, n, AP, x, incx);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_ztpsv(rocblas_handle                handle,
                             rocblas_fill                  uplo,
                             rocblas_operation             transA,
                             rocblas_diagonal              diag,
                             rocblas_int                   n,
                             const rocblas_double_complex* AP,
                             rocblas_double_complex*       x,
                             rocblas_int                   incx)
try
{
    // return rocblas_status_success;
    return rocblas_tpsv_impl<DTPSV_BLOCK>(handle, uplo, transA, diag, n, AP, x, incx);
}
catch(...)
{
    return exception_to_rocblas_status();
}

} // extern "C"
