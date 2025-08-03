
#include <iostream>
#include <vector>
#include <cmath> 
#include <cstring>
#include <fstream>
#include <TFile.h>
#include <TGraph.h>
#include <TF1.h>
#include <TLegend.h>
#include <iomanip> // Para setw e setfill
#include <string>



#include "Garfield/MediumMagboltz.hh"
#include "Garfield/ComponentElmer.hh"
#include "Garfield/GarfieldConstants.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/Sensor.hh"
#include "Garfield/TrackHeed.hh"


using namespace Garfield;
using namespace std;


auto fT = [](double t) {
  
	constexpr double tau = 160.; //ns
	constexpr double Q = 600*ElementaryCharge; //eletrons * carga elementar do elétron -> 1e-19
	constexpr double A = 1; //adimensional -> ganho do shaper
	constexpr double Cf = 18.5; //pF -> pico = 1e-12
	constexpr int n = 4; //shapingj order
	
	return (pow((t / tau),n))*exp(-n*t / tau);
  };


int main(int argc, char * argv[]) {   

    TF1* f = new TF1("f", "[0] * (pow((x/[1]),4)) * exp(-4*x/[1])",0,700); 
	const double tau = 160;

	//variáveis para salvar diretório dos arquivos inputs
	char* result = new char[150];		
	char* elements = new char[150];
	char* nodes = new char[150];
	char* header = new char[150];
	char* dielectrics = new char[150];

	const int kOrange = 800;
	const int kBrown = 920;

	//coloque aqui o diretório de cada um dos arquivos
	sprintf(result,"../compass/2dgem.result"); 	
	sprintf(elements,"../compass/mesh.elements");
	sprintf(nodes,"../compass/mesh.nodes");
	sprintf(header,"../compass/mesh.header");
	sprintf(dielectrics,"../compass/dielectrics.dat");	
	

	// Dimensão do problema em cm.
	const double axis_x = 0.060;             // Largura em X da região onde a simulação irá acontecer
	const double axis_y = 0.060;             // Largura em Y da região onde a simulação irá acontecer
	const double induction = 0.1725 + 2*0.0005 +2*0.0050+ 0.05;
	const double drift = 2*0.0005 +1.5*0.0050 + 0.0300;
	const double pitch =  0.0140;	       // Distancia em Z da região onde a simulação irá acontecer
	const double inhole = 0.02;

	//Concentração de gases
	double ArgonConcent = 70.0;
	double CO2Concent = 30.0;
	
	//------------ Define o meio de interação e as característica do gás --------------//
	MediumMagboltz* gas = new MediumMagboltz();
	gas->SetTemperature(293.15);                  // Temperatura [K]
	gas->SetPressure(760.);                       // Pressão [Torr]
	gas->EnableDrift();
	gas->SetComposition("ar", ArgonConcent, "co2", CO2Concent);   // Ar/CO2 90:10
	gas->SetMaxElectronEnergy(200.);              // Energia [eV]
	gas->EnableDebugging();
	gas->Initialise();
	gas->DisableDebugging();	
						
	// ----------- Importa os arquivos do Elmer para serem utilizados------------------//
	ComponentElmer *elm = new ComponentElmer(header,elements,nodes,dielectrics,result,"micron"); 
	//load em todos os mesh gerados pelo gmsh e .result do elmer
	elm->EnableMirrorPeriodicityX();		//Periodicidade espelhada em X
	elm->EnableMirrorPeriodicityY();		//Periodicidade espelhada em Y
	elm->SetMedium(0, gas);				    //Meio 	
	elm->EnableConvergenceWarnings(false);			
	elm->SetWeightingField("../compass/WF_stripx_2dgem.result","strip_x");
	elm->SetWeightingField("../compass/WF_stripy_2dgem.result","strip_y");	
	

    const int nEvents = 100000;
	
	//---- cria a classe sensor que integra a componente e o gas e calcula a simulacao---//
	Sensor* sensor = new Sensor();
	sensor->AddComponent(elm);
    sensor->SetArea();
	sensor->AddElectrode(elm, "strip_x");
	sensor->AddElectrode(elm, "strip_y");
	const unsigned int tStart = 0;
	const unsigned int tStep = 1; //largura do bin
	const unsigned int nSteps = 800/tStep;
	sensor->SetTimeWindow(tStart, tStep, nSteps); //100ns a a unidade entao 1 = 100ns
	sensor->SetTransferFunction(fT);
	sensor->ClearSignal();

	
	// -------------------------- criando as avalanches ---------------------------------//
	AvalancheMicroscopic* aval = new AvalancheMicroscopic();
	aval->SetSensor(sensor);


    //----------------------------criando o heed-----------------------------------------//
    TrackHeed* track = new TrackHeed();
    track->SetSensor(sensor);

	//[0] é a amplitude e [1] é o tau

	for(int j = 0; j<nEvents; j++){
		Double_t x[nSteps],y[nSteps];
		//------------------------fazer as avalanches--------------------------// 
        int nexr;
		const double x0 = 0;
		const double y0 = 0;
		const double z0 = 0.1*drift;
        const double dx = 0;
        const double dy = 0;
        const double dz = -1;
        const double t0 = 1e-9;
        const double egamma = 5e3; //fazer para 2,5,10,15 keV
        track->TransportPhoton(x0, y0, z0, t0, egamma, 0., 0, -1., nexr); //isso é o evento  

        std::vector<std::array<double, 4> > electrons;
        
        for (const auto& cluster : track->GetClusters()) {
              for (const auto& electron : cluster.electrons) {
            electrons.push_back({electron.x, electron.y, electron.z, electron.t});
        }
            const auto nesum = electrons.size();
            cout << "elétrons gerados = " << nesum << endl;

            for (size_t i = 0; i < nesum; ++i) { 
                aval->AvalancheElectron(electrons[i][0], electrons[i][1], electrons[i][2],electrons[i][3], 0,0,0,0);
	        }
		
        }
		

		//-------pedir pra função sensor retornar o valor do sinal a cada time-step----

		sensor->ConvoluteSignal("strip_x");
		for (int k = 0; k < nSteps; k++){
			y[k]=(sensor->GetSignal("strip_x",k));
			x[k] = k*tStep;
		}

        //-------------------criar o arquivo que vai salvar a waveform----------------------//
	    std::unique_ptr<TFile> myFile( TFile::Open("files/roots/waveform_5keV.root", "RECREATE") );
	
		/*
        //--------------criar o gráfico q vai ser armazenado no .root------------------
		TGraph *gr = new TGraph(sizeof(x)/sizeof(Double_t), x,y);
		gr->SetTitle("waveform");
		gr->GetYaxis()->SetTitle("Charge [fC]");
		gr->GetXaxis()->SetTitle("time [ns]");
		gr->SetLineColor(kBlue);
		gr->SetMarkerStyle(21);
		gr->SetMarkerSize(0.5);
		gr->SetMarkerColor(kBlue);
		gr->Write("waveform");
		*/

		//cout << "Arquivo salvo com sucesso nessa run! \n";
		
		//---------Limpar o sensor para o próximo sinal----------------------------
		sensor->ClearSignal();

		} 

		myFile->Close();

	//app.Run(kTRUE);	//Comente essa linha para fechar os histogramas

	return 0;
	};