#include "Game.h"
#include "BePlatform.h"

int main() {
    BePlatform::MoveWorkingDirectoryToExecutableDir();

    const auto game = new Game();
    const auto result = game->Run();
    delete game;

    if (result != 0) {
        return 1;
    }
    
    return 0;
}
