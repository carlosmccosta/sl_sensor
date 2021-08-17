#pragma once

#include <cv_bridge/cv_bridge.h>
#include <nodelet/nodelet.h>
#include <pluginlib/class_list_macros.h>
#include <ros/ros.h>
#include <std_srvs/Trigger.h>
#include <boost/thread/thread.hpp>
#include <opencv2/opencv.hpp>
#include <string>

#include "sl_sensor_image_acquisition/ImageArray.h"

namespace sl_sensor
{
namespace calibration
{
class CalibrationSequenceAcquisitionNodelet : public nodelet::Nodelet
{
public:
  CalibrationSequenceAcquisitionNodelet();
  ~CalibrationSequenceAcquisitionNodelet();

private:
  ros::Subscriber image_array_sub_;
  std::vector<std::vector<ros::Publisher>> image_pubs_;
  ros::ServiceClient image_synchroniser_client_;
  ros::ServiceClient projector_client_;
  ros::ServiceServer erase_sequence_client_;
  ros::ServiceServer grab_next_sequence_client_;

  boost::shared_ptr<boost::thread> initialisation_thread_ptr_;

  std::string image_array_sub_topic_ = "/image_array_receive";
  std::string save_directory_ = "/";
  std::string save_filename_ = "calibration_session";
  std::string erase_sequence_service_name_ = "erase_calibration_sequence";
  std::string grab_sequence_service_name_ = "grab_calibration_sequence";
  std::string image_synchroniser_service_name_ = "command_image_synchroniser";
  std::string projector_service_name_ = "command_projector";
  int checkerboard_num_rows_ = 10;
  int checkerboard_num_cols_ = 10;
  cv::Size checkerboard_size_;
  int number_cameras_ = 1;
  int counter_ = 0;

  ros::NodeHandle nh_;
  ros::NodeHandle private_nh_;

  std::vector<cv_bridge::CvImageConstPtr> image_vec_buffer_;

  boost::mutex mutex_;

  bool ready_ = false;

  virtual void onInit();

  void ImageArrayCb(const sl_sensor_image_acquisition::ImageArrayConstPtr image_arr_ptr);

  bool GetInputAndCheckIfItIsExpectedChar(const std::string& message, char expected_char);

  void ProcessFloatImage(const cv::Mat& input_image, cv::Mat& output_image);

  bool GenerateDataFolders();

  void Init();

  void SendCommandForNextCalibrationSequence();

  void SendProjectorCommand(const std::string& command, int pattern_no);

  bool ProcessGrabNextSequenceCommand(std_srvs::Trigger::Request& req, std_srvs::Trigger::Response& res);

  bool ProcessEraseSequenceCommand(std_srvs::Trigger::Request& req, std_srvs::Trigger::Response& res);

  void SaveData(const std::vector<cv_bridge::CvImageConstPtr>& cv_img_ptr_vec);
};

}  // namespace calibration
}  // namespace sl_sensor

PLUGINLIB_EXPORT_CLASS(sl_sensor::calibration::CalibrationSequenceAcquisitionNodelet, nodelet::Nodelet);
