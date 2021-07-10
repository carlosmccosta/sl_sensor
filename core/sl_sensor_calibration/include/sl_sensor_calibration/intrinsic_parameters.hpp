#pragma once

#include <opencv2/opencv.hpp>
#include <string>

namespace sl_sensor
{
namespace calibration
{
class IntrinsicParameters
{
public:
  IntrinsicParameters(const cv::Matx33f& intrinsic_mat, const cv::Vec<float, 5>& lens_distortion,
                      double calibration_error, int resolution_x, int resolution_y);

  IntrinsicParameters(const std::string& filename);

  IntrinsicParameters(){};

  bool Save(const std::string& filename);

  bool Load(const std::string& filename);

  const cv::Matx33f& intrinsic_mat() const;
  const cv::Vec<float, 5>& lens_distortion() const;
  const double& calibration_error() const;
  const int& resolution_x() const;
  const int& resolution_y() const;

protected:
  cv::Matx33f intrinsic_mat_ = cv::Matx33f::eye();
  cv::Vec<float, 5> lens_distortion_ = cv::Vec<float, 5>(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  double calibration_error_ = 0.0f;
  int resolution_x_ = 0;
  int resolution_y_ = 0;

  static std::string GetFileExtension(const std::string& filename);
};

std::ostream& operator<<(std::ostream& os, const IntrinsicParameters& dt);

}  // namespace calibration

}  // namespace sl_sensor