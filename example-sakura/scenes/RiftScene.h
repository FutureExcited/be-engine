#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <umbrellas/common.hpp>
#include <umbrellas/include-glm.h>

#include "DeliverySystem.h"
#include "FullScene.h"

class ShipCameraController;
class DeliverySystem;
class BeMaterial;

struct RiftSceneSettings {
    struct {
        float NearPlane = 0.5f;
        float FarPlane = 1100.0f;
        float SpawnHeight = 100.0f;
    } Camera;

    struct {
        //glm::vec3 Color = HexColor("#0A122E");
        //glm::vec3 Color = HexColor("#222222");
        //glm::vec3 Color = HexColor("#FF7F11");
        glm::vec3 Color = HexColor("#000000");
        //glm::vec3 Color = HexColor("#2E4372");
    } Ambient;

    struct { 
        float Size = 180.0f;
        int Cells = 45;
        float SpikeAmplitude = 75.0f;
        glm::vec3 Color = HexColor("#F28123");
    } Terrain;

    struct {
        float PixelSize = 4.0f;
        float DitherSpread = 1.0f;
        float FogStart = 280.0f;
        float FogEnd = 900.0f;
        glm::vec3 FogColor = HexColor("#2E4372");
        bool Enabled = true;
        std::vector<glm::vec3> Palette = {
            HexColor("#2E4372"),
            HexColor("#E89128"),
            HexColor("#FFFFFF"),
            HexColor("#D34E24"),
            HexColor("#8C3318"),
            HexColor("#1F2C47"),
        };
    } Posterize;

    struct {
        glm::vec3 Direction = { -0.5f, -1.0f, -0.5f };
        glm::vec3 Color = glm::vec3(1.0f);
        float Power = 1.0f;
        float ShadowCameraDistance = 50.0f;
        float ShadowMapWorldSize = 30.0f;
        float ShadowNearPlane = 0.1f;
        float ShadowFarPlane = 200.0f;
    } Sun;

    struct {
        DeliverySystem::Config Config = {
            .StationKinds = {
                {
                    .Prop = "station-solar-1",
                    .Path = "assets/rift/simple_solar_station_1.glb",
                    .EmissiveMix = 0.5f,
                    .AimPoint = glm::vec3(0.0f, 3.0f, 0.0f),
                    .Scale = 4.0f,
                    .Flying = false,
                },
                {
                    .Prop = "station-solar-2",
                    .Path = "assets/rift/simple_solar_station_2.glb",
                    .EmissiveMix = 0.5f,
                    .AimPoint = glm::vec3(0.0f, 3.0f, 0.0f),
                    .Scale = 4.0f,
                    .Flying = false,
                },
                {
                    .Prop = "station-solar-3",
                    .Path = "assets/rift/simple_solar_station_3.glb",
                    .EmissiveMix = 0.5f,
                    .AimPoint = glm::vec3(0.0f, 3.0f, 0.0f),
                    .Scale = 4.0f,
                    .Flying = false,
                },
                {
                    .Prop = "flying-station",
                    .Path = "assets/rift/flying_station.glb",
                    .EmissiveMix = 0.5f,
                    .AimPoint = glm::vec3(0.0f, 1.0f, 0.0f),
                    .Scale = 40.0f,
                    .Flying = true,
                },
                {
                    .Prop = "oxygen-station",
                    .Path = "assets/rift/oxygen_station.glb",
                    .EmissiveMix = 0.5f,
                    .AimPoint = glm::vec3(0.0f, 0.0f, 0.0f),
                    .Origin = glm::vec3(0.0f, -0.5f, 0.0f),
                    .Scale = 10.0f,
                    .Flying = false,
                },
                {
                    .Prop = "mining-station",
                    .Path = "assets/rift/mining_station/scene.gltf",
                    .EmissiveMix = 0.5f,
                    .AimPoint = glm::vec3(0.0f, 3.0f, 0.0f),
                    .Scale = 0.01f,
                    .Flying = true,
                },
                {
                    .Prop = "iss",
                    .Path = "assets/rift/iss/scene.gltf",
                    .EmissiveMix = 0.5f,
                    .AimPoint = glm::vec3(0.0f, 0.0f, 0.0f),
                    .Scale = 40.0f,
                    .Flying = true,
                },
                {
                    .Prop = "power_station",
                    .Path = "assets/rift/power_station/scene.gltf",
                    .EmissiveMix = 0.5f,
                    .AimPoint = glm::vec3(0.0f, 3.0f, 0.0f),
                    .Scale = 2.0f,
                    .Flying = true,
                },
            },
            .StationCount = 12,
            .MapRadius = 1500.0f,
            .AltitudeMin = 30.0f,
            .AltitudeMax = 150.0f,
            .MinSeparation = 250.0f,
            .VisitRadius = 35.0f,
        };
        struct {
            float ScreenMargin = 0.88f;
            float MinRadius = 1.5f;
            float MaxRadius = 10.0f;
            float SizeFar = 800.0f;
            float SizeNear = 120.0f;
            float FadeNear = 80.0f;
            float FadeFar = 200.0f;
        } Marker;
    } Delivery;

    struct {
        float HullRadius = 2.0f;
        int AlienCount = 16;
        float AlienHitRadius = 16.0f;
        float AlienMinSpeed = 11.0f;
        float AlienMaxSpeed = 20.0f;
        float AlienSpawnNear = 220.0f;
        float AlienSpawnFar = 1400.0f;
        float AlienMinAltitude = 20.0f;
        float CatchRadius = 10.0f;
        float FireCooldown = 0.11f;
        float TracerSpeed = 260.0f;
        float TracerLife = 0.85f;
        float TracerHitRadius = 2.5f;
        int StartingAmmo = 18;
        int AmmoPerKill = 3;
    } Combat;

    struct {
        float ApproachGlowRadius = 70.0f;
        float BoostMul = 1.20f;
        float BoostSeconds = 10.0f;
        int BasePay = 12;
        float PatrolTipChance = 0.32f;
        float PatrolTipMin = 6.0f;
        float PatrolTipSpeedScale = 0.45f;
        int SpeedCost = 40;
        int GunCost = 50;
        int StyleCost = 30;
    } Gig;
};

class RiftScene : public FullScene {
    hide
    std::unique_ptr<ShipCameraController> _shipCameraController;
    std::unique_ptr<DeliverySystem> _delivery;
    std::array<entt::entity, 9> _terrainTiles;
    std::shared_ptr<BeMaterial> _posterizeMaterial;
    std::shared_ptr<BeMaterial> _hudMaterial;
    std::mt19937 _combatRng{1337};
    bool _dead = false;
    int _kills = 0;
    float _fireCooldown = 0.0f;
    float _gameOverTime = 0.0f;
    bool _shopOpen = false;
    int _credits = 0;
    int _speedLevel = 0;
    int _gunLevel = 0;
    int _styleLevel = 0;
    float _boostLeft = 0.0f;
    float _toastLeft = 0.0f;
    std::string _toast;
    int _ammo = 18;
    int _ammoMax = 18;
    int _lastPay = 0;
    int _lastTip = 0;

    expose
    RiftSceneSettings Settings;

    explicit RiftScene(Game* game);
    ~RiftScene() override;

    auto Prepare() -> void override;
    auto Tick(float deltaTime) -> void override;

    hide
    auto EnterPlayMode() -> void;
    auto ExitPlayMode() -> void;
    auto RandomRange(float lo, float hi) -> float;
    auto SpawnAliens() -> void;
    auto SpawnAlienAround(glm::vec3 origin) -> void;
    auto SpawnBurst(glm::vec3 origin, glm::vec3 color, int count, float power) -> void;
    auto TriggerCrash() -> void;
    auto RestartRun() -> void;
    auto TickCombat(float deltaTime) -> void;
    auto ApplyLoadout() -> void;
    auto TickGigs(float deltaTime) -> void;
    auto SettleDelivery(float speed) -> void;
    auto TryBuy(int& level, int baseCost) -> bool;

    protect
    auto DefineAssets() -> void override;
    auto DefineSettings() -> void override;
    auto DefineScene() -> void override;
    auto DefinePasses() -> void override;
};
