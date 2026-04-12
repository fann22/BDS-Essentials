#pragma once
#include "core/HttpSender.h"
#include "core/ChunkCollector.h"
#include <memory>

namespace mipmap {

class MipMap {
public:
    static MipMap& getInstance() {
        static MipMap instance;
        return instance;
    }

    void init() {
        HttpSender::Config cfg;
        cfg.host = "127.0.0.1";
        cfg.port = 8080; // nanti dari config

        mSender    = std::make_unique<HttpSender>(cfg);
        mCollector = std::make_unique<ChunkCollector>(*mSender);
    }

    ChunkCollector& getCollector() { return *mCollector; }

private:
    std::unique_ptr<HttpSender>    mSender;
    std::unique_ptr<ChunkCollector> mCollector;
};

} // namespace mipmap