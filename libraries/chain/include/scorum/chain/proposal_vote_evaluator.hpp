#pragma once

#include <scorum/protocol/scorum_operations.hpp>

#include <scorum/chain/evaluator.hpp>
#include <scorum/chain/dbservice.hpp>

#include <scorum/chain/dbs_account.hpp>
#include <scorum/chain/dbs_proposal.hpp>
#include <scorum/chain/dbs_registration_committee.hpp>

#include <scorum/chain/proposal_vote_object.hpp>

namespace scorum {
namespace chain {

using namespace scorum::protocol;

// clang-format off
template <typename AccountService,
          typename ProposalService,
          typename CommiteeService,
          typename OperationType = scorum::protocol::operation>
class proposal_vote_evaluator_t : public evaluator<OperationType>
// clang-format on
{
public:
    //    typedef scorum::protocol::operation ;
    typedef proposal_vote_evaluator_t<AccountService, ProposalService, CommiteeService> EvaluatorType;
    typedef proposal_vote_operation operation_type;

    proposal_vote_evaluator_t(AccountService& account_service,
                              ProposalService& proposal_service,
                              CommiteeService& commitee_service,
                              uint32_t quorum)
        : _account_service(account_service)
        , _proposal_service(proposal_service)
        , _committee_service(commitee_service)
        , _quorum(quorum)
    {
    }

    virtual void apply(const OperationType& o) final override
    {
        auto* eval = static_cast<EvaluatorType*>(this);
        const auto& op = o.template get<typename EvaluatorType::operation_type>();
        eval->do_apply(op);
    }

    virtual int get_type() const override
    {
        return OperationType::template tag<typename EvaluatorType::operation_type>::value;
    }

    void do_apply(const proposal_vote_operation& op)
    {
        FC_ASSERT(_account_service.is_exists(op.voting_account), "Account \"${account_name}\" must exist.",
                  ("account_name", op.voting_account));

        FC_ASSERT(_proposal_service.is_exist(op.proposal_id), "There is no proposal with id '${id}'",
                  ("id", op.proposal_id));

        const proposal_vote_object& proposal = _proposal_service.get(op.proposal_id);

        FC_ASSERT(!_proposal_service.is_expired(proposal), "This proposal is expired '${id}'", ("id", op.proposal_id));

        _proposal_service.vote_for(proposal);

        const size_t members_count = _committee_service.get_members_count();

        if (check_quorum(proposal.votes, _quorum, members_count))
        {
            if (proposal.action == invite)
            {
                _committee_service.add_member(proposal.member);
            }
            else if (proposal.action == dropout)
            {
                _committee_service.exclude_member(proposal.member);
            }
            else
            {
                FC_ASSERT("Invalid proposal action type");
            }

            _proposal_service.remove(proposal);
        }
    }

protected:
    AccountService& _account_service;
    ProposalService& _proposal_service;
    CommiteeService& _committee_service;

    uint32_t _quorum;
};

typedef proposal_vote_evaluator_t<dbs_account, dbs_proposal, dbs_registration_committee> proposal_vote_evaluator;

} // namespace chain
} // namespace scorum