#ifndef EVENT_NUMBER_PROPOSAL_H
#define EVENT_NUMBER_PROPOSAL_H


#include "Proposal.h"

class MbRandom;
class Model;
class BranchEvent;


class EventNumberProposal : public Proposal
{
    // TODO: Future: Add and Remove events should be separate Proposals
    enum ProposalType {
        AddEvent,
        RemoveEvent
    };

public:

    EventNumberProposal(MbRandom& rng, Model& model);

    virtual void propose();
    virtual void accept();
    virtual void reject();

    virtual double acceptanceRatio();

private:

    double computeLogLikelihoodRatio();
    double computeLogPriorRatio();
    double computeLogQRatio();

    MbRandom& _rng;
    Model& _model;

    int _currentEventCount;
    double _currentLogLikelihood;
    double _currentLogPrior;

    int _proposedEventCount;
    double _proposedLogLikelihood;
    double _proposedLogPrior;

    ProposalType _lastProposal;
    BranchEvent* _lastEventChanged;
};


#endif
