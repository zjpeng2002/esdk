#ifndef JPEG_RECORDER_H
#define JPEG_RECORDER_H

#include <string>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <sys/stat.h>
#include <ctime>
#include "image_processor.h"
#include "logger.h"
#include "util_misc.h"
#include "liveview/sample_liveview.h"
char current_path_[128];

using namespace cv; 
namespace edge_app {

class JpegRecordProcessor : public ImageProcessor {
   public:
    JpegRecordProcessor(const std::string& name, std::shared_ptr<LiveviewSample> live_sample) 
        : name_(name), liveview_sample_(live_sample) {
        snprintf(file_path_, sizeof(file_path_), "%s../../build/%s",
                 current_path_, "video2jpeg");
        char cmd[532];
        snprintf(cmd, sizeof(cmd), "[ -d %s] || mkdir %s -p", file_path_,
                 file_path_);
        auto ret = system(cmd);
        if (ret != 0) {
            WARN("mkdir %s failed", file_path_);
        }
        INFO("jpegaa recorder init successfully");
    }

    ~JpegRecordProcessor() override {};

    void Process(const std::shared_ptr<Image> image) override {
        std::string h = std::to_string(image->size().width);
        std::string w = std::to_string(image->size().height);
        std::string osd = h + "x" + w;
        if (liveview_sample_) {
            auto kbps = liveview_sample_->GetStreamBitrate();
            osd += std::string(",") + std::to_string(kbps) + std::string("kbps");
        }
        putText(*image, osd, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1,
                cv::Scalar(0, 0, 255), 3);

        frame_counter_++;
        if (frame_counter_ > 150) {
            frame_counter_ = 0;
            char buf[32];
            auto now = time(NULL);
            strftime(buf, sizeof(buf), "_%Y-%m-%d-%H-%M-%S", localtime(&now));
            std::string file = file_path_ + std::string("/") + name_ +
                               std::string(buf) + ".jpg";
            INFO("write image: %s", file.c_str());
            cv::imwrite(file.c_str(), *image);
        }
    }

   private:
    std::shared_ptr<LiveviewSample> liveview_sample_;
    uint32_t frame_counter_ = 0;
    std::string name_;
    char file_path_[256];
};

class JpegRecorder {
public:
    static std::shared_ptr<ImageProcessor> Create(const std::string& name, 
        std::shared_ptr<LiveviewSample> live_sample) {
        return std::make_shared<JpegRecordProcessor>(name, live_sample);
    }
};
} // namespace edge_app

#endif