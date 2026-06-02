// from https://github.com/GroupMountain/FreeCamera
#pragma once

#include "mc/world/actor/player/Player.h"
#include "mc/network/packet/PlayerSkinPacket.h"

#include <mutex>
#include <unordered_set>

namespace bds_essentials::freeCamera {

class FreeCameraManager {
public:
    // All accesses to FreeCamList must hold mMtx.
    std::mutex                             mMtx;
    std::unordered_set<unsigned long long> FreeCamList;

    static FreeCameraManager& getInstance() {
        static FreeCameraManager instance;
        return instance;
    }

    // Convenience helpers — lock internally.
    bool contains(unsigned long long guid) {
        std::lock_guard lock(mMtx);
        return FreeCamList.count(guid) > 0;
    }
    void insert(unsigned long long guid) {
        std::lock_guard lock(mMtx);
        FreeCamList.insert(guid);
    }
    void erase(unsigned long long guid) {
        std::lock_guard lock(mMtx);
        FreeCamList.erase(guid);
    }

    static void DisableFreeCamera(Player* pl);
    static void EnableFreeCamera(Player* pl);
    static void freecameraHook(bool enable);
};

} // namespace bds_essentials::freeCamera
