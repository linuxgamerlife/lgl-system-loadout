#pragma once

#include <map>
#include <string>
#include <vector>

namespace loadout {

// The wizard's in-memory choices. Pages read and write this directly, so a
// selection survives moving between steps. Nothing is persisted: a setup
// wizard starts clean every time.
class Selection final {
public:
    bool get(const std::string &key) const;
    void set(const std::string &key, bool value);

    // A single-choice group: "gpu", "file-manager", "terminal". An unset group
    // reads as "none".
    const std::string &choice(const std::string &group) const;
    void setChoice(const std::string &group, std::string value);

    std::vector<std::string> selectedKeys() const;
    bool empty() const;

private:
    std::map<std::string, bool> m_values;
    std::map<std::string, std::string> m_choices;
};

} // namespace loadout
