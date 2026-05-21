#ifndef M_WINDOWSTATE_H
#define M_WINDOWSTATE_H

#include "MWindow/MWindowInit.h"
#include "MWindow/MMonitor.h"

namespace MW
{
    struct MWindowState {
        MWindowID   id;
        MWindowDesc desc;
        MWindowMode mode;
    };
} // namespace MW


#endif