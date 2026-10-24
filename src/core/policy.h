#pragma once

#include <string>
#include <vector>

namespace loadout::policy {

// True when the helper may run this exact operation. The first matching
// allow-list entry decides; anything not listed is refused. expectedUser is the
// name of the pkexec caller; user-scoped operations must target that account.
bool validate(const std::string &program, const std::vector<std::string> &args,
              std::string &error, const std::string &expectedUser = {});

} // namespace loadout::policy
