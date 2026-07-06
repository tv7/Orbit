// GogStore — wraps gog_games behind IStore. DRM-free, no account switching:
// accounts() is empty and launch() runs the game (via GOG Galaxy's runGame when
// present, else the exe directly). The `offline` option is ignored.

#include "store.h"

#include "../gog_games.h"

namespace ss {

namespace {

class GogStore : public IStore {
public:
    Store kind() const override { return Store::Gog; }

    std::vector<Game> scan() override { return gog::installedGames(); }

    PlayResult launch(const Game& game, const LaunchOptions& /*opts*/,
                      const Notify& notify,
                      const std::function<bool()>& shouldCancel) override {
        if (game.launchId.empty())
            return PlayResult::fail("This GOG game has no launch id.");
        if (shouldCancel && shouldCancel())
            return PlayResult::fail("Cancelled.");
        return gog::launch(game.launchId, notify);
    }
};

}  // namespace

std::unique_ptr<IStore> makeGogStore() { return std::make_unique<GogStore>(); }

}  // namespace ss
