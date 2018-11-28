/*__________________________________________________________________________________________

			(c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2018] ++

			(c) Copyright The Nexus Developers 2014 - 2018

			Distributed under the MIT software license, see the accompanying
			file COPYING or http://www.opensource.org/licenses/mit-license.php.

			"ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#ifndef NEXUS_LEGACY_WALLET_WALLETKEY_H
#define NEXUS_LEGACY_WALLET_WALLETKEY_H

#include <Util/include/runtime.h>
#include <Util/templates/serialize.h>

/* forward declaration */    
namespace LLC 
{
    class CPrivKey;
}

namespace Legacy
{
    
    namespace Wallet
    {
        /** @class CWalletKey
         *
         *  Class to hold unencrypted private key binary data. 
         *
         *  @deprecated This class is no longer used or written to the wallet database. It
         *              is supported for backward compatability, so values can be read
         *              that were previously written into the database.
         *
         *  Database key is wkey<publickey>
         **/
        class CWalletKey
        {
        public:
            /** Unencrypted private key data **/
            LLC::CPrivKey vchPrivKey;

            /** Timestamp when this wallet key was created. Only relevant if nTimeExpires has a value **/
            uint64_t nTimeCreated;

            /** Number of seconds after nTimeCreated that this wallet key expires **/
            uint64_t nTimeExpires;

            /** Space to include comments **/
            std::string strComment;


            /** Constructor
             *
             *  Sets nTimeCreated to current time if nExpires has a value. 
             *
             **/
            CWalletKey(uint64_t nExpires=0)
            {
                nTimeCreated = (nExpires ? UnifiedTimestamp() : 0);
                nTimeExpires = nExpires;
            }


            IMPLEMENT_SERIALIZE
            (
                if (!(nSerType & SER_GETHASH))
                    READWRITE(nVersion);
                READWRITE(vchPrivKey);
                READWRITE(nTimeCreated);
                READWRITE(nTimeExpires);
                READWRITE(strComment);
            )
        };

    }
}

#endif
