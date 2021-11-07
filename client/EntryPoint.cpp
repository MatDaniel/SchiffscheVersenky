#include "draw/Game.hpp"
#include "draw/scenes/MenuScene.hpp"

int main(int argc, const char* argv[])
{
    return Game::run<MenuScene>();
}