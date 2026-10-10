// C++20. Command filtering shared by the menu and the final CreateMove boundary.
#pragma once
#include <cstdint>

namespace command_input {
inline constexpr std::uint64_t attack_mask = (1ull << 0) | (1ull << 11) | (1ull << 25);

template<class Command>
bool suppress_attacks(Command& command) {
    bool changed = false;
    const auto clear = [&](auto& value) {
        const auto filtered = value & ~attack_mask;
        changed |= filtered != value;
        value = filtered;
    };
    clear(command.m_button_state.m_button_state);
    clear(command.m_button_state.m_button_state2);
    clear(command.m_button_state.m_button_state3);
    if (command.pb.attack1_start_history_index() != -1) {
        command.pb.clear_attack1_start_history_index();
        changed = true;
    }
    if (command.pb.attack2_start_history_index() != -1) {
        command.pb.clear_attack2_start_history_index();
        changed = true;
    }
    if (!command.pb.has_base())
        return changed;
    auto* base = command.pb.mutable_base();
    if (base->has_buttons_pb()) {
        auto* buttons = base->mutable_buttons_pb();
        const auto first = buttons->buttonstate1() & ~attack_mask;
        const auto second = buttons->buttonstate2() & ~attack_mask;
        const auto third = buttons->buttonstate3() & ~attack_mask;
        changed |= first != buttons->buttonstate1() || second != buttons->buttonstate2() ||
            third != buttons->buttonstate3();
        buttons->set_buttonstate1(first);
        buttons->set_buttonstate2(second);
        buttons->set_buttonstate3(third);
    }
    for (int i = 0; i < base->subtick_moves_size(); ++i) {
        auto* step = base->mutable_subtick_moves(i);
        if (!(step->button() & attack_mask))
            continue;
        step->set_button(step->button() & ~attack_mask);
        if (!step->button())
            step->clear_pressed();
        changed = true;
    }
    return changed;
}
}
