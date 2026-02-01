#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include "../voice/story_bank.h"

namespace brain {

class StoryGenerator {
public:
    StoryGenerator(voice::StoryBank& bank);
    ~StoryGenerator();

    // Set the base URL for the LLM service (e.g. http://localhost:8080)
    void setBaseUrl(const std::string& url);

    // Trigger a background generation for the given mood
    void requestStory(const std::string& moodId, const std::string& context);

    // Poll for completed stories to add to the bank (called from main thread)
    void update();

private:
    voice::StoryBank& bank_;
    std::string baseUrl_ = "http://localhost:8080";
    
    struct GenRequest {
        std::string mood;
        std::string context;
    };
    
    std::atomic<bool> generating_ = false;
    std::thread genThread_;

    void runGeneration(GenRequest req);
};

} // namespace brain
