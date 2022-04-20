#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "Player.h"
#include "Object.h"
#include "DataMap.h"

#if AC_COMPILER == AC_COMPILER_GNU
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

using namespace Acore::ChatCommands;

/*
Coded by Talamortis - For Azerothcore
Thanks to Rochet for the assistance
*/

bool IndividualXpEnabled;
bool IndividualXpAnnounceModule;
float MaxRate;
float DefaultRate;

class Individual_XP_conf : public WorldScript
{
public:
    Individual_XP_conf() : WorldScript("Individual_XP_conf_conf") { }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        IndividualXpAnnounceModule = sConfigMgr->GetOption<bool>("IndividualXp.Announce", 1);
        IndividualXpEnabled = sConfigMgr->GetBoolDefault("IndividualXp.Enabled", 1);
        MaxRate = sConfigMgr->GetIntDefault("MaxXPRate", 10.0f);
        DefaultRate = sConfigMgr->GetIntDefault("DefaultXPRate", 1.0f);
    }
};

class Individual_Xp_Announce : public PlayerScript
{
public:

    Individual_Xp_Announce() : PlayerScript("Individual_Xp_Announce") {}

    void OnLogin(Player* player)
    {
        // Announce Module
        if (IndividualXpEnabled & IndividualXpAnnounceModule)
        {
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00IndividualXpRate |rmodule");
        }
    }
};

class PlayerXpRate : public DataMap::Base
{
public:
    PlayerXpRate() {}
    PlayerXpRate(float XPRate) : XPRate(XPRate) {}
    float XPRate = 1.0f;
};

class Individual_XP : public PlayerScript
{
public:
    Individual_XP() : PlayerScript("Individual_XP") { }

    void OnLogin(Player* p) override
    {
        QueryResult result = CharacterDatabase.Query("SELECT `XPRate` FROM `individualxp` WHERE `CharacterGUID` = '{}'", p->GetGUID().GetCounter());
        if (!result)
        {
            p->CustomData.GetDefault<PlayerXpRate>("Individual_XP")->XPRate = DefaultRate;
        }
        else
        {
            Field* fields = result->Fetch();
            p->CustomData.Set("Individual_XP", new PlayerXpRate(fields[0].Get<float>()));
        }
    }

    void OnLogout(Player* p) override
    {
        if (PlayerXpRate* data = p->CustomData.Get<PlayerXpRate>("Individual_XP"))
        {
            float rate = data->XPRate;
            CharacterDatabase.DirectExecute("REPLACE INTO `individualxp` (`CharacterGUID`, `XPRate`) VALUES ('{}', '{}');", p->GetGUID().GetCounter(), rate);
        }
    }

    void OnGiveXP(Player* p, uint32& amount, Unit* /*victim*/) override
    {
        if (IndividualXpEnabled) {
            if (PlayerXpRate* data = p->CustomData.Get<PlayerXpRate>("Individual_XP"))
                amount = (uint32)(amount * data->XPRate);
        }
    }

};

class Individual_XP_command : public CommandScript
{
public:
    Individual_XP_command() : CommandScript("Individual_XP_command") { }
    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> IndividualXPCommandTable =
        {
            { "enable",     SEC_PLAYER, false, &HandleEnableCommand,    "" },
            { "disable",    SEC_PLAYER, false, &HandleDisableCommand,   "" },
            { "view",       SEC_PLAYER, false, &HandleViewCommand,      "" },
            { "set",        SEC_PLAYER, false, &HandleSetCommand,       "" },
            { "default",    SEC_PLAYER, false, &HandleDefaultCommand,   "" },
            { "",           SEC_PLAYER, false, &HandleXPCommand,        "" }
        };

        static std::vector<ChatCommand> IndividualXPBaseTable =
        {
            { "xp", SEC_PLAYER, false, nullptr, "", IndividualXPCommandTable }
        };

            return IndividualXPBaseTable;
    }

    static bool HandleXPCommand(ChatHandler* handler, char const* args)
    {
        if (!IndividualXpEnabled)
        {
            handler->PSendSysMessage("[XP] The Individual XP module is deactivated.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!*args)
            return false;

        return true;
    }

    static bool HandleViewCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!IndividualXpEnabled)
        {
            handler->PSendSysMessage("[XP] The Individual XP module is deactivated.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        if (me->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN))
        {
            handler->PSendSysMessage("[XP] Your Individual XP is currently disabled. Use .xp enable to re-enable it.");
            handler->SetSentErrorMessage(true);
            return false;
        }
        else
        {
            me->GetSession()->SendAreaTriggerMessage("Your current XP rate is %f", me->CustomData.GetDefault<PlayerXpRate>("Individual_XP")->XPRate);
        }
        return true;
    }

    // Set Command
    static bool HandleSetCommand(ChatHandler* handler, char const* args)
    {
        if (!IndividualXpEnabled)
        {
            handler->PSendSysMessage("[XP] The Individual XP module is deactivated.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!*args)
            return false;

        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        float rate = (float)atof(args);
        if (rate > MaxRate)
        {
            handler->PSendSysMessage("[XP] The maximum rate limit is %f.", MaxRate);
            handler->SetSentErrorMessage(true);
            return false;
        }
        else if (rate < 0.1f)
        {
            handler->PSendSysMessage("[XP] The minimum rate limit is 0.1");
            handler->SetSentErrorMessage(true);
            return false;
        }

        me->CustomData.GetDefault<PlayerXpRate>("Individual_XP")->XPRate = rate;
        me->GetSession()->SendAreaTriggerMessage("You have updated your XP rate to %f", rate);
        return true;
    }

    // Disable Command
    static bool HandleDisableCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!IndividualXpEnabled)
        {
            handler->PSendSysMessage("[XP] The Individual XP module is deactivated.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        // Turn Disabled On But Don't Change Value...
        me->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN);
        me->GetSession()->SendAreaTriggerMessage("You have disabled your XP gain.");
        return true;
    }

    // Enable Command
    static bool HandleEnableCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!IndividualXpEnabled)
        {
            handler->PSendSysMessage("[XP] The Individual XP module is deactivated.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        me->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_NO_XP_GAIN);
        me->GetSession()->SendAreaTriggerMessage("You have enabled your XP gain.");
        return true;
    }

    // Default Command
    static bool HandleDefaultCommand(ChatHandler* handler, char const* /*args*/)
    {
        if (!IndividualXpEnabled)
        {
            handler->PSendSysMessage("[XP] The Individual XP module is deactivated.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        me->CustomData.GetDefault<PlayerXpRate>("Individual_XP")->XPRate = DefaultRate;
        me->GetSession()->SendAreaTriggerMessage("You have restored your XP rate to the default value of %f", DefaultRate);
        return true;
    }
};

void AddIndividual_XPScripts()
{
    new Individual_XP_conf();
    new Individual_Xp_Announce();
    new Individual_XP();
    new Individual_XP_command();
}
