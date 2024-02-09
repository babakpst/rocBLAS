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
#include "logging.hpp"
#include "rocblas_block_sizes.h"
#include "rocblas_syrk_herk.hpp"
#include "utility.hpp"

namespace
{
    template <typename>
    constexpr char rocblas_herk_name[] = "unknown";
    template <>
    constexpr char rocblas_herk_name<rocblas_float_complex>[] = "rocblas_cherk";
    template <>
    constexpr char rocblas_herk_name<rocblas_double_complex>[] = "rocblas_zherk";

    template <typename T>
    rocblas_status rocblas_herk_impl(rocblas_handle    handle,
                                     rocblas_fill      uplo,
                                     rocblas_operation transA,
                                     rocblas_int       n,
                                     rocblas_int       k,
                                     const real_t<T>*  alpha,
                                     const T*          A,
                                     rocblas_int       lda,
                                     const real_t<T>*  beta,
                                     T*                C,
                                     rocblas_int       ldc)
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
            auto uplo_letter   = rocblas_fill_letter(uplo);
            auto transA_letter = rocblas_transpose_letter(transA);

            if(layer_mode & rocblas_layer_mode_log_trace)
                log_trace(handle,
                          rocblas_herk_name<T>,
                          uplo,
                          transA,
                          n,
                          k,
                          LOG_TRACE_SCALAR_VALUE(handle, alpha),
                          A,
                          lda,
                          LOG_TRACE_SCALAR_VALUE(handle, beta),
                          C,
                          ldc);

            if(layer_mode & rocblas_layer_mode_log_bench)
                log_bench(handle,
                          "./rocblas-bench -f herk -r",
                          rocblas_precision_string<T>,
                          "--uplo",
                          uplo_letter,
                          "--transposeA",
                          transA_letter,
                          "-n",
                          n,
                          "-k",
                          k,
                          LOG_BENCH_SCALAR_VALUE(handle, alpha),
                          "--lda",
                          lda,
                          LOG_BENCH_SCALAR_VALUE(handle, beta),
                          "--ldc",
                          ldc);

            if(layer_mode & rocblas_layer_mode_log_profile)
                log_profile(handle,
                            rocblas_herk_name<T>,
                            "uplo",
                            uplo_letter,
                            "transA",
                            transA_letter,
                            "N",
                            n,
                            "K",
                            k,
                            "lda",
                            lda,
                            "ldc",
                            ldc);
        }

        static constexpr rocblas_stride offset_C = 0, offset_A = 0;
        static constexpr rocblas_int    batch_count = 1;
        static constexpr rocblas_stride stride_C = 0, stride_A = 0;

        rocblas_status arg_status = rocblas_herk_arg_check(handle,
                                                           uplo,
                                                           transA,
                                                           n,
                                                           k,
                                                           alpha,
                                                           A,
                                                           offset_A,
                                                           lda,
                                                           stride_A,
                                                           beta,
                                                           C,
                                                           offset_C,
                                                           ldc,
                                                           stride_C,
                                                           batch_count);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        static constexpr bool Hermetian = true;
        static constexpr bool BATCHED   = false;
        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status herk_check_numerics_status
                = rocblas_herk_syrk_check_numerics<Hermetian>(rocblas_herk_name<T>,
                                                              handle,
                                                              uplo,
                                                              transA,
                                                              n,
                                                              k,
                                                              A,
                                                              lda,
                                                              stride_A,
                                                              C,
                                                              ldc,
                                                              stride_C,
                                                              batch_count,
                                                              check_numerics,
                                                              is_input);

            if(herk_check_numerics_status != rocblas_status_success)
                return herk_check_numerics_status;
        }

        rocblas_status status = rocblas_status_success;
        status                = rocblas_internal_herk_template(handle,
                                                uplo,
                                                transA,
                                                n,
                                                k,
                                                alpha,
                                                A,
                                                offset_A,
                                                lda,
                                                stride_A,
                                                beta,
                                                C,
                                                offset_C,
                                                ldc,
                                                stride_C,
                                                batch_count);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status herk_check_numerics_status
                = rocblas_herk_syrk_check_numerics<Hermetian>(rocblas_herk_name<T>,
                                                              handle,
                                                              uplo,
                                                              transA,
                                                              n,
                                                              k,
                                                              A,
                                                              lda,
                                                              stride_A,
                                                              C,
                                                              ldc,
                                                              stride_C,
                                                              batch_count,
                                                              check_numerics,
                                                              is_input);

            if(herk_check_numerics_status != rocblas_status_success)
                return herk_check_numerics_status;
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

#ifdef IMPL
#error IMPL ALREADY DEFINED
#endif

#define IMPL(routine_name_, T_)                                                            \
    rocblas_status routine_name_(rocblas_handle    handle,                                 \
                                 rocblas_fill      uplo,                                   \
                                 rocblas_operation transA,                                 \
                                 rocblas_int       n,                                      \
                                 rocblas_int       k,                                      \
                                 const real_t<T_>* alpha,                                  \
                                 const T_*         A,                                      \
                                 rocblas_int       lda,                                    \
                                 const real_t<T_>* beta,                                   \
                                 T_*               C,                                      \
                                 rocblas_int       ldc)                                    \
    try                                                                                    \
    {                                                                                      \
        return rocblas_herk_impl(handle, uplo, transA, n, k, alpha, A, lda, beta, C, ldc); \
    }                                                                                      \
    catch(...)                                                                             \
    {                                                                                      \
        return exception_to_rocblas_status();                                              \
    }

IMPL(rocblas_cherk, rocblas_float_complex);
IMPL(rocblas_zherk, rocblas_double_complex);

#undef IMPL

} // extern "C"
