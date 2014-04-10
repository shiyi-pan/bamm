#include "StdOutDataWriter.h"
#include "Settings.h"
#include "Model.h"
#include "MCMC.h"

#include <iostream>
#include <iomanip>


StdOutDataWriter::StdOutDataWriter(Settings& settings) :
    _outputFreq(settings.get<int>("printFreq")),
    _headerWritten(false)
{
}


StdOutDataWriter::~StdOutDataWriter()
{
}


void StdOutDataWriter::writeData(int generation, MCMC& mcmc)
{
    if (!_headerWritten) {
        writeHeader();
        _headerWritten = true;
    }

    if (generation % _outputFreq != 0) {
        return;
    }

    Model& model = mcmc.model();

    std::cout << std::setw(12) << generation
              << std::setw(12) << model.getNumberOfEvents()
              << std::setw(12) << model.computeLogPrior()
              << std::setw(12) << model.getCurrentLogLikelihood()
              << std::setw(12) << model.getEventRate()
              << std::setw(12) << model.getMHAcceptanceRate()
              << std::endl;
}


void StdOutDataWriter::writeHeader()
{
    std::cout << header() << std::endl;
}


std::string StdOutDataWriter::header()
{
    return "  generation"
           "    N_shifts"
           "    logPrior"
           "      logLik"
           "   eventRate"
           "  acceptRate";
}