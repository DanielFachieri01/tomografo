#include <iostream>
#include <vector>
#include <omp.h>
#include <string>
#include <sstream>

#include <TFile.h>
#include <TTree.h>

#include "Garfield/MediumMagboltz.hh"
#include "Garfield/ComponentElmer.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/Sensor.hh"

using namespace Garfield;
using namespace std;

int main(int argc, char * argv[]) {
    const char* result = "../teste/Field.result";
    const char* elements = "../teste/mesh.elements";
    const char* nodes = "../teste/mesh.nodes";
    const char* header = "../teste/mesh.header";
    const char* dielectrics = "../teste/dielectrics.dat";

    std::vector<std::tuple<double,double,double, std::string, double>> conditions;
    conditions.emplace_back(1.0,1,0.90,"Kr",0.30);
    conditions.emplace_back(3.0,2,0.70,"Ar",0.55);
    conditions.emplace_back(1.0,1,0.70,"Ar",0.57);

    // Criação do arquivo ROOT
    TFile* file = new TFile("ganho.root", "RECREATE");

    for (const auto& condition : conditions) {
        auto [window, pressure, proportion, tipo, rP] = condition;

        // Nome da TTree
        std::ostringstream oss;
        oss << tipo << "_" << int(proportion*100) << "_" << 100 - int(proportion*100)
            << "_" << pressure << "atm_" << window << "cm";
        std::string nome = oss.str();

        MediumMagboltz* gas = new MediumMagboltz();
        gas->SetComposition(tipo, proportion, "co2", 1 - proportion);
        gas->SetTemperature(293.15);
        gas->SetPressure(pressure * 706.4);
        gas->EnableDrift();
        gas->EnablePenningTransfer(rP, 0, tipo);
        gas->Initialise(true);

        ComponentElmer* elm = new ComponentElmer(header, elements, nodes, dielectrics, result, "micron");
        elm->EnableMirrorPeriodicityX();
        elm->EnableMirrorPeriodicityY();
        elm->SetMedium(0, gas);
        elm->EnableConvergenceWarnings(false);

        Sensor* sensor = new Sensor();
        sensor->AddComponent(elm);
        sensor->SetArea(-0.021, -0.021, -0.01, 0.021, 0.021, 0.01);

        const int nevents = 200000;
        const double x = 0., y = 0., z = 0.0095;
        const double dx = 0, dy = 0, dz = -1;
        const double e = 1;

        int nthreads = omp_get_max_threads();
        vector<vector<int>> ganhos_threads(nthreads);

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            AvalancheMicroscopic* aval = new AvalancheMicroscopic();
            aval->SetSensor(sensor);
            vector<int>& ganhos_local = ganhos_threads[tid];
            double x0,y0,z0,t0,e0;
            double x1,y1,z1,t1,e1;
            int status;

            #pragma omp for
            for (int i = 0; i < nevents; ++i) {
                double t = 10 * i;
                int charge = 0; // Zera o ganho para cada evento
                aval->AvalancheElectron(x, y, z, t, e, dx, dy, dz);
                //--------------------------------------------------------------
                //
                // TO COMPUTANDO O GANHO EFETIVO -> A CARGA QUE CHEGA NO ANODO (OU QUE PELO MENOS PASSA DO GEM A UMA DISTÂNCIA Z MÍNIMA)
                //
                //--------------------------------------------------------------
                if (aval->GetNumberOfElectronEndpoints() == 1) {
                    aval->GetElectronEndpoint(0,x0,y0,z0,t0,e0,x1,y1,z1,t1,e1,status);
                    if (z1 < -0.8*0.01){
                        ganhos_local.push_back(1);
                    }
                } else if (aval->GetNumberOfElectronEndpoints() > 1) {
                    for (size_t idx = 0; idx < aval->GetNumberOfElectronEndpoints(); ++idx) {
                        aval->GetElectronEndpoint(idx,x0,y0,z0,t0,e0,x1,y1,z1,t1,e1,status);
                        if (z1 < -0.8*0.01){
                            charge++;
                        }
                    }
                    // Guarda o ganho local
                    ganhos_local.push_back(charge);
                }
                //#-------------------------------------------------

                #pragma omp critical
                if (i % 1000 == 0) {
                    cout << "Thread " << tid << " evento " << i << "/" << nevents << " (" << nome << ")" << endl;
                }
            }
        }

        // Cria a TTree para essa condição
        TTree* tree = new TTree(nome.c_str(), "Ganho por avalanche");
        int gain = 0;
        tree->Branch("gain", &gain);

        // Preenche a árvore com os ganhos
        for (const auto& vec : ganhos_threads) {
            for (auto g : vec) {
                gain = g;
                tree->Fill();
            }
        }

        tree->Write();  // escreve essa TTree no arquivo

        delete gas;
        delete elm;
        delete sensor;

        cout << "Condição " << nome << " salva." << endl;
    }

    file->Close();
    cout << "Todas as árvores foram escritas em ganho.root" << endl;
    return 0;
}
