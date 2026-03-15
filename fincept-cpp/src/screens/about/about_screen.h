#pragma once
// About screen — Bloomberg terminal style info page
// Dense three-panel layout: System Info | License & Legal | Resources & Contact

#include <imgui.h>

namespace fincept::about {

class AboutScreen {
public:
    void render();

private:
    void render_header_bar();
    void render_system_info(float width, float height);
    void render_license_panel(float width, float height);
    void render_resources_panel(float width, float height);
};

} // namespace fincept::about
