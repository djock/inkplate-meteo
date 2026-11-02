#include "Scene.h"
#include "WeatherData.h"
#include <math.h>

namespace
{
    // Cheap deterministic hash so every particle has a stable "random" position.
    uint32_t hashOf(uint32_t i, uint32_t salt)
    {
        uint32_t h = i * 2654435761u ^ (salt * 40503u + 0x9E3779B9u);
        h ^= h >> 15;
        h *= 2246822519u;
        h ^= h >> 13;
        return h;
    }

    void thickLine(Inkplate &d, int x0, int y0, int x1, int y1, uint16_t ink, int t)
    {
        for (int i = 0; i < t; i++)
        {
            d.drawLine(x0 + i, y0, x1 + i, y1, ink);
        }
    }

    // k = scale factor relative to the original 244 px wide design.
    void drawSun(Inkplate &d, int cx, int cy, int r, float k, uint16_t ink, uint16_t paper)
    {
        int ring = 6 * k, core = 12 * k;
        d.fillCircle(cx, cy, r, ink);
        d.fillCircle(cx, cy, r - ring, paper);
        d.fillCircle(cx, cy, r - core, ink);
        const int rays = 12;
        for (int i = 0; i < rays; i++)
        {
            float a = i * 2.0f * M_PI / rays;
            int inner = r + 10 * k;
            int outer = r + ((i % 2 == 0) ? 30 : 20) * k;
            thickLine(d, cx + inner * cosf(a), cy + inner * sinf(a), cx + outer * cosf(a), cy + outer * sinf(a), ink, 5 + (int)k);
        }
    }

    void drawMoon(Inkplate &d, int cx, int cy, int r, uint16_t ink, uint16_t paper)
    {
        d.fillCircle(cx, cy, r, ink);
        d.fillCircle(cx + r / 2 + 4, cy - r / 4, r - 2, paper);
    }

    void drawStars(Inkplate &d, int count, float k, uint16_t ink, int ax, int ay, int aw, int ah)
    {
        for (int i = 0; i < count; i++)
        {
            int sx = ax + 8 + hashOf(i, 11) % (aw - 16);
            int sy = ay + 8 + hashOf(i, 13) % (ah - 16);
            int s = ((i % 3 == 0) ? 5 : 3) * k;
            thickLine(d, sx - s, sy, sx + s, sy, ink, 3 + (int)(k / 2));
            for (int t = 0; t < 3 + (int)(k / 2); t++)
            {
                d.drawLine(sx + t, sy - s, sx + t, sy + s, ink);
            }
        }
    }

    // Outlined cloud with its base at (x, y); s is the unit radius, the cloud is about 5s wide.
    void drawCloud(Inkplate &d, int x, int y, int s, int t, uint16_t ink, uint16_t paper)
    {
        d.fillCircle(x + s, y - s, s, ink);
        d.fillCircle(x + 2 * s + s / 2, y - s - s / 2, s + s / 3, ink);
        d.fillCircle(x + 4 * s, y - s, s, ink);
        d.fillRect(x + s, y - 2 * s, 3 * s, 2 * s, ink);
        if (t <= 0)
        {
            return; // solid cloud
        }
        d.fillCircle(x + s, y - s, s - t, paper);
        d.fillCircle(x + 2 * s + s / 2, y - s - s / 2, s + s / 3 - t, paper);
        d.fillCircle(x + 4 * s, y - s, s - t, paper);
        d.fillRect(x + s, y - 2 * s + t, 3 * s, 2 * s - 2 * t, paper);
    }

    void drawRainDrops(Inkplate &d, int count, float k, uint16_t ink, int ax, int ay, int aw, int ah)
    {
        int len = 15 * k, w = 3 + (int)k;
        if (len < 20)
        {
            len = 20; // keep drops readable in a small picture
        }
        for (int i = 0; i < count; i++)
        {
            int dx = ax + 10 + hashOf(i, 1) % (aw - 20);
            int dy = ay + 8 + hashOf(i, 2) % (uint32_t)(ah - 30 * k);
            for (int t = 0; t < w; t++)
            {
                d.drawLine(dx + t, dy, dx - 3 * k + t, dy + len, ink);
            }
        }
    }

    void drawSnowFlakes(Inkplate &d, int count, float k, uint16_t ink, int ax, int ay, int aw, int ah)
    {
        for (int i = 0; i < count; i++)
        {
            int dx = ax + 12 + hashOf(i, 3) % (aw - 24);
            int dy = ay + 12 + hashOf(i, 4) % (ah - 24);
            int r = ((i % 3 == 0) ? 5 : 3) * k;
            for (int t = 0; t < 3 + (int)(k / 2); t++)
            {
                d.drawLine(dx - r, dy + t, dx + r, dy + t, ink);
                d.drawLine(dx + t, dy - r, dx + t, dy + r, ink);
                d.drawLine(dx - r + 1, dy - r + 1 + t, dx + r - 1, dy + r - 1 + t, ink);
                d.drawLine(dx - r + 1, dy + r - 1 + t, dx + r - 1, dy - r + 1 + t, ink);
            }
        }
    }

    void drawBolt(Inkplate &d, int x, int y, int s, uint16_t ink)
    {
        d.fillTriangle(x + 6 * s, y, x, y + 8 * s, x + 4 * s, y + 8 * s, ink);
        d.fillTriangle(x + 4 * s, y + 8 * s, x + 8 * s, y + 8 * s, x + 2 * s, y + 18 * s, ink);
    }

    // Sun by day, moon (and stars) by night.
    void drawCelestial(Inkplate &d, bool night, int cx, int cy, int r, float k, uint16_t ink, uint16_t paper)
    {
        if (night)
        {
            drawMoon(d, cx, cy, r, ink, paper);
        }
        else
        {
            drawSun(d, cx, cy, r, k, ink, paper);
        }
    }
}

SceneKind sceneForWeather(int code)
{
    if (isStormCode(code))
        return SceneKind::Storm;
    if (isSnowCode(code))
        return SceneKind::Snow;
    if (isRainCode(code))
        return SceneKind::Rain;
    if (code == 45 || code == 48)
        return SceneKind::Fog;
    if (code == 3)
        return SceneKind::Cloudy;
    if (code == 1 || code == 2)
        return SceneKind::PartlyCloudy;
    return SceneKind::Clear;
}

void drawBackdrop(Inkplate &d, SceneKind kind, bool night, int x, int y, int w, int h)
{
    // Full-screen picture: always dark drawing on a white background (night only swaps the sun for moon and stars).
    // The drawing stays inside an inset (the panel bezel hides the edges).
    uint16_t ink = BLACK;
    uint16_t paper = WHITE;
    d.fillRect(x, y, w, h, paper);

    const int inset = 28;
    int ix = x + inset, iy = y + inset, iw = w - 2 * inset, ih = h - 2 * inset - 10;
    float k = iw / 244.0f;
    float dens = iw / 544.0f; // fewer drops and stars in a small picture
    auto N = [&](int n, int minV) { int v = (int)(n * dens); return v < minV ? minV : v; };
    auto X = [&](float v) { return ix + (int)(v * k); };
    auto Y = [&](int pct) { return iy + ih * pct / 100; };
    int t = 0; // cloud outline width; 0 = solid cloud
    int cloudTop = Y(40);

    switch (kind)
    {
    case SceneKind::Clear:
        if (night)
        {
            drawStars(d, N(24, 8), k * 0.8f, ink, ix, iy, iw, ih);
        }
        drawCelestial(d, night, ix + iw * 55 / 100, iy + ih * 45 / 100, 50 * k, k, ink, paper);
        break;

    case SceneKind::PartlyCloudy:
        if (night)
        {
            drawStars(d, N(16, 8), k * 0.8f, ink, ix, iy, iw, ih);
        }
        drawCelestial(d, night, ix + iw * 62 / 100, iy + ih * 34 / 100, 40 * k, k, ink, paper);
        drawCloud(d, X(0), Y(82), 28 * k, t, ink, paper);
        drawCloud(d, X(144), Y(98), 16 * k, t, ink, paper);
        break;

    case SceneKind::Cloudy:
        drawCloud(d, X(79), Y(36), 30 * k, t, ink, paper);
        drawCloud(d, X(0), Y(66), 26 * k, t, ink, paper);
        drawCloud(d, X(119), Y(92), 22 * k, t, ink, paper);
        break;

    case SceneKind::Fog:
        for (int i = 0; i < 6; i++)
        {
            int ly = iy + 24 + i * (ih - 56) / 5;
            int len = iw * 2 / 3 - (i % 2) * 30;
            int lx = ix + 14 + ((i % 2) ? 30 : 0);
            d.fillRoundRect(lx, ly, len, 6 * k, 3 * k, ink);
        }
        break;

    case SceneKind::Rain:
        drawRainDrops(d, N(40, 16), k, ink, ix, iy, iw, ih);
        drawCloud(d, X(74), cloudTop, 30 * k, t, ink, paper);
        drawCloud(d, X(0), cloudTop + 44 * k, 20 * k, t, ink, paper);
        break;

    case SceneKind::Snow:
        drawSnowFlakes(d, N(28, 12), k, ink, ix, iy, iw, ih);
        drawCloud(d, X(74), cloudTop, 30 * k, t, ink, paper);
        drawCloud(d, X(0), cloudTop + 44 * k, 20 * k, t, ink, paper);
        break;

    case SceneKind::Storm:
        drawRainDrops(d, N(32, 14), k, ink, ix, iy, iw, ih);
        drawCloud(d, X(74), cloudTop, 30 * k, t, ink, paper);
        drawBolt(d, X(124), cloudTop + 8 * k, 5 * k, ink);
        break;
    }
}
