# gem
Gem is a command line tool for coding with Google's gemini
Actions

To compile on linux.

    git clone https://github.com/openconstruct/gem
    cd gem
    cmake -B build
    cmake --build build



run ./gem listmodels the first time to select a model ( 17 is 2.5 pro)


usage:
gem create (filename) "description"
gem edit (filename) "description"
gem explain (filename) tells you what it does.

examples:

    gem create game.html "use inline javascript to create a full featured flappy bird game"
    gem edit game.html "make the bird have less gravity and implement a high score system"
