#include "render.hpp"
#include "../menu/menu_resources.h"
#include "../menu/hashes.hpp"
#include "../sdk/includes/imgui/imgui_impl_dx11.h"

namespace {
HMODULE current_module_handle() {
    HMODULE module = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&current_module_handle), &module);
    return module;
}

ImFont* add_embedded_font(int resource_id, float size, ImFontConfig config,
                          const ImWchar* ranges, bool required_slot = true) {
    HMODULE module = current_module_handle();
    HRSRC resource = module ? FindResourceW(module, MAKEINTRESOURCEW(resource_id), MAKEINTRESOURCEW(10)) : nullptr;
    HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
    void* data = loaded ? LockResource(loaded) : nullptr;
    const DWORD data_size = resource ? SizeofResource(module, resource) : 0;

    config.FontDataOwnedByAtlas = false;
    if (data && data_size)
        return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, static_cast<int>(data_size), size, &config, ranges);

    if (!required_slot)
        return nullptr;

    config.MergeMode = false;
    config.SizePixels = size;
    return ImGui::GetIO().Fonts->AddFontDefault(&config);
}
}

void c_render::update_screen_size( ImGuiIO io ){
	const ImVec2 display_size = io.DisplaySize;
	if (display_size.x <= 0.f || display_size.y <= 0.f)
		return;

	m_screen_size = vec3_t( display_size.x, display_size.y );

	m_screen_center = m_screen_size * 0.5;

	m_aspect_ratio = m_screen_size.x / m_screen_size.y;
}

void c_render::initialize( ) {
	auto& io = ImGui::GetIO( );
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	ImGui::StyleColorsDark();
	io.Fonts->Clear();

	static const ImWchar ui_ranges[] = {
		0x0020, 0x00FF,
		0x0100, 0x017F,
		0x0400, 0x052F,
		0
	};
	static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

	ImFontConfig text_config{};
	text_config.OversampleH = 3;
	text_config.PixelSnapH = true;
	text_config.RasterizerMultiply = 1.0f;

	ImFontConfig ui_config = text_config;
	ui_config.OversampleH = 1;

	// Preserve the exact ten-slot font order used by Neverlose Reloaded.
	ImFont* menu_body = add_embedded_font(IDR_FONT_INTER_MEDIUM, 15.0f, ui_config, ui_ranges);

	ImFontConfig icon_config{};
	icon_config.MergeMode = true;
	icon_config.PixelSnapH = true;
	add_embedded_font(IDR_FONT_FA_SOLID, 14.0f, icon_config, icon_ranges, false);

	add_embedded_font(IDR_FONT_SST_BOLD, 30.0f, text_config, ui_ranges);             // slot 1
	add_embedded_font(IDR_FONT_SPACE_GROTESK, 40.0f, text_config, ui_ranges);        // slot 2
	add_embedded_font(IDR_FONT_INTER_MEDIUM, 17.0f, text_config, ui_ranges);          // slot 3
	add_embedded_font(IDR_FONT_INTER_SEMIBOLD, 17.0f, text_config, ui_ranges);        // slot 4
	add_embedded_font(IDR_FONT_MUSEO_BOLD, 17.0f, text_config, ui_ranges);            // slot 5
	add_embedded_font(IDR_FONT_INTER_SEMIBOLD, 12.5f, text_config, ui_ranges);        // slot 6
	add_embedded_font(IDR_FONT_INTER_MEDIUM, 13.0f, ui_config, ui_ranges);            // slot 7

	ImFontConfig caption_icons = icon_config;
	add_embedded_font(IDR_FONT_FA_SOLID, 13.0f, caption_icons, icon_ranges, false);

	add_embedded_font(IDR_FONT_INTER_SEMIBOLD, 22.0f, text_config, ui_ranges);        // slot 8
	add_embedded_font(IDR_FONT_INTER_MEDIUM, 17.0f, text_config, ui_ranges);          // slot 9

	// Keep the existing ESP fonts as explicit handles after the menu's fixed slots.
	ImFontConfig esp_config{};
	esp_config.PixelSnapH = false;
	esp_config.OversampleH = 5;
	esp_config.OversampleV = 5;
	fonts.verdana = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 13.f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
	fonts.verdana_small = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 12.f, &esp_config, io.Fonts->GetGlyphRangesCyrillic());
	ImFontConfig onetap_config = esp_config;
	onetap_config.FontDataOwnedByAtlas = false;
	fonts.onetap_pixel = io.Fonts->AddFontFromMemoryTTF(onetap_pixel_font, sizeof(onetap_pixel_font), 8.f, &onetap_config, io.Fonts->GetGlyphRangesCyrillic());

	io.FontDefault = menu_body ? menu_body : (io.Fonts->Fonts.Size ? io.Fonts->Fonts[0] : nullptr);
	io.Fonts->Build();
	ImGui_ImplDX11_CreateDeviceObjects();

	gui.apply_theme(0);
	gui.apply_style_colors();
}

void c_render::update_background_drawlist( ImDrawList* draw_list ) {
	m_background_draw_list = draw_list;
}

bool c_render::world_to_screen( vec3_t& in, vec3_t& out ) {
	if ( !std::isfinite(in.x) || !std::isfinite(in.y) || !std::isfinite(in.z) ||
		m_screen_size.x <= 0.f || m_screen_size.y <= 0.f )
		return false;

	static auto screen_transofrm = reinterpret_cast<bool( __fastcall* )( vec3_t&, vec3_t& )>( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 89 5C 24 ? 57 48 83 EC ? 48 83 3D ? ? ? ? ? 48 8B DA" ) );

	if ( !screen_transofrm )
		return false;

	bool on_screen = screen_transofrm( in, out );
	if ( on_screen )
		return false;

	const float screen_size_x = m_screen_size.x;
	const float screen_size_y = m_screen_size.y;

	out.x = ( ( out.x + 1.0f ) * 0.5f ) * screen_size_x;
	out.y = screen_size_y - ( ( ( out.y + 1.0f ) * 0.5f ) * screen_size_y );

	return std::isfinite(out.x) && std::isfinite(out.y);
}

void c_render::line( vec3_t start_pos, vec3_t end_pos, c_color color ) {
	m_background_draw_list->AddLine( start_pos.im( ), end_pos.im( ), color.im( ) );
}

void c_render::rect( vec3_t start_pos, vec3_t end_pos, c_color color, float rounding ) {
	m_background_draw_list->AddRectFilled( start_pos.im( ), end_pos.im( ), color.im( ), rounding );
}

void c_render::rect_outline( vec3_t start_pos, vec3_t end_pos, c_color color, float thickness, float rounding ) {
	m_background_draw_list->AddRect( start_pos.im( ), end_pos.im( ), color.im( ), rounding, 0, thickness );
}

void c_render::text( vec3_t txt_pos, c_color color, const int flags, const ImFont* font, const std::string_view& message, const int font_size ) {
	if ( !m_background_draw_list || !font || message.empty() || font_size <= 0 )
		return;
	m_background_draw_list->PushTextureID( font->ContainerAtlas->TexID );
	const char* begin = message.data();
	const char* end = begin + message.size();
	const auto size = font->CalcTextSizeA( font_size, FLT_MAX, 0.0f, begin, end );
	auto pos = ImVec2( txt_pos.x, txt_pos.y );
	auto outline_clr = c_color( 0, 0, 0, color.a * 0.3f );

	if ( flags & font_flags_center )
		pos = ImVec2( txt_pos.x - size.x / 2.0f, txt_pos.y );

	if ( flags & font_flags_dropshadow )
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x + 1, pos.y + 1 ), outline_clr.im( ), begin, end );

	if ( flags & font_flags_outline )
	{
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x + 1, pos.y - 1 ), outline_clr.im( ), begin, end );
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x - 1, pos.y + 1 ), outline_clr.im( ), begin, end );
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x - 1, pos.y - 1 ), outline_clr.im( ), begin, end );
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x + 1, pos.y + 1 ), outline_clr.im( ), begin, end );
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x, pos.y + 1 ), outline_clr.im( ), begin, end );
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x, pos.y - 1 ), outline_clr.im( ), begin, end );
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x + 1, pos.y ), outline_clr.im( ), begin, end );
		m_background_draw_list->AddText( font, font_size, ImVec2( pos.x - 1, pos.y ), outline_clr.im( ), begin, end );
	}

	m_background_draw_list->AddText( font, font_size, pos, color.im( ), begin, end );
	m_background_draw_list->PopTextureID();
}

void c_render::circle( vec3_t center, c_color color, float radius, int segments ) {
	if ( !m_background_draw_list || radius <= 0.f )
		return;
	m_background_draw_list->AddCircle( center.im( ), radius, color.im( ), segments );
}
