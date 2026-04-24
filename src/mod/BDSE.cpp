#include "mod/BDSE.h"

#include "freeCamera/FreeCamera.h"
#include "features/FastLeafDecay.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/entity/ActorHurtEvent.h"
#include "ll/api/event/player/PlayerChatEvent.h"
#include "ll/api/event/player/PlayerDieEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/thread/ThreadPoolExecutor.h"

#include "ila/event/minecraft/server/ServerPongEvent.h"
#include "ila/event/minecraft/world/actor/MobHealthChangeEvent.h"
#include "ila/event/minecraft/world/level/block/FarmDecayEvent.h"

#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstanceRef.h"
#include "mc/world/attribute/BaseAttributeMap.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/LevelData.h"

#include "mc/world/scores/Objective.h"
#include "mc/world/scores/ObjectiveCriteria.h"
#include "mc/world/scores/ObjectiveRenderType.h"
#include "mc/world/scores/ObjectiveSortOrder.h"
#include "mc/world/scores/PlayerScoreSetFunction.h"
#include "mc/world/scores/Scoreboard.h"
#include "mc/world/scores/ScoreboardId.h"
#include "mc/world/scores/ScoreboardOperationResult.h"

#include "mc/network/packet/PlaySoundPacket.h"
#include "mc/network/packet/PlaySoundPacketPayload.h"
#include "mc/network/packet/TextPacket.h"

#include "mc/deps/shared_types/legacy/actor/ActorDamageCause.h"
#include "mc/network/packet/DebugDrawerPacket.h"
#include "mc/network/packet/LineDataPayload.h"
#include "mc/network/packet/ShapeDataPayload.h"
#include "mc/network/packet/cerealize/core/SerializationMode.h"
#include "mc/scripting/modules/minecraft/debugdrawer/ScriptDebugShapeType.h"
#include "mc/world/level/dimension/Dimension.h"

ShapeDataPayload::ShapeDataPayload() { mNetworkId = 0; }

namespace bds_essentials {

// ============================================================
//  Chunk border state
// ============================================================

static std::atomic<uint64_t>                                         sNextShapeId{UINT64_MAX};
static std::unordered_map<uint64_t, std::pair<int, int>>             gLastChunk;
static std::unordered_map<uint64_t, std::vector<uint64_t>>           gShapeIds;
static std::unordered_set<uint64_t>                                  ChunkBorderList;

// Remove all debug-drawer lines for a player and clear tracking state.
void removeChunkBorder(Player& player) {
    auto guid = player.getNetworkIdentifier().mGuid.g;
    auto it   = gShapeIds.find(guid);
    if (it == gShapeIds.end()) return;

    DebugDrawerPacket pkt;
    pkt.setSerializationMode(SerializationMode::CerealOnly);

    for (auto& id : it->second) {
        ShapeDataPayload shape;
        shape.mNetworkId = id;
        shape.mShapeType = std::nullopt; // nullopt = delete shape
        pkt.mShapes->emplace_back(std::move(shape));
    }

    pkt.sendTo(player);
    gShapeIds.erase(guid);
}

// Redraw chunk border lines if the player has moved to a new chunk.
void updateChunkBorder(Player& player) {
    Vec3 pos    = player.getPosition();
    int  chunkX = (int)std::floor(pos.x / 16);
    int  chunkZ = (int)std::floor(pos.z / 16);
    auto guid   = player.getNetworkIdentifier().mGuid.g;

    // Skip redraw when still in the same chunk.
    auto it = gLastChunk.find(guid);
    if (it != gLastChunk.end() && it->second == std::make_pair(chunkX, chunkZ)) return;

    removeChunkBorder(player);
    gLastChunk[guid] = {chunkX, chunkZ};

    float minX  = chunkX * 16.f,      maxX = minX + 16.f;
    float minZ  = chunkZ * 16.f,      maxZ = minZ + 16.f;
    float minY  = -64.f,              maxY = 320.f;
    auto  dimId = player.getDimension().mId;

    DebugDrawerPacket pkt;
    pkt.setSerializationMode(SerializationMode::CerealOnly);

    auto addLine = [&](Vec3 const& from, Vec3 const& to) {
        auto id = sNextShapeId.fetch_sub(1);
        ShapeDataPayload shape;
        shape.mNetworkId        = id;
        shape.mShapeType        = ScriptModuleDebugUtilities::ScriptDebugShapeType::Line;
        shape.mLocation         = from;
        shape.mColor            = mce::Color::RED();
        shape.mDimensionId      = dimId;
        shape.mExtraDataPayload = LineDataPayload{.mEndLocation = to};
        pkt.mShapes->emplace_back(std::move(shape));
        gShapeIds[guid].push_back(id);
    };

    // Vertical lines along North/South walls (sweep X)
    for (float x = minX; x <= maxX; x += 2.f) {
        addLine({x, minY, minZ}, {x, maxY, minZ});
        addLine({x, minY, maxZ}, {x, maxY, maxZ});
    }

    // Vertical lines along West/East walls (sweep Z)
    for (float z = minZ; z <= maxZ; z += 2.f) {
        addLine({minX, minY, z}, {minX, maxY, z});
        addLine({maxX, minY, z}, {maxX, maxY, z});
    }

    // Horizontal rings every 2 blocks
    for (float y = minY; y <= maxY; y += 2.f) {
        addLine({minX, y, minZ}, {maxX, y, minZ}); // North
        addLine({minX, y, maxZ}, {maxX, y, maxZ}); // South
        addLine({minX, y, minZ}, {minX, y, maxZ}); // West
        addLine({maxX, y, minZ}, {maxX, y, maxZ}); // East
    }

    pkt.sendTo(player);
}

// ============================================================
//  Hooks
// ============================================================

// Sync XP level to the sidebar scoreboard whenever a player gains levels.
LL_TYPE_INSTANCE_HOOK(
    PlayerAddLevelHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::$addLevels,
    void,
    int lvl
) {
    BDSE&       bdse        = BDSE::getInstance();
    Scoreboard* scoreboard  = bdse.getScoreboard();
    Objective*  xpObjective = bdse.getXPObjective();

    BaseAttributeMap&    attrMap = const_cast<BaseAttributeMap&>(*this->getAttributes());
    AttributeInstanceRef ref     = attrMap.getMutableInstance(Player::LEVEL().mIDValue);

    // Clamp to 0 — level can theoretically go negative on death edge cases.
    int newLevel = std::max(0, static_cast<int>(ref.mPtr->mCurrentValue) + lvl);

    if (scoreboard && xpObjective) {
        ScoreboardId const& id = scoreboard->getScoreboardId(*this);
        if (id != ScoreboardId::INVALID()) {
            ScoreboardOperationResult result;
            scoreboard->modifyPlayerScore(result, id, *xpObjective, newLevel, PlayerScoreSetFunction::Set);
        }
    }

    origin(lvl);
}

// Prevent achievements from being flagged as disabled on world load.
LL_TYPE_INSTANCE_HOOK(
    AchievementsWillBeDisabledHook,
    ll::memory::HookPriority::Normal,
    LevelData,
    &LevelData::achievementsWillBeDisabledOnLoad,
    bool
) {
    return false;
}

// Swallow the disable call entirely — achievements stay enabled.
LL_TYPE_INSTANCE_HOOK(
    DisableAchievementsHook,
    ll::memory::HookPriority::Normal,
    LevelData,
    &LevelData::disableAchievements,
    void
) {}

// ============================================================
//  Helpers
// ============================================================

// Returns (or creates) the ScoreboardId for a player.
const ScoreboardId* getOrCreateScoreboardId(Player& player) {
    Scoreboard* scoreboard = BDSE::getInstance().getScoreboard();
    if (!scoreboard) return nullptr;

    ScoreboardId const* id = &scoreboard->getScoreboardId(player);
    if (*id == ScoreboardId::INVALID())
        id = &scoreboard->createScoreboardId(player);

    return id;
}

// ============================================================
//  BDSE lifecycle
// ============================================================

BDSE& BDSE::getInstance() {
    static BDSE instance;
    return instance;
}

static std::vector<ll::event::ListenerPtr> gListeners;
static std::vector<std::string>            gMotdMessages = {
    "•> 𝗩𝗮𝗻𝗶𝗹𝗹𝗮 𝗦𝗲𝗿𝘃𝗲𝗿 <•",
    "•> play.anomaly.bond <•",
    "•> PORT: 25600 <•"
};
static std::atomic<int>  gMotdIndex{0};
static std::atomic<bool> gRunning{false};

bool BDSE::load() { return true; }

bool BDSE::enable() {
    gRunning = true;

    // Background: rotate MOTD every 1.5 s.
    ll::thread::ThreadPoolExecutor::getDefault().execute([]() {
        while (gRunning) {
            gMotdIndex = (gMotdIndex + 1) % (int)gMotdMessages.size();
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
    });

    // Background: update chunk borders for opted-in players every 150 ms.
    ll::thread::ThreadPoolExecutor::getDefault().execute([]() {
        while (gRunning) {
            ll::thread::ServerThreadExecutor::getDefault().execute([]() {
                ll::service::getLevel()->forEachPlayer([](Player& player) -> bool {
                    try {
                        if (ChunkBorderList.count(player.getNetworkIdentifier().mGuid.g))
                            updateChunkBorder(player);
                    } catch (std::exception& e) {
                        BDSE::getInstance().getSelf().getLogger().error("chunkBorder: {}", e.what());
                    }
                    return true;
                });
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    });

    // ── Commands ─────────────────────────────────────────────

    freeCamera::FreeCameraManager::freecameraHook(true);
    auto& fcCmd = ll::command::CommandRegistrar::getInstance(false)
                      .getOrCreateCommand("freecamera", "Toggle free camera.");
    ll::service::getCommandRegistry()->registerAlias("freecamera", "fc");
    fcCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) {
            output.error("Only players can use this command.");
            return;
        }
        auto* player = ll::service::getLevel()->getPlayer(entity->getOrCreateUniqueID());
        if (!player) { output.error("Player not found."); return; }

        auto  guid = player->getNetworkIdentifier().mGuid.g;
        auto& mgr  = freeCamera::FreeCameraManager::getInstance();
        if (!mgr.FreeCamList.count(guid)) {
            freeCamera::FreeCameraManager::EnableFreeCamera(player);
            output.success("Free camera enabled.");
        } else {
            freeCamera::FreeCameraManager::DisableFreeCamera(player);
            output.success("Free camera disabled.");
        }
    });

    auto& cbCmd = ll::command::CommandRegistrar::getInstance(false)
                      .getOrCreateCommand("chunkborder", "Toggle chunk border.");
    ll::service::getCommandRegistry()->registerAlias("chunkborder", "cb");
    cbCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) {
            output.error("Only players can use this command.");
            return;
        }
        auto* player = ll::service::getLevel()->getPlayer(entity->getOrCreateUniqueID());
        if (!player) { output.error("Player not found."); return; }

        auto guid = player->getNetworkIdentifier().mGuid.g;
        if (!ChunkBorderList.count(guid)) {
            ChunkBorderList.insert(guid);
            updateChunkBorder(*player);
            output.success("Chunk border enabled.");
        } else {
            ChunkBorderList.erase(guid);
            removeChunkBorder(*player);
            gLastChunk.erase(guid);
            gShapeIds.erase(guid);
            output.success("Chunk border disabled.");
        }
    });

    // ── Scoreboard setup ─────────────────────────────────────

    ll::service::getLevel()->getLevelData().mAchievementsDisabled = false;
    mScoreboard = &ll::service::getLevel()->getScoreboard();

    // Clear any pre-existing objectives to start fresh.
    for (auto* obj : mScoreboard->getObjectives())
        mScoreboard->removeObjective(const_cast<Objective*>(obj));

    ObjectiveCriteria* criteria = mScoreboard->getCriteria(Scoreboard::DEFAULT_CRITERIA());
    if (!criteria)
        criteria = const_cast<ObjectiveCriteria*>(
            &mScoreboard->createObjectiveCriteria(
                Scoreboard::DEFAULT_CRITERIA(), false, ObjectiveRenderType::Integer));

    mHealthObjective = mScoreboard->addObjective("PlayerHealth", "❤", *criteria);
    mScoreboard->setDisplayObjective(
        Scoreboard::DISPLAY_SLOT_BELOWNAME(), *mHealthObjective, ObjectiveSortOrder::Descending);

    mXPObjective = mScoreboard->addObjective("MostLVL", "•> Most Level <•", *criteria);
    mScoreboard->setDisplayObjective(
        Scoreboard::DISPLAY_SLOT_SIDEBAR(), *mXPObjective, ObjectiveSortOrder::Descending);

    // ── Feature hooks ─────────────────────────────────────────

    features::fast_leaf_decay::enable();
    AchievementsWillBeDisabledHook::hook();
    DisableAchievementsHook::hook();
    PlayerAddLevelHook::hook();

    // ── Event listeners ───────────────────────────────────────

    auto& bus = ll::event::EventBus::getInstance();
    auto  reg = [&]<typename E>(auto fn) {
        gListeners.push_back(bus.emplaceListener<E>(std::move(fn)));
    };

    // Prevent farmland from decaying.
    reg.operator()<ila::mc::FarmDecayBeforeEvent>(
        [](ila::mc::FarmDecayBeforeEvent& e) { e.cancel(); });

    // Rotate MOTD text.
    reg.operator()<ila::mc::ServerPongBeforeEvent>(
        [](ila::mc::ServerPongBeforeEvent& e) { e.motd() = gMotdMessages[gMotdIndex]; });

    // On join: populate health and XP scores.
    reg.operator()<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& e) {
        Player&              player  = e.self();
        BaseAttributeMap&    attrMap = const_cast<BaseAttributeMap&>(*player.getAttributes());
        AttributeInstanceRef ref     = attrMap.getMutableInstance(Player::LEVEL().mIDValue);
        int                  lvl     = static_cast<int>(ref.mPtr->mCurrentValue);

        const ScoreboardId* id = getOrCreateScoreboardId(player);
        if (!id) return;

        ScoreboardOperationResult result;
        mScoreboard->modifyPlayerScore(result, *id, *mHealthObjective, player.getHealth(),  PlayerScoreSetFunction::Set);
        mScoreboard->modifyPlayerScore(result, *id, *mXPObjective,     lvl,                PlayerScoreSetFunction::Set);
    });

    // On disconnect: remove scores and clean up chunk border state.
    reg.operator()<ll::event::PlayerDisconnectEvent>([this](ll::event::PlayerDisconnectEvent& e) {
        const ScoreboardId* id = getOrCreateScoreboardId(e.self());
        if (id) mScoreboard->resetPlayerScore(*id);

        auto guid = e.self().getNetworkIdentifier().mGuid.g;
        removeChunkBorder(e.self());
        gLastChunk.erase(guid);
        gShapeIds.erase(guid);
    });

    // On death: reset XP score to 0.
    reg.operator()<ll::event::PlayerDieEvent>([this](ll::event::PlayerDieEvent& e) {
        const ScoreboardId* id = getOrCreateScoreboardId(e.self());
        if (!id) return;

        ScoreboardOperationResult result;
        mScoreboard->modifyPlayerScore(result, *id, *mXPObjective, 0, PlayerScoreSetFunction::Set);
    });

    // Keep health score in sync whenever HP changes.
    reg.operator()<ila::mc::MobHealthChangeAfterEvent>([this](ila::mc::MobHealthChangeAfterEvent& e) {
        if (!e.self().isPlayer() || e.newValue() > float(e.self().getMaxHealth())) return;

        const ScoreboardId* id = getOrCreateScoreboardId(static_cast<Player&>(e.self()));
        if (!id) return;

        ScoreboardOperationResult result;
        mScoreboard->modifyPlayerScore(
            result, *id, *mHealthObjective,
            static_cast<int>(std::ceil(e.newValue())),
            PlayerScoreSetFunction::Set);
    });

    // Play a sound for the shooter when their projectile hits something.
    reg.operator()<ll::event::ActorHurtEvent>([](ll::event::ActorHurtEvent& e) {
        if (e.source().mCause != SharedTypes::Legacy::ActorDamageCause::Projectile) return;

        Player* player = ll::service::getLevel()->getPlayer(e.source().getEntityUniqueID());
        if (!player) return;

        PlaySoundPacket packet(PlaySoundPacketPayload("random.orb", player->getPosition(), 2.f, 1.2f));
        player->sendNetworkPacket(packet);
    });

    // Intercept chat: log, reformat with colour, and broadcast.
    reg.operator()<ll::event::PlayerChatEvent>([](ll::event::PlayerChatEvent& e) {
        BDSE::getInstance().getSelf().getLogger().info("{}: {}", e.self().getRealName(), e.message());
        TextPacket::createRawMessage("§b" + e.self().getRealName() + "§f: " + e.message()).sendToClients();
        e.cancel();
    });

    return true;
}

bool BDSE::disable() {
    gRunning = false;

    features::fast_leaf_decay::disable();
    freeCamera::FreeCameraManager::freecameraHook(false);
    AchievementsWillBeDisabledHook::unhook();
    DisableAchievementsHook::unhook();
    PlayerAddLevelHook::unhook();

    auto& bus = ll::event::EventBus::getInstance();
    for (auto& listener : gListeners)
        bus.removeListener(listener);
    gListeners.clear();

    mScoreboard      = nullptr;
    mHealthObjective = nullptr;
    mXPObjective     = nullptr;

    return true;
}

} // namespace bds_essentials

LL_REGISTER_MOD(bds_essentials::BDSE, bds_essentials::BDSE::getInstance());