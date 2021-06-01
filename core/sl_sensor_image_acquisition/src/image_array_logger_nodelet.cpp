#include "sl_sensor_image_acquisition/image_array_logger_nodelet.hpp"

#include <cv_bridge/cv_bridge.h>
#include <cstdint>
#include <opencv2/opencv.hpp>

namespace sl_sensor
{
namespace image_acquisition
{
ImageArrayLoggerNodelet::ImageArrayLoggerNodelet()
{
}

void ImageArrayLoggerNodelet::onInit()
{
  nh_ = getNodeHandle();
  private_nh_ = getPrivateNodeHandle();

  private_nh_.param<std::string>("log_directory", log_directory_, log_directory_);
  private_nh_.param<std::string>("file_header_", file_header_, file_header_);
}

void ImageArrayLoggerNodelet::ImageArrayCb(const sl_sensor_image_acquisition::ImageArrayConstPtr image_arr_ptr)
{
  std::string time = std::to_string(image_arr_ptr->header.stamp.toNSec());

  int number_images = image_arr_ptr->data.size();

  for (int i = 0; i < number_images; i++)
  {
    auto cv_img_ptr = cv_bridge::toCvShare(image_arr_ptr->data[i], nullptr);
    std::string filename = log_directory_ + "_" + time + "_" + std::to_string(i) + file_format_;
    cv::imwrite(filename, cv_img_ptr->image);
  }
}

}  // namespace image_acquisition
}  // namespace sl_sensor