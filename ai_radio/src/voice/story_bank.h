#pragma once

#include <string>
#include <vector>
#include <random>
#include <optional>
#include <mutex>
#include <memory>
#include "../audio/stem_player.h"

namespace voice {

struct Story {
    std::string id;
    std::string text;
    std::string audioFile;
    std::string moodId;
    
    // Audio data (pre-loaded)
    audio::StemPlayer player;
    
    // Runtime state
    float lastPlayedTime = -9999.0f; 
};

class StoryBank {
public:
    StoryBank();

    // Load stories from a JSON config file and pre-load audio.
    bool loadFromFile(const std::string& path);

    // Pick a valid story for the current mood and time.
    std::shared_ptr<Story> pickStory(const std::string& currentMoodId, float currentTime, float globalCooldown);

    // Mark a story as played right now.
    void markPlayed(std::shared_ptr<Story> story, float currentTime);

    // Add a dynamic story at runtime
    void addStory(std::shared_ptr<Story> story);
    
    size_t countForMood(const std::string& moodId);

private:
    std::vector<std::shared_ptr<Story>> stories_;
    std::mt19937 rng_;
    mutable std::mutex mutex_; 
};

} // namespace voice
