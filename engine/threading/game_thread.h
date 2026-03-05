#pragma once
#include <memory>
#include <atomic>
#include <engine/game/game.h>

class GameCache
{
public:
	GameCache()
	{
		game = std::make_shared<Game>();
		update_instance = std::make_shared<Game>(*game);
		published_instance = std::make_shared<Game>(*game);
		pub_game.store(published_instance);
	}

	auto tick() noexcept -> void;
	auto stop() noexcept -> void { running.store(false, std::memory_order_release); }

	[[nodiscard]] auto get_game_copy() const -> std::shared_ptr<Game> {
		return pub_game.load();
	}

private:
	std::shared_ptr<Game> game;
	std::shared_ptr<Game> update_instance;
	std::shared_ptr<Game> published_instance;
	std::atomic<std::shared_ptr<Game>> pub_game;
	std::atomic<bool> running{ true };
};
