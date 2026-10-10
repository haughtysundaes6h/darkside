#pragma once

#include "../schema/schema.hpp"
#include "../../sdk/typedefs/c_handle.hpp"
#include "../../sdk/typedefs/c_strong_handle.hpp"
#include "../../sdk/typedefs/c_utl_vector.hpp"
#include "../../sdk/typedefs/c_utl_memory.hpp"

#include "c_player_camera_service.hpp"
#include "c_model.hpp"

#include "../../sdk/typedefs/matrix_t.hpp"

#include "game_enums.hpp"

class c_econ_item_definition;

class c_skeleton_instace;

class c_game_scene_node {
public:
	SCHEMA( m_origin, vec3_t, "CGameSceneNode", "m_vecOrigin" );
	SCHEMA( m_abs_origin, vec3_t, "CGameSceneNode", "m_vecAbsOrigin" );

	c_skeleton_instace* get_skeleton_instance( ) {
		// Player and weapon scene nodes are CSkeletonInstance objects.
		// Slot 10 no longer returns the skeleton in the current client build.
		return reinterpret_cast<c_skeleton_instace*>( this );
	}

};

struct alignas( 16 ) c_bone_data {
	vec3_t m_pos;
	float m_scale;
	vec4_t m_rot;
};

class c_model_state {
public:
	SCHEMA( m_model, c_strong_handle<c_model>, "CModelState" , "m_hModel" );
	SCHEMA( m_mesh_group_mask, uint64_t, "CModelState", "m_MeshGroupMask" );

	OFFSET( c_bone_data*, get_bone_data, 0x80 );
	OFFSET( int, get_bone_count, 0x8C );
};

class c_skeleton_instace : public c_game_scene_node {
public:
	SCHEMA( m_model_state, c_model_state, "CSkeletonInstance", "m_modelState" );
	SCHEMA( m_hitbox_set, uint8_t, "CSkeletonInstance", "m_nHitboxSet" );

};

class c_entity_identity {
public:
	SCHEMA( m_name, const char*, "CEntityIdentity", "m_name" );
	SCHEMA( m_designer_name, const char*, "CEntityIdentity", "m_designerName" );
	SCHEMA( m_flags, std::uint32_t, "CEntityIdentity", "m_flags" );

	bool is_valid( ) {
		return m_index( ) != INVALID_EHANDLE_INDEX;
	}

	int get_entry_index( ) {
		if ( !is_valid( ) )
			return ENT_ENTRY_MASK;

		return m_index( ) & ENT_ENTRY_MASK;
	}

	int get_serial_number( ) {
		return m_index( ) >> NUM_SERIAL_NUM_SHIFT_BITS;
	}

	bool is_type( const char* name ) {
		return fnv1a::hash_64( name ) == fnv1a::hash_64( m_designer_name( ) );
	}

	OFFSET( int, m_index, 0x10 );
};

class c_entity_instance {
public:
	c_base_handle get_handle( ) {
		c_entity_identity* identity = m_entity( );
		if ( identity == nullptr )
			return c_base_handle( );

		return c_base_handle( identity->get_entry_index( ), identity->get_serial_number( ) - ( identity->m_flags( ) & 1 ) );
	}

	const char* get_entity_class_name( ) {
		auto* identity = m_entity();
		if ( !identity )
			return nullptr;
		const auto entity_class = *reinterpret_cast<std::uintptr_t*>( reinterpret_cast<std::uintptr_t>( identity ) + 0x8 );
		if ( !entity_class )
			return nullptr;
		const auto class_info = *reinterpret_cast<c_schema_class_info**>( entity_class + 0x58 );
		if ( !class_info )
			return nullptr;
		return class_info->get_name();
	}

	bool is_player( ) {
		return is_player_pawn( ) || is_player_controller( );
	}

	bool is_player_pawn( ) {
		const char* class_name = get_entity_class_name();
		return class_name && fnv1a::hash_64( class_name ) == fnv1a::hash_64( "C_CSPlayerPawn" );
	}

	bool is_player_controller( ) {
		const char* class_name = get_entity_class_name();
		return class_name && fnv1a::hash_64( class_name ) == fnv1a::hash_64( "CCSPlayerController" );
	}

	SCHEMA( m_entity, c_entity_identity*, "CEntityInstance", "m_pEntity" );
};

class c_collision_attribute {
public:
	SCHEMA( m_hierarchy_id, std::uint16_t, "VPhysicsCollisionAttribute_t", "m_nHierarchyId" );
};

class c_collision {
public:
	SCHEMA( m_mins, vec3_t, "CCollisionProperty", "m_vecMins" );
	SCHEMA( m_maxs, vec3_t, "CCollisionProperty", "m_vecMaxs" );
	SCHEMA( m_collision_attribute, c_collision_attribute, "CCollisionProperty", "m_collisionAttribute" );

	std::uint16_t get_collision_mask( ) {
		return m_collision_attribute( ).m_hierarchy_id( );
	}

	SCHEMA( m_solid_flags, std::uint8_t, "CCollisionProperty", "m_usSolidFlags" );
	SCHEMA( m_collision_group, std::uint8_t, "CCollisionProperty", "m_CollisionGroup" );
};

class c_base_anim_graph {
public:
	SCHEMA( m_sequence_finished, bool, "CBaseAnimGraphController", "m_bSequenceFinished" );
	SCHEMA( m_sound_sync_time, float, "CBaseAnimGraphController", "m_flSoundSyncTime" );
	SCHEMA( m_active_ik_chain_mask, std::uintptr_t, "CBaseAnimGraphController", "m_nActiveIKChainMask" );
	SCHEMA( m_sequence, int, "CBaseAnimGraphController", "m_hSequence" );
	SCHEMA( m_seq_start_time, int, "CBaseAnimGraphController", "m_flSeqStartTime" );
	SCHEMA( m_seq_fixed_cycle, int, "CBaseAnimGraphController", "m_flSeqFixedCycle" );
	SCHEMA( m_anim_loop_mode, int, "CBaseAnimGraphController", "m_nAnimLoopMode" );
	SCHEMA( m_playback_rate, float, "CBaseAnimGraphController", "m_flPlaybackRate" );
	SCHEMA( m_animation_inputs_changed, bool, "CBaseAnimGraphController", "m_bNetworkedAnimationInputsChanged" );
	SCHEMA( m_sequence_changed, bool, "CBaseAnimGraphController", "m_bNetworkedSequenceChanged" );
	SCHEMA( m_last_update_skipped, bool, "CBaseAnimGraphController", "m_bLastUpdateSkipped" );

	void write_new_sequence( int& sequence ) {
		sequence = m_sequence( );
	}
};

class c_body_component : public c_entity_instance {
public:
	SCHEMA( m_animation_controller, c_base_anim_graph, "CBodyComponentBaseAnimGraph", "m_animationController" );
	c_base_anim_graph* get_base_anim_graph_controller( ) {
		return &m_animation_controller( );
	}
};

class c_base_entity : public c_entity_instance {
public:
	SCHEMA( m_health, int, "C_BaseEntity", "m_iHealth" );
	SCHEMA( m_flags, int, "C_BaseEntity", "m_fFlags" );
	SCHEMA( m_move_type, int, "C_BaseEntity", "m_MoveType" );
	SCHEMA( m_actual_move_type, std::uint8_t, "C_BaseEntity", "m_nActualMoveType" );
	SCHEMA( m_vec_base_velocity, vec3_t, "C_BaseEntity", "m_vecBaseVelocity" );
	SCHEMA( m_vec_velocity, vec3_t, "C_BaseEntity", "m_vecVelocity" );
	SCHEMA( m_vec_abs_velocity, vec3_t, "C_BaseEntity", "m_vecAbsVelocity" );
	SCHEMA( m_collision, c_collision*, "C_BaseEntity", "m_pCollision" );
	SCHEMA( m_owner_entity, c_base_handle, "C_BaseEntity", "m_hOwnerEntity" );
	SCHEMA( m_scene_node, c_game_scene_node*, "C_BaseEntity", "m_pGameSceneNode" );
	SCHEMA( m_team_num, std::uint8_t, "C_BaseEntity", "m_iTeamNum" );
	SCHEMA( m_sim_time, float, "C_BaseEntity", "m_flSimulationTime" );
	SCHEMA( m_body_component, c_body_component*, "C_BaseEntity", "m_CBodyComponent" );

	void set_body_group( ) {
		static auto fn = reinterpret_cast<void( __fastcall* )( void*, int, unsigned int )>( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "85 D2 0F 88 ? ? ? ? 55 56 57" ) );
		fn( this, 0, 1 );
	}

	bool is_weapon( ) {
		return vmt::call_virtual<bool>( this, 160 );
	}

	bool is_view_model( ) {
		return vmt::call_virtual<bool>( this, 242 );
	}

	void set_model( const char* model_name ) {
		// xref: "Failed to create reference econ item for preview model (id %llu)"
		static auto fn = reinterpret_cast<void* ( __fastcall* )( void*, const char* )>( g_opcodes->scan_absolute( g_modules->m_modules.client_dll.get_name( ), "E8 ? ? ? ? 44 88 A6", 0x1 ) );
		fn( this, model_name );
	}

	c_hitboxsets* get_hitbox_set( unsigned int index ) {
		using fn_get_hitbox_set = std::int64_t( __fastcall* )( void*, unsigned int );
		static auto fn = reinterpret_cast<fn_get_hitbox_set>( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 89 5C 24 ? 48 89 74 24 ? 57 48 81 EC ? ? ? ? 8B DA 48 8B F9 E8 ? ? ? ? 48 8B F0 48 85 C0 0F 84" ) );
	
		return reinterpret_cast<c_hitboxsets*>( fn( this, index ) );
	}
};

class c_econ_item_view
{
public:
	SCHEMA( m_defenition_index, uint16_t, "C_EconItemView", "m_iItemDefinitionIndex" );
	SCHEMA( m_item_id, uint64_t, "C_EconItemView", "m_iItemID" );
	SCHEMA( m_item_id_low, uint32_t, "C_EconItemView", "m_iItemIDLow" );
	SCHEMA( m_item_id_high, uint32_t, "C_EconItemView", "m_iItemIDHigh" );
	SCHEMA( m_account_id, uint32_t, "C_EconItemView", "m_iAccountID" );
	SCHEMA( m_initialized, bool, "C_EconItemView", "m_bInitialized" );
	SCHEMA( m_custom_name, const char*, "C_EconItemView", "m_szCustomName" );
	SCHEMA( m_custom_name_override, const char*, "C_EconItemView", "m_szCustomNameOverride" );

	int get_custom_paint_kit( ) {
		return vmt::call_virtual<int>( this, 2 );
	}

	c_econ_item_definition* get_static_data( ) {
		return vmt::call_virtual<c_econ_item_definition*>( this, 13 );
	}
};

class c_attribute_container {
public:
	SCHEMA_ARRAY( m_item, c_econ_item_view, "C_AttributeContainer", "m_Item" );
};

class c_player_weapon_service {
public:
	SCHEMA( m_active_weapon, c_base_handle, "CPlayer_WeaponServices", "m_hActiveWeapon" );
	SCHEMA( m_last_weapon, c_base_handle, "CPlayer_WeaponServices", "m_hLastWeapon" );
	SCHEMA( m_weapon, c_network_utl_vector<c_base_handle>, "CPlayer_WeaponServices", "m_hMyWeapons" );
};

class c_econ_entity : public c_base_entity
{
public:
	SCHEMA_ARRAY( m_attribute_manager, c_attribute_container, "C_EconEntity", "m_AttributeManager" );
	SCHEMA( m_paint_kit, int, "C_EconEntity", "m_nFallbackPaintKit" );
	SCHEMA( m_seed, int, "C_EconEntity", "m_nFallbackSeed" );
	SCHEMA( m_xuid_low, uint32_t, "C_EconEntity", "m_OriginalOwnerXuidLow" );
	SCHEMA( m_xuid_high, uint32_t, "C_EconEntity", "m_OriginalOwnerXuidHigh" );
	SCHEMA( m_wear, float, "C_EconEntity", "m_flFallbackWear" );
	SCHEMA( m_statrack, int, "C_EconEntity", "m_nFallbackStatTrak" );
};

class c_player_movement_service {
public:
	SCHEMA( m_max_speed, float, "CPlayer_MovementServices", "m_flMaxspeed" );
	SCHEMA( m_surface_friction, float, "CPlayer_MovementServices_Humanoid", "m_flSurfaceFriction" );

	void set_prediction_command( c_user_cmd* user_cmd ) {

		using fn_set_prediction_command = void(__fastcall*)(c_player_movement_service*, c_user_cmd*);
		static auto set_prediction_command = reinterpret_cast<fn_set_prediction_command>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "48 89 5C 24 ? 57 48 83 EC ? 48 8B DA E8 ? ? ? ? 48 8B F8 48 85 C0 74"));

		set_prediction_command(this, user_cmd);
	}

	void reset_prediction_command( ) {
		using fn_reset_prediction_command = void(__fastcall*)(c_player_movement_service*);
		static auto reset_prediction_command = reinterpret_cast<fn_reset_prediction_command>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), "48 83 EC ? B9 ? ? ? ? E8 ? ? ? ? 48 C7 05"));

		reset_prediction_command(this);
	}
};

using firing_float_t = float[ 2 ];

class c_cs_weapon_base_v_data
{
public:
	SCHEMA( m_max_clip1, int32_t, "CBasePlayerWeaponVData", "m_iMaxClip1" );
	SCHEMA( m_max_clip2, int32_t, "CBasePlayerWeaponVData", "m_iMaxClip2" );
	SCHEMA( m_default_clip1, int32_t, "CBasePlayerWeaponVData", "m_iDefaultClip1" );
	SCHEMA( m_default_clip2, int32_t, "CBasePlayerWeaponVData", "m_iDefaultClip2" );
	SCHEMA( m_weight, int32_t, "CBasePlayerWeaponVData", "m_iWeight" );
	SCHEMA( m_name, const char*, "CCSWeaponBaseVData", "m_szName" );
	SCHEMA( m_throw_velocity, float, "CCSWeaponBaseVData", "m_flThrowVelocity" );
	SCHEMA( m_cycle_time, firing_float_t, "CCSWeaponBaseVData", "m_flCycleTime" );
	SCHEMA( m_damage, int, "CCSWeaponBaseVData", "m_nDamage" );
	SCHEMA( m_armor_ratio, float, "CCSWeaponBaseVData", "m_flArmorRatio" );
	SCHEMA( m_range, float, "CCSWeaponBaseVData", "m_flRange" );
	SCHEMA( m_range_modifier, float, "CCSWeaponBaseVData", "m_flRangeModifier" );
	SCHEMA( m_penetration, float, "CCSWeaponBaseVData", "m_flPenetration" );
	SCHEMA( m_headshot_multiplier, float, "CCSWeaponBaseVData", "m_flHeadshotMultiplier" );
	SCHEMA( m_spread, float, "CCSWeaponBaseVData", "m_flSpread" );
	SCHEMA( m_max_speed, float, "CCSWeaponBaseVData", "m_flMaxSpeed" );
	SCHEMA( m_bullets, float, "CCSWeaponBaseVData", "m_nNumBullets" );
	SCHEMA( m_weapon_type, int, "CCSWeaponBaseVData", "m_WeaponType" );
};

class c_base_player_weapon : public c_econ_entity
{
public:
	SCHEMA( m_next_primary_attack, int32_t, "C_BasePlayerWeapon", "m_nNextPrimaryAttackTick" );
	SCHEMA( m_next_primary_attack_ratio, float, "C_BasePlayerWeapon", "m_flNextPrimaryAttackTickRatio" );
	SCHEMA( m_next_secondary_attack, int32_t, "C_BasePlayerWeapon", "m_nNextSecondaryAttackTick" );
	SCHEMA( m_next_secondary_attack_ratio, float, "C_BasePlayerWeapon", "m_flNextSecondaryAttackTickRatio" );
	SCHEMA( m_clip1, int32_t, "C_BasePlayerWeapon", "m_iClip1" );
	SCHEMA( m_clip2, int32_t, "C_BasePlayerWeapon", "m_iClip2" );

	SCHEMA( m_in_reload, bool, "C_CSWeaponBase", "m_bInReload" );

	SCHEMA( m_burst_mode, bool, "C_CSWeaponBase", "m_bBurstMode" );
	SCHEMA( m_burst_shots_remaining, int, "C_CSWeaponBaseGun", "m_iBurstShotsRemaining" );

	schema_add_with_offset(m_get_sub_class_id, void*, "C_BaseEntity", "m_nSubclassID", 0x8);

	c_cs_weapon_base_v_data* get_weapon_data()
	{
		return static_cast<c_cs_weapon_base_v_data*>(m_get_sub_class_id());
	}
	float get_max_speed( ) {
		auto* data = get_weapon_data( );
		return data ? data->m_max_speed( ) : 0.f;
	}

	float get_inaccuracy( ) {
		using fn_get_inaccuracy_t = float(__fastcall*)(void*, float*, float*);
		float x = 0.f, y = 0.f;

        static fn_get_inaccuracy_t fn = reinterpret_cast<fn_get_inaccuracy_t>( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 89 5C 24 ? 55 56 57 48 81 EC ? ? ? ? 44 0F 29 84 24"   ) );

        return fn ? fn(this, &x, &y) : 0.f;

    }

	float get_spread( ) {
		using fn_get_spread_t = float( __fastcall* )( void* );
		static fn_get_spread_t fn = reinterpret_cast<fn_get_spread_t>( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "48 63 91 ? ? ? ? 48 8B 81 ? ? ? ? 85 D2 78 ? 48 83 FA 02 73 ? F3 0F 10 84 90 50 07 00 00 C3 F3 0F 10 80 50 07 00 00 C3" ) );

		return fn ? fn( this ) : 0.f;
	}

	void update_accuracy_penality( ) {
		using fn_update_accuracy_penality_t = void( __fastcall* )( void* );
		static fn_update_accuracy_penality_t fn = reinterpret_cast<fn_update_accuracy_penality_t>( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "40 57 41 56 48 83 EC 68 48 8B F9 E8 ? ? ? ? 4C 8B F0 48 85 C0" ) );

		if ( fn )
			fn( this );
	}
};

class c_cs_weapon_base : public c_base_player_weapon {
public:
	SCHEMA( m_zoom_level, int, "C_CSWeaponBaseGun", "m_zoomLevel" );
	SCHEMA( m_burst_mode, bool, "C_CSWeaponBase", "m_bBurstMode" );
};

class c_base_cs_grenade : public c_cs_weapon_base {
public:
	SCHEMA( m_held_by_player, bool, "C_BaseCSGrenade", "m_bIsHeldByPlayer" );
	SCHEMA( m_pin_pulled, bool, "C_BaseCSGrenade", "m_bPinPulled" );
	SCHEMA( m_throw_time, float, "C_BaseCSGrenade", "m_fThrowTime" );
	SCHEMA( m_throw_strength, float, "C_BaseCSGrenade", "m_flThrowStrength" );
};

class c_cs_player_item_service {
public:
	SCHEMA( m_has_helmet, bool, "CCSPlayer_ItemServices", "m_bHasHelmet" );
	SCHEMA( m_has_defuser, bool, "CCSPlayer_ItemServices", "m_bHasDefuser" );
};

class c_planted_c4 : public c_base_entity {
public:
	SCHEMA( m_ticking, bool, "C_PlantedC4", "m_bBombTicking" );
	SCHEMA( m_defused, bool, "C_PlantedC4", "m_bBombDefused" );
	SCHEMA( m_being_defused, bool, "C_PlantedC4", "m_bBeingDefused" );
	SCHEMA( m_blow_time, float, "C_PlantedC4", "m_flC4Blow" );
	SCHEMA( m_timer_length, float, "C_PlantedC4", "m_flTimerLength" );
	SCHEMA( m_defuse_countdown, float, "C_PlantedC4", "m_flDefuseCountDown" );
	SCHEMA( m_defuse_length, float, "C_PlantedC4", "m_flDefuseLength" );
	SCHEMA( m_bomb_site, int, "C_PlantedC4", "m_nBombSite" );
};

class c_observer_services {
public:
	SCHEMA(target,c_base_handle,"CPlayer_ObserverServices","m_hObserverTarget");
	SCHEMA(mode,std::uint8_t,"CPlayer_ObserverServices","m_iObserverMode");
};

class c_cs_player_pawn : public c_base_entity {
public:
	SCHEMA(m_observer_services,c_observer_services*,"C_BasePlayerPawn","m_pObserverServices");
	SCHEMA( m_weapon_services, c_player_weapon_service*, "C_BasePlayerPawn", "m_pWeaponServices" );
	SCHEMA( m_controller, c_base_handle, "C_BasePlayerPawn", "m_hController" );
	SCHEMA( m_econ_glove, c_econ_item_view, "C_CSPlayerPawn", "m_EconGloves" );
	SCHEMA( m_spawn_time_index, float, "C_CSPlayerPawnBase", "m_flLastSpawnTimeIndex" );
	SCHEMA( m_need_to_reapply_glove, bool, "C_CSPlayerPawn", "m_bNeedToReApplyGloves" );
	SCHEMA( m_armor_value, int, "C_CSPlayerPawn", "m_ArmorValue" );
	SCHEMA( m_movement_services, c_player_movement_service*, "C_BasePlayerPawn", "m_pMovementServices" );
	SCHEMA( m_camera_services, c_player_camera_service*, "C_BasePlayerPawn", "m_pCameraServices" );
	SCHEMA( m_item_services, c_cs_player_item_service*, "C_BasePlayerPawn", "m_pItemServices" );
	SCHEMA( m_scoped, bool, "C_CSPlayerPawn", "m_bIsScoped" );
	SCHEMA( m_view_offset, vec3_t, "C_BaseModelEntity", "m_vecViewOffset" );

	vec3_t get_eye_pos( ) {
		auto* scene_node = m_scene_node( );
		return scene_node ? scene_node->m_abs_origin( ) + m_view_offset( ) : vec3_t{};
	}

	vec3_t get_bone_position( int index ) {
		if ( index < 0 || index >= 128 )
			return {};

		auto* scene_node = m_scene_node();
		if ( !scene_node )
			return {};
		auto* skeleton = scene_node->get_skeleton_instance();
		if ( !skeleton )
			return {};
		auto& model_state = skeleton->m_model_state();
		if ( index >= model_state.get_bone_count() )
			return {};
		auto* bones = model_state.get_bone_data();
		return bones ? bones[index].m_pos : vec3_t{};
	}

	bool is_alive( ) {
		return m_health( ) > 0;
	}

	bool has_armor( int hitgroup ) {
		if ( hitgroup == 1 )
			return this->m_item_services( )->m_has_helmet( );

		return this->m_armor_value( );
	}

	std::uint32_t get_owner_handle_index( ) {
		std::uint32_t result = -1;
		if ( this && m_collision( ) && !( m_collision( )->m_solid_flags( ) & 4 ) )
			result = this->m_owner_entity( ).get_entry_index( );

		return result;
	}

	c_base_player_weapon* get_active_weapon( ) {
		c_player_weapon_service* m_weapon_services = this->m_weapon_services( );

		if ( !m_weapon_services )
			return nullptr;

		if ( !m_weapon_services->m_active_weapon( ).is_valid( ) )
			return nullptr;

		auto* m_active_weapon = reinterpret_cast< c_base_player_weapon* >(
			g_interfaces->m_entity_system->get_base_entity( m_weapon_services->m_active_weapon( ).get_entry_index( ) )
			);

		if ( m_active_weapon == nullptr )
			return nullptr;

		return m_active_weapon;
	}

	bool is_throwing( ) {
		c_base_player_weapon* active_weapon = this->get_active_weapon( );

		if ( !active_weapon )
			return false;

		c_base_cs_grenade* grenade = reinterpret_cast<c_base_cs_grenade*>( active_weapon );
		c_cs_weapon_base_v_data* weapon_data = active_weapon->get_weapon_data( );

		if ( !grenade || !weapon_data )
			return false;

		if ( weapon_data->m_weapon_type( ) == WEAPONTYPE_GRENADE && grenade->m_throw_time( ) != 0.f )
			return true;

		return false;
	}

	int get_bone_index( const char* name ) {
		static const auto fn = reinterpret_cast< int( __fastcall* )( void*, const char* ) >( g_opcodes->scan( g_modules->m_modules.client_dll.get_name( ), "40 53 48 83 EC ? 48 8B 89 ? ? ? ? 48 8B DA 48 8B 01 FF 50 ? 48 8B C8" ) );
		return fn( this, name );
	}
};
