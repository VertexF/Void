#include "Game/Game.hpp"

int main(int argc, char** argv)
{
    Game game;
    game.init();
    game.loop();
    game.shutdown();

    return 0;
}