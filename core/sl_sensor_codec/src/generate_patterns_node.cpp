#include <ros/ros.h>
#include <opencv2/opencv.hpp>
#include <sl_sensor_calibration/projector_parameters.hpp>
#include <sl_sensor_projector/projector_utils.hpp>

#include "sl_sensor_codec/codec_factory.hpp"
#include "sl_sensor_codec/encoder.hpp"

using namespace sl_sensor::codec;
using namespace sl_sensor::calibration;
using namespace sl_sensor::projector;

int main(int argc, char** argv)
{
  // Init ros node
  ros::init(argc, argv, "calibrator");
  ros::NodeHandle nh_public;
  ros::NodeHandle private_nh("~");

  std::string encoder_name;
  std::string projector_yaml_directory;
  std::string save_folder;
  std::string projector_parameters_file;

  private_nh.param<std::string>("encoder_name", encoder_name, encoder_name);
  private_nh.param<std::string>("projector_yaml_directory", projector_yaml_directory, projector_yaml_directory);
  private_nh.param<std::string>("save_folder", save_folder, save_folder);
  private_nh.param<std::string>("projector_parameters_file", projector_parameters_file, projector_parameters_file);

  // Setup encoder
  auto encoder_ptr = CodecFactory::GetInstanceEncoder(encoder_name, private_nh);

  if (!encoder_ptr)
  {
    std::string info = "[GeneratePatternsNode] Failed to load patterns from encoder with name " + encoder_name;
    ROS_INFO("%s", info.c_str());
    return 0;
  }

  // Generate patterns
  unsigned int screen_cols, screen_rows;
  GetProjectorResolution(projector_yaml_directory, screen_rows, screen_cols);
  bool is_diamond_pixel = GetIsDiamondPixel(projector_yaml_directory);
  ProjectorParameters projector_parameters;

  if (projector_parameters.Load(projector_parameters_file))
  {
    ROS_INFO("Projector parameters successfully loaded");
    std::cout << "Projector parameters: " << std::endl;
    std::cout << projector_parameters << std::endl;
  }
  else
  {
    ROS_INFO("Failed to load projector parameters");
    return 0;
  }

  cv::Mat map1, map2;
  cv::Size map_size = cv::Size(screen_cols, screen_rows);
  Encoder::InitDistortMap(projector_parameters.intrinsic_mat(), projector_parameters.lens_distortion(), map_size, map1,
                          map2);

  auto patterns = encoder_ptr->GetEncodingPatterns();

  // For each pattern, apply lens distortion, diamond downsample if necessary and then save
  for (unsigned int i = 0; i < patterns.size(); i++)
  {
    cv::Mat current_pattern = patterns[i].clone();

    // general repmat
    current_pattern =
        cv::repeat(current_pattern, screen_rows / current_pattern.rows + 1, screen_cols / current_pattern.cols + 1);
    current_pattern = current_pattern(cv::Range(0, screen_rows), cv::Range(0, screen_cols));

    // correct for lens distortion
    cv::remap(current_pattern, current_pattern, map1, map2, CV_INTER_CUBIC);

    if (is_diamond_pixel)
    {
      current_pattern = Encoder::DiamondDownsample(current_pattern);
    }

    std::string format_str = save_folder + "%d.bmp";
    cv::imwrite(cv::format(format_str.c_str(), i), current_pattern);
  }

  std::string info = "[GeneratePatternsNode] Patterns from encoder " + encoder_name +
                     " generated successfully to folder " + save_folder + ".";
  ROS_INFO("%s", info.c_str());

  return 0;
}