#ifndef ROULETTEGAME_H
#define ROULETTEGAME_H

enum class RouletteState {
    RS_UNSTARTED = 0,
    RS_PLAYING = 1,
    RS_GAME_OVER = 2,
    RS_MAX = 3
};

struct RouletteGame {
    bool playedIntro;
    int bulletPosition;
    int shotsFired;
    bool hammerLoaded;
    RouletteState state = RouletteState::RS_UNSTARTED;
};

#endif