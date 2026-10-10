// C++20. One tick of descending hull motion; never runs or alters engine simulation.
#pragma once
#include <algorithm>
#include <cmath>
namespace landing_predictor {
struct vector {
    float x{}, y{}, z{};
    vector operator+(vector v) const { return {x+v.x,y+v.y,z+v.z}; }
    vector operator*(float f) const { return {x*f,y*f,z*f}; }
    bool finite() const { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); }
};
struct collision { vector end{}, normal{}; float fraction{1.f}; bool solid{}, valid{true}; };
struct result { bool valid{}, landed{}; float fraction{}; };
template<class Trace>
result forecast(vector origin, vector velocity,
                float gravity, float interval, Trace trace) {
    if (!origin.finite() || !velocity.finite() || !std::isfinite(gravity) ||
        gravity < 0.f || !std::isfinite(interval) || interval <= 0.f || interval > 0.05f) return {};
    if (velocity.z > 0.f) return {true};
    auto previous = origin;
    for (int part=1; part<=4; ++part) {
        const float time = interval * part / 4.f;
        auto target = origin + velocity*time;
        target.z -= gravity*time*time*0.5f;
        const auto hit = trace(previous, target);
        if (!hit.valid || hit.solid || !std::isfinite(hit.fraction)) return {};
        if (hit.fraction < 1.f) {
            if (!hit.normal.finite() || hit.normal.z < 0.7f) return {true};
            // Place the rising edge just after hull contact, within this tick.
            const float fraction = (part-1 + std::clamp(hit.fraction,0.f,1.f)) / 4.f + 0.001f;
            return fraction < 1.f ? result{true,true,fraction} : result{true};
        }
        previous = target;
    }
    return {true};
}
}
