#include <functional>
#include <memory>
#include <rclcpp/rclcpp.hpp>

#include "interfaces/msg/camera_configuration_message.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "opencv2/opencv.hpp"
#include "camera_subscriber.h"

using namespace std;
using namespace rclcpp;
using namespace cv;

Camera_subscriber::Camera_subscriber():rclcpp::Node{ "camera_subscriber" }{
    using std::placeholders::_1;

    camera_configuration_subscriber =
    Node::create_subscription<interfaces::msg::CameraConfigurationMessage>(
    "camera_data/camera_configuration_data",
     1,
     std::bind(&Camera_subscriber::configuration_data_callback, this, _1));

    camera_data_subscriber =
    Node::create_subscription<sensor_msgs::msg::Image>(
    "camera_data/image_data",
    100,
    std::bind(&Camera_subscriber::camera_image_callback, this, _1));
}

void Camera_subscriber::configuration_data_callback(const interfaces::msg::CameraConfigurationMessage::SharedPtr data) const
{
    RCLCPP_INFO(Node::get_logger(), "Camera Configuration received");
    RCLCPP_INFO(Node::get_logger(), "Image Width: %i", data->width);
    RCLCPP_INFO(Node::get_logger(), "Image Height: %i", data->height);
    RCLCPP_INFO(Node::get_logger(), "Image Channels: %i", data->channels);
    RCLCPP_INFO(Node::get_logger(), "Video FPS: %i", data->fps);
}

void Camera_subscriber::camera_image_callback(const sensor_msgs::msg::Image::SharedPtr data) const
{
    RCLCPP_INFO(Node::get_logger(), "Camera Data received");
    RCLCPP_INFO(Node::get_logger(), "Image Width: %i", data->width);
    RCLCPP_INFO(Node::get_logger(), "Image Height: %i", data->height);
    RCLCPP_INFO(Node::get_logger(), "Image Encoding: %s", data->encoding.c_str());
    RCLCPP_INFO(Node::get_logger(), "Is Bigendian: %s", data->is_bigendian ? "true" : "false");
    RCLCPP_INFO(Node::get_logger(), "Image step: %i", data->step);


    Mat test_image = Mat(data->height, data->width, CV_8UC3, data->data.data()).clone();
    imwrite("test.jpg", test_image);
}