#include "sl_sensor_calibration/dual_camera_calibration_preparator.hpp"

#include "sl_sensor_calibration/calibration_utils.hpp"

#include <math.h>
#include <cmath>
#include <opencv2/sfm/triangulation.hpp>

namespace sl_sensor
{
namespace calibration
{
DualCameraCalibrationPreparator::DualCameraCalibrationPreparator(
    const ProjectorParameters& proj_params, const CameraParameters& pri_cam_params,
    const CameraParameters& sec_cam_params, unsigned int checkerboard_cols, unsigned int checkerboard_rows,
    double checkerboard_size, double projector_acceptance_tol)
  : proj_params_(proj_params)
  , pri_cam_params_(pri_cam_params)
  , sec_cam_params_(sec_cam_params)
  , checkerboard_cols_(checkerboard_cols)
  , checkerboard_rows_(checkerboard_rows)
  , checkerboard_size_(checkerboard_size)
  , projector_acceptance_tol_(projector_acceptance_tol)
{
  // Compute Projection Matrix of Primary Camera
  projection_matrix_pri_cam_ = cv::Mat(3, 4, CV_32F, cv::Scalar(0.0));
  cv::Mat(pri_cam_params.intrinsic_mat()).copyTo(projection_matrix_pri_cam_(cv::Range(0, 3), cv::Range(0, 3)));

  // Compute Projection Matrix of Projector wrt Primary Cam
  projection_matrix_projector_ = cv::Mat(proj_params_.intrinsic_mat()) * pri_cam_params_.GetExtrinsicMatrix();

  // Compute Projection Matrix of Secondary Cam wrt Primary Cam
  cv::Mat transformation_matrix_sec_cam_to_projector = sec_cam_params_.GetInverseTransformationMatrix();

  cv::Mat transformation_matrix_projector_to_pri_cam = pri_cam_params_.GetTransformationMatrix();

  cv::Mat transformation_matrix_sec_cam_to_pri_cam =
      transformation_matrix_sec_cam_to_projector * transformation_matrix_projector_to_pri_cam;

  projection_matrix_sec_cam_ = cv::Mat(sec_cam_params_.intrinsic_mat()) *
                               transformation_matrix_sec_cam_to_pri_cam(cv::Range(0, 3), cv::Range(0, 4));
}

bool DualCameraCalibrationPreparator::AddSingleCalibrationSequence(
    const cv::Mat& pri_camera_shading, const cv::Mat& pri_camera_mask, const cv::Mat& pri_up, const cv::Mat& pri_vp,
    const cv::Mat& sec_camera_shading, const cv::Mat& sec_camera_mask, const cv::Mat& sec_up, const cv::Mat& sec_vp,
    const std::string& label)
{
  cv::Size checkerboard_size(checkerboard_cols_, checkerboard_rows_);

  // Extract Checkerboard From Primary Camera, return false if fail
  std::vector<cv::Point2f> pri_camera_checkerboard_corners;
  bool pri_checkerboard_detected =
      FindCheckerboardAndRefineCorners(pri_camera_shading, checkerboard_size, pri_camera_checkerboard_corners);
  if (!pri_checkerboard_detected)
  {
    std::cout << "[Calibrator] No checkerboard detected for primary camera" << std::endl;
    return false;
  }

  // Extract Checkerboard from Secondary Camera, return false if fail
  std::vector<cv::Point2f> sec_camera_checkerboard_corners;
  bool sec_checkerboard_detected =
      FindCheckerboardAndRefineCorners(sec_camera_shading, checkerboard_size, sec_camera_checkerboard_corners);
  if (!sec_checkerboard_detected)
  {
    std::cout << "[Calibrator] No checkerboard detected for secondary camera" << std::endl;
    return false;
  }

  // Check that orientation of checkerboards are correct. If not, flip them
  OrientCheckerBoardCorners(pri_camera_checkerboard_corners);
  OrientCheckerBoardCorners(sec_camera_checkerboard_corners);

  // Generate a vector of checkerboard 3d points, from the top left corners and going row-wise downwards
  std::vector<cv::Point3f> all_checkerboard_3d_corners;
  for (unsigned int h = 0; h < checkerboard_rows_; h++)
  {
    for (unsigned int w = 0; w < checkerboard_cols_; w++)
    {
      all_checkerboard_3d_corners.push_back(cv::Point3f(checkerboard_size_ * w, checkerboard_size_ * h, 0.0));
    }
  }

  // Initialise some variables required for next few steps
  std::vector<cv::Point2f> current_projector_corners;
  std::vector<cv::Point2f> current_pri_camera_corners;
  std::vector<cv::Point2f> current_sec_camera_corners;
  std::vector<cv::Point3f> current_3d_corners;

  // std::vector<cv::Point3f> debug_3d_corners;

  // For each checkerboard corner
  for (unsigned int j = 0; j < all_checkerboard_3d_corners.size(); j++)
  {
    // Extract projector coordinate for primary camera, continue if fail
    const cv::Point2f& processed_pri_camera_corner = pri_camera_checkerboard_corners[j];
    std::vector<cv::Point2f> pri_neighbourhood_camera_coordinates, pri_neighbourhood_projector_coordinates;

    cv::Point2f pri_processed_projector_corner;
    bool pri_projector_coordinate_extracted = ExtractProjectorCoordinateUsingLocalHomography(
        processed_pri_camera_corner, pri_camera_mask, pri_up, pri_vp, window_radius_, minimum_valid_pixels_,
        pri_processed_projector_corner);

    if (!pri_projector_coordinate_extracted)
    {
      std::cout << "[Calibrator] Local Homography for primary projector failed for " << j << "th corner" << std::endl;
      continue;
    }

    // Extract projector coordinate for secondary camera, continue if fail
    const cv::Point2f& processed_sec_camera_corner = sec_camera_checkerboard_corners[j];
    std::vector<cv::Point2f> sec_neighbourhood_camera_coordinates, sec_neighbourhood_projector_coordinates;

    cv::Point2f sec_processed_projector_corner;
    bool sec_projector_coordinate_extracted = ExtractProjectorCoordinateUsingLocalHomography(
        processed_sec_camera_corner, sec_camera_mask, sec_up, sec_vp, window_radius_, minimum_valid_pixels_,
        sec_processed_projector_corner);

    if (!sec_projector_coordinate_extracted)
    {
      std::cout << "[Calibrator] Local Homography for secondary projector failed for " << j << "th corner" << std::endl;
      continue;
    }

    // Consistency check for using extracted projector coordinates, continue if fail

    // Simple script for visualising if corresponding corners from different cameras match
    /**
    cv::Mat pri_display = pri_up.clone();
    cv::Mat sec_display = sec_up.clone();

    cv::normalize(pri_display, pri_display, 0, 255, cv::NORM_MINMAX);
    pri_display.convertTo(pri_display, CV_8UC1);

    cv::normalize(sec_display, sec_display, 0, 255, cv::NORM_MINMAX);
    sec_display.convertTo(sec_display, CV_8UC1);

    cv::circle(pri_display, processed_pri_camera_corner, 5, cv::Scalar(255, 255, 255, 255));
    cv::circle(sec_display, processed_sec_camera_corner, 5, cv::Scalar(255, 255, 255, 255));

    cv::Mat display;  // These are the destination matrices
    cv::hconcat(pri_display, sec_display, display);
    cv::imshow("troubleshoot", display);
    cv::waitKey(0);
    **/

    if (!ProjectorCoordinatesConsistencyCheck(pri_processed_projector_corner, sec_processed_projector_corner))
    {
      // std::cout << "[Calibrator] Projector coordinates consistency check failed for " << j << "th corner" <<
      // std::endl;
      continue;
    }

    // Undistort 2D corners based on calibrated intrinsics of cameras and projector. Note: We do not triangulate with
    // projector coordinates from the secondary camera from the second with for now.

    auto undistorted_pri_cam_point =
        UndistortSinglePoint(processed_pri_camera_corner, cv::Mat(pri_cam_params_.intrinsic_mat()),
                             cv::Mat(pri_cam_params_.lens_distortion()));
    auto undistorted_sec_cam_point =
        UndistortSinglePoint(processed_sec_camera_corner, cv::Mat(sec_cam_params_.intrinsic_mat()),
                             cv::Mat(sec_cam_params_.lens_distortion()));
    auto undistorted_pri_proj_point = UndistortSinglePoint(
        pri_processed_projector_corner, cv::Mat(proj_params_.intrinsic_mat()), cv::Mat(proj_params_.lens_distortion()));
    // auto undistorted_sec_proj_point = UndistortSinglePoint(
    //    sec_processed_projector_corner, cv::Mat(proj_params_.intrinsic_mat()),
    //    cv::Mat(proj_params_.lens_distortion()));

    // Triangulate 3D coordinate of corner, wrt to primary camera

    // std::cout << projection_matrix_pri_cam_ << std::endl;
    // std::cout << projection_matrix_sec_cam_ << std::endl;
    // std::cout << projection_matrix_projector_ << std::endl;

    std::vector<cv::Mat> corner_points_vec;

    corner_points_vec.push_back(cv::Mat(undistorted_pri_cam_point));
    // corner_points_vec.push_back(cv::Mat(undistorted_sec_cam_point));
    corner_points_vec.push_back(cv::Mat(undistorted_pri_proj_point));
    // corner_points_vec.push_back(cv::Mat(undistorted_sec_proj_point));

    std::vector<cv::Mat> projection_matrix_vec = { projection_matrix_pri_cam_, projection_matrix_projector_ };

    // std::vector<cv::Mat> projection_matrix_vec = { projection_matrix_sec_cam_, projection_matrix_projector_ };

    cv::Mat triangulated_point_mat;
    cv::sfm::triangulatePoints(corner_points_vec, projection_matrix_vec, triangulated_point_mat);
    cv::Point3f triangulated_point(triangulated_point_mat);

    /**
    corner_points_vec.clear();

    corner_points_vec.push_back(cv::Mat(undistorted_sec_cam_point));
    corner_points_vec.push_back(cv::Mat(undistorted_sec_proj_point));
    projection_matrix_vec.clear();
    projection_matrix_vec = { projection_matrix_sec_cam_, projection_matrix_projector_ };
    cv::sfm::triangulatePoints(corner_points_vec, projection_matrix_vec, triangulated_point_mat);
    cv::Point3f triangulated_point_debug(triangulated_point_mat);
    **/

    // Append information to storage vectors
    // Note: We store the distorted observations since the reprojection error in the bundle adjustment will take lens
    // distortion into account
    current_projector_corners.push_back(pri_processed_projector_corner);
    current_pri_camera_corners.push_back(processed_pri_camera_corner);
    current_sec_camera_corners.push_back(processed_sec_camera_corner);
    current_3d_corners.push_back(triangulated_point);

    // debug_3d_corners.push_back(triangulated_point_debug);
  }

  // Store results to storage for this calibration sequence
  if (!current_3d_corners.empty())
  {
    corner_projector_coordinates_storage_.push_back(current_projector_corners);
    corner_pri_camera_coordinates_storage_.push_back(current_pri_camera_corners);
    corner_sec_camera_coordinates_storage_.push_back(current_sec_camera_corners);
    corner_3d_coordinates_storage_.push_back(current_3d_corners);
    sequence_label_storage_.push_back(label);

    /**
    for (const auto& corner : current_3d_corners)
    {
      std::cout << corner << std::endl;
    }
    **/

    /**
    cv::viz::Viz3d window;  // creating a Viz window
    // Displaying the Coordinate Origin (0,0,0)
    window.showWidget("coordinate", cv::viz::WCoordinateSystem(100));
    // Displaying the 3D points in green
    window.showWidget("points1", cv::viz::WCloud(current_3d_corners, cv::viz::Color::green()));
    // Displaying the 3D points in green
    window.showWidget("points2", cv::viz::WCloud(debug_3d_corners, cv::viz::Color::red()));
    window.spin();
    **/
  }

  return (!current_3d_corners.empty()) ? true : false;
}

bool DualCameraCalibrationPreparator::ProjectorCoordinatesConsistencyCheck(const cv::Point2f& coord_1,
                                                                           const cv::Point2f& coord_2)
{
  // std::cout << std::sqrt(pow(coord_1.x - coord_2.x, 2.0) + pow(coord_1.y - coord_2.y, 2.0)) << std::endl;
  return std::sqrt(pow(coord_1.x - coord_2.x, 2.0) + pow(coord_1.y - coord_2.y, 2.0)) < projector_acceptance_tol_;
}

void DualCameraCalibrationPreparator::SetLocalHomographySettings(unsigned int window_radius,
                                                                 unsigned int minimum_valid_pixels)
{
  window_radius_ = window_radius;
  minimum_valid_pixels_ = minimum_valid_pixels;
}

void DualCameraCalibrationPreparator::Reset()
{
  corner_pri_camera_coordinates_storage_.clear();
  corner_sec_camera_coordinates_storage_.clear();
  corner_projector_coordinates_storage_.clear();
  corner_3d_coordinates_storage_.clear();
  sequence_label_storage_.clear();
}

void DualCameraCalibrationPreparator::WriteExtrinsics(std::ofstream& ba_file, const cv::Mat& rvec, const cv::Mat& tvec)
{
  // Rotation vector (Rodrigues format)
  ba_file << rvec.at<float>(0, 0) << "\n" << rvec.at<float>(1, 0) << "\n" << rvec.at<float>(2, 0) << "\n";

  // Translation vector
  ba_file << tvec.at<float>(0, 0) << "\n" << tvec.at<float>(1, 0) << "\n" << tvec.at<float>(2, 0) << "\n";
}

void DualCameraCalibrationPreparator::WriteIntrinsics(std::ofstream& ba_file,
                                                      const IntrinsicParameters& intrinsic_params)
{
  // Intrinsics
  cv::Mat intrinsic_mat = cv::Mat(intrinsic_params.intrinsic_mat());

  // x focal length in pixel
  ba_file << intrinsic_mat.at<float>(0, 0) << "\n";

  // y focal length in pixels
  ba_file << intrinsic_mat.at<float>(1, 1) << "\n";

  // image center x
  ba_file << intrinsic_mat.at<float>(0, 2) << "\n";

  // image center y
  ba_file << intrinsic_mat.at<float>(1, 2) << "\n";

  // Lens distortion
  auto lens_distortion = intrinsic_params.lens_distortion();

  // radial distortion coefficient 1
  ba_file << lens_distortion[0] << "\n";

  // radial distortion coefficient 2
  ba_file << lens_distortion[1] << "\n";

  // radial distortion coefficient 3
  ba_file << lens_distortion[4] << "\n";

  // tangential distortion coefficient 1
  ba_file << lens_distortion[2] << "\n";

  // tangential distortion coefficient 2
  ba_file << lens_distortion[3] << "\n";
}

void DualCameraCalibrationPreparator::ExportFile(const std::string& filename)
{
  // Open file for writing
  std::ofstream ba_file;
  ba_file.open(filename);

  // Compute total number of points
  int number_points = 0;
  for (const auto& points_per_view : corner_3d_coordinates_storage_)
  {
    number_points += points_per_view.size();
  }

  // 1) First line of file is number of cameras, number of points, number of fixed points, number of observations,
  // number of fixed observations. Note that we do not have any fixed points / observations
  ba_file << 3 << " " << number_points << " " << 0 << " " << number_points * 3 << " " << 0 << "\n";

  // 2) Next n_obs rows are [camera index, point id, camera coord x, camera coord y]
  int point_id = 0;
  for (size_t i = 0; i < corner_3d_coordinates_storage_.size(); i++)
  {
    std::vector<cv::Point2f>& pri_cam_coords_current_frame = corner_pri_camera_coordinates_storage_.at(i);
    std::vector<cv::Point2f>& proj_cam_coords_current_frame = corner_projector_coordinates_storage_.at(i);
    std::vector<cv::Point2f>& sec_cam_coords_current_frame = corner_sec_camera_coordinates_storage_.at(i);

    for (size_t j = 0; j < proj_cam_coords_current_frame.size(); j++)
    {
      // Primary camera (camera index 0)
      ba_file << 0 << " " << point_id << " " << pri_cam_coords_current_frame.at(j).x << " "
              << pri_cam_coords_current_frame.at(j).y << "\n";

      // Projector (camera index 1)
      ba_file << 1 << " " << point_id << " " << proj_cam_coords_current_frame.at(j).x << " "
              << proj_cam_coords_current_frame.at(j).y << "\n";

      // Secondary camera (camera index 2)
      ba_file << 2 << " " << point_id << " " << sec_cam_coords_current_frame.at(j).x << " "
              << sec_cam_coords_current_frame.at(j).y << "\n";

      // Update point_id
      point_id++;
    }
  }

  // 3) Next n_fobs rows are [camera index, point id, camera cood x, camera coord y] (we have no fixed points so we do
  // not add anything)

  // 4) After all observations have been listed, we list out all camera parameters

  // Primary camera
  cv::Mat rvec_pri_cam = cv::Mat(3, 1, CV_32F);
  cv::Mat tvec_pri_cam = cv::Mat::zeros(3, 1, CV_32F);
  cv::Rodrigues(cv::Mat::eye(3, 3, CV_32F), rvec_pri_cam);

  WriteExtrinsics(ba_file, rvec_pri_cam, tvec_pri_cam);
  WriteIntrinsics(ba_file, pri_cam_params_);

  // Projector
  cv::Mat rvec_projector = cv::Mat(3, 1, CV_32F);
  cv::Mat tvec_projector = cv::Mat(pri_cam_params_.extrinsic_trans());  // Pri cam params because we want the extrinsics
                                                                        // between pri cam and projector
  cv::Rodrigues(cv::Mat(pri_cam_params_.extrinsic_rot()), rvec_projector);

  WriteExtrinsics(ba_file, rvec_projector, tvec_projector);
  WriteIntrinsics(ba_file, proj_params_);

  // Secondary camera
  cv::Mat transformation_matrix_sec_cam_to_projector = sec_cam_params_.GetInverseTransformationMatrix();
  cv::Mat transformation_matrix_projector_to_pri_cam = pri_cam_params_.GetTransformationMatrix();
  cv::Mat transformation_matrix_sec_cam_to_pri_cam =
      transformation_matrix_sec_cam_to_projector * transformation_matrix_projector_to_pri_cam;

  cv::Mat rvec_sec_cam = cv::Mat(3, 1, CV_32F);
  cv::Mat sec_cam_rot_matrix = cv::Mat(3, 3, CV_32F);
  transformation_matrix_sec_cam_to_pri_cam(cv::Range(0, 3), cv::Range(0, 3))
      .copyTo(sec_cam_rot_matrix(cv::Range(0, 3), cv::Range(0, 3)));
  cv::Rodrigues(sec_cam_rot_matrix, rvec_sec_cam);

  cv::Mat tvec_sec_cam = cv::Mat(3, 1, CV_32F);
  transformation_matrix_sec_cam_to_pri_cam(cv::Range(0, 3), cv::Range(3, 4))
      .copyTo(tvec_sec_cam(cv::Range(0, 3), cv::Range(0, 1)));

  WriteExtrinsics(ba_file, rvec_sec_cam, tvec_sec_cam);
  WriteIntrinsics(ba_file, sec_cam_params_);

  // 5) After camera parameters, print out 3d coordinates of points

  for (size_t i = 0; i < corner_3d_coordinates_storage_.size(); i++)
  {
    for (size_t j = 0; j < corner_3d_coordinates_storage_[i].size(); j++)
    {
      ba_file << corner_3d_coordinates_storage_[i][j].x << "\n";
      ba_file << corner_3d_coordinates_storage_[i][j].y << "\n";
      ba_file << corner_3d_coordinates_storage_[i][j].z << "\n";
    }
  }

  // 6) Followed by fixed 3D points (not applicable)

  // Close file
  ba_file.close();
}

}  // namespace calibration

}  // namespace sl_sensor