#pragma once

namespace Audio
{
    void begin();
    void update();
    void tick(bool high);
    void setEnabled(bool enabled);
    bool enabled();
}
