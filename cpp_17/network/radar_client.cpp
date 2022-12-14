// Copyright 2016 Navtech Radar Limited
// This file is part of iasdk which is released under The MIT License (MIT).
// See file LICENSE.txt in project root or go to https://opensource.org/licenses/MIT
// for full license details.
//

#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#endif

#include "../common.h"
#include "colossus_messages.h"
#include "colossus_network_message.h"
#include "radar_client.h"
#include "../navigation/sector_blanking.h"


namespace Navtech {
    Radar_client::Radar_client(const Utility::IP_address& radarAddress, const std::uint16_t& port) :
        radar_client    { radarAddress, port }, 
        running         { false }, 
        send_radar_data { false }
    { }

    void Radar_client::set_fft_data_callback(std::function<void(const Fft_data::Pointer&)> fn)
    {
        std::lock_guard lock { callback_mutex };
        fft_data_callback = std::move(fn);
    }

    void Radar_client::set_raw_fft_data_callback(std::function<void(const std::vector<uint8_t>&)> fn)
    {
        std::lock_guard lock { callback_mutex };
        raw_fft_data_callback = std::move(fn);
    }

    void Radar_client::set_navigation_data_callback(std::function<void(const Navigation_data::Pointer&)> fn)
    {
        std::lock_guard lock { callback_mutex };
        navigation_data_callback = std::move(fn);
    }

    void Radar_client::set_configuration_data_callback(
        std::function<void(const Configuration_data::Pointer&, const Configuration_data::ProtobufPointer&)> fn)
    {
        std::lock_guard lock { callback_mutex };
        configuration_data_callback = std::move(fn);
    }

    void Radar_client::set_raw_configuration_data_callback(std::function<void(const std::vector<uint8_t>&)> fn)
    {
        std::lock_guard lock { callback_mutex };
        raw_configuration_data_callback = std::move(fn);
    }

    void Radar_client::set_health_data_callback(std::function<void(const Shared_owner<Colossus::Protobuf::Health>&)> fn)
    {
        std::lock_guard lock { callback_mutex };
        health_data_callback = std::move(fn);
    }


    void Radar_client::set_navigation_config_callback(std::function<void(const Navigation_config::Pointer&)> fn)
    {
        std::lock_guard lock { callback_mutex };
        navigation_config_callback = std::move(fn);
    }


    void Radar_client::start()
    {
        if (running) return;
        Log("Radar_client - Starting");

        radar_client.set_receive_data_callback(std::bind(&Radar_client::handle_data, this, std::placeholders::_1));
        radar_client.start();
        running = true;

        Log("Radar_client - Started");
    }

    void Radar_client::stop()
    {
        if (!running) return;

        running = false;
        radar_client.stop();
        Log("Radar_client - Stopped");
    }

    void Radar_client::start_fft_data()
    {
        Log("Radar_client - Start FFT Data");
        send_simple_network_message(Network::Colossus_protocol::Message::Type::start_fft_data);
        send_radar_data = true;
    }

    void Radar_client::start_non_contour_fft_data()
    {
        Log("Radar_client - Start Non Contoured FFT Data");
        send_simple_network_message(Network::Colossus_protocol::Message::Type::start_non_contour_fft_data);
        send_radar_data = true;
    }

    void Radar_client::stop_fft_data()
    {
        Log("Radar_client - Stop FFT Data");
        send_simple_network_message(Network::Colossus_protocol::Message::Type::stop_fft_data);
        send_radar_data = false;
    }

    void Radar_client::start_health_data()
    {
        Log("Radar_client - Start Health Data");
        send_simple_network_message(Network::Colossus_protocol::Message::Type::start_health_msgs);
    }

    void Radar_client::stop_health_data()
    {
        Log("Radar_client - Stop Health Data");
        send_simple_network_message(Network::Colossus_protocol::Message::Type::stop_health_msgs);
    }

    void Radar_client::start_navigation_data()
    {
        Log("Radar_client - Start Navigation Data");
        send_simple_network_message(Network::Colossus_protocol::Message::Type::start_nav_data);
    }

    void Radar_client::stop_navigation_data()
    {
        Log("Radar_client - Stop Navigation Data");
        send_simple_network_message(Network::Colossus_protocol::Message::Type::stop_nav_data);
    }

    void Radar_client::set_navigation_threshold(std::uint16_t threshold)
    {
        if (radar_client.get_connection_state() != Connection_state::connected) return;

        std::vector<std::uint8_t> buffer(sizeof(threshold));
        auto network_threshold = ntohs(threshold);
        std::memcpy(buffer.data(), &network_threshold, sizeof(network_threshold));

        Network::Colossus_protocol::Message msg {};
        msg.type(Network::Colossus_protocol::Message::Type::set_nav_threshold);
        msg.append(buffer);
        radar_client.send(msg.relinquish());
    }

    void Radar_client::set_navigation_gain_and_offset(float gain, float offset)
    {
        if (radar_client.get_connection_state() != Connection_state::connected) return;

        std::vector<std::uint8_t> buffer(sizeof(std::uint32_t) * 2);
        std::uint32_t net_gain   = htonl(static_cast<std::uint32_t>(gain * range_multiplier));
        std::uint32_t net_offset = htonl(static_cast<std::uint32_t>(offset * range_multiplier));
        std::memcpy(buffer.data(), &net_gain, sizeof(net_gain));
        std::memcpy(&buffer[sizeof(net_gain)], &net_offset, sizeof(net_offset));

        Network::Colossus_protocol::Message msg {};
        msg.type(Network::Colossus_protocol::Message::Type::set_nav_range_offset_and_gain);
        msg.append(buffer);
        radar_client.send(msg.relinquish());
    }


    void Radar_client::request_navigation_configuration()
    {
        send_simple_network_message(Network::Colossus_protocol::Message::Type::navigation_config_request);
    }


    void Radar_client::set_navigation_configuration(const Navigation_config& cfg)
    {
        Log("Radar_client - Set navigation configuration");

        // Copy struct to message to ensure correct endianness.
        //
        // TODO - The Colossus message itself could be used as the argument 
        // to this function, removing the need for an intermediate struct.
        //
        Network::Colossus_protocol::Navigation_config nav_config { };
        nav_config.bins_to_operate_on(cfg.bins_to_operate_on);
        nav_config.min_bin_to_operate_on(cfg.min_bin);
        nav_config.navigation_threshold(cfg.navigation_threshold);
        nav_config.max_peaks_per_azimuth(cfg.max_peaks_per_azimuth);

        Network::Colossus_protocol::Message msg {};
        msg.type(Network::Colossus_protocol::Message::Type::set_navigation_configuration);
        msg.append(nav_config);

        radar_client.send(msg.relinquish());
    }


    void Radar_client::set_blanking_sectors(const Blanking_sector_list& sector_list)
    {
        Log("Radar_client - Set blanking sectors");

        Network::Colossus_protocol::Message msg { };
        msg.type(Network::Colossus_protocol::Message::Type::sector_blanking_update);
        msg.append(sector_list.to_vector());

        radar_client.send(msg.relinquish());
    }


    void Radar_client::send_simple_network_message(const Network::Colossus_protocol::Message::Type& type)
    {
        if (radar_client.get_connection_state() != Connection_state::connected) return;

        Network::Colossus_protocol::Message msg {};
        msg.type(type);
        radar_client.send(msg.relinquish());
    }

    void Radar_client::update_contour_map(const std::vector<std::uint8_t>& contour_data)
    {
        if (radar_client.get_connection_state() != Connection_state::connected) return;

        auto contour = std::vector<std::uint8_t>();
        if (contour_data.size() == 720) {
            contour.resize(contour_data.size());
            for (std::size_t i = 0; i < contour_data.size(); i += 2) {
                auto result    = (((contour_data[i]) << 8) | ((contour_data[i + 1])));
                result         = (std::uint16_t)std::ceil(result / bin_size);
                contour[i]     = (result & 0xff00) >> 8;
                contour[i + 1] = result & 0x00ff;
            }
        }

        Network::Colossus_protocol::Message msg {};
        msg.type(Network::Colossus_protocol::Message::Type::contour_update);
        msg.append(contour);
        radar_client.send(msg.relinquish());
    }

    void Radar_client::handle_data(std::vector<std::uint8_t>&& data)
    {
        Network::Colossus_protocol::Message msg { std::move(data) };

        switch (msg.type()) {
            case Network::Colossus_protocol::Message::Type::configuration:
                handle_configuration_message(msg);
                break;

            case Network::Colossus_protocol::Message::Type::fft_data:
                handle_fft_data_message(msg);
                break;

            case Network::Colossus_protocol::Message::Type::navigation_data:
                handle_navigation_data_message(msg);
                break;

            case Network::Colossus_protocol::Message::Type::navigation_alarm_data:
                break;

            case Network::Colossus_protocol::Message::Type::health:
                handle_health_message(msg);
                break;

            case Network::Colossus_protocol::Message::Type::navigation_configuration:
                handle_navigation_config_message(msg);
                break;

            default:
                Log("Radar_client - Unhandled Message [" + std::to_string(static_cast<uint32_t>(msg.type())) + "]");
                break;
        }
    }

    void Radar_client::handle_configuration_message(Network::Colossus_protocol::Message& msg)
    {
        Log("Radar_client - Handle Configuration Message");

        callback_mutex.lock();
        auto configuration_fn = configuration_data_callback;
        callback_mutex.unlock();
        if (configuration_fn != nullptr) {
            auto config                 = msg.view_as<Network::Colossus_protocol::Configuration>();
            auto protobuf_configuration = allocate_shared<Colossus::Protobuf::ConfigurationData>();
            protobuf_configuration->ParseFromString(config->to_string());

            if (send_radar_data) send_simple_network_message(Network::Colossus_protocol::Message::Type::start_fft_data);

            encoder_size                               = config->encoder_size();
            bin_size                                   = protobuf_configuration->rangeresolutionmetres();
            auto configuration_data                    = allocate_shared<Configuration_data>();
            configuration_data->azimuth_samples        = config->azimuth_samples();
            configuration_data->bin_size               = bin_size;
            configuration_data->range_in_bins          = config->range_in_bins();
            configuration_data->encoder_size           = encoder_size;
            configuration_data->expected_rotation_rate = config->rotation_speed() / 1000;
            configuration_data->range_gain             = config->range_gain();
            configuration_data->range_offset           = config->range_offset();

            configuration_fn(configuration_data, protobuf_configuration);
        }

        if (raw_configuration_data_callback == nullptr) return;
        raw_configuration_data_callback(msg.relinquish());
    }

    void Radar_client::handle_health_message(Network::Colossus_protocol::Message& msg)
    {
        callback_mutex.lock();
        auto health_data_fn = health_data_callback;
        callback_mutex.unlock();
        if (health_data_fn == nullptr) return;

        auto health          = msg.view_as<Network::Colossus_protocol::Health>();
        auto protobuf_health = allocate_shared<Colossus::Protobuf::Health>();
        protobuf_health->ParseFromString(health->to_string());

        health_data_fn(protobuf_health);
    }

    void Radar_client::handle_fft_data_message(Network::Colossus_protocol::Message& msg)
    {
        callback_mutex.lock();
        auto fft_data_fn = fft_data_callback;
        callback_mutex.unlock();
        if (fft_data_fn != nullptr) {
            auto fft_data = msg.view_as<Network::Colossus_protocol::Fft_data>();

            auto fftData               = allocate_shared<Fft_data>();
            fftData->azimuth           = fft_data->azimuth();
            fftData->angle             = (fft_data->azimuth() * 360.0f) / encoder_size;
            fftData->sweep_counter     = fft_data->sweep_counter();
            fftData->ntp_seconds       = fft_data->ntp_seconds();
            fftData->ntp_split_seconds = fft_data->ntp_split_seconds();
            fftData->data              = fft_data->to_vector();

            fft_data_fn(fftData);
        }

        if (raw_fft_data_callback == nullptr) return;
        raw_fft_data_callback(msg.relinquish());
    }

    void Radar_client::handle_navigation_data_message(Network::Colossus_protocol::Message& msg)
    {
        callback_mutex.lock();
        auto navigation_data_fn = navigation_data_callback;
        callback_mutex.unlock();
        if (navigation_data_fn == nullptr) return;

        auto nav_data = msg.view_as<Network::Colossus_protocol::Navigation_data>();
        auto targets  = nav_data->to_vector();

        auto navigation_data               = allocate_shared<Navigation_data>();
        navigation_data->azimuth           = nav_data->azimuth();
        navigation_data->ntp_seconds       = nav_data->ntp_seconds();
        navigation_data->ntp_split_seconds = nav_data->ntp_split_seconds();
        navigation_data->angle             = (nav_data->azimuth() * 360.0f) / encoder_size;

        auto peaks_count = targets.size() / nav_data_record_length;
        for (auto i = 0u; i < (peaks_count * nav_data_record_length); i += nav_data_record_length) {
            std::uint32_t peak_resolve = 0;
            std::memcpy(&peak_resolve, &targets[i], sizeof(peak_resolve));
            std::uint16_t power = 0;
            std::memcpy(&power, &targets[i + sizeof(peak_resolve)], sizeof(power));
            navigation_data->peaks.push_back(
                std::make_tuple<float, std::uint16_t>(htonl(peak_resolve) / range_multiplier_float, htons(power)));
        }

        navigation_data_fn(navigation_data);
    }


    void Radar_client::handle_navigation_config_message(Network::Colossus_protocol::Message& msg)
    {
        callback_mutex.lock();
        auto navigation_config_fn = navigation_config_callback;
        callback_mutex.unlock();
        if (navigation_config_fn == nullptr) return;

        auto view = msg.view_as<Network::Colossus_protocol::Navigation_config>();

        auto navigation_config                   = allocate_shared<Navigation_config>();
        navigation_config->bins_to_operate_on    = view->bins_to_operate_on();
        navigation_config->min_bin               = view->min_bin_to_operate_on();
        navigation_config->navigation_threshold  = view->navigation_threshold();
        navigation_config->max_peaks_per_azimuth = view->max_peaks_per_azimuth();

        navigation_config_fn(navigation_config);
    }

} // namespace Navtech
