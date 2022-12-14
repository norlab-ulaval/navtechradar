// Copyright 2016 Navtech Radar Limited
// This file is part of iasdk which is released under The MIT License (MIT).
// See file LICENSE.txt in project root or go to https://opensource.org/licenses/MIT
// for full license details.
//

#include <algorithm>
#include <cstring>
#include <functional>

#include "../common.h"
#include "colossus_network_message.h"
#include "tcp_radar_client.h"

namespace Navtech {
    Tcp_radar_client::Tcp_radar_client(const Utility::IP_address& ip_addr, const std::uint16_t& port) :
        receive_data_queue      { Threaded_queue<std::vector<std::uint8_t>>() }, 
        ip_address              { ip_addr }, 
        port                    { port },
        socket                  { ip_address, port }, 
        read_thread             { nullptr }, 
        connection_check_timer  { connection_check_timeout },
        connection_state        { Connection_state::disconnected }, 
        reading                 { false }, 
        running                 { false }
    {
    }


    void Tcp_radar_client::set_receive_data_callback(std::function<void(std::vector<std::uint8_t>&&)> callback)
    {
        receive_data_queue.set_dequeue_callback(std::move(callback));
    }


    Connection_state Tcp_radar_client::get_connection_state()
    {
        if (!running) return Connection_state::disconnected;

        connection_state_mutex.lock();
        auto state = connection_state;
        connection_state_mutex.unlock();
        return state;
    }


    void Tcp_radar_client::start()
    {
        if (running) return;

        receive_data_queue.start();
        running        = true;
        connect_thread = allocate_owned<std::thread>(std::bind(&Tcp_radar_client::connect_thread_handler, this));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        connect_condition.notify_all();
        connection_check_timer.set_callback(std::bind(&Tcp_radar_client::connection_check_handler, this));
        connection_check_timer.enable(true);
    }


    void Tcp_radar_client::stop()
    {
        if (!running) return;

        receive_data_queue.stop();

        connection_check_timer.enable(false);
        connection_check_timer.set_callback(nullptr);

        running = false;
        socket.close(Tcp_socket::Close_option::shutdown);
        connect_condition.notify_all();
        if (connect_thread != nullptr) {
            connect_thread->join();
            connect_thread = nullptr;
        }

        reading = false;
        if (read_thread != nullptr) {
            read_thread->join();
            read_thread = nullptr;
        }
    }


    void Tcp_radar_client::set_connection_state(const Connection_state& state)
    {
        if (!running) return;

        std::lock_guard lock { connection_state_mutex };
        if (connection_state == state) { return; }
        connection_state = state;

        std::string state_string;
        switch (state) {
            case Connection_state::connected:
                state_string = "Connected";
                break;
            case Connection_state::connecting:
                state_string = "Connecting";
                break;
            case Connection_state::disconnected:
                state_string = "Disconnected";
                break;
        }

        Log("Tcp_radar_client - Connection State Changed [" + state_string + "] for [" + ip_address.to_string() + ":" +
            std::to_string(port) + "]");
    }


    void Tcp_radar_client::connect_thread_handler()
    {
        Log("Tcp_radar_client - Connect Thread Started");

        while (running) {
            std::unique_lock<std::mutex> lock(connect_mutex);
            connect_condition.wait(lock);
            if (!running) break;
            connect();
        }

        Log("Tcp_radar_client - Connect Thread Finished");
    }


    void Tcp_radar_client::connection_check_handler()
    {
        if (get_connection_state() != Connection_state::disconnected) return;
        Log("Tcp_radar_client - Connection error try again");
        connect_condition.notify_one();
    }


    void Tcp_radar_client::connect()
    {
        if (get_connection_state() == Connection_state::connected) return;

        set_connection_state(Connection_state::connecting);

        socket.close();
        socket.create(read_timeout);
        socket.set_send_timeout(send_timeout);

        if (socket.connect()) {
            if (read_thread != nullptr) {
                read_thread->join();
                read_thread = nullptr;
            }
            set_connection_state(Connection_state::connected);
            reading     = true;
            read_thread = allocate_owned<std::thread>(std::bind(&Tcp_radar_client::read_thread_handler, this));
        }
        else {
            set_connection_state(Connection_state::disconnected);
        }
    }


    void Tcp_radar_client::read_thread_handler()
    {
        Log("Tcp_radar_client - Read Thread Started");

        using std::array;
        using std::begin;
        using std::memcmp;

        while (reading && running) {
            std::vector<std::uint8_t> signature { };

            std::int32_t bytes_read = socket.receive(
                signature, 
                Network::Colossus_protocol::Message::valid_signature().size(), 
                Tcp_socket::Receive_option::peek
            );

            if (bytes_read == 0) continue;

            if (bytes_read < 0 || !reading || !running) {
                set_connection_state(Connection_state::disconnected);
                break;
            }

            using std::begin;
            using std::end;
            using std::equal;

            // TODO - remove
            //
            // auto result = equal(begin(Network::Colossus_protocol::valid_signature),
            //                     end(Network::Colossus_protocol::valid_signature),
            //                     begin(signature));

            // if (!result) {

            if (signature != Network::Colossus_protocol::Message::valid_signature()) {
                bytes_read = socket.receive(signature, 1);
                if (bytes_read <= 0 || !reading || !running) {
                    set_connection_state(Connection_state::disconnected);
                    Log("Tcp_radar_client - Read Failed Signature");
                    break;
                }
                continue;
            }

            if (!handle_data() || !reading || !running) {
                set_connection_state(Connection_state::disconnected);
                Log("Tcp_radar_client - Failed Handle_data");
                break;
            }
        }
        reading = false;
        Log("Tcp_radar_client - Read Thread Exited");
    }


    void Tcp_radar_client::send(const std::vector<std::uint8_t> data)
    {
        if (get_connection_state() != Connection_state::connected) return;

        if (socket.send(data) != 0) {
            set_connection_state(Connection_state::disconnected);
            Log("Tcp_radar_client - Send Failed");
        }
    }


    bool Tcp_radar_client::handle_data()
    {
        Navtech::Network::Colossus_protocol::Message msg {};

        std::vector<std::uint8_t> data {};
        std::int32_t bytes_read = socket.receive(data, msg.header_size());

        if (bytes_read <= 0 || !reading || !running) {
            set_connection_state(Connection_state::disconnected);
            Log("Tcp_radar_client - Failed to read header");
            return false;
        }

        msg.replace(data);
        if (msg.is_valid() && msg.payload_size() != 0) {
            std::vector<std::uint8_t> payload_data;
            std::int32_t bytes_transferred = socket.receive(payload_data, msg.payload_size());
            if (bytes_transferred <= 0 || !reading || !running) {
                set_connection_state(Connection_state::disconnected);
                Log("Tcp_radar_client - Failed to read payload");
                return false;
            }
            bytes_transferred++;
            msg.append(payload_data);
            receive_data_queue.enqueue(msg.relinquish());
        }
        else if (msg.is_valid()) {
            receive_data_queue.enqueue(msg.relinquish());
        }

        return true;
    }

} // namespace Navtech
