#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static Uint32 SDLCALL pushWakeup(void *, SDL_TimerID, Uint32)
{
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    event.user.code = 71;
    SDL_PushEvent(&event);
    return 0;
}

static void checkEventPreserved()
{
    SDL_Event event{};
    // PollEvent pode encerrar um ciclo anterior de polling antes do evento.
    bool found = false;
    for (int attempt = 0; attempt < 3 && !found; ++attempt)
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_USER && event.user.code == 71)
                found = true;
    check(found, "Wait consumed the event instead of leaving it for pollEvents");
}

int main(int, char **)
{
    try
    {
        check(SDL_Init(SDL_INIT_EVENTS), "SDL init failed");
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        const Uint64 start = SDL_GetPerformanceCounter();
        check(SDL_WaitEventTimeout(nullptr, 20) == 0, "Unexpected event during idle wait");
        const double idleMs = 1000.0 * (SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency();
        check(idleMs >= 15.0, "Idle wait returned too early");

        pushWakeup(nullptr, 0, 0);
        check(SDL_WaitEventTimeout(nullptr, 250) == 1, "Pending event did not wake wait");
        checkEventPreserved();

        check(SDL_AddTimer(5, pushWakeup, nullptr) != 0, "Could not schedule wakeup");
        check(SDL_WaitEventTimeout(nullptr, 250) == 1, "New event did not interrupt wait");
        checkEventPreserved();
        SDL_Quit();
        std::cout << "Event wait tests passed (timeout, pending event, asynchronous wakeup)\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        SDL_Quit();
        std::cerr << error.what() << '\n';
        return 1;
    }
}