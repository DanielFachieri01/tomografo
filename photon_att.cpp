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

#include "Garfield/MediumMagboltz.hh"
#include "Garfield/Medium.hh"
#include "Garfield/SolidBox.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/Sensor.hh"
#include "Garfield/TrackHeed.hh"
#include "TRandom3.h"
#include "Garfield/GeometrySimple.hh"
#include "Garfield/ComponentConstant.hh"

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
TH1D* createEnergyHistogram(const Table& tab,const Table& tabkap, double E_min, double E_max, int bins,
                            double Emax, double Length, int Z, double airdensity,
                            double kaptondensity, double kaptonwindow) {
    TH1D* hist = new TH1D("energy_hist", "Distribuicao de Energia", bins, E_min, E_max);

    for (int i = 1; i <= bins; i++) {
        double E = hist->GetBinCenter(i);
        double mu_air = interpolate(tab, E) * airdensity; // mu em cm^-1~
        double mu_kapton = interpolate(tabkap, E) * kaptondensity; // mu em cm^-1~
        double value = I(E, Emax) * std::exp(-mu_air * Length) * std::exp(-mu_kapton * kaptonwindow);
        hist->SetBinContent(i, value);
    }
    return hist;
}

int main(int argc, char * argv[]){
    TRandom3 rnd(2222); // semente fixa

    // Parâmetros
    double Emax = 50e-3; // MeV
    double E_min = 1e-3; // MeV
    double E_max = 50e-3; // MeV
    int bins = 4096 * 0.5;
    double Length = 100; // cm
    int Z = 47;
    double airdensity = 0.0012041; // g/cm^3
    double kaptondensity = 1.42; // g/cm^3
    double kaptonwindow = 50e-4; // cm

    // Carregar tabela de atenuação
    Table tab = readXCOM("attenuation.txt"); //atenuação do ar
    Table tabkap = readXCOM("attenuation_kapton.txt");  //atenuação do kapton

    // Criar o histograma de energia
    TH1D* energy_hist = createEnergyHistogram(tab, E_min, E_max, bins, Emax, Length, Z, airdensity, kaptondensity, kaptonwindow);

    TH1D* energy_hist_wo = new TH1D("energy_hist_wo", "Distribuicao de Energia sem Atenuacao", bins/2, E_min, E_max);
    energy_hist_wo->GetXaxis()->SetTitle("Energia (MeV)");
    energy_hist_wo->GetYaxis()->SetTitle("Contagens");
    energy_hist_wo->SetLineColor(kRed);
    energy_hist_wo->Smooth(1);
    energy_hist_wo->SetTitle("Espectro de Energia sem Atenuacao");

    for (int i = 0; i< 1000000; i++){
        energy_hist_wo->Fill(energy_hist->GetRandom());
    }

    //Salvar o histograma
    TCanvas* canvas = new TCanvas("canvas", "Histograma de Energia", 800, 600);
    energy_hist_wo->Draw();
    canvas->SaveAs("energy_histogram.pdf");

    cout << "Histograma salvo como 'energy_histogram.pdf'" << endl;
    

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

        // Criação da árvore e variável de armazenamento
        TTree* tree = new TTree(treeName.c_str(), "Número de elétrons primários");
        int gain = 0;
        tree->Branch("gain", &gain);

        // Criar o gás
        MediumMagboltz* gas = new MediumMagboltz();
        gas->SetTemperature(293.15); // Temperatura em K
        gas->SetPressure(760.);      // Pressão em Torr
        gas->SetComposition("ar", 0.7, "co2", 0.3); // 70% Ar, 30% CO2
        gas->EnableDrift();
        gas->Initialise();

        // Criar a folha de cobre
        Medium* solid = new Medium();
        solid->SetName("Cobre");
        solid->SetDensity(8.96); // Densidade em g/cm³ cobre
        solid->SetAtomicNumber(29); // Número atômico cobre
        solid->SetAtomicWeight(63.546); // Peso atômico exemplo: cobre
        solid->SetDielectricConstant(1.0e11); // Constante dielétrica
        solid->IsConductor(); // Definir como condutor

        // Bloco de gás
        constexpr double Height = 10.0;
        constexpr double Width = 10.0; // Diâmetro do tubo (cm)
        constexpr double gasLength = window;    // Comprimento do cubo de gás(cm)
        SolidBox* gasBox = new SolidBox(0, 0, 0, 0.5 * Width, 0.5 * Height, 0.5 * gasLength);

        // Bloco da camada de cobre
        constexpr double solidDepth = 50e-4;  // Profundidade do bloco (cm)
        SolidBox* solidBox = new SolidBox(0, 0, -0.5*gasLength, 0.5 * Width, 0.5 * Height, 0.5 * solidDepth);

        // Criar a geometria completa
        GeometrySimple* geo = new GeometrySimple();
        geo->AddSolid(gasBox, gas);   // Associar o gás ao volume do tubo
        geo->AddSolid(solidBox, solid); // Associar o sólido ao volume do bloco

        // Definir o campo elétrico
        ComponentConstant* field = new ComponentConstant();
        field->SetGeometry(geo);
        field->SetElectricField(0., 0., 500.); // Campo elétrico em V/cm

        Sensor* sensor = new Sensor();
        sensor->AddComponent(field);

        const int nevents = 100000;
        TrackHeed* track = new TrackHeed();
        track->SetSensor(sensor);
        double xCluster, yCluster, zCluster;
        Medium* medium = nullptr;

        for (int idx = 0; idx < nevents; idx++) {
            gain = 0;  // zera para o novo evento
            double x, y, z, dx, dy, dz;
            //photons vindo do tubo de raios-X
            x = rnd.Uniform(-Width, Width);
            y = rnd.Uniform(-Height, Height);
            z = gasLength/2 - 1e-4;
            dx = 0;
            dy = 0;
            dz = -1;
            track->TransportPhoton(x, y, z, idx, energy_hist->GetRandom(), dx, dy, dz, gain);
            if (gain >0){
                track->GetCluster(0, xCluster, yCluster, zCluster);
                if (sensor->GetMedium(0,0,zCluster,medium)){ // se aconteceu interação dentro do volume onde existiria o cobre
                    if(medium->IsConductor()){
                    dx = rnd.Uniform(-1, 1);
                    dy = rnd.Uniform(-1, 1);
                    dz = rnd.Uniform(-1, 1);
                    double dr = sqrt(dx * dx + dy * dy + dz * dz);
                    dx /= dr;
                    dy /= dr;
                    dz /= dr; // normalizar
                    double costheta = (dz / dr);
                    double L_cu = zCluster * costheta;
                    double mu_cu = 2.784e2 * 8.96; // mu/rho * rho, mu em cm^-1
                    double prob_att = exp(-L_cu * mu_cu); // mu em cm^-1
                    double r = rnd.Uniform(0, 1);
                    if (r < prob_att) {// se não foi atenuado
                        track->TransportPhoton(xCluster, yCluster, zCluster, idx, 8.9789e3, dx, dy, dz, gain);
                    }
                }
            }
            if (idx % 10000 == 0) cout << idx << "/" << nevents << " (" << treeName << ")" << endl;

            tree->Fill();
        }

        }

        tree->Write();
        cout << "TTree " << treeName << " escrita com " << tree->GetEntries() << " entradas." << endl;
        delete gas;
        delete gasBox;
        delete solidBox;
        delete solid;
        delete medium;
        delete geo;
        delete field;
        delete sensor;
        delete track;


    }

    file->Close();
    cout << "Todas as árvores foram salvas em no_gain.root" << endl;
    return 0;
}


