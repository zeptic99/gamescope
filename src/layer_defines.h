#pragma once

#include <cstdint>
#include <string_view>

namespace GamescopeLayerClient
{
    // GAMESCOPE_LAYER_CLIENT_FLAGS
    namespace Flag {
        static constexpr uint32_t DisableHDR = 1u << 0;
    }
    using Flags = uint32_t;
}