#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <rclcpp/rclcpp.hpp>

#include "interfaces/msg/configuration_data_message.hpp"
#include "interfaces/msg/fft_data_message.hpp"
#include "radar_client.h"
#include "colossus_test_tool.h"

using namespace std;
using namespace Navtech;
using namespace rclcpp;

int main(int argc, char* argv[])
{
    init(argc, argv);
    auto node = std::make_shared<Colossus_test_tool>();

    RCLCPP_INFO(node->get_logger(), "Starting radar client");
    node->radar_client = allocate_owned<Navtech::Radar_client>(node->radar_ip, node->radar_port);
    node->radar_client->set_fft_data_callback(std::bind(&Colossus_test_tool::fft_data_handler, node.get(), std::placeholders::_1));
    node->radar_client->set_configuration_data_callback(std::bind(&Colossus_test_tool::configuration_data_handler, node.get(), std::placeholders::_1));

    node->radar_client->start();

    while (ok()) {
        spin(node);
    }

    node->radar_client->stop_fft_data();
    node->radar_client->set_configuration_data_callback();
    node->radar_client->set_fft_data_callback();
    node->radar_client->stop();
    shutdown();
    RCLCPP_INFO(node->get_logger(), "Stopped radar client");
}
