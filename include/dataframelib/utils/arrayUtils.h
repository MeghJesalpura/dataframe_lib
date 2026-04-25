#ifndef ArrayUtils_h
#define ArrayUtils_h

#include "../Types.h"
#include <arrow/api.h>
namespace dataframelib
{
    // Flattens ChunkedArray to a single Array for linear access
    std::shared_ptr<arrow::Array> flatten(
        const std::shared_ptr<arrow::ChunkedArray> &chunked);

    // Wraps single Array back into ChunkedArray
    std::shared_ptr<arrow::ChunkedArray> toChunked(
        const std::shared_ptr<arrow::Array> &arr);

    // Cast between numeric types
    std::shared_ptr<arrow::ChunkedArray> castArray(
        const std::shared_ptr<arrow::ChunkedArray> &arr,
        DataType from,
        DataType to);

    // Check if type is numeric
    bool isNumeric(DataType t);
    bool isInteger(DataType t);
    bool isFloat(DataType t);
}
#endif