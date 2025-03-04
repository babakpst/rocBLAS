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
#include "rocblas.hpp"
#include "rocblas_math.hpp"
#include "rocblas_test.hpp"
#include "rocblas_vector.hpp"
#include "utility.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#ifdef WIN32
#include <stdlib.h>
#define setenv(A, B, C) _putenv_s(A, B)
#else
#include <sys/param.h>
#include <unistd.h>
#endif

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error no filesystem found
#endif

template <typename T>
static constexpr auto precision_letter = "*";
template <>
ROCBLAS_CLANG_STATIC constexpr auto precision_letter<rocblas_half> = "h";
template <>
ROCBLAS_CLANG_STATIC constexpr auto precision_letter<float> = "s";
template <>
ROCBLAS_CLANG_STATIC constexpr auto precision_letter<double> = "d";
template <>
ROCBLAS_CLANG_STATIC constexpr auto precision_letter<rocblas_float_complex> = "c";
template <>
ROCBLAS_CLANG_STATIC constexpr auto precision_letter<rocblas_double_complex> = "z";

#ifdef WIN32
int diff_files(std::string path1, std::string path2)
{
    std::replace(path1.begin(), path1.end(), '/', '\\');
    std::replace(path2.begin(), path2.end(), '/', '\\');
    std::ifstream actual(path1);
    std::ifstream expected(path2);
    std::string   test((std::istreambuf_iterator<char>(actual)), std::istreambuf_iterator<char>());
    std::string gold((std::istreambuf_iterator<char>(expected)), std::istreambuf_iterator<char>());
    actual.close();
    expected.close();
    int cmp = test != gold;
    return cmp;
}
#endif

// replaces X in string with s, d, c, z or h depending on typename T
template <typename T>
std::string replaceX(std::string input_string)
{
    std::replace(input_string.begin(), input_string.end(), 'X', precision_letter<T>[0]);
    return input_string;
}

template <typename T>
void testing_logging(const Arguments& arg)
{
    rocblas_pointer_mode test_pointer_mode = rocblas_pointer_mode_host;

    // set environment variable ROCBLAS_LAYER to turn on logging. Note that setenv
    // only has scope for this executable, so it is not necessary to save and restore
    // this environment variable
    //
    // ROCBLAS_LAYER is a bit mask:
    // ROCBLAS_LAYER = 1 turns on log_trace
    // ROCBLAS_LAYER = 2 turns on log_bench
    // ROCBLAS_LAYER = 4 turns on log_profile
    int setenv_status;

    setenv_status = setenv("ROCBLAS_LAYER", "3", true);

#ifdef GOOGLE_TEST
    ASSERT_EQ(setenv_status, 0);
#endif

    // open files
    static std::string tmp_dir = rocblas_tempname();

    const fs::path trace_fspath1
        = tmp_dir + std::string("trace_") + std::string(precision_letter<T>) + std::string(".csv");
    const fs::path trace_fspath2 = tmp_dir + std::string("trace_")
                                   + std::string(precision_letter<T>) + std::string("_gold.csv");
    const fs::path bench_fspath1
        = tmp_dir + std::string("bench_") + std::string(precision_letter<T>) + std::string(".txt");
    const fs::path bench_fspath2 = tmp_dir + std::string("bench_")
                                   + std::string(precision_letter<T>) + std::string("_gold.txt");

    std::string trace_path1 = trace_fspath1.generic_string();
    std::string trace_path2 = trace_fspath2.generic_string();
    std::string bench_path1 = bench_fspath1.generic_string();
    std::string bench_path2 = bench_fspath2.generic_string();

    // set environment variable to give pathname of for log_trace file
    setenv_status = setenv("ROCBLAS_LOG_TRACE_PATH", trace_path1.c_str(), true);

#ifdef GOOGLE_TEST
    ASSERT_EQ(setenv_status, 0);
#endif

    // set environment variable to give pathname of for log_bench file
    setenv_status = setenv("ROCBLAS_LOG_BENCH_PATH", bench_path1.c_str(), true);

#ifdef GOOGLE_TEST
    ASSERT_EQ(setenv_status, 0);
#endif

    //
    // call rocBLAS functions with log_trace and log_bench to output log_trace and log_bench files
    //

    rocblas_int       m           = 1;
    rocblas_int       n           = 1;
    rocblas_int       k           = 1;
    rocblas_int       kl          = 1;
    rocblas_int       ku          = 1;
    rocblas_int       incx        = 1;
    rocblas_int       incy        = 1;
    rocblas_int       lda         = 1;
    rocblas_int       stride_a    = 1;
    rocblas_int       ldb         = 1;
    rocblas_int       stride_b    = 1;
    rocblas_int       ldc         = 1;
    rocblas_int       stride_c    = 1;
    rocblas_int       ldd         = 1;
    rocblas_int       stride_d    = 1;
    rocblas_int       batch_count = 1;
    T                 alpha       = 1.0;
    T                 beta        = 1.0;
    rocblas_operation transA      = rocblas_operation_none;
    rocblas_operation transB      = rocblas_operation_transpose;
    rocblas_fill      uplo        = rocblas_fill_upper;
    rocblas_diagonal  diag        = rocblas_diagonal_unit;
    rocblas_side      side        = rocblas_side_left;

    rocblas_int safe_dim = ((m > n ? m : n) > k ? (m > n ? m : n) : k);
    rocblas_int size_x   = n * incx;
    rocblas_int size_y   = n * incy;
    rocblas_int size_a   = (lda > stride_a ? lda : stride_a) * safe_dim * batch_count;
    rocblas_int size_b   = (ldb > stride_b ? ldb : stride_b) * safe_dim * batch_count;
    rocblas_int size_c   = (ldc > stride_c ? ldc : stride_c) * safe_dim * batch_count;
    rocblas_int size_d   = (ldd > stride_d ? ldd : stride_d) * safe_dim * batch_count;

    // allocate memory on device
    device_vector<T> dx(size_x);
    device_vector<T> dy(size_y);
    device_vector<T> da(size_a);
    device_vector<T> db(size_b);
    device_vector<T> dc(size_c);
    device_vector<T> dd(size_d);
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());
    CHECK_DEVICE_ALLOCATION(da.memcheck());
    CHECK_DEVICE_ALLOCATION(db.memcheck());
    CHECK_DEVICE_ALLOCATION(dc.memcheck());
    CHECK_DEVICE_ALLOCATION(dd.memcheck());

    // enclose in {} so rocblas_local_handle destructor called as it goes out of scope
    {
        int                  i_result;
        T                    result;
        rocblas_pointer_mode mode;

        // Auxiliary functions
        rocblas_local_handle handle;

        rocblas_set_pointer_mode(handle, test_pointer_mode);
        rocblas_get_pointer_mode(handle, &mode);

        // *************************************************** BLAS1 ***************************************************
        rocblas_iamax<T>(handle, n, dx, incx, &i_result);

        rocblas_iamin<T>(handle, n, dx, incx, &i_result);

        rocblas_asum<T>(handle, n, dx, incx, &result);

        rocblas_axpy<T>(handle, n, &alpha, dx, incx, dy, incy);

        rocblas_copy<T>(handle, n, dx, incx, dy, incy);

        rocblas_dot<T>(handle, n, dx, incx, dy, incy, &result);

        rocblas_nrm2<T>(handle, n, dx, incx, &result);

        rocblas_scal<T>(handle, n, &alpha, dx, incx);

        rocblas_swap<T>(handle, n, dx, incx, dy, incy);

        // *************************************************** BLAS2 ***************************************************

        rocblas_gbmv<T>(handle, transA, m, n, kl, ku, &alpha, da, lda, dx, incx, &beta, dy, incy);

        rocblas_gemv<T>(handle, transA, m, n, &alpha, da, lda, dx, incx, &beta, dy, incy);

        rocblas_ger<T, false>(handle, m, n, &alpha, dx, incx, dy, incy, da, lda);

        rocblas_sbmv<T>(handle, uplo, n, k, &alpha, da, lda, dx, incx, &beta, dy, incy);

        rocblas_spmv<T>(handle, uplo, n, &alpha, da, dx, incx, &beta, dy, incy);

        rocblas_spr<T>(handle, uplo, n, &alpha, dx, incx, da);

        rocblas_spr2<T>(handle, uplo, n, &alpha, dx, incx, dy, incy, da);

        rocblas_symv<T>(handle, uplo, n, &alpha, da, lda, dx, incx, &beta, dy, incy);

        rocblas_syr<T>(handle, uplo, n, &alpha, dx, incx, da, lda);

        rocblas_syr2<T>(handle, uplo, n, &alpha, dx, incx, dy, incy, da, lda);

        rocblas_tbmv<T>(handle, uplo, transA, diag, m, k, da, lda, dx, incx);

        rocblas_tbsv<T>(handle, uplo, transA, diag, n, k, da, lda, dx, incx);

        rocblas_trmv<T>(handle, uplo, transA, diag, m, da, lda, dx, incx);

        rocblas_tpmv<T>(handle, uplo, transA, diag, m, da, dx, incx);

        rocblas_tpsv<T>(handle, uplo, transA, diag, n, da, dx, incx);

        rocblas_trmv<T>(handle, uplo, transA, diag, m, da, lda, dx, incx);

        rocblas_trsv<T>(handle, uplo, transA, diag, m, da, lda, dx, incx);

        if(BUILD_WITH_TENSILE)
        {
            // *************************************************** BLAS3 ***************************************************

            rocblas_geam<T>(handle, transA, transB, m, n, &alpha, da, lda, &beta, db, ldb, dc, ldc);

            rocblas_gemm<T>(
                handle, transA, transB, m, n, k, &alpha, da, lda, db, ldb, &beta, dc, ldc);

            rocblas_symm<T>(handle, side, uplo, m, n, &alpha, da, lda, db, ldb, &beta, dc, ldc);

            rocblas_syrk<T>(handle, uplo, transA, n, k, &alpha, da, lda, &beta, dc, ldc);

            rocblas_syr2k<T>(handle, uplo, transA, n, k, &alpha, da, lda, db, ldb, &beta, dc, ldc);

            rocblas_syrkx<T>(handle, uplo, transA, n, k, &alpha, da, lda, db, ldb, &beta, dc, ldc);

            rocblas_trmm<T>(
                handle, side, uplo, transA, diag, m, n, &alpha, da, lda, db, ldb, dc, ldc);

            rocblas_trsm<T>(handle, side, uplo, transA, diag, m, n, &alpha, da, lda, db, ldb);

            rocblas_gemm_strided_batched<T>(handle,
                                            transA,
                                            transB,
                                            m,
                                            n,
                                            k,
                                            &alpha,
                                            da,
                                            lda,
                                            stride_a,
                                            db,
                                            ldb,
                                            stride_b,
                                            &beta,
                                            dc,
                                            ldc,
                                            stride_c,
                                            batch_count);
        }

        // exclude trtri as it is an internal function
        //      rocblas_trtri<T>(handle, uplo, diag, n, da, lda, db, ldb);

        // trmm
        // tritri

        // BLAS_EX
        if(BUILD_WITH_TENSILE)
        {
            void*             alpha       = 0;
            void*             beta        = 0;
            float             alpha_float = 1.0;
            float             beta_float  = 1.0;
            rocblas_half      alpha_half(alpha_float);
            rocblas_half      beta_half(beta_float);
            double            alpha_double(alpha_float);
            double            beta_double(beta_float);
            rocblas_gemm_algo algo           = rocblas_gemm_algo_standard;
            int32_t           solution_index = 0;
            uint32_t          flags          = 0;
            rocblas_datatype  a_type;
            rocblas_datatype  b_type;
            rocblas_datatype  c_type;
            rocblas_datatype  d_type;
            rocblas_datatype  compute_type;

            if(std::is_same_v<T, rocblas_half>)
            {
                a_type       = rocblas_datatype_f16_r;
                b_type       = rocblas_datatype_f16_r;
                c_type       = rocblas_datatype_f16_r;
                d_type       = rocblas_datatype_f16_r;
                compute_type = rocblas_datatype_f16_r;
                alpha        = &alpha_half;
                beta         = &beta_half;
            }
            else if(std::is_same_v<T, float>)
            {
                a_type       = rocblas_datatype_f32_r;
                b_type       = rocblas_datatype_f32_r;
                c_type       = rocblas_datatype_f32_r;
                d_type       = rocblas_datatype_f32_r;
                compute_type = rocblas_datatype_f32_r;
                alpha        = &alpha_float;
                beta         = &beta_float;
            }
            else if(std::is_same_v<T, double>)
            {
                a_type       = rocblas_datatype_f64_r;
                b_type       = rocblas_datatype_f64_r;
                c_type       = rocblas_datatype_f64_r;
                d_type       = rocblas_datatype_f64_r;
                compute_type = rocblas_datatype_f64_r;
                alpha        = &alpha_double;
                beta         = &beta_double;
            }

            rocblas_gemm_ex(handle,
                            transA,
                            transB,
                            m,
                            n,
                            k,
                            alpha,
                            da,
                            a_type,
                            lda,
                            db,
                            b_type,
                            ldb,
                            beta,
                            dc,
                            c_type,
                            ldc,
                            dd,
                            d_type,
                            ldd,
                            compute_type,
                            algo,
                            solution_index,
                            flags);

            rocblas_gemm_strided_batched_ex(handle,
                                            transA,
                                            transB,
                                            m,
                                            n,
                                            k,
                                            alpha,
                                            da,
                                            a_type,
                                            lda,
                                            stride_a,
                                            db,
                                            b_type,
                                            ldb,
                                            stride_b,
                                            beta,
                                            dc,
                                            c_type,
                                            ldc,
                                            stride_c,
                                            dd,
                                            d_type,
                                            ldd,
                                            stride_d,
                                            batch_count,
                                            compute_type,
                                            algo,
                                            solution_index,
                                            flags);
        }
    }

    setenv_status = setenv("ROCBLAS_LAYER", "0", true);

#ifdef GOOGLE_TEST
    ASSERT_EQ(setenv_status, 0);
#endif

    //
    // write "golden file"
    //

    std::ofstream trace_ofs;
    std::ofstream bench_ofs;

    trace_ofs.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    trace_ofs.exceptions(std::ofstream::badbit | std::ofstream::failbit);

    trace_ofs.open(trace_path2);
    bench_ofs.open(bench_path2);

#ifdef GOOGLE_TEST
    ASSERT_FALSE(trace_ofs.fail());
    ASSERT_FALSE(trace_ofs.bad());
    ASSERT_FALSE(bench_ofs.fail());
    ASSERT_FALSE(bench_ofs.bad());
#endif

    rocblas_internal_ostream trace_ofs2;
    rocblas_internal_ostream bench_ofs2;

    // Auxiliary function
    trace_ofs2 << "rocblas_create_handle,atomics_allowed\n";
    trace_ofs2 << "rocblas_set_pointer_mode,0,atomics_allowed\n";
    trace_ofs2 << "rocblas_get_pointer_mode,0,atomics_allowed\n";

    // *************************************************** BLAS1 ***************************************************

    //
    // AMAX
    //
    trace_ofs2 << replaceX<T>("rocblas_iXamax") << "," << n << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";
    bench_ofs2 << "./rocblas-bench -f iamax -r " << rocblas_precision_string<T> << " -n " << n
               << " --incx " << incx << "\n";

    //
    // AMIN
    //
    trace_ofs2 << replaceX<T>("rocblas_iXamin") << "," << n << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";
    bench_ofs2 << "./rocblas-bench -f iamin -r " << rocblas_precision_string<T> << " -n " << n
               << " --incx " << incx << "\n";

    //
    // ASUM
    //
    trace_ofs2 << replaceX<T>("rocblas_Xasum") << "," << n << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";
    bench_ofs2 << "./rocblas-bench -f asum -r " << rocblas_precision_string<T> << " -n " << n
               << " --incx " << incx << "\n";

    //
    // AXPY
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xaxpy") << "," << n << "," << alpha << "," << (void*)dx
                   << "," << incx << "," << (void*)dy << "," << incy << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f axpy -r " << rocblas_precision_string<T> << " -n " << n
                   << " --alpha " << alpha << " --incx " << incx << " --incy " << incy << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xaxpy") << "," << n << "," << (void*)&alpha << ","
                   << (void*)dx << "," << incx << "," << (void*)dy << "," << incy
                   << ",atomics_allowed\n";
    }

    //
    // COPY
    //
    trace_ofs2 << replaceX<T>("rocblas_Xcopy") << "," << n << "," << (void*)dx << "," << incx << ","
               << (void*)dy << "," << incy << ",atomics_allowed\n";
    bench_ofs2 << "./rocblas-bench -f copy -r " << rocblas_precision_string<T> << " -n " << n
               << " --incx " << incx << " --incy " << incy << "\n";

    //
    // DOT
    //
    trace_ofs2 << replaceX<T>("rocblas_Xdot") << "," << n << "," << (void*)dx << "," << incx << ","
               << (void*)dy << "," << incy << ",atomics_allowed\n";
    bench_ofs2 << "./rocblas-bench -f dot -r " << rocblas_precision_string<T> << " -n " << n
               << " --incx " << incx << " --incy " << incy << "\n";

    //
    // NRM2
    //
    trace_ofs2 << replaceX<T>("rocblas_Xnrm2") << "," << n << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";
    bench_ofs2 << "./rocblas-bench -f nrm2 -r " << rocblas_precision_string<T> << " -n " << n
               << " --incx " << incx << "\n";

    //
    // SCAL
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xscal") << "," << n << "," << alpha << "," << (void*)dx
                   << "," << incx << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f scal --a_type "
                   << rocblas_precision_string<T> << " --b_type "
                   << rocblas_precision_string<T> << " -n " << n << " --alpha " << alpha
                   << " --incx " << incx << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xscal") << "," << n << "," << (void*)&alpha << ","
                   << (void*)dx << "," << incx << ",atomics_allowed\n";
    }

    //
    // SWAP
    //
    trace_ofs2 << replaceX<T>("rocblas_Xswap") << "," << n << "," << (void*)dx << "," << incx << ","
               << (void*)dy << "," << incy << ",atomics_allowed\n";

    bench_ofs2 << "./rocblas-bench -f swap -r " << rocblas_precision_string<T> << " -n " << n
               << " --incx " << incx << " --incy " << incy << "\n";

    // *************************************************** BLAS2 ***************************************************

    auto transA_letter = rocblas2char_operation(transA);
    auto transB_letter = rocblas2char_operation(transB);
    auto side_letter   = rocblas2char_side(side);
    auto uplo_letter   = rocblas2char_fill(uplo);
    auto diag_letter   = rocblas2char_diagonal(diag);

    //
    // GBMV
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xgbmv") << "," << transA << "," << m << "," << n << ","
                   << kl << "," << ku << "," << alpha << "," << (void*)da << "," << lda << ","
                   << (void*)dx << "," << incx << "," << beta << "," << (void*)dy << "," << incy
                   << ",atomics_allowed\n";

        bench_ofs2 << "./rocblas-bench -f gbmv -r "
                   << rocblas_precision_string<T> << " --transposeA " << transA_letter << " -m "
                   << m << " -n " << n << " --kl " << kl << " --ku " << ku << " --alpha " << alpha
                   << " --lda " << lda << " --incx " << incx << " --beta " << beta << " --incy "
                   << incy << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xgbmv") << "," << transA << "," << m << "," << n << ","
                   << kl << "," << ku << "," << (void*)&alpha << "," << (void*)da << "," << lda
                   << "," << (void*)dx << "," << incx << "," << (void*)&beta << "," << (void*)dy
                   << "," << incy << ",atomics_allowed\n";
    }

    //
    // GEMV
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xgemv") << "," << transA << "," << m << "," << n << ","
                   << alpha << "," << (void*)da << "," << lda << "," << (void*)dx << "," << incx
                   << "," << beta << "," << (void*)dy << "," << incy << ",atomics_allowed\n";

        bench_ofs2 << "./rocblas-bench -f gemv -r "
                   << rocblas_precision_string<T> << " --transposeA " << transA_letter << " -m "
                   << m << " -n " << n << " --alpha " << alpha << " --lda " << lda << " --incx "
                   << incx << " --beta " << beta << " --incy " << incy << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xgemv") << "," << transA << "," << m << "," << n << ","
                   << (void*)&alpha << "," << (void*)da << "," << lda << "," << (void*)dx << ","
                   << incx << "," << (void*)&beta << "," << (void*)dy << "," << incy
                   << ",atomics_allowed\n";
    }

    //
    // GER
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xger") << "," << m << "," << n << "," << alpha << ","
                   << (void*)dx << "," << incx << "," << (void*)dy << "," << incy << ","
                   << (void*)da << "," << lda << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f ger -r " << rocblas_precision_string<T> << " -m " << m
                   << " -n " << n << " --alpha " << alpha << " --incx " << incx << " --incy "
                   << incy << " --lda " << lda << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xger") << "," << m << "," << n << "," << (void*)&alpha
                   << "," << (void*)dx << "," << incx << "," << (void*)dy << "," << incy << ","
                   << (void*)da << "," << lda << ",atomics_allowed\n";
    }

    //
    // SBMV
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xsbmv") << "," << uplo << "," << n << "," << k << ","
                   << alpha << "," << (void*)da << "," << lda << "," << (void*)dx << "," << incx
                   << "," << beta << "," << (void*)dy << "," << incy << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f sbmv -r " << rocblas_precision_string<T> << " --uplo "
                   << uplo_letter << " -n " << n << " -k " << k << " --alpha " << alpha << " --lda "
                   << lda << " --incx " << incx << " --beta " << beta << " --incy " << incy << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xsbmv") << "," << uplo << "," << n << "," << k << ","
                   << (void*)&alpha << "," << (void*)da << "," << lda << "," << (void*)dx << ","
                   << incx << "," << (void*)&beta << "," << (void*)dy << "," << incy
                   << ",atomics_allowed\n";
    }

    //
    // SPMV
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xspmv") << "," << uplo << "," << n << "," << alpha << ","
                   << (void*)da << "," << (void*)dx << "," << incx << "," << beta << ","
                   << (void*)dy << "," << incy << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f spmv -r " << rocblas_precision_string<T> << " --uplo "
                   << uplo_letter << " -n " << n << " --alpha " << alpha << " --incx " << incx
                   << " --beta " << beta << " --incy " << incy << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xspmv") << "," << uplo << "," << n << ","
                   << (void*)&alpha << "," << (void*)da << "," << (void*)dx << "," << incx << ","
                   << (void*)&beta << "," << (void*)dy << "," << incy << ",atomics_allowed\n";
    }

    //
    // SPR
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xspr") << "," << uplo << "," << n << "," << alpha << ","
                   << (void*)dx << "," << incx << "," << (void*)da << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f spr -r " << rocblas_precision_string<T> << " --uplo "
                   << uplo_letter << " -n " << n << " --alpha " << alpha << " --incx " << incx
                   << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xspr") << "," << uplo << "," << n << "," << (void*)&alpha
                   << "," << (void*)dx << "," << incx << "," << (void*)da << ",atomics_allowed\n";
    }

    //
    // SPR2
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xspr2") << "," << uplo << "," << n << "," << alpha << ","
                   << (void*)dx << "," << incx << "," << (void*)dy << "," << incy << ","
                   << (void*)da << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f spr2 -r " << rocblas_precision_string<T> << " --uplo "
                   << uplo_letter << " -n " << n << " --alpha " << alpha << " --incx " << incx
                   << " --incy " << incy << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xspr2") << "," << uplo << "," << n << ","
                   << (void*)&alpha << "," << (void*)dx << "," << incx << "," << (void*)dy << ","
                   << incy << "," << (void*)da << ",atomics_allowed\n";
    }

    //
    // SYMV
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xsymv") << "," << uplo << "," << n << "," << alpha << ","
                   << (void*)da << "," << lda << "," << (void*)dx << "," << incx << "," << beta
                   << "," << (void*)dy << "," << incy << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f symv -r " << rocblas_precision_string<T> << " --uplo "
                   << uplo_letter << " -n " << n << " --alpha " << alpha << " --lda " << lda
                   << " --incx " << incx << " --beta " << beta << " --incy " << incy << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xsymv") << "," << uplo << "," << n << ","
                   << (void*)&alpha << "," << (void*)da << "," << lda << "," << (void*)dx << ","
                   << incx << "," << (void*)&beta << "," << (void*)dy << "," << incy
                   << ",atomics_allowed\n";
    }

    //
    // SYR
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xsyr") << "," << uplo << "," << n << "," << alpha << ","
                   << (void*)dx << "," << incx << "," << (void*)da << "," << lda
                   << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f syr -r " << rocblas_precision_string<T> << " --uplo "
                   << uplo_letter << " -n " << n << " --alpha " << alpha << " --incx " << incx
                   << " --lda " << lda << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xsyr") << "," << uplo << "," << n << "," << (void*)&alpha
                   << "," << (void*)dx << "," << incx << "," << (void*)da << "," << lda
                   << ",atomics_allowed\n";
    }

    //
    // SYR2
    //
    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
        trace_ofs2 << replaceX<T>("rocblas_Xsyr2") << "," << uplo << "," << n << "," << alpha << ","
                   << (void*)dx << "," << incx << "," << (void*)dy << "," << incy << ","
                   << (void*)da << "," << lda << ",atomics_allowed\n";
        bench_ofs2 << "./rocblas-bench -f syr2 -r " << rocblas_precision_string<T> << " --uplo "
                   << uplo_letter << " -n " << n << " --alpha " << alpha << " --lda " << lda
                   << " --incx " << incx << " --incy " << incy << "\n";
    }
    else
    {
        trace_ofs2 << replaceX<T>("rocblas_Xsyr2") << "," << uplo << "," << n << ","
                   << (void*)&alpha << "," << (void*)dx << "," << incx << "," << (void*)dy << ","
                   << incy << (void*)da << "," << lda << ",atomics_allowed\n";
    }

    //
    // TBMV
    //
    trace_ofs2 << replaceX<T>("rocblas_Xtbmv") << "," << uplo << "," << transA << "," << diag << ","
               << n << "," << k << "," << (void*)da << "," << lda << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";

    bench_ofs2 << "./rocblas-bench -f tbmv -r " << rocblas_precision_string<T> << " --uplo "
               << uplo_letter << " --transposeA " << transA_letter << " --diag " << diag_letter
               << " -n " << n << " -k " << k << " --lda " << lda << " --incx " << incx << "\n";

    //
    // TBSV
    //
    trace_ofs2 << replaceX<T>("rocblas_Xtbsv") << "," << uplo << "," << transA << "," << diag << ","
               << n << "," << k << "," << (void*)da << "," << lda << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";

    bench_ofs2 << "./rocblas-bench -f tbsv -r " << rocblas_precision_string<T> << " --uplo "
               << uplo_letter << " --transposeA " << transA_letter << " --diag " << diag_letter
               << " -n " << n << " -k " << k << " --lda " << lda << " --incx " << incx << "\n";

    //
    // TRMV
    //
    trace_ofs2 << replaceX<T>("rocblas_Xtrmv") << "," << uplo << "," << transA << "," << diag << ","
               << n << "," << (void*)da << "," << lda << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";

    bench_ofs2 << "./rocblas-bench -f trmv -r " << rocblas_precision_string<T> << " --uplo "
               << uplo_letter << " --transposeA " << transA_letter << " --diag " << diag_letter
               << " -n " << n << " --lda " << lda << " --incx " << incx << "\n";

    //
    // TPMV
    //
    trace_ofs2 << replaceX<T>("rocblas_Xtpmv") << "," << uplo << "," << transA << "," << diag << ","
               << n << "," << (void*)da << "," << (void*)dx << "," << incx << ",atomics_allowed\n";

    bench_ofs2 << "./rocblas-bench -f tpmv -r " << rocblas_precision_string<T> << " --uplo "
               << uplo_letter << " --transposeA " << transA_letter << " --diag " << diag_letter
               << " -n " << n << " --incx " << incx << "\n";

    //
    // TPSV
    //
    trace_ofs2 << replaceX<T>("rocblas_Xtpsv") << "," << uplo << "," << transA << "," << diag << ","
               << n << "," << (void*)da << "," << (void*)dx << "," << incx << ",atomics_allowed\n";

    bench_ofs2 << "./rocblas-bench -f tpsv -r " << rocblas_precision_string<T> << " --uplo "
               << uplo_letter << " --transposeA " << transA_letter << " --diag " << diag_letter
               << " -n " << n << " --incx " << incx << "\n";

    //
    // TRMV
    //
    trace_ofs2 << replaceX<T>("rocblas_Xtrmv") << "," << uplo << "," << transA << "," << diag << ","
               << n << "," << (void*)da << "," << lda << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";

    bench_ofs2 << "./rocblas-bench -f trmv -r " << rocblas_precision_string<T> << " --uplo "
               << uplo_letter << " --transposeA " << transA_letter << " --diag " << diag_letter
               << " -n " << n << " --lda " << lda << " --incx " << incx << "\n";

    //
    // TRSV
    //
    trace_ofs2 << replaceX<T>("rocblas_Xtrsv") << "," << uplo << "," << transA << "," << diag << ","
               << n << "," << (void*)da << "," << lda << "," << (void*)dx << "," << incx
               << ",atomics_allowed\n";

    bench_ofs2 << "./rocblas-bench -f trsv -r " << rocblas_precision_string<T> << " --uplo "
               << uplo_letter << " --transposeA " << transA_letter << " --diag " << diag_letter
               << " -n " << n << " --lda " << lda << " --incx " << incx << "\n";

    if(BUILD_WITH_TENSILE)
    {
        // *************************************************** BLAS3 ***************************************************

        //
        // GEAM
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xgeam") << "," << transA << "," << transB << "," << m
                       << "," << n << "," << alpha << "," << (void*)da << "," << lda << "," << beta
                       << "," << (void*)db << "," << ldb << "," << (void*)dc << "," << ldc
                       << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f geam -r "
                       << rocblas_precision_string<T> << " --transposeA " << transA_letter
                       << " --transposeB " << transB_letter << " -m " << m << " -n " << n
                       << " --alpha " << alpha << " --lda " << lda << " --beta " << beta
                       << " --ldb " << ldb << " --ldc " << ldc << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xgeam") << "," << transA << "," << transB << "," << m
                       << "," << n << "," << (void*)&alpha << "," << (void*)da << "," << lda << ","
                       << (void*)&beta << "," << (void*)db << "," << ldb << "," << (void*)dc << ","
                       << ldc << ",atomics_allowed\n";
        }

        //
        // GEMM
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xgemm") << "," << transA << "," << transB << "," << m
                       << "," << n << "," << k << "," << alpha << "," << (void*)da << "," << lda
                       << "," << (void*)db << "," << ldb << "," << beta << "," << (void*)dc << ","
                       << ldc << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f gemm -r "
                       << rocblas_precision_string<T> << " --transposeA " << transA_letter
                       << " --transposeB " << transB_letter << " -m " << m << " -n " << n << " -k "
                       << k << " --alpha " << alpha << " --lda " << lda << " --ldb " << ldb
                       << " --beta " << beta << " --ldc " << ldc << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xgemm") << "," << transA << "," << transB << "," << m
                       << "," << n << "," << k << "," << (void*)&alpha << "," << (void*)da << ","
                       << lda << "," << (void*)db << "," << ldb << "," << (void*)&beta << ","
                       << (void*)dc << "," << ldc << ",atomics_allowed\n";
        }

        //
        // SYMM
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xsymm") << "," << side << "," << uplo << "," << m
                       << "," << n << "," << alpha << "," << (void*)da << "," << lda << ","
                       << (void*)db << "," << ldb << "," << beta << "," << (void*)dc << "," << ldc
                       << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f symm -r " << rocblas_precision_string<T> << " --side "
                       << side_letter << " --uplo " << uplo_letter << " -m " << m << " -n " << n
                       << " --alpha " << alpha << " --lda " << lda << " --ldb " << ldb << " --beta "
                       << beta << " --ldc " << ldc << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xsymm") << "," << side << "," << uplo << "," << m
                       << "," << n << "," << (void*)&alpha << "," << (void*)da << "," << lda << ","
                       << (void*)db << "," << ldb << "," << (void*)&beta << "," << (void*)dc << ","
                       << ldc << ",atomics_allowed\n";
        }

        //
        // SYRK
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xsyrk") << "," << uplo << "," << transA << "," << n
                       << "," << k << "," << alpha << "," << (void*)da << "," << lda << "," << beta
                       << "," << (void*)dc << "," << ldc << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f syrk -r " << rocblas_precision_string<T> << " --uplo "
                       << uplo_letter << " --transposeA " << transA_letter << " -n " << n << " -k "
                       << k << " --alpha " << alpha << " --lda " << lda << " --beta " << beta
                       << " --ldc " << ldc << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xsyrk") << "," << uplo << "," << transA << "," << n
                       << "," << k << "," << (void*)&alpha << "," << (void*)da << "," << lda << ","
                       << (void*)&beta << "," << (void*)dc << "," << ldc << ",atomics_allowed\n";
        }

        //
        // SYR2K
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xsyr2k") << "," << uplo << "," << transA << "," << n
                       << "," << k << "," << alpha << "," << (void*)da << "," << lda << ","
                       << (void*)db << "," << ldb << "," << beta << "," << (void*)dc << "," << ldc
                       << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f syr2k -r "
                       << rocblas_precision_string<T> << " --uplo " << uplo_letter
                       << " --transposeA " << transA_letter << " -n " << n << " -k " << k
                       << " --alpha " << alpha << " --lda " << lda << " --ldb " << ldb << " --beta "
                       << beta << " --ldc " << ldc << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xsyr2k") << "," << uplo << "," << transA << "," << n
                       << "," << k << "," << (void*)&alpha << "," << (void*)da << "," << lda << ","
                       << (void*)db << "," << ldb << "," << (void*)&beta << "," << (void*)dc << ","
                       << ldc << ",atomics_allowed\n";
        }

        //
        // SYRKX
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xsyrkx") << "," << uplo << "," << transA << "," << n
                       << "," << k << "," << alpha << "," << (void*)da << "," << lda << ","
                       << (void*)db << "," << ldb << "," << beta << "," << (void*)dc << "," << ldc
                       << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f syrkx -r "
                       << rocblas_precision_string<T> << " --uplo " << uplo_letter
                       << " --transposeA " << transA_letter << " -n " << n << " -k " << k
                       << " --alpha " << alpha << " --lda " << lda << " --ldb " << ldb << " --beta "
                       << beta << " --ldc " << ldc << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xsyrkx") << "," << uplo << "," << transA << "," << n
                       << "," << k << "," << (void*)&alpha << "," << (void*)da << "," << lda << ","
                       << (void*)db << "," << ldb << "," << (void*)&beta << "," << (void*)dc << ","
                       << ldc << ",atomics_allowed\n";
        }

        //
        // TRMM
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xtrmm") << "," << side << "," << uplo << ","
                       << transA << "," << diag << "," << m << "," << n << "," << alpha << ","
                       << (void*)da << "," << lda << "," << (void*)db << "," << ldb << ","
                       << (void*)dc << "," << ldc << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f trmm -r " << rocblas_precision_string<T> << " --side "
                       << side_letter << " --uplo " << uplo_letter << " --transposeA "
                       << transA_letter << " --diag " << diag_letter << " -m " << m << " -n " << n
                       << " --alpha " << alpha << " --lda " << lda << " --ldb " << ldb << " --ldc "
                       << ldc << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xtrmm") << "," << side << "," << uplo << ","
                       << transA << "," << diag << "," << m << "," << n << "," << (void*)&alpha
                       << "," << (void*)da << "," << lda << "," << (void*)db << "," << ldb << ","
                       << (void*)dc << "," << ldc << ",atomics_allowed\n";
        }

        //
        // TRSM
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xtrsm") << "," << side << "," << uplo << ","
                       << transA << "," << diag << "," << m << "," << n << "," << alpha << ","
                       << (void*)da << "," << lda << "," << (void*)db << "," << ldb
                       << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f trsm -r " << rocblas_precision_string<T> << " --side "
                       << side_letter << " --uplo " << uplo_letter << " --transposeA "
                       << transA_letter << " --diag " << diag_letter << " -m " << m << " -n " << n
                       << " --alpha " << alpha << " --lda " << lda << " --ldb " << ldb << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xtrsm") << "," << side << "," << uplo << ","
                       << transA << "," << diag << "," << m << "," << n << "," << (void*)&alpha
                       << "," << (void*)da << "," << lda << "," << (void*)db << "," << ldb
                       << ",atomics_allowed\n";
        }

        //
        // GEMM_STRIDED_BATCHED
        //
        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            trace_ofs2 << replaceX<T>("rocblas_Xgemm_strided_batched") << "," << transA << ","
                       << transB << "," << m << "," << n << "," << k << "," << alpha << ","
                       << (void*)da << "," << lda << "," << stride_a << "," << (void*)db << ","
                       << ldb << "," << stride_b << "," << beta << "," << (void*)dc << "," << ldc
                       << "," << stride_c << "," << batch_count << ",atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f gemm_strided_batched -r "
                       << rocblas_precision_string<T> << " --transposeA " << transA_letter
                       << " --transposeB " << transB_letter << " -m " << m << " -n " << n << " -k "
                       << k << " --alpha " << alpha << " --lda " << lda << " --stride_a "
                       << stride_a << " --ldb " << ldb << " --stride_b " << stride_b << " --beta "
                       << beta << " --ldc " << ldc << " --stride_c " << stride_c
                       << " --batch_count " << batch_count << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xgemm_strided_batched") << "," << transA << ","
                       << transB << "," << m << "," << n << "," << k << "," << (void*)&alpha << ","
                       << (void*)da << "," << lda << "," << stride_a << "," << (void*)db << ","
                       << ldb << "," << stride_b << "," << (void*)&beta << "," << (void*)dc << ","
                       << ldc << "," << stride_c << "," << batch_count << ",atomics_allowed\n";
        }

        if(test_pointer_mode == rocblas_pointer_mode_host)
        {
            rocblas_datatype a_type, b_type, c_type, d_type, compute_type;

            if(std::is_same_v<T, rocblas_half>)
            {
                a_type       = rocblas_datatype_f16_r;
                b_type       = rocblas_datatype_f16_r;
                c_type       = rocblas_datatype_f16_r;
                d_type       = rocblas_datatype_f16_r;
                compute_type = rocblas_datatype_f16_r;
            }
            else if(std::is_same_v<T, float>)
            {
                a_type       = rocblas_datatype_f32_r;
                b_type       = rocblas_datatype_f32_r;
                c_type       = rocblas_datatype_f32_r;
                d_type       = rocblas_datatype_f32_r;
                compute_type = rocblas_datatype_f32_r;
            }
            if(std::is_same_v<T, double>)
            {
                a_type       = rocblas_datatype_f64_r;
                b_type       = rocblas_datatype_f64_r;
                c_type       = rocblas_datatype_f64_r;
                d_type       = rocblas_datatype_f64_r;
                compute_type = rocblas_datatype_f64_r;
            }

            rocblas_gemm_algo algo           = rocblas_gemm_algo_standard;
            int32_t           solution_index = 0;
            uint32_t          flags          = 0;

            //
            //GEMM_EX
            //
            trace_ofs2 << "rocblas_gemm_ex"
                       << "," << transA << "," << transB << "," << m << "," << n << "," << k << ","
                       << alpha << "," << (void*)da << "," << rocblas_datatype_string(a_type) << ","
                       << lda << "," << (void*)db << "," << rocblas_datatype_string(b_type) << ","
                       << ldb << "," << beta << "," << (void*)dc << ","
                       << rocblas_datatype_string(c_type) << "," << ldc << "," << (void*)dd << ","
                       << rocblas_datatype_string(d_type) << "," << ldd << ","
                       << rocblas_datatype_string(compute_type) << "," << algo << ","
                       << solution_index << ",none,atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f gemm_ex"
                       << " --transposeA " << transA_letter << " --transposeB " << transB_letter
                       << " -m " << m << " -n " << n << " -k " << k << " --alpha " << alpha
                       << " --a_type " << rocblas_datatype_string(a_type) << " --lda " << lda
                       << " --b_type " << rocblas_datatype_string(b_type) << " --ldb " << ldb
                       << " --beta " << beta << " --c_type " << rocblas_datatype_string(c_type)
                       << " --ldc " << ldc << " --d_type " << rocblas_datatype_string(d_type)
                       << " --ldd " << ldd << " --compute_type "
                       << rocblas_datatype_string(compute_type) << " --algo " << algo
                       << " --solution_index " << solution_index << " --flags " << flags << "\n";

            //
            //GEMM_STRIDED_BATCHED_EX
            //
            trace_ofs2 << "rocblas_gemm_strided_batched_ex"
                       << "," << transA << "," << transB << "," << m << "," << n << "," << k << ","
                       << alpha << "," << (void*)da << "," << rocblas_datatype_string(a_type) << ","
                       << lda << "," << stride_a << "," << (void*)db << ","
                       << rocblas_datatype_string(b_type) << "," << ldb << "," << stride_b << ","
                       << beta << "," << (void*)dc << "," << rocblas_datatype_string(c_type) << ","
                       << ldc << "," << stride_c << "," << (void*)dd << ","
                       << rocblas_datatype_string(d_type) << "," << ldd << "," << stride_d << ","
                       << batch_count << "," << rocblas_datatype_string(compute_type) << "," << algo
                       << "," << solution_index << ",none,atomics_allowed\n";

            bench_ofs2 << "./rocblas-bench -f gemm_strided_batched_ex"
                       << " --transposeA " << transA_letter << " --transposeB " << transB_letter
                       << " -m " << m << " -n " << n << " -k " << k << " --alpha " << alpha
                       << " --a_type " << rocblas_datatype_string(a_type) << " --lda " << lda
                       << " --stride_a " << stride_a << " --b_type "
                       << rocblas_datatype_string(b_type) << " --ldb " << ldb << " --stride_b "
                       << stride_b << " --beta " << beta << " --c_type "
                       << rocblas_datatype_string(c_type) << " --ldc " << ldc << " --stride_c "
                       << stride_c << " --d_type " << rocblas_datatype_string(d_type) << " --ldd "
                       << ldd << " --stride_d " << stride_d << " --batch_count " << batch_count
                       << " --compute_type " << rocblas_datatype_string(compute_type) << " --algo "
                       << algo << " --solution_index " << solution_index << " --flags " << flags
                       << "\n";
        }
        else
        {
            trace_ofs2 << replaceX<T>("rocblas_Xgemm") << "," << transA << "," << transB << "," << m
                       << "," << n << "," << k << "," << (void*)&alpha << "," << (void*)da << ","
                       << lda << "," << (void*)db << "," << ldb << "," << (void*)&beta << ","
                       << (void*)dc << "," << ldc << ",atomics_allowed\n";
        }
    }
    // exclude trtri as it is an internal function
    //  trace_ofs2 << "\n" << replaceX<T>("rocblas_Xtrtri")  << "," << uplo << "," << diag << "," <<
    //  n
    //  << "," << (void*)da << "," << lda << "," << (void*)db << "," << ldb;

    // Auxiliary function
    trace_ofs2 << "rocblas_destroy_handle,atomics_allowed\n";

    // Flush the streams
    trace_ofs2.flush();
    bench_ofs2.flush();

    // Transfer the formatted output to the files
    trace_ofs << trace_ofs2;
    bench_ofs << bench_ofs2;

    // Flush the streams
    trace_ofs.flush();
    bench_ofs.flush();

    // Close the files
    trace_ofs.close();
    bench_ofs.close();

    //
    // check if rocBLAS output files same as "golden files"
    //
#ifdef WIN32
    // need all file descriptors closed to allow file removal on windows before process exits
    rocblas_internal_ostream::clear_workers();

    int trace_cmp = diff_files(trace_path1, trace_path2);
    //int trace_cmp = system(("fc.exe \"" + trace_path1 + "\" \"" + trace_path2 + "\" | findstr *****").c_str());
#else
    int trace_cmp = system(("/usr/bin/diff " + trace_path1 + " " + trace_path2).c_str());
#endif

#ifdef GOOGLE_TEST
    ASSERT_EQ(trace_cmp, 0);
#endif

    if(!trace_cmp)
    {
        fs::remove(trace_fspath1);
        fs::remove(trace_fspath2);
    }

    if(test_pointer_mode == rocblas_pointer_mode_host)
    {
#ifdef WIN32
        int bench_cmp = diff_files(bench_path1, bench_path2);
        //int bench_cmp = system(("fc.exe \"" + bench_path1 + "\" \"" + bench_path2 + "\" | findstr *****").c_str());
#else
        int bench_cmp = system(("/usr/bin/diff " + bench_path1 + " " + bench_path2).c_str());
#endif

#ifdef GOOGLE_TEST
        ASSERT_EQ(bench_cmp, 0);
#endif

        if(!bench_cmp)
        {
            fs::remove(bench_fspath1);
            fs::remove(bench_fspath2);
        }
    }
}
