#include "app/PhaseOneMigrationRegistry.h"

#include "storage/sqlite/migrations/MigrationRunner.h"

namespace fincept {

void PhaseOneMigrationRegistry::register_all() {
    register_migration_v001();
    register_migration_v002();
    register_migration_v003();
    register_migration_v004();
    register_migration_v005();
    register_migration_v006();
    register_migration_v007();
    register_migration_v008();
    register_migration_v009();
    register_migration_v010();
    register_migration_v011();
    register_migration_v012();
    register_migration_v013();
    register_migration_v014();
    register_migration_v015();
    register_migration_v016();
    register_migration_v017();
    register_migration_v018();
    register_migration_v019();
    register_migration_v020();
    register_migration_v021();
    register_migration_v022();
    register_migration_v023();
    register_migration_v024();
    register_migration_v025();
    register_migration_v026();
    register_migration_v027();
    register_migration_v028();
    register_migration_v029();
    register_migration_v030();
    register_migration_v031();
    register_migration_v032();
}

} // namespace fincept
