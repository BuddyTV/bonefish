#ifndef BONEFISH_MESSAGES_WAMP_PUBLISH_OPTIONS_HPP
#define BONEFISH_MESSAGES_WAMP_PUBLISH_OPTIONS_HPP

#include <msgpack.hpp>
#include <string>
#include <unordered_set>

namespace bonefish {

class wamp_publish_options
{
public:
    wamp_publish_options();
    virtual ~wamp_publish_options();

    msgpack::object marshal(msgpack::zone* zone=nullptr) const;
    void unmarshal(const msgpack::object& options);

    template <typename T>
    T get_option(const std::string& name) const;

    template <typename T>
    T get_option_or(const std::string& name, T default_value) const;

private:
    std::unordered_map<std::string, msgpack::object> m_options;
};

inline wamp_publish_options::wamp_publish_options()
    : m_options()
{
}

inline wamp_publish_options::~wamp_publish_options()
{
}

template <typename T>
T wamp_publish_options::get_option(const std::string& name) const
{
    const auto option_itr = m_options.find(name);
    if (option_itr == m_options.end()) {
        throw std::invalid_argument("invalid option requested");
    }

    return option_itr->second.as<T>();
}

template <typename T>
T wamp_publish_options::get_option_or(const std::string& name, T default_value) const
{
    const auto option_itr = m_options.find(name);
    if (option_itr == m_options.end()) {
        return default_value;
    }

    return option_itr->second.as<T>();
}

} // namespace bonefish

#endif // BONEFISH_MESSAGES_WAMP_PUBLISH_OPTIONS_HPP