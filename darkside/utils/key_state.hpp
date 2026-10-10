// C++20. Each physical press changes a toggle once, regardless of query count.
#pragma once
#include <array>

class key_toggle_state {
    std::array<bool,256> m_down{}, m_toggled{};
public:
    void sample(int key,bool down) {
        if(key<=0||key>0xA5)return;
        if(down&&!m_down[key])m_toggled[key]=!m_toggled[key];
        m_down[key]=down;
    }
    bool toggled(int key) const { return key>0&&key<=0xA5&&m_toggled[key]; }
};
