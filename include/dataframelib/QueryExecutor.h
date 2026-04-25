#ifndef QUERY_EXECUTOR_H
#define QUERY_EXECUTOR_H

#include "Types.h"
#include "EagerDataFrame.h"
#include "lazy/nodes.h"

namespace dataframelib
{
  class QueryExecutor
  {
  public:
    static EagerDataFrame execute(const planNode &node);
  };
}
#endif