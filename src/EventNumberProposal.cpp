#include "EventNumberProposal.h"
#include "MbRandom.h"
#include "Model.h"


EventNumberProposal::EventNumberProposal(MbRandom& rng, Model& model) :
    _rng(rng), _model(model)
{
}


void EventNumberProposal::propose()
{
    _currentEventCount = _model.getNumberOfEvents();
    _currentLogLikelihood = _model.getCurrentLogLikelihood();
    _currentLogPrior = _model.computeLogPrior();

    bool shouldAddEvent = (_currentEventCount == 0) || (_rng.uniformRv() < 0.5);

    if (shouldAddEvent) {
        _lastEventChanged = _model.addRandomEventToTree();
        _lastProposal = AddEvent;
    } else {
        _lastEventChanged = _model.removeRandomEventFromTree();
        _lastProposal = RemoveEvent;
    }

    _model.setMeanBranchParameters();

    _proposedEventCount = _model.getNumberOfEvents();
    _proposedLogLikelihood = _model.computeLogLikelihood();
    _proposedLogPrior = _model.computeLogPrior();
}


void EventNumberProposal::accept()
{
    if (_lastProposal == RemoveEvent) {
        if (_lastEventChanged != NULL) {
            delete _lastEventChanged;
            _lastEventChanged = NULL;
        }
    }

    _model.setCurrentLogLikelihood(_proposedLogLikelihood);
}


void EventNumberProposal::reject()
{
    if (_lastProposal == AddEvent) {
        _model.removeEventFromTree(_lastEventChanged);
        delete _lastEventChanged;
        _lastEventChanged = NULL;
    } else if (_lastProposal == RemoveEvent) {
        _model.addEventToTree(_lastEventChanged);
    }
}


double EventNumberProposal::acceptanceRatio()
{
    if (_lastProposal == AddEvent &&
        !_model.isEventConfigurationValid(_lastEventChanged)) {
        return 0.0;
    }

    double logLikelihoodRatio = computeLogLikelihoodRatio();
    double logPriorRatio = computeLogPriorRatio();
    double logQRatio = computeLogQRatio();

    double t = _model.getTemperatureMH();
    double logRatio = t * (logLikelihoodRatio + logPriorRatio) + logQRatio;

    return std::min(1.0, std::exp(logRatio));
}


double EventNumberProposal::computeLogLikelihoodRatio()
{
    return _proposedLogLikelihood - _currentLogLikelihood;
}


double EventNumberProposal::computeLogPriorRatio()
{
    if (_lastProposal == AddEvent) {
        return _proposedLogPrior - _currentLogPrior +
            std::log(_model.getEventRate()) - std::log(_proposedEventCount);
    } else {
        return _proposedLogPrior - _currentLogPrior +
            std::log(_proposedEventCount) - std::log(_model.getEventRate());
    }
}


double EventNumberProposal::computeLogQRatio()
{
    if (_lastProposal == AddEvent) {
        // -0.6931... is ln 0.5
        double logQRatio = (_currentEventCount > 0) ? 0.0 : -0.69314718055995;
        return logQRatio - _model.logQRatioJump();
    } else {
        // 0.6931... is ln 2.0
        double logQRatio = (_currentEventCount != 1) ? 0.0 : 0.69314718055995;
        return logQRatio + _model.logQRatioJump();
    }
}
