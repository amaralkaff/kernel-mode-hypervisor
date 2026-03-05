#pragma once
#define _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING
#include <engine/threading/player_thread.h>

extern auto player_loop(std::shared_ptr<PlayerCache> cache) -> void;