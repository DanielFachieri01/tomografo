#include <iostream>
#include <vector>
#include <omp.h>

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

 
    MediumMagboltz* gas = new MediumMagboltz();
    gas->SetComposition("Ar", 70, "co2", 30);
    gas->SetTemperature(293.15);
    gas->SetPressure(1*760.);
    gas->EnableDrift();
    gas->Initialise();
    //gas->EnablePenningTransfer(0.35,10.6); //para Kr90.10
	gas->EnablePenningTransfer(); //para AR 70 30

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

    // Agora grava o arquivo ROOT, só numa thread (serial)
    TFile *file = new TFile("tomografo.root", "RECREATE");
    TTree *tree = new TTree("Ar_70_30", "");

    int ganho_branch;
    tree->Branch("ganho", &ganho_branch, "ganho/I");

    for (const auto& ganhos_vec : ganhos_threads) { //junta o resultado de todas as threads num único vetor
        for (auto g : ganhos_vec) {
            ganho_branch = g;
            tree->Fill(); //preenche a tree com esse vetor total
        }
    }

    tree->Write();
    int nbins = 600;

    TH1F* hGanho = new TH1F("hGanho", "", 300 , -1, nbins -1);

    // Preencher histograma com os dados da TTree
    tree->Draw("ganho >> hGanho");

    TCanvas* c = new TCanvas("c", "", 800, 600);
    c->cd();
    c->SetLeftMargin(0.15);
    c->SetLogy();

    hGanho->SetLineColor(kBlue + 2);
    hGanho->GetYaxis()->SetTitle("Probability Density Function");
    hGanho->GetXaxis()->SetTitle("Multiplication Factor");
    hGanho->GetXaxis()->CenterTitle(true);
    hGanho->GetYaxis()->CenterTitle(true);

    hGanho->Scale(1.0 / hGanho->Integral());

    hGanho->Draw();
    c->Write();


        // Calcular Fano factor após preencher a tree
    std::vector<int> all_ganhos;
    for (const auto& ganhos_vec : ganhos_threads) {
        all_ganhos.insert(all_ganhos.end(), ganhos_vec.begin(), ganhos_vec.end());
    }

    double sum = std::accumulate(all_ganhos.begin(), all_ganhos.end(), 0.0);
    double mean = sum / all_ganhos.size();

    double sq_sum = 0.0;
    for (double g : all_ganhos) {
        sq_sum += (g - mean) * (g - mean);
    }
    double variance = sq_sum / all_ganhos.size();
    double fano = variance / mean;

    std::cout << "Ganho médio: " << mean << std::endl;
    std::cout << "Variância: " << variance << std::endl;
    std::cout << "Fano factor: " << fano << std::endl;



    file->Close();
    delete file;

    delete gas;
    delete elm;
    delete sensor;

    return 0;
}
