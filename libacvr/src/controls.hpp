// SPDX-License-Identifier: MIT
#pragma once
#include "gun_model.hpp"
#include <array>
#include <string>
#include <vector>

namespace acvr {
enum class RuntimeAction : uint32_t { Laser=0, Recenter, Pause, HandSwitch, Join };
constexpr uint32_t action_bit(RuntimeAction action) { return 1u<<uint32_t(action); }
struct ControllerButtons {
    float trigger=0,grip=0;
    bool primary=false,secondary=false,stick_click=false,menu_chord=false;
};
struct MappedControls {
    std::array<bool,2> trigger{},trigger_press{},reload{},offscreen_reload{};
    std::vector<acvr_axis_input> axes;
    std::vector<acvr_button_input> buttons;
    std::vector<RuntimeAction> actions;
};
class ControlMapper {
public:
    // Resolved v0.1 TOML only. Layering stays with the caller; unknown fields
    // survive in source(), and unavailable declarations never emit game inputs.
    bool prepare(std::string text,const std::vector<acvr_control_desc> &,uint32_t gun_player,uint32_t runtime_actions,std::string &error);
    acvr_result sample(const ControllerButtons (&hands)[2],const bool (&tracked)[2],uint32_t primary_hand,MappedControls &);
    bool can_use_hand(uint32_t primary_hand) const;
    void cancel();
    const std::vector<GunOutputRoute> &outputs() const { return output_routes; }
    const std::vector<std::string> &unavailable() const { return missing; }
    const std::string &source() const { return original; }
    uint32_t primary_hand() const { return selected_hand; } // UINT32_MAX: caller default
    const std::string &model_id() const { return selected_model; }
    uint32_t hand_switch() const { return selected_switch; }
    const std::vector<std::string> &node_references() const { return nodes; }
private:
    enum class Kind { Fire, Reload, Axis, Button, Runtime };
    enum class Control { Trigger, Grip, Primary, Secondary, StickClick, MenuChord, Offscreen };
    struct Binding {
        std::string id;
        Kind kind=Kind::Fire;Control control=Control::Trigger;
        uint32_t semantic=0,player=0,hand=0; // hand 0 slot, 1 right, 2 left, 3 either
        float threshold=.55f;bool invert=false,toggle=false,press=false;
        bool armed=true,previous=false,latched=false;
    };
    std::vector<Binding> bindings;
    std::vector<GunOutputRoute> output_routes;
    std::vector<std::string> missing;
    std::vector<std::string> nodes;
    std::string original;
    std::string selected_model;
    uint32_t selected_hand=UINT32_MAX;
    uint32_t selected_switch=UINT32_MAX;
};
bool read_control_file(const std::string &path,std::string &text,std::string &error) noexcept;
}
