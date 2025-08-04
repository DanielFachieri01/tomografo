#include <iostream>
#include <vector>
#include <omp.h>
#include <string>
#include <sstream>
#include <cstring>

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

double fundo(TRandom3& rnd, double emax) {
    while (true) {
        double E = rnd.Uniform(1000., emax);
        double I = (emax = E) / E;
        double prob = rnd.Uniform(0., (emax - 1000.) / 1000.);
        if (prob < I) return E;
    }
}

double gamma_ag(TRandom3& rnd) {
    double r = rnd.Uniform(0, 100);
    if (r < 66.7) return 22163.0;
    else if (r < 91.7) return 21983.0;
    else return 25136.0;
}

double gamma_cu(TRandom3& rnd) {
    double r = rnd.Uniform(0, 100);
    if (r < 66.7) return 8047.8;
    else if (r < 91.7) return 8027.8;
    else return 8905.3;
}

double gamma_Energy(TRandom3& rnd, double emax) {
    double r = rnd.Uniform(0, 100);
    if (r < 10) return fundo(rnd, emax);
    else return gamma_ag(rnd);
}

int main(int argc, char* argv[]) {
    TRandom3 rnd(2222); // semente fixa

    double emax = 50e3;

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
}
