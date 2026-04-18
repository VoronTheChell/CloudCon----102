#pragma once

#include "Operation.h"

class TaskManager {
public:
    TaskManager() = default;

    OperationResult execute(const OperationRequest& request);
};