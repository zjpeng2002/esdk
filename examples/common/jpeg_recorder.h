#ifndef JPEG_RECORDER_H
#define JPEG_RECORDER_H

#include <string>
#include <memory>
#include <opencv2/core.hpp>
#include "image_processor.h"

namespace edge_app {
class JpegRecorder {
public:
    static std::shared_ptr<ImageProcessor> Create(const std::string& name, 
        std::shared_ptr<LiveviewSample> live_sample);
};
} // namespace edge_app

#endif