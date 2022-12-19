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

#include <bonefish/dealer/wamp_dealer.hpp>
#include <bonefish/dealer/wamp_dealer_invocation.hpp>
#include <bonefish/dealer/wamp_dealer_registration.hpp>
#include <bonefish/messages/wamp_call_message.hpp>
#include <bonefish/messages/wamp_call_options.hpp>
#include <bonefish/messages/wamp_error_message.hpp>
#include <bonefish/messages/wamp_invocation_details.hpp>
#include <bonefish/messages/wamp_invocation_message.hpp>
#include <bonefish/messages/wamp_publish_message.hpp>
#include <bonefish/messages/wamp_register_message.hpp>
#include <bonefish/messages/wamp_registered_message.hpp>
#include <bonefish/messages/wamp_result_details.hpp>
#include <bonefish/messages/wamp_result_message.hpp>
#include <bonefish/messages/wamp_unregister_message.hpp>
#include <bonefish/messages/wamp_unregistered_message.hpp>
#include <bonefish/messages/wamp_yield_message.hpp>
#include <bonefish/messages/wamp_yield_options.hpp>
#include <bonefish/session/wamp_session.hpp>
#include <bonefish/trace/trace.hpp>

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace bonefish {

wamp_dealer::wamp_dealer(boost::asio::io_service& io_service)
    : m_io_service(io_service)
    , m_request_id_generator()
    , m_registration_id_generator()
    , m_sessions()
    , m_session_registrations()
    , m_builtin_procedures()
    , m_registered_procedures()
    , m_procedure_registrations()
    , m_pending_invocations()
    , m_pending_caller_invocations()
    , m_pending_callee_invocations()
{
    register_builtin_procedures();
}

wamp_dealer::~wamp_dealer()
{
}

void wamp_dealer::attach_session(const std::shared_ptr<wamp_session>& session)
{
    assert(session->get_role(wamp_role_type::CALLER) ||
            session->get_role(wamp_role_type::CALLEE));

    BONEFISH_TRACE("attach session: %1%", *session);
    auto result = m_sessions.insert(std::make_pair(session->get_session_id(), session));
    if (!result.second) {
        throw std::logic_error("dealer session already registered");
    }
}

void wamp_dealer::detach_session(const wamp_session_id& session_id)
{
    auto session_itr = m_sessions.find(session_id);
    if (session_itr == m_sessions.end()) {
        throw std::logic_error("dealer session does not exist");
    }

    assert(session_itr->second->get_role(wamp_role_type::CALLER) ||
            session_itr->second->get_role(wamp_role_type::CALLEE));

    BONEFISH_TRACE("detach session: %1%", *session_itr->second);

    // Cleanup all of the procedures that were registered by the session.
    // No messages need to be sent here it is strictly just cleaning up any
    // state left behind by a session.
    BONEFISH_TRACE("cleaning up session registrations");
    auto session_registrations_itr = m_session_registrations.find(session_id);
    if (session_registrations_itr != m_session_registrations.end()) {
        auto& registration_ids = session_registrations_itr->second;
        for (const auto& registration_id : registration_ids) {
            auto registered_procedures_itr =
                    m_registered_procedures.find(registration_id);
            if (registered_procedures_itr == m_registered_procedures.end()) {
                throw std::logic_error("dealer registered procedures out of sync");
            }

            auto procedure_registrations_itr =
                    m_procedure_registrations.find(registered_procedures_itr->second);
            if (procedure_registrations_itr == m_procedure_registrations.end()) {
                throw std::logic_error("dealer procedure registrations out of sync");
            }

            BONEFISH_TRACE("removing registration: %1%, procedure %2%",
                    *session_itr->second % procedure_registrations_itr->first);

            m_procedure_registrations.erase(procedure_registrations_itr);
            m_registered_procedures.erase(registered_procedures_itr);
        }
        m_session_registrations.erase(session_registrations_itr);
    }

    // Cleanup any pending caller invocations associated with the session.
    // Since the session would be the caller we do not have to send any
    // messages here. Just simply cleanup any pending invocations.
    BONEFISH_TRACE("cleaning up pending caller invocations");
    auto pending_caller_invocations_itr = m_pending_caller_invocations.find(session_id);
    if (pending_caller_invocations_itr != m_pending_caller_invocations.end()) {
        for (const auto& request_id : pending_caller_invocations_itr->second) {
            auto pending_invocations_itr = m_pending_invocations.find(request_id);
            if (pending_invocations_itr != m_pending_invocations.end()) {
                const auto& dealer_invocation = pending_invocations_itr->second;
                std::shared_ptr<wamp_session> session = dealer_invocation->get_session();

                BONEFISH_TRACE("cleaning up pending caller invocation: %1%, request_id %2%",
                        *session % request_id);

                m_pending_invocations.erase(pending_invocations_itr);
            }
        }
    }

    // Cleanup any pending callee invocations associated with the session.
    // Since the session would be the callee we need to send an error response
    // to the caller(s) and cleanup the pending invocations.
    BONEFISH_TRACE("cleaning up pending callee invocations");
    auto pending_callee_invocations_itr = m_pending_callee_invocations.find(session_id);
    if (pending_callee_invocations_itr != m_pending_callee_invocations.end()) {
        for (const auto& request_id : pending_callee_invocations_itr->second) {
            auto pending_invocations_itr = m_pending_invocations.find(request_id);
            if (pending_invocations_itr != m_pending_invocations.end()) {
                const auto& dealer_invocation = pending_invocations_itr->second;
                std::shared_ptr<wamp_session> session = dealer_invocation->get_session();
                BONEFISH_TRACE("cleaning up pending callee invocation: %1%, request_id %2%",
                        *session, request_id);

                send_error(session->get_transport(), wamp_message_type::CALL,
                        dealer_invocation->get_request_id(), "wamp.error.callee_session_closed");

                m_pending_invocations.erase(pending_invocations_itr);
            }
        }
    }

    m_sessions.erase(session_itr);
}

void wamp_dealer::process_call_message(const wamp_session_id& session_id,
        wamp_call_message* call_message)
{
    auto session_itr = m_sessions.find(session_id);
    if (session_itr == m_sessions.end()) {
        throw std::logic_error("dealer session does not exist");
    }

    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *call_message);

    // If the session placing the call does not support the caller role
    // than do not allow the call to be processed and send an error.
    if (!session_itr->second->get_role(wamp_role_type::CALLER)) {
        send_error(session_itr->second->get_transport(), call_message->get_type(),
                call_message->get_request_id(), "wamp.error.role_violation");
        return;
    }

    const auto procedure = call_message->get_procedure();
    if (!is_valid_uri(procedure)) {
        send_error(session_itr->second->get_transport(), call_message->get_type(),
                call_message->get_request_id(), "wamp.error.invalid_uri");
        return;
    }

    auto builtin_procedure_itr = m_builtin_procedures.find(procedure);
    if (builtin_procedure_itr != m_builtin_procedures.end() && builtin_procedure_itr->second) {
        builtin_procedure_itr->second(session_id, call_message);
        return;
    }

    auto procedure_registrations_itr = m_procedure_registrations.find(procedure);
    if (procedure_registrations_itr == m_procedure_registrations.end()) {
        send_error(session_itr->second->get_transport(), call_message->get_type(),
                call_message->get_request_id(), "wamp.error.no_such_procedure");
        return;
    }

    std::shared_ptr<wamp_session> session =
            procedure_registrations_itr->second->get_session();

    const wamp_request_id request_id = m_request_id_generator.generate();

    const wamp_registration_id& registration_id =
            procedure_registrations_itr->second->get_registration_id();

    wamp_call_options call_options;
    call_options.unmarshal(call_message->get_options());

    // You can't rely on simply assigning the call options to the invocation
    // details. Some call options may only be applicable to the dealer and
    // not the callee. Likewise, some invocation details may be in addition
    // to whatever is provided in the call options. As a result, we only copy
    // specific options over to the invocation details.
    wamp_invocation_details invocation_details;
    if (call_options.get_option_or("receive_progress", false)) {
        invocation_details.set_detail("receive_progress", true);
    }

    std::unique_ptr<wamp_invocation_message> invocation_message(
            new wamp_invocation_message(call_message->release_zone()));
    invocation_message->set_request_id(request_id);
    invocation_message->set_registration_id(registration_id);
    invocation_message->set_details(invocation_details.marshal(invocation_message->get_zone()));
    invocation_message->set_arguments(call_message->get_arguments());
    invocation_message->set_arguments_kw(call_message->get_arguments_kw());

    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *invocation_message);
    if (!session->get_transport()->send_message(std::move(*invocation_message))) {
        BONEFISH_ERROR("sending invocation message to callee failed: network failure");

        send_error(session_itr->second->get_transport(), call_message->get_type(),
                call_message->get_request_id(), "wamp.error.network_failure");
        return;
    }

    unsigned timeout_ms = call_options.get_option_or<unsigned>("timeout", 0);

    // We only setup the invocation state after sending the message is successful.
    // This saves us from having to cleanup any state if the send fails.
    std::unique_ptr<wamp_dealer_invocation> dealer_invocation(
            new wamp_dealer_invocation(m_io_service));

    dealer_invocation->set_session(session_itr->second);
    dealer_invocation->set_request_id(call_message->get_request_id());
    dealer_invocation->set_timeout(
            std::bind(&wamp_dealer::invocation_timeout_handler, this,
                    request_id, std::placeholders::_1), timeout_ms);

    m_pending_invocations.insert(std::make_pair(request_id, std::move(dealer_invocation)));
    m_pending_callee_invocations[session->get_session_id()].insert(request_id);
    m_pending_caller_invocations[session_id].insert(request_id);
}

void wamp_dealer::process_error_message(const wamp_session_id& session_id,
        wamp_error_message* error_message)
{
    auto session_itr = m_sessions.find(session_id);
    if (session_itr == m_sessions.end()) {
        throw std::logic_error("dealer session does not exist");
    }

    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *error_message);

    const auto request_id = error_message->get_request_id();
    auto pending_invocations_itr = m_pending_invocations.find(request_id);
    if (pending_invocations_itr == m_pending_invocations.end()) {
        // This is a mormal condition. It means that the caller has ended its
        // session after issuing a call. There is nothing to report to the
        // callee in this case so we just silently drop the message.
        BONEFISH_ERROR("unable to find invocation ... dropping error message");
        return;
    }

    const auto& dealer_invocation = pending_invocations_itr->second;

    std::unique_ptr<wamp_error_message> caller_error_message(
            new wamp_error_message(error_message->release_zone()));
    caller_error_message->set_request_type(wamp_message_type::CALL);
    caller_error_message->set_request_id(dealer_invocation->get_request_id());
    caller_error_message->set_details(error_message->get_details());
    caller_error_message->set_error(error_message->get_error());
    caller_error_message->set_arguments(error_message->get_arguments());
    caller_error_message->set_arguments_kw(error_message->get_arguments_kw());

    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *caller_error_message);
    std::shared_ptr<wamp_session> session = dealer_invocation->get_session();
    if (!session->get_transport()->send_message(std::move(*caller_error_message))) {
        // There is no error message to propogate in this case as this error
        // message was initiated by the callee and sending the callee an error
        // message in response to an error message would not make any sense.
        // Besides, the callers session has ended.
        BONEFISH_ERROR("failed to send error message to caller: network failure");
    }

    // The failure to send a message in the event of a network failure
    // will detach the session. When this happens the pending invocations
    // will be cleaned up. So we don't use an iterator here to erase the
    // pending invocation because it may have just been invalidated above.
    m_pending_callee_invocations[session_id].erase(request_id);
    m_pending_caller_invocations[session->get_session_id()].erase(request_id);
    m_pending_invocations.erase(request_id);
}

void wamp_dealer::process_register_message(const wamp_session_id& session_id,
        wamp_register_message* register_message)
{
    auto session_itr = m_sessions.find(session_id);
    if (session_itr == m_sessions.end()) {
        throw std::logic_error("dealer session does not exist");
    }

    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *register_message);

    // If the session registering the procedure does not support the callee
    // role than do not allow the call to be processed and send an error.
    if (!session_itr->second->get_role(wamp_role_type::CALLEE)) {
        send_error(session_itr->second->get_transport(), register_message->get_type(),
                register_message->get_request_id(), "wamp.error.role_violation");
        return;
    }

    const auto procedure = register_message->get_procedure();
    if (!is_valid_uri(procedure)) {
        send_error(session_itr->second->get_transport(), register_message->get_type(),
                register_message->get_request_id(), "wamp.error.invalid_uri");
        return;
    }

    auto procedure_registrations_itr = m_procedure_registrations.find(procedure);
    if (procedure_registrations_itr != m_procedure_registrations.end()) {
        send_error(session_itr->second->get_transport(), register_message->get_type(),
                register_message->get_request_id(), "wamp.error.procedure_already_exists");
        return;
    }

    const wamp_registration_id registration_id = m_registration_id_generator.generate();
    std::unique_ptr<wamp_dealer_registration> dealer_registration(
            new wamp_dealer_registration(session_itr->second, registration_id));
    m_procedure_registrations[procedure] = std::move(dealer_registration);

    m_session_registrations[session_id].insert(registration_id);
    m_registered_procedures[registration_id] = procedure;

    std::unique_ptr<wamp_registered_message> registered_message(
            new wamp_registered_message(register_message->release_zone()));
    registered_message->set_request_id(register_message->get_request_id());
    registered_message->set_registration_id(registration_id);

    // If we fail to send the registered message it is most likely that
    // the underlying network connection has been closed/lost which means
    // that the callee is no longer reachable on this session. So all we
    // do here is trace the fact that this event occured.
    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *registered_message);
    if (!session_itr->second->get_transport()->send_message(std::move(*registered_message))) {
        BONEFISH_ERROR("failed to send registered message to caller: network failure");
    }
}

void wamp_dealer::process_unregister_message(const wamp_session_id& session_id,
        wamp_unregister_message* unregister_message)
{
    auto session_itr = m_sessions.find(session_id);
    if (session_itr == m_sessions.end()) {
        throw std::logic_error("dealer session does not exist");
    }

    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *unregister_message);

    auto session_registrations_itr = m_session_registrations.find(session_id);
    if (session_registrations_itr == m_session_registrations.end()) {
        send_error(session_itr->second->get_transport(), unregister_message->get_type(),
                unregister_message->get_request_id(), "wamp.error.no_such_registration");
        return;
    }

    auto& registrations = session_registrations_itr->second;
    auto registrations_itr = registrations.find(unregister_message->get_registration_id());
    if (registrations_itr == registrations.end()) {
        BONEFISH_ERROR("error: dealer session registration id does not exist");
        send_error(session_itr->second->get_transport(), unregister_message->get_type(),
                unregister_message->get_request_id(), "wamp.error.no_such_registration");
        return;
    }

    auto registered_procedures_itr =
            m_registered_procedures.find(*registrations_itr);
    if (registered_procedures_itr == m_registered_procedures.end()) {
        BONEFISH_ERROR("error: dealer registered procedures out of sync");
        send_error(session_itr->second->get_transport(), unregister_message->get_type(),
                unregister_message->get_request_id(), "wamp.error.no_such_registration");
        return;
    }

    auto procedure_registrations_itr =
            m_procedure_registrations.find(registered_procedures_itr->second);
    if (procedure_registrations_itr == m_procedure_registrations.end()) {
        BONEFISH_ERROR("error: dealer procedure registrations out of sync");
        send_error(session_itr->second->get_transport(), unregister_message->get_type(),
                unregister_message->get_request_id(), "wamp.error.no_such_registration");
        return;
    }

    m_procedure_registrations.erase(procedure_registrations_itr);
    m_registered_procedures.erase(registered_procedures_itr);
    registrations.erase(registrations_itr);

    std::unique_ptr<wamp_unregistered_message> unregistered_message(
            new wamp_unregistered_message(unregister_message->release_zone()));
    unregistered_message->set_request_id(unregister_message->get_request_id());

    // If we fail to send the unregistered message it is most likely that
    // the underlying network connection has been closed/lost which means
    // that the callee is no longer reachable on this session. So all we
    // do here is trace the fact that this event occured.
    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *unregistered_message);
    if (!session_itr->second->get_transport()->send_message(std::move(*unregistered_message))) {
        BONEFISH_ERROR("failed to send unregistered message to caller: network failure");
    }
}

void wamp_dealer::process_yield_message(const wamp_session_id& session_id,
        wamp_yield_message* yield_message)
{
    auto session_itr = m_sessions.find(session_id);
    if (session_itr == m_sessions.end()) {
        throw std::logic_error("dealer session does not exist");
    }

    // It is considered to be a normal condition if we cannot find the
    // associated invocation. Typically this occurs if the invocation
    // timed out or if the callers session has ended.
    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *yield_message);
    const auto request_id = yield_message->get_request_id();
    auto pending_invocations_itr = m_pending_invocations.find(request_id);
    if (pending_invocations_itr == m_pending_invocations.end()) {
        BONEFISH_ERROR("unable to find invocation ... timed out or session closed");
        return;
    }

    const auto& dealer_invocation = pending_invocations_itr->second;
    std::shared_ptr<wamp_session> session = dealer_invocation->get_session();

    // We can't have a pending invocation without a pending caller/callee
    // as they are tracked in a synchronized manner. So to have one without
    // the other is considered to be an error.
    assert(m_pending_callee_invocations.count(session_itr->second->get_session_id()));
    assert(m_pending_caller_invocations.count(session->get_session_id()));

    wamp_yield_options yield_options;
    yield_options.unmarshal(yield_message->get_options());

    // You can't rely on simply assigning the yield options to the result
    // details. Some yield options may only be applicable to the dealer.
    // Likewise, some result details may be in addition to whatever is
    // provided in the yield options. As a result, we only copy specific
    // options over to the result details.
    wamp_result_details result_details;
    if (yield_options.get_option_or("progress", false)) {
        result_details.set_detail("progress", true);
    }

    std::unique_ptr<wamp_result_message> result_message(
            new wamp_result_message(yield_message->release_zone()));
    result_message->set_request_id(dealer_invocation->get_request_id());
    result_message->set_details(result_details.marshal(result_message->get_zone()));
    result_message->set_arguments(yield_message->get_arguments());
    result_message->set_arguments_kw(yield_message->get_arguments_kw());

    // If we fail to send the result message it is most likely that the
    // underlying network connection has been closed/lost which means
    // that the caller is no longer reachable on this session. So all
    // we do here is trace the fact that this event occured.
    BONEFISH_TRACE("%1%, %2%", *session_itr->second % *result_message);
    if (!session->get_transport()->send_message(std::move(*result_message))) {
        BONEFISH_ERROR("failed to send result message to caller: network failure");
    }

    // If the call has more results in progress then do not cleanup any of the
    // invocation state and just bail out here.
    if (yield_options.get_option_or("progress", false)) {
        return;
    }

    // Otherwise, the call still has results in progress so do not remove the
    // invocation. The failure to send a message in the event of a network failure
    // will detach the session. When this happens the pending invocations
    // be cleaned up. So we don't use an iterator here to erase the pending
    // invocation because it may have just been invalidated above if an
    // error occured.
    m_pending_callee_invocations[session_id].erase(request_id);
    m_pending_caller_invocations[session->get_session_id()].erase(request_id);
    m_pending_invocations.erase(request_id);
}

void wamp_dealer::send_error(const std::unique_ptr<wamp_transport>& transport,
            const wamp_message_type request_type, const wamp_request_id& request_id,
            const std::string& error) const
{
    std::unique_ptr<wamp_error_message> error_message(new wamp_error_message);
    error_message->set_request_type(request_type);
    error_message->set_request_id(request_id);
    error_message->set_error(error);

    BONEFISH_ERROR("%1%", *error_message);
    if (!transport->send_message(std::move(*error_message))) {
        BONEFISH_ERROR("failed to send error message");
    }
}

void wamp_dealer::invocation_timeout_handler(const wamp_request_id& request_id,
        const boost::system::error_code& error)
{
    if (error == boost::asio::error::operation_aborted) {
        return;
    }

    auto pending_invocations_itr = m_pending_invocations.find(request_id);
    if (pending_invocations_itr == m_pending_invocations.end()) {
        BONEFISH_ERROR("error: unable to find pending invocation");
        return;
    }

    BONEFISH_ERROR("timing out a pending invocation");
    const auto& call_request_id = pending_invocations_itr->second->get_request_id();
    std::shared_ptr<wamp_session> session = pending_invocations_itr->second->get_session();

    send_error(session->get_transport(), wamp_message_type::CALL,
            call_request_id, "wamp.error.call_timed_out");

    auto pending_callee_invocations_itr =
            m_pending_callee_invocations.find(session->get_session_id());
    if (pending_callee_invocations_itr != m_pending_callee_invocations.end()) {
        pending_callee_invocations_itr->second.erase(request_id);
    }

    auto pending_caller_invocations_itr =
            m_pending_caller_invocations.find(session->get_session_id());
    if (pending_caller_invocations_itr != m_pending_caller_invocations.end()) {
        pending_caller_invocations_itr->second.erase(request_id);
    }

    // The failure to send a message in the event of a network failure
    // will detach the session. When this happens the pending invocations
    // be cleaned up. So we don't use an iterator here to erase the pending
    // invocation because it may have just been invalidated above.
    m_pending_invocations.erase(request_id);
}

void wamp_dealer::register_builtin_procedures()
{
    using namespace std::placeholders;

    m_builtin_procedures["wamp.session.add_testament"] =
        std::bind(&wamp_dealer::wamp_session_add_testament, this, _1, _2);

    m_builtin_procedures["wamp.session.flush_testaments"] =
        std::bind(&wamp_dealer::wamp_session_flush_testaments, this, _1, _2);
}

void wamp_dealer::wamp_session_add_testament(const wamp_session_id& session_id, wamp_call_message* call_message)
{
    auto session_itr = m_sessions.find(session_id);
    if (session_itr == m_sessions.end()) {
        throw std::logic_error("wamp.error.no_such_session");
    }

    // Parse the call message and store all the information in publish message (such a shortcut)

    // Positional arguments:
    //   topic|uri - the topic to publish the event on
    //   args|list - positional arguments for the event
    //   kwargs|dict - keyword arguments for the event
    // Keyword arguments:
    //   publish_options|dict - options for the event when it is published -- see Publish.Options. Not all options may
    //                          be honoured (for example, acknowledge). By default, there are no options.
    //   scope|string - When the testament should be published. Valid values are detached (when the WAMP session is
    //                  detached, for example, when using Event Retention) or destroyed (when the WAMP session is
    //                  finalized and destroyed on the Broker). Default MUST be destroyed.
    // wamp.session.add_testament does not return a value.

    std::tuple<std::string, msgpack::object, msgpack::object> args;
    call_message->get_arguments().convert(args);

    std::unordered_map<std::string, msgpack::object> kwargs;
    call_message->get_arguments_kw().convert(kwargs);

    const wamp_request_id request_id = m_request_id_generator.generate();

    // Create publish message and store topic, positional and keyword arguments in it
    std::unique_ptr<wamp_publish_message> publish_message(new wamp_publish_message(call_message->release_zone()));
    // Also, options are not supported at the moment, defaulting to empty
    publish_message->set_request_id(request_id);
    publish_message->set_topic(std::get<0>(args));
    publish_message->set_arguments(std::get<1>(args));
    publish_message->set_arguments_kw(std::get<2>(args));

    // Get the scope
    wamp_session::testament_scope scope = wamp_session::testament_scope::destroyed;
    auto scope_itr = kwargs.find("scope");
    if (scope_itr != kwargs.end() && scope_itr->second.as<std::string>() == "detached") {
        scope = wamp_session::testament_scope::detached;
    }

    BONEFISH_TRACE("wamp_session_add_testament %1%, %2%", *session_itr->second % *call_message);

    // Add testament
    session_itr->second->add_testament(scope, std::move(publish_message));

    // Send the result back
    std::unique_ptr<wamp_result_message> result_message(new wamp_result_message());
    result_message->set_request_id(call_message->get_request_id());

    // If we fail to send the result message it is most likely that the
    // underlying network connection has been closed/lost which means
    // that the caller is no longer reachable on this session. So all
    // we do here is trace the fact that this event occured.
    BONEFISH_TRACE("wamp_session_add_testament %1%, %2%, %3%", *session_itr->second % *call_message % *result_message);
    if (!session_itr->second->get_transport()->send_message(std::move(*result_message))) {
        BONEFISH_ERROR("failed to send result message to caller: network failure");
    }
}

void wamp_dealer::wamp_session_flush_testaments(const wamp_session_id& session_id, wamp_call_message* call_message)
{
    auto session_itr = m_sessions.find(session_id);
    if (session_itr == m_sessions.end()) {
        throw std::logic_error("wamp.error.no_such_session");
    }

    std::unordered_map<std::string, msgpack::object> kwargs;
    call_message->get_arguments_kw().convert(kwargs);

    wamp_session::testament_scope scope = wamp_session::testament_scope::destroyed;
    auto scope_itr = kwargs.find("scope");
    if (scope_itr != kwargs.end() && scope_itr->second.as<std::string>() == "detached") {
        scope = wamp_session::testament_scope::detached;
    }

    session_itr->second->flush_testaments(scope);

    // Send the result back
    std::unique_ptr<wamp_result_message> result_message(new wamp_result_message());
    result_message->set_request_id(call_message->get_request_id());

    // If we fail to send the result message it is most likely that the
    // underlying network connection has been closed/lost which means
    // that the caller is no longer reachable on this session. So all
    // we do here is trace the fact that this event occured.
    BONEFISH_TRACE("wamp_session_flush_testaments %1%, %2% ,%3%", *session_itr->second % static_cast<int>(scope) % *result_message);
    if (!session_itr->second->get_transport()->send_message(std::move(*result_message))) {
        BONEFISH_ERROR("failed to send result message to caller: network failure");
    }
}

} // namespace bonefish
