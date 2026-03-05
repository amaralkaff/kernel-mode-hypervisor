#pragma once
#include <memory>
#include <atomic>
#include <vector>
#include <engine/player/player.h>
#include <engine/threading/game_thread.h>

class PlayerCache
{
public:
    explicit PlayerCache(std::shared_ptr<GameCache> wc)
    {
        world_cache.store(std::move(wc));
        local_player.store(std::make_shared<Player>());
        pub_players.store(std::make_shared<std::vector<std::shared_ptr<Player>>>());
    }

    ~PlayerCache() { stop(); }

    void tick() noexcept;
    void stop() noexcept { running.store(false, std::memory_order_release); }

    [[nodiscard]] std::shared_ptr<std::vector<std::shared_ptr<Player>>> get_players() const
    {
        return pub_players.load();
    }

    [[nodiscard]] std::shared_ptr<Player> get_local_player() const
    {
        return local_player.load();
    }

    [[nodiscard]] std::shared_ptr<GameCache> get_world() const
    {
        return world_cache.load();
    }

private:
    std::atomic<std::shared_ptr<GameCache>> world_cache;
    std::atomic<std::shared_ptr<Player>> local_player;
    std::atomic<std::shared_ptr<std::vector<std::shared_ptr<Player>>>> pub_players;
    std::atomic<bool> running{ true };
};
