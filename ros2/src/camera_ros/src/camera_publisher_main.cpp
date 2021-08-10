#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <ctime>
#include <rclcpp/rclcpp.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>

#include "interfaces/msg/camera_image_message.hpp"
#include "camera_publisher.h"

using namespace std;
using namespace rclcpp;
using namespace cv;
using namespace chrono;

extern string camera_url;
extern std::shared_ptr<Camera_publisher> node;

int main(int argc, char* argv[]){
    init(argc, argv);
    node = std::make_shared<Camera_publisher>();

    RCLCPP_INFO(node->get_logger(), "Starting camera publisher");

    VideoCapture capture(camera_url);

    if (!capture.isOpened()) {
        RCLCPP_INFO(node->get_logger(), "Unable to connect to camera");
    }
    else {
        RCLCPP_INFO(node->get_logger(), "Camera connected");
        RCLCPP_INFO(node->get_logger(), "Width: %f", capture.get(CAP_PROP_FRAME_WIDTH));
        RCLCPP_INFO(node->get_logger(), "Height: %f", capture.get(CAP_PROP_FRAME_HEIGHT)); 
        RCLCPP_INFO(node->get_logger(), "FPS: %f", capture.get(CAP_PROP_FPS));
    }

    Mat image;
    while (ok()) {
        capture >> image;
        node->camera_image_handler(image, capture.get(CAP_PROP_FPS));
        spin_some(node);
    }

    shutdown();
    RCLCPP_INFO(node->get_logger(), "Stopped camera publisher");
}
