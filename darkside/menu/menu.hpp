#pragma once

#include "gui.hpp"
#include <memory>

class c_menu {
public:
    void init();
    void render();
    void toggle() { m_opened = !m_opened; }
    bool is_opened() const { return m_opened; }
    
    c_gui& gui() { return m_gui; }
    
private:
    bool m_opened = false;
    c_gui m_gui;
    
    void draw_tabs();
    void draw_rage();
    void draw_legit();
    void draw_antiaim();
    void draw_visuals();
    void draw_misc();
    void draw_skins();
    void draw_configs();
};

inline auto g_menu = std::make_unique<c_menu>();
