#pragma once

#include <d3d11.h>

namespace menu_runtime {
    bool initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                    ID3D11RenderTargetView* render_target);
    void set_render_target(ID3D11RenderTargetView* render_target);
    void shutdown();
}
