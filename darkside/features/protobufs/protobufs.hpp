#include "../../darkside.hpp"

class c_protobuf {
	CSubtickMoveStep* subtick_move_step = nullptr;
	CSGOInputHistoryEntryPB* input_history_entry = nullptr;

	CSubtickMoveStep* create_new_subtick_move_step( google::protobuf::repeated_ptr_field_t<CSubtickMoveStep>* rept_ptr, void* arena );
	CSGOInputHistoryEntryPB* create_new_input_history( google::protobuf::repeated_ptr_field_t<CSGOInputHistoryEntryPB>* rept_ptr, void* arena );
public:
	CSubtickMoveStep* add_subtick_move_step( c_user_cmd* user_cmd );
	void sort_subtick_move_steps( c_user_cmd* user_cmd );
	bool update_move_crc( c_user_cmd* user_cmd );
	CSGOInputHistoryEntryPB* add_input_history( c_user_cmd* user_cmd );
};

inline const auto g_protobuf = std::make_unique<c_protobuf>( );
