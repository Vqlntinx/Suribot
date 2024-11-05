#pragma once
// voice_tracker.h
#ifndef VOICE_TRACKER_H
#define VOICE_TRACKER_H

#include <dpp/dpp.h>
#include <unordered_map>
#include <chrono>

class VoiceTracker {
public:
    VoiceTracker(dpp::cluster& bot);
    ~VoiceTracker();
    void handle_voice_state_update(const dpp::voice_state_update_t& event);
    void saveUserData();
    void loadUserData();

private:
    dpp::cluster& bot;
    std::unordered_map<dpp::snowflake, std::chrono::time_point<std::chrono::system_clock>> userJoinTimes;
    std::unordered_map<dpp::snowflake, int> userTotalTimes;
};

#endif // VOICE_TRACKER_H