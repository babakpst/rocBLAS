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

#include "check_numerics_vector.hpp"
#include "handle.hpp"
#include "rocblas_hbmv.hpp"

/**
  *  Helper for the non-transpose case. Iterates through each diagonal
  *  and creates partial sums for each ty.
  */
template <rocblas_int DIM_Y, typename T>
__device__ T rocblas_hbmvn_kernel_helper(rocblas_int ty,
                                         rocblas_int ind,
                                         bool        is_upper,
                                         rocblas_int m,
                                         rocblas_int k,
                                         const T*    A,
                                         rocblas_int lda,
                                         const T*    x,
                                         rocblas_int incx)
{
    T           res_A = 0.0;
    rocblas_int col;

    // Since the column is consistent, we can iterate up the diagonal
    // ty defines the column of banded & regular matrix
    for(col = ty; col < m; col += DIM_Y)
    {
        // We have to convert ind to banded matrix row
        rocblas_int row = is_upper ? ind + (k - col) : ind - col;

        if(ind < m)
        {
            if((ind <= col && is_upper) || (ind >= col && !is_upper))
            {
                // in is_upper/lower triangular part
                if(row < k && row > 0)
                {
                    // not on main diagonal, simply multiply
                    res_A += (A[row + col * size_t(lda)] * x[col * int64_t(incx)]);
                }
                else if(row == 0)
                {
                    // cppcheck-suppress knownConditionTrueFalse
                    // If main diagonal, assume 0 imaginary part.
                    if(!is_upper || (k == 0 && is_upper))
                        res_A += (std::real(A[row + col * size_t(lda)]) * x[col * int64_t(incx)]);
                    else
                        res_A += (A[row + col * size_t(lda)] * x[col * int64_t(incx)]);
                }
                else if(row == k)
                {
                    // If main diagonal, assume 0 imaginary part.
                    if(is_upper)
                        res_A += (std::real(A[row + col * size_t(lda)]) * x[col * int64_t(incx)]);
                    else
                        res_A += (A[row + col * size_t(lda)] * x[col * int64_t(incx)]);
                }
            }
            else
            {
                // in the opposite triangle, get conjugate of value at transposed position
                rocblas_int trans_row = col;
                rocblas_int trans_col = ind;
                trans_row = is_upper ? trans_row + (k - trans_col) : trans_row - trans_col;
                if(trans_row <= k && trans_row >= 0)
                {
                    res_A
                        += (conj(A[trans_row + trans_col * size_t(lda)]) * x[col * int64_t(incx)]);
                }
            }
        }
    }
    return res_A;
}

/**
  *  Computes y := alpha*A*x + beta*y where A is a Hermitian matrix.
  *  If uplo == upper, the strictly lower part of A is not referenced,
  *  if uplo == lower, the strictly upper part of A is not referenced.
  *  The imaginary part of the main diagonal is assumed to always be == 0.
  */
template <rocblas_int DIM_X, rocblas_int DIM_Y, typename T>
__device__ void rocblas_hbmvn_kernel_calc(bool        is_upper,
                                          rocblas_int n,
                                          rocblas_int k,
                                          T           alpha,
                                          const T*    A,
                                          rocblas_int lda,
                                          const T*    x,
                                          rocblas_int incx,
                                          T           beta,
                                          T*          y,
                                          rocblas_int incy)
{
    rocblas_int  thread_id = threadIdx.x + threadIdx.y * blockDim.x;
    __shared__ T sdata[DIM_X * DIM_Y];

    if(alpha)
    {
        // threads are all configurated locally
        rocblas_int ty  = thread_id / DIM_X;
        rocblas_int tx  = thread_id % DIM_X;
        rocblas_int ind = blockIdx.x * DIM_X + tx;
        sdata[tx + ty * DIM_X]
            = rocblas_hbmvn_kernel_helper<DIM_Y>(ty, ind, is_upper, n, k, A, lda, x, incx);
        __syncthreads();
    }

    if(thread_id < DIM_X)
    {
        rocblas_int ind = blockIdx.x * DIM_X + thread_id;

        if(alpha)
        {
            for(rocblas_int i = 1; i < DIM_Y; i++)
                sdata[thread_id] += sdata[thread_id + DIM_X * i];

            if(ind < n)
                y[ind * int64_t(incy)]
                    = beta ? alpha * sdata[thread_id] + beta * y[ind * int64_t(incy)]
                           : alpha * sdata[thread_id];
        }
        else
        {
            if(ind < n)
                y[ind * int64_t(incy)] = beta ? y[ind * int64_t(incy)] * beta : 0;
        }
    }
}

/**
  *  U is either: const T* OR T
  *  V is either: const T* OR const T* const*
  *  W is either:       T* OR       T* const*
  */
template <rocblas_int DIM_X, rocblas_int DIM_Y, typename U, typename V, typename W>
ROCBLAS_KERNEL(DIM_X* DIM_Y)
rocblas_hbmvn_kernel(bool           is_upper,
                     rocblas_int    n,
                     rocblas_int    k,
                     U              alpha_device_host,
                     V              Aa,
                     rocblas_stride shifta,
                     rocblas_int    lda,
                     rocblas_stride strideA,
                     V              xa,
                     rocblas_stride shiftx,
                     rocblas_int    incx,
                     rocblas_stride stridex,
                     U              beta_device_host,
                     W              ya,
                     rocblas_stride shifty,
                     rocblas_int    incy,
                     rocblas_stride stridey)
{
    rocblas_int num_threads = blockDim.x * blockDim.y * blockDim.z;
    if(DIM_X * DIM_Y != num_threads)
        return; // need to launch exactly the same number of threads as template parameters indicate

    auto alpha = load_scalar(alpha_device_host);
    auto beta  = load_scalar(beta_device_host);

    if(!alpha && beta == 1)
        return;

    const auto* A = cond_load_ptr_batch(alpha, Aa, blockIdx.y, shifta, strideA);
    const auto* x = cond_load_ptr_batch(alpha, xa, blockIdx.y, shiftx, stridex);

    auto* y = load_ptr_batch(ya, blockIdx.y, shifty, stridey);

    rocblas_hbmvn_kernel_calc<DIM_X, DIM_Y>(is_upper, n, k, alpha, A, lda, x, incx, beta, y, incy);
}

/**
  *  U is always: const T* (either host or device)
  *  V is either: const T* OR const T* const*
  *  W is either:       T* OR       T* const*
  */
template <typename U, typename V, typename W>
rocblas_status rocblas_hbmv_template(rocblas_handle handle,
                                     rocblas_fill   uplo,
                                     rocblas_int    n,
                                     rocblas_int    k,
                                     U              alpha,
                                     V              A,
                                     rocblas_stride offseta,
                                     rocblas_int    lda,
                                     rocblas_stride strideA,
                                     V              x,
                                     rocblas_stride offsetx,
                                     rocblas_int    incx,
                                     rocblas_stride stridex,
                                     U              beta,
                                     W              y,
                                     rocblas_stride offsety,
                                     rocblas_int    incy,
                                     rocblas_stride stridey,
                                     rocblas_int    batch_count)
{
    //quick return
    if(!n || !batch_count)
        return rocblas_status_success;

    hipStream_t rocblas_stream = handle->get_stream();

    // in case of negative inc shift pointer to end of data for negative indexing tid*inc
    auto shiftx = incx < 0 ? offsetx - int64_t(incx) * (n - 1) : offsetx;
    auto shifty = incy < 0 ? offsety - int64_t(incy) * (n - 1) : offsety;

    // hbmvN_DIM_Y must be at least 4, 8 * 8 is very slow only 40Gflop/s
    static constexpr int hbmvN_DIM_X = 64;
    static constexpr int hbmvN_DIM_Y = 16;
    rocblas_int          blocks      = (n - 1) / (hbmvN_DIM_X) + 1;
    dim3                 hbmvn_grid(blocks, batch_count);
    dim3                 hbmvn_threads(hbmvN_DIM_X, hbmvN_DIM_Y);

    if(handle->pointer_mode == rocblas_pointer_mode_device)
    {
        ROCBLAS_LAUNCH_KERNEL((rocblas_hbmvn_kernel<hbmvN_DIM_X, hbmvN_DIM_Y>),
                              hbmvn_grid,
                              hbmvn_threads,
                              0,
                              rocblas_stream,
                              uplo == rocblas_fill_upper,
                              n,
                              k,
                              alpha,
                              A,
                              offseta,
                              lda,
                              strideA,
                              x,
                              shiftx,
                              incx,
                              stridex,
                              beta,
                              y,
                              shifty,
                              incy,
                              stridey);
    }
    else
    {
        if(!*alpha && *beta == 1)
            return rocblas_status_success;

        ROCBLAS_LAUNCH_KERNEL((rocblas_hbmvn_kernel<hbmvN_DIM_X, hbmvN_DIM_Y>),
                              hbmvn_grid,
                              hbmvn_threads,
                              0,
                              rocblas_stream,
                              uplo == rocblas_fill_upper,
                              n,
                              k,
                              *alpha,
                              A,
                              offseta,
                              lda,
                              strideA,
                              x,
                              shiftx,
                              incx,
                              stridex,
                              *beta,
                              y,
                              shifty,
                              incy,
                              stridey);
    }

    return rocblas_status_success;
}

//TODO :-Add rocblas_check_numerics_hb_matrix_template for checking Matrix `A` which is a Hermitian Band matrix
template <typename T, typename U>
rocblas_status rocblas_hbmv_check_numerics(const char*    function_name,
                                           rocblas_handle handle,
                                           rocblas_int    n,
                                           rocblas_int    k,
                                           T              A,
                                           rocblas_stride offset_a,
                                           rocblas_int    lda,
                                           rocblas_stride stride_a,
                                           T              x,
                                           rocblas_stride offset_x,
                                           rocblas_int    inc_x,
                                           rocblas_stride stride_x,
                                           U              y,
                                           rocblas_stride offset_y,
                                           rocblas_int    inc_y,
                                           rocblas_stride stride_y,
                                           rocblas_int    batch_count,
                                           const int      check_numerics,
                                           bool           is_input)
{
    rocblas_status check_numerics_status
        = rocblas_internal_check_numerics_vector_template(function_name,
                                                          handle,
                                                          n,
                                                          x,
                                                          offset_x,
                                                          inc_x,
                                                          stride_x,
                                                          batch_count,
                                                          check_numerics,
                                                          is_input);
    if(check_numerics_status != rocblas_status_success)
        return check_numerics_status;

    check_numerics_status = rocblas_internal_check_numerics_vector_template(function_name,
                                                                            handle,
                                                                            n,
                                                                            y,
                                                                            offset_y,
                                                                            inc_y,
                                                                            stride_y,
                                                                            batch_count,
                                                                            check_numerics,
                                                                            is_input);

    return check_numerics_status;
}

// Instantiations below will need to be manually updated to match any change in
// template parameters in the files *hbmv*.cpp

// clang-format off

#ifdef INSTANTIATE_HBMV_TEMPLATE
#error INSTANTIATE_HBMV_TEMPLATE already defined
#endif

#define INSTANTIATE_HBMV_TEMPLATE(U_, V_, W_)                    \
template rocblas_status rocblas_hbmv_template<U_, V_, W_>        \
                                    (rocblas_handle handle,      \
                                     rocblas_fill   uplo,        \
                                     rocblas_int    n,           \
                                     rocblas_int    k,           \
                                     U_              alpha,      \
                                     V_              A,          \
                                     rocblas_stride offseta,     \
                                     rocblas_int    lda,         \
                                     rocblas_stride strideA,     \
                                     V_              x,          \
                                     rocblas_stride offsetx,     \
                                     rocblas_int    incx,        \
                                     rocblas_stride stridex,     \
                                     U_              beta,       \
                                     W_              y,          \
                                     rocblas_stride offsety,     \
                                     rocblas_int    incy,        \
                                     rocblas_stride stridey,     \
                                     rocblas_int    batch_count);

INSTANTIATE_HBMV_TEMPLATE(rocblas_float_complex const*, rocblas_float_complex const*, rocblas_float_complex*)
INSTANTIATE_HBMV_TEMPLATE(rocblas_double_complex const*, rocblas_double_complex const*, rocblas_double_complex*)
INSTANTIATE_HBMV_TEMPLATE(rocblas_float_complex const*, rocblas_float_complex const* const*, rocblas_float_complex* const*)
INSTANTIATE_HBMV_TEMPLATE(rocblas_double_complex const*, rocblas_double_complex const* const*, rocblas_double_complex* const*)

#undef INSTANTIATE_HBMV_TEMPLATE

#ifdef INSTANTIATE_HBMV_NUMERICS
#error INSTANTIATE_HBMV_NUMERICS already defined
#endif

#define INSTANTIATE_HBMV_NUMERICS(T_, U_)                                 \
template rocblas_status rocblas_hbmv_check_numerics<T_, U_>               \
                                          (const char*    function_name,  \
                                           rocblas_handle handle,         \
                                           rocblas_int    n,              \
                                           rocblas_int    k,              \
                                           T_             A,              \
                                           rocblas_stride offset_a,       \
                                           rocblas_int    lda,            \
                                           rocblas_stride stride_a,       \
                                           T_             x,              \
                                           rocblas_stride offset_x,       \
                                           rocblas_int    inc_x,          \
                                           rocblas_stride stride_x,       \
                                           U_              y,             \
                                           rocblas_stride  offset_y,      \
                                           rocblas_int    inc_y,          \
                                           rocblas_stride stride_y,       \
                                           rocblas_int    batch_count,    \
                                           const int      check_numerics, \
                                           bool           is_input);

INSTANTIATE_HBMV_NUMERICS(rocblas_float_complex const*, rocblas_float_complex*)
INSTANTIATE_HBMV_NUMERICS(rocblas_double_complex const*, rocblas_double_complex*)
INSTANTIATE_HBMV_NUMERICS(rocblas_float_complex const* const*, rocblas_float_complex* const*)
INSTANTIATE_HBMV_NUMERICS(rocblas_double_complex const* const*, rocblas_double_complex* const*)

#undef INSTANTIATE_HBMV_NUMERICS

// clang-format on
