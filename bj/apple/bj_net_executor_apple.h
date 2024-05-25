//
//  bj_net_executor_apple.h
//
//  Created by Gabriele Mondada on 10.04.2024.
//  Copyright © 2024 Gabriele Mondada.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the “Software”),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//

#pragma once
#include "bj_net.h"
#include <dispatch/dispatch.h>

class Bj_net_single_apple;
class Bj_net_group_apple;

class Bj_net_executor_apple : public Bj_net_executor {
    friend Bj_net_single_apple;
    friend Bj_net_group_apple;
    
public:
    Bj_net_executor_apple() {
        queue = dispatch_queue_create("net-executor", nullptr);
    }

    Bj_net_executor_apple(const Bj_net_executor_apple& executor) {
        queue = executor.queue;
        dispatch_retain(queue);
    }

    Bj_net_executor_apple& operator= (const Bj_net_executor_apple& executor) {
        if (queue != executor.queue) {
            dispatch_release(queue);
            queue = executor.queue;
            dispatch_retain(queue);
        }
        return *this;
    }

    ~Bj_net_executor_apple() {
        dispatch_release(queue);
        queue = nullptr;
    }

    bool operator== (const Bj_net_executor_apple& executor) {
        return queue == executor.queue;
    }

    void invoke_async(std::function<void()> handler) const {
        auto ctx = new std::function<void()>(handler);
        dispatch_async_f(queue, ctx, [](void *ctx) {
            std::function<void()>* f = static_cast<std::function<void()>*>(ctx);
            (*f)();
            delete f;
        });
    }

private:
    dispatch_queue_t queue;
};
