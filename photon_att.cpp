#include <iostream>
#include <vector>
//#include <omp.h>
#include <string>
#include <sstream>
#include <cstring>
#include <cmath>
#include <fstream>
#include <stdexcept>

#include <TFile.h>
#include <TTree.h>
#include <TH1D.h>
#include <TCanvas.h>
/*
#include "Garfield/MediumMagboltz.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/Sensor.hh"
#include "Garfield/TrackHeed.hh"
#include "TRandom3.h"
#include "Garfield/SolidTube.hh"
#include "Garfield/GeometrySimple.hh"
#include "Garfield/ComponentConstant.hh"
*/
using namespace std;

// Estrutura para tabela de atenuação
struct Table {
    std::vector<double> E;   // energia em MeV
    std::vector<double> mu;  // mu/rho em cm^2/g
};

Table readXCOM(const std::string& filename) {
    Table tab;
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Erro ao abrir arquivo");

    double E, mu;
    while (file >> E >> mu) {
        tab.E.push_back(E);
        tab.mu.push_back(mu);
    }
    return tab;
}

double interpolate(const Table& tab, double E) {
    if (E < tab.E.front() || E > tab.E.back())
        throw std::out_of_range("Energia fora do range da tabela!");

    for (size_t i = 0; i < tab.E.size() - 1; i++) {
        if (E >= tab.E[i] && E <= tab.E[i+1]) {
            double x0 = tab.E[i], x1 = tab.E[i+1];
            double y0 = tab.mu[i], y1 = tab.mu[i+1];
            return y0 + (E - x0) * (y1 - y0) / (x1 - x0);
        }
    }
    return tab.mu.back();
}

// Gaussiana para picos característicos
inline double gaussian(double E, double E0, double amp, double sigma) {
    double x = (E - E0) / sigma;
    return amp * std::exp(-0.5 * x * x);
}

// Espectro contínuo + picos característicos sem atenuação e não normalizado
double I(double E, double Emax = 50e-3, double kalpha = 22.1e-3,
         double kbetha = 24.9e-3, double sigma = 0.25e-3) {
    double amp = 2.0; // Amplitude arbitrária para o espectro contínuo
    return ( E * (Emax - E))*log(Emax/E)+ gaussian(E, kalpha, amp*1e-3, sigma) + gaussian(E, kbetha, amp*1e-3/3, sigma);
}

// Função para preencher o histograma com a função I atenuada
TH1D* createEnergyHistogram(const Table& tab, double E_min, double E_max, int bins,
                            double Emax, double Length, int Z, double airdensity) {
    TH1D* hist = new TH1D("energy_hist", "Distribuicao de Energia", bins, E_min, E_max);

    for (int i = 1; i <= bins; i++) {
        double E = hist->GetBinCenter(i);
        double mu = interpolate(tab, E)*airdensity; // mu em cm^-1
        double value = I(E, Emax)*std::exp(-mu * Length);
        hist->SetBinContent(i, value);
    }
    return hist;
}

void photon_att() {
    TRandom3 rnd(2222); // semente fixa

    // Parâmetros
    double Emax = 50e-3; // MeV
    double E_min = 1e-3; // MeV
    double E_max = 50e-3; // MeV
    int bins = 4096 * 0.5;
    double Length = 100; // cm
    int Z = 47;
    double airdensity = 0.0012041; // g/cm^3

    // Carregar tabela de atenuação
    Table tab = readXCOM("attenuation.txt"); // ajuste o nome do arquivo conforme necessário

    // Criar o histograma de energia
    TH1D* energy_hist = createEnergyHistogram(tab, E_min, E_max, bins, Emax, Length, Z, airdensity);
    energy_hist->GetXaxis()->SetTitle("Energia (MeV)");
    energy_hist->GetYaxis()->SetTitle("Intensidade (a.u.)");
    energy_hist->SetLineColor(kBlue);
    energy_hist->Smooth(1);
    energy_hist->SetTitle("Espectro de Energia com Atenuacao");

    TH1D* energy_hist_wo = new TH1D("energy_hist_wo", "Distribuicao de Energia sem Atenuacao", bins/2, E_min, E_max);
    energy_hist_wo->GetXaxis()->SetTitle("Energia (MeV)");
    energy_hist_wo->GetYaxis()->SetTitle("Contagens");
    energy_hist_wo->SetLineColor(kRed);
    energy_hist_wo->Smooth(1);
    energy_hist_wo->SetTitle("Espectro de Energia sem Atenuacao");

    for (int i = 0; i< 1000000; i++){
        energy_hist_wo->Fill(energy_hist->GetRandom());
    }

    // Salvar o histograma como PDF
    TCanvas* canvas = new TCanvas("canvas", "Histograma de Energia", 800, 600);
    energy_hist->Draw();
    canvas->SaveAs("energy_histogram.pdf");

    TCanvas* canvas2 = new TCanvas("canvas", "Histograma de Energia", 800, 600);
    energy_hist_wo->Draw();
    //canvas2->SaveAs("energy_histogram.pdf");

    cout << "Histograma salvo como 'energy_histogram.pdf'" << endl;
}
