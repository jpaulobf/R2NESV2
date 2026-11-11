#pragma once

namespace R2NES::System
{
    // Cada origem pode ser alterada sem modificar as demais.
    class PauseState
    {
    public:
        bool paused() const { return manual_ || menu_ || minimized_; }
        bool manual() const { return manual_; }
        void setManual(bool value) { manual_ = value; }
        void setMenu(bool value) { menu_ = value; }
        void setMinimized(bool value) { minimized_ = value; }

    private:
        bool manual_ = false;
        bool menu_ = false;
        bool minimized_ = false;
    };
}