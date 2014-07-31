//
//  FastSimulatePrior.cpp
//  bamm
//
//  Created by Dan Rabosky on 11/29/13.
//  Copyright (c) 2013 Dan Rabosky. All rights reserved.
//

#include "FastSimulatePrior.h"

#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <vector>

#include "Random.h"
#include "Settings.h"
#include "Stat.h"
#include "EventCountLog.h"


FastSimulatePrior::FastSimulatePrior(Random& random, Settings* sp) :
    _random(random), sttings(sp)
{

    /**** New params May 23 2014 ****/

    _maxEvents = sttings->get<double>("maxNumberEvents");
    _intervalGens = sttings->get<double>("priorSim_IntervalGenerations");


    // End new params

    _generations = 0;

    _eventRate = 1 / sttings->get<double>("poissonRatePrior");

    _updateEventRateScale = sttings->get<double>("updateEventRateScale");

    _poissonRatePrior = sttings->get<double>("poissonRatePrior");
    _numberEvents = 0;

    /********* EventCountLog stuff *****************/
    // Setting up vector of event transition data...
    for (int i = 0; i <= _maxEvents; i++){
        EventCountLog *ecl = new EventCountLog(i);
        _TrackingVector.push_back(ecl);
    }


    // Cannot figure out why new way and old ways of doing this
    // give different results, hence leave "old way" as the default.
    // fastSimulatePriorExperimental may ultimately work better
    // but for now, I (DLR) find slight discrepancies between distributions
    // generated by these that I cannot yet reconcile


    if (sttings->get<bool>("fastSimulatePriorExperimental")){
        fastSimulatePriorExperimental();
    }else{
        fastSimulatePriorOldWay();
    }




}

FastSimulatePrior::~FastSimulatePrior()
{
    _fspOutStream.close();
}


void FastSimulatePrior::fastSimulatePriorExperimental()
{

    std::cout << "\nSimulating prior distribution with poissonRatePrior = << ";
    std::cout << _poissonRatePrior << " >>\n" << std::endl;


    for (int i = 1; i < _maxEvents; i++){
        // Must set the number of events:
        _numberEvents = _random.uniformInteger(i-1, i);

        for (int k = 0; k < _intervalGens; k++){

            updateState(i-1, i);

        }
    }

    //std::cout << "Writing transition probs..." << std::endl;
    //writeTransitionProbsToFile();

    writePriorProbsToFile_Experimental();
    std::cout << "Done generating prior distribution\n" << std::endl;


}


void FastSimulatePrior::fastSimulatePriorOldWay()
{

    //_outfileName = sttings->get("priorOutputFileName");
    // Open streams for writing
    //_fspOutStream.open(_outfileName.c_str());

    //writeHeaderToOutputFile();




    std::cout << "\nSimulating prior distribution on shifts....\n" << std::endl;
    std::cout << "Progress: 0% ";
    std::cout.flush();



    int fints =
        round(sttings->get<double>("fastSimulatePrior_Generations") / 32.0);

    double burnin = sttings->get<double>("fastSimulatePrior_BurnIn");
    int samplestart =
        round(burnin * sttings->get<double>("fastSimulatePrior_Generations"));

    // Burnin phase:
    for (int i = 0; i < samplestart; i++){
        updateState();
    }

    // sampling phase:
    for (int i = samplestart; i < sttings->get<int>("fastSimulatePrior_Generations"); i++){

        _TrackingVector[_numberEvents]->incrementInStateCount();

        updateState();

        // No longer writing state to file
        //if ((i % sttings->get<int>("fastSimulatePrior_SampleFreq")) == 0){
        //  writeStateTofile();
        // }

        if ((i % fints) == 0){

            if (i == (fints*16)){
                std::cout << " 50% ";
            }

            std::cout << "|";
            std::cout.flush();
        }

    }

    writePriorProbsToFile_OldWay();

    std::cout << " 100% \n\nDone...Results written to file << ";
    std::cout << sttings->get("priorOutputFileName");
    std::cout << " >>\n" << std::endl;


}


void FastSimulatePrior::writeStateTofile()
{
    writeStateToStream(_fspOutStream);
}

void FastSimulatePrior::writeStateToStream(std::ostream &outStream)
{
    outStream   << _generations      << ","
                << _numberEvents    << ","
                << _eventRate       << std::endl;
}

void FastSimulatePrior::updateEventRateMH()
{

    double oldEventRate = getEventRate();

    double cterm = exp( _updateEventRateScale * (_random.uniform() - 0.5) );
    setEventRate(cterm * oldEventRate);

    double LogPriorRatio =
        Stat::lnExponentialPDF(getEventRate(), _poissonRatePrior) -
        Stat::lnExponentialPDF(oldEventRate, _poissonRatePrior);
    double logProposalRatio = log(cterm);

    double logHR = LogPriorRatio + logProposalRatio;
    const bool acceptMove = acceptMetropolisHastings(logHR);


    if (acceptMove == false){
        setEventRate(oldEventRate);
    }

}

void FastSimulatePrior::changeNumberOfEventsMH(int min, int max)
{
    bool acceptMove = false;

    // Current number of events on the tree, not counting root state:
    double K = (double)(_numberEvents);

    bool gain = _random.trueWithProbability(0.5);
    if (K == min) {
        // set event to gain IF on boundary
        gain = true;
    }else if (K == max) {
        gain = false;
    }

    if (gain) {

        double qratio = 1.0;
        if (K == min && max != (K + 1)) {
            // event count at minimum but upper bound is greater.
            // can only propose gains.
            qratio = 0.5;
        } else if (K == (max - 1) && K != min){
            qratio = 2.0;
        }

        // Prior ratio is eventRate / (k + 1)
        // but now, eventCollection.size() == k + 1
        //  because event has already been added.
        // Here HR is just the prior ratio

        double logHR = log(_eventRate) - log(K + 1.0);

        // Now add log qratio

        logHR += log(qratio);
        acceptMove = acceptMetropolisHastings(logHR);

        _TrackingVector[_numberEvents]->incrementAddProposeCount();
        if (_numberEvents == _maxEvents - 1){
            std::cout << " Max number of events exceeded" << std::endl;
            exit(1);
        }

        if (acceptMove) {
            _TrackingVector[_numberEvents]->incrementAddAcceptCount();

            _numberEvents++;

            // If ACCEPT GAIN or if REJECT LOSS
            //      you are still in state i, otherwise not.

            _TrackingVector[_numberEvents]->incrementInStateCount();
        }



    } else {

        double qratio = 1.0; // if loss, can only be qratio of 1.0
        if (K  == (min + 1) && K != max ){
            qratio = 2.0;
        }else if (K == max && (K - 1) != min) {
            qratio = 0.5;
        }


        // This is probability of going from k to k-1
        // So, prior ratio is (k / eventRate)

        // First get prior ratio:
        double logHR = log(K) - log(_eventRate);

        // Now correct for proposal ratio:
        logHR += log(qratio);

        acceptMove = acceptMetropolisHastings(logHR);

        _TrackingVector[_numberEvents]->incrementSubtractProposeCount();
        if (acceptMove) {

            _TrackingVector[_numberEvents]->incrementSubtractAcceptCount();
            _numberEvents--;

        }else{
            // If GAIN or if REJECT LOSS
            //      you are still in state i, otherwise not.
            _TrackingVector[_numberEvents]->incrementInStateCount();
        }

    }



}



void FastSimulatePrior::changeNumberOfEventsMH()
{

    bool acceptMove = false;

    // Propose gains & losses equally if not on boundary (n = 0) events:

    // Current number of events on the tree, not counting root state:
    double K = (double)(_numberEvents);

    bool gain = _random.trueWithProbability(0.5);
    if (K == 0) {
        // set event to gain IF on boundary
        gain = true;
    }

    // now to adjust acceptance ratios:

    if (gain) {

        double qratio = 1.0;
        if (K == 0) {
            // no events on tree
            // can only propose gains.
            qratio = 0.5;
        } else {
            // DO nothing.
        }

        // Prior ratio is eventRate / (k + 1)
        // but now, eventCollection.size() == k + 1
        //  because event has already been added.
        // Here HR is just the prior ratio

        double logHR = log(_eventRate) - log(K + 1.0);

        // Now add log qratio

        logHR += log(qratio);
        acceptMove = acceptMetropolisHastings(logHR);

        if (_numberEvents == _maxEvents - 1){
            std::cout << " Max number of events exceeded" << std::endl;
            exit(1);
        }

        if (acceptMove) {

            _numberEvents++;

        }

    } else {

        double qratio = 1.0; // if loss, can only be qratio of 1.0
        if (K  == 1)
            qratio = 2.0;

        // This is probability of going from k to k-1
        // So, prior ratio is (k / eventRate)

        // First get prior ratio:
        double logHR = log(K) - log(_eventRate);

        // Now correct for proposal ratio:
        logHR += log(qratio);

        acceptMove = acceptMetropolisHastings(logHR);

        if (acceptMove) {

            _numberEvents--;

        }

    }

}


void FastSimulatePrior::updateState(int min, int max)
{
    updateEventRateMH();
    changeNumberOfEventsMH(min, max);
    incrementGeneration();
}

void FastSimulatePrior::updateState()
{

    updateEventRateMH();

    changeNumberOfEventsMH();

    incrementGeneration();
}


bool FastSimulatePrior::acceptMetropolisHastings(const double lnR)
{
    const double r = exp(lnR);
    return _random.trueWithProbability(r);
}


void FastSimulatePrior::writeHeaderToOutputFile()
{
    _fspOutStream << "generation,N_shifts,eventRate" << std::endl;
}

// writeTransitionProbsToFile writes raw data associated with
// transitioning between models to allow calculation of prior.
void FastSimulatePrior::writeTransitionProbsToFile()
{
    std::string outname = "prior_transitions.txt";
    std::ofstream _priorfile;
    _priorfile.open(outname.c_str());

    _priorfile << "count,prop_loss,accept_loss,prop_gain,accept_gain" << std::endl;

    for (int i = 0; i < (int)_TrackingVector.size(); i++){
        _priorfile << _TrackingVector[i]->getEventCount() << ",";
        _priorfile << _TrackingVector[i]->getSubtractProposeCount() << ",";
        _priorfile << _TrackingVector[i]->getSubtractAcceptCount() << ",";
        _priorfile << _TrackingVector[i]->getAddProposeCount() << ",";
        _priorfile << _TrackingVector[i]->getAddAcceptCount() << std::endl;

    }

    _priorfile.close();

}


void FastSimulatePrior::writePriorProbsToFile_OldWay()
{

    double totalcount = 0;
    for (int i = 0; i <= sttings->get<int>("maxNumberEvents"); i++){
        totalcount += _TrackingVector[i]->getInStateCount();
    }



    std::string outname = sttings->get("priorOutputFileName");
    //std::string outname = "prior_probs.csv";
    std::ofstream _priorfile;
    _priorfile.open(outname.c_str());
    _priorfile << "N_shifts,prob" << std::endl;



    for (int i = 0; i <= sttings->get<int>("maxNumberEvents"); i++){
        double prob = ((double)_TrackingVector[i]->getInStateCount()) / totalcount;
        _priorfile << i << "," << prob << std::endl;

    }

    _priorfile.close();


}


void FastSimulatePrior::writePriorProbsToFile_Experimental()
{

    // Must deal better with these for lower bounds.
    int threshold_prop = 1;
    int threshold_acc = 1;
    int maxx = 0;

    for (int i = 1; i < (int)_TrackingVector.size(); i++){
        // Here need to check if counts are sufficient large to get non-zeros.
        bool c1 = _TrackingVector[i]->getAddProposeCount() < threshold_prop;
        bool c2 = _TrackingVector[i]->getSubtractProposeCount() < threshold_prop;
        bool c3 = _TrackingVector[i]->getAddAcceptCount() < threshold_acc;
        bool c4 = _TrackingVector[i]->getSubtractAcceptCount() < threshold_acc;
        if (c1 | c2 | c3 | c4){
            maxx = i;
            break;
        }
    }


    std::vector<double> relprob;
    std::vector<double> cumsumlog;
    for (int i = 0; i < (int)_TrackingVector.size(); i++){
        relprob.push_back(0.0);
        cumsumlog.push_back(0.0);
    }

    for (int i = 1; i < maxx; i++){

        double num = (double)_TrackingVector[i-1]->getAddAcceptCount() / (double)_TrackingVector[i-1]->getAddProposeCount();
        double denom = (double)_TrackingVector[i]->getSubtractAcceptCount() / (double) _TrackingVector[i]->getSubtractProposeCount();


        relprob[i] = num / denom;

        //double prop_i = (double)_TrackingVector[i]->getInStateCount() / (double)sttings->get<double>("priorSim_IntervalGenerations");

        //double relprob2 = prop_i / (1 - prop_i);

        //relprob[i] = (double)_TrackingVector[i]->getInStateCount() / (double)sttings->get<double>("priorSim_IntervalGenerations");

        //std::cout << "RP1: " << relprob[i] << "\tRP2: " << relprob2 << std::endl;

        cumsumlog[i] = std::log(relprob[i]) + cumsumlog[i-1];

    }

    double tmpsum = 0.0;
    for (int i = 1; i < maxx; i++){
        tmpsum += std::exp(cumsumlog[i]);
    }

    double P0 = 1 / tmpsum;

    std::string outname = sttings->get("priorOutputFileName");
    //std::string outname = "prior_probs.csv";
    std::ofstream _priorfile;
    _priorfile.open(outname.c_str());
    _priorfile << "N_shifts,prob" << std::endl;
    _priorfile << "0," << P0 << std::endl;

    for (int i = 1; i < maxx; i++){
        double prob = P0 * std::exp(cumsumlog[i]);
        _priorfile << i << "," << prob << std::endl;
    }



    _priorfile.close();

}







