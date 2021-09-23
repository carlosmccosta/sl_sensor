// Code adapted from SLStudio https://github.com/jakobwilm/slstudio

#include "sl_sensor_codec/decoder.hpp"

#include <tuple>

namespace sl_sensor
{
namespace codec
{
Decoder::Decoder(unsigned int screen_cols, unsigned int screen_rows, CodecDirection dir)
  : number_patterns_(0), screen_cols_(screen_cols), screen_rows_(screen_rows), direction_(dir)
{
}

Decoder::Decoder(const YAML::Node& node)
{
  InitFromYAMLNode(node);
}

unsigned int Decoder::GetNumberPatterns() const
{
  return number_patterns_;
}

CodecDirection Decoder::GetDirection() const
{
  return direction_;
}

void Decoder::SetFrames(std::vector<cv::Mat>& frames)
{
  frames_ = std::move(frames);
}

void Decoder::InitFromYAMLNode(const YAML::Node& node)
{
  std::tuple<unsigned int, unsigned int, CodecDirection> result = GetBasicCodecInformationFromYAMLNode(node);
  screen_rows_ = std::get<0>(result);
  screen_cols_ = std::get<1>(result);
  direction_ = std::get<2>(result);
}

void Decoder::SetFrame(const cv::Mat& frame, unsigned int number)
{
  frames_[number] = frame;
}

}  // namespace codec
}  // namespace sl_sensor