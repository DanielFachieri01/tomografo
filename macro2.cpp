#include <TFile.h>
#include <TTree.h>
#include <TDirectory.h>
#include <TGraph.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <TKey.h>


void macro2(){
    
    int nbins=100;
    int min=-1;
    int max=200;
    TFile* file = TFile::Open("tomografo.root");
    TTree* tree = (TTree*) file->Get("Ar_70_30");
    // Criar histograma
    TH1F* hGanho = new TH1F("hGanho", "", nbins,min,max);

    // Preencher histograma com a branch "ganho"
    tree->Draw("ganho >> hGanho");
    // Mostrar histograma
    TCanvas* c = new TCanvas("c", "", 800, 600);
    c->cd();
    c->SetLeftMargin(0.15);
    c->SetLogy(); // log y se quiser
    hGanho->SetLineColor(kBlue + 2);
    hGanho->GetYaxis()->SetTitle("Probability Density Function");
    hGanho->GetXaxis()->SetTitle("Gain");
    hGanho->GetXaxis()->CenterTitle(true);
    hGanho->GetYaxis()->CenterTitle(true); 
    hGanho->Scale(1/hGanho->Integral("width"));
    hGanho->Draw();
    //c->SaveAs("Ar_70_30.pdf");

}