/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLD/include/global.h>

#include <TAO/API/include/accounts.h>

#include <TAO/Ledger/types/transaction.h>
#include <TAO/Ledger/types/sigchain.h>

#include <Util/include/hex.h>

#include <Util/include/args.h>

/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /** List of accounts in API. **/
        Accounts accounts;


        /* Standard initialization function. */
        void Accounts::Initialize()
        {
            mapFunctions["login"]               = Function(std::bind(&Accounts::Login,           this, std::placeholders::_1, std::placeholders::_2));
            mapFunctions["unlock"]              = Function(std::bind(&Accounts::Unlock,          this, std::placeholders::_1, std::placeholders::_2));
            mapFunctions["lock"]                = Function(std::bind(&Accounts::Lock,            this, std::placeholders::_1, std::placeholders::_2));
            mapFunctions["logout"]              = Function(std::bind(&Accounts::Logout,          this, std::placeholders::_1, std::placeholders::_2));
            mapFunctions["create"]              = Function(std::bind(&Accounts::CreateAccount,   this, std::placeholders::_1, std::placeholders::_2));
            mapFunctions["transactions"]        = Function(std::bind(&Accounts::GetTransactions, this, std::placeholders::_1, std::placeholders::_2));
        }


        /* Determine if a sessionless user is logged in. */
        bool Accounts::LoggedIn() const
        {
            return !config::fAPISessions && mapSessions.count(0); 
        }


        /* Determine if the accounts are locked. */
        bool Accounts::Locked(SecureString& strSecret) const
        {
            if(config::fAPISessions || strActivePIN.empty())
                return true;

            strSecret = strActivePIN;
            return false;
        }


        /* Returns a key from the account logged in. */
        uint512_t Accounts::GetKey(uint32_t nKey, SecureString strSecret, uint64_t nSession) const
        {
            LOCK(MUTEX);

            /* For sessionless API use the active sig chain which is stored in session 0 */
            uint64_t nSessionToUse = config::fAPISessions ? nSession : 0;

            if(!mapSessions.count(nSessionToUse))
            {
                if( config::fAPISessions)
                    throw APIException(-1, debug::safe_printstr("session ", nSessionToUse, " doesn't exist"));
                else
                    throw APIException(-1, "User not logged in.");
            }

            return mapSessions[nSessionToUse].Generate(nKey, strSecret);
        }


        /* Returns the genesis ID from the account logged in. */
        uint256_t Accounts::GetGenesis(uint64_t nSession) const
        {
            LOCK(MUTEX);

            /* For sessionless API use the active sig chain which is stored in session 0 */
            uint64_t nSessionToUse = config::fAPISessions ? nSession : 0;

            if(!mapSessions.count(nSessionToUse))
            {
                if( config::fAPISessions)
                    throw APIException(-1, debug::safe_printstr("session ", nSessionToUse, " doesn't exist"));
                else
                    throw APIException(-1, "User not logged in.");
            }

            return mapSessions[nSessionToUse].Genesis(); //TODO: Assess the security of being able to generate genesis. Most likely this should be a localDB thing.
        }


        /* Returns the sigchain the account logged in. */
        bool Accounts::GetAccount(uint64_t nSession, TAO::Ledger::SignatureChain &user) const
        {
            LOCK(MUTEX);

            /* For sessionless API use the active sig chain which is stored in session 0 */
            uint64_t nSessionToUse = config::fAPISessions ? nSession : 0;
            
            /* Check if you are logged in. */
            if(!mapSessions.count(nSessionToUse))
                return false;

            user = mapSessions[nSessionToUse];

            return true;
        }
    }
}
