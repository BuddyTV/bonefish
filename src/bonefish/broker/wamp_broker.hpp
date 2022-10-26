/**
 *  Copyright (C) 2015 Topology LP
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

#ifndef BONEFISH_BROKER_WAMP_BROKER_HPP
#define BONEFISH_BROKER_WAMP_BROKER_HPP

#include <bonefish/identifiers/wamp_publication_id.hpp>
#include <bonefish/identifiers/wamp_publication_id_generator.hpp>
#include <bonefish/identifiers/wamp_request_id.hpp>
#include <bonefish/identifiers/wamp_session_id.hpp>
#include <bonefish/identifiers/wamp_subscription_id.hpp>
#include <bonefish/identifiers/wamp_subscription_id_generator.hpp>
#include <bonefish/messages/wamp_message_type.hpp>

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace bonefish {

class wamp_broker_subscriptions;
class wamp_broker_topic;
class wamp_publish_message;
class wamp_session;
class wamp_subscribe_message;
class wamp_transport;
class wamp_unsubscribe_message;

class wamp_broker
{
public:
    wamp_broker(const std::string& realm);
    ~wamp_broker();

    void attach_session(const std::shared_ptr<wamp_session>& session);
    void detach_session(const wamp_session_id& id);
    void allow_disclose(bool allow);

    void process_publish_message(const wamp_session_id& session_id,
            wamp_publish_message* publish_message);
    void process_subscribe_message(const wamp_session_id& session_id,
            wamp_subscribe_message* subscribe_message);
    void process_unsubscribe_message(const wamp_session_id& session_id,
            wamp_unsubscribe_message* unsubscribe_message);

private:
    void send_error(const std::unique_ptr<wamp_transport>& transport,
            const wamp_message_type request_type, const wamp_request_id& request_id,
            const std::string& error) const;

private:
    const std::string m_realm;
    wamp_publication_id_generator m_publication_id_generator;
    wamp_subscription_id_generator m_subscription_id_generator;
    std::unordered_map<wamp_session_id, std::shared_ptr<wamp_session>> m_sessions;
    std::unordered_map<wamp_session_id, std::unordered_set<wamp_subscription_id>> m_session_subscriptions;
    std::unordered_map<std::string, std::unique_ptr<wamp_broker_subscriptions>> m_topic_subscriptions;
    std::unordered_map<wamp_subscription_id, std::unique_ptr<wamp_broker_topic>> m_subscription_topics;

    /// Allow or disallow publisher identification
    bool m_disclosure_allowed;
};

} // namespace bonefish

#endif // BONEFISH_BROKER_WAMP_BROKER_HPP
