#include "System/PauseState.h"
#include <stdexcept>

static void check(bool condition)
{
    if (!condition)
        throw std::runtime_error("Pause causes must remain independent");
}

int main()
{
    R2NES::System::PauseState state;
    check(!state.paused());
    state.setManual(true);
    state.setMenu(true);
    state.setMinimized(true);
    state.setMenu(false);
    state.setMinimized(false);
    check(state.paused() && state.manual());
    state.setManual(false);
    check(!state.paused());
    state.setMenu(true);
    state.setManual(true);
    state.setManual(false);
    check(state.paused());
    state.setMinimized(true);
    state.setMenu(false);
    check(state.paused());
    state.setMinimized(false);
    check(!state.paused());
}