// CustomStore — wraps custom_games behind IStore. User-authored games, no account
// switching: accounts() is empty and launch() runs the game's exe directly. The
// `offline` option is ignored (there's no Steam session to flag).

#include "store.h"

#include "../custom_games.h"

namespace ss {

namespace {

class CustomStore : public IStore {
public:
    Store kind() const override { return Store::Custom; }

    std::vector<Game> scan() override { return custom::installedGames(); }

    PlayResult launch(const Game& game, const LaunchOptions& /*opts*/,
                      const Notify& notify,
                      const std::function<bool()>& shouldCancel) override {
        if (game.launchId.empty())
            return PlayResult::fail("This game has no launch id.");
        if (shouldCancel && shouldCancel())
            return PlayResult::fail("Cancelled.");
        return custom::launch(game.launchId, notify);
    }
};

}  // namespace

std::unique_ptr<IStore> makeCustomStore() { return std::make_unique<CustomStore>(); }

}  // namespace ss
