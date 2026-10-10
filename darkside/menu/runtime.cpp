#include "runtime.hpp"
#include "blur.hpp"
#include "shader_bg.hpp"
#include "spotify.hpp"
#include "gifbg.hpp"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "WindowsApp.lib")

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

namespace {
Microsoft::WRL::ComPtr<IWICImagingFactory> g_wic;
bool g_started = false;

bool ensure_wic() {
    if (g_wic)
        return true;

    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE)
        return false;

    return SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(g_wic.ReleaseAndGetAddressOf())));
}

bool create_texture_from_decoder(IWICBitmapDecoder* decoder, ID3D11ShaderResourceView** out) {
    if (!decoder || !out || !g_pd3dDevice || !g_pd3dDeviceContext)
        return false;

    using Microsoft::WRL::ComPtr;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
        return false;

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(g_wic->CreateFormatConverter(converter.GetAddressOf())) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom)))
        return false;

    UINT width = 0, height = 0;
    if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0)
        return false;

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    if (FAILED(converter->CopyPixels(nullptr, width * 4,
                                     static_cast<UINT>(pixels.size()), pixels.data())))
        return false;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, nullptr, texture.GetAddressOf())))
        return false;

    g_pd3dDeviceContext->UpdateSubresource(texture.Get(), 0, nullptr, pixels.data(), width * 4, 0);
    if (FAILED(g_pd3dDevice->CreateShaderResourceView(texture.Get(), nullptr, out)))
        return false;

    g_pd3dDeviceContext->GenerateMips(*out);
    return true;
}
}

bool LoadTextureFromMemory(const void* data, size_t size, ID3D11ShaderResourceView** out) {
    if (!ensure_wic() || !data || size == 0 || !out)
        return false;

    Microsoft::WRL::ComPtr<IWICStream> stream;
    if (FAILED(g_wic->CreateStream(stream.GetAddressOf())) ||
        FAILED(stream->InitializeFromMemory(
            static_cast<WICInProcPointer>(const_cast<void*>(data)), static_cast<DWORD>(size))))
        return false;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(g_wic->CreateDecoderFromStream(stream.Get(), nullptr,
                                               WICDecodeMetadataCacheOnDemand,
                                               decoder.GetAddressOf())))
        return false;

    return create_texture_from_decoder(decoder.Get(), out);
}

bool menu_runtime::initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                              ID3D11RenderTargetView* render_target) {
    if (!device || !context || !render_target)
        return false;

    g_pd3dDevice = device;
    g_pd3dDeviceContext = context;
    g_mainRenderTargetView = render_target;
    blur::device = device;
    blur::context = context;

    if (!g_started) {
        spotify::start();
        gifbg::start();
        g_started = true;
    }
    return true;
}

void menu_runtime::set_render_target(ID3D11RenderTargetView* render_target) {
    g_mainRenderTargetView = render_target;
}

void menu_runtime::shutdown() {
    if (g_started) {
        gifbg::stop();
        spotify::stop();
        g_started = false;
    }

    shader_bg::shutdown();
    blur::shutdown();
    g_wic.Reset();

    g_mainRenderTargetView = nullptr;
    g_pd3dDeviceContext = nullptr;
    g_pd3dDevice = nullptr;
}
