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
#include "rocblas_block_sizes.h"
#include "rocblas_tpmv.hpp"
#include "utility.hpp"

namespace
{

    template <typename>
    constexpr char rocblas_tpmv_strided_batched_name[] = "unknown";
    template <>
    constexpr char rocblas_tpmv_strided_batched_name<float>[] = "rocblas_stpmv_strided_batched";
    template <>
    constexpr char rocblas_tpmv_strided_batched_name<double>[] = "rocblas_dtpmv_strided_batched";
    template <>
    constexpr char rocblas_tpmv_strided_batched_name<rocblas_float_complex>[]
        = "rocblas_ctpmv_strided_batched";
    template <>
    constexpr char rocblas_tpmv_strided_batched_name<rocblas_double_complex>[]
        = "rocblas_ztpmv_strided_batched";

    template <typename T>
    rocblas_status rocblas_tpmv_strided_batched_impl(rocblas_handle    handle,
                                                     rocblas_fill      uplo,
                                                     rocblas_operation transa,
                                                     rocblas_diagonal  diag,
                                                     rocblas_int       m,
                                                     const T*          a,
                                                     rocblas_stride    stridea,
                                                     T*                x,
                                                     rocblas_int       incx,
                                                     rocblas_stride    stridex,
                                                     rocblas_int       batch_count)
    {
        if(!handle)
            return rocblas_status_invalid_handle;

        auto check_numerics = handle->check_numerics;

        if(!handle->is_device_memory_size_query())
        {
            auto layer_mode = handle->layer_mode;
            if(layer_mode
               & (rocblas_layer_mode_log_trace | rocblas_layer_mode_log_bench
                  | rocblas_layer_mode_log_profile))
            {
                auto uplo_letter   = rocblas_fill_letter(uplo);
                auto transa_letter = rocblas_transpose_letter(transa);
                auto diag_letter   = rocblas_diag_letter(diag);
                if(layer_mode & rocblas_layer_mode_log_trace)
                {
                    log_trace(handle,
                              rocblas_tpmv_strided_batched_name<T>,
                              uplo,
                              transa,
                              diag,
                              m,
                              a,
                              x,
                              incx,
                              stridea,
                              incx,
                              stridex,
                              batch_count);
                }

                if(layer_mode & rocblas_layer_mode_log_bench)
                {
                    log_bench(handle,
                              "./rocblas-bench",
                              "-f",
                              "tpmv_strided_batched",
                              "-r",
                              rocblas_precision_string<T>,
                              "--uplo",
                              uplo_letter,
                              "--transposeA",
                              transa_letter,
                              "--diag",
                              diag_letter,
                              "-m",
                              m,
                              "--stride_a",
                              stridea,
                              "--incx",
                              incx,
                              "--stride_x",
                              stridex,
                              "--batch_count",
                              batch_count);
                }

                if(layer_mode & rocblas_layer_mode_log_profile)
                {
                    log_profile(handle,
                                rocblas_tpmv_strided_batched_name<T>,
                                "uplo",
                                uplo_letter,
                                "transA",
                                transa_letter,
                                "diag",
                                diag_letter,
                                "M",
                                m,
                                "stride_a",
                                stridea,
                                "incx",
                                incx,
                                "stride_x",
                                stridex,
                                "batch_count",
                                batch_count);
                }
            }
        }

        size_t         dev_bytes;
        rocblas_status arg_status = rocblas_tpmv_arg_check<T>(
            handle, uplo, transa, diag, m, a, x, incx, batch_count, dev_bytes);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        auto w_mem = handle->device_malloc(dev_bytes);
        if(!w_mem)
            return rocblas_status_memory_error;

        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status tpmv_check_numerics_status
                = rocblas_tpmv_check_numerics(rocblas_tpmv_strided_batched_name<T>,
                                              handle,
                                              m,
                                              a,
                                              0,
                                              stridea,
                                              x,
                                              0,
                                              incx,
                                              stridex,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(tpmv_check_numerics_status != rocblas_status_success)
                return tpmv_check_numerics_status;
        }

        rocblas_stride                  stridew = m;
        static constexpr rocblas_int    NB      = ROCBLAS_TPMV_NB;
        static constexpr rocblas_stride offseta = 0;
        static constexpr rocblas_stride offsetx = 0;
        rocblas_status                  status  = rocblas_tpmv_template<NB>(handle,
                                                          uplo,
                                                          transa,
                                                          diag,
                                                          m,
                                                          a,
                                                          offseta,
                                                          stridea,
                                                          x,
                                                          offsetx,
                                                          incx,
                                                          stridex,
                                                          (T*)w_mem,
                                                          stridew,
                                                          batch_count);

        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status tpmv_check_numerics_status
                = rocblas_tpmv_check_numerics(rocblas_tpmv_strided_batched_name<T>,
                                              handle,
                                              m,
                                              a,
                                              0,
                                              stridea,
                                              x,
                                              0,
                                              incx,
                                              stridex,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(tpmv_check_numerics_status != rocblas_status_success)
                return tpmv_check_numerics_status;
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

#ifdef IMPL
#error IMPL ALREADY DEFINED
#endif

#define IMPL(routine_name_, T_)                                                        \
    rocblas_status routine_name_(rocblas_handle    handle,                             \
                                 rocblas_fill      uplo,                               \
                                 rocblas_operation transA,                             \
                                 rocblas_diagonal  diag,                               \
                                 rocblas_int       m,                                  \
                                 const T_*         A,                                  \
                                 rocblas_stride    stridea,                            \
                                 T_*               x,                                  \
                                 rocblas_int       incx,                               \
                                 rocblas_stride    stridex,                            \
                                 rocblas_int       batch_count)                        \
    try                                                                                \
    {                                                                                  \
        return rocblas_tpmv_strided_batched_impl(                                      \
            handle, uplo, transA, diag, m, A, stridea, x, incx, stridex, batch_count); \
    }                                                                                  \
    catch(...)                                                                         \
    {                                                                                  \
        return exception_to_rocblas_status();                                          \
    }

IMPL(rocblas_stpmv_strided_batched, float);
IMPL(rocblas_dtpmv_strided_batched, double);
IMPL(rocblas_ctpmv_strided_batched, rocblas_float_complex);
IMPL(rocblas_ztpmv_strided_batched, rocblas_double_complex);

#undef IMPL

} // extern "C"
