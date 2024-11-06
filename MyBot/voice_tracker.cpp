#include "voice_tracker.h"
const dpp::snowflake VOICE_LOG_CHANNEL_ID = 1303441956520722553;

VoiceTracker::VoiceTracker(dpp::cluster& bot) : bot(bot)
{
	loadUserData();
}

VoiceTracker::~VoiceTracker()
{
    saveUserData();
}
void VoiceTracker::handle_voice_state_update(const dpp::voice_state_update_t& event) {
    dpp::snowflake user_id = event.state.user_id;
    dpp::snowflake channel_id = event.state.channel_id;

    if (channel_id && userJoinTimes.find(user_id) == userJoinTimes.end()) {
        
        userJoinTimes[user_id] = std::chrono::system_clock::now();
        userChannelNames[user_id] = "알 수 없음"; 

        
        bot.channel_get(channel_id, [this, user_id](const dpp::confirmation_callback_t& callback) {
            if (!callback.is_error()) {
                const dpp::channel& channel = std::get<dpp::channel>(callback.value);
                userChannelNames[user_id] = "<#" + std::to_string(channel.id) + ">"; 
            }
            else {
                bot.log(dpp::ll_error, "채널 정보를 가져오는 데 실패했습니다.");
            }
            });

        bot.log(dpp::ll_info, "User joined the voice channel.");

    }
    else if (!channel_id && userJoinTimes.find(user_id) != userJoinTimes.end()) {
        
        auto joinTime = userJoinTimes[user_id];
        auto leaveTime = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(leaveTime - joinTime);

        userTotalTimes[user_id] += duration.count();

        
        std::string channel_name = userChannelNames[user_id];
        std::string message = "유저: <@" + std::to_string(user_id) + ">   채널명: " + channel_name + "   업타임: " + std::to_string(duration.count()) + "분";

        bot.message_create(dpp::message(VOICE_LOG_CHANNEL_ID, message));

        userJoinTimes.erase(user_id);
        userChannelNames.erase(user_id);

        saveUserData();
        bot.log(dpp::ll_info, "User left the voice channel.");
    }
}




void VoiceTracker::saveUserData() {
    nlohmann::json jsonData;
    for (const auto& [user_id, totalTime] : userTotalTimes) {
        jsonData[std::to_string(user_id)] = totalTime;
    }
    std::ofstream file("user_data.json");
    file << jsonData.dump(4);
}

void VoiceTracker::loadUserData() {
    std::ifstream file("user_data.json");
    if (file.is_open()) {
        nlohmann::json jsonData;
        file >> jsonData;
        for (auto& [key, value] : jsonData.items()) {
            userTotalTimes[std::stoull(key)] = value;
        }
    }
}

void VoiceTracker::handleTopUsersCommand(const dpp::slashcommand_t& event) {
    std::vector<std::pair<dpp::snowflake, int>> userTimes(userTotalTimes.begin(), userTotalTimes.end());
    std::sort(userTimes.begin(), userTimes.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
        });
    std::string message = "이번 주 최고의 망령:\n";
    int rank = 1;
    for (const auto& [user_id, totalTime] : userTimes) {
        message += "Rank " + std::to_string(rank) + ": <@" + std::to_string(user_id) + "> - " +
            std::to_string(totalTime) + " 분\n";
        if (++rank > 5) break;
    }
    event.reply(message);
}

void VoiceTracker::getUserUptime(const dpp::slashcommand_t& event, dpp::snowflake user_id) {
    bot.log(dpp::ll_info, "getUserUptime 호출됨. 입력된 user_id: " + std::to_string(user_id));

	if (user_id == 0) {
        event.reply("잘못된 사용자 ID입니다. 다시 시도해 주세요.");
        return;
    }

    auto it = userTotalTimes.find(user_id);
    if (it != userTotalTimes.end()) {
        int totalTime = it->second;
        std::string message = "<@" + std::to_string(user_id) + ">님의 총 업타임: " + std::to_string(totalTime) + " 분";
        event.reply(message);
    }
    else {
        event.reply("<@" + std::to_string(user_id) + ">님의 업타임 기록이 없습니다.");
    }
}

void VoiceTracker::clearUserData() { //유저 데이터 초기화 메소드
    userTotalTimes.clear();
    saveUserData();
}