/*
 *  TraitMCMC.h
 *  BAMMt
 *
 *  Created by Dan Rabosky on 6/20/12.

 *
 */

#ifndef TRAIT_MCMC_H
#define TRAIT_MCMC_H

#include <stdlib.h>
#include <string>
#include <vector>
#include <iosfwd>

class MbRandom;
class TraitModel;
class Settings;


class TraitMCMC
{

public:

    TraitMCMC(MbRandom* ran, TraitModel* mymodel, Settings* sp);
    ~TraitMCMC(void);

    void writeStateToFile(void);
    void printStateData(void);
    void writeBranchBetaRatesToFile(void);
    void writeNodeStatesToFile(void);
    void writeEventDataToFile(void);

    int  pickParameterClassToUpdate(void);
    void updateState(int parm);

    void setUpdateWeights(void);

    void writeParamAcceptRates(void);

private:

    void writeHeaderToStream(std::ostream& outStream);
    void writeStateToStream(std::ostream& outStream);

    MbRandom* ranPtr;
    TraitModel* ModelPtr;
    Settings* sttings;

    std::vector<double> parWts;

    std::vector<int> acceptCount;
    std::vector<int> rejectCount;

    std::string mcmcOutfile;
    std::string betaOutfile;
    std::string nodeStateOutfile;
    std::string acceptFile;
    std::string eventDataFile;

    int _treeWriteFreq;
    int _mcmcWriteFreq;
    int _eventDataWriteFreq;
    int _acceptWriteFreq;
    int _printFreq;
    int _NGENS;

    bool _firstLine;
};


#endif
