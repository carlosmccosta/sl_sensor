#pragma once

#include <opencv2/opencv.hpp>

namespace sl_sensor
{
namespace calibration
{
void OrientCheckerBoardCorners(std::vector<cv::Point2f>& corners);

bool FindCheckerboardAndRefineCorners(const cv::Mat& image, const cv::Size& checkerboard_size,
                                      std::vector<cv::Point2f>& checkerboard_corners);

void GetNeighbourhoodPoints(const cv::Point2f& cam_coordinate, const cv::Mat& mask, const cv::Mat& up,
                            const cv::Mat& vp, std::vector<cv::Point2f>& neighbourhood_camera_coordinates,
                            std::vector<cv::Point2f>& neighbourhood_projector_coordinates, unsigned int window_radius);

bool ExtractProjectorCoordinateUsingLocalHomography(const cv::Point2f& cam_coordinate, const cv::Mat& mask,
                                                    const cv::Mat& up, const cv::Mat& vp, unsigned int window_radius,
                                                    unsigned int minimum_valid_pixels,
                                                    cv::Point2f& output_proj_coordinate);

}  // namespace calibration

}  // namespace sl_sensor