/*
INSERT INTO `creature_template` VALUES ('99013', '0', '0', '0', '0', '0', '491', '491', '523', '523', 'Lottofee', 'Versucht euer Glück', '', '0', '85', '85', '2', '35', '35', '1', '1', '1.14286', '1', '0', '0', '0', '0', '0', '1.4', '0', '0', '2', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '', '0', '3', '1', '1024', '1', '1', '0', '0', '0', '0', '0', '0', '0', '0', '1', '0', '0', '0', 'npc_lottery', '1');
*/

#include "ScriptPCH.h"
#include "Config.h"

#define GOSSIP_BUY_TICKET           "Ticket kaufen."
#define EVENT_LOTTERY               132


class npc_lottery : public CreatureScript
{
public:
    npc_lottery() : CreatureScript("npc_lottery") { }

    bool OnGossipHello(Player* player, Creature* pCreature)
    {
        if (player && !player->isGameMaster() && sWorld->getBoolConfig(CONFIG_LOTTERY_ENABLE))
        {
            if (player->getLevel() >= uint32(sWorld->getIntConfig(CONFIG_LOTTERY_MINLVL)) && player->GetMoney() >= uint32(sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST)))
            {
				pCreature->MonsterWhisper("Hallo MMOler. Möchtest du dein Glück versuchen? Es ist ganz einfach - Gib mir 5 Zahlen von 1 bis 30 (durch Leerzeichen getrennt), sowie die Kosten für die Teilnahme und warten auf die Ziehung.");
                player->PrepareGossipMenu(pCreature);
                player->ADD_GOSSIP_ITEM_EXTENDED(0, GOSSIP_BUY_TICKET, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF, "", 0, true);
                player->SEND_GOSSIP_MENU(player->GetGossipTextId(pCreature), pCreature->GetGUID());
            }
            else
            {
                std::string wh = ("Du hast nicht genug Gold (erfordert 80 Gold oder dein Level ist noch nicht hoch genug (erfordert Level 60");
                pCreature->MonsterWhisper(wh.c_str(), player->GetGUID());
            }
        }
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* pCreature, uint32 uiSender, uint32 uiAction, const char* code)
    {
        player->PlayerTalkClass->ClearMenus();
        if (uiSender == GOSSIP_SENDER_MAIN)
        {
            switch (uiAction)
            {
                case GOSSIP_ACTION_INFO_DEF:
                {
                    char * charCode = (char*)code;
                    std::string strCode = (char*)code;
                    char * tmp;
                    int32 number[5];
                    std::string error = ("Du hast eine ungültige Zahl eingegeben. Die Zahlen müssen im Bereich von 1 bis 30");
                    std::string errordub = ("Du hast bereits auf die nächste Ziehung gesetzt");
                    std::string numbers = ("Du hast auf folgende Zahlen getippt " + std::string(strCode));

                    tmp = strtok (charCode," ");
                    for (int8 n = 0; n < 5; ++n)
                    {
                        if (tmp != NULL)
                        {
                            number[n] = atoi(tmp);
                            if (number[n] < 0 || number[n] > ConfigMgr::GetIntDefault("Lottery.MaxNumber", 30))
                            {
                                pCreature->MonsterWhisper(error.c_str(), player->GetGUID());
                                player->CLOSE_GOSSIP_MENU();
                                return false;
                            }
                            tmp = strtok (NULL, " ");
                        }
                        else
                        {
                            pCreature->MonsterWhisper(error.c_str(), player->GetGUID());
                            player->CLOSE_GOSSIP_MENU();
                            return false;
                        }
                    }

                    uint32 betMaxID;
                    QueryResult qbetMaxID = WorldDatabase.Query("SELECT MAX(id) FROM lottery_bets");

                    if (qbetMaxID)
                        betMaxID = qbetMaxID->Fetch()->GetUInt32();
                    else
                        betMaxID = 0;
                        
                    uint32 rBetGuid;
                    QueryResult pBetGuid = WorldDatabase.Query("SELECT guid FROM lottery_bets");
                    
                    if (pBetGuid)
                        rBetGuid = pBetGuid->Fetch()->GetUInt32();
                    else
                        rBetGuid = 0;
                    
                    if (rBetGuid == player->GetGUID())
                    {
                        pCreature->MonsterWhisper(errordub.c_str(), player->GetGUID());
                        player->CLOSE_GOSSIP_MENU();
                        return false;
                    }

                    WorldDatabase.PExecute("INSERT INTO lottery_bets (id, name, guid, bet) VALUES ('%u', '%s', '%u', '%s')", betMaxID+1, player->GetName(), player->GetGUIDLow(), strCode.c_str());
                    pCreature->MonsterWhisper(numbers.c_str(), player->GetGUID());
                    player->ModifyMoney(-sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST));

                    player->CLOSE_GOSSIP_MENU();

                    return true;
                }
            }
        }

        return false;
        }

    CreatureAI* GetAI(Creature* pCreature) const
    {
        return new npc_lotteryAI (pCreature);
    }

    struct npc_lotteryAI : public ScriptedAI
    {
        npc_lotteryAI(Creature* pCreature) : ScriptedAI(pCreature) { }

        void UpdateAI(const uint32 diff)
        {
            if (IsEventActive(EVENT_LOTTERY))
            {
                if (me->IsVisible())
                {
                    QueryResult qMaxID = WorldDatabase.Query("SELECT MAX(id) FROM lottery_bets");
                    if (!qMaxID)
                        return;

                    uint32 betMaxID = qMaxID->Fetch()->GetUInt32();
                    uint32 luckyNumber[5];
                    uint32 lotteryID;
                    uint32 jackpotWinners = 0;

                    QueryResult qlotteryID = WorldDatabase.Query("SELECT MAX(id) FROM lottery");
                    if (!qlotteryID)
                        lotteryID = 0;
                    else
                        lotteryID = qlotteryID->Fetch()->GetUInt32();

                    QueryResult qBets  = WorldDatabase.Query("SELECT guid, bet, name FROM lottery_bets");
                    if (qBets)
                    {
                        for (int8 n = 0; n < 5; ++n)
                        {
                            luckyNumber[n] = urand(1, ConfigMgr::GetIntDefault("Lottery.MaxNumber", 30));
                        }

                        do
                        {
                            Field *fBets = qBets->Fetch();
                            uint32 guid = fBets[0].GetUInt32();
                            std::string bet = fBets[1].GetString();
                            std::string rBet = bet; 
                            std::string name = fBets[2].GetString();
                            uint32 points = 0;
                            uint32 cash;

                            int32 number[5];
                            char * tmp;

                            tmp = strtok ((char*)bet.c_str()," ");

                            for (int8 n = 0; n < 5; ++n)
                            {
                                if (tmp != NULL)
                                {
                                    number[n] = atoi(tmp);
                                    tmp = strtok (NULL, " ");
                                }
                            }

                            for (int8 n = 0; n < 5; ++n)
                            {
                                for (int8 i = 0; i < 5; ++i)
                                {
                                    if (number[n] == luckyNumber[i])
                                        ++points;
                                }
                            }

                            switch (points)
                            {
                                case 1:
                                {
                                    cash = uint32(betMaxID * sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST) * 0.005f);
                                    break;
                                }
                                case 2:
                                {
                                    cash = uint32(betMaxID * sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST) * 0.01f);
                                    break;
                                }
                                case 3:
                                {
                                    cash = uint32(betMaxID * sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST) * 0.05f);
                                    break;
                                }
                                case 4:
                                {
                                    cash = uint32(betMaxID * sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST) * 0.2f);
                                    break;
                                }
                                case 5:
                                {
                                    WorldDatabase.PExecute("INSERT INTO lottery_winners (id, name, guid, bet, betPoints) VALUES ('%u', '%s', '%u', '%s', '%u')", lotteryID+1, name.c_str(), guid, rBet.c_str(), points);
                                    ++jackpotWinners;
                                    continue;
                                }
                                default:
                                    continue;
                            }

                            Player *pWinner = sObjectMgr->GetPlayerByLowGUID(guid);
                            SQLTransaction trans = CharacterDatabase.BeginTransaction();
                            MailDraft("Gewonnen", "Herzlichen Glückwunsch! Du hast auf die richtigen Zahlen gesetzt!")
                                .AddMoney(cash)
                                .SendMailTo(trans, MailReceiver(pWinner, GUID_LOPART(guid)), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
                            CharacterDatabase.CommitTransaction(trans);

                            WorldDatabase.PExecute("INSERT INTO lottery_winners (id, name, guid, bet, betPoints) VALUES ('%u', '%s', '%u', '%s', '%u')", lotteryID+1, name.c_str(), guid, rBet.c_str(), points);
                        } while (qBets->NextRow());
                        uint64 jackpot;
                        uint64 rJackpot;
                        uint64 defJackpot = uint64(betMaxID * sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST) * 0.7f);
                        QueryResult qJackpot  = WorldDatabase.PQuery("SELECT jackpot FROM lottery WHERE id = '%u'", lotteryID);
                        if (qJackpot)
                            jackpot = qJackpot->Fetch()->GetUInt32();
                        else
                            jackpot = 0;

                        rJackpot = jackpot;

                        if (jackpotWinners > 0)
                        {
                            QueryResult qJackpotWinners = WorldDatabase.PQuery("SELECT guid FROM lottery_winners WHERE betPoints = '5' and id = '%u'", lotteryID+1);
                            QueryResult qJackpotWinnersName;
                            if (jackpotWinners == 1)
                                qJackpotWinnersName = WorldDatabase.PQuery("SELECT name FROM lottery_winners WHERE betPoints = '5' and id = '%u'", lotteryID+1);

                            if (qJackpotWinners)
                            {
                                jackpot = uint64(jackpot / jackpotWinners);
                                do
                                {
                                    Field *fJackpotWinners = qJackpotWinners->Fetch();
                                    uint32 JPguid = fJackpotWinners[0].GetUInt32();

                                    Player *pJPWinner = sObjectMgr->GetPlayerByLowGUID(JPguid);
                                    SQLTransaction trans = CharacterDatabase.BeginTransaction();
                                    MailDraft("Jackpot!", "WoW! Du hast einfach massig Glück! Herzlichen Glückwunsch")
                                        .AddMoney(jackpot = 0 ? uint64(betMaxID * sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST) * 0.7f) : jackpot)
                                        .SendMailTo(trans, MailReceiver(pJPWinner, GUID_LOPART(JPguid)), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
                                    CharacterDatabase.CommitTransaction(trans);
                                } while (qJackpotWinners->NextRow());
                            }

                            if (jackpotWinners == 1 && qJackpotWinnersName)
                            {
                                std::string wName = qJackpotWinnersName->Fetch()->GetString();
                                sWorld->SendWorldText(LANG_LOTTERY_ANNOUNCE_JACKPOT, betMaxID, luckyNumber[0], luckyNumber[1], luckyNumber[2], luckyNumber[3], luckyNumber[4], ((jackpot = 0 ? uint64(betMaxID * sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST) * 0.7f) : rJackpot) * 0.0001f), wName.c_str());
                            }
                            else
                                sWorld->SendWorldText(LANG_LOTTERY_ANNOUNCE_JACKPOT_M_PLAYERS, betMaxID, luckyNumber[0], luckyNumber[1], luckyNumber[2], luckyNumber[3], luckyNumber[4], ((jackpot = 0 ? uint64(betMaxID * sWorld->getIntConfig(CONFIG_LOTTERY_BETCOST) * 0.7f) : rJackpot) * 0.0001f), jackpotWinners);

                            WorldDatabase.PExecute("INSERT INTO lottery (number_1, number_2, number_3, number_4, number_5, jackpot) VALUES ('%u', '%u', '%u', '%u', '%u', '%u')", luckyNumber[0], luckyNumber[1], luckyNumber[2], luckyNumber[3], luckyNumber[4], defJackpot);
                        }
                        else
                        {
                            sWorld->SendWorldText(LANG_LOTTERY_ANNOUNCE, betMaxID, luckyNumber[0], luckyNumber[1], luckyNumber[2], luckyNumber[3], luckyNumber[4], ((jackpot + defJackpot) * 0.0001f));
                            WorldDatabase.PExecute("INSERT INTO lottery (number_1, number_2, number_3, number_4, number_5, jackpot) VALUES ('%u', '%u', '%u', '%u', '%u', '%u')", luckyNumber[0], luckyNumber[1], luckyNumber[2], luckyNumber[3], luckyNumber[4], (jackpot + defJackpot));
                        }
                        WorldDatabase.PExecute("DELETE FROM lottery_bets");

                        me->SetVisible(false);
                    }
                }
            }
            else
            {
                if (!me->IsVisible())
                    me->SetVisible(true);
            }
        }
    };

};


void AddSC_npc_lottery()
{
    new npc_lottery;
}
