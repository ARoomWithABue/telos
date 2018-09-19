#include <typeinfo>
#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>

#include <eosio.system/eosio.system.wast.hpp>
#include <eosio.system/eosio.system.abi.hpp>
// These contracts are still under dev
#include <eosio.bios/eosio.bios.wast.hpp>
#include <eosio.bios/eosio.bios.abi.hpp>
#include <eosio.token/eosio.token.wast.hpp>
#include <eosio.token/eosio.token.abi.hpp>
#include <eosio.msig/eosio.msig.wast.hpp>
#include <eosio.msig/eosio.msig.abi.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>

#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif


using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;


/**
    * TELOS CHANGES:
    *
    * 1. Updated continuous_rate (inflation rate) to 2.5% --- DONE
    * 2. Added producer_rate constant for BP/Standby payout --- DONE
    * 3. Updated min_activated_stake to reflect TELOS 15% activation threshold --- DONE
    * 4. Added worker_proposal_rate constant for Worker Proposal Fund --- DONE
    * 5. Added min_unpaid_blocks_threshold constant for producer payout qualification --- DONE
    * 
    * NOTE: A full breakdown of the calculated token supply and 15% activation threshold
    * can be found at: https://docs.google.com/document/d/1K8w_Kd8Vmk_L0tAK56ETfAWlqgLo7r7Ae3JX25A5zVk/edit?usp=sharing
    */

// constants
const string ten_times_max_supply_string = "1904732490.0000";
const string max_supply_string = "190473249.0000";
const uint64_t max_supply_count = 190'473'249'0000;   // max TLOS supply of 190,473,249 (fluctuating value until mainnet activation)
const uint32_t activation_threshold = 15;             // 15% treshold required for vote
const uint64_t min_activated_stake = 28'570'987'0000; // calculated from max TLOS supply of 190,473,249 (fluctuating value until mainnet activation)
const double continuous_rate = 0.025;                // 2.5% annual inflation rate
const double producer_rate = 0.01;                   // 1% TLOS rate to BP/Standby
const double worker_rate = 0.015;                    // 1.5% TLOS rate to worker fund
const uint64_t min_unpaid_blocks_threshold = 342;    // Minimum unpaid blocks required to qualify for Producer level payout

// Calculated constants
const uint64_t now_seconds = time(NULL);
const uint32_t seconds_per_year = 52 * 7 * 24 * 3600;
const uint32_t seconds_per_30days = 30 * 24 * 3600;

const uint64_t one_token_balance = 1'0000ll;
const uint64_t one_hundred_token_balance = 100'0000ll;
const uint64_t voter2_balance    = floor( min_activated_stake * 10 / activation_threshold );
const uint64_t voter3_balance    = floor( min_activated_stake *  1 / activation_threshold );
const uint64_t voter4_balance    = min_activated_stake - voter2_balance - voter3_balance;
const uint64_t voter5_balance    = floor( min_activated_stake *  3 / activation_threshold );
const uint64_t one_part_balance  = floor( min_activated_stake *  1 / activation_threshold );
const uint64_t mases_balance = max_supply_count - voter2_balance - voter3_balance - voter4_balance - voter5_balance - 24 * one_part_balance - one_hundred_token_balance - 2 * one_token_balance; 

const uint64_t amount_spent_on_ram = 1000;


struct genesis_account {
   account_name aname;
   uint64_t     initial_balance;
};

// Updated voter weights based on https://docs.google.com/document/d/1K8w_Kd8Vmk_L0tAK56ETfAWlqgLo7r7Ae3JX25A5zVk/edit?usp=sharing
std::vector<genesis_account> test_genesis( {
  {N(voter2),    voter2_balance},
  {N(voter3),    voter3_balance},
  {N(voter4),    voter4_balance},
  {N(voter5),    voter5_balance},
  {N(proda),     one_part_balance},
  {N(prodb),     one_part_balance},
  {N(prodc),     one_part_balance},
  {N(prodd),     one_part_balance},
  {N(prode),     one_part_balance},
  {N(prodf),     one_part_balance},
  {N(prodg),     one_part_balance},
  {N(prodh),     one_part_balance},
  {N(prodi),     one_part_balance},
  {N(prodj),     one_part_balance},
  {N(prodk),     one_part_balance},
  {N(prodl),     one_part_balance},
  {N(prodm),     one_part_balance},
  {N(prodn),     one_part_balance},
  {N(prodo),     one_part_balance},
  {N(prodp),     one_part_balance},
  {N(prodq),     one_part_balance},
  {N(prodr),     one_part_balance},
  {N(prods),     one_part_balance},
  {N(prodt),     one_part_balance},
  {N(produ),     one_part_balance},
  {N(runnerup1), one_part_balance},
  {N(runnerup2), one_part_balance},
  {N(runnerup3), one_part_balance},
  {N(minow1),    one_hundred_token_balance},
  {N(minow2),    one_token_balance},
  {N(minow3),    one_token_balance},
  {N(masses),    mases_balance}
});

class bootseq_tester : public TESTER {
public:

   fc::variant get_global_state() {
      vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, N(global), N(global) );
      if (data.empty()) std::cout << "\nData is empty\n" << std::endl;
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "eosio_global_state", data, abi_serializer_max_time );

   }

    auto buyram( name payer, name receiver, asset ram ) {
       auto r = base_tester::push_action(config::system_account_name, N(buyram), payer, mvo()
                    ("payer", payer)
                    ("receiver", receiver)
                    ("quant", ram)
                    );
       produce_block();
       return r;
    }

    auto delegate_bandwidth( name from, name receiver, asset net, asset cpu, uint8_t transfer = 1) {
       auto r = base_tester::push_action(config::system_account_name, N(delegatebw), from, mvo()
                    ("from", from )
                    ("receiver", receiver)
                    ("stake_net_quantity", net)
                    ("stake_cpu_quantity", cpu)
                    ("transfer", transfer)
                    );
       produce_block();
       return r;
    }

    void create_currency( name contract, name manager, asset maxsupply, const private_key_type* signer = nullptr ) {
        auto act =  mutable_variant_object()
                ("issuer",       manager )
                ("maximum_supply", maxsupply );

        base_tester::push_action(contract, N(create), contract, act );
    }

    auto issue( name contract, name manager, name to, asset amount ) {
       auto r = base_tester::push_action( contract, N(issue), manager, mutable_variant_object()
                ("to",      to )
                ("quantity", amount )
                ("memo", "")
        );
        produce_block();
        return r;
    }

    auto claim_rewards( name owner ) {
       auto r = base_tester::push_action( config::system_account_name, N(claimrewards), owner, mvo()("owner",  owner ));
       produce_block();
       return r;
    }

    auto set_privileged( name account ) {
       auto r = base_tester::push_action(config::system_account_name, N(setpriv), config::system_account_name,  mvo()("account", account)("is_priv", 1));
       produce_block();
       return r;
    }

    auto register_producer(name producer) {
       auto r = base_tester::push_action(config::system_account_name, N(regproducer), producer, mvo()
                       ("producer",  name(producer))
                       ("producer_key", get_public_key( producer, "active" ) )
                       ("url", "" )
                       ("location", 0 )
                    );
       produce_block();
       return r;
    }


    auto undelegate_bandwidth( name from, name receiver, asset net, asset cpu ) {
       auto r = base_tester::push_action(config::system_account_name, N(undelegatebw), from, mvo()
                    ("from", from )
                    ("receiver", receiver)
                    ("unstake_net_quantity", net)
                    ("unstake_cpu_quantity", cpu)
                    );
       produce_block();
       return r;
    }

    asset get_balance( const account_name& act ) {
         return get_currency_balance(N(eosio.token), symbol(CORE_SYMBOL), act);
    }

    void set_code_abi(const account_name& account, const char* wast, const char* abi, const private_key_type* signer = nullptr) {
       wdump((account));
        set_code(account, wast, signer);
        set_abi(account, abi, signer);
        if (account == config::system_account_name) {
           const auto& accnt = control->db().get<account_object,by_name>( account );
           abi_def abi_definition;
           BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi_definition), true);
           abi_ser.set_abi(abi_definition, abi_serializer_max_time);
        }
        produce_blocks();
    }


    abi_serializer abi_ser;
};

BOOST_AUTO_TEST_SUITE(bootseq_tests)

BOOST_FIXTURE_TEST_CASE( bootseq_test, bootseq_tester ) {
    try {
        // Create eosio.msig and eosio.token
        create_accounts({N(eosio.msig), N(eosio.token), N(eosio.ram), N(eosio.ramfee), N(eosio.stake), N(eosio.vpay), N(eosio.bpay), N(eosio.saving) });

        // Set code for the following accounts:
        //  - eosio (code: eosio.bios) (already set by tester constructor)
        //  - eosio.msig (code: eosio.msig)
        //  - eosio.token (code: eosio.token)
        set_code_abi(N(eosio.msig), eosio_msig_wast, eosio_msig_abi);//, &eosio_active_pk);
        set_code_abi(N(eosio.token), eosio_token_wast, eosio_token_abi); //, &eosio_active_pk);

        // Set privileged for eosio.msig and eosio.token
        set_privileged(N(eosio.msig));
        set_privileged(N(eosio.token));

        // Verify eosio.msig and eosio.token is privileged
        const auto& eosio_msig_acc = get<account_object, by_name>(N(eosio.msig));
        BOOST_TEST(eosio_msig_acc.privileged == true);
        const auto& eosio_token_acc = get<account_object, by_name>(N(eosio.token));
        BOOST_TEST(eosio_token_acc.privileged == true);


        // Create SYS tokens in eosio.token, set its manager as eosio
        auto max_supply = core_from_string(ten_times_max_supply_string); 
        auto initial_supply = core_from_string(max_supply_string); 

        create_currency(N(eosio.token), config::system_account_name, max_supply);
        // Issue the genesis supply of 1 billion SYS tokens to eosio.system
        issue(N(eosio.token), config::system_account_name, config::system_account_name, initial_supply);

        auto actual = get_balance(config::system_account_name);
        BOOST_REQUIRE_EQUAL(initial_supply, actual);


        // Create genesis accounts
        for( const auto& a : test_genesis ) {
           create_account( a.aname, config::system_account_name );
        }

        // Set eosio.system to eosio
        set_code_abi(config::system_account_name, eosio_system_wast, eosio_system_abi);

        uint64_t sum = 0;
        // Buy ram and stake cpu and net for each genesis accounts
        for( const auto& a : test_genesis ) {
           auto ib = a.initial_balance;
           auto ram = amount_spent_on_ram;
           auto net = (ib - ram) / 2;
           auto cpu = ib - net - ram;
           sum += ib;

           auto r = buyram(config::system_account_name, a.aname, asset(ram));
           BOOST_REQUIRE( !r->except_ptr );

           r = delegate_bandwidth(N(eosio.stake), a.aname, asset(net), asset(cpu));
           BOOST_REQUIRE( !r->except_ptr );
        }

        auto producer_candidates = {
                N(proda), N(prodb), N(prodc), N(prodd), N(prode), N(prodf), N(prodg),
                N(prodh), N(prodi), N(prodj), N(prodk), N(prodl), N(prodm), N(prodn),
                N(prodo), N(prodp), N(prodq), N(prodr), N(prods), N(prodt), N(produ),
                N(runnerup1), N(runnerup2), N(runnerup3)
        };

        // Register producers
        for( auto pro : producer_candidates ) {
           register_producer(pro);
        }

        // Vote for producers
        auto votepro = [&]( account_name voter, vector<account_name> producers ) {
          std::sort( producers.begin(), producers.end() );
          base_tester::push_action(config::system_account_name, N(voteproducer), voter, mvo()
                                ("voter",  name(voter))
                                ("proxy", name(0) )
                                ("producers", producers)
                     );
        };
        votepro( N(voter2), { N(proda), N(prodb), N(prodc), N(prodd), N(prode), N(prodf), N(prodg),
                           N(prodh), N(prodi), N(prodj), N(prodk), N(prodl), N(prodm), N(prodn),
                           N(prodo), N(prodp), N(prodq), N(prodr), N(prods), N(prodt), N(produ)} );
        votepro( N(voter3), {N(runnerup1), N(runnerup2), N(runnerup3)} );
        votepro( N(voter4), {N(proda), N(prodb), N(prodc), N(prodd), N(prode)} );

        // Total Stakes = voter2_stake + voter3_stake + voter4_stake : stake = initial_balance - amount_spent_on_ram
        int64_t expected_sub_min_activated_stake = voter2_balance - amount_spent_on_ram + voter3_balance - amount_spent_on_ram + voter4_balance - amount_spent_on_ram;
        BOOST_TEST(get_global_state()["total_activated_stake"].as<int64_t>() == expected_sub_min_activated_stake);

        // No producers will be set, since the total activated stake is less than min_activated_stake
        produce_blocks_for_n_rounds(2); // 2 rounds since new producer schedule is set when the first block of next round is irreversible
        auto active_schedule = control->head_block_state()->active_schedule;

        BOOST_TEST(active_schedule.producers.size() == 1);
        BOOST_TEST(active_schedule.producers.front().producer_name == "eosio");

        // Spend some time so the producer pay pool is filled by the inflation rate
        produce_min_num_of_blocks_to_spend_time_wo_inactive_prod(fc::seconds(seconds_per_30days)); // 30 days

        // Since the total activated stake is less than min_activated_stake, it shouldn't be possible to claim rewards
        BOOST_REQUIRE_THROW(claim_rewards(N(runnerup1)), eosio_assert_message_exception);

        // This will increase the total vote stake by one part
        votepro( N(voter5), {N(prodq), N(prodr), N(prods), N(prodt), N(produ)} );
        int64_t expected_over_min_activated_stake = expected_sub_min_activated_stake + voter5_balance - amount_spent_on_ram;
        BOOST_TEST(get_global_state()["total_activated_stake"].as<int64_t>() == expected_over_min_activated_stake);

        // Since the total vote stake is more than min_activated_stake, the new producer set will be set
        produce_blocks_for_n_rounds(2); // 2 rounds since new producer schedule is set when the first block of next round is irreversible
        active_schedule = control->head_block_state()->active_schedule;
        BOOST_REQUIRE(active_schedule.producers.size() == 21);

        // Since we skipped some time previously, rotation came in effect !
        // BOOST_TEST(active_schedule.producers.at(0).producer_name == "prod1");
        BOOST_TEST(active_schedule.producers.at(0).producer_name == "prodb");
        BOOST_TEST(active_schedule.producers.at(1).producer_name == "prodc");
        BOOST_TEST(active_schedule.producers.at(2).producer_name == "prodd");
        BOOST_TEST(active_schedule.producers.at(3).producer_name == "prode");
        BOOST_TEST(active_schedule.producers.at(4).producer_name == "prodf");
        BOOST_TEST(active_schedule.producers.at(5).producer_name == "prodg");
        BOOST_TEST(active_schedule.producers.at(6).producer_name == "prodh");
        BOOST_TEST(active_schedule.producers.at(7).producer_name == "prodi");
        BOOST_TEST(active_schedule.producers.at(8).producer_name == "prodj");
        BOOST_TEST(active_schedule.producers.at(9).producer_name == "prodk");
        BOOST_TEST(active_schedule.producers.at(10).producer_name == "prodl");
        BOOST_TEST(active_schedule.producers.at(11).producer_name == "prodm");
        BOOST_TEST(active_schedule.producers.at(12).producer_name == "prodn");
        BOOST_TEST(active_schedule.producers.at(13).producer_name == "prodo");
        BOOST_TEST(active_schedule.producers.at(14).producer_name == "prodp");
        BOOST_TEST(active_schedule.producers.at(15).producer_name == "prodq");
        BOOST_TEST(active_schedule.producers.at(16).producer_name == "prodr");
        BOOST_TEST(active_schedule.producers.at(17).producer_name == "prods");
        BOOST_TEST(active_schedule.producers.at(18).producer_name == "prodt");
        BOOST_TEST(active_schedule.producers.at(19).producer_name == "produ");
        BOOST_TEST(active_schedule.producers.at(20).producer_name == "runnerup1");

        // Spend some time so the producer pay pool is filled by the inflation rate
        produce_min_num_of_blocks_to_spend_time_wo_inactive_prod(fc::seconds(seconds_per_30days)); // 30 days
        // Since the total activated stake is larger than 28,570,987.35, pool should be filled reward should be bigger than zero
        claim_rewards(N(runnerup1));
        BOOST_TEST(get_balance(N(runnerup1)).get_amount() > 0);

        // const auto first_june_2018 = fc::seconds(1527811200); // 2018-06-01
        // const auto first_june_2028 = fc::seconds(1843430400); // 2028-06-01
        // // Ensure that now is yet 10 years after 2018-06-01 yet
        // BOOST_REQUIRE(control->head_block_time().time_since_epoch() < first_june_2028);

        // // This should thrown an error, since block one can only unstake all his stake after 10 years
        // BOOST_REQUIRE_THROW(undelegate_bandwidth(N(b1), N(b1), core_from_string("9523567.2134"), core_from_string("9523567.2134")), eosio_assert_message_exception);
        // // Skip 10 years
        // produce_block(first_june_2028 - control->head_block_time().time_since_epoch());
        // // Block one should be able to unstake all his stake now
        // undelegate_bandwidth(N(b1), N(b1), core_from_string("9523567.2134"), core_from_string("9523567.2134"));

        return;
        // produce_blocks(7000); /// produce blocks until virutal bandwidth can acomadate a small user
        // wlog("minow" );
        // votepro( N(minow1), {N(p1), N(p2)} );
    } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()