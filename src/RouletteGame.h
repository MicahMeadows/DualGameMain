#ifndef ROULETTEGAME_H
#define ROULETTEGAME_H

enum class RouletteState {
    RS_UNSTARTED = 0,
    RS_PLAYING = 1,
    RS_GAME_OVER = 2,
    RS_MAX = 3
};

struct RouletteGun {
    int bulletPosition;
    int shotsFired;
    bool dead;
};

struct RouletteGame {
    bool playedIntro;
    bool chosePlayers;
    // int bulletPosition;
    // int shotsFired;
    int playerCount;
    int currentPlayer;
    bool hammerLoaded;
    RouletteGun gun1;
    RouletteGun gun2;
    RouletteGun gun3;
    RouletteGun gun4;
    RouletteState state = RouletteState::RS_UNSTARTED;
};

#endif