#include <bonefish/messages/wamp_event_details.hpp>

#include <stdexcept>

namespace bonefish {

msgpack::object wamp_event_details::marshal(msgpack::zone& zone) const
{
    return msgpack::object(m_details, zone);
}

void wamp_event_details::unmarshal(const msgpack::object& object)
{
    object.convert(m_details);
}

} // namespace bonefish