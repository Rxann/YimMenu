#include "reaction.hpp"

#include "backend/player_command.hpp"
#include "core/globals.hpp"
#include "fiber_pool.hpp"
#include "hooking.hpp"
#include "pointers.hpp"
#include "script.hpp"
#include "services/player_database/player_database_service.hpp"
#include "util/notify.hpp"


namespace big
{
	reaction::reaction(const char* event_name, const char* notify_message, const char* announce_message) :
	    m_event_name(event_name),
	    m_notify_message(notify_message),
	    m_announce_message(announce_message)
	{
	}

	void reaction::process_common(player_ptr player)
	{
		if (log)
		{
			uint64_t rockstar_id = player->get_net_data() == nullptr ? 0 : player->get_net_data()->m_gamer_handle.m_rockstar_id;
			LOG(WARNING) << std::format("Received {} from {} ({})", m_event_name, player->get_name(), rockstar_id);
		}

		if (add_to_player_db)
		{
			auto entry = g_player_database_service->get_or_create_player(player);

			if (block_joins)
			{
				entry->block_join = true;
				g_player_database_service->save();
			}
		}

		if (kick)
		{
			g_fiber_pool->queue_job([player] {
				if (g.settings.default_reaction_kick)
				{
					dynamic_cast<player_command*>(command::get(RAGE_JOAAT("bailkick")))->call(player, {});
					dynamic_cast<player_command*>(command::get(RAGE_JOAAT("nfkick")))->call(player, {});
					script::get_current()->yield(700ms);
					dynamic_cast<player_command*>(command::get(RAGE_JOAAT("breakup")))->call(player, {});
				}
				else if (g.settings.breakup_reaction_kick)
				{
					dynamic_cast<player_command*>(command::get(RAGE_JOAAT("breakup")))->call(player, {});
				}
				else if (g.settings.end_reaction_kick)
				{
					dynamic_cast<player_command*>(command::get(RAGE_JOAAT("endkick")))->call(player, {});
					script::get_current()->yield(700ms);
					if (player->is_valid())
					{
						g_notification_service->push_error("End Kick Failure", "End kicks are blocked by most menus");
					}
				}
				else if (g.settings.nf_reaction_kick)
				{
					dynamic_cast<player_command*>(command::get(RAGE_JOAAT("nfkick")))->call(player, {});
					script::get_current()->yield(15s);
					if (player->is_valid())
					{
						g_notification_service->push_error("Null Function Kick Failed", "Attempting Try 2. Breakup kick will be used if kick fails again...");
						dynamic_cast<player_command*>(command::get(RAGE_JOAAT("nfkick")))->call(player, {});
						script::get_current()->yield(700ms);
						if (player->is_valid())
						{
							dynamic_cast<player_command*>(command::get(RAGE_JOAAT("breakup")))->call(player, {});
							g_notification_service->push_warning("Breakup Kick Sent", "A breakup kick has been sent due to Null Function failed attempts.");
						}
					}
				}
			});
		}
	}

	void reaction::process(player_ptr player)
	{
		if (!player->is_valid())
			return;

		if (announce_in_chat)
		{
			g_fiber_pool->queue_job([player, this] {
				char chat[255];
				snprintf(chat,
				    sizeof(chat),
				    std::format("{} {}", g.session.chat_output_prefix, m_announce_message).data(),
				    player->get_name());

				if (g_hooking->get_original<hooks::send_chat_message>()(*g_pointers->m_gta.m_send_chat_ptr,
				        g_player_service->get_self()->get_net_data(),
				        chat,
				        false))
					notify::draw_chat(chat, g_player_service->get_self()->get_name(), false);
			});
		}

		if (notify)
		{
			char notification[500]{}; // I don't like using sprintf but there isn't an alternative afaik
			snprintf(notification, sizeof(notification), m_notify_message, player->get_name());
			g_notification_service->push_warning("Protections", notification);
		}

		process_common(player);
	}
}