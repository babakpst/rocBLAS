/* ************************************************************************
 * Copyright (C) 2018-2023 Advanced Micro Devices, Inc. All rights reserved.
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

#pragma once

#include "cblas_interface.hpp"
#include "flops.hpp"
#include "norm.hpp"
#include "rocblas.hpp"
#include "rocblas_datatype2string.hpp"
#include "rocblas_init.hpp"
#include "rocblas_math.hpp"
#include "rocblas_matrix.hpp"
#include "rocblas_random.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "unit.hpp"
#include "utility.hpp"

#define ERROR_EPS_MULTIPLIER 40
#define RESIDUAL_EPS_MULTIPLIER 40

template <typename T>
void testing_trsm_batched_bad_arg(const Arguments& arg)
{
    auto rocblas_trsm_batched_fn
        = arg.fortran ? rocblas_trsm_batched<T, true> : rocblas_trsm_batched<T, false>;

    for(auto pointer_mode : {rocblas_pointer_mode_host, rocblas_pointer_mode_device})
    {
        rocblas_local_handle handle{arg};
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, pointer_mode));

        const rocblas_int M           = 100;
        const rocblas_int N           = 100;
        const rocblas_int lda         = 100;
        const rocblas_int ldb         = 100;
        const rocblas_int batch_count = 2;

        device_vector<T> alpha_d(1), zero_d(1);

        const T alpha_h(1), zero_h(0);

        const T* alpha = &alpha_h;
        const T* zero  = &zero_h;

        if(pointer_mode == rocblas_pointer_mode_device)
        {
            CHECK_HIP_ERROR(hipMemcpy(alpha_d, alpha, sizeof(*alpha), hipMemcpyHostToDevice));
            alpha = alpha_d;
            CHECK_HIP_ERROR(hipMemcpy(zero_d, zero, sizeof(*zero), hipMemcpyHostToDevice));
            zero = zero_d;
        }

        const rocblas_side      side   = rocblas_side_left;
        const rocblas_fill      uplo   = rocblas_fill_upper;
        const rocblas_operation transA = rocblas_operation_none;
        const rocblas_diagonal  diag   = rocblas_diagonal_non_unit;

        rocblas_int K = side == rocblas_side_left ? M : N;

        // Allocate device memory
        device_batch_matrix<T> dA(K, K, lda, batch_count);
        device_batch_matrix<T> dB(M, N, ldb, batch_count);

        // Check device memory allocation
        CHECK_DEVICE_ALLOCATION(dA.memcheck());
        CHECK_DEVICE_ALLOCATION(dB.memcheck());

        // check for invalid enum
        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      rocblas_side_both,
                                                      uplo,
                                                      transA,
                                                      diag,
                                                      M,
                                                      N,
                                                      alpha,
                                                      dA,
                                                      lda,
                                                      dB,
                                                      ldb,
                                                      batch_count),
                              rocblas_status_invalid_value);

        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      side,
                                                      (rocblas_fill)rocblas_side_both,
                                                      transA,
                                                      diag,
                                                      M,
                                                      N,
                                                      alpha,
                                                      dA,
                                                      lda,
                                                      dB,
                                                      ldb,
                                                      batch_count),
                              rocblas_status_invalid_value);

        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      side,
                                                      uplo,
                                                      (rocblas_operation)rocblas_side_both,
                                                      diag,
                                                      M,
                                                      N,
                                                      alpha,
                                                      dA,
                                                      lda,
                                                      dB,
                                                      ldb,
                                                      batch_count),
                              rocblas_status_invalid_value);

        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      side,
                                                      uplo,
                                                      transA,
                                                      (rocblas_diagonal)rocblas_side_both,
                                                      M,
                                                      N,
                                                      alpha,
                                                      dA,
                                                      lda,
                                                      dB,
                                                      ldb,
                                                      batch_count),
                              rocblas_status_invalid_value);

        // check for invalid size
        EXPECT_ROCBLAS_STATUS(
            rocblas_trsm_batched_fn(
                handle, side, uplo, transA, diag, -1, N, alpha, dA, lda, dB, ldb, batch_count),
            rocblas_status_invalid_size);

        EXPECT_ROCBLAS_STATUS(
            rocblas_trsm_batched_fn(
                handle, side, uplo, transA, diag, M, -1, alpha, dA, lda, dB, ldb, batch_count),
            rocblas_status_invalid_size);

        EXPECT_ROCBLAS_STATUS(
            rocblas_trsm_batched_fn(
                handle, side, uplo, transA, diag, M, N, alpha, dA, lda, dB, ldb, -1),
            rocblas_status_invalid_size);

        // check for invalid leading dimension
        EXPECT_ROCBLAS_STATUS(
            rocblas_trsm_batched_fn(
                handle, side, uplo, transA, diag, M, N, alpha, dA, lda, dB, M - 1, batch_count),
            rocblas_status_invalid_size);

        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      rocblas_side_left,
                                                      uplo,
                                                      transA,
                                                      diag,
                                                      M,
                                                      N,
                                                      alpha,
                                                      dA,
                                                      M - 1,
                                                      dB,
                                                      ldb,
                                                      batch_count),
                              rocblas_status_invalid_size);

        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      rocblas_side_right,
                                                      uplo,
                                                      transA,
                                                      diag,
                                                      M,
                                                      N,
                                                      alpha,
                                                      dA,
                                                      N - 1,
                                                      dB,
                                                      ldb,
                                                      batch_count),
                              rocblas_status_invalid_size);

        // check that nullpointer gives rocblas_status_invalid_handle or rocblas_status_invalid_pointer
        EXPECT_ROCBLAS_STATUS(
            rocblas_trsm_batched_fn(
                nullptr, side, uplo, transA, diag, M, N, alpha, dA, lda, dB, ldb, batch_count),
            rocblas_status_invalid_handle);

        EXPECT_ROCBLAS_STATUS(
            rocblas_trsm_batched_fn(
                handle, side, uplo, transA, diag, M, N, nullptr, dA, lda, dB, ldb, batch_count),
            rocblas_status_invalid_pointer);

        if(pointer_mode == rocblas_pointer_mode_host)
        {
            EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                          side,
                                                          uplo,
                                                          transA,
                                                          diag,
                                                          M,
                                                          N,
                                                          alpha,
                                                          nullptr,
                                                          lda,
                                                          dB,
                                                          ldb,
                                                          batch_count),
                                  rocblas_status_invalid_pointer);
        }

        EXPECT_ROCBLAS_STATUS(
            rocblas_trsm_batched_fn(
                handle, side, uplo, transA, diag, M, N, alpha, dA, lda, nullptr, ldb, batch_count),
            rocblas_status_invalid_pointer);

        // When batch_count==0, all pointers may be nullptr without error
        EXPECT_ROCBLAS_STATUS(
            rocblas_trsm_batched_fn(
                handle, side, uplo, transA, diag, M, N, nullptr, nullptr, lda, nullptr, ldb, 0),
            rocblas_status_success);

        // When M==0, all pointers may be nullptr without error
        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      side,
                                                      uplo,
                                                      transA,
                                                      diag,
                                                      0,
                                                      N,
                                                      nullptr,
                                                      nullptr,
                                                      lda,
                                                      nullptr,
                                                      ldb,
                                                      batch_count),
                              rocblas_status_success);

        // When N==0, all pointers may be nullptr without error
        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      side,
                                                      uplo,
                                                      transA,
                                                      diag,
                                                      M,
                                                      0,
                                                      nullptr,
                                                      nullptr,
                                                      lda,
                                                      nullptr,
                                                      ldb,
                                                      batch_count),
                              rocblas_status_success);

        // If alpha==0, then A can be nullptr without error
        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      side,
                                                      uplo,
                                                      transA,
                                                      diag,
                                                      M,
                                                      N,
                                                      zero,
                                                      nullptr,
                                                      lda,
                                                      dB.ptr_on_device(),
                                                      ldb,
                                                      batch_count),
                              rocblas_status_success);
    }
}

template <typename T>
void testing_trsm_batched(const Arguments& arg)
{
    auto rocblas_trsm_batched_fn
        = arg.fortran ? rocblas_trsm_batched<T, true> : rocblas_trsm_batched<T, false>;

    rocblas_int M           = arg.M;
    rocblas_int N           = arg.N;
    rocblas_int lda         = arg.lda;
    rocblas_int ldb         = arg.ldb;
    rocblas_int batch_count = arg.batch_count;

    char char_side   = arg.side;
    char char_uplo   = arg.uplo;
    char char_transA = arg.transA;
    char char_diag   = arg.diag;
    T    alpha_h     = arg.alpha;

    rocblas_side      side   = char2rocblas_side(char_side);
    rocblas_fill      uplo   = char2rocblas_fill(char_uplo);
    rocblas_operation transA = char2rocblas_operation(char_transA);
    rocblas_diagonal  diag   = char2rocblas_diagonal(char_diag);

    rocblas_int K = side == rocblas_side_left ? M : N;

    rocblas_local_handle handle{arg};
    // check here to prevent undefined memory allocation error
    bool invalid_size = M < 0 || N < 0 || lda < K || ldb < M || batch_count < 0;
    if(invalid_size || batch_count == 0)
    {
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        EXPECT_ROCBLAS_STATUS(rocblas_trsm_batched_fn(handle,
                                                      side,
                                                      uplo,
                                                      transA,
                                                      diag,
                                                      M,
                                                      N,
                                                      nullptr,
                                                      nullptr,
                                                      lda,
                                                      nullptr,
                                                      ldb,
                                                      batch_count),
                              invalid_size ? rocblas_status_invalid_size : rocblas_status_success);
        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hA), `d` is in GPU (device) memory (eg dA).
    // Allocate host memory
    host_batch_matrix<T> hA(K, K, lda, batch_count);
    host_batch_matrix<T> hAAT(K, K, lda, batch_count);
    host_batch_matrix<T> hB(M, N, ldb, batch_count);
    host_batch_matrix<T> hX(M, N, ldb, batch_count);
    host_batch_matrix<T> hXorB_1(M, N, ldb, batch_count);
    host_batch_matrix<T> hXorB_2(M, N, ldb, batch_count);
    host_batch_matrix<T> cpuXorB(M, N, ldb, batch_count);
    host_vector<T>       halpha(1);
    halpha[0] = alpha_h;

    // Check host memory allocation
    CHECK_HIP_ERROR(hA.memcheck());
    CHECK_HIP_ERROR(hAAT.memcheck());
    CHECK_HIP_ERROR(hB.memcheck());
    CHECK_HIP_ERROR(hX.memcheck());
    CHECK_HIP_ERROR(hXorB_1.memcheck());
    CHECK_HIP_ERROR(hXorB_2.memcheck());
    CHECK_HIP_ERROR(cpuXorB.memcheck());

    // Allocate device memory
    device_batch_matrix<T> dA(K, K, lda, batch_count);
    device_batch_matrix<T> dXorB(M, N, ldb, batch_count);
    device_vector<T>       alpha_d(1);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dA.memcheck());
    CHECK_DEVICE_ALLOCATION(dXorB.memcheck());
    CHECK_DEVICE_ALLOCATION(alpha_d.memcheck());

    // Initialize data on host memory
    rocblas_init_matrix(hA,
                        arg,
                        rocblas_client_never_set_nan,
                        rocblas_client_diagonally_dominant_triangular_matrix,
                        true);
    rocblas_init_matrix(
        hX, arg, rocblas_client_never_set_nan, rocblas_client_general_matrix, false, true);

    //  make hA unit diagonal if diag == rocblas_diagonal_unit
    if(diag == rocblas_diagonal_unit)
    {
        make_unit_diagonal(uplo, hA);
    }

    hB.copy_from(hX);

    for(int b = 0; b < batch_count; b++)
    {
        // Calculate hB = hA*hX
        cblas_trmm<T>(side, uplo, transA, diag, M, N, 1.0 / alpha_h, hA[b], lda, hB[b], ldb);
    }

    hXorB_1.copy_from(hB);
    hXorB_2.copy_from(hB);
    cpuXorB.copy_from(hB);

    CHECK_HIP_ERROR(dA.transfer_from(hA));
    CHECK_HIP_ERROR(dXorB.transfer_from(hXorB_1));

    double gpu_time_used, cpu_time_used;
    gpu_time_used = cpu_time_used  = 0.0;
    double error_eps_multiplier    = ERROR_EPS_MULTIPLIER;
    double residual_eps_multiplier = RESIDUAL_EPS_MULTIPLIER;
    double eps                     = std::numeric_limits<real_t<T>>::epsilon();
    double max_err_1               = 0.0;
    double max_err_2               = 0.0;

    if(!ROCBLAS_REALLOC_ON_DEMAND)
    {
        // Compute size
        CHECK_ROCBLAS_ERROR(rocblas_start_device_memory_size_query(handle));
        CHECK_ALLOC_QUERY(rocblas_trsm_batched_fn(handle,
                                                  side,
                                                  uplo,
                                                  transA,
                                                  diag,
                                                  M,
                                                  N,
                                                  &alpha_h,
                                                  dA.ptr_on_device(),
                                                  lda,
                                                  dXorB.ptr_on_device(),
                                                  ldb,
                                                  batch_count));
        size_t size;
        CHECK_ROCBLAS_ERROR(rocblas_stop_device_memory_size_query(handle, &size));

        // Allocate memory
        CHECK_ROCBLAS_ERROR(rocblas_set_device_memory_size(handle, size));
    }

    if(arg.unit_check || arg.norm_check)
    {
        // calculate dXorB <- A^(-1) B   rocblas_device_pointer_host
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        CHECK_HIP_ERROR(dXorB.transfer_from(hXorB_1));
        handle.pre_test(arg);
        CHECK_ROCBLAS_ERROR(rocblas_trsm_batched_fn(handle,
                                                    side,
                                                    uplo,
                                                    transA,
                                                    diag,
                                                    M,
                                                    N,
                                                    &alpha_h,
                                                    dA.ptr_on_device(),
                                                    lda,
                                                    dXorB.ptr_on_device(),
                                                    ldb,
                                                    batch_count));
        handle.post_test(arg);
        CHECK_HIP_ERROR(hXorB_1.transfer_from(dXorB));

        // calculate dXorB <- A^(-1) B   rocblas_device_pointer_device
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
        CHECK_HIP_ERROR(alpha_d.transfer_from(halpha));
        CHECK_HIP_ERROR(dXorB.transfer_from(hXorB_2));

        CHECK_ROCBLAS_ERROR(rocblas_trsm_batched_fn(handle,
                                                    side,
                                                    uplo,
                                                    transA,
                                                    diag,
                                                    M,
                                                    N,
                                                    alpha_d,
                                                    dA.ptr_on_device(),
                                                    lda,
                                                    dXorB.ptr_on_device(),
                                                    ldb,
                                                    batch_count));

        CHECK_HIP_ERROR(hXorB_2.transfer_from(dXorB));

        if(alpha_h == 0)
        {
            // expecting 0 output, set hX == 0
            for(rocblas_int b = 0; b < batch_count; b++)
                rocblas_init_zero((T*)hX[b], M, N, ldb);

            if(arg.unit_check)
            {
                unit_check_general<T>(M, N, ldb, hX, hXorB_1, batch_count);
                unit_check_general<T>(M, N, ldb, hX, hXorB_2, batch_count);
            }

            if(arg.norm_check)
            {
                max_err_1
                    = std::abs(norm_check_general<T>('F', M, N, ldb, hX, hXorB_1, batch_count));
                max_err_2
                    = std::abs(norm_check_general<T>('F', M, N, ldb, hX, hXorB_2, batch_count));
            }
        }
        else
        {
            //computed result is in hx_or_b, so forward error is E = hx - hx_or_b
            // calculate vector-induced-norm 1 of matrix E
            for(int b = 0; b < batch_count; b++)
            {
                max_err_1 = rocblas_abs(matrix_norm_1<T>(M, N, ldb, hX[b], hXorB_1[b]));
                max_err_2 = rocblas_abs(matrix_norm_1<T>(M, N, ldb, hX[b], hXorB_2[b]));

                //unit test
                trsm_err_res_check<T>(max_err_1, M, error_eps_multiplier, eps);
                trsm_err_res_check<T>(max_err_2, M, error_eps_multiplier, eps);

                // hx_or_b contains A * (calculated X), so res = A * (calculated x) - b = hx_or_b - hb
                cblas_trmm<T>(
                    side, uplo, transA, diag, M, N, 1.0 / alpha_h, hA[b], lda, hXorB_1[b], ldb);
                cblas_trmm<T>(
                    side, uplo, transA, diag, M, N, 1.0 / alpha_h, hA[b], lda, hXorB_2[b], ldb);

                // calculate vector-induced-norm 1 of matrix res
                max_err_1 = rocblas_abs(matrix_norm_1<T>(M, N, ldb, hXorB_1[b], hB[b]));
                max_err_2 = rocblas_abs(matrix_norm_1<T>(M, N, ldb, hXorB_2[b], hB[b]));

                //unit test
                trsm_err_res_check<T>(max_err_1, M, residual_eps_multiplier, eps);
                trsm_err_res_check<T>(max_err_2, M, residual_eps_multiplier, eps);
            }
        }
    }

    if(arg.timing)
    {
        int number_cold_calls = arg.cold_iters;
        int number_hot_calls  = arg.iters;

        // GPU rocBLAS
        CHECK_HIP_ERROR(dXorB.transfer_from(hXorB_1));

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
        gpu_time_used = get_time_us_sync(stream); // in microseconds

        for(int i = 0; i < number_cold_calls; i++)
        {
            CHECK_ROCBLAS_ERROR(rocblas_trsm_batched_fn(handle,
                                                        side,
                                                        uplo,
                                                        transA,
                                                        diag,
                                                        M,
                                                        N,
                                                        &alpha_h,
                                                        dA.ptr_on_device(),
                                                        lda,
                                                        dXorB.ptr_on_device(),
                                                        ldb,
                                                        batch_count));
        }

        gpu_time_used = get_time_us_sync(stream);

        for(int i = 0; i < number_hot_calls; i++)
        {
            CHECK_ROCBLAS_ERROR(rocblas_trsm_batched_fn(handle,
                                                        side,
                                                        uplo,
                                                        transA,
                                                        diag,
                                                        M,
                                                        N,
                                                        &alpha_h,
                                                        dA.ptr_on_device(),
                                                        lda,
                                                        dXorB.ptr_on_device(),
                                                        ldb,
                                                        batch_count));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        // CPU cblas
        cpu_time_used = get_time_us_no_sync();

        for(int b = 0; b < batch_count; b++)
            cblas_trsm<T>(side, uplo, transA, diag, M, N, alpha_h, hA[b], lda, cpuXorB[b], ldb);

        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        ArgumentModel<e_side,
                      e_uplo,
                      e_transA,
                      e_diag,
                      e_M,
                      e_N,
                      e_alpha,
                      e_lda,
                      e_ldb,
                      e_batch_count>{}
            .log_args<T>(rocblas_cout,
                         arg,
                         gpu_time_used,
                         trsm_gflop_count<T>(M, N, K),
                         ArgumentLogging::NA_value,
                         cpu_time_used,
                         max_err_1,
                         max_err_2);
    }
}
