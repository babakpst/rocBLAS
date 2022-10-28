/* ************************************************************************
 * Copyright (C) 2018-2022 Advanced Micro Devices, Inc. All rights reserved.
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
#include "norm.hpp"
#include "rocblas.hpp"
#include "rocblas_init.hpp"
#include "rocblas_math.hpp"
#include "rocblas_random.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "unit.hpp"
#include "utility.hpp"

template <typename T, typename U = T>
void testing_scal_bad_arg(const Arguments& arg)
{
    auto rocblas_scal_fn = arg.fortran ? rocblas_scal<T, U, true> : rocblas_scal<T, U, false>;

    rocblas_int N     = 100;
    rocblas_int incx  = 1;
    U           alpha = (U)0.6;

    rocblas_local_handle handle{arg};

    // Allocate device memory
    device_vector<T> dx(N, incx);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dx.memcheck());

    EXPECT_ROCBLAS_STATUS((rocblas_scal_fn(nullptr, N, &alpha, dx, incx)),
                          rocblas_status_invalid_handle);
    EXPECT_ROCBLAS_STATUS((rocblas_scal_fn(handle, N, nullptr, dx, incx)),
                          rocblas_status_invalid_pointer);
    EXPECT_ROCBLAS_STATUS((rocblas_scal_fn(handle, N, &alpha, nullptr, incx)),
                          rocblas_status_invalid_pointer);
}

template <typename T, typename U = T>
void testing_scal(const Arguments& arg)
{
    auto rocblas_scal_fn = arg.fortran ? rocblas_scal<T, U, true> : rocblas_scal<T, U, false>;

    rocblas_int N       = arg.N;
    rocblas_int incx    = arg.incx;
    U           h_alpha = arg.get_alpha<U>();

    rocblas_local_handle handle{arg};

    // argument sanity check before allocating invalid memory
    if(N <= 0 || incx <= 0)
    {
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        CHECK_ROCBLAS_ERROR((rocblas_scal_fn(handle, N, nullptr, nullptr, incx)));
        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hx_1), `d` is in GPU (device) memory (eg dx_1).
    // Allocate host memory
    host_vector<T> hx_1(N, incx);
    host_vector<T> hx_2(N, incx);
    host_vector<T> hx_gold(N, incx);
    host_vector<U> halpha(1);
    halpha[0] = h_alpha;

    // Allocate device memory
    device_vector<T> dx_1(N, incx);
    device_vector<T> dx_2(N, incx);
    device_vector<U> d_alpha(1);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dx_1.memcheck());
    CHECK_DEVICE_ALLOCATION(dx_2.memcheck());
    CHECK_DEVICE_ALLOCATION(d_alpha.memcheck());

    // Initial Data on CPU
    rocblas_init_vector(hx_1, arg, rocblas_client_alpha_sets_nan, true);

    // copy vector is easy in STL; hx_gold = hx: save a copy in hx_gold which will be output of CPU
    // BLAS
    hx_2    = hx_1;
    hx_gold = hx_1;

    // copy data from CPU to device
    CHECK_HIP_ERROR(dx_1.transfer_from(hx_1));

    double gpu_time_used, cpu_time_used;
    double rocblas_error_1 = 0.0;
    double rocblas_error_2 = 0.0;
    if(arg.unit_check || arg.norm_check)
    {
        CHECK_HIP_ERROR(dx_2.transfer_from(hx_2));
        CHECK_HIP_ERROR(d_alpha.transfer_from(halpha));

        // GPU BLAS, rocblas_pointer_mode_host
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        handle.pre_test(arg);
        CHECK_ROCBLAS_ERROR((rocblas_scal_fn(handle, N, &h_alpha, dx_1, incx)));
        handle.post_test(arg);

        // GPU BLAS, rocblas_pointer_mode_device
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
        handle.pre_test(arg);
        CHECK_ROCBLAS_ERROR((rocblas_scal_fn(handle, N, d_alpha, dx_2, incx)));
        handle.post_test(arg);

        // Transfer output from device to CPU
        CHECK_HIP_ERROR(hx_1.transfer_from(dx_1));
        CHECK_HIP_ERROR(hx_2.transfer_from(dx_2));

        // CPU BLAS
        cpu_time_used = get_time_us_no_sync();
        cblas_scal(N, h_alpha, (T*)hx_gold, incx);
        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        if(arg.unit_check)
        {
            unit_check_general<T>(1, N, incx, hx_gold, hx_1);
            unit_check_general<T>(1, N, incx, hx_gold, hx_2);
        }

        if(arg.norm_check)
        {
            rocblas_error_1 = norm_check_general<T>('F', 1, N, incx, hx_gold, hx_1);
            rocblas_error_2 = norm_check_general<T>('F', 1, N, incx, hx_gold, hx_2);
        }

    } // end of if unit/norm check

    if(arg.timing)
    {
        int number_cold_calls = arg.cold_iters;
        int number_hot_calls  = arg.iters;
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));

        for(int iter = 0; iter < number_cold_calls; iter++)
        {
            rocblas_scal_fn(handle, N, &h_alpha, dx_1, incx);
        }

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
        gpu_time_used = get_time_us_sync(stream); // in microseconds

        for(int iter = 0; iter < number_hot_calls; iter++)
        {
            rocblas_scal_fn(handle, N, &h_alpha, dx_1, incx);
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_N, e_alpha, e_incx>{}.log_args<T>(rocblas_cout,
                                                          arg,
                                                          gpu_time_used,
                                                          scal_gflop_count<T, U>(N),
                                                          scal_gbyte_count<T>(N),
                                                          cpu_time_used,
                                                          rocblas_error_1,
                                                          rocblas_error_2);
    }
}
