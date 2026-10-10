#include "interfaces.hpp"
#include "../../darkside.hpp"
#include "../cs2_build.hpp"

c_cs_player_pawn* i_entity_system::get_local_pawn() {
	auto* controller = get_local_controller();
	if (!controller)
		return nullptr;

	const auto pawn_handle = controller->m_pawn();
	if (!pawn_handle.is_valid())
		return nullptr;

	auto* pawn = get_base_entity<c_cs_player_pawn>(pawn_handle.get_entry_index());
	return pawn && pawn->is_player_pawn() ? pawn : nullptr;
}

#define CHECK(name, arg)										\
    if (arg == nullptr) {										\
        LOG_ERROR( xorstr_( "[-] Failed to get: %s" ), name);	\
																\
        return false;										\
    }

bool c_interfaces::initialize() 
{
	const auto client_base = g_modules->m_modules.client_dll.get();
	if (!client_base) return false;
	const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(client_base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(client_base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE ||
		nt->FileHeader.TimeDateStamp != cs2_build::client_timestamp ||
		nt->OptionalHeader.SizeOfImage != cs2_build::client_image_size) {
		char client_path[MAX_PATH]{};
		if (!GetModuleFileNameA(reinterpret_cast<HMODULE>(client_base), client_path, MAX_PATH))
			strcpy_s(client_path, "<path unavailable>");
		LOG_ERROR("Unsupported client.dll build: loaded=%s timestamp=0x%08X image=0x%X expected_timestamp=0x%08X expected_image=0x%X",
			client_path, nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage,
			cs2_build::client_timestamp, cs2_build::client_image_size);
		return false;
	}
	m_client = get_interface<i_client>(&g_modules->m_modules.client_dll, xorstr_("Source2Client002"));
	m_network_client_service = get_interface<i_network_client_service>(&g_modules->m_modules.engine2_dll, xorstr_("NetworkClientService_001"));
	m_schema_system = get_interface<i_schema_system>(&g_modules->m_modules.schemasystem_dll, xorstr_("SchemaSystem_001"));
	m_input_system = get_interface<void>(&g_modules->m_modules.input_system, xorstr_("InputSystemVersion001"));
	m_engine = get_interface<i_engine_client>(&g_modules->m_modules.engine2_dll, xorstr_("Source2EngineToClient001"));
	m_var = get_interface<i_cvar>(&g_modules->m_modules.tier0_dll, xorstr_("VEngineCvar007"));
	m_localize = get_interface<i_localize>(&g_modules->m_modules.localize_dll, xorstr_("Localize_001"));
	m_file_system = get_interface<i_file_system>(&g_modules->m_modules.filesystem_stdio, xorstr_("VFileSystem017"));
	m_scene_system = get_interface<i_scene_system>(&g_modules->m_modules.scenesystem_dll, xorstr_("SceneSystem_002"));
	CHECK("Client", m_client);
	CHECK("Network client service", m_network_client_service);
	CHECK("Schema system", m_schema_system);
	CHECK("Input system", m_input_system);
	CHECK("Engine", m_engine);
	CHECK("Convar", m_var);
	CHECK("Localize", m_localize);
	CHECK("File system", m_file_system);
	CHECK("Scene system", m_scene_system);
	m_light_data_queue = reinterpret_cast<c_light_data_queue**>(
		g_opcodes->scan_absolute(g_modules->m_modules.scenesystem_dll.get_name(),
			"48 8B 05 ? ? ? ? 48 C1 E1 04", 0x3, 0x8));
	CHECK("Light data queue address", m_light_data_queue);

	const char* client_dll = g_modules->m_modules.client_dll.get_name();

	m_global_vars = *reinterpret_cast<i_global_vars**>(client_base + cs2_build::global_vars);
	CHECK(xorstr_("Global Vars"), m_global_vars);

	auto trace_address = g_opcodes->scan_absolute(client_dll, xorstr_("4C 8B 35 ? ? ? ? 24 ? 0C ? 66 0F 7F 45 ? 88 45 ? 48 8B CB 48 8D 05 ? ? ? ? 89 7D ? 48 89 45 ? 89 7D ? C7 45 ? ? ? ? ? 66 C7 45 ? ? ? 44 88 7D"), 0x3);
	CHECK("Trace address", trace_address);
	m_trace = *reinterpret_cast<i_trace**>(trace_address);
	CHECK(xorstr_("Traces"), m_trace);

	m_entity_system = *reinterpret_cast<i_entity_system**>(client_base + cs2_build::game_entity_system);
	CHECK(xorstr_("Entity"), m_entity_system);


	m_csgo_input = reinterpret_cast<i_csgo_input*>(client_base + cs2_build::csgo_input);
	CHECK(xorstr_("Input"), m_csgo_input);

	const auto tier0_base = g_modules->m_modules.tier0_dll.get();
	if (!tier0_base) return false;
	const auto mem_alloc_export = g_opcodes->export_fn(tier0_base, xorstr_("g_pMemAlloc"));
	if (!mem_alloc_export) return false;
	m_mem_alloc = *reinterpret_cast<i_mem_alloc**>(mem_alloc_export);
	CHECK("Mem Alloc", m_mem_alloc);

	m_random_float = reinterpret_cast<decltype(m_random_float)>(g_opcodes->export_fn((std::size_t)g_modules->m_modules.tier0_dll.get(), xorstr_("RandomFloat")));
	CHECK("Random Float", m_random_float);

	m_random_seed = reinterpret_cast<decltype(m_random_seed)>(g_opcodes->export_fn((std::size_t)g_modules->m_modules.tier0_dll.get(), xorstr_("RandomSeed")));
	CHECK("Random Seed", m_random_seed);

	LOG_INFO(xorstr_("[+] Interfaces initialization completed!"));
	return true;
}
