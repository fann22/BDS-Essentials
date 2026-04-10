// from https://github.com/GroupMountain/FreeCamera
#pragma once

#include "mc/world/actor/player/Player.h"
#include "mc/network/packet/PlayerSkinPacket.h"

#include <unordered_set>

namespace bds_essentials::freeCamera {
class FreeCameraManager {
public:
    std::unordered_set<unsigned long long> FreeCamList;
    std::optional<PlayerSkinPacket> cachedSkinPacket;
    static FreeCameraManager&              getInstance() {
        static FreeCameraManager instance;
        return instance;
    }
    static void DisableFreeCamera(Player* pl);
    static void EnableFreeCamera(Player* pl);
    static void freecameraHook(bool enable);
};
} // namespace bds_essentials::freeCamera