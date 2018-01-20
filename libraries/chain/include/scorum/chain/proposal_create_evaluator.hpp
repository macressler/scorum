#pragma once

#include <scorum/protocol/scorum_operations.hpp>

#include <scorum/chain/evaluator.hpp>

namespace scorum {
namespace chain {

class account_service_i;
class proposal_service_i;
class registration_committee_service_i;
class dynamic_global_property_service_i;

class data_service_factory_i;

class proposal_create_evaluator : public evaluator_impl<data_service_factory_i, proposal_create_evaluator>
{
public:
    using operation_type = scorum::protocol::proposal_create_operation;

    proposal_create_evaluator(data_service_factory_i& services);

    void do_apply(const operation_type& op);

    uint64_t get_quorum(scorum::protocol::proposal_action action);

private:
    account_service_i& _account_service;
    proposal_service_i& _proposal_service;
    registration_committee_service_i& _committee_service;
    dynamic_global_property_service_i& _property_service;
};

} // namespace chain
} // namespace scorum
