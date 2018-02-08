#include <boost/test/unit_test.hpp>

#include <scorum/chain/genesis/genesis_state.hpp>
#include <scorum/chain/genesis/initializators/accounts_initializator.hpp>

#include <scorum/chain/data_service_factory.hpp>
#include <scorum/chain/services/account.hpp>

#include <scorum/chain/schema/account_objects.hpp>
#include <scorum/chain/schema/dynamic_global_property_object.hpp>

#include "defines.hpp"
#include "genesis.hpp"

#include <hippomocks.h>

using namespace scorum::chain;
using namespace scorum::chain::genesis;
using namespace scorum::protocol;

struct genesis_initiate_accounts_fixture
{
    MockRepository mocks;

    data_service_factory_i* pservices = mocks.Mock<data_service_factory_i>();

    account_service_i* paccount_service = mocks.Mock<account_service_i>();

    genesis_initiate_accounts_fixture()
    {
        mocks.ExpectCall(pservices, data_service_factory_i::account_service).ReturnByRef(*paccount_service);
    }

    accounts_initializator test_it;
};

BOOST_AUTO_TEST_SUITE(accounts_initializator_tests)

BOOST_FIXTURE_TEST_CASE(check_empty_genesis, genesis_initiate_accounts_fixture)
{
    genesis_state_type input_genesis = Genesis::create().generate();

    BOOST_REQUIRE_NO_THROW(test_it.apply(*pservices, input_genesis));
}

BOOST_FIXTURE_TEST_CASE(check_empty_account_list, genesis_initiate_accounts_fixture)
{
    genesis_state_type input_genesis = Genesis::create().accounts_supply(ASSET_SCR(1e+6)).generate();

    SCORUM_REQUIRE_THROW(test_it.apply(*pservices, input_genesis), fc::assert_exception);
}

struct genesis_initiate_accounts_with_actors_fixture : public genesis_initiate_accounts_fixture
{
    genesis_initiate_accounts_with_actors_fixture()
        : alice("alice")
        , bob("bob")
    /*,    , INIT_MEMBER_OBJ(global_property)
     INIT_MEMBER_OBJ(alice_obj)
    , INIT_MEMBER_OBJ(bob_obj)*/
    {
    }

    Actor alice;
    Actor bob;

    // dynamic_global_property_object global_property;
    // account_object alice_obj;
    // account_object bob_obj;
};

BOOST_FIXTURE_TEST_CASE(check_invalid_account_sum, genesis_initiate_accounts_with_actors_fixture)
{
    asset total = ASSET_SCR(1e+6);

    alice.scorum(total / 2);

    genesis_state_type input_genesis = Genesis::create().accounts(alice, bob).accounts_supply(total).generate();

    //    auto lbCreate = [=](const account_name_type& new_account_name, const public_key_type& memo_key,
    //                        const asset& balance_in_scorums, const account_name_type& recovery_account,
    //                        const std::string& json_metadata) -> const account_object& {
    //        account_object* pret = nullptr;
    //        if (new_account_name == "alice")
    //            pret = &alice_obj;
    //        else if (new_account_name == "bob")
    //            pret = &bob_obj;
    //        else
    //            FC_ASSERT(false);

    //        pret->name = new_account_name;
    //        pret->balance = balance_in_scorums;
    //        pret->recovery_account = recovery_account;
    //        fc::from_string(pret->json_metadata, json_metadata);
    //        return *pret;
    //    };

    //    mocks.OnCall(paccount_service, account_service_i::create_initial_account).Do(lbCreate);

    SCORUM_REQUIRE_THROW(test_it.apply(*pservices, input_genesis), fc::assert_exception);
}

BOOST_AUTO_TEST_SUITE_END()
