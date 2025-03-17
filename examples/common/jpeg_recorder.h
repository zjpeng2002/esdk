#ifndef JPEG_RECORDER_H
#define JPEG_RECORDER_H

#include <string>
#include <memory>
#include <opencv2/core.hpp>
#include "image_processor.h"

class LiveviewSample;

class JpegRecorder {
public:
    static std::shared_ptr<ImageProcessor> Create(const std::string& name, 
        std::shared_ptr<LiveviewSample> live_sample);
};

#endif // JPEG_RECORDER_H
