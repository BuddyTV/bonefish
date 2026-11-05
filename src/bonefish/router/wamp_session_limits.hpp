/**
 *  Copyright (C) 2026 Vizio Services
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

#ifndef BONEFISH_ROUTER_WAMP_SESSION_LIMITS_HPP
#define BONEFISH_ROUTER_WAMP_SESSION_LIMITS_HPP

#include <chrono>
#include <cstddef>
#include <functional>

namespace bonefish {

class wamp_session_id;

/// Per-session resource limits of a realm. A value of 0 disables the corresponding limit.
struct wamp_session_limits
{
    /// Subscriptions a session may hold at once.
    std::size_t max_subscriptions = 0;
    /// Procedures a session may register at once.
    std::size_t max_registrations = 0;
    /// Calls a session may have waiting for a result at once.
    std::size_t max_pending_calls = 0;
    /// Rejections tolerated within violation_window; one more closes the session.
    std::size_t max_violations = 0;
    std::chrono::milliseconds violation_window{60000};
};

namespace session_limit_error {

inline constexpr char too_many_subscriptions[] = "com.vizio.conjure.error.too_many_subscriptions";
inline constexpr char too_many_registrations[] = "com.vizio.conjure.error.too_many_registrations";
inline constexpr char too_many_pending_calls[] = "com.vizio.conjure.error.too_many_pending_calls";
inline constexpr char limit_exceeded[] = "com.vizio.conjure.error.limit_exceeded";

} // namespace session_limit_error

/// Called by the broker and dealer when a request is rejected because of a session limit.
using session_limit_violation_handler = std::function<void(const wamp_session_id&)>;

} // namespace bonefish

#endif // BONEFISH_ROUTER_WAMP_SESSION_LIMITS_HPP
