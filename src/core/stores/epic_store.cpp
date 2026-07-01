// EpicStore — wraps epic_games behind IStore. No account switching (the Epic
// launcher owns a single signed-in account), so accounts() is empty and launch()
// just fires the com.epicgames.launcher:// URI; the `offline` option is ignored.

#include "store.h"

#include "../epic_games.h"
#include "../platform.h"

namespace ss {

namespace {

class EpicStore : public IStore {
public:
    Store kind() const override { return Store::Epic; }

    std::vector<Game> scan() override { return epic::installedGames(); }

    PlayResult launch(const Game& game, const LaunchOptions& /*opts*/,
                      const Notify& notify,
                      const std::function<bool()>& shouldCancel) override {
        if (game.launchId.empty())
            return PlayResult::fail("This Epic game has no launch id.");
        if (shouldCancel && shouldCancel())
            return PlayResult::fail("Cancelled.");
        if (notify) notify("Launching \"" + game.name + "\" via the Epic launcher…");
        platform::openUri(epic::launchUri(game.launchId));
        return PlayResult::success("Handed \"" + game.name + "\" to the Epic launcher.");
    }
};

}  // namespace

std::unique_ptr<IStore> makeEpicStore() { return std::make_unique<EpicStore>(); }

}  // namespace ss
