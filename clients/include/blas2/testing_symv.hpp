/* ************************************************************************
 * Copyright (C) 2018-2024 Advanced Micro Devices, Inc. All rights reserved.
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

#include "testing_common.hpp"

template <typename T>
void testing_symv_bad_arg(const Arguments& arg)
{
    auto rocblas_symv_fn = arg.api == FORTRAN ? rocblas_symv<T, true> : rocblas_symv<T, false>;
    auto rocblas_symv_fn_64
        = arg.api == FORTRAN_64 ? rocblas_symv_64<T, true> : rocblas_symv_64<T, false>;

    for(auto pointer_mode : {rocblas_pointer_mode_host, rocblas_pointer_mode_device})
    {
        rocblas_local_handle handle{arg};
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, pointer_mode));

        const rocblas_fill uplo = rocblas_fill_upper;
        const int64_t      N    = 100;
        const int64_t      incx = 1;
        const int64_t      incy = 1;
        const int64_t      lda  = 100;

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

        // Allocate device memory
        device_matrix<T> dA(N, N, lda);
        device_vector<T> dx(N, incx);
        device_vector<T> dy(N, incy);

        // Check device memory allocation
        CHECK_DEVICE_ALLOCATION(dA.memcheck());
        CHECK_DEVICE_ALLOCATION(dx.memcheck());
        CHECK_DEVICE_ALLOCATION(dy.memcheck());

        DAPI_EXPECT(rocblas_status_invalid_handle,
                    rocblas_symv_fn,
                    (nullptr, uplo, N, alpha, dA, lda, dx, incx, beta, dy, incy));

        DAPI_EXPECT(rocblas_status_invalid_value,
                    rocblas_symv_fn,
                    (handle, rocblas_fill_full, N, alpha, dA, lda, dx, incx, beta, dy, incy));

        DAPI_EXPECT(rocblas_status_invalid_pointer,
                    rocblas_symv_fn,
                    (handle, uplo, N, nullptr, dA, lda, dx, incx, beta, dy, incy));

        DAPI_EXPECT(rocblas_status_invalid_pointer,
                    rocblas_symv_fn,
                    (handle, uplo, N, alpha, dA, lda, dx, incx, nullptr, dy, incy));

        if(pointer_mode == rocblas_pointer_mode_host)
        {
            DAPI_EXPECT(rocblas_status_invalid_pointer,
                        rocblas_symv_fn,
                        (handle, uplo, N, alpha, nullptr, lda, dx, incx, beta, dy, incy));

            DAPI_EXPECT(rocblas_status_invalid_pointer,
                        rocblas_symv_fn,
                        (handle, uplo, N, alpha, dA, lda, nullptr, incx, beta, dy, incy));

            DAPI_EXPECT(rocblas_status_invalid_pointer,
                        rocblas_symv_fn,
                        (handle, uplo, N, alpha, dA, lda, dx, incx, beta, nullptr, incy));
        }

        // If N==0, all pointers can be nullptr without error
        DAPI_EXPECT(
            rocblas_status_success,
            rocblas_symv_fn,
            (handle, uplo, 0, nullptr, nullptr, lda, nullptr, incx, nullptr, nullptr, incy));

        // If alpha==0, then A and x may be nullptr without error
        DAPI_EXPECT(rocblas_status_success,
                    rocblas_symv_fn,
                    (handle, uplo, N, zero, nullptr, lda, nullptr, incx, beta, dy, incy));

        // If alpha==0 && beta==1, then A, x and y may be nullptr without error
        DAPI_EXPECT(rocblas_status_success,
                    rocblas_symv_fn,
                    (handle, uplo, N, zero, nullptr, lda, nullptr, incx, one, nullptr, incy));

        if(arg.api & c_API_64)
        {
            int64_t n_large = 2147483649;
            DAPI_EXPECT(rocblas_status_invalid_size,
                        rocblas_symv_fn,
                        (handle, uplo, n_large, alpha, dA, lda, dx, incx, beta, dy, incy));
        }
    }
}

template <typename T>
void testing_symv(const Arguments& arg)
{
    auto rocblas_symv_fn = arg.api == FORTRAN ? rocblas_symv<T, true> : rocblas_symv<T, false>;
    auto rocblas_symv_fn_64
        = arg.api == FORTRAN_64 ? rocblas_symv_64<T, true> : rocblas_symv_64<T, false>;

    int64_t N    = arg.N;
    int64_t lda  = arg.lda;
    int64_t incx = arg.incx;
    int64_t incy = arg.incy;

    host_vector<T> alpha(1);
    host_vector<T> beta(1);
    alpha[0] = arg.get_alpha<T>();
    beta[0]  = arg.get_beta<T>();

    rocblas_fill uplo = char2rocblas_fill(arg.uplo);

    rocblas_local_handle handle{arg};

    // argument sanity check before allocating invalid memory
    if(N < 0 || lda < 1 || lda < N || !incx || !incy)
    {
        DAPI_EXPECT(
            rocblas_status_invalid_size,
            rocblas_symv_fn,
            (handle, uplo, N, nullptr, nullptr, lda, nullptr, incx, nullptr, nullptr, incy));
        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hA), `d` is in GPU (device) memory (eg dA).
    // Allocate host memory
    host_matrix<T> hA(N, N, lda);
    host_vector<T> hx(N, incx);
    host_vector<T> hy(N, incy);
    host_vector<T> hy_gold(N, incy); // gold standard

    // Allocate device memory
    device_matrix<T> dA(N, N, lda);
    device_vector<T> dx(N, incx);
    device_vector<T> dy(N, incy);
    device_vector<T> d_alpha(1);
    device_vector<T> d_beta(1);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dA.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());
    CHECK_DEVICE_ALLOCATION(d_alpha.memcheck());
    CHECK_DEVICE_ALLOCATION(d_beta.memcheck());

    // Initialize data on host memory
    rocblas_init_matrix(
        hA, arg, rocblas_client_alpha_sets_nan, rocblas_client_symmetric_matrix, true);
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, false, true);
    rocblas_init_vector(hy, arg, rocblas_client_beta_sets_nan);

    // Make copy in hy_gold which will later be used with CPU BLAS
    hy_gold = hy;

    // copy data from CPU to device
    CHECK_HIP_ERROR(dx.transfer_from(hx));
    CHECK_HIP_ERROR(dy.transfer_from(hy));
    CHECK_HIP_ERROR(dA.transfer_from(hA));

    double cpu_time_used;
    double h_error, d_error;

    if(arg.unit_check || arg.norm_check)
    {
        if(arg.pointer_mode_host)
        {
            // rocblas_pointer_mode_host test
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

            handle.pre_test(arg);
            DAPI_CHECK(rocblas_symv_fn,
                       (handle, uplo, N, alpha, dA, lda, dx, incx, beta, dy, incy));
            handle.post_test(arg);

            // copy output from device to CPU
            CHECK_HIP_ERROR(hy.transfer_from(dy));
        }

        if(arg.pointer_mode_device)
        {
            // rocblas_pointer_mode_device test
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
            CHECK_HIP_ERROR(d_alpha.transfer_from(alpha));
            CHECK_HIP_ERROR(d_beta.transfer_from(beta));

            dy.transfer_from(hy_gold);

            handle.pre_test(arg);
            DAPI_CHECK(rocblas_symv_fn,
                       (handle, uplo, N, d_alpha, dA, lda, dx, incx, d_beta, dy, incy));
            handle.post_test(arg);
        }

        cpu_time_used = get_time_us_no_sync();

        // cpu reference
        ref_symv<T>(uplo, N, alpha[0], hA, lda, hx, incx, beta[0], hy_gold, incy);

        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        if(arg.pointer_mode_host)
        {
            if(arg.unit_check)
            {
                if(std::is_same_v<T, float> || std::is_same_v<T, double>)
                {
                    unit_check_general<T>(1, N, incy, hy_gold, hy);
                }
                else
                {
                    const double tol = N * sum_error_tolerance<T>;
                    near_check_general<T>(1, N, incy, hy_gold, hy, tol);
                }
            }

            if(arg.norm_check)
            {
                h_error = norm_check_general<T>('F', 1, N, incy, hy_gold, hy);
            }
        }

        if(arg.pointer_mode_device)
        {
            // copy output from device to CPU
            CHECK_HIP_ERROR(hy.transfer_from(dy));

            if(arg.unit_check)
            {
                if(std::is_same_v<T, float> || std::is_same_v<T, double>)
                {
                    unit_check_general<T>(1, N, incy, hy_gold, hy);
                }
                else
                {
                    const double tol = N * sum_error_tolerance<T>;
                    near_check_general<T>(1, N, incy, hy_gold, hy, tol);
                }
            }

            if(arg.norm_check)
            {
                d_error = norm_check_general<T>('F', 1, N, incy, hy_gold, hy);
            }
        }
    }

    if(arg.timing)
    {
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

        double gpu_time_used;
        int    number_cold_calls = arg.cold_iters;
        int    total_calls       = number_cold_calls + arg.iters;

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));

        for(int iter = 0; iter < total_calls; iter++)
        {
            if(iter == number_cold_calls)
                gpu_time_used = get_time_us_sync(stream);

            DAPI_DISPATCH(rocblas_symv_fn,
                          (handle, uplo, N, alpha, dA, lda, dx, incx, beta, dy, incy));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_uplo, e_N, e_alpha, e_lda, e_incx, e_beta, e_incy>{}.log_args<T>(
            rocblas_cout,
            arg,
            gpu_time_used,
            symv_gflop_count<T>(N),
            symv_gbyte_count<T>(N),
            cpu_time_used,
            h_error,
            d_error);
    }
}
