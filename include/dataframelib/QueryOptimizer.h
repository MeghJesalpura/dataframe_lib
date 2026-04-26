#ifndef QUERY_OPTIMIZER_H
#define QUERY_OPTIMIZER_H

#include "Types.h"
#include "lazy/nodes.h"
namespace dataframelib
{
  class QueryOptimizer
  {
    // This is the optimizer, it will have a single static method optimize that takes in an expression tree and returns an optimized expression tree
  public:
    static planNode optimize(planNode input);
  };
}

#endif