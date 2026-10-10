// C++20. Keep button/view events separate from analog impulses in the game's protobuf.
#pragma once
#include <cmath>
#include <cstdint>

namespace command_movement {
inline constexpr std::uint64_t jump = 1ull << 1;
inline constexpr std::uint64_t duck = 1ull << 2;
inline constexpr std::uint64_t directions = (1ull << 3) | (1ull << 4) | (1ull << 9) | (1ull << 10);

template<class Base, class Allocate>
bool analog(Base& base, float prior_forward, float prior_left, Allocate allocate) {
    if (!std::isfinite(prior_forward) || !std::isfinite(prior_left) ||
        !std::isfinite(base.forwardmove()) || !std::isfinite(base.leftmove())) return false;
    const int count = base.subtick_moves_size();
    if (count < 0 || count > 64) return false;
    decltype(base.mutable_subtick_moves(0)) impulse = nullptr;
    for (int i=0; i<count; ++i) {
        auto* step = base.mutable_subtick_moves(i);
        if (step && !impulse && step->button()==0 && step->when()==0.f) impulse = step;
    }
    if (!impulse) impulse = allocate();
    if (!impulse) return false;
    for (int i=0; i<count; ++i) {
        auto* step = base.mutable_subtick_moves(i);
        if (!step) continue;
        step->clear_analog_forward_delta();
        step->clear_analog_left_delta();
        if (step->button() & directions) {
            step->set_button(step->button() & ~directions);
            if (!step->button()) step->set_pressed(false);
        }
    }
    impulse->set_button(0);
    impulse->set_pressed(false);
    impulse->set_when(0.f);
    impulse->set_analog_forward_delta(base.forwardmove() - prior_forward);
    impulse->set_analog_left_delta(base.leftmove() - prior_left);
    return true;
}

template<class Command, class Allocate>
bool set_jump(Command& command, bool press, float when, Allocate allocate) {
    auto* base = command.pb.mutable_base();
    if (!base || !std::isfinite(when) || when < 0.f || when >= 1.f || base->subtick_moves_size()>64) return false;
    decltype(base->mutable_subtick_moves(0)) event = nullptr;
    for (int i=0; i<base->subtick_moves_size(); ++i) {
        auto* step = base->mutable_subtick_moves(i);
        if (!step || !(step->button() & jump)) continue;
        if (!event && step->button()==jump &&
            step->analog_forward_delta()==0.f && step->analog_left_delta()==0.f &&
            step->pitch_delta()==0.f && step->yaw_delta()==0.f) event = step;
    }
    if (!event) event = allocate();
    if (!event) return false;
    for (int i=0; i<base->subtick_moves_size(); ++i) {
        auto* step = base->mutable_subtick_moves(i);
        if (!step || step==event || !(step->button() & jump)) continue;
        step->set_button(step->button() & ~jump);
        if (!step->button()) step->set_pressed(false);
    }
    event->set_button(jump);
    event->set_pressed(press);
    event->set_when(when);
    event->clear_analog_forward_delta();
    event->clear_analog_left_delta();
    event->clear_pitch_delta();
    event->clear_yaw_delta();
    auto& buttons = command.m_button_state;
    if (press) buttons.m_button_state |= jump;
    else buttons.m_button_state &= ~jump;
    buttons.m_button_state2 |= jump;
    // Current CreateMove's transition table reserves state3 for a press and
    // release within the same tick. This emits one transition only.
    buttons.m_button_state3 &= ~jump;
    return true;
}

// Plague's movement processor (dump RVA 0x4313C0) queues press/release
// pairs at a landing fraction. Keep unrelated subtick inputs intact.
template<class Command, class Allocate>
bool landing_button(Command& command, float fraction, std::uint64_t button, Allocate allocate) {
    auto* base = command.pb.mutable_base();
    if (!base || !button || !std::isfinite(fraction) || fraction <= 0.f || fraction >= 0.999f ||
        base->subtick_moves_size() < 0 || base->subtick_moves_size() > 62) return false;
    using Step = decltype(base->mutable_subtick_moves(0));
    Step press = nullptr, release = nullptr;
    const int count = base->subtick_moves_size();
    for (int i = 0; i < count; ++i) {
        auto* step = base->mutable_subtick_moves(i);
        if (!step || step->button() != button || step->analog_forward_delta() != 0.f ||
            step->analog_left_delta() != 0.f || step->pitch_delta() != 0.f ||
            step->yaw_delta() != 0.f) continue;
        if (!press) press = step;
        else if (!release) release = step;
    }
    if (!press) press = allocate();
    if (!press) return false;
    if (!release) release = allocate();
    if (!release) return false;
    for (int i = 0; i < base->subtick_moves_size(); ++i) {
        auto* step = base->mutable_subtick_moves(i);
        if (!step || step == press || step == release || !(step->button() & button)) continue;
        step->set_button(step->button() & ~button);
        if (!step->button()) step->set_pressed(false);
    }
    const auto set_event = [button](Step step, bool pressed, float when) {
        step->set_button(button);
        step->set_pressed(pressed);
        step->set_when(when);
        step->clear_analog_forward_delta();
        step->clear_analog_left_delta();
        step->clear_pitch_delta();
        step->clear_yaw_delta();
    };
    set_event(press, true, fraction);
    set_event(release, false, fraction + 0.001f < 0.999f ? fraction + 0.001f : 0.999f);
    auto& buttons = command.m_button_state;
    buttons.m_button_state &= ~button;
    buttons.m_button_state2 |= button;
    buttons.m_button_state3 |= button;
    return true;
}

template<class Command, class Allocate>
bool landing_jump(Command& command, float fraction, Allocate allocate) {
    return landing_button(command, fraction, jump, allocate);
}

template<class Command, class Allocate>
bool landing_duck(Command& command, float fraction, Allocate allocate) {
    return landing_button(command, fraction, duck, allocate);
}
}
