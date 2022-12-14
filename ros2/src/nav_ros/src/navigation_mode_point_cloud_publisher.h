#include <rclcpp/rclcpp.hpp>
#include "radar_client.h"
#include "navigation/peak_finder.h"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "messages/msg/radar_configuration_message.hpp"
#include <vector>

class Navigation_mode_point_cloud_publisher : public ::rclcpp::Node
{
public:
    Navigation_mode_point_cloud_publisher();

    void set_peak_finder(std::shared_ptr<Navtech::Peak_finder> peak) {
        peak_finder = peak;
    }

    std::shared_ptr<Navtech::Peak_finder> get_peak_finder() {
        return peak_finder;
    }

    void set_radar_client(std::shared_ptr<Navtech::Radar_client> client) {
        radar_client = client;
    }

    std::shared_ptr<Navtech::Radar_client> get_radar_client() {
        return radar_client;
    }

    void set_radar_ip(std::string ip) {
        radar_ip = ip;
    }

    std::string get_radar_ip() {
        return radar_ip;
    }

    void get_radar_port(uint16_t port) {
        radar_port = port;
    }

    uint16_t get_radar_port() {
        return radar_port;
    }

    void fft_data_handler(const Navtech::Fft_data::Pointer& data);
    void target_data_handler(const Navtech::Azimuth_target& target_data);
    void configuration_data_handler(const Navtech::Configuration_data::Pointer& data, const Navtech::Configuration_data::ProtobufPointer& protobuf_data);
    void navigation_data_handler(const Navtech::Navigation_data::Pointer& data);
    void navigation_config_data_handler(const Navtech::Navigation_config::Pointer& data);

private:
    constexpr static int radar_configuration_queue_size{ 1 };
    constexpr static int radar_point_cloud_queue_size{ 4 };

    std::shared_ptr<Navtech::Radar_client> radar_client{};
    std::shared_ptr<Navtech::Peak_finder> peak_finder{};

    std::string radar_ip{ "" };
    uint16_t radar_port{ 0 };

    uint16_t start_azimuth{ 0 };
    uint16_t end_azimuth{ 0 };
    uint16_t start_bin{ 0 };
    uint16_t end_bin{ 0 };
    uint16_t azimuth_offset{ 0 };

    uint16_t bins_to_operate_on{ 0 };
    uint16_t min_bin{ 0 };
    double power_threshold{ 0 };
    uint32_t max_peaks_per_azimuth{ 0 };
    bool process_locally{ false };

    std::vector <float> azimuth_values;
    std::vector <float> bin_values;
    std::vector <float> intensity_values;
    Navtech::Configuration_data::Pointer config_data;
    Navtech::Configuration_data::ProtobufPointer protobuf_config_data;

    int azimuth_samples{ 0 };
    float bin_size{ 0 };
    int range_in_bins{ 0 };
    int expected_rotation_rate{ 0 };
    int last_azimuth{ 0 };
    bool rotated_once{ false };
    int rotation_count{ 0 };
    int config_publish_count{ 4 };

    void publish_point_cloud(const Navtech::Navigation_data::Pointer& data);
    std::vector<uint8_t> floats_to_uint8_t_vector(float x, float y, float z, float intensity);
    void update_navigation_config();
    void update_local_navigation_config();
    void check_config_publish();

    messages::msg::RadarConfigurationMessage config_message = messages::msg::RadarConfigurationMessage{};

    rclcpp::Publisher<messages::msg::RadarConfigurationMessage>::SharedPtr configuration_data_publisher{};
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr point_cloud_publisher{};
};