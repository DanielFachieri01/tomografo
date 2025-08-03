#include <iostream>
#include <vector>
#include <cmath> 
#include <cstring>
#include <fstream>
#include <TCanvas.h>
#include <TApplication.h>
#include <TFile.h>
#include <TGraph.h>
#include <TF1.h>
#include <TLegend.h>
#include <string>
#include <sstream>


#include <TCanvas.h> //pra criar as figuras
#include <TROOT.h>
#include <TH1F.h>  //criar os histogramas


#include "Garfield/MediumMagboltz.hh"
#include "Garfield/ComponentElmer.hh"
#include "Garfield/SolidTube.hh"
#include "Garfield/GeometrySimple.hh"
#include "Garfield/ComponentConstant.hh"
#include "Garfield/Plotting.hh"
#include "Garfield/GarfieldConstants.hh"
#include "Garfield/Random.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/FundamentalConstants.hh"
#include "Garfield/TrackHeed.hh"


using namespace Garfield;
using namespace std;

#include <iostream>
#include <vector>



/*
Simulação de ganho do detector que varia com a pressão do gás
Vai ser definido ganho a quantidade de elétrons que chega lá no readout
*/

#include <vector>

std::vector<double> linspace(double start, double end, int num) {
    std::vector<double> result;

    if (num == 0) return result;
    if (num == 1) {
        result.push_back(start);
        return result;
    }

    double step = (end - start) / (num - 1);
    for (int i = 0; i < num; ++i) {
        result.push_back(start + i * step);
    }

    return result;
}


int main(int argc, char * argv[]) {   

    std::vector<std::tuple<double,double,double, std::string>> conditions;
    conditions.emplace_back(1.0,1,70,"Ar");
    conditions.emplace_back(3.0,1,70,"Ar");
    conditions.emplace_back(3.0,2,70,"Ar");
    conditions.emplace_back(1.0,1,90,"Kr");
    conditions.emplace_back(3.0,1,90,"Kr");

	TApplication app("app", &argc, argv);
	// Dimensão do problema em cm.

    const float copper = 5e-4;
    const float kap = 50e-4;
    const float ind = 1750e-4;
    const float drift = 1000e-4;

    TFile* file = new TFile("tomografo.root", "RECREATE");
    TDirectory *graphdir = (TDirectory*)file->mkdir("eff");
    graphdir->cd();

for(const auto& condition : conditions){

    auto [window, pressure, proportion, type] = condition;

	//------------ Define o meio de interação e as característica do gás --------------//
	MediumMagboltz* gas = new MediumMagboltz();
	gas->SetTemperature(293.15);                  // Temperatura [K]
	gas->SetPressure(pressure*760.);                       // Pressão [Torr]
	gas->SetComposition(type, proportion, "co2", 100-proportion);   //
						
    // Create a cylinder in which the x-rays can convert.
    // Diameter [cm]
    constexpr double diameter = 10;
    // Half-length of the cylinder [cm].
    //constexpr double length = window ;
    SolidTube tube(0, 0, 0, 0.5 * diameter, window/2);

    // Combine gas and box to a simple geometry.
    GeometrySimple* geo = new GeometrySimple();
    geo->AddSolid(&tube, gas);


    // Make a component with constant electric field.
    ComponentConstant* field = new ComponentConstant();
    field->SetGeometry(geo);
    field->SetElectricField(0., 0., 500.); 
    
    Sensor* sensor = new Sensor();
	sensor->AddComponent(field);
    
    int nevents = 100000;
	// -------------------------- criando as avalanches ---------------------------------//
	AvalancheMicroscopic* aval = new AvalancheMicroscopic();
	aval->SetSensor(sensor);

    //----------------------------Criando os fótons pra medir probabilidade--------------//
    //se mais que 100 elétrons foram gerados, então esse fóton interagiu com o meio e foi
    // detectado. Se fóton foi gerado, acumula isso e depois divide pelo número de fótons
    // incidentes pra ter uma medida de eficiência de detecção.

    TrackHeed* track = new TrackHeed();
    track->SetSensor(sensor);

    double xf= 0.;
    double yf= 0.;
    double zf = (window/2)-0.001;
    double dxf = 0;
    double dyf = 0;
    double dzf = -1;

    std::vector<double> egamma = linspace(10,50,100);
    std::vector<double> y;

    for (auto& energy : egamma){
        double detected = 0;
    for(int i=0;i<nevents;i++){
        int nef = 0;
        track->TransportPhoton(xf,yf,zf,i,energy*1e3,dxf,dyf,dzf,nef);
        if(nef>100){
            detected++;
        }
    }
    y.push_back(detected/nevents);
}

    TCanvas* c1 = new TCanvas();
    TGraph* geff = new TGraph(egamma.size(),egamma.data(),y.data());
    geff->SetTitle("Detection efficiency");
    geff->GetYaxis()->SetTitle("Probability");
    geff->GetXaxis()->SetTitle("Photon Energy (keV)");
    geff->SetLineColor(kBlue);
    geff->SetMarkerStyle(21);
    geff->SetMarkerSize(1.1);
    geff->SetMarkerColor(kBlue);
    //c1->SaveAs("eff_2atm.png");
    y.clear();

    std::ostringstream oss;
    oss << type << " (" << proportion << "/" << 100 - proportion << ") - " << pressure << "atm - " << window << "cm";
    std::string resultado = oss.str();
    geff->SetName(resultado.c_str());
    
    
    // 2. Abre o arquivo em modo UPDATE (mantém os anteriores)

    // 3. Escreve no arquivo com nome único
    geff->Write(resultado.c_str());

    // 4. Fecha o arquivo (salva e libera)
    

    std::cout << "trabalhando..." << endl;
    

    //app.Run(true);
}
    file->Close();
    std::cout << "Terminou" << endl;
}