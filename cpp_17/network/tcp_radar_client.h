// Copyright 2016 Navtech Radar Limited
// This file is part of iasdk which is released under The MIT License (MIT).
// See file LICENSE.txt in project root or go to https://opensource.org/licenses/MIT
// for full license details.
//

#ifndef TCP_RADAR_CLIENT_H
#define TCP_RADAR_CLIENT_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "pointer_types.h"
#include "tcp_socket.h"
#include "threaded_queue.h"
#include "timer.h"

#include "../utility/ip_address.h"

namespace Navtech {

    enum class Connection_state { disconnected, connecting, connected };

    struct Connection_info
    {
        std::uint32_t unique_id { 0 };
        Connection_state state { Connection_state::disconnected };
    };

    typedef Shared_owner<Connection_info> ConnectionInfoPtr_t;

    constexpr std::chrono::milliseconds connection_check_timeout { std::chrono::milliseconds(5000) };
    constexpr std::uint16_t read_timeout { 60 };
    constexpr std::uint16_t send_timeout { 10 };

    class Tcp_radar_client {
    public:
        explicit Tcp_radar_client(const Utility::IP_address& ip_address, const std::uint16_t& port = 6317);
        explicit Tcp_radar_client(const Tcp_radar_client&) = delete;
        Tcp_radar_client& operator=(const Tcp_radar_client&) = delete;
        void start();
        void stop();
        void send(const std::vector<std::uint8_t> data);
        void set_receive_data_callback(std::function<void(std::vector<std::uint8_t>&&)> callback = nullptr);
        Navtech::Connection_state get_connection_state();

    private:
        Threaded_queue<std::vector<std::uint8_t>> receive_data_queue;
        Utility::IP_address ip_address { "192.168.0.1" };
        std::uint16_t port { 6317 };
        Tcp_socket socket;
        Owner_of<std::thread> connect_thread { nullptr };
        Owner_of<std::thread> read_thread { nullptr };
        Timer connection_check_timer;
        Connection_state connection_state { Connection_state::disconnected };
        std::mutex connection_state_mutex {};
        std::condition_variable connect_condition {};
        std::mutex connect_mutex {};
        std::atomic_bool reading {};
        std::atomic_bool running {};

        void set_connection_state(const Connection_state& state);
        void connection_check_handler();
        void connect_thread_handler();
        void connect();
        void read_thread_handler();
        bool handle_data();
    };

} // namespace Navtech

#endif // TCP_RADAR_CLIENT_H
