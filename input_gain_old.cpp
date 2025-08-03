#include <TFile.h>
#include <TTree.h>
#include <TH1F.h>
#include <TCanvas.h>
#include <iostream>

void input_gain_old() {
    // Abrir arquivo com número de elétrons por evento
    TFile* file = TFile::Open("no_gain.root");
    TTree* tree = (TTree*) file->Get("no_gain");

    // Definir variável para ler a branch (int, pois a branch não é vetor)
    int nelectrons = 0;
    tree->SetBranchAddress("Kr_90_10_1atm_1cm", &nelectrons);

    // Abrir arquivo com a distribuição de ganho
    TFile* file2 = TFile::Open("tomografo.root");
    TTree* gain_tree = (TTree*) file2->Get("Ar_70_30");

    // Preparar variável para a branch de ganho
    int ganho = 0;
    gain_tree->SetBranchAddress("ganho", &ganho);

    // Criar histograma PDF do ganho
    TH1F* pdf = new TH1F("pdf", "Gain PDF", 1000, 0, 600);
    Long64_t nentries_gain = gain_tree->GetEntries();
    for (Long64_t i = 0; i < nentries_gain; ++i) {
        gain_tree->GetEntry(i);
        pdf->Fill(ganho);
    }
    pdf->Scale(1.0 / pdf->Integral(), "width");  // Normalizar como PDF

    // Criar histograma para o espectro final
    TH1F* spectra = new TH1F("spectra", "Spectrum", 512, 0, 2*4096);

    // Loop nos eventos
    Long64_t nentries = tree->GetEntries();
    for (Long64_t i = 0; i < nentries; ++i) {
        tree->GetEntry(i);

        int sum = 0;
        for (int j = 0; j < nelectrons; ++j) {
            sum += pdf->GetRandom();  // Simula o ganho de cada elétron
        }
        spectra->Fill(sum);
    }

    // Visualização
    TCanvas* c = new TCanvas("c", "Canvas", 800, 600);
    spectra->SetTitle("Kr_90_10_1atm_1cm");
    spectra->GetYaxis()->SetTitle("Counts");
    spectra->GetXaxis()->SetTitle("Spectrum [# electrons]");
    spectra->GetXaxis()->CenterTitle(true);
    spectra->GetYaxis()->CenterTitle(true);
    spectra->Draw();
    //c->SaveAs("spectra.pdf");
}
