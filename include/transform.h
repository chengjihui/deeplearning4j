/*
 * transform.h
 *
 *  Created on: Dec 28, 2015
 *  @author: agibsonccc
 *  @author: raver119@gmail.com
 */

#ifndef TRANSFORM_H_
#define TRANSFORM_H_
#include <vector>
#include <templatemath.h>
#include <ops.h>
#include <omp.h>
#include <pairwise_util.h>
#include <dll.h>
#include "reduce.h"
#include "scalar.h"
#include "indexreduce.h"
#include "broadcasting.h"
#include <shape.h>
#include <ops.h>
#include <special_ops.h>

#ifdef __CUDACC__
#include <helper_cuda.h>
#endif

#ifdef __JNI__
#include <jni.h>
#endif
#ifdef __CUDACC__
#include <cuda.h>
#include <cuda_runtime.h>
#endif


namespace functions {
    namespace transform {

        template<typename T>
        class Transform {
        public:

#ifdef __CUDACC__
            /**
	 * Cuda implementation of transform
	 * @param dx
	 * @param xShapeInfo
	 * @param result
	 * @param resultShapeInfo
	 * @param extraParams
	 * @param n
	 */
	virtual __inline__ __device__ void transform(
			T *dy,
			int *shapeInfo,
			T *params,
			T *result,
			int *indexes) {
		Nd4jIndex n = shape::length(shapeInfo);
		int totalThreads = gridDim.x * blockDim.x;
		int tid = threadIdx.x;
		Nd4jIndex i = blockIdx.x * blockDim.x + tid;

		/* equal, positive, non-unit increments. */
#pragma unroll
		for (; i < n; i+= totalThreads) {
			result[indexes[i]] = op(dy[indexes[i]], params);
		}

	}


	/**
	 * Cuda implementation of transform
	 * @param dx
	 * @param xShapeInfo
	 * @param result
	 * @param resultShapeInfo
	 * @param extraParams
	 * @param n
	 */
template<typename OpType>
static __inline__ __device__ void transformCuda(
			T *dy,
			int *shapeInfo,
			T *params,
			T *result,
			int *resultShapeInfo,
			int *allocationPointer, T *reductionPointer, UnifiedSharedMemory *manager) {

		if(OpType::requiresSpecial) {
			OpType::execSpecialCuda(dy,shapeInfo,result,resultShapeInfo,params, allocationPointer, reductionPointer, manager);
			return;
		} else {

		int *xShape = shape::shapeOf(shapeInfo);
		int *xStride = shape::stride(shapeInfo);
		char xOrder = shape::order(shapeInfo);
		char resultOrder = shape::order(resultShapeInfo);
		int xRank = shape::rank(shapeInfo);
		int xOffset = shape::offset(shapeInfo);

		int xElementWiseStride = shape::elementWiseStride(shapeInfo);
		int resultElementWiseStride = shape::elementWiseStride(resultShapeInfo);
		int tid = blockIdx.x * blockDim.x + threadIdx.x;


		__shared__ int length;
		if(threadIdx.x == 0)
			length = shape::length(shapeInfo);
		__syncthreads();

		if(xElementWiseStride >= 1 && resultElementWiseStride >= 1 && xOrder == resultOrder) {
			transformCuda<OpType>(
					length,
					dy,
					xElementWiseStride,
					params,
					result,
					resultElementWiseStride, allocationPointer, reductionPointer, manager);
		}
		else {
			/* equal, positive, non-unit increments. */
			//long allocSize = sizeof(int) * xRank;
			//int *xIdx = shape::cuMalloc(manager->getT1ShapeBuffer(), allocSize);
			int xCoord[MAX_RANK];

#pragma unroll
			for (int i = tid; i < length; i+= gridDim.x * blockDim.x) {
				//int *xIdx = shape::ind2sub(xRank, xShape, i, xIdx);
				shape::ind2sub(xRank,shape::shapeOf(shapeInfo),i, xCoord);
				Nd4jIndex xOffset2 = shape::getOffset(xOffset, xShape, xStride, xCoord, xRank);
				Nd4jIndex resultOffset2 = shape::getOffset(0,xShape,shape::stride(resultShapeInfo),xCoord,xRank);
				result[resultOffset2] = OpType::op(dy[xOffset2], params);
			}
		}
	  }
	}

	/**
	 * Cuda implementation of transform
	 * @param dx
	 * @param xShapeInfo
	 * @param result
	 * @param resultShapeInfo
	 * @param extraParams
	 * @param n
	 */
template<typename OpType>
	static  __inline__ __device__ void transformCuda(
			Nd4jIndex n,
			T *dy,
			int incy,
			T *params,
			T *result,
			int resultStride,
			int *allocationPointer, T *reductionPointer, UnifiedSharedMemory *manager) {
		int totalThreads = gridDim.x * blockDim.x;
		Nd4jIndex i = blockIdx.x * blockDim.x + threadIdx.x;

		if(incy == 1 && resultStride == 1) {
			/* equal, positive, non-unit increments. */
#pragma unroll
			for (; i < n; i += totalThreads) {
				result[i] = OpType::op(dy[i], params);
			}
		}
		else {
			/* equal, positive, non-unit increments. */
#pragma unroll
			for (; i < n; i += totalThreads) {
				result[i * resultStride] = OpType::op(dy[i * incy], params);
			}
		}


	}

	static  __inline__ __device__ void transformCuda(
			const int op,
			T *dy,
			int *shapeInfo,
			T *params,
			T *result,
			int *resultShapeInfo,
			int *allocationPointer,
			T *reductionPointer,
			UnifiedSharedMemory *manager) {

				if (op == 0)
					transformCuda<simdOps::Abs<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 1)
					transformCuda<simdOps::Ceiling<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 2)
					transformCuda<simdOps::Cosine<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 3)
					transformCuda<simdOps::Exp<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 4)
					transformCuda<simdOps::Floor<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 5)
					transformCuda<simdOps::Log<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 6)
					transformCuda<simdOps::Neg<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 7)
					transformCuda<simdOps::Pow<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 8)
					transformCuda<simdOps::Round<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 9)
					transformCuda<simdOps::SetRange<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 10)
					transformCuda<simdOps::Sigmoid<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 11)
					transformCuda<simdOps::Sign<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 12)
					transformCuda<simdOps::Sin<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 13)
					transformCuda<simdOps::SoftPlus<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 14)
					transformCuda<simdOps::Sqrt<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 15)
					transformCuda<simdOps::Tanh<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 16)
					transformCuda<simdOps::ACos<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 17)
					transformCuda<simdOps::ASin<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 18)
					transformCuda<simdOps::ATan<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 19)
					transformCuda<simdOps::HardTanh<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 20)
					transformCuda<simdOps::SoftSign<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 21)
					transformCuda<simdOps::ELU<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 22)
					transformCuda<simdOps::ELUDerivative<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 23)
					transformCuda<simdOps::TanhDerivative<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 24)
					transformCuda<simdOps::TimesOneMinus<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 25)
					transformCuda<simdOps::HardTanhDerivative<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 26)
					transformCuda<simdOps::Ones<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 27)
					transformCuda<simdOps::Identity<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 28)
					transformCuda<simdOps::Stabilize<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 29)
					transformCuda<simdOps::SigmoidDerivative<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 30)
					transformCuda<simdOps::SoftSignDerivative<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 31)
					transformCuda<simdOps::LeakyRELU<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 32)
					transformCuda<simdOps::LeakyRELUDerivative<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 33)
					transformCuda<simdOps::RELU<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 34)
					transformCuda<simdOps::Step<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 35)
					transformCuda<simdOps::OneMinus<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 36)
					transformCuda<simdOps::Col2Im<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 37)
					transformCuda<simdOps::Im2col<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 38)
					transformCuda<simdOps::SoftMax<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 39)
					transformCuda<simdOps::SoftMaxDerivative<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 40)
					transformCuda<simdOps::LogSoftMax<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 41)
					transformCuda<simdOps::IsMax<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else if (op == 42)
					transformCuda<simdOps::SpecialDerivative<T>>(dy, shapeInfo, params, result, resultShapeInfo, allocationPointer, reductionPointer, manager);
				else printf("[ERROR] Unknow opNum %d for transform\n", op);
	}


	static  __inline__ __device__ void transformCuda(
			const int op,
			Nd4jIndex n,
			T *dy,
			int incy,
			T *params,
			T *result,
			int resultStride,
			int *allocationPointer,
			T *reductionPointer,
			UnifiedSharedMemory *manager) {

				if (op == 0)
					transformCuda<simdOps::Abs<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 1)
					transformCuda<simdOps::Ceiling<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 2)
					transformCuda<simdOps::Cosine<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 3)
					transformCuda<simdOps::Exp<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 4)
					transformCuda<simdOps::Floor<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 5)
					transformCuda<simdOps::Log<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 6)
					transformCuda<simdOps::Neg<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 7)
					transformCuda<simdOps::Pow<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 8)
					transformCuda<simdOps::Round<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 9)
					transformCuda<simdOps::SetRange<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 10)
					transformCuda<simdOps::Sigmoid<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 11)
					transformCuda<simdOps::Sign<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 12)
					transformCuda<simdOps::Sin<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 13)
					transformCuda<simdOps::SoftPlus<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 14)
					transformCuda<simdOps::Sqrt<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 15)
					transformCuda<simdOps::Tanh<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 16)
					transformCuda<simdOps::ACos<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 17)
					transformCuda<simdOps::ASin<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 18)
					transformCuda<simdOps::ATan<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 19)
					transformCuda<simdOps::HardTanh<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 20)
					transformCuda<simdOps::SoftSign<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 21)
					transformCuda<simdOps::ELU<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 22)
					transformCuda<simdOps::ELUDerivative<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 23)
					transformCuda<simdOps::TanhDerivative<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 24)
					transformCuda<simdOps::TimesOneMinus<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 25)
					transformCuda<simdOps::HardTanhDerivative<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 26)
					transformCuda<simdOps::Ones<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 27)
					transformCuda<simdOps::Identity<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 28)
					transformCuda<simdOps::Stabilize<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 29)
					transformCuda<simdOps::SigmoidDerivative<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 30)
					transformCuda<simdOps::SoftSignDerivative<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 31)
					transformCuda<simdOps::LeakyRELU<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 32)
					transformCuda<simdOps::LeakyRELUDerivative<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 33)
					transformCuda<simdOps::RELU<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 34)
					transformCuda<simdOps::Step<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 35)
					transformCuda<simdOps::OneMinus<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 36)
					transformCuda<simdOps::Col2Im<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 37)
					transformCuda<simdOps::Im2col<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 38)
					transformCuda<simdOps::SoftMax<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 39)
					transformCuda<simdOps::SoftMaxDerivative<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 40)
					transformCuda<simdOps::LogSoftMax<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 41)
					transformCuda<simdOps::IsMax<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else if (op == 42)
					transformCuda<simdOps::SpecialDerivative<T>>(n, dy, incy, params, result, resultStride, allocationPointer, reductionPointer, manager);
				else printf("[ERROR] Unknow opNum %d for transform\n", op);
	}
#endif


			static void exec(
				int op,
				T *dx,
				int xStride,
				T *result,
				int resultStride,
				T *extraParams,
				const int n) {
				if (op == 0) {
					exec<simdOps::Abs>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 1) {
					exec<simdOps::Ceiling>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 2) {
					exec<simdOps::Cosine>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 3) {
					exec<simdOps::Exp>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 4) {
					exec<simdOps::Floor>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 5) {
					exec<simdOps::Log>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 6) {
					exec<simdOps::Neg>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 7) {
					exec<simdOps::Pow>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 8) {
					exec<simdOps::Round>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 9) {
					exec<simdOps::SetRange>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 10) {
					exec<simdOps::Sigmoid>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 11) {
					exec<simdOps::Sign>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 12) {
					exec<simdOps::Sin>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 13) {
					exec<simdOps::SoftPlus>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 14) {
					exec<simdOps::Sqrt>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 15) {
					exec<simdOps::Tanh>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 16) {
					exec<simdOps::ACos>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 17) {
					exec<simdOps::ASin>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 18) {
					exec<simdOps::ATan>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 19) {
					exec<simdOps::HardTanh>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 20) {
					exec<simdOps::SoftSign>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 21) {
					exec<simdOps::ELU>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 22) {
					exec<simdOps::ELUDerivative>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 23) {
					exec<simdOps::TanhDerivative>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 24) {
					exec<simdOps::TimesOneMinus>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 25) {
					exec<simdOps::HardTanhDerivative>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 26) {
					exec<simdOps::Ones>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 27) {
					exec<simdOps::Identity>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 28) {
					exec<simdOps::Stabilize>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 29) {
					exec<simdOps::SigmoidDerivative>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 30) {
					exec<simdOps::SoftSignDerivative>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 31) {
					exec<simdOps::LeakyRELU>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 32) {
					exec<simdOps::LeakyRELUDerivative>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 33) {
					exec<simdOps::RELU>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 34) {
					exec<simdOps::Step>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 35) {
					exec<simdOps::OneMinus>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 36) {
					exec<simdOps::Col2Im>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 37) {
					exec<simdOps::Im2col>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 38) {
					exec<simdOps::SoftMax>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 39) {
					exec<simdOps::SoftMaxDerivative>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 40) {
					exec<simdOps::LogSoftMax>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 41) {
					exec<simdOps::IsMax>(dx, xStride, result, resultStride, extraParams, n);
				}
				else if (op == 42) {
					// temporary special op for blockwise SoftMax Derivative
					exec<simdOps::SpecialDerivative>(dx, xStride, result, resultStride, extraParams, n);
				}
				else {
					printf("[ERROR] Unknow opNum %d for transform\n", op);
				}
			}

			static void exec(
				int op,
				T *dx,
				int *xShapeInfo,
				T *result,
				int *resultShapeInfo,
				T *extraParams,
				Nd4jIndex *indexes,
				Nd4jIndex *resultIndexes) {
				if (op == 0) {
					exec<simdOps::Abs>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 1) {
					exec<simdOps::Ceiling>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 2) {
					exec<simdOps::Cosine>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 3) {
					exec<simdOps::Exp>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 4) {
					exec<simdOps::Floor>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 5) {
					exec<simdOps::Log>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 6) {
					exec<simdOps::Neg>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 7) {
					exec<simdOps::Pow>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 8) {
					exec<simdOps::Round>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 9) {
					exec<simdOps::SetRange>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 10) {
					exec<simdOps::Sigmoid>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 11) {
					exec<simdOps::Sign>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 12) {
					exec<simdOps::Sin>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 13) {
					exec<simdOps::SoftPlus>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 14) {
					exec<simdOps::Sqrt>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 15) {
					exec<simdOps::Tanh>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 16) {
					exec<simdOps::ACos>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 17) {
					exec<simdOps::ASin>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 18) {
					exec<simdOps::ATan>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 19) {
					exec<simdOps::HardTanh>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 20) {
					exec<simdOps::SoftSign>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 21) {
					exec<simdOps::ELU>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 22) {
					exec<simdOps::ELUDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 23) {
					exec<simdOps::TanhDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 24) {
					exec<simdOps::TimesOneMinus>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 25) {
					exec<simdOps::HardTanhDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 26) {
					exec<simdOps::Ones>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 27) {
					exec<simdOps::Identity>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 28) {
					exec<simdOps::Stabilize>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 29) {
					exec<simdOps::SigmoidDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 30) {
					exec<simdOps::SoftSignDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 31) {
					exec<simdOps::LeakyRELU>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 32) {
					exec<simdOps::LeakyRELUDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 33) {
					exec<simdOps::RELU>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 34) {
					exec<simdOps::Step>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 35) {
					exec<simdOps::OneMinus>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 36) {
					exec<simdOps::Col2Im>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 37) {
					exec<simdOps::Im2col>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 38) {
					exec<simdOps::SoftMax>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 39) {
					exec<simdOps::SoftMaxDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 40) {
					exec<simdOps::LogSoftMax>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 41) {
					exec<simdOps::IsMax>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else if (op == 42) {
					// temporary special op for blockwise SoftMax Derivative
					exec<simdOps::SpecialDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams, indexes, resultIndexes);
				}
				else {
					printf("[ERROR] Unknow opNum %d for transform\n", op);
				}
			}


			static void exec(
				int op,
				T *dx,
				int *xShapeInfo,
				T *result,
				int *resultShapeInfo,
				T *extraParams) {
				if (op == 0) {
					exec<simdOps::Abs>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 1) {
					exec<simdOps::Ceiling>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 2) {
					exec<simdOps::Cosine>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 3) {
					exec<simdOps::Exp>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 4) {
					exec<simdOps::Floor>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 5) {
					exec<simdOps::Log>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 6) {
					exec<simdOps::Neg>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 7) {
					exec<simdOps::Pow>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 8) {
					exec<simdOps::Round>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 9) {
					exec<simdOps::SetRange>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 10) {
					exec<simdOps::Sigmoid>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 11) {
					exec<simdOps::Sign>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 12) {
					exec<simdOps::Sin>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 13) {
					exec<simdOps::SoftPlus>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 14) {
					exec<simdOps::Sqrt>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 15) {
					exec<simdOps::Tanh>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 16) {
					exec<simdOps::ACos>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 17) {
					exec<simdOps::ASin>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 18) {
					exec<simdOps::ATan>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 19) {
					exec<simdOps::HardTanh>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 20) {
					exec<simdOps::SoftSign>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 21) {
					exec<simdOps::ELU>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 22) {
					exec<simdOps::ELUDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 23) {
					exec<simdOps::TanhDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 24) {
					exec<simdOps::TimesOneMinus>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 25) {
					exec<simdOps::HardTanhDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 26) {
					exec<simdOps::Ones>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 27) {
					exec<simdOps::Identity>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 28) {
					exec<simdOps::Stabilize>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 29) {
					exec<simdOps::SigmoidDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 30) {
					exec<simdOps::SoftSignDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 31) {
					exec<simdOps::LeakyRELU>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 32) {
					exec<simdOps::LeakyRELUDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 33) {
					exec<simdOps::RELU>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 34) {
					exec<simdOps::Step>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 35) {
					exec<simdOps::OneMinus>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 36) {
					exec<simdOps::Col2Im>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 37) {
					exec<simdOps::Im2col>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 38) {
					exec<simdOps::SoftMax>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 39) {
					exec<simdOps::SoftMaxDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 40) {
					exec<simdOps::LogSoftMax>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 41) {
					exec<simdOps::IsMax>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else if (op == 42) {
					// temporary special op for blockwise SoftMax Derivative
					exec<simdOps::SpecialDerivative>(dx, xShapeInfo, result, resultShapeInfo, extraParams);
				}
				else {
					printf("[ERROR] Unknow opNum %d for transform\n", op);
				}
			}


			template<template <typename> typename OpType>
			static void exec(
                    T *dx,
                    int *xShapeInfo,
                    T *result,
                    int *resultShapeInfo,
                    T *extraParams) {

                if(OpType<T>::requiresSpecial) {
                    OpType<T>::execSpecial(dx,xShapeInfo,result,resultShapeInfo,extraParams);
                    return;
                }

                int n = shape::length(xShapeInfo);
                int xElementWiseStride = shape::elementWiseStride(xShapeInfo);
                int resultElementWiseStride = shape::elementWiseStride(resultShapeInfo);
                if(xElementWiseStride >= 1 && resultElementWiseStride >= 1 && shape::order(xShapeInfo) == shape::order(resultShapeInfo)) {
                    exec<OpType>(dx,xElementWiseStride,result,resultElementWiseStride,extraParams,n);
                }
                else {
                    int shapeIter[MAX_RANK];
                    int coord[MAX_RANK];
                    int dim;
                    int xStridesIter[MAX_RANK];
                    int resultStridesIter[MAX_RANK];
                    int *xShape = shape::shapeOf(xShapeInfo);
                    int *xStride = shape::stride(xShapeInfo);
                    int *resultStride = shape::stride(resultShapeInfo);
                    int rank = shape::rank(xShapeInfo);
                    if(PrepareTwoRawArrayIter<T>(rank,
                                                 xShape,
                                                 dx,
                                                 xStride,
                                                 result,
                                                 resultStride,
                                                 &rank,
                                                 shapeIter,
                                                 &dx,
                                                 xStridesIter,
                                                 &result,
                                                 resultStridesIter) >= 0) {
                        ND4J_RAW_ITER_START(dim, rank, coord, shapeIter);
                        {
                            /* Process the innermost dimension */
                            T *xIter = dx;
                            T *resultIter = result;
                            resultIter[0] = OpType<T>::op(xIter[0], extraParams);
                        }
                        ND4J_RAW_ITER_TWO_NEXT(dim,
                                               rank,
                                               coord,
                                               shapeIter,
                                               dx,
                                               xStridesIter,
                                               result,
                                               resultStridesIter);

                    }

                }

            }


			template<template <typename> typename OpType>
			static void exec(
				T *dx,
				int *xShapeInfo,
				T *result,
				int *resultShapeInfo,
				T *extraParams,
				Nd4jIndex *indexes,
				Nd4jIndex *resultIndexes) {

				int n = shape::length(xShapeInfo);
#pragma omp parallel for simd schedule(guided)
				for (int i = 0; i < n; i++) {
					result[resultIndexes[i]] = OpType<T>::op(dx[indexes[i]], extraParams);
				}
			}

			template<template <typename> typename OpType>
			static void exec(T *dx,
                              int xStride,
                              T *result,
                              int resultStride,
                              T *extraParams,
                              const int n) {
                if (xStride == 1 && resultStride == 1) {
#pragma omp parallel for simd schedule(guided) if (n > 2048)
                        for (int i = 0; i < n; i++) {
                            result[i] = OpType<T>::op(dx[i], extraParams);
                        }
                } else {
#pragma omp parallel for simd schedule(guided) if (n > 2048)
                        for (int i = 0; i < n; i++) {
                            result[i * resultStride] = OpType<T>::op(dx[i * xStride], extraParams);
                        }
                }
            }
        };
    }
}




#ifdef __CUDACC__
/*
 * 	T *dy,
			int *shapeInfo,
			T *params,
			T *result,
			int *indexes
 */



/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
template <typename T>
__device__ void transformGeneric(
		int opNum,
		Nd4jIndex n,
		T *dy,
		int incy,
		T *params,
		T *result,
		int resultStride, int *allocationPointer, T *reductionPointer) {

	__shared__ UnifiedSharedMemory *manager;

	if(threadIdx.x == 0) {
	    extern __shared__ unsigned char shmem[];
        manager = new(shmem) UnifiedSharedMemory((int *) shmem);
	    manager->init(sizeof(UnifiedSharedMemory), 0, sizeof(functions::transform::Transform<T>), sizeof(shape::TAD), 0);
	}
	__syncthreads();

	functions::transform::Transform<T>::transformCuda(
		opNum,
		n,
		dy,
		incy,
		params,
		result,
		resultStride,
		allocationPointer,
		reductionPointer,
		manager);
}

/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
__global__ void transformDouble(
		int opNum,
		Nd4jIndex n,
		double *dy,
		int incy,
		double *params,
		double *result,int resultStride, int *allocationPointer, double *reductionPointer) {

	transformGeneric<double>(
			opNum,
			n,
			dy,
			incy,
			params,
			result,
			resultStride, allocationPointer, reductionPointer);
}

/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
__global__ void transformFloat(
		int opNum,
		Nd4jIndex n,
		float *dy,
		int incy,
		float *params,
		float *result,int resultStride, int *allocationPointer, float *reductionPointer) {

	transformGeneric<float>(
			opNum,
			n,
			dy,
			incy,
			params,
			result,resultStride, allocationPointer, reductionPointer);

}

/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
template <typename T>
__device__ void transformGeneric(
		int opNum,
		T *dy,
		int *xShapeInfo, int xRank,
		T *params,
		T *result,int *resultShapeInfo, int zRank, int *allocationPointer, T *reductionPointer) {

	__shared__ UnifiedSharedMemory *manager;

    if (threadIdx.x == 0) {
        extern __shared__ unsigned char shmem[];
        manager = new(shmem) UnifiedSharedMemory((int *) shmem);
	    manager->init(sizeof(UnifiedSharedMemory), 0, sizeof(functions::transform::Transform<T>), sizeof(shape::TAD), xRank);
	}
	__syncthreads();


	functions::transform::Transform<T>::transformCuda(
	    opNum,
	    dy,
	    xShapeInfo,
	    params,
	    result,
	    resultShapeInfo,
	    allocationPointer,
	    reductionPointer,
	    manager);
}



/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
extern "C" __global__ void transformDouble(
		int opNum,
		double *dy,
		int *shapeInfo, int xRank,
		double *params,
		double *result,int *resultShapeInfo, int zRank, int *allocationPointer, double *reductionPointer) {

	transformGeneric<double>(
			opNum,
			dy,
			shapeInfo, xRank,
			params,
			result,resultShapeInfo, zRank, allocationPointer, reductionPointer);
}

/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
extern "C" __global__ void transformFloat(
		int opNum,
		float *dy,
		int *shapeInfo, int xRank,
		float *params,
		float *result,int *resultShapeInfo, int zRank, int *allocationPointer, float *reductionPointer) {

	transformGeneric<float>(
			opNum,
			dy,
			shapeInfo, xRank,
			params,
			result,
			resultShapeInfo, zRank, allocationPointer, reductionPointer);

}



/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
template <typename T>
__device__ void transformGenericIndexes(
		int opNum,
		T *dy,
		int *xShapeInfo, int xRank,
		T *params,
		T *result,int *indexes, int *allocationPointer, T *reductionPointer) {

	__shared__ UnifiedSharedMemory *manager;

    if (threadIdx.x == 0) {
        extern __shared__ unsigned char shmem[];
        manager = new(shmem) UnifiedSharedMemory((int *) shmem);
	    manager->init(sizeof(UnifiedSharedMemory), 0, sizeof(functions::transform::Transform<T>), sizeof(shape::TAD), xRank);
	}
	__syncthreads();


	functions::transform::Transform<T>::transformCuda(
	        opNum,
	        dy,
	        xShapeInfo,
	        params,
	        result,
	        indexes,
	        allocationPointer,
	        reductionPointer,
	        manager);
}



/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
extern "C" __global__ void transformDoubleIndexes(
		int opNum,
		double *dy,
		int *shapeInfo, int xRank,
		double *params,
		double *result,int *indexes, int *allocationPointer, double *reductionPointer) {

	transformGenericIndexes<double>(
			opNum,
			dy,
			shapeInfo, xRank,
			params,
			result,indexes, allocationPointer, reductionPointer);
}

/**
 * The c and driver interface
 *  for th kernels
 * @param opNum the op number
 * @param n the length of the problem
 * @param idx
 * the start index
 * @param dy the vector to transform
 * @param incy the stride for the vector
 * @param params the extra parameters for the problem
 * @param result the result storage
 * @param blockernelHeight the block size for the problem
 */
extern "C" __global__ void transformFloatIndexes(
		int opNum,
		float *dy,
		int *shapeInfo, int xRank,
		float *params,
		float *result,int *indexes, int *allocationPointer, float *reductionPointer) {

	transformGenericIndexes<float>(
			opNum,
			dy,
			shapeInfo, xRank,
			params,
			result,indexes, allocationPointer, reductionPointer);

}

/**
* This is utility kernel, that updates given special buffer with proper values in device memory
*/
extern "C" __global__ void prepareShapeBuffer(int *dimension, int *maxDimension, int *specialPointer, int rows) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid > 0)
        return;

    dimension[0] = 0;
    maxDimension[0] = 1;

    specialPointer[0] = 2;
    specialPointer[1] = rows;
    specialPointer[2] = 1;
    specialPointer[3] = 1;
    specialPointer[4] = 1;
    specialPointer[5] = 0;
    specialPointer[6] = 1;
    specialPointer[7] = 99;
}

extern "C" __global__ void prepareDimensionalShapeBuffer(int *xShapeInfoBuffer, float *extraParams, int *zShapeInfo) {
    // extraParams[0] - number of dimensions
    // extraParams[1] - dimension
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid > 0)
        return;

    int targetDimension = (int) extraParams[1];
    printf("Target dimension: [%i]\n", targetDimension);

    int targetWidth = shape::shapeOf(xShapeInfoBuffer)[targetDimension];
    printf("Target rank: [%i]\n", targetWidth);
}

template <typename T>
__device__ void fillIsMaxGeneric(T *dx, long length, long idx) {

   int tid = blockIdx.x * blockDim.x + threadIdx.x;
   for (long i = tid; i < length; i+= blockDim.x * gridDim.x) {
        dx[i] = (i == idx? 1.0 : 0.0);
   }
}

extern "C" __global__ void fillIsMaxFloat(float *dx, long length, long idx) {
    fillIsMaxGeneric<float>(dx, length, idx);
}

extern "C" __global__ void fillIsMaxDouble(double *dx, long length, long idx) {
    fillIsMaxGeneric<double>(dx, length, idx);
}


template <typename T>
__device__ void fillDimensionalIsMaxGeneric(T *dX, int *xShapeInfo, T *dZ, int *zShapeInfo, int *tadOnlyShapeInfo, int *dimension, int dimensionLength, int *tadOffsets) {

    __shared__ int tadLength;
    __shared__ int tadEWS;
    __shared__ int numTads;

    if (threadIdx.x == 0) {

        tadLength = shape::tadLength(zShapeInfo, dimension, dimensionLength);
        tadEWS = shape::elementWiseStride(tadOnlyShapeInfo);
        numTads = shape::length(zShapeInfo) / tadLength;
/*
        if (blockIdx.x == 0) {
            printf("original X shape: \n");
            shape::printShapeInfoLinear(xShapeInfo);

            printf("original Z shape: \n");
            shape::printShapeInfoLinear(zShapeInfo);

            printf("Target dimension: [%i], dimensionLength: [%i], numTads: [%i], rnumTads: [%i]\n", dimension[0], dimensionLength, numTads, tad->numTads);

            printf("TAD shape: \n");
            shape::printShapeInfoLinear(tadOnlyShapeInfo);

            printf("TAD shape2: \n");
            shape::printShapeInfoLinear(tad->tadOnlyShapeInfo);
        }
        */
    }
    __syncthreads();


    for (int r = blockIdx.x; r < numTads; r+= gridDim.x) {
        int tadOffsetForBlock = tadOffsets[r];

        int highestElement = (int) dX[r];
/*
        if (threadIdx.x == 0)
            printf("TAD: [%i], highestElement: [%i], numTads: [%i], tadLength: [%i]\n", r, highestElement, numTads, tadLength);
*/

        for (int e = threadIdx.x; e < tadLength; e += blockDim.x) {
            // so, we just set dZ[e] for each TAD. Sure, e should be replaced with
            dZ[tadOffsetForBlock + e * tadEWS] = (e == highestElement? 1.0 : 0.0);
        }
    }
}

extern "C" __global__ void fillDimensionalIsMaxFloat(float *dx, int *xShapeInfo, float *dz, int *zShapeInfo, int *tadOnlyShapeInfo, int *dimension, int dimensionLength, int *tadOffsets) {
    fillDimensionalIsMaxGeneric<float>(dx, xShapeInfo, dz, zShapeInfo, tadOnlyShapeInfo, dimension, dimensionLength, tadOffsets);
}

extern "C" __global__ void fillDimensionalIsMaxDouble(double *dx, int *xShapeInfo, double *dz, int *zShapeInfo, int *tadOnlyShapeInfo, int *dimension, int dimensionLength, int *tadOffsets) {
    fillDimensionalIsMaxGeneric<double>(dx, xShapeInfo, dz, zShapeInfo, tadOnlyShapeInfo, dimension, dimensionLength, tadOffsets);
}

template <typename T>
__device__ void concatKernelGeneric(int dimension,
									int numArrays,
									Nd4jPointer *data,
									Nd4jPointer *inputShapeInfos,
									T *result,
									int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {
	int tid = threadIdx.x + blockIdx.x * blockDim.x;

	__shared__ UnifiedSharedMemory *manager;

	int zRank = shape::rank(resultShapeInfo);

	if (threadIdx.x == 0) {
		extern __shared__ unsigned char shmem[];
		manager = new(shmem) UnifiedSharedMemory((int *) shmem);
		manager->init(sizeof(UnifiedSharedMemory), 0, 0, sizeof(shape::TAD), zRank + 2);
	}
	__syncthreads();

	T **dataT = (T **) data;
	int **shapeInfoPointers = (int **) inputShapeInfos;
	int **tadShapes = (int **) tadPointers;
	int **tadOffsets = (int **) offsetPointers;


    __shared__ int tDim[1];
    __shared__ int baseIdx;

		__shared__ shape::TAD *tad;
		__shared__ int yLength;
		__shared__ char yOrder;
		__shared__ int yEWS;
		if (threadIdx.x == 0) {
			tDim[0] = dimension;
			tad = new(manager->getTADSpace()) shape::TAD(); //(xShapeInfo,dimension,dimensionLength)
			tad->setExternalBuffers((void *) manager);
			//    tad->initWithExternalTAD(manager->getT1ShapeBuffer(), manager->getXShapeBuffer(), dimension, dimensionLength);
			tad->init(resultShapeInfo, tDim, 1);
			tad->createTadOnlyShapeInfo();
		}
		__syncthreads();

		char zOrder = shape::order(resultShapeInfo);

		int zEWS = shape::elementWiseStride(resultShapeInfo);
		int tadEWS = shape::elementWiseStride(tad->tadOnlyShapeInfo);
		int zLength = shape::length(resultShapeInfo);

        __shared__ int arrOffset;
		__shared__ int numTads;


        if (shape::isVector(resultShapeInfo)) {
			//if (threadIdx.x == 0)
				//printf("Vector here\n");
			if (zEWS >= 1) {
				for (int r = blockIdx.x; r < numArrays; r += gridDim.x) {
					if(shape::isVector(shapeInfoPointers[r]) || shape::order(shapeInfoPointers[r]) == shape::order(resultShapeInfo)) {
						yLength = shape::length(shapeInfoPointers[r]);
						yEWS = shape::elementWiseStride(shapeInfoPointers[r]);
						// FIXME: this is bad
						__shared__ int baseIdx;
						if (threadIdx.x == 0) {
							baseIdx = 0;
							for (int f = 0; f < r; f++) {
								baseIdx += shape::length(shapeInfoPointers[f]);
							}
						}
						__syncthreads();
						for (int i = threadIdx.x; i < yLength && baseIdx + i < zLength; i += blockDim.x) {
							result[baseIdx + i * zEWS] = dataT[r][i * yEWS];
						}
						__syncthreads();
					} else {
						if (tid == 0)
							printf("Non-matched order for vector\n");
					}
				}
			} else {
				if (tid == 0)
					printf("Vector Non-1 zEWS\n");
			}
			return;
		}


		// TODO: to be pulled into separate kernel. matrix concatenation
		for (int r = blockIdx.x; r < numArrays; r += gridDim.x) {

			int *currentShape = shapeInfoPointers[r];
			T *currentData = dataT[r];
			int *currentTad = tadShapes[r];
			int *currentOffsets = tadOffsets[r];


			if (threadIdx.x == 0) {
				//inputTAD = new((unsigned char *)managerInput->getTADSpace()) shape::TAD(); //(xShapeInfo,dimension,dimensionLength)
				//inputTAD->setExternalBuffers((void *) managerInput);
				//inputTAD->initWithExternalTAD(currentTad, currentShape, tDim, 1);
				//inputTAD->init(shapeInfoPointers[r], &dimension, 1);
				//inputTAD->createTadOnlyShapeInfo();

				yLength = shape::length(currentTad);
				yOrder = shape::order(currentTad);
				yEWS = shape::elementWiseStride(currentTad);
                numTads = shape::length(currentShape) / yLength;
			}
			__syncthreads();


			if (threadIdx.x == 0) {
				arrOffset = 0;
				for (int f = 0; f < r; f++) {
					arrOffset +=  shape::length(tadShapes[f]);
				}
			}
			__syncthreads();

			for (int j = 0; j < numTads; j++) {
				int inputOffset = currentOffsets[j];
				int resultOffset = tad->tadOffset(j);

				T *dataTAD = currentData + inputOffset;
				T *resultTAD = result + resultOffset;

                int sub[MAX_RANK];

				shape::ind2subC(shape::rank(tad->tadOnlyShapeInfo),shape::shapeOf(tad->tadOnlyShapeInfo),arrOffset, sub);
				Nd4jIndex baseOffset = shape::getOffset(0,shape::shapeOf(tad->tadOnlyShapeInfo),shape::stride(tad->tadOnlyShapeInfo), sub, shape::rank(tad->tadOnlyShapeInfo));

				resultTAD += baseOffset;

				if (zOrder == yOrder && yEWS > 0  && tadEWS > 0) {
					for (int i = threadIdx.x; i < yLength; i += blockDim.x) {
						resultTAD[i * tadEWS] = dataTAD[i * yEWS];
					}
				} else {
					if(tadEWS > 0 && shape::order(resultShapeInfo) == shape::order(currentTad)) {

						// FIXME: this is bad

						if (threadIdx.x == 0) {
							baseIdx = 0;
							for (int f = 0; f < r; f++) {
								baseIdx += shape::length(shapeInfoPointers[f]);
							}
						}
						__syncthreads();

						if (numTads == 1) {
							for(int k = threadIdx.x; k < yLength; k+= blockDim.x) {
								resultTAD[baseIdx + k * tadEWS] = dataTAD[k];
							}
						} else {
							int yIdx[MAX_RANK];
							int yRank = shape::rank(currentTad);

							for (int i = threadIdx.x; i < yLength; i+= blockDim.x) {
								shape::ind2sub(yRank, shape::shapeOf(currentTad), i, yIdx);
								int yOffset = shape::getOffset(0, shape::shapeOf(currentTad), shape::stride(currentTad), yIdx, yRank);

								resultTAD[baseIdx + i * tadEWS] =  dataTAD[yOffset];
							}
						}
						__syncthreads();
					} else {
						int yIdx[MAX_RANK];
						int yRank = shape::rank(currentTad);
						int tadRank = shape::rank(tad->tadOnlyShapeInfo);

						for (int i = threadIdx.x; i < yLength; i+= blockDim.x) {
							shape::ind2sub(yRank, shape::shapeOf(currentTad), i,yIdx);

							int yOffset = shape::getOffset(0, shape::shapeOf(currentTad), shape::stride(currentTad), yIdx, yRank);
							int resultOffset = shape::getOffset(0, shape::shapeOf(tad->tadOnlyShapeInfo), shape::stride(tad->tadOnlyShapeInfo), yIdx, tadRank);

							resultTAD[resultOffset] =  dataTAD[yOffset];
						}
					}
				}
				__syncthreads();
			}
			__syncthreads();

//			if (threadIdx.x == 0)
//				delete inputTAD;
		}

		if (threadIdx.x == 0)
			delete tad;
}

template <typename T>
__device__ void concatKernelScalarGeneric(int dimension,
									int numArrays,
									Nd4jPointer *data,
									Nd4jPointer *inputShapeInfos,
									T *result,
									int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {

    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    T **input = (T **) data;

    for (int i = tid; i < numArrays; i += blockDim.x * gridDim.x) {
			result[i] = input[i][0];
	}
}

extern "C" __global__ void concatKernelScalarFloat(int dimension,
											  int numArrays,
											  Nd4jPointer *data,
											  Nd4jPointer *inputShapeInfo,
											  float *result,
											  int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {

    concatKernelScalarGeneric<float>(dimension, numArrays, data, inputShapeInfo, result, resultShapeInfo, tadPointers, offsetPointers);
}

extern "C" __global__ void concatKernelScalarDouble(int dimension,
											  int numArrays,
											  Nd4jPointer *data,
											  Nd4jPointer *inputShapeInfo,
											  double *result,
											  int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {

    concatKernelScalarGeneric<double>(dimension, numArrays, data, inputShapeInfo, result, resultShapeInfo, tadPointers, offsetPointers);
}


template <typename T>
__device__ void concatKernelHStackGeneric(int dimension,
									int numArrays,
									Nd4jPointer *data,
									Nd4jPointer *inputShapeInfos,
									T *result,
									int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {
    // we expect all data coming in as vectors, and result as 2D matrix
    // the only significant difference here is the fact that input lengths might be different
    int **inputShapes = (int**) inputShapeInfos;
     T **input = (T **) data;

     __shared__ int inputEWS;
     __shared__ int resultEWS;
     __shared__ int inputLength;

     if (threadIdx.x == 0) {
        resultEWS = shape::elementWiseStride(resultShapeInfo);
     }
     __syncthreads();

     for (int r = blockIdx.x; r < numArrays; r+= gridDim.x) {

        __shared__ int baseIdx;
		if (threadIdx.x == 0) {
			baseIdx = 0;
			for (int f = 0; f < r; f++) {
			    baseIdx += shape::length(inputShapes[f]);
		    }
		}
		__syncthreads();


        T *inputData = (T *) input[r];

        if (threadIdx.x == 0) {
         inputEWS = shape::elementWiseStride(inputShapes[r]);
         inputLength = shape::length(inputShapes[r]);
        }
        __syncthreads();

        for(int i = threadIdx.x; i < inputLength; i += blockDim.x) {
            result[baseIdx + i * resultEWS] = inputData[i * inputEWS];
        }
        __syncthreads();
     }
}

extern "C" __global__ void concatKernelHStackFloat(int dimension,
											  int numArrays,
											  Nd4jPointer *data,
											  Nd4jPointer *inputShapeInfo,
											  float *result,
											  int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {

    concatKernelHStackGeneric<float>(dimension, numArrays, data, inputShapeInfo, result, resultShapeInfo, tadPointers, offsetPointers);
}

extern "C" __global__ void concatKernelHStackDouble(int dimension,
											  int numArrays,
											  Nd4jPointer *data,
											  Nd4jPointer *inputShapeInfo,
											  double *result,
											  int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {

    concatKernelHStackGeneric<double>(dimension, numArrays, data, inputShapeInfo, result, resultShapeInfo, tadPointers, offsetPointers);
}


template <typename T>
__device__ void concatKernelVStackGeneric(int dimension,
									int numArrays,
									Nd4jPointer *data,
									Nd4jPointer *inputShapeInfos,
									T *result,
									int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {

    /*
     this is special case for concat: we group bunch of vectors into 2D matrix
     also: we expect each inputShapeInfo to have EWS, be a vector, and have equal size
     */

     int **inputShapes = (int**) inputShapeInfos;
     T **input = (T **) data;

     __shared__ int inputEWS;
     __shared__ int resultEWS;
     __shared__ int inputLength;

     if (threadIdx.x == 0) {
        inputLength = shape::length(inputShapes[0]);
        resultEWS = shape::elementWiseStride(resultShapeInfo);
     }
     __syncthreads();

     for (int r = blockIdx.x; r < numArrays; r+= gridDim.x) {

        int resultOffset = r * inputLength * resultEWS;
        T *inputData = (T *) input[r];

        if (threadIdx.x == 0) {
         inputEWS = shape::elementWiseStride(inputShapes[r]);
        }
        __syncthreads();

        for(int i = threadIdx.x; i < inputLength; i += blockDim.x) {
            result[resultOffset + i * resultEWS] = inputData[i * inputEWS];
        }
        __syncthreads();
     }
}

extern "C" __global__ void concatKernelVStackFloat(int dimension,
											  int numArrays,
											  Nd4jPointer *data,
											  Nd4jPointer *inputShapeInfo,
											  float *result,
											  int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {

    concatKernelVStackGeneric<float>(dimension, numArrays, data, inputShapeInfo, result, resultShapeInfo, tadPointers, offsetPointers);
}

extern "C" __global__ void concatKernelVStackDouble(int dimension,
											  int numArrays,
											  Nd4jPointer *data,
											  Nd4jPointer *inputShapeInfo,
											  double *result,
											  int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {

    concatKernelVStackGeneric<double>(dimension, numArrays, data, inputShapeInfo, result, resultShapeInfo, tadPointers, offsetPointers);
}


extern "C" __global__ void concatKernelDouble(int dimension,
											  int numArrays,
											  Nd4jPointer *data,
											  Nd4jPointer *inputShapeInfo,
											  double *result,
											  int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {
	concatKernelGeneric<double>(dimension, numArrays, data, inputShapeInfo, result, resultShapeInfo, tadPointers, offsetPointers);
}

extern "C" __global__ void concatKernelFloat(int dimension,
											 int numArrays,
											 Nd4jPointer *data,
											 Nd4jPointer *inputShapeInfo,
											 float *result,
											 int *resultShapeInfo, Nd4jPointer *tadPointers, Nd4jPointer *offsetPointers) {
	concatKernelGeneric<float>(dimension, numArrays, data, inputShapeInfo, result, resultShapeInfo, tadPointers, offsetPointers);
}


#endif

#endif /* TRANSFORM_H_ */
