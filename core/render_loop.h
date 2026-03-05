#include <deps/overlay/overlay.h>
#include <engine/threading/player_thread.h>

namespace core
{
	extern auto render_loop(std::shared_ptr<PlayerCache> p_cache, Overlay overlay) -> void;
}