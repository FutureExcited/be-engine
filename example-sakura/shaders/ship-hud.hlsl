/*

@be-material: ship-hud-material {
    ScreenSize: float2 = (1280.0, 720.0)
    AimOffset: float2 = (0.0, 0.0)
    AimRadius: float = 150.0
    PixelSize: float = 4.0
    LineHalf: float = 0.6
    PipHalf: float = 1.0
    BracketOffset: float = 6.0
    BracketArm: float = 3.0
    TickLength: float = 2.0
    AimBoxHalf: float = 1.0
    DashPeriod: float = 2.0
    UiColor: float3 = (0.93, 0.91, 0.84)
    
    TargetPos: float2 = (0.0, 0.0)
    TargetDir: float2 = (0.0, 1.0)
    TargetState: float = 0.0
    TargetRingRadius: float = 5.0
    TargetArrowSize: float = 4.0
    TargetAlpha: float = 1.0
    HorizonDir: float2 = (1.0, 0.0)
    GameOver: float = 0.0
    Kills: float = 0.0
    Ammo: float = 18.0
    AmmoMax: float = 18.0
    Credits: float = 0.0
    Boost: float = 0.0
    Shop: float = 0.0
    SpeedLvl: float = 0.0
    GunLvl: float = 0.0
    StyleLvl: float = 0.0
    PayFlash: float = 0.0
}

@be-shader ship-hud {
    topology triangle-strip
    rasterizer back-solid
    blend disable
    depth disable

    vertex FullscreenVertexKernel
    pixel PS

    bind s0 frame uniform-material
    bind s1 main ship-hud-material

    target s0 HudOutput float4
}

*/

/*========================================================*/
// region @be-auto-boilerplate
#include "uniform-material.hlsl"

struct ship_hud_material {
    float2 ScreenSize;
    float2 AimOffset;
    float AimRadius;
    float PixelSize;
    float LineHalf;
    float PipHalf;
    float BracketOffset;
    float BracketArm;
    float TickLength;
    float AimBoxHalf;
    float DashPeriod;
    float3 UiColor;
    float2 TargetPos;
    float2 TargetDir;
    float TargetState;
    float TargetRingRadius;
    float TargetArrowSize;
    float TargetAlpha;
    float2 HorizonDir;
    float GameOver;
    float Kills;
    float Ammo;
    float AmmoMax;
    float Credits;
    float Boost;
    float Shop;
    float SpeedLvl;
    float GunLvl;
    float StyleLvl;
    float PayFlash;
};

cbuffer CBuffer_0 : register(b0, space0) {
    uniform_material _Frame;
};

cbuffer CBuffer_1 : register(b0, space1) {
    ship_hud_material _Main;
};

struct PixelOutput {
    float4 HudOutput : SV_Target0;
};

// endregion
/*========================================================*/

#include "fullscreen-vertex.hlsl"

float Box(float2 p, float2 lo, float2 hi) {
    return step(lo.x, p.x) * step(p.x, hi.x) * step(lo.y, p.y) * step(p.y, hi.y);
}

float Frame(float2 p, float2 lo, float2 hi, float t) {
    float outer = Box(p, lo, hi);
    float inner = Box(p, lo + t, hi - t);
    return saturate(outer - inner);
}

PixelOutput PS(FullscreenVSOutput input) {
    float ps = _Main.PixelSize;
    float hw = _Main.LineHalf;

    float2 center = _Main.ScreenSize * 0.5;
    float2 aimPos = center + _Main.AimOffset * _Main.AimRadius;

    float2 cell = floor(input.Position.xy / ps);
    float2 c0 = floor(center / ps);
    float2 d = cell - c0;
    float2 ad = abs(d);
    float2 topLeft = float2(-floor(c0.x) + 3.0, -floor(c0.y) + 3.0);
    float2 botLeft = float2(-floor(c0.x) + 3.0, floor(_Main.ScreenSize.y / ps) - floor(c0.y) - 8.0);

    float hit = 0.0;

    if (max(ad.x, ad.y) <= _Main.PipHalf) hit = 1.0;

    float bd = _Main.BracketOffset;
    float arm = _Main.BracketArm;
    bool hArm = abs(ad.y - bd) <= hw && ad.x <= bd + hw && ad.x >= bd - arm;
    bool vArm = abs(ad.x - bd) <= hw && ad.y <= bd + hw && ad.y >= bd - arm;
    if (hArm || vArm) hit = 1.0;

    float aimR = _Main.AimRadius / ps;
    float2 barDir = _Main.HorizonDir;
    float2 barPerp = float2(-barDir.y, barDir.x);
    float barAlong = dot(d, barDir);
    float barAcross = dot(d, barPerp);
    if (abs(abs(barAlong) - aimR) <= _Main.TickLength && abs(barAcross) <= hw + 0.5) hit = 1.0;

    float2 aimD = floor(aimPos / ps) - c0;
    float2 da = abs(d - aimD);
    if (max(da.x, da.y) <= _Main.AimBoxHalf) hit = 1.0;

    float lenA = length(aimD);
    float tproj = saturate(dot(d, aimD) / max(dot(aimD, aimD), 1e-4));
    float2 closest = aimD * tproj;
    float along = tproj * lenA;
    bool inRange = along > bd + hw + 1.0 && along < lenA - (_Main.AimBoxHalf + 1.0);
    bool dashOn = fmod(floor(along / _Main.DashPeriod), 2.0) < 0.5;
    if (length(d - closest) <= hw + 0.5 && inRange && dashOn) hit = 1.0;

    float state = _Main.TargetState;
    if (state > 0.5) {
        float2 tCell = floor(_Main.TargetPos / ps) - c0;
        float2 rel = d - tCell;
        if (state < 1.5) {
            float ring = abs(rel.x) + abs(rel.y);
            if (abs(ring - _Main.TargetRingRadius) <= hw + 0.5) hit = max(hit, _Main.TargetAlpha);
        } else {
            float2 fwd = _Main.TargetDir;
            float2 side = float2(-fwd.y, fwd.x);
            float f = dot(rel, fwd);
            float s = dot(rel, side);
            float sz = _Main.TargetArrowSize;
            float taper = saturate((sz - f) / (sz * 1.5));
            if (f <= sz && f >= -sz * 0.5 && abs(s) <= taper * sz * 0.8) hit = 1.0;
        }
    }

    // ammo magazine: 2 rows of cells, bottom-left
    float ammoMax = max(_Main.AmmoMax, 1.0);
    float ammoCount = min(_Main.Ammo, ammoMax);
    float2 mag0 = botLeft;
    float cols = ceil(ammoMax * 0.5);
    float cellW = 4.0;
    float cellH = 7.0;
    float magW = cols * (cellW + 1.0) + 3.0;
    float magH = 2.0 * (cellH + 1.0) + 3.0;
    hit = max(hit, Frame(d, mag0, mag0 + float2(magW, magH), 1.0));
    for (int i = 0; i < 32; i++) {
        if (i >= int(ammoMax)) break;
        float col = fmod(float(i), cols);
        float row = floor(float(i) / cols);
        float2 c = mag0 + float2(2.0 + col * (cellW + 1.0), 2.0 + row * (cellH + 1.0));
        float filled = step(float(i) + 0.5, ammoCount);
        hit = max(hit, Box(d, c, c + float2(cellW, cellH)) * lerp(0.18, 1.0, filled));
    }

    // credit ticks, top-left
    float cred = min(_Main.Credits, 80.0);
    float2 cred0 = topLeft;
    hit = max(hit, Frame(d, cred0, cred0 + float2(42.0, 6.0), 1.0));
    float slot = floor((d.x - cred0.x - 2.0) / 3.0);
    if (d.y >= cred0.y + 2.0 && d.y <= cred0.y + 4.0 && slot >= 0.0 && slot < cred && fmod(d.x - cred0.x - 2.0, 3.0) <= 1.0) {
        hit = 1.0;
    }

    if (_Main.Boost > 0.01) {
        float2 b0 = cred0 + float2(0.0, 8.0);
        hit = max(hit, Frame(d, b0, b0 + float2(42.0, 4.0), 1.0));
        hit = max(hit, Box(d, b0 + 1.0, b0 + float2(1.0 + 40.0 * saturate(_Main.Boost), 3.0)));
    }

    if (_Main.PayFlash > 0.01) {
        float pulse = 0.4 + 0.6 * _Main.PayFlash;
        if (abs(abs(d.x) - 18.0) <= 1.0 && abs(d.y) < 8.0) hit = max(hit, pulse);
    }

    if (_Main.Shop > 0.5) {
        float2 shop0 = float2(-26.0, -22.0);
        float2 shop1 = shop0 + float2(52.0, 44.0);
        hit = max(hit, Frame(d, shop0, shop1, 1.0));
        hit = max(hit, Box(d, shop0, shop1) * 0.10);
        // title bar
        hit = max(hit, Box(d, shop0, shop0 + float2(52.0, 4.0)));
        for (int rail = 0; rail < 3; rail++) {
            float filled = (rail == 0) ? _Main.SpeedLvl : ((rail == 1) ? _Main.GunLvl : _Main.StyleLvl);
            float2 r0 = shop0 + float2(4.0, 8.0 + rail * 11.0);
            // key pip 1/2/3
            hit = max(hit, Box(d, r0, r0 + float2(4.0, 4.0)));
            hit = max(hit, Frame(d, r0 + float2(7.0, 0.0), r0 + float2(43.0, 6.0), 1.0));
            hit = max(hit, Box(d, r0 + float2(8.0, 1.0), r0 + float2(8.0 + 7.0 * min(filled, 5.0), 5.0)));
        }
    }

    if (_Main.GameOver > 0.5) {
        float pulse = 0.55 + 0.45 * sin(_Main.GameOver * 8.0);
        if (abs(d.x - d.y) <= hw + 1.0 || abs(d.x + d.y) <= hw + 1.0) hit = 1.0;
        if (max(ad.x, ad.y) >= min(_Main.ScreenSize.x, _Main.ScreenSize.y) * 0.22 / ps) {
            hit = max(hit, 0.15 * pulse);
        }
    }

    PixelOutput output;
    output.HudOutput = float4(_Main.UiColor, hit);
    return output;
}
