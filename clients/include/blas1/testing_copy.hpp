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

#include "testing_common.hpp"

template <typename T>
void testing_copy_bad_arg(const Arguments& arg)
{
    auto rocblas_copy_fn = arg.api == FORTRAN ? rocblas_copy<T, true> : rocblas_copy<T, false>;
    auto rocblas_copy_fn_64
        = arg.api == FORTRAN_64 ? rocblas_copy_64<T, true> : rocblas_copy_64<T, false>;

    rocblas_local_handle handle{arg};

    int64_t N    = 100;
    int64_t incx = 1;
    int64_t incy = 1;

    device_vector<T> dx(N);
    device_vector<T> dy(N);
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());

    DAPI_EXPECT(rocblas_status_invalid_handle, rocblas_copy_fn, (nullptr, N, dx, incx, dy, incy));

    DAPI_EXPECT(
        rocblas_status_invalid_pointer, rocblas_copy_fn, (handle, N, nullptr, incx, dy, incy));
    DAPI_EXPECT(
        rocblas_status_invalid_pointer, rocblas_copy_fn, (handle, N, dx, incx, nullptr, incy));
}

template <typename T>
void testing_copy(const Arguments& arg)
{
    auto rocblas_copy_fn = arg.api == FORTRAN ? rocblas_copy<T, true> : rocblas_copy<T, false>;
    auto rocblas_copy_fn_64
        = arg.api == FORTRAN_64 ? rocblas_copy_64<T, true> : rocblas_copy_64<T, false>;

    int64_t              N    = arg.N;
    int64_t              incx = arg.incx;
    int64_t              incy = arg.incy;
    rocblas_local_handle handle{arg};

    // Argument sanity check before allocating invalid memory
    if(N <= 0)
    {
        DAPI_CHECK(rocblas_copy_fn, (handle, N, nullptr, incx, nullptr, incy));
        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hx), `d` is in GPU (device) memory (eg dx).
    // Allocate host memory
    host_vector<T> hx(N, incx);
    host_vector<T> hy(N, incy);
    host_vector<T> hy_gold(N, incy);

    // Allocate device memory
    device_vector<T> dx(N, incx);
    device_vector<T> dy(N, incy);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());

    // Initialize data on host memory
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, true);
    rocblas_init_vector(hy, arg, rocblas_client_alpha_sets_nan, false);

    // copy vector is easy in STL; hy_gold = hx: save a copy in hy_gold which will be output of CPU
    // BLAS
    hy_gold = hy;

    // copy data from CPU to device
    CHECK_HIP_ERROR(dx.transfer_from(hx));
    CHECK_HIP_ERROR(dy.transfer_from(hy));

    double cpu_time_used;
    double rocblas_error = 0.0;

    if(arg.unit_check || arg.norm_check)
    {
        handle.pre_test(arg);
        // GPU BLAS
        DAPI_CHECK(rocblas_copy_fn, (handle, N, dx, incx, dy, incy));
        handle.post_test(arg);

        CHECK_HIP_ERROR(hy.transfer_from(dy));

        // CPU BLAS
        cpu_time_used = get_time_us_no_sync();
        ref_copy<T>(N, hx, incx, hy_gold, incy);
        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        if(arg.unit_check)
        {
            unit_check_general<T>(1, N, incy, hy_gold, hy);
        }

        if(arg.norm_check)
        {
            rocblas_error = norm_check_general<T>('F', 1, N, incy, hy_gold, hy);
        }
    }

    if(arg.timing)
    {
        double gpu_time_used;
        int    number_cold_calls = arg.cold_iters;
        int    total_calls       = number_cold_calls + arg.iters;

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));

        for(int iter = 0; iter < total_calls; iter++)
        {
            if(iter == number_cold_calls)
                gpu_time_used = get_time_us_sync(stream);

            DAPI_DISPATCH(rocblas_copy_fn, (handle, N, dx, incx, dy, incy));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_N, e_incx, e_incy>{}.log_args<T>(rocblas_cout,
                                                         arg,
                                                         gpu_time_used,
                                                         ArgumentLogging::NA_value,
                                                         copy_gbyte_count<T>(N),
                                                         cpu_time_used,
                                                         rocblas_error);
    }
}
