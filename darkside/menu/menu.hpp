#pragma once

#include "gui.hpp"
#include <memory>

class c_menu {
public:
    bool m_opened = false;
    bool m_block_attack_until_release = false;

    void init();
    void render();
    void draw();
    void toggle() { m_opened = !m_opened; }
    bool is_opened() const { return m_opened; }
    bool blocks_attacks() const { return m_block_attack_until_release; }
    void on_create_move() { }

    c_gui& ui() { return ::gui; }

private:
    void draw_rage();
    void draw_legit();
    void draw_antiaim();
    void draw_visuals();
    void draw_misc();
    void draw_skins();
    void draw_configs();
};

inline auto g_menu = std::make_unique<c_menu>();
