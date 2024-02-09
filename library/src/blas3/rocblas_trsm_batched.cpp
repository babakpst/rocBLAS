/* ************************************************************************
 * Copyright (C) 2019-2023 Advanced Micro Devices, Inc. All rights reserved.
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
#include "rocblas_trmm.hpp"
#include "rocblas_trsm.hpp"
#include "trtri_trsm.hpp"
#include "utility.hpp"

// Shared memory usuage is (128/2)^2 * sizeof(float) = 32K. LDS is 64K per CU. Theoretically
// you can use all 64K, but in practice no.
// constexpr rocblas_int STRSM_BLOCK = 128;
// constexpr rocblas_int DTRSM_BLOCK = 128;

namespace
{
    template <typename>
    constexpr char rocblas_trsm_name[] = "unknown";
    template <>
    constexpr char rocblas_trsm_name<float>[] = "rocblas_batched_strsm";
    template <>
    constexpr char rocblas_trsm_name<double>[] = "rocblas_batched_dtrsm";

    /* ============================================================================================ */

    template <typename T>
    rocblas_status rocblas_trsm_batched_ex_impl(rocblas_handle    handle,
                                                rocblas_side      side,
                                                rocblas_fill      uplo,
                                                rocblas_operation transA,
                                                rocblas_diagonal  diag,
                                                rocblas_int       m,
                                                rocblas_int       n,
                                                const T*          alpha,
                                                const T* const    A[],
                                                rocblas_int       lda,
                                                T* const          B[],
                                                rocblas_int       ldb,
                                                rocblas_int       batch_count,
                                                const T* const    supplied_invA[]    = nullptr,
                                                rocblas_int       supplied_invA_size = 0)
    {
        if(!handle)
            return rocblas_status_invalid_handle;

        auto check_numerics = handle->check_numerics;
        /////////////
        // LOGGING //
        /////////////
        if(!handle->is_device_memory_size_query())
        {
            auto layer_mode = handle->layer_mode;
            if(layer_mode
               & (rocblas_layer_mode_log_trace | rocblas_layer_mode_log_bench
                  | rocblas_layer_mode_log_profile))
            {
                auto side_letter   = rocblas_side_letter(side);
                auto uplo_letter   = rocblas_fill_letter(uplo);
                auto transA_letter = rocblas_transpose_letter(transA);
                auto diag_letter   = rocblas_diag_letter(diag);

                if(layer_mode & rocblas_layer_mode_log_trace)
                    log_trace(handle,
                              rocblas_trsm_name<T>,
                              side,
                              uplo,
                              transA,
                              diag,
                              m,
                              n,
                              LOG_TRACE_SCALAR_VALUE(handle, alpha),
                              A,
                              lda,
                              B,
                              ldb,
                              batch_count);

                if(layer_mode & rocblas_layer_mode_log_bench)
                {
                    log_bench(handle,
                              "./rocblas-bench -f trsm_batched -r",
                              rocblas_precision_string<T>,
                              "--side",
                              side_letter,
                              "--uplo",
                              uplo_letter,
                              "--transposeA",
                              transA_letter,
                              "--diag",
                              diag_letter,
                              "-m",
                              m,
                              "-n",
                              n,
                              LOG_BENCH_SCALAR_VALUE(handle, alpha),
                              "--lda",
                              lda,
                              "--ldb",
                              ldb,
                              "--batch_count",
                              batch_count);
                }

                if(layer_mode & rocblas_layer_mode_log_profile)
                {
                    log_profile(handle,
                                rocblas_trsm_name<T>,
                                "side",
                                side_letter,
                                "uplo",
                                uplo_letter,
                                "transA",
                                transA_letter,
                                "diag",
                                diag_letter,
                                "m",
                                m,
                                "n",
                                n,
                                "lda",
                                lda,
                                "ldb",
                                ldb,
                                "batch_count",
                                batch_count);
                }
            }
        }

        rocblas_status arg_status = rocblas_trsm_arg_check(
            handle, side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb, batch_count);

        if(arg_status != rocblas_status_continue)
            return arg_status;

        if(rocblas_pointer_mode_host == handle->pointer_mode && 0 == *alpha)
        {
            set_block_unit<T>(handle, m, n, B, ldb, 0, batch_count, 0);
            return rocblas_status_success;
        }

        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status trsm_check_numerics_status
                = rocblas_trmm_check_numerics(rocblas_trsm_name<T>,
                                              handle,
                                              side,
                                              uplo,
                                              transA,
                                              m,
                                              n,
                                              A,
                                              lda,
                                              0,
                                              B,
                                              ldb,
                                              0,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(trsm_check_numerics_status != rocblas_status_success)
                return trsm_check_numerics_status;
        }

        rocblas_status status = rocblas_status_success;
        //////////////////////
        // MEMORY MANAGEMENT//
        //////////////////////
        //kernel function is enclosed inside the brackets so that the handle device memory used by the kernel is released after the computation.
        {
            // Proxy object holds the allocation. It must stay alive as long as mem_* pointers below are alive.
            auto  w_mem = handle->device_malloc(0);
            void* w_mem_x_temp;
            void* w_mem_x_temp_arr;
            void* w_mem_invA;
            void* w_mem_invA_arr;

            rocblas_status perf_status
                = rocblas_internal_trsm_template_mem<true, T>(handle,
                                                              side,
                                                              transA,
                                                              m,
                                                              n,
                                                              batch_count,
                                                              w_mem,
                                                              w_mem_x_temp,
                                                              w_mem_x_temp_arr,
                                                              w_mem_invA,
                                                              w_mem_invA_arr,
                                                              supplied_invA,
                                                              supplied_invA_size);

            if(perf_status != rocblas_status_success && perf_status != rocblas_status_perf_degraded)
                return perf_status;

            bool optimal_mem = perf_status == rocblas_status_success;

            status = rocblas_internal_trsm_batched_template(handle,
                                                            side,
                                                            uplo,
                                                            transA,
                                                            diag,
                                                            m,
                                                            n,
                                                            alpha,
                                                            A,
                                                            0,
                                                            lda,
                                                            0,
                                                            B,
                                                            0,
                                                            ldb,
                                                            0,
                                                            batch_count,
                                                            optimal_mem,
                                                            w_mem_x_temp,
                                                            w_mem_x_temp_arr,
                                                            w_mem_invA,
                                                            w_mem_invA_arr,
                                                            supplied_invA,
                                                            supplied_invA_size,
                                                            0,
                                                            0);
            status = (status != rocblas_status_success) ? status : perf_status;
            if(status != rocblas_status_success)
                return status;
        }

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status trsm_check_numerics_status
                = rocblas_trmm_check_numerics(rocblas_trsm_name<T>,
                                              handle,
                                              side,
                                              uplo,
                                              transA,
                                              m,
                                              n,
                                              A,
                                              lda,
                                              0,
                                              B,
                                              ldb,
                                              0,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(trsm_check_numerics_status != rocblas_status_success)
                return trsm_check_numerics_status;
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

rocblas_status rocblas_strsm_batched(rocblas_handle     handle,
                                     rocblas_side       side,
                                     rocblas_fill       uplo,
                                     rocblas_operation  transA,
                                     rocblas_diagonal   diag,
                                     rocblas_int        m,
                                     rocblas_int        n,
                                     const float*       alpha,
                                     const float* const A[],
                                     rocblas_int        lda,
                                     float* const       B[],
                                     rocblas_int        ldb,
                                     rocblas_int        batch_count)
try
{
    return rocblas_trsm_batched_ex_impl(
        handle, side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_dtrsm_batched(rocblas_handle      handle,
                                     rocblas_side        side,
                                     rocblas_fill        uplo,
                                     rocblas_operation   transA,
                                     rocblas_diagonal    diag,
                                     rocblas_int         m,
                                     rocblas_int         n,
                                     const double*       alpha,
                                     const double* const A[],
                                     rocblas_int         lda,
                                     double* const       B[],
                                     rocblas_int         ldb,
                                     rocblas_int         batch_count)
try
{
    return rocblas_trsm_batched_ex_impl(
        handle, side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_ctrsm_batched(rocblas_handle                     handle,
                                     rocblas_side                       side,
                                     rocblas_fill                       uplo,
                                     rocblas_operation                  transA,
                                     rocblas_diagonal                   diag,
                                     rocblas_int                        m,
                                     rocblas_int                        n,
                                     const rocblas_float_complex*       alpha,
                                     const rocblas_float_complex* const A[],
                                     rocblas_int                        lda,
                                     rocblas_float_complex* const       B[],
                                     rocblas_int                        ldb,
                                     rocblas_int                        batch_count)
try
{
    return rocblas_trsm_batched_ex_impl(
        handle, side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_ztrsm_batched(rocblas_handle                      handle,
                                     rocblas_side                        side,
                                     rocblas_fill                        uplo,
                                     rocblas_operation                   transA,
                                     rocblas_diagonal                    diag,
                                     rocblas_int                         m,
                                     rocblas_int                         n,
                                     const rocblas_double_complex*       alpha,
                                     const rocblas_double_complex* const A[],
                                     rocblas_int                         lda,
                                     rocblas_double_complex* const       B[],
                                     rocblas_int                         ldb,
                                     rocblas_int                         batch_count)
try
{
    return rocblas_trsm_batched_ex_impl(
        handle, side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb, batch_count);
}
catch(...)
{
    return exception_to_rocblas_status();
}

rocblas_status rocblas_trsm_batched_ex(rocblas_handle    handle,
                                       rocblas_side      side,
                                       rocblas_fill      uplo,
                                       rocblas_operation transA,
                                       rocblas_diagonal  diag,
                                       rocblas_int       m,
                                       rocblas_int       n,
                                       const void*       alpha,
                                       const void*       A,
                                       rocblas_int       lda,
                                       void*             B,
                                       rocblas_int       ldb,
                                       rocblas_int       batch_count,
                                       const void*       invA,
                                       rocblas_int       invA_size,
                                       rocblas_datatype  compute_type)
try
{
    switch(compute_type)
    {
    case rocblas_datatype_f64_r:
        return rocblas_trsm_batched_ex_impl(handle,
                                            side,
                                            uplo,
                                            transA,
                                            diag,
                                            m,
                                            n,
                                            (double*)(alpha),
                                            (const double* const*)(A),
                                            lda,
                                            (double* const*)(B),
                                            ldb,
                                            batch_count,
                                            (const double* const*)(invA),
                                            invA_size);

    case rocblas_datatype_f32_r:
        return rocblas_trsm_batched_ex_impl(handle,
                                            side,
                                            uplo,
                                            transA,
                                            diag,
                                            m,
                                            n,
                                            (float*)(alpha),
                                            (const float* const*)(A),
                                            lda,
                                            (float* const*)(B),
                                            ldb,
                                            batch_count,
                                            (const float* const*)(invA),
                                            invA_size);
    case rocblas_datatype_f64_c:
        return rocblas_trsm_batched_ex_impl(handle,
                                            side,
                                            uplo,
                                            transA,
                                            diag,
                                            m,
                                            n,
                                            (rocblas_double_complex*)(alpha),
                                            (const rocblas_double_complex* const*)(A),
                                            lda,
                                            (rocblas_double_complex* const*)(B),
                                            ldb,
                                            batch_count,
                                            (const rocblas_double_complex* const*)(invA),
                                            invA_size);

    case rocblas_datatype_f32_c:
        return rocblas_trsm_batched_ex_impl(handle,
                                            side,
                                            uplo,
                                            transA,
                                            diag,
                                            m,
                                            n,
                                            (rocblas_float_complex*)(alpha),
                                            (const rocblas_float_complex* const*)(A),
                                            lda,
                                            (rocblas_float_complex* const*)(B),
                                            ldb,
                                            batch_count,
                                            (const rocblas_float_complex* const*)(invA),
                                            invA_size);

    default:
        return rocblas_status_not_implemented;
    }
}
catch(...)
{
    return exception_to_rocblas_status();
}

} // extern "C"
