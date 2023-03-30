/**
 *  Copyright (C) 2015 Topology LP
 *  Copyright (C) 2023 Vizio Services
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <bonefish/messages/wamp_abort_details.hpp>

namespace bonefish {

msgpack::object wamp_abort_details::marshal(msgpack::zone& zone) const
{
    return msgpack::object(m_details, zone);
}

void wamp_abort_details::unmarshal(const msgpack::object& object)
{
    object.convert(m_details);
}

} // namespace bonefish
