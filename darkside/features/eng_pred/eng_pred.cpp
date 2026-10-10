// C++20
#include "eng_pred.hpp"
#include "../movement/landing_predictor.hpp"

namespace {
bool hull_trace(c_cs_player_pawn* pawn, vec3_t from, vec3_t to, vec3_t mins, vec3_t maxs, game_trace_t* hit) {
    __try {
        if (!g_interfaces->m_trace) return false;
        trace_filter_t filter(0x2001c3003ull, pawn, nullptr, 4);
        if (!filter.m_vtable) return false;
        ray_t ray{};
        ray.m_mins = mins; ray.m_maxs = maxs; ray.type = 2;
        hit->m_fraction = -1.f;
        g_interfaces->m_trace->trace_shape(&ray, from, to, &filter, hit);
        return std::isfinite(hit->m_fraction) && hit->m_fraction>=0.f && hit->m_fraction<=1.f;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}

void c_eng_pred::capture_movement_prestate(c_cs_player_pawn* pawn, c_user_cmd* cmd, int flags, const vec3_t& velocity) {
    m_movement_prestate = {pawn, cmd ? cmd->m_command_number : -1, flags, velocity, pawn && cmd};
}

bool c_eng_pred::predicted_grounded(c_cs_player_pawn* pawn, c_user_cmd* cmd) const {
    return m_movement_prestate.valid && pawn==m_movement_prestate.pawn && cmd &&
        cmd->m_command_number==m_movement_prestate.command_number &&
        (m_movement_prestate.flags & FL_ONGROUND)!=0;
}



c_eng_pred::landing_state c_eng_pred::landing(c_cs_player_pawn* pawn, c_user_cmd* cmd) const {
    landing_state state;
    if (!pawn || !cmd || !m_movement_prestate.valid || pawn!=m_movement_prestate.pawn ||
        cmd->m_command_number!=m_movement_prestate.command_number) return state;
    state.grounded = (m_movement_prestate.flags & FL_ONGROUND)!=0;
    if (state.grounded) { state.valid=true; return state; }
    auto* node = pawn->m_scene_node();
    auto* collision = pawn->m_collision();
    if (!node || !collision) return state;
    const auto mins=collision->m_mins(), maxs=collision->m_maxs();
    if (!std::isfinite(mins.x) || !std::isfinite(mins.y) || !std::isfinite(mins.z) ||
        !std::isfinite(maxs.x) || !std::isfinite(maxs.y) || !std::isfinite(maxs.z) ||
        maxs.x<=mins.x || maxs.y<=mins.y || maxs.z<=mins.z ||
        maxs.x-mins.x>128.f || maxs.y-mins.y>128.f || maxs.z-mins.z>128.f) return state;
    float gravity=800.f;
    if (auto* var=g_interfaces->m_var->get_by_name("sv_gravity")) {
        const float value=var->get_float();
        if (std::isfinite(value) && value>=0.f && value<=4000.f) gravity=value;
    }
    const auto origin=node->m_abs_origin(), velocity=m_movement_prestate.velocity;
    const auto predicted=landing_predictor::forecast({origin.x,origin.y,origin.z},
        {velocity.x,velocity.y,velocity.z}, gravity, 1.f/64.f,
        [&](landing_predictor::vector a, landing_predictor::vector b) {
            game_trace_t hit{};
            const bool valid=hull_trace(pawn,{a.x,a.y,a.z},{b.x,b.y,b.z},mins,maxs,&hit);
            return landing_predictor::collision{{hit.m_end.x,hit.m_end.y,hit.m_end.z},
                {hit.m_normal.x,hit.m_normal.y,hit.m_normal.z},hit.m_fraction,hit.m_all_solid,valid};
        });
    state.valid=predicted.valid; state.will_land=predicted.landed; state.fraction=predicted.fraction;
    return state;
}

void c_eng_pred::run() {
    m_pred_data = {};
    if (!g_ctx->m_local_pawn || !g_ctx->m_local_controller || !g_ctx->m_user_cmd ||
        !g_ctx->m_local_pawn->is_alive() || !g_interfaces->m_engine->is_in_game()) return;
    m_pred_data.m_tick_base=g_ctx->m_local_controller->m_tick_base();
    m_pred_data.m_shoot_tick=m_pred_data.m_tick_base-1;
    m_pred_data.m_velocity=g_ctx->m_local_pawn->m_vec_abs_velocity();
    m_pred_data.m_eye_pos=g_ctx->m_local_pawn->get_eye_pos();
    if (auto* weapon=g_ctx->m_local_pawn->get_active_weapon()) {
        m_pred_data.m_spread=weapon->get_spread();
        m_pred_data.m_inaccuracy=weapon->get_inaccuracy();
    }
    predict();
}

void c_eng_pred::predict() {
    const auto* info=g_interfaces->m_engine->get_networked_client_info();
    if (!info || !info->m_local_data) return;
    m_pred_data.m_eye_pos=info->m_local_data->m_eye_pos;
    m_pred_data.m_player_tick=info->m_player_tick_count;
    m_pred_data.m_player_tick_fraction=info->m_player_tick_fraction;
    m_pred_data.m_render_tick=info->m_render_tick;
    m_pred_data.m_render_tick_fraction=info->m_render_tick_fraction;
    if (m_pred_data.m_player_tick_fraction>0.99f) ++m_pred_data.m_tick_base;
}

void c_eng_pred::end() {
    // This snapshot path does not borrow engine prediction state.
}
