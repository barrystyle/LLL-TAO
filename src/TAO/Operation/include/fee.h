/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2021

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#pragma once
#ifndef NEXUS_TAO_OPERATION_INCLUDE_FEE_H
#define NEXUS_TAO_OPERATION_INCLUDE_FEE_H

#include <LLC/types/uint1024.h>

/* Global TAO namespace. */
namespace TAO
{

    /* Register layer. */
    namespace Register
    {
        /* Forward declarations. */
        class Object;
    }


    /* Operation Layer namespace. */
    namespace Operation
    {

        /* Forward declarations. */
        class Contract;


        /** Debit
         *
         *  Namespace to contain main functions for OP::DEBIT
         *
         **/
        namespace Fee
        {

            /** Commit
             *
             *  Commit the final state to disk.
             *
             *  @param[in] account The account to commit.
             *  @param[in] hashTx The tx-id of calling transaction.
             *  @param[in] hashFrom The register address to commit.
             *  @param[in] hashTo The register address to.
             *  @param[in] nFlags Flags to the LLD instance.
             *
             *  @return true if successful.
             *
             **/
            bool Commit(const TAO::Register::Object& account,
                const uint256_t& hashAddress, const uint8_t nFlags);


            /** Execute
             *
             *  Authorizes funds from an account to an account
             *
             *  @param[out] account The object register to debit from.
             *  @param[in] nFees The amount to debit from object.
             *  @param[in] nTimestamp The timestamp to update register to.
             *
             *  @return true if successful.
             *
             **/
            bool Execute(TAO::Register::Object &account,
                const uint64_t nFees, const uint64_t nTimestamp);


            /** Verify
             *
             *  Verify claim validation rules and caller.
             *
             *  @param[in] contract The contract to verify.
             *
             *  @return true if successful.
             *
             **/
            bool Verify(const Contract& contract);
        }
    }
}

#endif
