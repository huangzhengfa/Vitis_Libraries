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
 * @file utils.h
 * @brief common datatypes for L1 modules.
 * 
 * This file is part of XF BLAS Library.
 */

#ifndef XF_BLAS_UTILS_H
#define XF_BLAS_UTILS_H

#include <stdint.h>
#include <ostream>
#include <iomanip>
#include <iostream>
#include "hls_math.h"
#include "hls_stream.h"
#include "ap_int.h"
#include "ap_shift_reg.h"
#include "types.h"

namespace xf {
namespace linear_algebra {
namespace blas {
// Helper macros for renaming kernel
#define PASTER(x,y) x ## y
#define EVALUATOR(x,y)  PASTER(x,y)

constexpr size_t mylog2(size_t n) {
  return ( (n<2) ? 0 : 1+mylog2(n/2));
}

template<typename t_DataType, 
  unsigned int t_Entries, 
  typename t_SumType = t_DataType>
  class BinarySum{
    public:
      static const t_SumType sum(t_DataType p_x[t_Entries]){
        const unsigned int l_halfEntries = t_Entries >> 1;
        return BinarySum<t_DataType, l_halfEntries, t_SumType>::sum(p_x) +
          BinarySum<t_DataType, l_halfEntries, t_SumType>::sum(p_x + l_halfEntries);
      }
  };
template<typename t_DataType, 
  typename t_SumType> 
  class BinarySum<t_DataType, 1, t_SumType>{
    public:
      static const t_SumType sum(t_DataType p_x[1]){
        return p_x[0];
      }
  };

template<typename t_DataType>
class AdderDelay{
  public:
    static const unsigned int m_logDelays = 0;
};

template<>
class AdderDelay<double>{
  public:
    static const unsigned int m_logDelays = 3;
};

template<>
class AdderDelay<float>{
  public:
    static const unsigned int m_logDelays = 2;
};

}
}
}
#endif
