#pragma once
#include "screens/dashboard/canvas/GridLayout.h"
#include "storage/repositories/BaseRepository.h"

namespace fincept {

class DashboardLayoutRepository : public BaseRepository<screens::GridItem> {
  public:
    static DashboardLayoutRepository& instance();

    /// Load the active layout profile. Returns default template hint on first run.
    Result<screens::GridLayout> load_layout(const QString& profile_name = "default");

    /// Save a layout (upsert profile + replace all widget instances).
    Result<void> save_layout(const screens::GridLayout& layout, const QString& profile_name = "default");

    /// Delete all widget instances for a profile (reset).
    Result<void> clear_layout(const QString& profile_name = "default");

  private:
    DashboardLayoutRepository() = default;
};

} // namespace fincept
