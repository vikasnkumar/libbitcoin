/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_RESUBSCRIBER_IPP
#define LIBBITCOIN_RESUBSCRIBER_IPP

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <bitcoin/bitcoin/utility/assert.hpp>
#include <bitcoin/bitcoin/utility/dispatcher.hpp>
#include <bitcoin/bitcoin/utility/thread.hpp>
#include <bitcoin/bitcoin/utility/threadpool.hpp>
////#include <bitcoin/bitcoin/utility/track.hpp>

namespace libbitcoin {

template <typename... Args>
resubscriber<Args...>::resubscriber(threadpool& pool,
    const std::string& class_name)
  : stopped_(true), dispatch_(pool, class_name)
    /*, track<resubscriber<Args...>>(class_name)*/
{
}

template <typename... Args>
resubscriber<Args...>::~resubscriber()
{
    BITCOIN_ASSERT_MSG(subscriptions_.empty(), "resubscriber not cleared");
}

template <typename... Args>
void resubscriber<Args...>::start()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    subscribe_mutex_.lock_upgrade();

    if (stopped_)
    {
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        subscribe_mutex_.unlock_upgrade_and_lock();
        stopped_ = false;
        subscribe_mutex_.unlock();
        //---------------------------------------------------------------------
        return;
    }

    subscribe_mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////
}

template <typename... Args>
void resubscriber<Args...>::stop()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    subscribe_mutex_.lock_upgrade();

    if (!stopped_)
    {
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        subscribe_mutex_.unlock_upgrade_and_lock();
        stopped_ = true;
        subscribe_mutex_.unlock();
        //---------------------------------------------------------------------
        return;
    }

    subscribe_mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////
}

template <typename... Args>
void resubscriber<Args...>::subscribe(handler handler, Args... stopped_args)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    subscribe_mutex_.lock_upgrade();

    if (!stopped_)
    {
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        subscribe_mutex_.unlock_upgrade_and_lock();
        subscriptions_.emplace_back(handler);
        subscribe_mutex_.unlock();
        //---------------------------------------------------------------------
        return;
    }

    subscribe_mutex_.unlock_upgrade();
    ///////////////////////////////////////////////////////////////////////////

    handler(stopped_args...);
}

template <typename... Args>
void resubscriber<Args...>::invoke(Args... args)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock(invoke_mutex_);
    do_invoke(args...);
    ///////////////////////////////////////////////////////////////////////////
}

template <typename... Args>
void resubscriber<Args...>::relay(Args... args)
{
    // The ordered dispatch prevents concurrent do_invoke.
    dispatch_.ordered(&resubscriber<Args...>::do_invoke,
        this->shared_from_this(), args...);
}

// private
template <typename... Args>
void resubscriber<Args...>::do_invoke(Args... args)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    subscribe_mutex_.lock();

    // Move subscribers from the member list to a temporary list.
    list subscriptions;
    std::swap(subscriptions, subscriptions_);

    subscribe_mutex_.unlock();
    ///////////////////////////////////////////////////////////////////////////

    // Subscriptions may be created while this loop is executing.
    // Invoke subscribers from temporary list and resubscribe as indicated.
    for (const auto& handler: subscriptions)
    {
        if (handler(args...))
        {
            // Critical Section
            ///////////////////////////////////////////////////////////////////
            unique_lock(subscribe_mutex_);
            subscriptions_.emplace_back(handler);
            ///////////////////////////////////////////////////////////////////
        }
    }
}

} // namespace libbitcoin

#endif
