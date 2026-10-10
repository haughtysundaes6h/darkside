// C++20. Source movement uses positive left input, with yaw measured in degrees.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace movement_math {
struct input { float forward{}, left{}; };
inline input rotate(input move, float from_yaw, float to_yaw) {
    if (!std::isfinite(move.forward) || !std::isfinite(move.left) ||
        !std::isfinite(from_yaw) || !std::isfinite(to_yaw)) return {};
    const float angle = std::remainder(from_yaw - to_yaw, 360.f) * std::numbers::pi_v<float> / 180.f;
    const float sine = std::sin(angle), cosine = std::cos(angle);
    return {cosine * move.forward - sine * move.left, sine * move.forward + cosine * move.left};
}
inline input bounded(input move) {
    const float scale = (std::max)({1.f, std::abs(move.forward), std::abs(move.left)});
    return {move.forward / scale, move.left / scale};
}
inline input strafe(float yaw, float previous_yaw, input keys, float vx, float vy,
                    int command, float smoothing) {
    if (!std::isfinite(yaw) || !std::isfinite(previous_yaw) || !std::isfinite(vx) || !std::isfinite(vy))
        return {};
    const float speed = std::hypot(vx, vy);
    if (speed <= 0.1f) return {1.f, 0.f};
    const float ideal = std::clamp(std::atan2(15.f, speed) * 180.f / std::numbers::pi_v<float>, 0.f, 45.f);
    const float key_offset = std::hypot(keys.forward, keys.left) > 0.01f
        ? std::atan2(keys.left, keys.forward) * 180.f / std::numbers::pi_v<float> : 0.f;
    float target_yaw = yaw + key_offset;
    const float yaw_delta = std::remainder(yaw - previous_yaw, 360.f);
    float side = yaw_delta > 0.f ? -1.f : 1.f;
    if (std::abs(yaw_delta) <= ideal || std::abs(yaw_delta) >= 30.f) {
        const float velocity_yaw = std::atan2(vy, vx) * 180.f / std::numbers::pi_v<float>;
        const float delta = std::remainder(target_yaw - velocity_yaw, 360.f);
        const float retrack = ideal * std::clamp(smoothing, 0.01f, 1.f) * 3.f;
        if (speed > 15.f && delta > retrack) { target_yaw = velocity_yaw + retrack; side = -1.f; }
        else if (speed > 15.f && delta < -retrack) { target_yaw = velocity_yaw - retrack; side = 1.f; }
        else {
            const float sign = command & 1 ? 1.f : -1.f;
            target_yaw += ideal * sign;
            side = -sign;
        }
    }
    return bounded(rotate({0.f, side}, target_yaw, yaw));
}

class yaw_history {
    const void* m_pawn{};
    int m_command{-1};
    float m_current{}, m_previous{};
public:
    float sample(const void* pawn, int command, float yaw) {
        if (pawn != m_pawn || command < m_command || m_command < 0) {
            m_pawn = pawn; m_command = command; m_current = m_previous = yaw;
        } else if (command != m_command) {
            m_previous = m_current; m_current = yaw; m_command = command;
        } else m_current = yaw;
        return m_previous;
    }
};
}
