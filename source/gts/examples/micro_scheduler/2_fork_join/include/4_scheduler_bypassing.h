/*******************************************************************************
 * Copyright 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/
#pragma once

#include <cassert>
#include <iostream>

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>

using namespace gts;

namespace gts_examples {

// Builds on 3_continuation_join

//------------------------------------------------------------------------------
// A task that explicitly represent a join.
struct ParallelFibContinuationTask3
{
    ParallelFibContinuationTask3(
        uint32_t l,
        uint32_t r,
        uint64_t* sum)
        : l(l)
        , r(r)
        , sum(sum) {}

    uint64_t l;
    uint64_t r;
    uint64_t* sum;

    static Task* taskFunc(gts::Task* thisTask, gts::TaskContext const&)
    {
        ParallelFibContinuationTask3& data = *(ParallelFibContinuationTask3*)thisTask->getData();
        *(data.sum) = data.l + data.r;
        return nullptr;
    }
};

//------------------------------------------------------------------------------
struct ParallelFibTask3
{
    uint32_t fibN;
    uint64_t* sum;

    ParallelFibTask3(
        uint32_t fibN,
        uint64_t* sum)
        : fibN(fibN)
        , sum(sum) {}

    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        // Unpack the data.
        ParallelFibTask3& data = *(ParallelFibTask3*)pThisTask->getData();

        uint32_t fibN = data.fibN;
        uint64_t* sum = data.sum;

        if (data.fibN <= 2)
        {
            *sum = 1;
            return nullptr;
        }
        else
        {
            // Create the continuation task with the join function.
            Task* pContinuationTask = ctx.pMicroScheduler->allocateTaskRaw(ParallelFibContinuationTask3::taskFunc, sizeof(ParallelFibContinuationTask3));
            ParallelFibContinuationTask3* pContinuationData = pContinuationTask->emplaceData<ParallelFibContinuationTask3>(0, 0, sum);
            pThisTask->setContinuationTask(pContinuationTask);
            pContinuationTask->addRef(2, gts::memory_order::relaxed);
            
            // Fork f(n-1)
            Task* pLeftChild = ctx.pMicroScheduler->allocateTask<ParallelFibTask3>(fibN - 1, &pContinuationData->l);
            pContinuationTask->addChildTaskWithoutRef(pLeftChild);
            ctx.pMicroScheduler->spawnTask(pLeftChild);

            // Fork f(n-2)
            Task* pRightChild = ctx.pMicroScheduler->allocateTask<ParallelFibTask3>(fibN - 2, &pContinuationData->r);
            pContinuationTask->addChildTaskWithoutRef(pRightChild);
            // Don't queue the right child! return it.

            // We return right child. The makes the task execute immediately,
            // bypassing the more expensive scheduler operations.
            return pRightChild;
        }
    }
};

//------------------------------------------------------------------------------
void bypassForkJoin(uint32_t fibN)
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    uint64_t fibVal = 0;

    // Create the fib task.
    Task* pTask = taskScheduler.allocateTask<ParallelFibTask3>(fibN, &fibVal);

    // Queue and wait for the task to complete.
    taskScheduler.spawnTaskAndWait(pTask);

    // NOTE: wait ^^^^ does NOT mean that this thread is blocked. This thread will
    // actively execute any tasks in the scheduler until pTask completes.

    std::cout << "Fib " << fibN << " is: " << fibVal << std::endl;

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
