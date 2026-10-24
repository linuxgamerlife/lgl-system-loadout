#pragma once

#include <functional>
#include <string>
#include <vector>

namespace loadout::helper {

// Runs one validated command, streaming merged stdout and stderr line by line.
// The pump callback lets the server keep reading the client socket while the
// command runs, so cancellation is noticed promptly.
class Executor final {
public:
    using LineSink = std::function<void(const std::string &)>;
    using CancelCheck = std::function<bool()>;
    using Pump = std::function<void()>;

    int run(const std::vector<std::string> &argv, const LineSink &onLine,
            const CancelCheck &cancelled, const Pump &pump);
};

} // namespace loadout::helper
