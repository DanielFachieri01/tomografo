//--------------------------------------------------------------
//
// muitas vezes eu faço o arquivo no gmsh/Elmer e checo no garfield++ se as coisas estão certas
// tipo campo elétrico, onde to posicionando os elétrons pra computar avalanche e etc
//
//--------------------------------------------------------------
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



#include "Garfield/MediumMagboltz.hh"
#include "Garfield/ComponentElmer.hh"
#include "Garfield/ViewField.hh"
#include "Garfield/Plotting.hh"
#include "Garfield/ViewFEMesh.hh"
#include "Garfield/ViewSignal.hh"
#include "Garfield/ViewDrift.hh"
#include "Garfield/GarfieldConstants.hh"
#include "Garfield/Random.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/AvalancheMC.hh"
#include "Garfield/ViewSignal.hh"
#include "Garfield/FundamentalConstants.hh"
#include "Garfield/Sensor.hh"


using namespace Garfield;
using namespace std;


int main(int argc, char * argv[]) {   

	TApplication app("app", &argc, argv);

    char result[150], elements[150], nodes[150], header[150], dielectrics[150];
    sprintf(result,"../teste/Field.result"); 	
    sprintf(elements,"../teste/mesh.elements");
    sprintf(nodes,"../teste/mesh.nodes");
    sprintf(header,"../teste/mesh.header");
    sprintf(dielectrics,"../teste/dielectrics.dat");	

	// Dimensão do problema em cm.
	const double axis_x = 0.060;             // Largura em X da região onde a simulação irá acontecer
	const double axis_y = 0.060;             // Largura em Y da região onde a simulação irá acontecer
	const double induction = 0.1725 + 2*0.0005 +2*0.0050+ 0.05;
	const double drift = 2*0.0005 +1.5*0.0050 + 0.0300;
	const double pitch =  0.020;	       // Distancia em Z da região onde a simulação irá acontecer
	const double inhole = 0.02;
		//Concentração de gases
	double ArgonConcent = 70.0;
	double CO2Concent = 30.0;
	

		
	//------------ Define o meio de interação e as característica do gás --------------//
	MediumMagboltz* gas = new MediumMagboltz();
	gas->SetTemperature(293.15);                  // Temperatura [K]
	gas->SetPressure(740.);                       // Pressão [Torr]
	gas->EnableDrift();
	gas->SetComposition("ar", ArgonConcent, "co2", CO2Concent);   // Ar/CO2 90:10
	gas->SetMaxElectronEnergy(200.);              // Energia [eV]
	gas->EnableDebugging();
	//gas->Initialise();
	//gas->DisableDebugging();	
						
	// ----------- Importa os arquivos do Elmer para serem utilizados------------------//
	ComponentElmer *elm = new ComponentElmer(header,elements,nodes,dielectrics,result,"micron"); 
	//load em todos os mesh gerados pelo gmsh e .result do elmer
	elm->EnableMirrorPeriodicityX();		//Periodicidade espelhada em X
	elm->EnableMirrorPeriodicityY();		//Periodicidade espelhada em Y
	elm->SetMedium(0, gas);				//Meio 
	elm->PrintMaterials();				//Printa os materiais na tela
	elm->PrintRange();	
	elm->EnableConvergenceWarnings(false);			

	
	
	//---- cria a classe sensor que integra a componente e o gas e calcula a simulacao---//
	Sensor* sensor = new Sensor();
	sensor->AddComponent(elm);
	sensor->SetArea(-0.1,-0.1,-0.05,0.1,0.1,0.05);


    // -------------------------- criando as avalanches ---------------------------------//
	AvalancheMicroscopic* aval = new AvalancheMicroscopic();
	ViewDrift* driftview = new ViewDrift();
	aval->SetSensor(sensor);
	aval->EnablePlotting(driftview, 10);

    const int nAval = 5;
    for (int i=0; i< nAval;i++){
        const double x0 = 0;//-0.5*inhole + RndmUniform() * inhole;
        const double y0 = 0;//-0.5*inhole + RndmUniform() * inhole;
        const double z0 = 0.009;
        aval->AvalancheElectron(x0,y0,z0,0,0.01,0,0,-1);
		cout << "avalanche size : "<<  aval->GetNumberOfElectronEndpoints() << endl;
    }

    //---------------- visualizando as linhas de deriva dos elétrons---------------------//
	TCanvas* vdrift = new TCanvas("vdrift", "Drift View", 800, 800);

	ViewFEMesh* meshView2 = new ViewFEMesh();

	meshView2->SetComponent(elm);
	driftview->SetPlane(0,-1,0,0,0,0);
	driftview->SetCanvas(vdrift);
	meshView2->SetViewDrift(driftview);
	meshView2->SetPlane(0, -1, 0, 0, 0, 0);  // Plano y = 0
	meshView2->SetArea(-0.015, -0.015, 0.015, 0.015); 
	meshView2->SetCanvas(vdrift);
	meshView2->SetColor(3, kGray);
	meshView2->SetColor(1, kOrange);
	meshView2->SetColor(2, kOrange);
	meshView2->SetFillMesh(true);
	meshView2->Plot(true);       // Primeiro a malha
	vdrift->SaveAs("driftlines.png");


	
	//----------------------- visualiza o campo elétrico importado -------------------//
		
	


	//Mostra o campo elétrico no interior de um dos furos
	TCanvas* eField = new TCanvas("eField", "Electric Field",800,800);
	ViewField * vf = new ViewField();
	vf->SetComponent(elm);
	vf->SetArea(-0.015, -0.015,0.015, 0.015); 	
	vf->SetNumberOfSamples2d(600,600);
	vf->SetPlane(0,-1, 0, 0, 0, 0);
	vf->SetCanvas(eField);

	ViewFEMesh* meshView = new ViewFEMesh();
	meshView->SetComponent(elm);
	meshView->SetArea(-0.015, -0.015, 0.015, 0.015); 
	meshView->SetCanvas(eField);
	meshView->SetPlane(0, -1, 0, 0, 0, 0);
	meshView->SetColor(3, kGray);
	meshView->SetColor(2, kOrange);
	meshView->SetColor(1, kOrange);
	meshView->SetFillMesh(true);
	//vf->PlotContour("e");	//a opção "v" faz o potencial
	vf->PlotProfile(0,0,-0.1,0,0,0.05,"e");
	meshView->Plot();
	eField->Draw();	
	eField->SaveAs("electric_field.pdf");
	

	
	cout << "---" << endl;
	cout << "end" << endl;
	app.Run(kTRUE);	//Comente essa linha para fechar os histogramas

	//return 0;
    
	};
