#ifndef ArrayOps_h
#define ArrayOps_h

#include "../Types.h"
#include "arrayUtils.h"
#include "typeDispatch.h"

// Core numeric binary op — returns same type as input
std::shared_ptr<arrow::ChunkedArray> applyBinaryOp(
    const std::shared_ptr<arrow::ChunkedArray> &left,
    const std::shared_ptr<arrow::ChunkedArray> &right,
    DataType type,
    BinaryOp op);

// Relational op — always returns boolean
std::shared_ptr<arrow::ChunkedArray> applyRelOp(
    const std::shared_ptr<arrow::ChunkedArray> &left,
    const std::shared_ptr<arrow::ChunkedArray> &right,
    DataType type,
    RelOp op);

// Boolean ops — inputs must be boolean
std::shared_ptr<arrow::ChunkedArray> applyBoolOp(
    const std::shared_ptr<arrow::ChunkedArray> &left,
    const std::shared_ptr<arrow::ChunkedArray> &right,
    BoolOp op);

// Unary ops
std::shared_ptr<arrow::ChunkedArray> applyUnaryOp(
    const std::shared_ptr<arrow::ChunkedArray> &arr,
    DataType type,
    UnaryOp op);

// String ops
std::shared_ptr<arrow::ChunkedArray> applyStringUnOp(
    const std::shared_ptr<arrow::ChunkedArray> &arr,
    StringUnOp op,
    const std::string &arg = "");

std::shared_ptr<arrow::ChunkedArray> applyStringBinOp(
    const std::shared_ptr<arrow::ChunkedArray> &left,
    const std::string &arg,
    StringBinOp op);

std::shared_ptr<arrow::ChunkedArray> applyAggOp(
    const std::shared_ptr<arrow::ChunkedArray> &arr,
    AggOp op);
#endif

template <typename T>
struct SumImpl
{
    std::shared_ptr<arrow::ChunkedArray> operator()(
        const std::shared_ptr<arrow::ChunkedArray> &arr)
    {
        auto val = computeSum<T>(arr);
        typename TypeTraits<T>::BuilderType builder;
        builder.Append(val);
        std::shared_ptr<arrow::Array> result;
        builder.Finish(&result);
        return toChunked(result);
    }
};

template <typename T>
struct MeanImpl
{
    std::shared_ptr<arrow::ChunkedArray> operator()(
        const std::shared_ptr<arrow::ChunkedArray> &arr)
    {
        auto val = computeMean<T>(arr);
        typename TypeTraits<T>::BuilderType builder;
        builder.Append(val);
        std::shared_ptr<arrow::Array> result;
        builder.Finish(&result);
        return toChunked(result);
    }
};

// MinImpl and MaxImpl follow identical pattern
template <typename T>
struct MinImpl
{
    std::shared_ptr<arrow::ChunkedArray> operator()(
        const std::shared_ptr<arrow::ChunkedArray> &arr)
    {
        auto val = computeMin<T>(arr);
        typename TypeTraits<T>::BuilderType builder;
        builder.Append(val);
        std::shared_ptr<arrow::Array> result;
        builder.Finish(&result);
        return toChunked(result);
    }
};

template <typename T>
struct MaxImpl
{
    std::shared_ptr<arrow::ChunkedArray> operator()(
        const std::shared_ptr<arrow::ChunkedArray> &arr)
    {
        auto val = computeMax<T>(arr);
        typename TypeTraits<T>::BuilderType builder;
        builder.Append(val);
        std::shared_ptr<arrow::Array> result;
        builder.Finish(&result);
        return toChunked(result);
    }
};