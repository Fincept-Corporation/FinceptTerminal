#pragma once
// About screen — static info page showing version, license, resources, contact

#include <imgui.h>

namespace fincept::about {

class AboutScreen {
public:
    void render();

private:
    void render_version_info();
    void render_license_panels();
    void render_trademarks();
    void render_resources();
    void render_contact();
};

} // namespace fincept::about
