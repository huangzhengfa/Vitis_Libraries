/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file bandedMatMoverB1.hpp
 * @brief data movers for banded matrices and corresponding vectors.
 *
 * This file is part of XF BLAS Library.
 */

#ifndef XF_BLAS_BANDEDMATMOVERB2_HPP
#define XF_BLAS_BANDEDMATMOVERB2_HPP

#include "hls_stream.h"
#include "ap_int.h"
#include "ap_shift_reg.h"

namespace xf {
namespace linear_algebra {
namespace blas {

/**
 * @brief processUpSbMatStream function that moves the head paddings to the end of each super-diagonal
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of super-diagonals
 * @param p_in stream of the super-diagonals not including main diagonal of a p_n x p_n symmetric banded matrix
 * @param p_out output stream of super-diagonals not including main diagonal with 0 paddings in the tails
 */
template <typename t_DataType, unsigned int t_ParEntries, int t_LastRowIdx = 0>
void processUpSbMatStream(unsigned int p_n,
                          unsigned int p_k,
                          hls::stream<WideType<t_DataType, t_ParEntries> >& p_in,
                          hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
    unsigned int l_parBlocks = p_n / t_ParEntries;
    for (int i = p_k; i > t_LastRowIdx; --i) {
        uint16_t l_numPaddings = i % t_ParEntries;
        uint16_t l_entBegin = t_ParEntries - l_numPaddings;
        uint16_t j = 0;
        uint16_t l_numOut = 0;
        WideType<t_DataType, t_ParEntries> l_intVal;
#pragma HLS ARRAY_PARTITION variable = l_intVal complete dim = 1
        WideType<t_DataType, t_ParEntries> l_out;
#pragma HLS ARRAY_PARTITION variable = l_out complete dim = 1

        // skip the paddings
        while (i >= (j * t_ParEntries)) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val = p_in.read();
            l_intVal = l_val;
            j++;
        }

        // output superdiagonals without paddings
        while (j < l_parBlocks) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val = p_in.read();
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_out[b] = (b < l_entBegin) ? l_intVal[l_numPaddings + b] : l_val[b - l_entBegin];
            }
            p_out.write(l_out);
            l_numOut++;
            l_intVal = l_val;
            j++;
        }

        // pad zeros in the end of each super-diagonal
        while (l_numOut < l_parBlocks) {
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_out[b] = (b < l_entBegin) ? l_intVal[l_numPaddings + b] : 0;
            }
            p_out.write(l_out);
            l_intVal = 0;
            l_numOut++;
        }
    }
}

/**
 * @brief processLoSbMatStream function that moves the tail paddings to the beginning of each super-diagonal
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of sub-diagonals
 * @param p_in stream of the sub-diagonals including main diagonal of a p_n x p_n symmetric banded matrix
 * @param p_out output stream of sub-diagonals including main diagonal with 0 paddings at the beginnings
 */
template <typename t_DataType, unsigned int t_ParEntries>
void processLoSbMatStream(unsigned int p_n,
                          unsigned int p_k,
                          hls::stream<WideType<t_DataType, t_ParEntries> >& p_in,
                          hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
    unsigned int l_parBlocks = p_n / t_ParEntries;
    for (int i = 0; i <= p_k; ++i) {
        uint16_t l_numPaddings = i % t_ParEntries;
        uint16_t l_entBegin = t_ParEntries - l_numPaddings;
        uint16_t j = 1;
        uint16_t l_numOut = 0;
        WideType<t_DataType, t_ParEntries> l_intVal(0);
#pragma HLS ARRAY_PARTITION variable = l_intVal complete dim = 1
        WideType<t_DataType, t_ParEntries> l_out;
#pragma HLS ARRAY_PARTITION variable = l_out complete dim = 1

        // pad zeros
        while (i >= (j * t_ParEntries)) {
#pragma HLS PIPELINE
            l_out = l_intVal;
            p_out.write(l_out);
            l_numOut++;
            j++;
        }

        // output entries along sub-diagonals
        while (l_numOut < l_parBlocks) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val = p_in.read();
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_out[b] = (b < l_numPaddings) ? l_intVal[l_entBegin + b] : l_val[b - l_numPaddings];
            }
            p_out.write(l_out);
            l_numOut++;
            l_intVal = l_val;
        }
        // read out redundant data
        while (j > 1) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val = p_in.read();
            j--;
        }
    }
}

/**
 * @brief forwardSbMatStream function that forwards the diagonal streams
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of diagonals
 * @param p_in stream of the diagonals of a p_n x p_n symmetric banded matrix
 * @param p_out the output copy of the input diagonal stream
 */
template <typename t_DataType, unsigned int t_ParEntries>
void forwardSbMatStream(unsigned int p_n,
                        unsigned int p_k,
                        hls::stream<WideType<t_DataType, t_ParEntries> >& p_in,
                        hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
    unsigned int l_parBlocks = p_n / t_ParEntries;
    for (unsigned int i = 0; i < p_k; ++i) {
        for (unsigned int j = 0; j < l_parBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val = p_in.read();
            p_out.write(l_val);
        }
    }
}

/**
 * @brief mergeGbMatStream function that merges processed gbMat streams
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_ku number of super-diagonals
 * @param p_kl number of sub-diagonals
 * @param p_inUp stream of the processed super-diagonals, not including main diagonal
 * @param p_inLo stream of the processed sub_diagonals, including main diagonal
 * @param p_out the output gbMat stream
 */
template <typename t_DataType, unsigned int t_ParEntries>
void mergeGbMatStream(unsigned int p_n,
                      unsigned int p_ku,
                      unsigned int p_kl,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_inUp,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_inLo,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
    unsigned int l_parBlocks = p_n / t_ParEntries;
    for (unsigned int i = 0; i < p_ku; ++i) {
        for (unsigned int j = 0; j < l_parBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val = p_inUp.read();
            p_out.write(l_val);
        }
    }
    for (unsigned int i = 0; i <= p_kl; ++i) {
        for (unsigned int j = 0; j < l_parBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val = p_inLo.read();
            p_out.write(l_val);
        }
    }
}

/**
 * @brief readUpSbMat2Stream function that read the sbMat from Memory into the streams
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of super-diagonals
 * @param p_a memory location of a p_n x p_n symmetric banded matrix with super-diagonals
 * @param p_outUp the stream of super-diagonals
 * @param p_outLo the stream of ready sub-diagonals
 */
template <typename t_DataType, unsigned int t_ParEntries>
void readUpSbMat2Stream(unsigned int p_n,
                        unsigned int p_k,
                        t_DataType* p_a,
                        hls::stream<WideType<t_DataType, t_ParEntries> >& p_outUp,
                        hls::stream<WideType<t_DataType, t_ParEntries> >& p_outLo) {
    unsigned int l_parBlocks = p_n * p_k / t_ParEntries;
    unsigned int l_nParBlocks = p_n / t_ParEntries;
    for (unsigned int i = 0; i < l_parBlocks; ++i) {
#pragma HLS PIPELINE
        WideType<t_DataType, t_ParEntries> l_val;
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
        for (unsigned int b = 0; b < t_ParEntries; ++b) {
            l_val[b] = p_a[i * t_ParEntries + b];
        }
        p_outUp.write(l_val);
    }
    for (int i = p_k; i >= 0; --i) {
        for (unsigned int j = 0; j < l_nParBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val;
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_val[b] = p_a[(i * l_nParBlocks + j) * t_ParEntries + b];
            }
            p_outLo.write(l_val);
        }
    }
}

/**
 * @brief readLoSbMat2Stream function that read the sbMat from memory into the streams
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of sub-diagonals
 * @param p_a memory location of a p_n x p_n symmetric banded matrix with sub-diagonals
 * @param p_outUp the stream of ready super-diagonals
 * @param p_outLo the stream of sub-diagonals
 */
template <typename t_DataType, unsigned int t_ParEntries>
void readLoSbMat2Stream(unsigned int p_n,
                        unsigned int p_k,
                        t_DataType* p_a,
                        hls::stream<WideType<t_DataType, t_ParEntries> >& p_outUp,
                        hls::stream<WideType<t_DataType, t_ParEntries> >& p_outLo) {
    unsigned int l_parBlocks = p_n * (p_k + 1) / t_ParEntries;
    unsigned int l_nParBlocks = p_n / t_ParEntries;
    for (int i = p_k; i > 0; --i) {
        for (unsigned int j = 0; j < l_nParBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val;
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_val[b] = p_a[(i * l_nParBlocks + j) * t_ParEntries + b];
            }
            p_outUp.write(l_val);
        }
    }
    for (unsigned int i = 0; i < l_parBlocks; ++i) {
#pragma HLS PIPELINE
        WideType<t_DataType, t_ParEntries> l_val;
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
        for (unsigned int b = 0; b < t_ParEntries; ++b) {
            l_val[b] = p_a[i * t_ParEntries + b];
        }
        p_outLo.write(l_val);
    }
}

/**
 * @brief readVec2GbStream function that read the vector from memory into stream
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_ku number of super-diagonals
 * @param p_kl number of sub-siagonals
 * @param p_a memory location of vector x
 * @param p_outUp the vector stream of for super-diagonals
 * @param p_outLo the vector stream of for sub-diagonals
 */
template <typename t_DataType, unsigned int t_ParEntries>
void readVec2GbStream(unsigned int p_n,
                      unsigned int p_ku,
                      unsigned int p_kl,
                      t_DataType* p_x,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_outUp,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_outLo) {
    unsigned int l_parBlocks = p_n / t_ParEntries;
    for (unsigned int i = 0; i < p_ku; ++i) {
        for (unsigned int j = 0; j < l_parBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val;
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_val[b] = p_x[j * t_ParEntries + b];
            }
            p_outUp.write(l_val);
        }
    }
    for (unsigned int i = 0; i < p_kl + 1; ++i) {
        for (unsigned int j = 0; j < l_parBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val;
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_val[b] = p_x[j * t_ParEntries + b];
            }
            p_outLo.write(l_val);
        }
    }
}

/**
 * @brief readGbMat2Stream function that read the gbMat from memory into the streams
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_ku number of super-diagonals
 * @param p_kl number of sub-diagonals
 * @param p_a memory location of a p_n x p_n symmetric banded matrix with sub-diagonals
 * @param p_outUp the stream of super-diagonals
 * @param p_outLo the stream of sub-diagonals including main diagonal
 */
template <typename t_DataType, unsigned int t_ParEntries>
void readGbMat2Stream(unsigned int p_n,
                      unsigned int p_ku,
                      unsigned int p_kl,
                      t_DataType* p_a,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_outUp,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_outLo) {
    unsigned int l_parBlocks = p_n / t_ParEntries;
    for (unsigned int i = 0; i < p_ku; ++i) {
        for (unsigned int j = 0; j < l_parBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val;
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_val[b] = p_a[(i * l_parBlocks + j) * t_ParEntries + b];
            }
            p_outUp.write(l_val);
        }
    }
    t_DataType* l_a = p_a;
    l_a = &(p_a[p_ku * l_parBlocks * t_ParEntries]);
    for (unsigned int i = 0; i <= p_kl; ++i) {
        for (unsigned int j = 0; j < l_parBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val;
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_val[b] = l_a[(i * l_parBlocks + j) * t_ParEntries + b];
            }
            p_outLo.write(l_val);
        }
    }
}
/**
 * @brief sbmSuper2Stream function that moves symmetric banded matrix with super diagonals from memory to stream
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 * @tparam t_ParBlocks number of t_ParEntries, p_n must be multiple t_ParEntries * t_ParBlocks
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of superdiagonals
 * @param p_a a p_n x p_n symmetric banded matrix with on-chip column-major storage and corresponding 0 paddings
 * @param p_out output stream, which is row-aligned with 0 paddings along subdiagonals
 */
template <typename t_DataType, unsigned int t_ParEntries, unsigned int t_ParBlocks = 1>
void sbmSuper2Stream(unsigned int p_n,
                     unsigned int p_k,
                     t_DataType* p_a,
                     hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
#ifndef __SYNTHESIS__
    assert((p_n % (t_ParEntries * t_ParBlocks)) == 0);
#endif
#pragma HLS DATAFLOW
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strUp;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strLo;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strUpRes;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strLoRes;
    readUpSbMat2Stream<t_DataType, t_ParEntries>(p_n, p_k, p_a, l_strUp, l_strLo);
    processUpSbMatStream<t_DataType, t_ParEntries>(p_n, p_k, l_strUp, l_strUpRes);
    forwardSbMatStream<t_DataType, t_ParEntries>(p_n, p_k + 1, l_strLo, l_strLoRes);
    mergeGbMatStream<t_DataType, t_ParEntries>(p_n, p_k, p_k, l_strUpRes, l_strLoRes, p_out);
}

/**
 * @brief sbmSub2Stream function that moves symmetric banded matrix with sub diagonals from memory to stream
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 * @tparam t_ParBlocks number of t_ParEntries, p_n must be multiple t_ParEntries * t_ParBlocks
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of subdiagonals
 * @param p_a a p_n x p_n symmetric banded matrix with on-chip column-major storage and corresponding 0 paddings
 * @param p_out output stream, which is row-aligned with 0 paddings along subdiagonals
 */
template <typename t_DataType, unsigned int t_ParEntries, unsigned int t_ParBlocks = 1>
void sbmSub2Stream(unsigned int p_n,
                   unsigned int p_k,
                   t_DataType* p_a,
                   hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
#ifndef __SYNTHESIS__
    assert((p_n % (t_ParEntries * t_ParBlocks)) == 0);
#endif
#pragma HLS DATAFLOW
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strUp;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strLo;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strUpRes;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strLoRes;
    readLoSbMat2Stream<t_DataType, t_ParEntries>(p_n, p_k, p_a, l_strUp, l_strLo);
    forwardSbMatStream<t_DataType, t_ParEntries>(p_n, p_k, l_strUp, l_strUpRes);
    processLoSbMatStream<t_DataType, t_ParEntries>(p_n, p_k, l_strLo, l_strLoRes);
    mergeGbMatStream<t_DataType, t_ParEntries>(p_n, p_k, p_k, l_strUpRes, l_strLoRes, p_out);
}

/**
 * @brief gbm2Stream function that moves symmetric banded matrix with from memory to stream
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 * @tparam t_ParBlocks number of t_ParEntries, p_n must be multiple t_ParEntries * t_ParBlocks
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_kl number of subdiagonals
 * @param p_ku number of superdiagonals
 * @param p_a a p_m x p_n symmetric banded matrix with on-chip column-major storage and corresponding 0 paddings
 * @param p_out output stream, which is row-aligned with 0 paddings along subdiagonals
 */
template <typename t_DataType, unsigned int t_ParEntries, unsigned int t_ParBlocks = 1>
void gbm2Stream(unsigned int p_n,
                unsigned int p_kl,
                unsigned int p_ku,
                t_DataType* p_a,
                hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
#ifndef __SYNTHESIS__
    assert((p_n % (t_ParEntries * t_ParBlocks)) == 0);
#endif
#pragma HLS DATAFLOW
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strUp;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strLo;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strUpRes;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strLoRes;
    readGbMat2Stream<t_DataType, t_ParEntries>(p_n, p_ku, p_kl, p_a, l_strUp, l_strLo);
    processUpSbMatStream<t_DataType, t_ParEntries>(p_n, p_ku, l_strUp, l_strUpRes);
    processLoSbMatStream<t_DataType, t_ParEntries>(p_n, p_kl, l_strLo, l_strLoRes);
    mergeGbMatStream<t_DataType, t_ParEntries>(p_n, p_ku, p_kl, l_strUpRes, l_strLoRes, p_out);
}

/**
 * @brief vec2SbMatStream function that moves vector from memory to stream that matches the sbMat2Stream outputs
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_ku number of superdiagonals
 * @param p_kl number of subdiagonals
 * @param p_x vector input
 * @param p_out output stream, which matches the outputs of gbMat2Stream or sbMat2Stream
 */
template <typename t_DataType, unsigned int t_ParEntries>
void vec2GbMatStream(unsigned int p_n,
                     unsigned int p_kl,
                     unsigned int p_ku,
                     t_DataType* p_x,
                     hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
#ifndef __SYNTHESIS__
    assert((p_n % t_ParEntries) == 0);
#endif
#pragma HLS DATAFLOW
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strUp;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strLo;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strUpRes;
    hls::stream<WideType<t_DataType, t_ParEntries> > l_strLoRes;
    readVec2GbStream<t_DataType, t_ParEntries>(p_n, p_ku, p_kl, p_x, l_strUp, l_strLo);
    processUpSbMatStream<t_DataType, t_ParEntries>(p_n, p_ku, l_strUp, l_strUpRes);
    processLoSbMatStream<t_DataType, t_ParEntries>(p_n, p_kl, l_strLo, l_strLoRes);
    mergeGbMatStream<t_DataType, t_ParEntries>(p_n, p_ku, p_kl, l_strUpRes, l_strLoRes, p_out);
} // end vec2GbMatStream

/**
 * @brief readTbMat2Stream function that read the tbMat from Memory into the streams
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of super/sub-diagonals
 * @param p_a memory location of a p_n x p_n symmetric banded matrix with super-diagonals
 * @param p_out the stream of diagonals
 */
template <typename t_DataType, unsigned int t_ParEntries>
void readTbMat2Stream(unsigned int p_n,
                      unsigned int p_k,
                      t_DataType* p_a,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
    unsigned int l_parBlocks = p_n * (p_k + 1) / t_ParEntries;
    for (unsigned int i = 0; i < l_parBlocks; ++i) {
#pragma HLS PIPELINE
        WideType<t_DataType, t_ParEntries> l_val;
#pragma HLS ARRAY_PARTITION variable = l_val complete dim = 1
        for (unsigned int b = 0; b < t_ParEntries; ++b) {
            l_val[b] = p_a[i * t_ParEntries + b];
        }
        p_out.write(l_val);
    }
}

/**
 * @brief readVec2TbStream function that read the vector from memory into stream
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of super/sub-diagonals
 * @param p_a memory location of vector x
 * @param p_out the vector stream of for diagonals
 */
template <typename t_DataType, unsigned int t_ParEntries>
void readVec2TbStream(unsigned int p_n,
                      unsigned int p_k,
                      t_DataType* p_x,
                      hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
    unsigned int l_parBlocks = p_n / t_ParEntries;
    for (unsigned int i = 0; i <= p_k; ++i) {
        for (unsigned int j = 0; j < l_parBlocks; ++j) {
#pragma HLS PIPELINE
            WideType<t_DataType, t_ParEntries> l_val;
            for (unsigned int b = 0; b < t_ParEntries; ++b) {
                l_val[b] = p_x[j * t_ParEntries + b];
            }
            p_out.write(l_val);
        }
    }
}

/**
 * @brief tbmSuper2Stream function that moves triangular banded matrix with super diagonals from memory to stream
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 * @tparam t_ParBlocks number of t_ParEntries, p_n must be multiple t_ParEntries * t_ParBlocks
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of superdiagonals
 * @param p_a a p_n x p_n triangular banded matrix with on-chip column-major storage and corresponding 0 paddings
 * @param p_out output stream, which is row-aligned with 0 paddings along subdiagonals
 */
template <typename t_DataType, unsigned int t_ParEntries, unsigned int t_ParBlocks = 1>
void tbmSuper2Stream(unsigned int p_n,
                     unsigned int p_k,
                     t_DataType* p_a,
                     hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
#ifndef __SYNTHESIS__
    assert((p_n % (t_ParEntries * t_ParBlocks)) == 0);
#endif
#pragma HLS DATAFLOW
    hls::stream<WideType<t_DataType, t_ParEntries> > l_str;
    readTbMat2Stream<t_DataType, t_ParEntries>(p_n, p_k, p_a, l_str);
    processUpSbMatStream<t_DataType, t_ParEntries, -1>(p_n, p_k, l_str, p_out);
}

/**
 * @brief tbmSub2Stream function that moves triangular banded matrix with sub diagonals from memory to stream
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 * @tparam t_ParBlocks number of t_ParEntries, p_n must be multiple t_ParEntries * t_ParBlocks
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of subdiagonals
 * @param p_a a p_n x p_n triangular banded matrix with on-chip column-major storage and corresponding 0 paddings
 * @param p_out output stream, which is row-aligned with 0 paddings along subdiagonals
 */
template <typename t_DataType, unsigned int t_ParEntries, unsigned int t_ParBlocks = 1>
void tbmSub2Stream(unsigned int p_n,
                   unsigned int p_k,
                   t_DataType* p_a,
                   hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
#ifndef __SYNTHESIS__
    assert((p_n % (t_ParEntries * t_ParBlocks)) == 0);
#endif
#pragma HLS DATAFLOW
    hls::stream<WideType<t_DataType, t_ParEntries> > l_str;
    readTbMat2Stream<t_DataType, t_ParEntries>(p_n, p_k, p_a, l_str);
    processLoSbMatStream<t_DataType, t_ParEntries>(p_n, p_k, l_str, p_out);
}

/**
 * @brief vec2TbUpMatStream function that moves vector from memory to stream that matches the sbMat2Stream outputs
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of super/sub-diagonals
 * @param p_x vector input
 * @param p_out output stream
 */
template <typename t_DataType, unsigned int t_ParEntries>
void vec2TbUpMatStream(unsigned int p_n,
                       unsigned int p_k,
                       t_DataType* p_x,
                       hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
#ifndef __SYNTHESIS__
    assert((p_n % t_ParEntries) == 0);
#endif
#pragma HLS DATAFLOW
    hls::stream<WideType<t_DataType, t_ParEntries> > l_str;
    readVec2TbStream<t_DataType, t_ParEntries>(p_n, p_k, p_x, l_str);
    processUpSbMatStream<t_DataType, t_ParEntries, -1>(p_n, p_k, l_str, p_out);
} // end vec2TbUpMatStream

/**
 * @brief vec2TbLoMatStream function that moves vector from memory to stream that matches the sbMat2Stream outputs
 *
 * @tparam t_DataType the data type of the matrix entries
 * @tparam t_ParEntries number of parallelly processed entries in the matrix
 *
 * @param p_n number of rows/cols in a square matrix
 * @param p_k number of sub-diagonals
 * @param p_x vector input
 * @param p_out output stream
 */
template <typename t_DataType, unsigned int t_ParEntries>
void vec2TbLoMatStream(unsigned int p_n,
                       unsigned int p_k,
                       t_DataType* p_x,
                       hls::stream<WideType<t_DataType, t_ParEntries> >& p_out) {
#ifndef __SYNTHESIS__
    assert((p_n % t_ParEntries) == 0);
#endif
#pragma HLS DATAFLOW
    hls::stream<WideType<t_DataType, t_ParEntries> > l_str;
    readVec2TbStream<t_DataType, t_ParEntries>(p_n, p_k, p_x, l_str);
    processLoSbMatStream<t_DataType, t_ParEntries>(p_n, p_k, l_str, p_out);
} // end vec2TbLoMatStream

} // namespace blas
} // namespace linear_algebra
} // namespace xf
#endif
