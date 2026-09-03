#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <umbrellas/common.hpp>

#include "DeliverySystem.h"
#include "FullScene.h"
#include "MetaSystem.h"

class ShipCameraController;
class DeliverySystem;
class RiftTerrain;
class BeMaterial;
class BeImGuiPass;
struct ImFont;

class RiftScene : public FullScene {
    hide
    std::unique_ptr<RiftTerrain> _terrain;
    std::unique_ptr<ShipCameraController> _shipCameraController;
    std::unique_ptr<DeliverySystem> _delivery;
    std::array<entt::entity, 9> _terrainTiles;
    std::shared_ptr<BeMaterial> _posterizeMaterial;
    std::shared_ptr<BeMaterial> _hudMaterial;
    ImFont* _riftFont = nullptr;
    bool _stationUiOpen = false;
    bool _dying = false;
    bool _showDebug = false;
    MetaSystem _meta;

    // combat + shop
    std::mt19937 _combatRng{1337};
    int _kills = 0;
    float _fireCooldown = 0.0f;
    bool _shopOpen = false;
    int _speedLevel = 0;
    int _gunLevel = 0;
    float _boostLeft = 0.0f;
    float _toastLeft = 0.0f;
    int _ammo = 18;
    int _ammoMax = 18;

    expose
    explicit RiftScene(Game* game);
    ~RiftScene() override;

    auto Prepare() -> void override;
    auto Tick(float deltaTime) -> void override;

    hide
    auto EnterPlayMode() -> void;
    auto ExitPlayMode() -> void;
    auto SetStationUiOpen(bool open) -> void;
    auto DeathSequence() -> BeCoroutine;
    auto StartDeath() -> void;

    auto RandomRange(float lo, float hi) -> float;
    auto SpawnAliens() -> void;
    auto SpawnAlienAround(glm::vec3 origin) -> void;
    auto SpawnBurst(glm::vec3 origin, glm::vec3 color, int count, float power) -> void;
    auto ClearCombatEntities() -> void;
    auto TickCombat(float deltaTime, bool canFire) -> void;
    auto TickShop(float deltaTime) -> void;
    auto ApplyLoadout() -> void;
    auto TryBuy(int& level, int baseCost) -> bool;

    protect
    auto DefineAssets() -> void override;
    auto DefineSettings() -> void override;
    auto DefineScene() -> void override;
    auto DefinePasses() -> void override;
};
