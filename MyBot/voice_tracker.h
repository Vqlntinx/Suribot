#pragma once
#ifndef VOICE_TRACKER_H
#define VOICE_TRACKER_H
#include <vector>
#include <algorithm>
#include <iostream>
#include <dpp/dpp.h>
#include <unordered_map>
#include <chrono>
#include <fstream>
#include <DPP/nlohmann/json.hpp>  // JSON 라이브러리 필요

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