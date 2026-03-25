#include "app.h"

#include <SDL3/SDL_main.h>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    App app{};
    app.Run();
    return 0;
}
