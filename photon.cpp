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
    if(r<10){
        return fundo(rnd, emax);
    }
    else{
        return gamma_ag(rnd);
    }
}

int main(int argc, char * argv[]) {

    TRandom3 rnd(2222); //semente de controle

    double emax = 50e3;

    TFile* file = TFile::Open("no_gain.root", "RECREATE");
    TTree* tree = new TTree("no_gain","");


    std::vector<std::tuple<double,double,double, std::string>> conditions;
    conditions.emplace_back(1.0,1,0.70,"Ar");
    conditions.emplace_back(3.0,1,0.70,"Ar");
    conditions.emplace_back(3.0,2,0.70,"Ar");
    conditions.emplace_back(1.0,1,0.90,"Kr");
    conditions.emplace_back(3.0,1,0.90,"Kr");

    for(const auto& condition : conditions){

    auto [window, pressure, proportion, tipo] = condition;


    MediumMagboltz* gas = new MediumMagboltz();
    gas->SetTemperature(293.15);
    gas->SetPressure(pressure*760.);
    gas->EnableDrift();
    gas->SetComposition(tipo, proportion, "co2", 1 - proportion);
    gas->Initialise();
    //gas->EnablePenningTransfer();
    gas->EnableDebugging();
    gas->DisableDebugging();

    constexpr double diameter = 10;
    // Half-length of the cylinder [cm].
    double length = window/2 ;
    SolidTube tube(0, 0, 0, 0.5 * diameter, length);

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

    int nef = 0;
    std::ostringstream oss;
    oss << tipo << "_" << proportion << "_" << 100 - proportion << "_" << pressure << "atm_" << window << "cm";
    std::string resultado = oss.str();
    tree->Branch(resultado.c_str(),&nef);

    TrackHeed* track = new TrackHeed();
    track->SetSensor(sensor);

    for(int idx =0; idx < nevents; idx++){

        double gamma_origin = rnd.Uniform(0,1);

        if (gamma_origin < 0.88){ //tem mais fótons do tubo de raios-X que de cobre
        double x = 0. + rnd.Uniform(-0.035,0.035);
        double y = 0. + rnd.Uniform(-0.035,0.035);
        double z = length/2 - 1e-4; //garantir que o fóton está sendo emitido dentro do volume de gás mas logo acima da região de deriva
        double dx = rnd.Uniform(-0.01,0.01);//fóton chegnado quase
        double dy = rnd.Uniform(-0.01,0.01);//paralelo ao gás
        double dz = -1; 
        track->TransportPhoton(x,y,z,idx,gamma_Energy(rnd, emax), dx,dy,dz,nef);
    }
        else{
        double x = 0. + rnd.Uniform(-0.035,0.035);
        double y = 0. + rnd.Uniform(-0.035,0.035);
        double z = -length + 1e-4; //garantir que o fóton está sendo emitido logo acima da camada de cobre em meio angulo sólido
        double dx = rnd.Uniform(-1,1);//fóton saindo com 
        double dy = rnd.Uniform(-1,1);//meio angulo sólido
        double dz = rnd.Uniform(0,1); 
        track->TransportPhoton(x,y,z,idx,gamma_cu(rnd), dx,dy,dz,nef);
        }
        if (idx % 10000 == 0){
            cout << idx << "/100000" << endl;
        }
        if(nef > 0){
            tree->Fill();
        }
}
    

}
    tree->Write();
    file->Close();

    cout << "Terminou !" << endl;
    return 0;
}