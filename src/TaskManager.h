#pragma once

#include "CloudProvider.h"
#include "Operation.h"

class TaskManager {
public:
    explicit TaskManager(CloudProvider* provider);

    OperationResult execute(const OperationRequest& request);

private:
    CloudProvider* provider_ {};
};