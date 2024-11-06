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
#include <string>

class VoiceTracker {
public:
    VoiceTracker(dpp::cluster& bot);
    ~VoiceTracker();
    void handle_voice_state_update(const dpp::voice_state_update_t& event);
    void saveUserData();
    void loadUserData();
    void handleTopUsersCommand(const dpp::slashcommand_t& event);
    void getUserUptime(const dpp::slashcommand_t& event, dpp::snowflake user_id);
    void clearUserData();

private:
    dpp::cluster& bot;
    std::unordered_map<dpp::snowflake, std::chrono::time_point<std::chrono::system_clock>> userJoinTimes;
    std::unordered_map<dpp::snowflake, std::string> userChannelNames;
	std::unordered_map<dpp::snowflake, int> userTotalTimes;
    std::unordered_map<dpp::snowflake, std::string> userNames;
};

#endif // VOICE_TRACKER_H