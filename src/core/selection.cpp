#include "core/selection.h"

#include <utility>

namespace loadout {

bool Selection::get(const std::string &key) const
{
    const auto found = m_values.find(key);
    return found != m_values.end() && found->second;
}

void Selection::set(const std::string &key, const bool value)
{
    if (value)
        m_values[key] = true;
    else
        m_values.erase(key);
}

const std::string &Selection::choice(const std::string &group) const
{
    static const std::string none{"none"};
    const auto found = m_choices.find(group);
    return found == m_choices.end() ? none : found->second;
}

void Selection::setChoice(const std::string &group, std::string value)
{
    m_choices[group] = std::move(value);
}

std::vector<std::string> Selection::selectedKeys() const
{
    std::vector<std::string> keys;
    keys.reserve(m_values.size());
    for (const auto &[key, value] : m_values)
        if (value)
            keys.push_back(key);
    return keys;
}

bool Selection::empty() const
{
    return m_values.empty();
}

} // namespace loadout
