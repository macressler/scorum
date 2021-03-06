#include <scorum/chain/database.hpp>
#include <scorum/chain/schema/witness_objects.hpp>

#include <scorum/protocol/config.hpp>

namespace scorum {
namespace chain {

/**
 *
 *  See @ref witness_object::virtual_last_update
 */
void database::update_witness_schedule()
{
    database& _db = (*this);

    if ((_db.head_block_num() % SCORUM_MAX_WITNESSES) == 0)
    {
        const witness_schedule_object& wso = _db.get_witness_schedule_object();

        using active_witnesses_container = boost::container::flat_map<witness_id_type, account_name_type>;

        active_witnesses_container active_witnesses;
        active_witnesses.reserve(SCORUM_MAX_WITNESSES);

        const auto& widx = _db.get_index<witness_index>().indices().get<by_vote_name>();
        for (auto itr = widx.begin(); itr != widx.end() && active_witnesses.size() < SCORUM_MAX_VOTED_WITNESSES; ++itr)
        {
            if (itr->signing_key == public_key_type())
            {
                continue;
            }

            FC_ASSERT(active_witnesses.insert(std::make_pair(itr->id, itr->owner)).second);
            _db.modify(*itr, [&](witness_object& wo) { wo.schedule = witness_object::top20; });
        }

        /// Add the running witnesses in the lead
        fc::uint128 new_virtual_time = wso.current_virtual_time;

        const auto& schedule_idx = _db.get_index<witness_index>().indices().get<by_schedule_time>();
        auto sitr = schedule_idx.begin();
        std::vector<decltype(sitr)> processed_witnesses;

        for (; sitr != schedule_idx.end() && active_witnesses.size() < SCORUM_MAX_WITNESSES; ++sitr)
        {
            new_virtual_time = sitr->virtual_scheduled_time; /// everyone advances to at least this time
            processed_witnesses.push_back(sitr);

            if (sitr->signing_key == public_key_type())
            {
                continue; /// skip witnesses without a valid block signing key
            }

            if (active_witnesses.find(sitr->id) == active_witnesses.end())
            {
                FC_ASSERT(active_witnesses.insert(std::make_pair(sitr->id, sitr->owner)).second);
                _db.modify(*sitr, [&](witness_object& wo) { wo.schedule = witness_object::timeshare; });
            }
        }

        dlog("number of active witnesses is (${active_witnesses}), max number is (${SCORUM_MAX_WITNESSES})",
             ("active_witnesses", active_witnesses.size())("SCORUM_MAX_WITNESSES", SCORUM_MAX_WITNESSES));

        /// Update virtual schedule of processed witnesses
        for (auto itr = processed_witnesses.begin(); itr != processed_witnesses.end(); ++itr)
        {
            auto new_virtual_scheduled_time
                = new_virtual_time + VIRTUAL_SCHEDULE_LAP_LENGTH / ((*itr)->votes.value + 1);
            if (new_virtual_scheduled_time < new_virtual_time)
            {
                /// overflow, reset virtual time
                new_virtual_time = fc::uint128();
                _reset_witness_virtual_schedule_time();
                break;
            }
            _db.modify(*(*itr), [&](witness_object& wo) {
                wo.virtual_position = fc::uint128();
                wo.virtual_last_update = new_virtual_time;
                wo.virtual_scheduled_time = new_virtual_scheduled_time;
            });
        }

        _db.modify(wso, [&](witness_schedule_object& _wso) {
            for (size_t i = 0; i < active_witnesses.size(); i++)
            {
                _wso.current_shuffled_witnesses[i] = active_witnesses.nth(i)->second;
            }

            for (size_t i = active_witnesses.size(); i < SCORUM_MAX_WITNESSES; i++)
            {
                _wso.current_shuffled_witnesses[i] = account_name_type();
            }

            _wso.num_scheduled_witnesses = std::max<uint8_t>(active_witnesses.size(), 1);

            /// shuffle current shuffled witnesses
            auto now_hi = uint64_t(_db.head_block_time().sec_since_epoch()) << 32;
            for (uint32_t i = 0; i < _wso.num_scheduled_witnesses; ++i)
            {
                /// High performance random generator
                /// http://xorshift.di.unimi.it/
                uint64_t k = now_hi + uint64_t(i) * 2685821657736338717ULL;
                k ^= (k >> 12);
                k ^= (k << 25);
                k ^= (k >> 27);
                k *= 2685821657736338717ULL;

                uint32_t jmax = _wso.num_scheduled_witnesses - i;
                uint32_t j = i + k % jmax;
                std::swap(_wso.current_shuffled_witnesses[i], _wso.current_shuffled_witnesses[j]);
            }

            _wso.current_virtual_time = new_virtual_time;
        });

        _update_witness_majority_version();
        _update_witness_hardfork_version_votes();
        _update_witness_median_props();
    }
}

void database::_reset_witness_virtual_schedule_time()
{
    database& _db = (*this);

    const witness_schedule_object& wso = _db.get_witness_schedule_object();
    _db.modify(wso, [&](witness_schedule_object& o) {
        o.current_virtual_time = fc::uint128(); // reset it 0
    });

    const auto& idx = _db.get_index<witness_index>().indices();
    for (const auto& witness : idx)
    {
        _db.modify(witness, [&](witness_object& wobj) {
            wobj.virtual_position = fc::uint128();
            wobj.virtual_last_update = wso.current_virtual_time;
            wobj.virtual_scheduled_time = VIRTUAL_SCHEDULE_LAP_LENGTH / (wobj.votes.value + 1);
        });
    }
}

void database::_update_witness_median_props()
{
    // clang-format off

    database& _db = (*this);

    const witness_schedule_object& wso = _db.get_witness_schedule_object();

    /// fetch all witness objects
    std::vector<const witness_object*> active;
    active.reserve(wso.num_scheduled_witnesses);
    for (int i = 0; i < wso.num_scheduled_witnesses; i++)
    {
        active.push_back(&_db.get_witness(wso.current_shuffled_witnesses[i]));
    }

    /// sort them by account_creation_fee
    std::sort(active.begin(), active.end(), [&](const witness_object* a, const witness_object* b) {
        return a->proposed_chain_props.account_creation_fee.amount < b->proposed_chain_props.account_creation_fee.amount;
    });
    asset median_account_creation_fee = active[active.size() / 2]->proposed_chain_props.account_creation_fee;

    /// sort them by maximum_block_size
    std::sort(active.begin(), active.end(), [&](const witness_object* a, const witness_object* b) {
        return a->proposed_chain_props.maximum_block_size < b->proposed_chain_props.maximum_block_size;
    });
    uint32_t median_maximum_block_size = active[active.size() / 2]->proposed_chain_props.maximum_block_size;

    _db.modify(_db.get_dynamic_global_properties(), [&](dynamic_global_property_object& _dgpo) {
        _dgpo.median_chain_props.account_creation_fee = median_account_creation_fee;
        _dgpo.median_chain_props.maximum_block_size = median_maximum_block_size;
    });

    // clang-format on
}

void database::_update_witness_majority_version()
{
    database& _db = (*this);

    const witness_schedule_object& wso = _db.get_witness_schedule_object();

    flat_map<version, uint32_t, std::greater<version>> witness_versions;
    for (uint32_t i = 0; i < wso.num_scheduled_witnesses; i++)
    {
        auto witness = _db.get_witness(wso.current_shuffled_witnesses[i]);
        if (witness_versions.find(witness.running_version) == witness_versions.end())
        {
            witness_versions[witness.running_version] = 1;
        }
        else
        {
            witness_versions[witness.running_version] += 1;
        }
    }

    const auto& dgpo = _db.get_dynamic_global_properties();
    auto majority_version = dgpo.majority_version;

    // The map should be sorted highest version to smallest, so we iterate until we hit the majority of witnesses on
    // at least this version
    for (auto ver_itr = witness_versions.begin(); ver_itr != witness_versions.end(); ++ver_itr)
    {
        auto witnesses_on_version = ver_itr->second;
        if (witnesses_on_version >= SCORUM_HARDFORK_REQUIRED_WITNESSES)
        {
            majority_version = ver_itr->first;
            break;
        }
    }

    _db.modify(dgpo, [&](dynamic_global_property_object& _dgpo) { _dgpo.majority_version = majority_version; });
}

void database::_update_witness_hardfork_version_votes()
{
    database& _db = (*this);

    const witness_schedule_object& wso = _db.get_witness_schedule_object();

    flat_map<std::tuple<hardfork_version, time_point_sec>, uint32_t> hardfork_version_votes;

    for (uint32_t i = 0; i < wso.num_scheduled_witnesses; i++)
    {
        auto witness = _db.get_witness(wso.current_shuffled_witnesses[i]);

        auto version_vote = std::make_tuple(witness.hardfork_version_vote, witness.hardfork_time_vote);
        if (hardfork_version_votes.find(version_vote) == hardfork_version_votes.end())
        {
            hardfork_version_votes[version_vote] = 1;
        }
        else
        {
            hardfork_version_votes[version_vote] += 1;
        }
    }

    auto hf_itr = hardfork_version_votes.begin();

    while (hf_itr != hardfork_version_votes.end())
    {
        if (hf_itr->second >= SCORUM_HARDFORK_REQUIRED_WITNESSES)
        {
            const auto& hfp = _db.get_hardfork_property_object();
            if (hfp.next_hardfork != std::get<0>(hf_itr->first) || hfp.next_hardfork_time != std::get<1>(hf_itr->first))
            {

                _db.modify(hfp, [&](hardfork_property_object& hpo) {
                    hpo.next_hardfork = std::get<0>(hf_itr->first);
                    hpo.next_hardfork_time = std::get<1>(hf_itr->first);
                });
            }
            break;
        }

        ++hf_itr;
    }

    // We no longer have a majority
    if (hf_itr == hardfork_version_votes.end())
    {
        _db.modify(_db.get_hardfork_property_object(),
                   [&](hardfork_property_object& hpo) { hpo.next_hardfork = hpo.current_hardfork_version; });
    }
}

} // namespace chain
} // namespace scorum
