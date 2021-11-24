//
//  Created by Ivan Mejia on 12/24/16.
//
// MIT License
//
// Copyright (c) 2016 ivmeroLabs.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once 

#include <bulldog/engine.h>
#include "basic_controller.hpp"

using namespace cfx;

struct EnginePack {
  Engine* engine_;
  std::chrono::steady_clock::time_point last_access_timestamp_;
};

class BulldogController : public BasicController, Controller {
public:

    BulldogController() : BasicController() {
      bucket_pool_ = new BucketPool();
      blueprint_pool_ = new StrategyPool();
    }
    ~BulldogController() {
      delete bucket_pool_;
      delete blueprint_pool_;
    }
    void handleGet(http_request request) override;
    void handlePut(http_request request) override;
    void handlePost(http_request request) override;
    void handlePatch(http_request request) override;
    void handleDelete(http_request request) override;
    void handleHead(http_request request) override;
    void handleOptions(http_request request) override;
    void handleTrace(http_request request) override;
    void handleConnect(http_request request) override;
    void handleMerge(http_request request) override;
    void initRestOpHandlers() override;

    bool LoadDefault(std::string engine_file_name, std::string game_conf);
    bool Cleanup();
 private:
    static json::value responseNotImpl(const http::method & method);

    std::string default_engine_conf_;
    Game default_normalized_game_;
    StrategyPool* blueprint_pool_; //todo: not smart, currently hardcoded
    BucketPool* bucket_pool_;
    std::unordered_map<std::string, EnginePack> sessions_;
    unsigned long max_capacity_ = 3;
    std::mutex mutex_;
};