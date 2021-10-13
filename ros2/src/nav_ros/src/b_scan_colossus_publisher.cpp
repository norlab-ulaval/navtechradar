#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/qos.hpp>

#include "interfaces/msg/configuration_data_message.hpp"
#include "interfaces/msg/fft_data_message.hpp"
#include "radar_client.h"
#include "b_scan_colossus_publisher.h"
#include "net_conversion.h"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/header.hpp"
#include <vector>

B_scan_colossus_publisher::B_scan_colossus_publisher():Node{ "b_scan_colossus_publisher" }
{
    declare_parameter("radar_ip", "");
    declare_parameter("radar_port", 0);
    declare_parameter("start_azimuth", 0);
    declare_parameter("end_azimuth", 0);
    declare_parameter("start_bin", 0);
    declare_parameter("end_bin", 0);
    declare_parameter("azimuth_offset", 0);

    radar_ip = get_parameter("radar_ip").as_string();
    radar_port = get_parameter("radar_port").as_int();
    start_azimuth = get_parameter("start_azimuth").as_int();
    end_azimuth = get_parameter("end_azimuth").as_int();
    start_bin = get_parameter("start_bin").as_int();
    end_bin = get_parameter("end_bin").as_int();
    azimuth_offset = get_parameter("azimuth_offset").as_int();

    rclcpp::QoS qos_b_scan_image_publisher(b_scan_image_queue_size);
    qos_b_scan_image_publisher.reliable();

    b_scan_image_publisher =
    Node::create_publisher<sensor_msgs::msg::Image>(
    "radar_data/b_scan_image_data",
    qos_b_scan_image_publisher);
}

void B_scan_colossus_publisher::image_data_handler(const Navtech::Fft_data::Pointer& data) {

    if (intensity_values.size() != azimuth_samples * range_in_bins) {
        return;
    }

    auto message = sensor_msgs::msg::Image();
    message.header = std_msgs::msg::Header();
    message.header.stamp.sec = data->ntp_seconds;
    message.header.stamp.nanosec = data->ntp_split_seconds;
    message.header.frame_id = "b_scan_image";

    message.height = azimuth_samples;
    message.width = range_in_bins;
    message.encoding = "8UC1";
    message.is_bigendian = false;
    message.step = message.width;
    message.data = std::move(intensity_values);

    b_scan_image_publisher->publish(message);
}

void B_scan_colossus_publisher::fft_data_handler(const Navtech::Fft_data::Pointer& data)
{
    int azimuth_index = static_cast<int>(data->angle / (360.0 / azimuth_samples));

    // To adjust radar start azimuth, for sake of visualisation
    // Note - this value will be different for every setup!
    // Values based on 0 angle of radar, and surrounding landscape
    int adjusted_azimuth_index = azimuth_index + azimuth_offset;
    if (adjusted_azimuth_index >= azimuth_samples) {
        adjusted_azimuth_index = adjusted_azimuth_index - azimuth_samples;
    }

    if ((azimuth_index >= start_azimuth) && (azimuth_index < end_azimuth)) {
        for (int y = 0; y < range_in_bins; y++) {
            if ((y >= start_bin) && (y < end_bin)) {
                intensity_values.push_back(data->data[y]);
            }
            else {
                intensity_values.push_back(0);
            }
        }
    }
    else {
        for (int y = 0; y < range_in_bins; y++) {
            intensity_values.push_back(0);
        }
    }

    if (data->azimuth < last_azimuth) {
        rotated_once = true;
        B_scan_colossus_publisher::image_data_handler(data);
        intensity_values.clear();
    }
    last_azimuth = data->azimuth;

    if (!rotated_once) {
        return;
    }
}

void B_scan_colossus_publisher::configuration_data_handler(const Navtech::Configuration_data::Pointer& data) {
    RCLCPP_INFO(Node::get_logger(), "Configuration Data Received");
    RCLCPP_INFO(Node::get_logger(), "Azimuth Samples: %i", data->azimuth_samples);
    RCLCPP_INFO(Node::get_logger(), "Encoder Size: %i", data->encoder_size);
    RCLCPP_INFO(Node::get_logger(), "Bin Size: %f", data->bin_size);
    RCLCPP_INFO(Node::get_logger(), "Range In Bins: : %i", data->range_in_bins);
    RCLCPP_INFO(Node::get_logger(), "Expected Rotation Rate: %i", data->expected_rotation_rate);
    RCLCPP_INFO(Node::get_logger(), "Publishing Configuration Data");

    azimuth_samples = data->azimuth_samples;
    range_in_bins = data->range_in_bins;
    buffer_length = azimuth_samples * range_in_bins * sizeof(uint8_t);

    RCLCPP_INFO(Node::get_logger(), "Starting b scan publisher");
    RCLCPP_INFO(Node::get_logger(), "Start azimuth: %i", start_azimuth);
    RCLCPP_INFO(Node::get_logger(), "End azimuth: %i", end_azimuth);
    RCLCPP_INFO(Node::get_logger(), "Start bin: %i", start_bin);
    RCLCPP_INFO(Node::get_logger(), "End bin: %i", end_bin);
    RCLCPP_INFO(Node::get_logger(), "Azimuth offset: %i", azimuth_offset);

    radar_client->start_fft_data();
}