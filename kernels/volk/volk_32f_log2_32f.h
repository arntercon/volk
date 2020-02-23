/* -*- c++ -*- */
/*
 * Copyright 2014 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*!
 * \page volk_32f_log2_32f
 *
 * \b Overview
 *
 * Computes base 2 log of input vector and stores results in output vector.
 *
 * Note that this implementation is not conforming to the IEEE FP standard, i.e.,
 * +-Inf outputs are mapped to +-127.0f and +-NaN input values are not supported.
 *
 * This kernel was adapted from Jose Fonseca's Fast SSE2 log implementation
 * http://jrfonseca.blogspot.in/2008/09/fast-sse2-pow-tables-or-polynomials.htm
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * This is the MIT License (MIT)
 *
 * <b>Dispatcher Prototype</b>
 * \code
 * void volk_32f_log2_32f(float* bVector, const float* aVector, unsigned int num_points)
 * \endcode
 *
 * \b Inputs
 * \li aVector: the input vector of floats.
 * \li num_points: The number of data points.
 *
 * \b Outputs
 * \li bVector: The output vector.
 *
 * \b Example
 * \code
 *   int N = 10;
 *   unsigned int alignment = volk_get_alignment();
 *   float* in = (float*)volk_malloc(sizeof(float)*N, alignment);
 *   float* out = (float*)volk_malloc(sizeof(float)*N, alignment);
 *
 *   for(unsigned int ii = 0; ii < N; ++ii){
 *       in[ii] = std::pow(2.f,((float)ii));
 *   }
 *
 *   volk_32f_log2_32f(out, in, N);
 *
 *   for(unsigned int ii = 0; ii < N; ++ii){
 *       printf("out(%i) = %f\n", ii, out[ii]);
 *   }
 *
 *   volk_free(in);
 *   volk_free(out);
 * \endcode
 */

#ifndef INCLUDED_volk_32f_log2_32f_a_H
#define INCLUDED_volk_32f_log2_32f_a_H

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_POLY_DEGREE 6

// +-Inf -> +-127.0f in order to match the behaviour of the SIMD kernels
static inline float log2f_non_ieee(float f)
{
    float const result = log2f(f);
    return isinf(result) ? copysignf(127.0f, result) : result;
}

#ifdef LV_HAVE_GENERIC

static inline void
volk_32f_log2_32f_generic(float* bVector, const float* aVector, unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;
    unsigned int number = 0;

    for (number = 0; number < num_points; number++)
        *bPtr++ = log2f_non_ieee(*aPtr++);
}
#endif /* LV_HAVE_GENERIC */

#if LV_HAVE_AVX2 && LV_HAVE_FMA
#include <immintrin.h>

#define POLY0_FMAAVX2(x, c0) _mm256_set1_ps(c0)
#define POLY1_FMAAVX2(x, c0, c1) \
    _mm256_fmadd_ps(POLY0_FMAAVX2(x, c1), x, _mm256_set1_ps(c0))
#define POLY2_FMAAVX2(x, c0, c1, c2) \
    _mm256_fmadd_ps(POLY1_FMAAVX2(x, c1, c2), x, _mm256_set1_ps(c0))
#define POLY3_FMAAVX2(x, c0, c1, c2, c3) \
    _mm256_fmadd_ps(POLY2_FMAAVX2(x, c1, c2, c3), x, _mm256_set1_ps(c0))
#define POLY4_FMAAVX2(x, c0, c1, c2, c3, c4) \
    _mm256_fmadd_ps(POLY3_FMAAVX2(x, c1, c2, c3, c4), x, _mm256_set1_ps(c0))
#define POLY5_FMAAVX2(x, c0, c1, c2, c3, c4, c5) \
    _mm256_fmadd_ps(POLY4_FMAAVX2(x, c1, c2, c3, c4, c5), x, _mm256_set1_ps(c0))

static inline void volk_32f_log2_32f_a_avx2_fma(float* bVector,
                                                const float* aVector,
                                                unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;

    unsigned int number = 0;
    const unsigned int eighthPoints = num_points / 8;

    __m256 aVal, bVal, mantissa, frac, leadingOne;
    __m256i bias, exp;

    for (; number < eighthPoints; number++) {

        aVal = _mm256_load_ps(aPtr);
        bias = _mm256_set1_epi32(127);
        leadingOne = _mm256_set1_ps(1.0f);
        exp = _mm256_sub_epi32(
            _mm256_srli_epi32(_mm256_and_si256(_mm256_castps_si256(aVal),
                                               _mm256_set1_epi32(0x7f800000)),
                              23),
            bias);
        bVal = _mm256_cvtepi32_ps(exp);

        // Now to extract mantissa
        frac = _mm256_or_ps(
            leadingOne,
            _mm256_and_ps(aVal, _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffff))));

#if LOG_POLY_DEGREE == 6
        mantissa = POLY5_FMAAVX2(frac,
                                 3.1157899f,
                                 -3.3241990f,
                                 2.5988452f,
                                 -1.2315303f,
                                 3.1821337e-1f,
                                 -3.4436006e-2f);
#elif LOG_POLY_DEGREE == 5
        mantissa = POLY4_FMAAVX2(frac,
                                 2.8882704548164776201f,
                                 -2.52074962577807006663f,
                                 1.48116647521213171641f,
                                 -0.465725644288844778798f,
                                 0.0596515482674574969533f);
#elif LOG_POLY_DEGREE == 4
        mantissa = POLY3_FMAAVX2(frac,
                                 2.61761038894603480148f,
                                 -1.75647175389045657003f,
                                 0.688243882994381274313f,
                                 -0.107254423828329604454f);
#elif LOG_POLY_DEGREE == 3
        mantissa = POLY2_FMAAVX2(frac,
                                 2.28330284476918490682f,
                                 -1.04913055217340124191f,
                                 0.204446009836232697516f);
#else
#error
#endif

        bVal = _mm256_fmadd_ps(mantissa, _mm256_sub_ps(frac, leadingOne), bVal);
        _mm256_store_ps(bPtr, bVal);

        aPtr += 8;
        bPtr += 8;
    }

    number = eighthPoints * 8;
    volk_32f_log2_32f_generic(bPtr, aPtr, num_points - number);
}

#endif /* LV_HAVE_AVX2 && LV_HAVE_FMA for aligned */

#ifdef LV_HAVE_AVX2
#include <immintrin.h>

#define POLY0_AVX2(x, c0) _mm256_set1_ps(c0)
#define POLY1_AVX2(x, c0, c1) \
    _mm256_add_ps(_mm256_mul_ps(POLY0_AVX2(x, c1), x), _mm256_set1_ps(c0))
#define POLY2_AVX2(x, c0, c1, c2) \
    _mm256_add_ps(_mm256_mul_ps(POLY1_AVX2(x, c1, c2), x), _mm256_set1_ps(c0))
#define POLY3_AVX2(x, c0, c1, c2, c3) \
    _mm256_add_ps(_mm256_mul_ps(POLY2_AVX2(x, c1, c2, c3), x), _mm256_set1_ps(c0))
#define POLY4_AVX2(x, c0, c1, c2, c3, c4) \
    _mm256_add_ps(_mm256_mul_ps(POLY3_AVX2(x, c1, c2, c3, c4), x), _mm256_set1_ps(c0))
#define POLY5_AVX2(x, c0, c1, c2, c3, c4, c5) \
    _mm256_add_ps(_mm256_mul_ps(POLY4_AVX2(x, c1, c2, c3, c4, c5), x), _mm256_set1_ps(c0))

static inline void
volk_32f_log2_32f_a_avx2(float* bVector, const float* aVector, unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;

    unsigned int number = 0;
    const unsigned int eighthPoints = num_points / 8;

    __m256 aVal, bVal, mantissa, frac, leadingOne;
    __m256i bias, exp;

    for (; number < eighthPoints; number++) {

        aVal = _mm256_load_ps(aPtr);
        bias = _mm256_set1_epi32(127);
        leadingOne = _mm256_set1_ps(1.0f);
        exp = _mm256_sub_epi32(
            _mm256_srli_epi32(_mm256_and_si256(_mm256_castps_si256(aVal),
                                               _mm256_set1_epi32(0x7f800000)),
                              23),
            bias);
        bVal = _mm256_cvtepi32_ps(exp);

        // Now to extract mantissa
        frac = _mm256_or_ps(
            leadingOne,
            _mm256_and_ps(aVal, _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffff))));

#if LOG_POLY_DEGREE == 6
        mantissa = POLY5_AVX2(frac,
                              3.1157899f,
                              -3.3241990f,
                              2.5988452f,
                              -1.2315303f,
                              3.1821337e-1f,
                              -3.4436006e-2f);
#elif LOG_POLY_DEGREE == 5
        mantissa = POLY4_AVX2(frac,
                              2.8882704548164776201f,
                              -2.52074962577807006663f,
                              1.48116647521213171641f,
                              -0.465725644288844778798f,
                              0.0596515482674574969533f);
#elif LOG_POLY_DEGREE == 4
        mantissa = POLY3_AVX2(frac,
                              2.61761038894603480148f,
                              -1.75647175389045657003f,
                              0.688243882994381274313f,
                              -0.107254423828329604454f);
#elif LOG_POLY_DEGREE == 3
        mantissa = POLY2_AVX2(frac,
                              2.28330284476918490682f,
                              -1.04913055217340124191f,
                              0.204446009836232697516f);
#else
#error
#endif

        bVal =
            _mm256_add_ps(_mm256_mul_ps(mantissa, _mm256_sub_ps(frac, leadingOne)), bVal);
        _mm256_store_ps(bPtr, bVal);

        aPtr += 8;
        bPtr += 8;
    }

    number = eighthPoints * 8;
    volk_32f_log2_32f_generic(bPtr, aPtr, num_points - number);
}

#endif /* LV_HAVE_AVX2 for aligned */

#ifdef LV_HAVE_SSE4_1
#include <smmintrin.h>

#define POLY0(x, c0) _mm_set1_ps(c0)
#define POLY1(x, c0, c1) _mm_add_ps(_mm_mul_ps(POLY0(x, c1), x), _mm_set1_ps(c0))
#define POLY2(x, c0, c1, c2) _mm_add_ps(_mm_mul_ps(POLY1(x, c1, c2), x), _mm_set1_ps(c0))
#define POLY3(x, c0, c1, c2, c3) \
    _mm_add_ps(_mm_mul_ps(POLY2(x, c1, c2, c3), x), _mm_set1_ps(c0))
#define POLY4(x, c0, c1, c2, c3, c4) \
    _mm_add_ps(_mm_mul_ps(POLY3(x, c1, c2, c3, c4), x), _mm_set1_ps(c0))
#define POLY5(x, c0, c1, c2, c3, c4, c5) \
    _mm_add_ps(_mm_mul_ps(POLY4(x, c1, c2, c3, c4, c5), x), _mm_set1_ps(c0))

static inline void
volk_32f_log2_32f_a_sse4_1(float* bVector, const float* aVector, unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;

    unsigned int number = 0;
    const unsigned int quarterPoints = num_points / 4;

    __m128 aVal, bVal, mantissa, frac, leadingOne;
    __m128i bias, exp;

    for (; number < quarterPoints; number++) {

        aVal = _mm_load_ps(aPtr);
        bias = _mm_set1_epi32(127);
        leadingOne = _mm_set1_ps(1.0f);
        exp = _mm_sub_epi32(
            _mm_srli_epi32(
                _mm_and_si128(_mm_castps_si128(aVal), _mm_set1_epi32(0x7f800000)), 23),
            bias);
        bVal = _mm_cvtepi32_ps(exp);

        // Now to extract mantissa
        frac = _mm_or_ps(leadingOne,
                         _mm_and_ps(aVal, _mm_castsi128_ps(_mm_set1_epi32(0x7fffff))));

#if LOG_POLY_DEGREE == 6
        mantissa = POLY5(frac,
                         3.1157899f,
                         -3.3241990f,
                         2.5988452f,
                         -1.2315303f,
                         3.1821337e-1f,
                         -3.4436006e-2f);
#elif LOG_POLY_DEGREE == 5
        mantissa = POLY4(frac,
                         2.8882704548164776201f,
                         -2.52074962577807006663f,
                         1.48116647521213171641f,
                         -0.465725644288844778798f,
                         0.0596515482674574969533f);
#elif LOG_POLY_DEGREE == 4
        mantissa = POLY3(frac,
                         2.61761038894603480148f,
                         -1.75647175389045657003f,
                         0.688243882994381274313f,
                         -0.107254423828329604454f);
#elif LOG_POLY_DEGREE == 3
        mantissa = POLY2(frac,
                         2.28330284476918490682f,
                         -1.04913055217340124191f,
                         0.204446009836232697516f);
#else
#error
#endif

        bVal = _mm_add_ps(bVal, _mm_mul_ps(mantissa, _mm_sub_ps(frac, leadingOne)));
        _mm_store_ps(bPtr, bVal);

        aPtr += 4;
        bPtr += 4;
    }

    number = quarterPoints * 4;
    volk_32f_log2_32f_generic(bPtr, aPtr, num_points - number);
}

#endif /* LV_HAVE_SSE4_1 for aligned */

#ifdef LV_HAVE_NEON
#include <arm_neon.h>

/* these macros allow us to embed logs in other kernels */
#define VLOG2Q_NEON_PREAMBLE()                         \
    int32x4_t one = vdupq_n_s32(0x000800000);          \
    /* minimax polynomial */                           \
    float32x4_t p0 = vdupq_n_f32(-3.0400402727048585); \
    float32x4_t p1 = vdupq_n_f32(6.1129631282966113);  \
    float32x4_t p2 = vdupq_n_f32(-5.3419892024633207); \
    float32x4_t p3 = vdupq_n_f32(3.2865287703753912);  \
    float32x4_t p4 = vdupq_n_f32(-1.2669182593441635); \
    float32x4_t p5 = vdupq_n_f32(0.2751487703421256);  \
    float32x4_t p6 = vdupq_n_f32(-0.0256910888150985); \
    int32x4_t exp_mask = vdupq_n_s32(0x7f800000);      \
    int32x4_t sig_mask = vdupq_n_s32(0x007fffff);      \
    int32x4_t exp_bias = vdupq_n_s32(127);


#define VLOG2Q_NEON_F32(log2_approx, aval)                                      \
    int32x4_t exponent_i = vandq_s32(aval, exp_mask);                           \
    int32x4_t significand_i = vandq_s32(aval, sig_mask);                        \
    exponent_i = vshrq_n_s32(exponent_i, 23);                                   \
                                                                                \
    /* extract the exponent and significand                                     \
       we can treat this as fixed point to save ~9% on the                      \
       conversion + float add */                                                \
    significand_i = vorrq_s32(one, significand_i);                              \
    float32x4_t significand_f = vcvtq_n_f32_s32(significand_i, 23);             \
    /* debias the exponent and convert to float */                              \
    exponent_i = vsubq_s32(exponent_i, exp_bias);                               \
    float32x4_t exponent_f = vcvtq_f32_s32(exponent_i);                         \
                                                                                \
    /* put the significand through a polynomial fit of log2(x) [1,2]            \
       add the result to the exponent */                                        \
    log2_approx = vaddq_f32(exponent_f, p0);         /* p0 */                   \
    float32x4_t tmp1 = vmulq_f32(significand_f, p1); /* p1 * x */               \
    log2_approx = vaddq_f32(log2_approx, tmp1);                                 \
    float32x4_t sig_2 = vmulq_f32(significand_f, significand_f); /* x^2 */      \
    tmp1 = vmulq_f32(sig_2, p2);                                 /* p2 * x^2 */ \
    log2_approx = vaddq_f32(log2_approx, tmp1);                                 \
                                                                                \
    float32x4_t sig_3 = vmulq_f32(sig_2, significand_f); /* x^3 */              \
    tmp1 = vmulq_f32(sig_3, p3);                         /* p3 * x^3 */         \
    log2_approx = vaddq_f32(log2_approx, tmp1);                                 \
    float32x4_t sig_4 = vmulq_f32(sig_2, sig_2); /* x^4 */                      \
    tmp1 = vmulq_f32(sig_4, p4);                 /* p4 * x^4 */                 \
    log2_approx = vaddq_f32(log2_approx, tmp1);                                 \
    float32x4_t sig_5 = vmulq_f32(sig_3, sig_2); /* x^5 */                      \
    tmp1 = vmulq_f32(sig_5, p5);                 /* p5 * x^5 */                 \
    log2_approx = vaddq_f32(log2_approx, tmp1);                                 \
    float32x4_t sig_6 = vmulq_f32(sig_3, sig_3); /* x^6 */                      \
    tmp1 = vmulq_f32(sig_6, p6);                 /* p6 * x^6 */                 \
    log2_approx = vaddq_f32(log2_approx, tmp1);

static inline void
volk_32f_log2_32f_neon(float* bVector, const float* aVector, unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;
    unsigned int number;
    const unsigned int quarterPoints = num_points / 4;

    int32x4_t aval;
    float32x4_t log2_approx;

    VLOG2Q_NEON_PREAMBLE()
    // lms
    // p0 = vdupq_n_f32(-1.649132280361871);
    // p1 = vdupq_n_f32(1.995047138579499);
    // p2 = vdupq_n_f32(-0.336914839219728);

    // keep in mind a single precision float is represented as
    //   (-1)^sign * 2^exp * 1.significand, so the log2 is
    // log2(2^exp * sig) = exponent + log2(1 + significand/(1<<23)
    for (number = 0; number < quarterPoints; ++number) {
        // load float in to an int register without conversion
        aval = vld1q_s32((int*)aPtr);

        VLOG2Q_NEON_F32(log2_approx, aval)

        vst1q_f32(bPtr, log2_approx);

        aPtr += 4;
        bPtr += 4;
    }

    number = quarterPoints * 4;
    volk_32f_log2_32f_generic(bPtr, aPtr, num_points - number);
}

#endif /* LV_HAVE_NEON */


#endif /* INCLUDED_volk_32f_log2_32f_a_H */

#ifndef INCLUDED_volk_32f_log2_32f_u_H
#define INCLUDED_volk_32f_log2_32f_u_H


#ifdef LV_HAVE_GENERIC

static inline void
volk_32f_log2_32f_u_generic(float* bVector, const float* aVector, unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;
    unsigned int number = 0;

    for (number = 0; number < num_points; number++) {
        float const result = log2f(*aPtr++);
        *bPtr++ = isinf(result) ? -127.0f : result;
    }
}

#endif /* LV_HAVE_GENERIC */


#ifdef LV_HAVE_SSE4_1
#include <smmintrin.h>

#define POLY0(x, c0) _mm_set1_ps(c0)
#define POLY1(x, c0, c1) _mm_add_ps(_mm_mul_ps(POLY0(x, c1), x), _mm_set1_ps(c0))
#define POLY2(x, c0, c1, c2) _mm_add_ps(_mm_mul_ps(POLY1(x, c1, c2), x), _mm_set1_ps(c0))
#define POLY3(x, c0, c1, c2, c3) \
    _mm_add_ps(_mm_mul_ps(POLY2(x, c1, c2, c3), x), _mm_set1_ps(c0))
#define POLY4(x, c0, c1, c2, c3, c4) \
    _mm_add_ps(_mm_mul_ps(POLY3(x, c1, c2, c3, c4), x), _mm_set1_ps(c0))
#define POLY5(x, c0, c1, c2, c3, c4, c5) \
    _mm_add_ps(_mm_mul_ps(POLY4(x, c1, c2, c3, c4, c5), x), _mm_set1_ps(c0))

static inline void
volk_32f_log2_32f_u_sse4_1(float* bVector, const float* aVector, unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;

    unsigned int number = 0;
    const unsigned int quarterPoints = num_points / 4;

    __m128 aVal, bVal, mantissa, frac, leadingOne;
    __m128i bias, exp;

    for (; number < quarterPoints; number++) {

        aVal = _mm_loadu_ps(aPtr);
        bias = _mm_set1_epi32(127);
        leadingOne = _mm_set1_ps(1.0f);
        exp = _mm_sub_epi32(
            _mm_srli_epi32(
                _mm_and_si128(_mm_castps_si128(aVal), _mm_set1_epi32(0x7f800000)), 23),
            bias);
        bVal = _mm_cvtepi32_ps(exp);

        // Now to extract mantissa
        frac = _mm_or_ps(leadingOne,
                         _mm_and_ps(aVal, _mm_castsi128_ps(_mm_set1_epi32(0x7fffff))));

#if LOG_POLY_DEGREE == 6
        mantissa = POLY5(frac,
                         3.1157899f,
                         -3.3241990f,
                         2.5988452f,
                         -1.2315303f,
                         3.1821337e-1f,
                         -3.4436006e-2f);
#elif LOG_POLY_DEGREE == 5
        mantissa = POLY4(frac,
                         2.8882704548164776201f,
                         -2.52074962577807006663f,
                         1.48116647521213171641f,
                         -0.465725644288844778798f,
                         0.0596515482674574969533f);
#elif LOG_POLY_DEGREE == 4
        mantissa = POLY3(frac,
                         2.61761038894603480148f,
                         -1.75647175389045657003f,
                         0.688243882994381274313f,
                         -0.107254423828329604454f);
#elif LOG_POLY_DEGREE == 3
        mantissa = POLY2(frac,
                         2.28330284476918490682f,
                         -1.04913055217340124191f,
                         0.204446009836232697516f);
#else
#error
#endif

        bVal = _mm_add_ps(bVal, _mm_mul_ps(mantissa, _mm_sub_ps(frac, leadingOne)));
        _mm_storeu_ps(bPtr, bVal);

        aPtr += 4;
        bPtr += 4;
    }

    number = quarterPoints * 4;
    volk_32f_log2_32f_u_generic(bPtr, aPtr, num_points - number);
}

#endif /* LV_HAVE_SSE4_1 for unaligned */

#if LV_HAVE_AVX2 && LV_HAVE_FMA
#include <immintrin.h>

#define POLY0_FMAAVX2(x, c0) _mm256_set1_ps(c0)
#define POLY1_FMAAVX2(x, c0, c1) \
    _mm256_fmadd_ps(POLY0_FMAAVX2(x, c1), x, _mm256_set1_ps(c0))
#define POLY2_FMAAVX2(x, c0, c1, c2) \
    _mm256_fmadd_ps(POLY1_FMAAVX2(x, c1, c2), x, _mm256_set1_ps(c0))
#define POLY3_FMAAVX2(x, c0, c1, c2, c3) \
    _mm256_fmadd_ps(POLY2_FMAAVX2(x, c1, c2, c3), x, _mm256_set1_ps(c0))
#define POLY4_FMAAVX2(x, c0, c1, c2, c3, c4) \
    _mm256_fmadd_ps(POLY3_FMAAVX2(x, c1, c2, c3, c4), x, _mm256_set1_ps(c0))
#define POLY5_FMAAVX2(x, c0, c1, c2, c3, c4, c5) \
    _mm256_fmadd_ps(POLY4_FMAAVX2(x, c1, c2, c3, c4, c5), x, _mm256_set1_ps(c0))

static inline void volk_32f_log2_32f_u_avx2_fma(float* bVector,
                                                const float* aVector,
                                                unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;

    unsigned int number = 0;
    const unsigned int eighthPoints = num_points / 8;

    __m256 aVal, bVal, mantissa, frac, leadingOne;
    __m256i bias, exp;

    for (; number < eighthPoints; number++) {

        aVal = _mm256_loadu_ps(aPtr);
        bias = _mm256_set1_epi32(127);
        leadingOne = _mm256_set1_ps(1.0f);
        exp = _mm256_sub_epi32(
            _mm256_srli_epi32(_mm256_and_si256(_mm256_castps_si256(aVal),
                                               _mm256_set1_epi32(0x7f800000)),
                              23),
            bias);
        bVal = _mm256_cvtepi32_ps(exp);

        // Now to extract mantissa
        frac = _mm256_or_ps(
            leadingOne,
            _mm256_and_ps(aVal, _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffff))));

#if LOG_POLY_DEGREE == 6
        mantissa = POLY5_FMAAVX2(frac,
                                 3.1157899f,
                                 -3.3241990f,
                                 2.5988452f,
                                 -1.2315303f,
                                 3.1821337e-1f,
                                 -3.4436006e-2f);
#elif LOG_POLY_DEGREE == 5
        mantissa = POLY4_FMAAVX2(frac,
                                 2.8882704548164776201f,
                                 -2.52074962577807006663f,
                                 1.48116647521213171641f,
                                 -0.465725644288844778798f,
                                 0.0596515482674574969533f);
#elif LOG_POLY_DEGREE == 4
        mantissa = POLY3_FMAAVX2(frac,
                                 2.61761038894603480148f,
                                 -1.75647175389045657003f,
                                 0.688243882994381274313f,
                                 -0.107254423828329604454f);
#elif LOG_POLY_DEGREE == 3
        mantissa = POLY2_FMAAVX2(frac,
                                 2.28330284476918490682f,
                                 -1.04913055217340124191f,
                                 0.204446009836232697516f);
#else
#error
#endif

        bVal = _mm256_fmadd_ps(mantissa, _mm256_sub_ps(frac, leadingOne), bVal);
        _mm256_storeu_ps(bPtr, bVal);

        aPtr += 8;
        bPtr += 8;
    }

    number = eighthPoints * 8;
    volk_32f_log2_32f_u_generic(bPtr, aPtr, num_points - number);
}

#endif /* LV_HAVE_AVX2 && LV_HAVE_FMA for unaligned */

#ifdef LV_HAVE_AVX2
#include <immintrin.h>

#define POLY0_AVX2(x, c0) _mm256_set1_ps(c0)
#define POLY1_AVX2(x, c0, c1) \
    _mm256_add_ps(_mm256_mul_ps(POLY0_AVX2(x, c1), x), _mm256_set1_ps(c0))
#define POLY2_AVX2(x, c0, c1, c2) \
    _mm256_add_ps(_mm256_mul_ps(POLY1_AVX2(x, c1, c2), x), _mm256_set1_ps(c0))
#define POLY3_AVX2(x, c0, c1, c2, c3) \
    _mm256_add_ps(_mm256_mul_ps(POLY2_AVX2(x, c1, c2, c3), x), _mm256_set1_ps(c0))
#define POLY4_AVX2(x, c0, c1, c2, c3, c4) \
    _mm256_add_ps(_mm256_mul_ps(POLY3_AVX2(x, c1, c2, c3, c4), x), _mm256_set1_ps(c0))
#define POLY5_AVX2(x, c0, c1, c2, c3, c4, c5) \
    _mm256_add_ps(_mm256_mul_ps(POLY4_AVX2(x, c1, c2, c3, c4, c5), x), _mm256_set1_ps(c0))

static inline void
volk_32f_log2_32f_u_avx2(float* bVector, const float* aVector, unsigned int num_points)
{
    float* bPtr = bVector;
    const float* aPtr = aVector;

    unsigned int number = 0;
    const unsigned int eighthPoints = num_points / 8;

    __m256 aVal, bVal, mantissa, frac, leadingOne;
    __m256i bias, exp;

    for (; number < eighthPoints; number++) {

        aVal = _mm256_loadu_ps(aPtr);
        bias = _mm256_set1_epi32(127);
        leadingOne = _mm256_set1_ps(1.0f);
        exp = _mm256_sub_epi32(
            _mm256_srli_epi32(_mm256_and_si256(_mm256_castps_si256(aVal),
                                               _mm256_set1_epi32(0x7f800000)),
                              23),
            bias);
        bVal = _mm256_cvtepi32_ps(exp);

        // Now to extract mantissa
        frac = _mm256_or_ps(
            leadingOne,
            _mm256_and_ps(aVal, _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffff))));

#if LOG_POLY_DEGREE == 6
        mantissa = POLY5_AVX2(frac,
                              3.1157899f,
                              -3.3241990f,
                              2.5988452f,
                              -1.2315303f,
                              3.1821337e-1f,
                              -3.4436006e-2f);
#elif LOG_POLY_DEGREE == 5
        mantissa = POLY4_AVX2(frac,
                              2.8882704548164776201f,
                              -2.52074962577807006663f,
                              1.48116647521213171641f,
                              -0.465725644288844778798f,
                              0.0596515482674574969533f);
#elif LOG_POLY_DEGREE == 4
        mantissa = POLY3_AVX2(frac,
                              2.61761038894603480148f,
                              -1.75647175389045657003f,
                              0.688243882994381274313f,
                              -0.107254423828329604454f);
#elif LOG_POLY_DEGREE == 3
        mantissa = POLY2_AVX2(frac,
                              2.28330284476918490682f,
                              -1.04913055217340124191f,
                              0.204446009836232697516f);
#else
#error
#endif

        bVal =
            _mm256_add_ps(_mm256_mul_ps(mantissa, _mm256_sub_ps(frac, leadingOne)), bVal);
        _mm256_storeu_ps(bPtr, bVal);

        aPtr += 8;
        bPtr += 8;
    }

    number = eighthPoints * 8;
    volk_32f_log2_32f_u_generic(bPtr, aPtr, num_points - number);
}

#endif /* LV_HAVE_AVX2 for unaligned */


#endif /* INCLUDED_volk_32f_log2_32f_u_H */
