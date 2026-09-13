#pragma once

#include "void_types.hpp"
#include "void_draw.hpp"
#include <string>

namespace VoidGUI {

    class Context;

    struct PlayerBarData {
        std::string username = "Danabro_20115";
        int health = 85;
        int max_health = 100;
        int armor = 65;
        std::string weapon = "AK-47 [Asiimov]";
        float distance = 142.5f;
        bool is_enemy = true;
    };

    class PlayerBar {
    public:
        static void Render(Context* ctx, Vec2* pos, bool* open = nullptr);
        static PlayerBarData& GetData();

    private:
        static PlayerBarData s_data;
    };

}
