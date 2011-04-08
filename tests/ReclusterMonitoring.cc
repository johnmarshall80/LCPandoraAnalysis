/**
 *  @file   PandoraAnalysis/tests/ReclusterMonitoring.cc
 * 
 *  @brief  Implementation of pandora recluster monitoring binary.
 * 
 *  $Log: $
 */

#include "TFile.h"
#include "TH1F.h"
#include "TTree.h"

#include "AnalysisHelper.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace pandora_analysis;

void ReclusterMonitoring(TFile *pTFile, const unsigned int nRegionBins, const float maxValue, const std::string &outputRootFileName);

//------------------------------------------------------------------------------------------------------------------------------------------

int main(int argc, char **argv)
{
    const int nArgs(argc - 1);

    if ((nArgs < 1) || (nArgs > 4))
    {
        std::cout << std::endl
                  << "Usage: ./reclusterMonitoring inputFileName [nResBins] [maxResValue] [outputFileName]" << std::endl << std::endl
                  << "  inputFileName  : file containing pandora pfo analysis tree" << std::endl
                  << "  nResBins       : optional number of bins to cover resolution range, default 20 " << std::endl
                  << "  maxResValue    : optional maximum resolution value to consider, default 500" << std::endl
                  << "  outputFileName : optional output root file, for histogram output" << std::endl<< std::endl
                  << std::endl;
        return 1;
    }

    const std::string inputFileName(argv[1]);
    const int nResBins((nArgs >= 2) ? atoi(argv[2]) : 20);
    const float maxResValue((nArgs >= 3) ? atof(argv[3]) : 500.f);
    const std::string outputRootFileName((nArgs == 4) ? argv[4] : "");

    TFile *pTFile = new TFile(inputFileName.c_str(), "READ");
    ReclusterMonitoring(pTFile, nResBins, maxResValue, outputRootFileName);
    pTFile->Close();

    return 0;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void ReclusterMonitoring(TFile *pTFile, const unsigned int nRegionBins, const float maxValue, const std::string &outputRootFileName)
{
    // Define regions for which to create and investigate pfo energy spectra
    if (0 == nRegionBins)
        throw;

    float *pRegionBinEdges = new float[nRegionBins + 1];

    for (unsigned int i = 0; i <= nRegionBins; ++i)
        pRegionBinEdges[i] = static_cast<float>(i) * (maxValue / static_cast<float>(nRegionBins));

    TH1F *pEvsRHist = new TH1F("SigmaEvsRes", "#sigma(E) vs resolution", nRegionBins, pRegionBinEdges);
    pEvsRHist->SetYTitle("#sigma(E)");
    pEvsRHist->SetXTitle("sqrt(SumSquaredEnergyChanges)");

    // Book histograms
    TH1F **pRegionHistograms = new TH1F*[nRegionBins];

    for (unsigned int i = 0; i < nRegionBins; ++i)
    {
        std::ostringstream name, title;
        name << "fPFA_" << i;
        title << "TotalEnergy_" << pRegionBinEdges[i] << "-" << pRegionBinEdges[i + 1];
        pRegionHistograms[i] = new TH1F(name.str().c_str(), title.str().c_str(), 10000, 0., 5000.);
    }

    // Loop over entries in pfo analysis tree, creating energy spectra for specified regions
    TTree *pTTree = (TTree*)(pTFile->Get("PfoAnalysisTree"));
    const unsigned int nTreeEntries(pTTree->GetEntries());

    int qPdg(0);
    float pfoEnergyTotal(0.f), thrust(0.f), sumSquaredEnergyChanges(0.f);
    pTTree->SetBranchAddress("pfoEnergyTotal", &pfoEnergyTotal);
    pTTree->SetBranchAddress("qPdg", &qPdg);
    pTTree->SetBranchAddress("thrust", &thrust);
    pTTree->SetBranchAddress("sumSquaredEnergyChanges", &sumSquaredEnergyChanges);

    for (unsigned int iTree = 0; iTree < nTreeEntries; ++iTree)
    {
        pTTree->GetEntry(iTree);

        if ((qPdg < 1) || (qPdg > 3) || (thrust > 0.7f))
            continue;

        const float resolution(std::sqrt(sumSquaredEnergyChanges));

        for (unsigned int i = 0; i < nRegionBins; ++i)
        {
            if ((resolution >= pRegionBinEdges[i]) && (resolution < pRegionBinEdges[i + 1]))
                pRegionHistograms[i]->Fill(pfoEnergyTotal, 1.);
        }
    }

    // Extract performance figures from energy spectra histograms
    float sigma(0.f), sigmasigma(0.f);

    for (unsigned int i = 0; i < nRegionBins; ++i)
    {
        sigma = 0.f; sigmasigma = 0.f;
        AnalysisHelper::CalculatePerformance(pRegionHistograms[i], sigma, sigmasigma);
        pEvsRHist->SetBinContent(i + 1, sigma); pEvsRHist->SetBinError(i + 1, sigmasigma);
    }

    // Write histograms to output file, if specified
    if (!outputRootFileName.empty())
    {
        std::cout << "Will write histograms to file : " << outputRootFileName << std::endl;
        TFile *pTOutputFile = new TFile(outputRootFileName.c_str(), "RECREATE");
        pTOutputFile->cd();
        pEvsRHist->GetYaxis()->SetRangeUser(0.f, 1.f);
        pEvsRHist->Write();

        for (unsigned int i = 0; i < nRegionBins; ++i)
            pRegionHistograms[i]->Write();

        pTOutputFile->Close();
        delete pTOutputFile;
    }

    // Tidy up
    for (unsigned int i = 0; i < nRegionBins; ++i)
        delete pRegionHistograms[i];

    delete pEvsRHist;
    delete [] pRegionBinEdges;
    delete [] pRegionHistograms;
}