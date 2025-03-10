/* ************************************************************************
 * Copyright (C) 2020-2023 Advanced Micro Devices, Inc. All rights reserved.
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

#include "bytes.hpp"
#include "cblas_interface.hpp"
#include "flops.hpp"
#include "near.hpp"
#include "norm.hpp"
#include "rocblas.hpp"
#include "rocblas_init.hpp"
#include "rocblas_math.hpp"
#include "rocblas_random.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "unit.hpp"
#include "utility.hpp"

template <typename T>
void testing_sbmv_strided_batched_bad_arg(const Arguments& arg)
{
    auto rocblas_sbmv_strided_batched_fn = arg.api == FORTRAN
                                               ? rocblas_sbmv_strided_batched<T, true>
                                               : rocblas_sbmv_strided_batched<T, false>;

    for(auto pointer_mode : {rocblas_pointer_mode_host, rocblas_pointer_mode_device})
    {
        rocblas_local_handle handle{arg};
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, pointer_mode));

        rocblas_fill uplo        = rocblas_fill_upper;
        rocblas_int  N           = 100;
        rocblas_int  K           = 2;
        rocblas_int  incx        = 1;
        rocblas_int  incy        = 1;
        rocblas_int  lda         = 100;
        rocblas_int  batch_count = 2;

        device_vector<T> alpha_d(1), beta_d(1), one_d(1), zero_d(1);

        const T alpha_h(1), beta_h(2), one_h(1), zero_h(0);

        const T* alpha = &alpha_h;
        const T* beta  = &beta_h;
        const T* one   = &one_h;
        const T* zero  = &zero_h;

        if(pointer_mode == rocblas_pointer_mode_device)
        {
            CHECK_HIP_ERROR(hipMemcpy(alpha_d, alpha, sizeof(*alpha), hipMemcpyHostToDevice));
            alpha = alpha_d;
            CHECK_HIP_ERROR(hipMemcpy(beta_d, beta, sizeof(*beta), hipMemcpyHostToDevice));
            beta = beta_d;
            CHECK_HIP_ERROR(hipMemcpy(one_d, one, sizeof(*one), hipMemcpyHostToDevice));
            one = one_d;
            CHECK_HIP_ERROR(hipMemcpy(zero_d, zero, sizeof(*zero), hipMemcpyHostToDevice));
            zero = zero_d;
        }

        rocblas_int banded_matrix_row = K + 1;

        rocblas_stride strideA = N * lda;
        rocblas_stride stridex = N * incx;
        rocblas_stride stridey = N * incy;

        // Allocate device memory
        device_strided_batch_matrix<T> dAb(banded_matrix_row, N, 1, strideA, batch_count);
        device_strided_batch_vector<T> dx(N, incx, stridex, batch_count);
        device_strided_batch_vector<T> dy(N, incy, stridey, batch_count);

        // Check device memory allocation
        CHECK_DEVICE_ALLOCATION(dAb.memcheck());
        CHECK_DEVICE_ALLOCATION(dx.memcheck());
        CHECK_DEVICE_ALLOCATION(dy.memcheck());

        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(nullptr,
                                                              uplo,
                                                              N,
                                                              K,
                                                              alpha,
                                                              dAb,
                                                              lda,
                                                              strideA,
                                                              dx,
                                                              incx,
                                                              stridex,
                                                              beta,
                                                              dy,
                                                              incy,
                                                              stridey,
                                                              batch_count),
                              rocblas_status_invalid_handle);

        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                              rocblas_fill_full,
                                                              N,
                                                              K,
                                                              alpha,
                                                              dAb,
                                                              lda,
                                                              strideA,
                                                              dx,
                                                              incx,
                                                              stridex,
                                                              beta,
                                                              dy,
                                                              incy,
                                                              stridey,
                                                              batch_count),
                              rocblas_status_invalid_value);

        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                              uplo,
                                                              N,
                                                              K,
                                                              nullptr,
                                                              dAb,
                                                              lda,
                                                              strideA,
                                                              dx,
                                                              incx,
                                                              stridex,
                                                              beta,
                                                              dy,
                                                              incy,
                                                              stridey,
                                                              batch_count),
                              rocblas_status_invalid_pointer);

        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                              uplo,
                                                              N,
                                                              K,
                                                              alpha,
                                                              dAb,
                                                              lda,
                                                              strideA,
                                                              dx,
                                                              incx,
                                                              stridex,
                                                              nullptr,
                                                              dy,
                                                              incy,
                                                              stridey,
                                                              batch_count),
                              rocblas_status_invalid_pointer);

        if(pointer_mode == rocblas_pointer_mode_host)
        {
            EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                                  uplo,
                                                                  N,
                                                                  K,
                                                                  alpha,
                                                                  nullptr,
                                                                  lda,
                                                                  strideA,
                                                                  dx,
                                                                  incx,
                                                                  stridex,
                                                                  beta,
                                                                  dy,
                                                                  incy,
                                                                  stridey,
                                                                  batch_count),
                                  rocblas_status_invalid_pointer);

            EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                                  uplo,
                                                                  N,
                                                                  K,
                                                                  alpha,
                                                                  dAb,
                                                                  lda,
                                                                  strideA,
                                                                  nullptr,
                                                                  incx,
                                                                  stridex,
                                                                  beta,
                                                                  dy,
                                                                  incy,
                                                                  stridey,
                                                                  batch_count),
                                  rocblas_status_invalid_pointer);

            EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                                  uplo,
                                                                  N,
                                                                  K,
                                                                  alpha,
                                                                  dAb,
                                                                  lda,
                                                                  strideA,
                                                                  dx,
                                                                  incx,
                                                                  stridex,
                                                                  beta,
                                                                  nullptr,
                                                                  incy,
                                                                  stridey,
                                                                  batch_count),
                                  rocblas_status_invalid_pointer);
        }

        // N==0 all pointers may be null
        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                              uplo,
                                                              0,
                                                              K,
                                                              nullptr,
                                                              nullptr,
                                                              lda,
                                                              strideA,
                                                              nullptr,
                                                              incx,
                                                              stridex,
                                                              nullptr,
                                                              nullptr,
                                                              incy,
                                                              stridey,
                                                              batch_count),
                              rocblas_status_success);

        // alpha==0 then A and x pointers may be null
        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                              uplo,
                                                              N,
                                                              K,
                                                              zero,
                                                              nullptr,
                                                              lda,
                                                              strideA,
                                                              nullptr,
                                                              incx,
                                                              stridex,
                                                              beta,
                                                              dy,
                                                              incy,
                                                              stridey,
                                                              batch_count),
                              rocblas_status_success);

        // alpha==0 and beta==1 all pointers may be null
        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                              uplo,
                                                              N,
                                                              K,
                                                              zero,
                                                              nullptr,
                                                              lda,
                                                              strideA,
                                                              nullptr,
                                                              incx,
                                                              stridex,
                                                              one,
                                                              nullptr,
                                                              incy,
                                                              stridey,
                                                              batch_count),
                              rocblas_status_success);

        // batch_count==0 all pointers may be null
        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                              uplo,
                                                              N,
                                                              K,
                                                              nullptr,
                                                              nullptr,
                                                              lda,
                                                              strideA,
                                                              nullptr,
                                                              incx,
                                                              stridex,
                                                              nullptr,
                                                              nullptr,
                                                              incy,
                                                              stridey,
                                                              0),
                              rocblas_status_success);
    }
}

template <typename T>
void testing_sbmv_strided_batched(const Arguments& arg)
{
    auto rocblas_sbmv_strided_batched_fn = arg.api == FORTRAN
                                               ? rocblas_sbmv_strided_batched<T, true>
                                               : rocblas_sbmv_strided_batched<T, false>;

    rocblas_int N                 = arg.N;
    rocblas_int lda               = arg.lda;
    rocblas_int K                 = arg.K;
    rocblas_int incx              = arg.incx;
    rocblas_int incy              = arg.incy;
    rocblas_int banded_matrix_row = K + 1;

    host_vector<T> alpha(1);
    host_vector<T> beta(1);
    alpha[0] = arg.get_alpha<T>();
    beta[0]  = arg.get_beta<T>();

    rocblas_fill uplo        = char2rocblas_fill(arg.uplo);
    rocblas_int  batch_count = arg.batch_count;

    rocblas_stride strideA = arg.stride_a;
    size_t         size_A  = size_t(lda) * N;
    rocblas_stride stridex = arg.stride_x;
    rocblas_stride stridey = arg.stride_y;

    rocblas_local_handle handle{arg};

    // argument sanity check before allocating invalid memory
    bool invalid_size
        = N < 0 || lda < banded_matrix_row || K < 0 || !incx || !incy || batch_count < 0;
    if(invalid_size || !N || !batch_count)
    {
        EXPECT_ROCBLAS_STATUS(rocblas_sbmv_strided_batched_fn(handle,
                                                              uplo,
                                                              N,
                                                              K,
                                                              nullptr,
                                                              nullptr,
                                                              lda,
                                                              strideA,
                                                              nullptr,
                                                              incx,
                                                              stridex,
                                                              nullptr,
                                                              nullptr,
                                                              incy,
                                                              stridey,
                                                              batch_count),
                              invalid_size ? rocblas_status_invalid_size : rocblas_status_success);

        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hAb), `d` is in GPU (device) memory (eg dAb).
    // Allocate host memory
    host_strided_batch_matrix<T> hAb(banded_matrix_row, N, lda, strideA, batch_count);
    host_strided_batch_vector<T> hx(N, incx, stridex, batch_count);
    host_strided_batch_vector<T> hy(N, incy, stridey, batch_count);
    host_strided_batch_vector<T> hy_gold(N, incy, stridey, batch_count); // gold standard

    // Check host memory allocation
    CHECK_HIP_ERROR(hAb.memcheck());
    CHECK_HIP_ERROR(hx.memcheck());
    CHECK_HIP_ERROR(hy.memcheck());
    CHECK_HIP_ERROR(hy_gold.memcheck());

    // Allocate device memory
    device_strided_batch_matrix<T> dAb(banded_matrix_row, N, lda, strideA, batch_count);
    device_strided_batch_vector<T> dx(N, incx, stridex, batch_count);
    device_strided_batch_vector<T> dy(N, incy, stridey, batch_count);
    device_vector<T>               d_alpha(1);
    device_vector<T>               d_beta(1);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dAb.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());
    CHECK_DEVICE_ALLOCATION(d_alpha.memcheck());
    CHECK_DEVICE_ALLOCATION(d_beta.memcheck());

    // Initialize data on host memory
    rocblas_init_matrix(
        hAb, arg, rocblas_client_alpha_sets_nan, rocblas_client_general_matrix, true);
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, false, true);
    rocblas_init_vector(hy, arg, rocblas_client_beta_sets_nan);

    // make copy in hy_gold which will later be used with CPU BLAS
    hy_gold.copy_from(hy);

    // copy data from CPU to device
    dx.transfer_from(hx);
    dy.transfer_from(hy);
    dAb.transfer_from(hAb);

    double gpu_time_used, cpu_time_used;
    double h_error = 0.0, d_error = 0.0;

    if(arg.unit_check || arg.norm_check)
    {
        if(arg.pointer_mode_host)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

            handle.pre_test(arg);
            CHECK_ROCBLAS_ERROR(rocblas_sbmv_strided_batched_fn(handle,
                                                                uplo,
                                                                N,
                                                                K,
                                                                alpha,
                                                                dAb,
                                                                lda,
                                                                strideA,
                                                                dx,
                                                                incx,
                                                                stridex,
                                                                beta,
                                                                dy,
                                                                incy,
                                                                stridey,
                                                                batch_count));
            handle.post_test(arg);

            // copy output from device to CPU
            CHECK_HIP_ERROR(hy.transfer_from(dy));
        }

        if(arg.pointer_mode_device)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
            CHECK_HIP_ERROR(d_alpha.transfer_from(alpha));
            CHECK_HIP_ERROR(d_beta.transfer_from(beta));

            dy.transfer_from(hy_gold);

            handle.pre_test(arg);
            CHECK_ROCBLAS_ERROR(rocblas_sbmv_strided_batched_fn(handle,
                                                                uplo,
                                                                N,
                                                                K,
                                                                d_alpha,
                                                                dAb,
                                                                lda,
                                                                strideA,
                                                                dx,
                                                                incx,
                                                                stridex,
                                                                d_beta,
                                                                dy,
                                                                incy,
                                                                stridey,
                                                                batch_count));
            handle.post_test(arg);
        }

        // cpu reference
        cpu_time_used = get_time_us_no_sync();
        for(int b = 0; b < batch_count; b++)
        {
            ref_sbmv<T>(uplo, N, K, alpha[0], hAb[b], lda, hx[b], incx, beta[0], hy_gold[b], incy);
        }
        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        if(arg.pointer_mode_host)
        {
            if(arg.unit_check)
            {
                unit_check_general<T>(1, N, incy, stridey, hy_gold, hy, batch_count);
            }

            if(arg.norm_check)
            {
                h_error = norm_check_general<T>('F', 1, N, incy, stridey, hy_gold, hy, batch_count);
            }
        }

        if(arg.pointer_mode_device)
        {
            // copy output from device to CPU
            CHECK_HIP_ERROR(hy.transfer_from(dy));

            if(arg.unit_check)
            {
                unit_check_general<T>(1, N, incy, stridey, hy_gold, hy, batch_count);
            }

            if(arg.norm_check)
            {
                d_error = norm_check_general<T>('F', 1, N, incy, stridey, hy_gold, hy, batch_count);
            }
        }
    }

    if(arg.timing)
    {

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

        int number_cold_calls = arg.cold_iters;
        int number_hot_calls  = arg.iters;

        for(int iter = 0; iter < number_cold_calls; iter++)
        {
            CHECK_ROCBLAS_ERROR(rocblas_sbmv_strided_batched_fn(handle,
                                                                uplo,
                                                                N,
                                                                K,
                                                                alpha,
                                                                dAb,
                                                                lda,
                                                                strideA,
                                                                dx,
                                                                incx,
                                                                stridex,
                                                                beta,
                                                                dy,
                                                                incy,
                                                                stridey,
                                                                batch_count));
        }

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
        gpu_time_used = get_time_us_sync(stream); // in microseconds

        for(int iter = 0; iter < number_hot_calls; iter++)
        {
            CHECK_ROCBLAS_ERROR(rocblas_sbmv_strided_batched_fn(handle,
                                                                uplo,
                                                                N,
                                                                K,
                                                                alpha,
                                                                dAb,
                                                                lda,
                                                                strideA,
                                                                dx,
                                                                incx,
                                                                stridex,
                                                                beta,
                                                                dy,
                                                                incy,
                                                                stridey,
                                                                batch_count));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_uplo,
                      e_N,
                      e_K,
                      e_alpha,
                      e_lda,
                      e_stride_a,
                      e_incx,
                      e_stride_x,
                      e_beta,
                      e_incy,
                      e_stride_y,
                      e_batch_count>{}
            .log_args<T>(rocblas_cout,
                         arg,
                         gpu_time_used,
                         sbmv_gflop_count<T>(N, K),
                         sbmv_gbyte_count<T>(N, K),
                         cpu_time_used,
                         h_error,
                         d_error);
    }
}
