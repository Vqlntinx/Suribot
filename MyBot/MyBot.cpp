#include "MyBot.h"
#include "voice_tracker.h"

/* Be sure to place your token in the line below.
 * Follow steps here to get a token:
 * https://dpp.dev/creating-a-bot-application.html
 * When you invite the bot, be sure to invite it with the 
 * scopes 'bot' and 'applications.commands', e.g.
 * https://discord.com/oauth2/authorize?client_id=940762342495518720&scope=bot+applications.commands&permissions=139586816064
 */
const std::string    BOT_TOKEN    = "MTMwMjg0MTkxNjcxNDM4NTUwMQ.GLSPvb.77GttuY_7m--s01nARcVzWqR_xJNCrHYyPidHY";

int main()
{
	/* Create bot cluster */
	dpp::cluster bot(BOT_TOKEN);

	/* Output simple log messages to stdout */
	bot.on_log(dpp::utility::cout_logger());

	VoiceTracker voice_tracker(bot);
	bot.on_voice_state_update([&voice_tracker](const dpp::voice_state_update_t& event) {
		voice_tracker.handle_voice_state_update(event);
		});

	/* Start the bot */
	bot.start(dpp::st_wait);

	return 0;
}
