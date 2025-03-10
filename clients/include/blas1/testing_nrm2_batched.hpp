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
void testing_nrm2_batched_bad_arg(const Arguments& arg)
{
    auto rocblas_nrm2_batched_fn
        = arg.api == FORTRAN ? rocblas_nrm2_batched<T, true> : rocblas_nrm2_batched<T, false>;

    auto rocblas_nrm2_batched_fn_64 = arg.api == FORTRAN_64 ? rocblas_nrm2_batched_64<T, true>
                                                            : rocblas_nrm2_batched_64<T, false>;

    int64_t N           = 100;
    int64_t incx        = 1;
    int64_t batch_count = 1;

    rocblas_local_handle handle{arg};

    // Allocate device memory
    device_batch_vector<T>   dx(N, incx, batch_count);
    device_vector<real_t<T>> d_rocblas_result(1);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(d_rocblas_result.memcheck());

    CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));

    DAPI_EXPECT(rocblas_status_invalid_handle,
                rocblas_nrm2_batched_fn,
                (nullptr, N, dx.ptr_on_device(), incx, batch_count, d_rocblas_result));
    DAPI_EXPECT(rocblas_status_invalid_pointer,
                rocblas_nrm2_batched_fn,
                (handle, N, nullptr, incx, batch_count, d_rocblas_result));
    DAPI_EXPECT(rocblas_status_invalid_pointer,
                rocblas_nrm2_batched_fn,
                (handle, N, dx.ptr_on_device(), incx, batch_count, nullptr));
}

template <typename T>
void testing_nrm2_batched(const Arguments& arg)
{

    auto rocblas_nrm2_batched_fn
        = arg.api == FORTRAN ? rocblas_nrm2_batched<T, true> : rocblas_nrm2_batched<T, false>;

    auto rocblas_nrm2_batched_fn_64 = arg.api == FORTRAN_64 ? rocblas_nrm2_batched_64<T, true>
                                                            : rocblas_nrm2_batched_64<T, false>;

    int64_t N           = arg.N;
    int64_t incx        = arg.incx;
    int64_t batch_count = arg.batch_count;

    double rocblas_error_1;
    double rocblas_error_2;

    rocblas_local_handle handle{arg};

    // check to prevent undefined memory allocation error
    if(N <= 0 || incx <= 0 || batch_count <= 0)
    {
        device_vector<real_t<T>> d_rocblas_result_0(std::max(int64_t(1), batch_count));
        host_vector<real_t<T>>   h_rocblas_result_0(std::max(int64_t(1), batch_count));
        CHECK_HIP_ERROR(d_rocblas_result_0.memcheck());
        CHECK_HIP_ERROR(h_rocblas_result_0.memcheck());

        rocblas_init_nan(h_rocblas_result_0, 1, std::max(int64_t(1), batch_count), 1);
        CHECK_HIP_ERROR(hipMemcpy(d_rocblas_result_0,
                                  h_rocblas_result_0,
                                  sizeof(real_t<T>) * std::max(int64_t(1), batch_count),
                                  hipMemcpyHostToDevice));

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
        DAPI_CHECK(rocblas_nrm2_batched_fn,
                   (handle, N, nullptr, incx, batch_count, d_rocblas_result_0));

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        DAPI_CHECK(rocblas_nrm2_batched_fn,
                   (handle, N, nullptr, incx, batch_count, h_rocblas_result_0));

        if(batch_count > 0)
        {
            host_vector<real_t<T>> cpu_0(batch_count);
            host_vector<real_t<T>> gpu_0(batch_count);
            CHECK_HIP_ERROR(cpu_0.memcheck());
            CHECK_HIP_ERROR(gpu_0.memcheck());

            CHECK_HIP_ERROR(hipMemcpy(
                gpu_0, d_rocblas_result_0, sizeof(real_t<T>) * batch_count, hipMemcpyDeviceToHost));
            unit_check_general<real_t<T>>(1, batch_count, 1, cpu_0, gpu_0);
            unit_check_general<real_t<T>>(1, batch_count, 1, cpu_0, h_rocblas_result_0);
        }

        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hx), `d` is in GPU (device) memory (eg dx).
    // Allocate host memory
    host_batch_vector<T>   hx(N, incx, batch_count);
    host_vector<real_t<T>> rocblas_result_1(batch_count);
    host_vector<real_t<T>> rocblas_result_2(batch_count);
    host_vector<real_t<T>> cpu_result(batch_count);

    // Check host memory allocation
    CHECK_HIP_ERROR(hx.memcheck());

    // Allocate device memory
    device_batch_vector<T>   dx(N, incx, batch_count);
    device_vector<real_t<T>> d_rocblas_result_2(batch_count);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(d_rocblas_result_2.memcheck());
    CHECK_DEVICE_ALLOCATION(dx.memcheck());

    // Initialize memory on host.
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, true, true);

    CHECK_HIP_ERROR(dx.transfer_from(hx));

    double cpu_time_used;

    if(arg.unit_check || arg.norm_check)
    {
        if(arg.pointer_mode_host)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
            DAPI_CHECK(rocblas_nrm2_batched_fn,
                       (handle, N, dx.ptr_on_device(), incx, batch_count, rocblas_result_1));
        }

        if(arg.pointer_mode_device)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
            handle.pre_test(arg);
            DAPI_CHECK(rocblas_nrm2_batched_fn,
                       (handle, N, dx.ptr_on_device(), incx, batch_count, d_rocblas_result_2));
            handle.post_test(arg);
        }

        // CPU BLAS
        cpu_time_used = get_time_us_no_sync();
        for(int b = 0; b < batch_count; b++)
        {
            ref_nrm2<T>(N, hx[b], incx, cpu_result + b);
        }
        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        real_t<T> abs_result = cpu_result[0] > 0 ? cpu_result[0] : -cpu_result[0];
        real_t<T> abs_error;
        if(abs_result > 0)
        {
            abs_error = std::numeric_limits<real_t<T>>::epsilon() * N * abs_result;
        }
        else
        {
            abs_error = std::numeric_limits<real_t<T>>::epsilon() * N;
        }
        real_t<T> tolerance = 2.0; //  accounts for rounding in reduction sum. depends on n.
            //  If test fails, try decreasing n or increasing tolerance.
        abs_error *= tolerance;

        if(arg.pointer_mode_host)
        {
            if(!rocblas_isnan(arg.alpha))
            {
                if(arg.unit_check)
                {
                    near_check_general<real_t<T>, real_t<T>>(
                        batch_count, 1, 1, cpu_result, rocblas_result_1, abs_error);
                }
            }

            if(arg.norm_check)
            {
                for(int b = 0; b < batch_count; ++b)
                {
                    rocblas_error_1
                        += rocblas_abs((cpu_result[b] - rocblas_result_1[b]) / cpu_result[b]);
                }
            }
        }

        if(arg.pointer_mode_device)
        {
            CHECK_HIP_ERROR(rocblas_result_2.transfer_from(d_rocblas_result_2));

            if(!rocblas_isnan(arg.alpha))
            {
                if(arg.unit_check)
                {
                    near_check_general<real_t<T>, real_t<T>>(
                        batch_count, 1, 1, cpu_result, rocblas_result_2, abs_error);
                }
            }

            if(arg.norm_check)
            {
                for(int b = 0; b < batch_count; ++b)
                {
                    rocblas_error_2
                        += rocblas_abs((cpu_result[b] - rocblas_result_2[b]) / cpu_result[b]);
                }
            }
        }
    }

    if(arg.timing)
    {
        double gpu_time_used;
        int    number_cold_calls = arg.cold_iters;
        int    total_calls       = number_cold_calls + arg.iters;

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));

        for(int iter = 0; iter < total_calls; iter++)
        {
            if(iter == number_cold_calls)
                gpu_time_used = get_time_us_sync(stream); // in microseconds

            DAPI_DISPATCH(rocblas_nrm2_batched_fn,
                          (handle, N, dx.ptr_on_device(), incx, batch_count, d_rocblas_result_2));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_N, e_incx, e_batch_count>{}.log_args<T>(rocblas_cout,
                                                                arg,
                                                                gpu_time_used,
                                                                nrm2_gflop_count<T>(N),
                                                                nrm2_gbyte_count<T>(N),
                                                                cpu_time_used,
                                                                rocblas_error_1,
                                                                rocblas_error_2);
    }
}
