#ifndef BONEFISH_MESSAGES_WAMP_EVENT_DETAILS_HPP
#define BONEFISH_MESSAGES_WAMP_EVENT_DETAILS_HPP

#include <map>
#include <msgpack.hpp>
#include <string>

namespace bonefish {

class wamp_event_details
{
public:
    wamp_event_details();
    virtual ~wamp_event_details();

    msgpack::object marshal(msgpack::zone& zone) const;
    void unmarshal(const msgpack::object& details);

    template <typename T>
    T get_detail(const std::string& name) const;

    template <typename T>
    T get_detail_or(const std::string& name, const T& default_value) const;

    template <typename T>
    void set_detail(const std::string& name, const T& value);

private:
    msgpack::zone m_zone;
    std::map<std::string, msgpack::object> m_details;
};

inline wamp_event_details::wamp_event_details()
    : m_zone()
    , m_details()
{
}

inline wamp_event_details::~wamp_event_details()
{
}

template <typename T>
T wamp_event_details::get_detail(const std::string& name) const
{
    const auto detail_itr = m_details.find(name);
    if (detail_itr == m_details.end()) {
        throw std::invalid_argument("invalid detail requested: " + name);
    }

    return detail_itr->second.as<T>();
}

template <typename T>
T wamp_event_details::get_detail_or(const std::string& name, const T& default_value) const
{
    const auto detail_itr = m_details.find(name);
    if (detail_itr == m_details.end()) {
        return default_value;
    }

    return detail_itr->second.as<T>();
}

template <typename T>
void wamp_event_details::set_detail(const std::string& name, const T& value)
{
    m_details[name] = msgpack::object(value, m_zone);
}

} // namespace bonefish

#endif // BONEFISH_MESSAGES_WAMP_EVENT_DETAILS_HPP