#include "RiftScene.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include <umbrellas/include-glfw.h>
#include <umbrellas/include-glm.h>

#include "BeMesh.h"

#include "ShipCameraController.h"
#include "DeliverySystem.h"
#include "RiftTerrain.h"
#include "BeCamera.h"

#include "BeInput.h"
#include "BeMaterial.h"
#include "BeMeshPrimitives.h"
#include "BeProp.h"
#include "BeRenderer.h"
#include "BeTexture.h"
#include "Components.h"
#include "Game.h"
#include "scenes/BeSceneManager.h"
#include "BeShaderLibrary.h"
#include "BeShader.h"
#include "standard-render-machine/BeStandardRenderMachine.h"

RiftScene::RiftScene(Game* game) : FullScene(game) {}
RiftScene::~RiftScene() = default;

void RiftScene::Prepare() {
    FullScene::Prepare();

    _camera->Position = glm::vec3(0.0f, Settings.Camera.SpawnHeight, 0.0f);

    _shipCameraController = std::make_unique<ShipCameraController>(_camera.get());
    _hudMaterial->SetFloat1("AimRadius", _shipCameraController->AimRadius);
    _ammo = Settings.Combat.StartingAmmo;
    _ammoMax = Settings.Combat.StartingAmmo;
    ApplyLoadout();
    SpawnAliens();
    EnterPlayMode();
}

auto RiftScene::EnterPlayMode() -> void {
    auto deliveryConfig = Settings.Delivery.Config;
    deliveryConfig.TerrainSize = Settings.Terrain.Size;
    deliveryConfig.TerrainSpikeAmplitude = Settings.Terrain.SpikeAmplitude;
    deliveryConfig.Seed = std::random_device{}();
    _delivery = std::make_unique<DeliverySystem>(_registry, _assetRegistry, deliveryConfig);
    _delivery->GenerateStations();
    _delivery->Begin(_camera->Position);
}

auto RiftScene::ExitPlayMode() -> void {
    const auto stations = _registry.view<StationComponent>();
    _registry.destroy(stations.begin(), stations.end());
    _delivery.reset();
    _hudMaterial->SetFloat1("TargetState", 0.0f);
}

auto RiftScene::DefineSettings() -> void {
    _camera->NearPlane = Settings.Camera.NearPlane;
    _camera->FarPlane = Settings.Camera.FarPlane;
    _machine->UniformMaterial->SetFloat3("AmbientColor", Settings.Ambient.Color);
}

auto RiftScene::DefineAssets() -> void {
    auto phongShader = BeShaderLibrary::GetShader("standard-phong");

    auto box = BeProp::FromMesh(BeMeshPrimitives::Cube(), phongShader, "geometry-main");
    box->Materials[0]->SetFloat3("DiffuseColor", glm::vec3(1.0f));
    _assetRegistry.AddProp("box", box);
    _machine->RegisterMesh(box->Mesh);

    auto hunterMesh = std::make_shared<BeMesh>();
    auto pushBox = [&](glm::vec3 c, glm::vec3 e) {
        const glm::vec3 p[8] = {
            c + glm::vec3(-e.x, -e.y, -e.z), c + glm::vec3( e.x, -e.y, -e.z),
            c + glm::vec3( e.x,  e.y, -e.z), c + glm::vec3(-e.x,  e.y, -e.z),
            c + glm::vec3(-e.x, -e.y,  e.z), c + glm::vec3( e.x, -e.y,  e.z),
            c + glm::vec3( e.x,  e.y,  e.z), c + glm::vec3(-e.x,  e.y,  e.z),
        };
        const int faces[6][4] = {
            {0,1,2,3}, {5,4,7,6}, {4,0,3,7}, {1,5,6,2}, {3,2,6,7}, {4,5,1,0}
        };
        for (const auto& f : faces) {
            const glm::vec3 a = p[f[0]], b = p[f[1]], c0 = p[f[2]], d0 = p[f[3]];
            const glm::vec3 n = glm::normalize(glm::cross(b - a, c0 - a));
            const uint32_t base = static_cast<uint32_t>(hunterMesh->Vertices.size());
            for (const auto& q : { a, b, c0, d0 }) {
                BeFullVertex v{};
                v.Position = q;
                v.Normal = n;
                hunterMesh->Vertices.push_back(v);
            }
            hunterMesh->Indices.insert(hunterMesh->Indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
        }
    };
    pushBox({0.0f, 0.0f, 0.2f}, {0.45f, 0.22f, 1.6f});
    pushBox({0.0f, 0.0f, 1.7f}, {0.18f, 0.18f, 0.35f});
    pushBox({ 1.4f, 0.0f, 0.1f}, {1.1f, 0.06f, 0.28f});
    pushBox({-1.4f, 0.0f, 0.1f}, {1.1f, 0.06f, 0.28f});
    pushBox({0.0f, 0.0f, -1.5f}, {0.12f, 0.35f, 0.45f});
    hunterMesh->Slices.push_back({
        .IndexCount = static_cast<uint32_t>(hunterMesh->Indices.size()),
        .StartIndexLocation = 0,
        .BaseVertexLocation = 0,
    });
    auto alien = BeProp::FromMesh(hunterMesh, phongShader, "geometry-main");
    alien->Slices[0].TwoSided = true;
    alien->Materials[0]->SetFloat3("DiffuseColor", glm::vec3(0.15f, 0.95f, 0.22f));
    alien->Materials[0]->SetFloat3("EmissiveColor", glm::vec3(0.4f, 4.5f, 0.5f));
    _assetRegistry.AddProp("alien", alien);
    _machine->RegisterMesh(alien->Mesh);

    auto tracer = BeProp::FromMesh(BeMeshPrimitives::Cube(), phongShader, "geometry-main");
    tracer->Materials[0]->SetFloat3("DiffuseColor", glm::vec3(1.0f));
    tracer->Materials[0]->SetFloat3("EmissiveColor", glm::vec3(8.0f, 7.0f, 2.2f));
    _assetRegistry.AddProp("tracer", tracer);
    _machine->RegisterMesh(tracer->Mesh);

    auto debris = BeProp::FromMesh(BeMeshPrimitives::Cube(), phongShader, "geometry-main");
    debris->Materials[0]->SetFloat3("DiffuseColor", glm::vec3(1.0f, 0.45f, 0.08f));
    debris->Materials[0]->SetFloat3("EmissiveColor", glm::vec3(6.0f, 2.1f, 0.25f));
    _assetRegistry.AddProp("debris", debris);
    _machine->RegisterMesh(debris->Mesh);

    auto flash = BeProp::FromMesh(BeMeshPrimitives::Sphere(10, 16), phongShader, "geometry-main");
    flash->Materials[0]->SetFloat3("DiffuseColor", glm::vec3(1.0f, 0.7f, 0.2f));
    flash->Materials[0]->SetFloat3("EmissiveColor", glm::vec3(18.0f, 8.0f, 1.4f));
    _assetRegistry.AddProp("flash", flash);
    _machine->RegisterMesh(flash->Mesh);

    auto floor = BeProp::FromMesh(
        RiftTerrain::BuildMesh(Settings.Terrain.Size, Settings.Terrain.Cells, Settings.Terrain.SpikeAmplitude),
        phongShader, "geometry-main"
    );
    floor->Materials[0]->SetFloat3("DiffuseColor", Settings.Terrain.Color);
    _assetRegistry.AddProp("floor", floor);
    _machine->RegisterMesh(floor->Mesh);

    auto stationShader = BeShaderLibrary::GetShader("station");
    for (const auto& kind : Settings.Delivery.Config.StationKinds) {
        auto prop = _machine->LoadProp(kind.Path, stationShader, BeSRMLightingModel::Phong);
        for (const auto& material : prop->Materials) {
            material->SetFloat1("EmissiveMix", kind.EmissiveMix);
        }
        _assetRegistry.AddProp(kind.Prop, prop);
    }

    _machine->BakeMeshes();

    _machine->DeclareGBufferTarget("Rift_BaseColor",         SenFormat::R11G11B10_Float);
    _machine->DeclareGBufferTarget("Rift_WorldNormal",       SenFormat::RGBA16_Float);
    _machine->DeclareGBufferTarget("Rift_SpecularShininess", SenFormat::RGBA8_Unorm);
    _machine->DeclareGBufferTarget("Rift_Emissive",          SenFormat::R11G11B10_Float);
    _machine->DeclareDepthTarget  ("Rift_Depth",             SenFormat::Depth32);
    _machine->DeclareTextureTarget("Rift_HDR",               SenFormat::R11G11B10_Float);
    _machine->DeclareTextureTarget("Rift_Post",              SenFormat::R11G11B10_Float);
    _machine->DeclareTextureTarget("Rift_UI",                SenFormat::RGBA8_Unorm);
}

auto RiftScene::DefineScene() -> void {
    _registry.clear();

    CreateEntity(_registry
        ,NameComponent { .Name = "box-left" }
        ,TransformComponent { .Position = { -1.5f, 0.5f, 0.f } }
        ,RenderComponent { .Prop = _assetRegistry.GetProp("box").lock(), .CastShadows = true }
    );
    CreateEntity(_registry
        ,NameComponent { .Name = "box-right" }
        ,TransformComponent { .Position = { 1.5f, 0.5f, 0.f } }
        ,RenderComponent { .Prop = _assetRegistry.GetProp("box").lock(), .CastShadows = true }
    );

    for (int tile = 0; tile < 9; ++tile) {
        _terrainTiles[tile] = CreateEntity(_registry
            ,NameComponent { .Name = "terrain-" + std::to_string(tile) }
            ,TransformComponent { }
            ,RenderComponent { .Prop = _assetRegistry.GetProp("floor").lock(), .CastShadows = true }
        );
    }

    CreateEntity(_registry
        ,NameComponent { .Name = "Sun" }
        ,SunLightComponent {
            .Direction = Settings.Sun.Direction,
            .Color = Settings.Sun.Color,
            .Power = Settings.Sun.Power,
            .CastsShadows = false,
            .ShadowCameraDistance = Settings.Sun.ShadowCameraDistance,
            .ShadowMapWorldSize = Settings.Sun.ShadowMapWorldSize,
            .ShadowNearPlane = Settings.Sun.ShadowNearPlane,
            .ShadowFarPlane = Settings.Sun.ShadowFarPlane,
        }
    );
}

auto RiftScene::DefinePasses() -> void {
    _machine->ClearPasses();
    _machine->AddShadowPass();
    _machine->AddGeometryPass();
    _machine->AddLightingPass("Rift_HDR");

    const auto& posterizeScheme = BeShaderLibrary::GetShader("posterize")->GetMaterialScheme("main");
    _posterizeMaterial = BeMaterial::Create(posterizeScheme);
    _posterizeMaterial->SetTexture("ColorTexture", _machine->GetRenderTexture("Rift_HDR"));
    _posterizeMaterial->SetTexture("DepthTexture", _machine->GetRenderTexture("Rift_Depth"));
    _posterizeMaterial->SetFloat1("PixelSize", Settings.Posterize.PixelSize);
    _posterizeMaterial->SetFloat1("DitherSpread", Settings.Posterize.DitherSpread);
    _posterizeMaterial->SetFloat1("FogStart", Settings.Posterize.FogStart);
    _posterizeMaterial->SetFloat1("FogEnd", Settings.Posterize.FogEnd);
    _posterizeMaterial->SetFloat3("FogColor", Settings.Posterize.FogColor);
    _posterizeMaterial->SetFloat3Array("Palette", Settings.Posterize.Palette);
    _posterizeMaterial->SetFloat1("PaletteCount", static_cast<float>(Settings.Posterize.Palette.size()));
    _posterizeMaterial->SetFloat1("Enabled", Settings.Posterize.Enabled ? 1.0f : 0.0f);
    _posterizeMaterial->SetFloat1("Glow", 0.0f);
    _posterizeMaterial->SetFloat3("GlowColor", glm::vec3(1.0f, 0.76f, 0.29f));
    _posterizeMaterial->SetTexture("UITexture", _machine->GetRenderTexture("Rift_UI"));

    const uint32_t screenWidth  = _gameIns->Renderer->GetSwapchainPixelWidth();
    const uint32_t screenHeight = _gameIns->Renderer->GetSwapchainPixelHeight();
    const auto& hudScheme = BeShaderLibrary::GetShader("ship-hud")->GetMaterialScheme("main");
    _hudMaterial = BeMaterial::Create(hudScheme);
    _hudMaterial->SetFloat2("ScreenSize", { static_cast<float>(screenWidth), static_cast<float>(screenHeight) });
    _hudMaterial->SetFloat1("PixelSize", Settings.Posterize.PixelSize);
    _hudMaterial->SetFloat1("LineHalf", 0.6f);
    _hudMaterial->SetFloat1("PipHalf", 1.0f);
    _machine->AddFullscreenPass(BeShaderLibrary::GetShader("ship-hud"), _hudMaterial, { "Rift_UI" });

    _machine->AddFullscreenPass(BeShaderLibrary::GetShader("posterize"), _posterizeMaterial, { "Rift_Post" });

    _machine->AddBackbufferPass("Rift_Post");
    _machine->InitialisePasses();
}

void RiftScene::Tick(float deltaTime) {
    if (_gameIns->Input->GetKeyDown(GLFW_KEY_ESCAPE)) {
        if (_shopOpen) {
            _shopOpen = false;
        } else {
            _gameIns->Input->SetMouseCapture(false);
            _gameIns->SceneManager->RequestSceneChange("menu");
            return;
        }
    }

    if (_gameIns->Input->GetKeyDown(GLFW_KEY_TAB) && !_dead) {
        _shopOpen = !_shopOpen;
    }
    if (_shopOpen && !_dead) {
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_1)) TryBuy(_speedLevel, Settings.Gig.SpeedCost);
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_2)) TryBuy(_gunLevel, Settings.Gig.GunCost);
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_3)) TryBuy(_styleLevel, Settings.Gig.StyleCost);
        ApplyLoadout();
    }

    if (_dead && _gameIns->Input->GetKeyDown(GLFW_KEY_R)) {
        RestartRun();
    }

    if (_gameIns->Input->GetKeyDown(GLFW_KEY_ENTER)) {
        Settings.Posterize.Enabled = !Settings.Posterize.Enabled;
        _posterizeMaterial->SetFloat1("Enabled", Settings.Posterize.Enabled ? 1.0f : 0.0f);
    }

    if (_gameIns->Input->GetKeyDown(GLFW_KEY_P)) {
        if (_delivery) ExitPlayMode();
        else EnterPlayMode();
    }

    if (_gameIns->Input->GetKeyDown(GLFW_KEY_T) && _delivery) {
        _delivery->TargetNearest(_camera->Position);
    }

    if (_shopOpen) {
        _gameIns->Input->SetMouseCapture(false);
    } else {
        _shipCameraController->Update(deltaTime, _gameIns->Input.get());
        TickCombat(deltaTime);
        TickGigs(deltaTime);
    }
    _hudMaterial->SetFloat2("AimOffset", _shipCameraController->GetAim());
    _hudMaterial->SetFloat1("GameOver", _dead ? std::max(_gameOverTime, 0.2f) : 0.0f);
    _hudMaterial->SetFloat1("Kills", static_cast<float>(_kills));
    _hudMaterial->SetFloat1("Ammo", static_cast<float>(_ammo));
    _hudMaterial->SetFloat1("AmmoMax", static_cast<float>(_ammoMax));
    _hudMaterial->SetFloat1("Credits", static_cast<float>(_credits));
    _hudMaterial->SetFloat1("Boost", _boostLeft / Settings.Gig.BoostSeconds);
    _hudMaterial->SetFloat1("Shop", _shopOpen ? 1.0f : 0.0f);
    _hudMaterial->SetFloat1("SpeedLvl", static_cast<float>(_speedLevel));
    _hudMaterial->SetFloat1("GunLvl", static_cast<float>(_gunLevel));
    _hudMaterial->SetFloat1("StyleLvl", static_cast<float>(_styleLevel));
    _hudMaterial->SetFloat1("PayFlash", _toastLeft);

    const glm::vec3 worldUp = { 0.0f, 1.0f, 0.0f };
    const glm::vec2 upScreen = { glm::dot(worldUp, _camera->GetRight()), glm::dot(worldUp, _camera->GetUp()) };
    glm::vec2 horizonDir = { upScreen.y, -upScreen.x };
    const float horizonLen = glm::length(horizonDir);
    horizonDir = horizonLen > 1e-3f ? horizonDir / horizonLen : glm::vec2(1.0f, 0.0f);
    _hudMaterial->SetFloat2("HorizonDir", { horizonDir.x, -horizonDir.y });

    if (_delivery && _delivery->Update(_camera->Position)) {
        SettleDelivery(glm::length(_shipCameraController->GetVelocity()));
    }

    const float screenW = static_cast<float>(_gameIns->Renderer->GetSwapchainPixelWidth());
    const float screenH = static_cast<float>(_gameIns->Renderer->GetSwapchainPixelHeight());
    const auto& marker = Settings.Delivery.Marker;
    float targetState = 0.0f;
    glm::vec2 targetPixel = { screenW * 0.5f, screenH * 0.5f };
    glm::vec2 targetDir = { 0.0f, 1.0f };
    float targetRadius = marker.MinRadius;
    float targetAlpha = 1.0f;
    if (_delivery && _delivery->HasTarget()) {
        const glm::vec4 clip = _camera->GetProjectionMatrix() * _camera->GetViewMatrix()
            * glm::vec4(_delivery->TargetPosition(), 1.0f);
        const bool behind = clip.w <= 1e-4f;
        glm::vec2 ndc = glm::vec2(clip.x, clip.y) / clip.w;
        if (behind) ndc = -ndc;
        const bool onScreen = !behind && std::abs(ndc.x) <= 1.0f && std::abs(ndc.y) <= 1.0f;
        if (onScreen) {
            targetState = 1.0f;
            targetPixel = { (ndc.x * 0.5f + 0.5f) * screenW, (0.5f - ndc.y * 0.5f) * screenH };
            const float distance = glm::length(_delivery->TargetPosition() - _camera->Position);
            targetRadius = glm::mix(marker.MinRadius, marker.MaxRadius, glm::smoothstep(marker.SizeFar, marker.SizeNear, distance));
            targetAlpha = glm::smoothstep(marker.FadeNear, marker.FadeFar, distance);
        } else {
            targetState = 2.0f;
            const float extent = std::max(std::max(std::abs(ndc.x), std::abs(ndc.y)), 1e-4f);
            const glm::vec2 marked = (ndc / extent) * marker.ScreenMargin;
            targetPixel = { (marked.x * 0.5f + 0.5f) * screenW, (0.5f - marked.y * 0.5f) * screenH };
            targetDir = glm::normalize(glm::vec2(ndc.x, -ndc.y));
        }
    }
    _hudMaterial->SetFloat2("TargetPos", targetPixel);
    _hudMaterial->SetFloat2("TargetDir", targetDir);
    _hudMaterial->SetFloat1("TargetState", targetState);
    _hudMaterial->SetFloat1("TargetRingRadius", targetRadius);
    _hudMaterial->SetFloat1("TargetAlpha", targetAlpha);

    const float tileSize = Settings.Terrain.Size;
    const int centerX = static_cast<int>(std::round(_camera->Position.x / tileSize));
    const int centerZ = static_cast<int>(std::round(_camera->Position.z / tileSize));
    int tileIndex = 0;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            auto& tileTransform = _registry.get<TransformComponent>(_terrainTiles[tileIndex++]);
            tileTransform.Position = glm::vec3((centerX + i) * tileSize, 0.0f, (centerZ + j) * tileSize);
        }
    }

    FullScene::Tick(deltaTime);
}

auto RiftScene::RandomRange(float lo, float hi) -> float {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(_combatRng);
}

auto RiftScene::SpawnAlienAround(glm::vec3 origin) -> void {
    auto prop = _assetRegistry.GetProp("alien").lock();
    if (!prop) return;

    glm::vec3 position = origin;
    for (int attempt = 0; attempt < 24; ++attempt) {
        const float angle = RandomRange(0.0f, glm::two_pi<float>());
        const float radius = Settings.Combat.AlienSpawnFar * std::sqrt(RandomRange(0.0f, 1.0f));
        const float x = radius * std::cos(angle);
        const float z = radius * std::sin(angle);
        const float ground = RiftTerrain::SampleHeight(x, z, Settings.Terrain.Size, Settings.Terrain.SpikeAmplitude);
        const glm::vec3 candidate{
            x,
            ground + Settings.Combat.AlienMinAltitude + RandomRange(0.0f, 80.0f),
            z
        };
        if (glm::length(candidate - origin) >= Settings.Combat.AlienSpawnNear) {
            position = candidate;
            break;
        }
    }
    const glm::vec3 toShip = glm::normalize(origin - position);
    const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 safeUp = std::abs(glm::dot(toShip, up)) > 0.95f ? glm::vec3(1.0f, 0.0f, 0.0f) : up;
    CreateEntity(_registry
        ,NameComponent { .Name = "alien" }
        ,TransformComponent {
            .Position = position,
            .Rotation = glm::quatLookAtLH(toShip, safeUp),
            .Scale = glm::vec3(14.0f)
        }
        ,RenderComponent { .Prop = prop, .CastShadows = false }
        ,AlienComponent {
            .HitRadius = Settings.Combat.AlienHitRadius,
            .Speed = RandomRange(Settings.Combat.AlienMinSpeed, Settings.Combat.AlienMaxSpeed),
            .Wobble = RandomRange(0.0f, glm::two_pi<float>()),
        }
    );
}

auto RiftScene::SpawnAliens() -> void {
    const auto aliens = _registry.view<AlienComponent>();
    _registry.destroy(aliens.begin(), aliens.end());
    for (int i = 0; i < Settings.Combat.AlienCount; ++i) {
        SpawnAlienAround(_camera->Position);
    }
}

auto RiftScene::SpawnBurst(glm::vec3 origin, glm::vec3 color, int count, float power) -> void {
    auto debrisProp = _assetRegistry.GetProp("debris").lock();
    auto flashProp = _assetRegistry.GetProp("flash").lock();
    if (flashProp) {
        CreateEntity(_registry
            ,NameComponent { .Name = "flash" }
            ,TransformComponent { .Position = origin, .Scale = glm::vec3(power * 0.35f) }
            ,RenderComponent { .Prop = flashProp, .CastShadows = false }
            ,DebrisComponent { .Velocity = {}, .Life = 0.22f, .MaxLife = 0.22f }
            ,PointLightComponent { .Radius = power * 4.0f, .Color = color, .Power = power * 8.0f }
        );
    }
    if (!debrisProp) return;
    for (int i = 0; i < count; ++i) {
        const glm::vec3 dir = glm::normalize(glm::vec3(
            RandomRange(-1.0f, 1.0f),
            RandomRange(0.15f, 1.0f),
            RandomRange(-1.0f, 1.0f)
        ));
        const float speed = RandomRange(power * 0.6f, power * 1.8f);
        const float life = RandomRange(0.45f, 1.15f);
        CreateEntity(_registry
            ,NameComponent { .Name = "debris" }
            ,TransformComponent {
                .Position = origin + dir * RandomRange(0.4f, 2.2f),
                .Scale = glm::vec3(RandomRange(0.6f, 2.4f))
            }
            ,RenderComponent { .Prop = debrisProp, .CastShadows = false }
            ,DebrisComponent { .Velocity = dir * speed, .Life = life, .MaxLife = life }
        );
    }
}

auto RiftScene::TriggerCrash() -> void {
    if (_dead) return;
    _dead = true;
    _gameOverTime = 0.0f;
    _shipCameraController->StopHard();
    SpawnBurst(_camera->Position, glm::vec3(1.0f, 0.45f, 0.08f), 28, 22.0f);
}

auto RiftScene::RestartRun() -> void {
    const auto debris = _registry.view<DebrisComponent>();
    _registry.destroy(debris.begin(), debris.end());
    const auto tracers = _registry.view<TracerComponent>();
    _registry.destroy(tracers.begin(), tracers.end());
    _dead = false;
    _kills = 0;
    _fireCooldown = 0.0f;
    _gameOverTime = 0.0f;
    _shopOpen = false;
    _boostLeft = 0.0f;
    _toastLeft = 0.0f;
    _ammo = _ammoMax;
    _camera->Position = glm::vec3(0.0f, Settings.Camera.SpawnHeight, 0.0f);
    _camera->SetOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    _shipCameraController->ResetMotion();
    ApplyLoadout();
    SpawnAliens();
}

auto RiftScene::TickCombat(float deltaTime) -> void {
    if (_dead) {
        _gameOverTime += deltaTime;
    } else {
        const float ground = RiftTerrain::SampleHeight(
            _camera->Position.x, _camera->Position.z,
            Settings.Terrain.Size, Settings.Terrain.SpikeAmplitude
        );
        if (_camera->Position.y < ground) {
            _camera->Position.y = ground;
            TriggerCrash();
        }
    }

    _fireCooldown = std::max(0.0f, _fireCooldown - deltaTime);
    if (!_dead && _ammo > 0 && _fireCooldown <= 0.0f && _gameIns->Input->GetMouseButton(GLFW_MOUSE_BUTTON_LEFT)) {
        _fireCooldown = Settings.Combat.FireCooldown;
        --_ammo;
        auto tracerProp = _assetRegistry.GetProp("tracer").lock();
        if (tracerProp) {
            const glm::vec3 origin = _camera->Position + _camera->GetFront() * 4.0f + _camera->GetRight() * 1.4f - _camera->GetUp() * 0.8f;
            CreateEntity(_registry
                ,NameComponent { .Name = "tracer" }
                ,TransformComponent {
                    .Position = origin,
                    .Rotation = _camera->GetOrientation(),
                    .Scale = glm::vec3(0.25f, 0.25f, 2.8f)
                }
                ,RenderComponent { .Prop = tracerProp, .CastShadows = false }
                ,TracerComponent { .Velocity = _camera->GetFront() * Settings.Combat.TracerSpeed, .Life = Settings.Combat.TracerLife, .Spent = false }
            );
        }
    }

    std::vector<entt::entity> doomed;
    auto tracers = _registry.view<TransformComponent, TracerComponent>();
    for (auto entity : tracers) {
        auto& transform = tracers.get<TransformComponent>(entity);
        auto& tracer = tracers.get<TracerComponent>(entity);
        transform.Position += tracer.Velocity * deltaTime;
        tracer.Life -= deltaTime;

        bool hit = false;
        auto aliens = _registry.view<TransformComponent, AlienComponent>();
        for (auto alienEntity : aliens) {
            auto& alienTransform = aliens.get<TransformComponent>(alienEntity);
            const auto& alien = aliens.get<AlienComponent>(alienEntity);
            if (glm::length(alienTransform.Position - transform.Position) <= alien.HitRadius + Settings.Combat.TracerHitRadius) {
                const glm::vec3 boomAt = alienTransform.Position;
                doomed.push_back(alienEntity);
                doomed.push_back(entity);
                ++_kills;
                _ammo = std::min(_ammo + Settings.Combat.AmmoPerKill, _ammoMax);
                SpawnBurst(boomAt, glm::vec3(0.35f, 1.6f, 0.4f), 14, 10.0f);
                SpawnAlienAround(_camera->Position);
                hit = true;
                break;
            }
        }
        if (!hit && tracer.Life <= 0.0f) {
            doomed.push_back(entity);
        }
    }

    if (!_dead) {
        auto aliens = _registry.view<TransformComponent, AlienComponent>();
        for (auto [entity, transform, alien] : aliens.each()) {
            const glm::vec3 toShip = _camera->Position - transform.Position;
            const float distance = glm::length(toShip);
            if (distance > 0.001f) {
                const glm::vec3 dir = toShip / distance;
                transform.Position += dir * alien.Speed * deltaTime;
                transform.Rotation = glm::normalize(glm::quatLookAtLH(dir, glm::vec3(0.0f, 1.0f, 0.0f)));
            }
            transform.Position.y += std::sin(_time * 3.0f + alien.Wobble) * 4.0f * deltaTime;
            if (distance <= Settings.Combat.CatchRadius) {
                TriggerCrash();
            }
        }
    }

    auto debris = _registry.view<TransformComponent, DebrisComponent>();
    for (auto entity : debris) {
        auto& transform = debris.get<TransformComponent>(entity);
        auto& piece = debris.get<DebrisComponent>(entity);
        transform.Position += piece.Velocity * deltaTime;
        piece.Velocity.y -= 28.0f * deltaTime;
        piece.Velocity *= (1.0f - 0.8f * deltaTime);
        piece.Life -= deltaTime;
        const float age = 1.0f - glm::clamp(piece.Life / std::max(piece.MaxLife, 0.001f), 0.0f, 1.0f);
        transform.Scale *= (1.0f - 0.7f * deltaTime);
        if (_registry.all_of<PointLightComponent>(entity)) {
            _registry.get<PointLightComponent>(entity).Power *= (1.0f - age);
        }
        if (piece.Life <= 0.0f || std::min({transform.Scale.x, transform.Scale.y, transform.Scale.z}) < 0.05f) {
            doomed.push_back(entity);
        }
    }

    std::ranges::sort(doomed);
    doomed.erase(std::ranges::unique(doomed).begin(), doomed.end());
    for (const auto entity : doomed) {
        if (_registry.valid(entity)) {
            _registry.destroy(entity);
        }
    }
}

auto RiftScene::ApplyLoadout() -> void {
    _ammoMax = Settings.Combat.StartingAmmo + _gunLevel * 4;
    _ammo = std::min(_ammo, _ammoMax);
    const float boost = _boostLeft > 0.0f ? Settings.Gig.BoostMul : 1.0f;
    _shipCameraController->SpeedMul = (1.0f + 0.15f * static_cast<float>(_speedLevel)) * boost;
    Settings.Combat.FireCooldown = 0.11f * std::pow(0.84f, static_cast<float>(_gunLevel));
    Settings.Combat.TracerSpeed = 260.0f + 40.0f * static_cast<float>(_gunLevel);
}

auto RiftScene::TryBuy(int& level, int baseCost) -> bool {
    const int cost = baseCost * (level + 1);
    if (_credits < cost || level >= 5) return false;
    _credits -= cost;
    ++level;
    _toastLeft = 1.0f;
    return true;
}

auto RiftScene::SettleDelivery(float speed) -> void {
    const int pay = Settings.Gig.BasePay + _styleLevel * 4;
    int tip = 0;
    const float chance = Settings.Gig.PatrolTipChance + 0.06f * static_cast<float>(_styleLevel);
    if (RandomRange(0.0f, 1.0f) < chance) {
        tip = static_cast<int>(Settings.Gig.PatrolTipMin + speed * Settings.Gig.PatrolTipSpeedScale + 3.0f * static_cast<float>(_styleLevel));
    }
    _credits += pay + tip;
    _lastPay = pay;
    _lastTip = tip;
    _toastLeft = 1.6f;
}

auto RiftScene::TickGigs(float deltaTime) -> void {
    _toastLeft = std::max(0.0f, _toastLeft - deltaTime);
    _boostLeft = std::max(0.0f, _boostLeft - deltaTime);

    float glow = 0.0f;
    if (_delivery && _delivery->HasTarget()) {
        const float dist = glm::length(_delivery->TargetPosition() - _camera->Position);
        if (dist < Settings.Gig.ApproachGlowRadius) {
            glow = 1.0f - dist / Settings.Gig.ApproachGlowRadius;
            if (_boostLeft <= 0.0f) _boostLeft = Settings.Gig.BoostSeconds;
        }
    }
    if (_boostLeft > 0.0f) {
        glow = std::max(glow, 0.35f + 0.65f * (_boostLeft / Settings.Gig.BoostSeconds));
    }
    _posterizeMaterial->SetFloat1("Glow", glow);
    ApplyLoadout();
}
