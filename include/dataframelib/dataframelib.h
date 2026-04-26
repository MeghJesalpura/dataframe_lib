#ifndef DATAFRAMELIB_H
#define DATAFRAMELIB_H

#include "EagerDataFrame.h"
#include "groupByObj.h"
#include "LazyDataFrame.h"
#include "expression/colExpr.h"
#include "expression/litExpr.h"
#include "expression/exprPtr.h"
#include "operations/binOpExpr.h"
#include "operations/relOpExpr.h"
#include "operations/boolOpExpr.h"
#include "operations/unaryOpExpr.h"
#include "operations/strBinOpExpr.h"
#include "operations/strUnOpExpr.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <stdexcept>

#ifndef ARROW_THROW_NOT_OK
#define ARROW_THROW_NOT_OK(expr)                                 \
    do                                                           \
    {                                                            \
        const auto _arrow_status_ = (expr);                      \
        if (!_arrow_status_.ok())                                \
        {                                                        \
            throw std::runtime_error(_arrow_status_.ToString()); \
        }                                                        \
    } while (0)
#endif

namespace dataframelib
{

    inline EagerDataFrame read_csv(const std::string &path)
    {
        return EagerDataFrame::read_csv(path);
    }

    inline EagerDataFrame read_parquet(const std::string &path)
    {
        return EagerDataFrame::read_parquet(path);
    }

    inline EagerDataFrame from_columns(
        const std::map<std::string, std::shared_ptr<arrow::ChunkedArray>> &cols)
    {
        return EagerDataFrame::from_columns(cols);
    }

    inline EagerDataFrame from_columns(
        const std::vector<std::pair<std::string, std::shared_ptr<arrow::Array>>> &cols)
    {
        return EagerDataFrame::from_columns(cols);
    }

} // namespace dataframelib
#endif
