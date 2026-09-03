#include "RiftScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include <umbrellas/include-glfw.h>
#include <umbrellas/include-glm.h>
#include <umbrellas/include-libassert.h>

#include "BeMesh.h"

#include "ShipCameraController.h"
#include "DeliverySystem.h"
#include "MetaSystem.h"
#include "StationUI.h"
#include "RiftSettings.h"
#include "RiftTerrain.h"
#include "BeCamera.h"
#include "imgui/BeImGuiPass.h"
#include "imgui/imgui.h"
#include "BeInput.h"
#include "BeWindow.h"
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

RiftScene::RiftScene(Game* game) : FullScene(game) {
    RiftStore::Bootstrap();
    RiftStore::Get().Seed = std::random_device{}();
}

RiftScene::~RiftScene() {
    RiftStore::Shutdown();
}

void RiftScene::Prepare() {
    FullScene::Prepare();

    _camera->Position = glm::vec3(0.0f, RiftStore::Get().Camera.SpawnHeight, 0.0f);

    _shipCameraController = std::make_unique<ShipCameraController>(_camera.get(), _terrain.get());
    _hudMaterial->SetFloat1("AimRadius", RiftStore::Get().Ship.AimRadius);

    _ammo = RiftStore::Get().Combat.StartingAmmo;
    _ammoMax = RiftStore::Get().Combat.StartingAmmo;
    ApplyLoadout();
}

auto RiftScene::EnterPlayMode() -> void {
    _delivery = std::make_unique<DeliverySystem>(_registry, _assetRegistry, *_terrain);
    _delivery->GenerateStations();
    _meta.Begin();

    _kills = 0;
    _ammo = _ammoMax;
    SpawnAliens();
}

auto RiftScene::SetStationUiOpen(bool open) -> void {
    _stationUiOpen = open;
}

auto RiftScene::ExitPlayMode() -> void {
    const auto stations = _registry.view<StationComponent>();
    _registry.destroy(stations.begin(), stations.end());
    const auto docks = _registry.view<DockComponent>();
    _registry.destroy(docks.begin(), docks.end());
    ClearCombatEntities();
    _shipCameraController->Uncapture();
    SetStationUiOpen(false);
    _shopOpen = false;
    _boostLeft = 0.0f;
    _delivery.reset();
    _meta.End();
    _hudMaterial->SetFloat1("TargetState", 0.0f);
}

auto RiftScene::StartDeath() -> void {
    if (_dying || !_delivery) return;
    SpawnBurst(_camera->Position, glm::vec3(1.0f, 0.45f, 0.08f), 28, 22.0f);
    _coroutineScheduler.Start(DeathSequence());
}

auto RiftScene::DeathSequence() -> BeCoroutine {
    const auto& ship = RiftStore::Get().Ship;
    _dying = true;

    const float fadeOutStart = _time;
    while (_time - fadeOutStart < ship.DeathFadeOutTime) {
        _posterizeMaterial->SetFloat1("Fade", (_time - fadeOutStart) / ship.DeathFadeOutTime);
        co_yield 0.0f;
    }
    _posterizeMaterial->SetFloat1("Fade", 1.0f);

    _delivery->ApplyCrashPenalty();
    _shipCameraController->Respawn(_delivery->GetRespawnDock(_camera->Position));
    _ammo = _ammoMax;
    _boostLeft = 0.0f;
    _shopOpen = false;

    co_yield ship.DeathHoldTime;

    const float fadeInStart = _time;
    while (_time - fadeInStart < ship.DeathFadeInTime) {
        _posterizeMaterial->SetFloat1("Fade", 1.0f - (_time - fadeInStart) / ship.DeathFadeInTime);
        co_yield 0.0f;
    }
    _posterizeMaterial->SetFloat1("Fade", 0.0f);

    _dying = false;
}

auto RiftScene::DefineSettings() -> void {
    const auto& settings = RiftStore::Get();
    _camera->NearPlane = settings.Camera.NearPlane;
    _camera->FarPlane = settings.Camera.FarPlane;
    _machine->UniformMaterial->SetFloat3("AmbientColor", settings.Ambient.Color);
}

auto RiftScene::DefineAssets() -> void {
    const auto& settings = RiftStore::Get();
    auto phongShader = BeShaderLibrary::GetShader("standard-phong");

    auto box = BeProp::FromMesh(BeMeshPrimitives::Cube(), phongShader, "geometry-main");
    box->Materials[0]->SetFloat3("DiffuseColor", glm::vec3(1.0f));
    _assetRegistry.AddProp("box", box);
    _machine->RegisterMesh(box->Mesh);

    // combat props: alien hunter, tracer, debris, flash
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

    _terrain = std::make_unique<RiftTerrain>();

    const auto packedHeights = _terrain->CopyPackedHeights();
    const auto resolution = static_cast<uint32_t>(_terrain->GetResolution());
    auto heightMap = BeTexture::Create("rift-heightmap")
        .SetSize(resolution, resolution)
        .SetFormat(SenFormat::R32_Float)
        .SetUsage(SenTextureUsage::ShaderResource)
        .FillFromMemory(reinterpret_cast<const uint8_t*>(packedHeights.data()))
        .Build();

    auto terrainShader = BeShaderLibrary::GetShader("rift-terrain");
    be_assert(settings.Terrain.GridVerticesPerRenderTile % 2 == 1, "vertices per render tile must be odd so tile vertices land on integer logical coords");
    auto floor = BeProp::FromMesh(BeMeshPrimitives::Plane(settings.Terrain.GridVerticesPerRenderTile - 1), terrainShader, "geometry-main");
    floor->Materials[0]->SetFloat3("DiffuseColor", settings.Terrain.Color);
    floor->Materials[0]->SetTexture("HeightMap", heightMap);
    floor->Materials[0]->SetSampler("HeightSampler", BeShaderLibrary::GetSampler("point-wrap"));
    floor->Materials[0]->SetFloat1("MapSize", settings.Terrain.LogicalMapWorldSize);
    floor->Materials[0]->SetFloat1("MapResolution", static_cast<float>(resolution));
    floor->Materials[0]->SetFloat1("HeightScale", 1.0f);
    _assetRegistry.AddProp("floor", floor);
    _machine->RegisterMesh(floor->Mesh);

    auto stationShader = BeShaderLibrary::GetShader("station");
    for (const auto& kind : settings.Delivery.Kinds) {
        auto prop = _machine->LoadProp(kind.Path, stationShader, BeSRMLightingModel::Phong);
        for (const auto& material : prop->Materials) {
            material->SetFloat1("EmissiveMix", kind.EmissiveMix);
        }
        _assetRegistry.AddProp(kind.Prop, prop);
    }

    auto ringShader = BeShaderLibrary::GetShader("dock-ring");
    auto ring = BeProp::FromMesh(BeMeshPrimitives::Plane(), ringShader, "geometry-main");
    _assetRegistry.AddProp("dock-ring", ring);
    _machine->RegisterMesh(ring->Mesh);

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

    const float tileSize = RiftStore::Get().Terrain.GetRenderTileWorldSize();
    for (int tile = 0; tile < 9; ++tile) {
        _terrainTiles[tile] = CreateEntity(_registry
            ,NameComponent { .Name = "terrain-" + std::to_string(tile) }
            ,TransformComponent { .Scale = { tileSize, 1.0f, tileSize } }
            ,RenderComponent { .Prop = _assetRegistry.GetProp("floor").lock(), .CastShadows = false }
        );
    }

    const auto& sun = RiftStore::Get().Sun;
    CreateEntity(_registry
        ,NameComponent { .Name = "Sun" }
        ,SunLightComponent {
            .Direction = sun.Direction,
            .Color = sun.Color,
            .Power = sun.Power,
            .CastsShadows = false,
            .ShadowCameraDistance = sun.ShadowCameraDistance,
            .ShadowMapWorldSize = sun.ShadowMapWorldSize,
            .ShadowNearPlane = sun.ShadowNearPlane,
            .ShadowFarPlane = sun.ShadowFarPlane,
        }
    );
}

auto RiftScene::DefinePasses() -> void {
    _machine->ClearPasses();
    _machine->AddShadowPass();
    _machine->AddGeometryPass();
    _machine->AddLightingPass("Rift_HDR");

    const auto& posterize = RiftStore::Get().Posterize;
    const auto& posterizeScheme = BeShaderLibrary::GetShader("posterize")->GetMaterialScheme("main");
    _posterizeMaterial = BeMaterial::Create(posterizeScheme);
    _posterizeMaterial->SetTexture("ColorTexture", _machine->GetRenderTexture("Rift_HDR"));
    _posterizeMaterial->SetTexture("DepthTexture", _machine->GetRenderTexture("Rift_Depth"));
    _posterizeMaterial->SetFloat1("PixelSize", posterize.PixelSize);
    _posterizeMaterial->SetFloat1("DitherSpread", posterize.DitherSpread);
    _posterizeMaterial->SetFloat1("FogStart", posterize.FogStart);
    _posterizeMaterial->SetFloat1("FogEnd", posterize.FogEnd);
    _posterizeMaterial->SetFloat3("FogColor", posterize.FogColor);
    _posterizeMaterial->SetFloat3Array("Palette", posterize.Palette);
    _posterizeMaterial->SetFloat1("PaletteCount", static_cast<float>(posterize.Palette.size()));
    _posterizeMaterial->SetFloat1("Enabled", posterize.Enabled ? 1.0f : 0.0f);
    _posterizeMaterial->SetFloat1("Glow", 0.0f);
    _posterizeMaterial->SetFloat3("GlowColor", glm::vec3(1.0f, 0.76f, 0.29f));
    _posterizeMaterial->SetTexture("UITexture", _machine->GetRenderTexture("Rift_UI"));

    const uint32_t screenWidth  = _gameIns->Renderer->GetSwapchainPixelWidth();
    const uint32_t screenHeight = _gameIns->Renderer->GetSwapchainPixelHeight();
    const auto& hudScheme = BeShaderLibrary::GetShader("ship-hud")->GetMaterialScheme("main");
    _hudMaterial = BeMaterial::Create(hudScheme);
    _hudMaterial->SetFloat2("ScreenSize", { static_cast<float>(screenWidth), static_cast<float>(screenHeight) });
    _hudMaterial->SetFloat1("PixelSize", posterize.PixelSize);
    _hudMaterial->SetFloat1("LineHalf", 0.6f);
    _hudMaterial->SetFloat1("PipHalf", 1.0f);
    _machine->AddFullscreenPass(BeShaderLibrary::GetShader("ship-hud"), _hudMaterial, { "Rift_UI" });

    _machine->AddFullscreenPass(BeShaderLibrary::GetShader("posterize"), _posterizeMaterial, { "Rift_Post" });

    _machine->AddBackbufferPass("Rift_Post");

    auto imguiPass = std::make_unique<BeImGuiPass>(_gameIns->Window);
    imguiPass->SetUICallback([this]() {
        ImGui::PushFont(_riftFont);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImGui::GetStyle().Colors[ImGuiCol_TitleBg]);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.22f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.35f));
        if (_delivery) _meta.DrawUI(*_delivery);
        if (_showDebug) _shipCameraController->DrawDebugUI();
        if (_stationUiOpen && _delivery) StationUI::Draw(*_delivery, _camera->Position);
        ImGui::PopStyleColor(4);
        ImGui::PopFont();
    });
    _machine->AddPass(std::move(imguiPass));

    _machine->InitialisePasses();

    _riftFont = ImGui::GetIO().Fonts->AddFontFromFileTTF("assets/rift/b612-mono/B612Mono-Regular.ttf", 20.0f);
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

    if (_gameIns->Input->GetKeyDown(GLFW_KEY_ENTER)) {
        bool& enabled = RiftStore::Get().Posterize.Enabled;
        enabled = !enabled;
        _posterizeMaterial->SetFloat1("Enabled", enabled ? 1.0f : 0.0f);
    }

    if (_gameIns->Input->GetKeyDown(GLFW_KEY_P) && !_dying) {
        if (_delivery) ExitPlayMode();
        else EnterPlayMode();
    }

    if (_gameIns->Input->GetKeyDown(GLFW_KEY_F1)) _showDebug = !_showDebug;

    const bool cheatMod = _gameIns->Input->GetKey(GLFW_KEY_LEFT_CONTROL) && _gameIns->Input->GetKey(GLFW_KEY_LEFT_ALT);
    if (cheatMod && _delivery) {
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_EQUAL)) _delivery->SetCredits(_delivery->GetCredits() + 1000);
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_MINUS)) _delivery->SetCredits(_delivery->GetCredits() - 1000);
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_LEFT_BRACKET)) _meta.SetElapsed(_meta.GetElapsed() - 30.0f);
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_RIGHT_BRACKET)) _meta.SetElapsed(_meta.GetElapsed() + 30.0f);
    }

    if (_delivery) {
        _meta.Update(deltaTime, *_delivery);
        if (_meta.WantsClose()) {
            _gameIns->Window->RequestClose();
            return;
        }
    }

    // shop: Tab toggles, 1/2 buy upgrades with delivery credits
    if (_delivery && !_dying && !_meta.IsPaused() && !_stationUiOpen && _gameIns->Input->GetKeyDown(GLFW_KEY_TAB)) {
        _shopOpen = !_shopOpen;
    }
    if (!_delivery) _shopOpen = false;
    if (_shopOpen) {
        const auto& shop = RiftStore::Get().Shop;
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_1)) TryBuy(_speedLevel, shop.SpeedCost);
        if (_gameIns->Input->GetKeyDown(GLFW_KEY_2)) TryBuy(_gunLevel, shop.GunCost);
    }

    const bool uiOpen = _meta.IsPaused() || _stationUiOpen || _shopOpen;
    _shipCameraController->SetControlsEnabled(!uiOpen && !_dying);
    _gameIns->Input->SetMouseCapture(!uiOpen);

    _shipCameraController->Update(deltaTime, _gameIns->Input.get());
    _hudMaterial->SetFloat2("AimOffset", _shipCameraController->GetAim());

    const glm::vec3 worldUp = { 0.0f, 1.0f, 0.0f };
    const glm::vec2 upScreen = { glm::dot(worldUp, _camera->GetRight()), glm::dot(worldUp, _camera->GetUp()) };
    glm::vec2 horizonDir = { upScreen.y, -upScreen.x };
    const float horizonLen = glm::length(horizonDir);
    horizonDir = horizonLen > 1e-3f ? horizonDir / horizonLen : glm::vec2(1.0f, 0.0f);
    _hudMaterial->SetFloat2("HorizonDir", { horizonDir.x, -horizonDir.y });

    if (_delivery && !_dying) {
        if (_shipCameraController->GetLastImpactSpeed() > RiftStore::Get().Ship.CrashImpactSpeed) {
            StartDeath();
        }
    }

    TickCombat(deltaTime, !uiOpen && !_dying);
    TickShop(deltaTime);

    _hudMaterial->SetFloat1("GameOver", 0.0f);
    _hudMaterial->SetFloat1("Kills", static_cast<float>(_kills));
    _hudMaterial->SetFloat1("Ammo", static_cast<float>(_ammo));
    _hudMaterial->SetFloat1("AmmoMax", static_cast<float>(_ammoMax));
    _hudMaterial->SetFloat1("Credits", _delivery ? static_cast<float>(_delivery->GetCredits()) / 100.0f : 0.0f);
    _hudMaterial->SetFloat1("Boost", _boostLeft / RiftStore::Get().Shop.BoostSeconds);
    _hudMaterial->SetFloat1("Shop", _shopOpen ? 1.0f : 0.0f);
    _hudMaterial->SetFloat1("SpeedLvl", static_cast<float>(_speedLevel));
    _hudMaterial->SetFloat1("GunLvl", static_cast<float>(_gunLevel));
    _hudMaterial->SetFloat1("PayFlash", _toastLeft);

    if (_delivery && !_dying) {
        const auto dock = _delivery->CheckDock(_camera->Position);
        _shipCameraController->SetInDock(dock.Hit);

        if (_shipCameraController->HasJustEnteredDock()) {
            _shipCameraController->Capture(dock.Anchor);
            _delivery->NotifyDocked(dock);
        }

        if (_shipCameraController->IsCaptured() && _gameIns->Input->GetKeyDown(GLFW_KEY_S)) {
            SetStationUiOpen(!_stationUiOpen);
        }

        if (_gameIns->Input->GetKeyDown(GLFW_KEY_C)) {
            _shipCameraController->Uncapture();
            _delivery->NotifyUndocked();
            SetStationUiOpen(false);
        }
    }

    const float screenW = static_cast<float>(_gameIns->Renderer->GetSwapchainPixelWidth());
    const float screenH = static_cast<float>(_gameIns->Renderer->GetSwapchainPixelHeight());
    _hudMaterial->SetFloat2("ScreenSize", { screenW, screenH });
    const auto& marker = RiftStore::Get().Delivery.Marker;
    float targetState = 0.0f;
    glm::vec2 targetPixel = { screenW * 0.5f, screenH * 0.5f };
    glm::vec2 targetDir = { 0.0f, 1.0f };
    float targetRadius = marker.MinRadius;
    float targetAlpha = 1.0f;
    if (_delivery && _delivery->HasContract() && !_delivery->CanComplete()) {
        const glm::vec3 targetWorld = _delivery->GetTargetPosition(_camera->Position);
        const glm::vec4 clip = _camera->GetProjectionMatrix() * _camera->GetViewMatrix()
            * glm::vec4(targetWorld, 1.0f);
        const bool behind = clip.w <= 1e-4f;
        glm::vec2 ndc = glm::vec2(clip.x, clip.y) / clip.w;
        if (behind) ndc = -ndc;
        const bool onScreen = !behind && std::abs(ndc.x) <= 1.0f && std::abs(ndc.y) <= 1.0f;
        if (onScreen) {
            targetState = 1.0f;
            targetPixel = { (ndc.x * 0.5f + 0.5f) * screenW, (0.5f - ndc.y * 0.5f) * screenH };
            const float distance = glm::length(targetWorld - _camera->Position);
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

    const float tileSize = RiftStore::Get().Terrain.GetRenderTileWorldSize();
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

// combat ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

auto RiftScene::RandomRange(float lo, float hi) -> float {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(_combatRng);
}

auto RiftScene::SpawnAlienAround(glm::vec3 origin) -> void {
    auto prop = _assetRegistry.GetProp("alien").lock();
    if (!prop || !_terrain) return;
    const auto& combat = RiftStore::Get().Combat;

    glm::vec3 position = origin + glm::vec3(0.0f, 0.0f, combat.AlienSpawnFar);
    for (int attempt = 0; attempt < 24; ++attempt) {
        const float angle = RandomRange(0.0f, glm::two_pi<float>());
        const float radius = combat.AlienSpawnFar * std::sqrt(RandomRange(0.0f, 1.0f));
        const float x = origin.x + radius * std::cos(angle);
        const float z = origin.z + radius * std::sin(angle);
        const float ground = _terrain->GetHeight(x, z);
        const glm::vec3 candidate{ x, ground + combat.AlienMinAltitude + RandomRange(0.0f, 80.0f), z };
        if (glm::length(candidate - origin) >= combat.AlienSpawnNear) {
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
            .HitRadius = combat.AlienHitRadius,
            .Speed = RandomRange(combat.AlienMinSpeed, combat.AlienMaxSpeed),
            .Wobble = RandomRange(0.0f, glm::two_pi<float>()),
        }
    );
}

auto RiftScene::SpawnAliens() -> void {
    const auto aliens = _registry.view<AlienComponent>();
    _registry.destroy(aliens.begin(), aliens.end());
    for (int i = 0; i < RiftStore::Get().Combat.AlienCount; ++i) {
        SpawnAlienAround(_camera->Position);
    }
}

auto RiftScene::ClearCombatEntities() -> void {
    const auto aliens = _registry.view<AlienComponent>();
    _registry.destroy(aliens.begin(), aliens.end());
    const auto tracers = _registry.view<TracerComponent>();
    _registry.destroy(tracers.begin(), tracers.end());
    const auto debris = _registry.view<DebrisComponent>();
    _registry.destroy(debris.begin(), debris.end());
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

auto RiftScene::TickCombat(float deltaTime, bool canFire) -> void {
    const auto& combat = RiftStore::Get().Combat;
    const bool hunting = _delivery && !_dying;

    _fireCooldown = std::max(0.0f, _fireCooldown - deltaTime);
    if (canFire && _ammo > 0 && _fireCooldown <= 0.0f && _gameIns->Input->GetMouseButton(GLFW_MOUSE_BUTTON_LEFT)) {
        _fireCooldown = combat.FireCooldown;
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
                ,TracerComponent { .Velocity = _camera->GetFront() * combat.TracerSpeed, .Life = combat.TracerLife, .Spent = false }
            );
        }
    }

    std::vector<entt::entity> doomed;
    int respawns = 0;
    auto tracers = _registry.view<TransformComponent, TracerComponent>();
    for (auto entity : tracers) {
        auto& transform = tracers.get<TransformComponent>(entity);
        auto& tracer = tracers.get<TracerComponent>(entity);
        transform.Position += tracer.Velocity * deltaTime;
        tracer.Life -= deltaTime;

        bool hit = false;
        auto aliens = _registry.view<TransformComponent, AlienComponent>();
        for (auto alienEntity : aliens) {
            const auto& alienTransform = aliens.get<TransformComponent>(alienEntity);
            const auto& alien = aliens.get<AlienComponent>(alienEntity);
            if (glm::length(alienTransform.Position - transform.Position) <= alien.HitRadius + combat.TracerHitRadius) {
                const glm::vec3 boomAt = alienTransform.Position;
                doomed.push_back(alienEntity);
                doomed.push_back(entity);
                ++_kills;
                _ammo = std::min(_ammo + combat.AmmoPerKill, _ammoMax);
                SpawnBurst(boomAt, glm::vec3(0.35f, 1.6f, 0.4f), 14, 10.0f);
                ++respawns;
                hit = true;
                break;
            }
        }
        if (!hit && tracer.Life <= 0.0f) {
            doomed.push_back(entity);
        }
    }

    if (hunting) {
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
            if (distance <= combat.CatchRadius) {
                StartDeath();
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

    if (hunting) {
        for (int i = 0; i < respawns; ++i) SpawnAlienAround(_camera->Position);
    }
}

// shop /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

auto RiftScene::ApplyLoadout() -> void {
    const auto& shop = RiftStore::Get().Shop;
    auto& combat = RiftStore::Get().Combat;
    _ammoMax = combat.StartingAmmo + _gunLevel * 4;
    _ammo = std::min(_ammo, _ammoMax);
    const float boost = _boostLeft > 0.0f ? shop.BoostMul : 1.0f;
    _shipCameraController->SpeedMul = (1.0f + 0.15f * static_cast<float>(_speedLevel)) * boost;
    combat.FireCooldown = 0.11f * std::pow(0.84f, static_cast<float>(_gunLevel));
    combat.TracerSpeed = 260.0f + 40.0f * static_cast<float>(_gunLevel);
}

auto RiftScene::TryBuy(int& level, int baseCost) -> bool {
    if (!_delivery) return false;
    const int cost = baseCost * (level + 1);
    if (_delivery->GetCredits() < cost || level >= RiftStore::Get().Shop.MaxLevel) return false;
    _delivery->SetCredits(_delivery->GetCredits() - cost);
    ++level;
    _toastLeft = 1.0f;
    ApplyLoadout();
    return true;
}

auto RiftScene::TickShop(float deltaTime) -> void {
    const auto& shop = RiftStore::Get().Shop;
    _toastLeft = std::max(0.0f, _toastLeft - deltaTime);
    _boostLeft = std::max(0.0f, _boostLeft - deltaTime);

    // approaching the contract target lights the screen edge and grants a short speed boost
    float glow = 0.0f;
    if (_delivery && !_dying && _delivery->HasContract() && !_delivery->CanComplete()) {
        const float dist = glm::length(_delivery->GetTargetPosition(_camera->Position) - _camera->Position);
        if (dist < shop.ApproachGlowRadius) {
            glow = 1.0f - dist / shop.ApproachGlowRadius;
            if (_boostLeft <= 0.0f) _boostLeft = shop.BoostSeconds;
        }
    }
    if (_boostLeft > 0.0f) {
        glow = std::max(glow, 0.35f + 0.65f * (_boostLeft / shop.BoostSeconds));
    }
    _posterizeMaterial->SetFloat1("Glow", glow);
    ApplyLoadout();
}
