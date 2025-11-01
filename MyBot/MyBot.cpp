#include "MyBot.h"
#include "voice_tracker.h"

/* Be sure to place your token in the line below.
 * Follow steps here to get a token:
 * https://dpp.dev/creating-a-bot-application.html
 * When you invite the bot, be sure to invite it with the 
 * scopes 'bot' and 'applications.commands', e.g.
 * https://discord.com/oauth2/authorize?client_id=940762342495518720&scope=bot+applications.commands&permissions=139586816064
 */
const std::string    BOT_TOKEN    = "";

int main()
{
	
	dpp::cluster bot(BOT_TOKEN);

	
	bot.on_log(dpp::utility::cout_logger());

	VoiceTracker voice_tracker(bot);

	bot.on_voice_state_update([&voice_tracker](const dpp::voice_state_update_t& event) {
		voice_tracker.handle_voice_state_update(event);
		});

	bot.on_slashcommand([&voice_tracker](const dpp::slashcommand_t& event) {
    std::string command_name = event.command.get_command_name();

    if (command_name == "순위") {
        voice_tracker.handleTopUsersCommand(event);
    }
    else if (command_name == "업타임") {
        auto param = event.get_parameter("유저");
        if (std::holds_alternative<dpp::snowflake>(param)) { // 올바른 형식인지 확인
            dpp::snowflake user_id = std::get<dpp::snowflake>(param);
            voice_tracker.getUserUptime(event, user_id);
        }
        else {
            event.reply("유효한 유저를 선택해 주세요.");
        }
    }
});



    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(
                dpp::slashcommand("순위", "누적 시간 상위 5명 사용자 목록을 표시합니다.", bot.me.id)
            );
            
            dpp::slashcommand uptime_cmd("업타임", "특정 사용자의 음성 채널 업타임을 조회합니다.", bot.me.id);
            uptime_cmd.add_option(
                dpp::command_option(dpp::co_user, "유저", "업타임을 확인할 사용자", true)
            );
            bot.global_command_create(uptime_cmd);

            bot.log(dpp::ll_info, "명령어 등록 완료");
        }
        });



	bot.start(dpp::st_wait);

	return 0;
}
