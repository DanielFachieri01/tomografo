#include <iostream>
#include <vector>
#include <omp.h>
#include <string>
#include <sstream>
#include <cstring>
#include <cmath>
#include <fstream>
#include <string>
#include <stdexcept>

#include <TFile.h>
#include <TTree.h>
#include "Garfield/MediumMagboltz.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/Sensor.hh"
#include "Garfield/TrackHeed.hh"
#include "TRandom3.h"
#include "Garfield/SolidTube.hh"
#include "Garfield/GeometrySimple.hh"
#include "Garfield/ComponentConstant.hh"

using namespace Garfield;
using namespace std;

//o xcom importa a energia em MeV ent tem que alterar o input de keV

struct Table { //organiza os vetores com acesso tipo Table.E ou Table.mu
    std::vector<double> E;   // energia em MeV
    std::vector<double> mu;  // mu/rho em cm^2/g
};

Table readXCOM(const std::string& filename) {
    Table tab;
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Erro ao abrir arquivo");

    double E, mu;
    while (file >> E >> mu) { // se a leitura deu certo, ent coloca na tabela do E e do mu
        tab.E.push_back(E);
        tab.mu.push_back(mu);
    }
    return tab;
}

double interpolate(const Table& tab, double E) { //pra eu poder ter range de energia maiores e melhores
    if (E < tab.E.front() || E > tab.E.back())
        throw std::out_of_range("Energia fora do range da tabela!");

    for (size_t i = 0; i < tab.E.size() - 1; i++) { // vai percorrer a tabela e verificar se ele é de algum dos pontos
        if (E >= tab.E[i] && E <= tab.E[i+1]) { //se não for, então vai calcular o dy/dx daquele ponto e dar um valor 'médio'
            double x0 = tab.E[i], x1 = tab.E[i+1];
            double y0 = tab.mu[i], y1 = tab.mu[i+1];
            return y0 + (E - x0) * (y1 - y0) / (x1 - x0);
        }
    }
    return tab.mu.back(); // fallback
}

//gaussianas para os picos característicos
inline double gaussian(double E, double E0, double amp, double sigma) {
    double x = (E - E0) / sigma;
    return amp * std::exp(-0.5 * x * x);
}


//espectro contínuo + picos característicos sem atenuação e não normalizado
double I(double E, double Emax = 50e-3, int Z = 47, double kalpha = 22.1e-3;
    double kbetha = 24.9e-3, double sigma = 0.25e-3) {
    double h = 4.1357e-15; //eV.s
    double c = 3e8; //m/s
    double amp = 1
    return 0.1*amp*E*(Emax-E) + gaussian(E, kalpha, amp, sigma) + gaussian(E, kbetha, amp/10, sigma);
}


// Função para preencher o histograma com a função I_normalized
TH1D* createEnergyHistogram(double E_min, double E_max, int bins = 4096, double Emax, double mu, double Length, int Z) {
    TH1D* hist = new TH1D("energy_hist", "Distribuição de Energia", bins, E_min, E_max);

    // Preencher o histograma
    for (int i = 1; i <= bins; i++) {
        double E = hist->GetBinCenter(i); // Centro do bin
        double mu = interpolate(tab, E); // Interpola o mu/rho para a energia E
        double value = I(E, Emax, mu, Length, Z)*std::exp(-mu*Lenght) // intensidade atenuada
        hist->SetBinContent(i, value); // Preenche o bin com o valor calculado 
    }
    return hist;
}


int main(int argc, char* argv[]) {
    TRandom3 rnd(2222); // semente fixa

    double Emax = 50e3;
    double E_min = 1e-3; // em MeV
    double Length = 100; // em cm

    // Criar o histograma de energia
    TH1D* energy_hist = createEnergyHistogram(E_min, E_max, bins, Emax, mu, Length, Z);

    // Normalizar o histograma
    energy_hist->Scale(1.0 / energy_hist->Integral("width"));

    TCanvas* canvas = new TCanvas("canvas", "Histograma de Energia", 800, 600);
    energy_hist->Draw();

    /*
    TFile* file = TFile::Open("no_gain.root", "RECREATE");

    std::vector<std::tuple<double, double, double, std::string>> conditions = {
        {1.0, 1, 0.70, "Ar"},
        {3.0, 1, 0.70, "Ar"},
        {3.0, 2, 0.70, "Ar"},
        {1.0, 1, 0.90, "Kr"},
        {3.0, 1, 0.90, "Kr"},
    };

    for (const auto& condition : conditions) {
        auto [window, pressure, proportion, tipo] = condition;

        // Nome da TTree
        std::ostringstream oss;
        oss << tipo << "_" << int(proportion * 100) << "_" << 100 - int(proportion * 100)
            << "_" << pressure << "atm_" << window << "cm";
        std::string treeName = oss.str();

        // Criação da árvore e variável
        TTree* tree = new TTree(treeName.c_str(), "Número de elétrons primários");
        int gain = 0;
        tree->Branch("gain", &gain);

        MediumMagboltz* gas = new MediumMagboltz();
        gas->SetTemperature(293.15);
        gas->SetPressure(pressure * 760.);
        gas->EnableDrift();
        gas->SetComposition(tipo, proportion, "co2", 1 - proportion);
        gas->Initialise();

        constexpr double diameter = 10;
        double length = window / 2;
        SolidTube tube(0, 0, 0, 0.5 * diameter, length);

        GeometrySimple* geo = new GeometrySimple();
        geo->AddSolid(&tube, gas);

        ComponentConstant* field = new ComponentConstant();
        field->SetGeometry(geo);
        field->SetElectricField(0., 0., 500.);

        Sensor* sensor = new Sensor();
        sensor->AddComponent(field);

        const int nevents = 100000;
        TrackHeed* track = new TrackHeed();
        track->SetSensor(sensor);

        for (int idx = 0; idx < nevents; idx++) {
            gain = 0;  // zera para o novo evento

            double gamma_origin = rnd.Uniform(0, 1);
            double x, y, z, dx, dy, dz;

            if (gamma_origin < 0.88) {
                x = rnd.Uniform(-0.035, 0.035);
                y = rnd.Uniform(-0.035, 0.035);
                z = length / 2 - 1e-4;
                dx = rnd.Uniform(-0.01, 0.01);
                dy = rnd.Uniform(-0.01, 0.01);
                dz = -1;
                track->TransportPhoton(x, y, z, idx, gamma_Energy(rnd, emax), dx, dy, dz, gain);
            } else {
                x = rnd.Uniform(-0.035, 0.035);
                y = rnd.Uniform(-0.035, 0.035);
                z = -length + 1e-4;
                dx = rnd.Uniform(-1, 1);
                dy = rnd.Uniform(-1, 1);
                dz = rnd.Uniform(0, 1);
                track->TransportPhoton(x, y, z, idx, gamma_cu(rnd), dx, dy, dz, gain);
            }

            if (idx % 10000 == 0) cout << idx << "/" << nevents << " (" << treeName << ")" << endl;

            if (gain > 0) tree->Fill();
        }

        tree->Write();
        cout << "TTree " << treeName << " escrita com " << tree->GetEntries() << " entradas." << endl;

        delete gas;
        delete geo;
        delete field;
        delete sensor;
        delete track;
    }

    file->Close();
    cout << "Todas as árvores foram salvas em no_gain.root" << endl;
    return 0;
    */
   app.Run();
}
