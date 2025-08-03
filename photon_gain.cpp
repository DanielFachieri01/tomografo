#include <iostream>
#include <vector>
#include <omp.h>

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
#include "TH1F.h"
#include "TCanvas.h"


using namespace Garfield;
using namespace std;

double fundo(TRandom3& rnd, double emax){
    while(true){
        double E = rnd.Uniform(1000.,emax);
        double I = (emax = E) / E;
        double prob = rnd.Uniform(0., (emax - 1000.)/1000.);

        if (prob < I){
            return E;
        }
    }

}

double gamma_ag(TRandom3& rnd){
    double r = rnd.Uniform(0,100);
    if(r < 66.7) return 22163.0;
    else if (r < 91.7) return 21983.0;
    else return 25136.0;
}

double gamma_cu(TRandom3& rnd){
    double r = rnd.Uniform(0,100);
    if(r < 66.7) return 8047.8;
    else if (r < 91.7) return 8027.8;
    else return 8905.3;
}

double gamma_Energy(TRandom3& rnd, double emax){
    double r = rnd.Uniform(0,100);
    if(r<88){
        return fundo(rnd, emax);
    }
    else{
        return gamma_ag(rnd);
    }
}

int main(int argc, char * argv[]) {
    char result[150], elements[150], nodes[150], header[150], dielectrics[150], WTpotential[150];
    sprintf(result,"../standard_GEM/Field.result"); 	
    sprintf(elements,"../standard_GEM/mesh.elements");
    sprintf(nodes,"../standard_GEM/mesh.nodes");
    sprintf(header,"../standard_GEM/mesh.header");
    sprintf(dielectrics,"../standard_GEM/dielectrics.dat");

    const float copper = 5e-4;
    const float kap = 50e-4;

    TRandom3 rnd(0); //semente de controle

    double emax = 30e3;

    MediumMagboltz* gas = new MediumMagboltz();
    gas->SetTemperature(293.15);
    gas->SetPressure(1*760.);
    gas->EnableDrift();
    gas->SetComposition("Ar", 70, "co2", 30);
	gas->EnablePenningTransfer(0.57, 0,"ar"); //para AR 70 30
    gas->EnableDebugging();
    gas->DisableDebugging();

    constexpr double diameter = 10;
    // Half-length of the cylinder [cm].
    //constexpr double length = window ;
    SolidTube tube(0, 0, 0, 0.5 * diameter, 0.5);

    // Combine gas and box to a simple geometry.
    GeometrySimple* geo = new GeometrySimple();
    geo->AddSolid(&tube, gas);

    // Make a component with constant electric field.
    ComponentConstant* field = new ComponentConstant();
    field->SetGeometry(geo);
    field->SetElectricField(0., 0., 500.); 
    
    Sensor* sensor = new Sensor();
	sensor->AddComponent(field);

    double xmin,xmax,ymin,ymax,zmin,zmax;
    field->GetBoundingBox(xmin,xmax,ymin,ymax,zmin,zmax);

    const int nevents = 100000;

    TFile* file = TFile::Open("tomografo.root", "UPDATE");
    TTree* tree = (TTree*)file->Get("Ar_70_30");

    int ganho;
    tree->SetBranchAddress("ganho", &ganho);

    TH1F* hPDF = new TH1F("hPDF", "Distribuição de Ganho", 100, 0, 100);  // Ajuste os limites conforme seus dados

    TH1F* dummy = new TH1F("dummy", "sinal com ganho", 1000,100,1600);

    Long64_t nentries = tree->GetEntries();
    for (Long64_t i = 0; i < nentries; ++i) {
        tree->GetEntry(i);
        hPDF->Fill(ganho);
    }

    // Normaliza para PDF (área total = 1)
    hPDF->Scale(1.0 / hPDF->Integral("width"));

    int nef;
    int sum;
    tree->Branch("with_gain",&sum);

    TrackHeed* track = new TrackHeed();
    track->SetSensor(sensor);

    for(int idx =0; idx < nevents; idx++){
        sum = 0;

        double gamma_origin = rnd.Uniform(0,1);

        if (gamma_origin < 0.97){ //tem mais fótons do tubo de raios-X que de cobre
        double x = 0. + rnd.Uniform(-0.035,0.035);
        double y = 0. + rnd.Uniform(-0.035,0.035);
        double z = zmax - 1e-4; //garantir que o fóton está sendo emitido dentro do volume de gás mas logo acima da região de deriva
        double dx = rnd.Uniform(-0.01,0.01);//fóton chegnado quase
        double dy = rnd.Uniform(-0.01,0.01);//paralelo ao gás
        double dz = -1; 
        track->TransportPhoton(x,y,z,idx,gamma_Energy(rnd, emax), dx,dy,dz,nef);
        }

        else{
        double x = 0. + rnd.Uniform(-0.035,0.035);
        double y = 0. + rnd.Uniform(-0.035,0.035);
        double z = 0 + (50e-4)/2 + 5e-4 + 1e-4; //garantir que o fóton está sendo emitido fora da camada de cobre 
        double dx = rnd.Uniform(-1,1);//fóton saindo com 
        double dy = rnd.Uniform(-1,1);//meio angulo sólido
        double dz = 1; 
        track->TransportPhoton(x,y,z,idx,gamma_cu(rnd), dx,dy,dz,nef);
        }

        if(nef > 0){
            for(int i = 0; i<nef ; i++){
                sum += hPDF->GetRandom();
            }

            tree->Fill();
            dummy->Fill(sum);
        }

        if (idx % 10000 == 0){
            cout << idx << "/100000" << endl;
            cout << "------------------" << endl;
            cout << "Elétrons gerados com avalanche :" << sum << endl;
        }
}
    tree->Write();
    file->Close();

    TCanvas* c1 = new TCanvas("c1","sinal com ganho",600,800);
    c1->cd();
    dummy->GetYaxis()->SetTitle("counts");
    dummy->GetXaxis()->SetTitle("n electrons");
    dummy->Draw();

    TFile* fout = new TFile("pdf_de_ganho.root", "RECREATE");
    hPDF->Write();
    fout->Close();

    /*
    TFile* f = TFile::Open("pdf_de_ganho.root");
    TH1F* hPDF = (TH1F*) f->Get("hPDF");
    double g = hPDF->GetRandom(); // usa direto!
    */

    cout << "Terminou !" << endl;
    return 0;
}
