#include <bonefish/messages/wamp_publish_options.hpp>

#include <stdexcept>

namespace bonefish {

msgpack::object wamp_publish_options::marshal(msgpack::zone*) const
{
    throw std::logic_error("marshal not implemented");
}

void wamp_publish_options::unmarshal(const msgpack::object& object)
{
    object.convert(m_options);
}

} // namespace bonefish