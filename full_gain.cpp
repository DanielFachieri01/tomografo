#include <iostream>
#include <vector>
#include <omp.h>
#include <string>
#include <sstream>
#include <cstring>

#include <TFile.h>
#include <TTree.h>
#include <TH1F.h>
#include <TDirectory.h>
#include <TGraph.h>
#include <TCanvas.h>
#include <TLegend.h>

#include "Garfield/MediumMagboltz.hh"
#include "Garfield/ComponentElmer.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/Sensor.hh"

using namespace Garfield;
using namespace std;

int main(int argc, char * argv[]) {
    char result[150], elements[150], nodes[150], header[150], dielectrics[150], WTpotential[150];
    sprintf(result,"../teste/Field.result"); 	
    sprintf(elements,"../teste/mesh.elements");
    sprintf(nodes,"../teste/mesh.nodes");
    sprintf(header,"../teste/mesh.header");
    sprintf(dielectrics,"../teste/dielectrics.dat");

    const float copper = 5e-4;
    const float kap = 50e-4;

    std::vector<std::tuple<double,double,double, std::string, double>> conditions;

    conditions.emplace_back(1.0,1,0.90,"Kr",0.30);
    conditions.emplace_back(3.0,2,0.70,"Ar",0.55);
    conditions.emplace_back(1.0,1,0.70,"Ar",0.57);

    // Agora grava o arquivo ROOT, só numa thread (serial)
    TFile *file = new TFile("ganho.root", "RECREATE");
    TTree *tree = new TTree("multiplication", "");

    //--------------------------------------------------------------------------
    //
    // CONDIÇÃO DE CADA ENSEMBLE
    //
    //--------------------------------------------------------------------------

    for(const auto& condition : conditions){

    auto [window, pressure, proportion, tipo, rP] = condition;
    int perc1 = static_cast<int>(proportion * 100);
    int perc2 = 100 - perc1;

    std::ostringstream oss;
    oss << tipo << "_" << perc1 << "_" << perc2 << "_" << pressure << "atm_" << window << "cm";
    std::string nome = oss.str();

 
    MediumMagboltz* gas = new MediumMagboltz();
    gas->SetComposition(tipo, proportion, "co2", 1-proportion);
    gas->SetTemperature(293.15);
    gas->SetPressure(pressure*760.);
    gas->EnableDrift();
    gas->Initialise();
    //gas->EnablePenningTransfer(0.35,10.6); //para Kr90.10
	gas->EnablePenningTransfer(rP,0,tipo); //para AR 70 30

    ComponentElmer *elm = new ComponentElmer(header,elements,nodes,dielectrics,result,"micron");
	//elm->SetWeightingField(WTpotential, "readout");
	elm->EnableMirrorPeriodicityX();
    elm->EnableMirrorPeriodicityY();
    elm->SetMedium(0, gas);
    elm->PrintMaterials();
    elm->PrintRange();
    elm->EnableConvergenceWarnings(false);


    Sensor* sensor = new Sensor();
    sensor->AddComponent(elm);
    sensor->SetArea(-3*0.007,-3*0.007,-0.01,3*0.007,3*0.007,0.01);
	//sensor->AddElectrode(elm, "readout");

    const int nevents = 100000;

    const double x= 0.;
    const double y= 0.;
    const double z = 0.0095;
    const double dx = 0;
    const double dy = 0;
    const double dz = -1;
    const double e = 1;

    int nthreads = omp_get_max_threads();

    cout << "Número de threads OpenMP: " << nthreads << endl;

    // Vetor para guardar ganhos por thread
    vector<vector<int>> ganhos_threads(nthreads);


    #pragma omp parallel //separa entre as threads
    {
        int tid = omp_get_thread_num(); //cada tid vai ser único para cada thread
        AvalancheMicroscopic aval; //cria um objeto avalanche para cada thread
        aval.SetSensor(sensor);
        


        vector<int>& ganhos_local = ganhos_threads[tid]; //a thread acessa apenas o seu correspondente

        #pragma omp for //executa em paralelo o for loop
        for (int i = 0; i < nevents; ++i) {
            double t = 10 * i;
            aval.AvalancheElectron(x, y, z, t, e, dx, dy, dz);

            ganhos_local.push_back(aval.GetNumberOfElectronEndpoints()); //cada thread guarda em um vetor local o ganho dela

            #pragma omp critical
            if(i%100 == 0){
                cout << "Thread " << tid << "; Evento " << i << "/" << nevents << endl; //para ter um controle doq tá acontecendo 
            }
        }
    }



    int ganho_branch=0;
    tree->Branch(nome.c_str(), &ganho_branch);

    for (const auto& ganhos_vec : ganhos_threads) { //junta o resultado de todas as threads num único vetor
        for (auto g : ganhos_vec) {
            ganho_branch = g;
            tree->Fill(); //preenche a tree com esse vetor total
        }
    }

    delete gas;
    delete elm;
    delete sensor;

    
}
    tree->Write();
    file->Close();
    return 0;
}