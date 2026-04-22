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

#endif