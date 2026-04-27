#ifndef ArrayOps_h
#define ArrayOps_h

#include "../Types.h"
#include "arrayUtils.h"
#include "typeDispatch.h"
#include "opsHelper.h"

namespace dataframelib
{
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

    template <DataType T>
    struct SumImpl
    {
        std::shared_ptr<arrow::ChunkedArray> operator()(
            const std::shared_ptr<arrow::ChunkedArray> &arr)
        {
            auto val = computeSum<T>(arr);
            typename TypeTraits<T>::BuilderType builder;
            (void)builder.Append(val);
            std::shared_ptr<arrow::Array> result;
            (void)builder.Finish(&result);
            return toChunked(result);
        }
    };

    template <DataType T>
    struct MeanImpl
    {
        std::shared_ptr<arrow::ChunkedArray> operator()(
            const std::shared_ptr<arrow::ChunkedArray> &arr)
        {
            auto val = computeMean<T>(arr);
            typename TypeTraits<T>::BuilderType builder;
            (void)builder.Append(val);
            std::shared_ptr<arrow::Array> result;
            (void)builder.Finish(&result);
            return toChunked(result);
        }
    };

    template <DataType T>
    struct MinImpl
    {
        std::shared_ptr<arrow::ChunkedArray> operator()(
            const std::shared_ptr<arrow::ChunkedArray> &arr)
        {
            auto val = computeMin<T>(arr);
            typename TypeTraits<T>::BuilderType builder;
            (void)builder.Append(val);
            std::shared_ptr<arrow::Array> result;
            (void)builder.Finish(&result);
            return toChunked(result);
        }
    };

    template <DataType T>
    struct MaxImpl
    {
        std::shared_ptr<arrow::ChunkedArray> operator()(
            const std::shared_ptr<arrow::ChunkedArray> &arr)
        {
            auto val = computeMax<T>(arr);
            typename TypeTraits<T>::BuilderType builder;
            (void)builder.Append(val);
            std::shared_ptr<arrow::Array> result;
            (void)builder.Finish(&result);
            return toChunked(result);
        }
    };

    std::shared_ptr<arrow::ChunkedArray> applyBooleanMask(
        const std::shared_ptr<arrow::Array> &arr,
        const std::shared_ptr<arrow::BooleanArray> &mask,
        DataType type);

    // Extracts a comparable value from a row as a variant
    using RowValue = std::variant<int32_t, int64_t, float, double,
                                  std::string, bool>;

    RowValue extractRowValue(
        const std::shared_ptr<arrow::Array> &arr,
        int64_t row,
        DataType type);

    // Compare two RowValues — returns negative, 0, or positive
    int compareRowValues(const RowValue &a, const RowValue &b);

    // ArrayOps.h
    std::shared_ptr<arrow::ChunkedArray> reorderByIndices(
        const std::shared_ptr<arrow::Array> &arr,
        const std::vector<int64_t> &indices,
        DataType type);
}
#endif